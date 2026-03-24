#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUF_SIZE 2048

int main() {
    int sockfd;
    char send_buffer[BUF_SIZE];
    char recv_buffer[BUF_SIZE];
    struct sockaddr_in servaddr;

    // Tạo socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Kết nối đến localhost
    
    printf("Nhập nội dung (Gõ 'exit' để thoát):\n");

    while (1) {
        printf("> ");
        // Đọc dữ liệu từ bàn phím
        if (fgets(send_buffer, BUF_SIZE, stdin) == NULL) break;

        // Xóa ký tự xuống dòng '\n' ở cuối chuỗi nếu có
        send_buffer[strcspn(send_buffer, "\n")] = 0;

        if (strcmp(send_buffer, "exit") == 0) break;

        // Gửi dữ liệu đi
        sendto(sockfd, send_buffer, strlen(send_buffer), 0, 
               (const struct sockaddr *) &servaddr, sizeof(servaddr));

        // Nhận phản hồi từ Server
        int n = recvfrom(sockfd, recv_buffer, BUF_SIZE - 1, 0, NULL, NULL);
        recv_buffer[n] = '\0';

        printf("Server phản hồi: %s\n", recv_buffer);
    }

    printf("Đã đóng kết nối.\n");
    close(sockfd);
    return 0;
}