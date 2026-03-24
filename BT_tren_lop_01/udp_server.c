#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUF_SIZE 2048

int main() {
    char buffer[BUF_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t len;

    // Tạo socket UDP 
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT); //

    // Bind socket với cổng
    bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("Server đang chạy tại port %d...\n", PORT);

    while (1) {
        len = sizeof(cli_addr); 

        // Nhận dữ liệu và lưu lại thông tin người gửi (cli_addr)
        // BUF_SIZE - 1 vì có thể cần thêm ký tự kết thúc xâu  
        int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0, 
                         (struct sockaddr *) &cli_addr, &len);
        
        buffer[n] = '\0'; 
        printf("Nhận từ Client: %s", buffer);

        // Gửi ngược lại chính dữ liệu
        sendto(sockfd, buffer, n, 0, 
               (const struct sockaddr *) &cli_addr, len);
        
        printf("--> Đã phản hồi lại cho Client.\n");
    }

    close(sockfd);
    return 0;
}