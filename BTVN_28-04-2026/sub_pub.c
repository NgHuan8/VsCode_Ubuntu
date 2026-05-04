#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define MAX_SUBS 500

typedef struct {
    int client_socket;
    char topic[50];
} Subscription;

Subscription subs[MAX_SUBS];
int sub_count = 0;

void remove_all_subscriptions(int sd) {
    for (int i = 0; i < sub_count; i++) {
        if (subs[i].client_socket == sd) {
            subs[i] = subs[sub_count - 1];
            sub_count--;
            i--;
        }
    }
}

void unsubscribe(int sd, char *topic) {
    for (int i = 0; i < sub_count; i++) {
        if (subs[i].client_socket == sd && strcmp(subs[i].topic, topic) == 0) {
            subs[i] = subs[sub_count - 1];
            sub_count--;
            printf("Client %d da UNSUB topic: %s\n", sd, topic);
            return;
        }
    }
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {AF_INET, htons(PORT), INADDR_ANY};
    bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sock, 10);

    int client_sockets[MAX_CLIENTS] = {0};
    fd_set readfds;

    printf("Server dang chay tai port %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_sock, &readfds);
        int max_sd = server_sock;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0) {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_sd) max_sd = client_sockets[i];
            }
        }

        select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_sock, &readfds)) {
            int new_sock = accept(server_sock, NULL, NULL);
            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_sock;
                    printf("Client moi ket noi: FD %d\n", new_sock);
                    added = 1;
                    break;
                }
            }
            if (!added) close(new_sock);
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[2048];
                int valread = read(sd, buffer, sizeof(buffer) - 1);

                if (valread <= 0) {
                    printf("Client FD %d da ngat ket noi.\n", sd);
                    close(sd);
                    client_sockets[i] = 0;
                    remove_all_subscriptions(sd);
                } else {
                    buffer[valread] = '\0';
                    
                    if (strncmp(buffer, "SUB ", 4) == 0) {
                        char topic[50];
                        sscanf(buffer + 4, "%s", topic);
                        if (sub_count < MAX_SUBS) {
                            subs[sub_count].client_socket = sd;
                            strncpy(subs[sub_count].topic, topic, 49);
                            sub_count++;
                            printf("Client %d SUB topic: %s\n", sd, topic);
                        }
                    } 
                    else if (strncmp(buffer, "UNSUB ", 6) == 0) {
                        char topic[50];
                        sscanf(buffer + 6, "%s", topic);
                        unsubscribe(sd, topic);
                    }
                    else if (strncmp(buffer, "PUB ", 4) == 0) {
                        char topic[50], msg[1024];
                        if (sscanf(buffer + 4, "%s %[^\n]", topic, msg) == 2) {
                            printf("PUB tu %d toi topic [%s]: %s\n", sd, topic, msg);
                            for (int j = 0; j < sub_count; j++) {
                                if (strcmp(subs[j].topic, topic) == 0) {
                                    send(subs[j].client_socket, msg, strlen(msg), 0);
                                    send(subs[j].client_socket, "\n", 1, 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}