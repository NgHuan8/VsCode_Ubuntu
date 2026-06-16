#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <ctype.h>

#define PORT 8080
#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define MAX_NICK_LEN 50

// Cấu trúc quản lý một Client
typedef struct {
    int fd;
    char nickname[MAX_NICK_LEN];
    bool is_joined;
    bool is_op;
} ClientSession;

ClientSession clients[MAX_CLIENTS];
char current_topic[BUFFER_SIZE] = "Welcome to the Chat Room!";

// Khởi tạo mảng clients
void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].is_joined = false;
        clients[i].is_op = false;
        memset(clients[i].nickname, 0, MAX_NICK_LEN);
    }
}

// Gửi tin nhắn đến một client cụ thể
void send_to_client(int fd, const char *message) {
    send(fd, message, strlen(message), 0);
}

// Gửi tin nhắn cho toàn bộ client trong phòng (trừ sender nếu skip_sender = true)
void broadcast(const char *message, int sender_fd, bool skip_sender) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != 0 && clients[i].is_joined) {
            if (skip_sender && clients[i].fd == sender_fd) continue;
            send_to_client(clients[i].fd, message);
        }
    }
}

// Kiểm tra nickname hợp lệ (chỉ chữ thường và số)
bool is_valid_nickname(const char *nick) {
    if (strlen(nick) == 0) return false;
    for (int i = 0; nick[i] != '\0'; i++) {
        if (!islower(nick[i]) && !isdigit(nick[i])) {
            return false;
        }
    }
    return true;
}

// Lấy con trỏ client theo nickname
ClientSession* find_client_by_nick(const char *nick) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != 0 && clients[i].is_joined && strcmp(clients[i].nickname, nick) == 0) {
            return &clients[i];
        }
    }
    return NULL;
}

// Kiểm tra xem phòng có ai là OP chưa
bool has_op() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != 0 && clients[i].is_joined && clients[i].is_op) return true;
    }
    return false;
}

// Ngắt kết nối client
void disconnect_client(int index) {
    int fd = clients[index].fd;
    if (clients[index].is_joined) {
        char msg[BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "QUIT %s\n", clients[index].nickname);
        broadcast(msg, fd, true); // Thông báo cho người khác
    }
    close(fd);
    clients[index].fd = 0;
    clients[index].is_joined = false;
    clients[index].is_op = false;
    memset(clients[index].nickname, 0, MAX_NICK_LEN);
}

// Xử lý logic lệnh từ client
void process_command(int client_index, char *buffer) {
    ClientSession *client = &clients[client_index];
    int fd = client->fd;
    char response[BUFFER_SIZE];
    char broadcast_msg[2 * BUFFER_SIZE];

    // Loại bỏ ký tự \r hoặc \n ở cuối chuỗi
    buffer[strcspn(buffer, "\r\n")] = 0;

    // Tách lệnh
    char *cmd = strtok(buffer, " ");
    if (cmd == NULL) return;

    if (strcmp(cmd, "JOIN") == 0) {
        char *nick = strtok(NULL, " ");
        if (nick == NULL) {
            send_to_client(fd, "201 INVALID NICK NAME\n");
            return;
        }

        if (!is_valid_nickname(nick)) {
            send_to_client(fd, "201 INVALID NICK NAME\n");
            return;
        }

        if (find_client_by_nick(nick) != NULL) {
            send_to_client(fd, "200 NICKNAME IN USE\n");
            return;
        }

        strncpy(client->nickname, nick, MAX_NICK_LEN - 1);
        client->is_joined = true;
        
        // Người đầu tiên vào phòng sẽ là OP
        if (!has_op()) client->is_op = true;

        send_to_client(fd, "100 OK\n");
        
        snprintf(broadcast_msg, sizeof(broadcast_msg), "JOIN %s\n", client->nickname);
        broadcast(broadcast_msg, fd, true);
    }
    else if (strcmp(cmd, "MSG") == 0) {
        if (!client->is_joined) return;
        char *msg = strtok(NULL, ""); // Lấy phần còn lại
        if (msg) {
            send_to_client(fd, "100 OK\n");
            snprintf(broadcast_msg, sizeof(broadcast_msg), "MSG %s %s\n", client->nickname, msg);
            broadcast(broadcast_msg, fd, true);
        } else {
            send_to_client(fd, "999 UNKNOWN ERROR\n");
        }
    }
    else if (strcmp(cmd, "PMSG") == 0) {
        if (!client->is_joined) return;
        char *target_nick = strtok(NULL, " ");
        char *msg = strtok(NULL, "");
        if (target_nick && msg) {
            ClientSession *target = find_client_by_nick(target_nick);
            if (target) {
                send_to_client(fd, "100 OK\n");
                snprintf(response, sizeof(response), "PMSG %s %s\n", client->nickname, msg);
                send_to_client(target->fd, response);
            } else {
                send_to_client(fd, "202 UNKNOWN NICKNAME\n");
            }
        } else {
             send_to_client(fd, "999 UNKNOWN ERROR\n");
        }
    }
    else if (strcmp(cmd, "OP") == 0) {
        if (!client->is_joined) return;
        if (!client->is_op) {
            send_to_client(fd, "203 DENIED\n");
            return;
        }
        char *target_nick = strtok(NULL, " ");
        if (target_nick) {
            ClientSession *target = find_client_by_nick(target_nick);
            if (target) {
                client->is_op = false; // Chuyển quyền (người hiện tại mất quyền)
                target->is_op = true;
                send_to_client(fd, "100 OK\n");
                snprintf(broadcast_msg, sizeof(broadcast_msg), "OP %s\n", target->nickname);
                broadcast(broadcast_msg, fd, true);
            } else {
                send_to_client(fd, "202 UNKNOWN NICKNAME\n");
            }
        } else {
            send_to_client(fd, "999 UNKNOWN ERROR\n");
        }
    }
    else if (strcmp(cmd, "KICK") == 0) {
        if (!client->is_joined) return;
        if (!client->is_op) {
            send_to_client(fd, "203 DENIED\n");
            return;
        }
        char *target_nick = strtok(NULL, " ");
        if (target_nick) {
            ClientSession *target = find_client_by_nick(target_nick);
            if (target) {
                send_to_client(fd, "100 OK\n");
                snprintf(broadcast_msg, sizeof(broadcast_msg), "KICK %s %s\n", target->nickname, client->nickname);
                broadcast(broadcast_msg, fd, true);
                
                // Đóng kết nối người bị kick
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == target->fd) {
                        close(clients[i].fd);
                        clients[i].fd = 0;
                        clients[i].is_joined = false;
                        clients[i].is_op = false;
                        memset(clients[i].nickname, 0, MAX_NICK_LEN);
                        break;
                    }
                }
            } else {
                send_to_client(fd, "202 UNKNOWN NICKNAME\n");
            }
        } else {
            send_to_client(fd, "999 UNKNOWN ERROR\n");
        }
    }
    else if (strcmp(cmd, "TOPIC") == 0) {
        if (!client->is_joined) return;
        if (!client->is_op) {
            send_to_client(fd, "203 DENIED\n");
            return;
        }
        char *topic = strtok(NULL, "");
        if (topic) {
            strncpy(current_topic, topic, BUFFER_SIZE - 1);
            send_to_client(fd, "100 OK\n");
            snprintf(broadcast_msg, sizeof(broadcast_msg), "TOPIC %s %s\n", client->nickname, current_topic);
            broadcast(broadcast_msg, fd, true);
        } else {
            send_to_client(fd, "999 UNKNOWN ERROR\n");
        }
    }
    else if (strcmp(cmd, "QUIT") == 0) {
        send_to_client(fd, "100 OK\n");
        disconnect_client(client_index);
    }
    else {
        send_to_client(fd, "999 UNKNOWN ERROR\n");
    }
}

int main() {
    int master_socket, addrlen, new_socket, activity, valread;
    int max_sd, sd;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    fd_set readfds;

    init_clients();

    // Tạo master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Thiết lập option để reuse address
    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket với port 8080
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d \n", PORT);

    // Đang lắng nghe kết nối, tối đa 10 kết nối hàng đợi
    if (listen(master_socket, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);

    // Vòng lặp server chính
    while (true) {
        // Xóa tập socket và thêm master socket
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Thêm các socket của client vào tập readfds
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Đợi có activity trên một trong các socket
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
        }

        // Xử lý kết nối mới
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            
            // Lưu client mới vào mảng
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    break;
                }
            }
        }

        // Xử lý I/O trên các client hiện có
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;

            if (FD_ISSET(sd, &readfds)) {
                memset(buffer, 0, BUFFER_SIZE);
                valread = read(sd, buffer, BUFFER_SIZE - 1);
                
                // Nếu client ngắt kết nối (VD: đóng terminal ngang)
                if (valread == 0) {
                    disconnect_client(i);
                } 
                // Xử lý lệnh từ client
                else {
                    process_command(i, buffer);
                }
            }
        }
    }
    return 0;
}