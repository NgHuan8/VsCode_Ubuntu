#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 8888
#define BUFFER_SIZE 2048

// Hàm xử lý luồng cho một chiều gửi tin nhắn (Từ My_Sock -> Partner_Sock)
void *handle_client(void *arg) {
    int *socks = (int *)arg;
    int my_sock = socks[0];
    int partner_sock = socks[1];
    free(arg); // Giải phóng bộ nhớ mảng tham số ngay lập tức

    char buffer[BUFFER_SIZE];
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        // Chờ nhận tin nhắn từ client hiện tại
        int bytes = recv(my_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes <= 0) {
            printf("[Luồng %lu] Một client đã ngắt kết nối.\n", (unsigned long)pthread_self());
            break; // Thoát vòng lặp nếu client đóng kết nối hoặc có lỗi
        }
        
        // Chuyển tiếp toàn bộ dữ liệu nhận được sang đối tác (Partner)
        send(partner_sock, buffer, bytes, 0);
    }

    // --- Xử lý ngắt kết nối đồng bộ ---
    // Dùng shutdown(SHUT_RDWR) để đánh thức và ép luồng của partner thoát khỏi hàm recv() đang bị block.
    shutdown(my_sock, SHUT_RDWR);
    shutdown(partner_sock, SHUT_RDWR);
    
    // Chỉ close my_sock của luồng này. 
    // Không close(partner_sock) để tránh đóng đúp (Double free/close fd), 
    // vì luồng kia cũng sẽ tự gọi close() cho socket của nó khi bị shutdown.
    close(my_sock);

    pthread_exit(NULL);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 1. Khởi tạo Socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Lỗi tạo socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi Bind");
        return 1;
    }

    // 3. Listen
    if (listen(server_sock, 10) < 0) {
        perror("Lỗi Listen");
        return 1;
    }

    printf("=== CHAT GHÉP CẶP 1-1 ĐANG CHẠY TRÊN PORT %d ===\n", PORT);

    int waiting_client = -1; // Biến lưu trữ client đang chờ

    // 4. Vòng lặp chính quản lý hàng đợi
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Lỗi Accept");
            continue;
        }

        printf("Có người mới kết nối (Socket ID: %d)\n", client_sock);

        if (waiting_client == -1) {
            // Trường hợp 1: Hàng đợi trống -> Cho client này vào chờ
            waiting_client = client_sock;
            char *wait_msg = "He thong: Dang tim kiem doi tac...\n";
            send(waiting_client, wait_msg, strlen(wait_msg), 0);
        } else {
            // Trường hợp 2: Đã có 1 người chờ -> Ghép cặp
            int client1 = waiting_client;
            int client2 = client_sock;
            waiting_client = -1; // Reset hàng đợi để cặp tiếp theo có thể kết nối

            char *match_msg = "He thong: Da ghep cap thanh cong! Ban co the bat dau chat.\n";
            send(client1, match_msg, strlen(match_msg), 0);
            send(client2, match_msg, strlen(match_msg), 0);

            // Tạo tham số cho Luồng 1 (Phục vụ Client 1)
            int *args1 = malloc(2 * sizeof(int));
            args1[0] = client1; 
            args1[1] = client2;

            // Tạo tham số cho Luồng 2 (Phục vụ Client 2)
            int *args2 = malloc(2 * sizeof(int));
            args2[0] = client2; 
            args2[1] = client1;

            pthread_t t1, t2;
            
            // Khởi chạy 2 luồng chuyển tiếp dữ liệu
            if (pthread_create(&t1, NULL, handle_client, args1) != 0 || 
                pthread_create(&t2, NULL, handle_client, args2) != 0) {
                perror("Lỗi tạo luồng ghép cặp");
                free(args1); free(args2);
                close(client1); close(client2);
                continue;
            }

            // Tách luồng để hệ điều hành tự dọn dẹp RAM khi kết thúc
            pthread_detach(t1);
            pthread_detach(t2);
            
            printf("Đã ghép cặp thành công Socket %d và Socket %d\n", client1, client2);
        }
    }

    close(server_sock);
    return 0;
}