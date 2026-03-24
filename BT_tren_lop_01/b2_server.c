#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define PATTERN "0123456789"
#define PAT_LEN 10

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1, addrlen = sizeof(address);
    
    char leftover[PAT_LEN] = {0}; 
    int total_count = 0;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    printf("Server tại cổng %d...\n", PORT);
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

    char recv_buf[BUFFER_SIZE];
    while (1) {
        memset(recv_buf, 0, BUFFER_SIZE);
        int n = recv(new_socket, recv_buf, BUFFER_SIZE - 1, 0);
        if (n <= 0) break;

        // 1. Kết hợp phần dư cũ và dữ liệu mới
        char combined[BUFFER_SIZE + PAT_LEN] = {0};
        strcpy(combined, leftover);
        strcat(combined, recv_buf);

        // 2. Đếm xâu mục tiêu
        char *ptr = combined;
        while ((ptr = strstr(ptr, PATTERN)) != NULL) {
            total_count++;
            ptr += 1; // Tìm kiếm các xâu có khả năng gối nhau
        }

        // 3. Cập nhật phần dư cho lần kế tiếp (lấy 9 ký tự cuối)
        int len = strlen(combined);
        memset(leftover, 0, PAT_LEN);
        if (len >= PAT_LEN - 1) {
            strncpy(leftover, combined + len - (PAT_LEN - 1), PAT_LEN - 1);
        } else {
            strcpy(leftover, combined);
        }

        printf("\nNội dung dữ liệu: %s\n", recv_buf);
        printf(" => Tổng số lần xuất hiện '%s' là: %d\n", PATTERN, total_count);
    }

    printf("\nKết nối đóng. Tổng : %d\n", total_count);
    close(new_socket);
    close(server_fd);
    return 0;
}