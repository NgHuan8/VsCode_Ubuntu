#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h> // Sử dụng cho kỹ thuật Multiplexing

#define PORT 9000
#define MAX_CLIENTS 64
#define BUFFER_SIZE 1024
#define MAX_TOPICS 10

// Cấu trúc quản lý đăng ký của mỗi client
typedef struct {
    int fd;
    char topics[MAX_TOPICS][64]; // Một client có thể đăng ký nhiều chủ đề
    int num_topics;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];

int main() {
    // 1. Tạo socket TCP
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    // Thiết lập địa chỉ server cổng 9000
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    // 2. Gắn socket với địa chỉ
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind() failed");
        return 1;
    }

    // 3. Chuyển sang trạng thái chờ kết nối
    if (listen(listener, 5) == -1) {
        perror("listen() failed");
        return 1;
    }

    // Khởi tạo mảng quản lý client
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    fd_set readfds; // Tập hợp các socket để thăm dò sự kiện
    char buf[BUFFER_SIZE];

    printf("Server Pub/Sub dang chay tai cong %d...\n", PORT);

    while (1) {
        // Khởi tạo tập readfds
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);
        int max_fd = listener;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }

        // 4. Thăm dò sự kiện trên các socket
        int ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0) break;

        // Kiểm tra kết nối mới
        if (FD_ISSET(listener, &readfds)) {
            int client_fd = accept(listener, NULL, NULL);
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    clients[i].fd = client_fd;
                    clients[i].num_topics = 0;
                    printf("Client moi ket noi: %d\n", client_fd);
                    break;
                }
            }
        }

        // Kiểm tra dữ liệu từ client đã kết nối
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0 && FD_ISSET(clients[i].fd, &readfds)) {
                int n = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    printf("Client %d da thoat\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].fd = -1;
                } else {
                    buf[n] = 0;
                    char cmd[16], topic[64], msg[BUFFER_SIZE];
                    
                    // Phân tách lệnh sử dụng sscanf
                    if (sscanf(buf, "%s %s %[^\n]", cmd, topic, msg) >= 2) {
                        if (strcmp(cmd, "SUB") == 0) {
                            // Đăng ký topic
                            if (clients[i].num_topics < MAX_TOPICS) {
                                strcpy(clients[i].topics[clients[i].num_topics++], topic);
                                send(clients[i].fd, "SUB OK\n", 11, 0);
                            }
                        } else if (strcmp(cmd, "UNSUB") == 0) {
                            // Hủy đăng ký
                            for (int j = 0; j < clients[i].num_topics; j++) {
                                if (strcmp(clients[i].topics[j], topic) == 0) {
                                    for (int k = j; k < clients[i].num_topics - 1; k++)
                                        strcpy(clients[i].topics[k], clients[i].topics[k+1]);
                                    clients[i].num_topics--;
                                    send(clients[i].fd, "UNSUB OK\n", 13, 0);
                                    break;
                                }
                            }
                        } else if (strcmp(cmd, "PUB") == 0) {
                            // Định tuyến dữ liệu: Gửi cho tất cả subscriber của topic này
                            char forward_buf[BUFFER_SIZE + 128];
                            sprintf(forward_buf, "[%s]: %s\n", topic, msg);
                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (clients[j].fd > 0) {
                                    for (int k = 0; k < clients[j].num_topics; k++) {
                                        if (strcmp(clients[j].topics[k], topic) == 0) {
                                            send(clients[j].fd, forward_buf, strlen(forward_buf), 0);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}