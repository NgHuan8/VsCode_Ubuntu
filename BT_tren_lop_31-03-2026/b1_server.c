#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 1024

typedef enum { WAIT_NAME, WAIT_MSSV } ClientState;

typedef struct {
    int fd;
    ClientState state;
    char name[100];
} ClientContext;

// Hàm xử lý tách tên theo định dạng email ĐHBK 
void process_name(char *full_name, char *first_name, char *initials) {
    char temp[256];
    strcpy(temp, full_name);
    char *words[20];
    int count = 0;
    char *token = strtok(temp, " \n\r");
    while (token != NULL) {
        words[count++] = token;
        token = strtok(NULL, " \n\r");
    }
    if (count > 0) {
        strcpy(first_name, words[count - 1]);
        for(int i = 0; first_name[i]; i++) first_name[i] = tolower(first_name[i]);
        int idx = 0;
        for (int i = 0; i < count - 1; i++) initials[idx++] = tolower(words[i][0]);
        initials[idx] = '\0';
    }
}

int main() {
    int server_fd, new_socket, max_sd, sd;
    struct sockaddr_in address;
    fd_set readfds;
    ClientContext clients[FD_SETSIZE];

    for (int i = 0; i < FD_SETSIZE; i++) clients[i].fd = 0;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    printf("Server dang chay tai port %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < FD_SETSIZE; i++) {
            sd = clients[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, NULL, NULL);
            // Server gửi thông điệp hỏi Họ tên ngay khi kết nối 
            char *msg = "NHAP_TEN: Moi ban nhap Ho ten: ";
            send(new_socket, msg, strlen(msg), 0);
            for (int i = 0; i < FD_SETSIZE; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].state = WAIT_NAME;
                    break;
                }
            }
        }

        for (int i = 0; i < FD_SETSIZE; i++) {
            sd = clients[i].fd;
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread <= 0) {
                    close(sd);
                    clients[i].fd = 0;
                } else {
                    buffer[strcspn(buffer, "\r\n")] = 0;
                    if (clients[i].state == WAIT_NAME) {
                        strcpy(clients[i].name, buffer);
                        clients[i].state = WAIT_MSSV;
                        // Server tiếp tục hỏi MSSV 
                        char *msg = "NHAP_MSSV: Moi ban nhap MSSV: ";
                        send(sd, msg, strlen(msg), 0);
                    } 
                    else if (clients[i].state == WAIT_MSSV) {
                        char clean_mssv[20] = {0};
                        int d = 0;
                        for(int k=0; buffer[k]; k++) if(isdigit(buffer[k])) clean_mssv[d++] = buffer[k];
                        
                        if (strlen(clean_mssv) >= 8) {
                            char f_name[50], inits[20], email[256];
                            process_name(clients[i].name, f_name, inits);
                            char year_code[3] = {clean_mssv[2], clean_mssv[3], '\0'};
                            char last_four[5];
                            strcpy(last_four, &clean_mssv[strlen(clean_mssv)-4]);

                            snprintf(email, sizeof(email), "\n=> Email cua ban: %s.%s%s%s@sis.hust.edu.vn\n\nNHAP_TEN: Nhap Ho ten moi: ", f_name, inits, year_code, last_four);
                            send(sd, email, strlen(email), 0);
                        } else {
                            char *err = "LOI: MSSV can 8 so. Nhap lai MSSV: ";
                            send(sd, err, strlen(err), 0);
                        }
                        clients[i].state = WAIT_NAME;
                    }
                }
            }
        }
    }
    return 0;
}