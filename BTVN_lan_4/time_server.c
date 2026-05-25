#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h> // Thêm thư viện quản lý luồng thay cho signal.h và tiến trình

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 1024

// Đổi chữ ký hàm để phù hợp với pthread_create
void *process_client(void *arg);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Tạo socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Tránh lỗi "Address already in use" khi khởi động lại server
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind và Listen
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, BACKLOG) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Time Server (Multithreaded) is running on port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        // Cấp phát động bộ nhớ cho socket client để truyền vào thread an toàn
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t thread_id;
        
        // Tạo luồng con để xử lý client
        if (pthread_create(&thread_id, NULL, process_client, (void *)new_sock) != 0) {
            perror("Thread creation failed");
            free(new_sock);
            close(client_sock);
            continue;
        }

        // Tách luồng để hệ điều hành tự động thu hồi tài nguyên khi thread kết thúc
        pthread_detach(thread_id);
        
        // KHÔNG close(client_sock) hay close(server_sock) tại đây
        // vì luồng con dùng chung không gian bộ nhớ với main.
    }

    close(server_sock);
    return 0;
}

// Cài đặt hàm xử lý client
void *process_client(void *arg) {
    // Lấy client_sock từ con trỏ void* và giải phóng bộ nhớ ngay
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char cmd[20], format[50], response[150];
    
    // Sử dụng pthread_self() thay cho getpid() để nhận dạng luồng
    pthread_t my_thread_id = pthread_self(); 

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Thread [%lu]: Client disconnected.\n", (unsigned long)my_thread_id);
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("Thread [%lu] received: %s\n", (unsigned long)my_thread_id, buffer);

        int num = sscanf(buffer, "%s %s", cmd, format);

        if (num == 2 && strcmp(cmd, "GET_TIME") == 0) {
            time_t t = time(NULL);
            struct tm *tm_info = localtime(&t);
            char time_str[50];

            if (strcmp(format, "dd/mm/yyyy") == 0) strftime(time_str, 50, "%d/%m/%Y", tm_info);
            else if (strcmp(format, "dd/mm/yy") == 0) strftime(time_str, 50, "%d/%m/%y", tm_info);
            else if (strcmp(format, "mm/dd/yyyy") == 0) strftime(time_str, 50, "%m/%d/%Y", tm_info);
            else if (strcmp(format, "mm/dd/yy") == 0) strftime(time_str, 50, "%m/%d/%y", tm_info);
            else {
                sprintf(response, "[Thread %lu]: Invalid format!\n", (unsigned long)my_thread_id);
                send(client_sock, response, strlen(response), 0);
                continue;
            }

            // Gửi kèm thông tin Thread ID để chứng minh multithreading
            sprintf(response, "[Thread %lu] Result: %s\n", (unsigned long)my_thread_id, time_str);
            send(client_sock, response, strlen(response), 0);
        } else {
            sprintf(response, "[Thread %lu]: Invalid command!\n", (unsigned long)my_thread_id);
            send(client_sock, response, strlen(response), 0);
        }
    }
    
    close(client_sock);
    return NULL; // Thay vì exit(0) để kết thúc luồng một cách an toàn
}