#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 8888
#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define NAME_SIZE 100

// Cấu trúc quản lý thông tin mỗi client
typedef struct {
    int sockfd;
    struct sockaddr_in address;
    char name[NAME_SIZE];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Thêm client vào mảng quản lý
void queue_add(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Xóa client khỏi mảng quản lý
void queue_remove(int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->sockfd == sockfd) {
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Gửi tin nhắn tới tất cả client ngoại trừ người gửi
void send_message(char *s, int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->sockfd != uid) {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0) {
                    perror("LỖI: write thất bại");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Hàm lấy thời gian hiện tại định dạng YYYY/MM/DD HH:MM:SS
void get_current_time(char *buffer, size_t max_len) {
    time_t timer;
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);

    strftime(buffer, max_len, "%Y/%m/%d %H:%M:%S", tm_info);
}

// Xóa ký tự xuống dòng (\n hoặc \r) ở cuối chuỗi
void strip_newline(char *s) {
    while (*s) {
        if (*s == '\r' || *s == '\n') {
            *s = '\0';
            break;
        }
        s++;
    }
}

// Kiểm tra xem chuỗi có chứa khoảng trắng hay không
int has_space(const char *s) {
    while (*s) {
        if (*s == ' ' || *s == '\t') return 1;
        s++;
    }
    return 0;
}

// Luồng xử lý riêng cho từng client
void *handle_client(void *arg) {
    char buff_out[BUFFER_SIZE];
    char buff_in[BUFFER_SIZE];
    char time_str[50];
    int leave_flag = 0;

    client_t *cli = (client_t *)arg;

    // Bước 1: Hỏi tên và kiểm tra cú pháp "client_id: client_name"
    while (1) {
        char ask_msg[] = "Vui long nhap ten theo cu phap (client_id: client_name): ";
        write(cli->sockfd, ask_msg, strlen(ask_msg));

        memset(buff_in, 0, BUFFER_SIZE);
        int receive = recv(cli->sockfd, buff_in, BUFFER_SIZE - 1, 0);
        
        if (receive <= 0) {
            leave_flag = 1;
            break;
        }

        strip_newline(buff_in);

        // Kiểm tra tiền tố "client_id: "
        if (strncmp(buff_in, "client_id: ", 11) == 0) {
            char *name_part = buff_in + 11;
            
            // Kiểm tra xem tên có viết liền không (không chứa khoảng trắng) và không rỗng
            if (strlen(name_part) > 0 && !has_space(name_part)) {
                strncpy(cli->name, name_part, NAME_SIZE - 1);
                
                // Thông báo đăng nhập thành công cho client đó
                char welcome[BUFFER_SIZE];
                sprintf(welcome, "--- Chào mừng %s đã tham gia phòng chat! ---\n", cli->name);
                write(cli->sockfd, welcome, strlen(welcome));
                
                // Thông báo cho các client khác
                get_current_time(time_str, sizeof(time_str));
                sprintf(buff_out, "[%s] Hệ thống: %s đã tham gia phòng chat.\n", time_str, cli->name);
                send_message(buff_out, cli->sockfd);
                break;
            }
        }
        
        // Nếu sai cú pháp, vòng lặp tiếp tục lặp lại để hỏi lại tên
        char error_msg[] = "Sai cu phap hoac ten chua khoang trang! Thử lại.\n";
        write(cli->sockfd, error_msg, strlen(error_msg));
    }

    // Bước 2: Vòng lặp nhận và chuyển tiếp tin nhắn (Chat)
    while (1) {
        if (leave_flag) break;

        memset(buff_in, 0, BUFFER_SIZE);
        int receive = recv(cli->sockfd, buff_in, BUFFER_SIZE - 1, 0);

        if (receive > 0) {
            if (strlen(buff_in) > 0) {
                strip_newline(buff_in);
                // Bỏ qua nếu client chỉ ấn Enter gửi tin nhắn rỗng
                if (strlen(buff_in) == 0) continue; 

                get_current_time(time_str, sizeof(time_str));
                // Định dạng đầu ra: "2026/05/19 21:00:00 abc: xin chao"
                snprintf(buff_out, sizeof(buff_out), "%s %.31s: %.1900s\n", time_str, cli->name, buff_in);
                send_message(buff_out, cli->sockfd);
                
                // In log ra màn hình server để theo dõi
                printf("%s", buff_out);
            }
        } else if (receive == 0 || strcmp(buff_in, "exit") == 0) {
            get_current_time(time_str, sizeof(time_str));
            sprintf(buff_out, "[%s] Hệ thống: %s đã rời phòng chat.\n", time_str, cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->sockfd);
            leave_flag = 1;
        } else {
            perror("LỖI: recv thất bại");
            leave_flag = 1;
        }
    }

    // Gỡ cài đặt khi client ngắt kết nối
    close(cli->sockfd);
    queue_remove(cli->sockfd);
    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}

int main() {
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // Khởi tạo Socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Bỏ qua lỗi "Address already in use" khi khởi động lại server nhanh
    int option = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option));

    // Bind socket vào cổng chỉ định
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("LỖI: Bind thất bại");
        return EXIT_FAILURE;
    }

    // Lắng nghe kết nối
    if (listen(listenfd, 10) < 0) {
        perror("LỖI: Listen thất bại");
        return EXIT_FAILURE;
    }

    printf("=== CHAT SERVER ĐANG CHẠY TRÊN PORT %d ===\n", PORT);

    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // Kiểm tra số lượng client tối đa
        if (connfd > 0) {
            client_t *cli = (client_t *)malloc(sizeof(client_t));
            cli->address = cli_addr;
            cli->sockfd = connfd;
            memset(cli->name, 0, NAME_SIZE);

            queue_add(cli);
            // Tạo một luồng mới (Thread) độc lập để xử lý client vừa kết nối
            pthread_create(&tid, NULL, &handle_client, (void*)cli);
        }
        
        // Giảm tải cho CPU
        usleep(1000);
    }

    return EXIT_SUCCESS;
}