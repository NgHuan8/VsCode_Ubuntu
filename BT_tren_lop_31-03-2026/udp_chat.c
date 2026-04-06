#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Sử dụng: %s <port_s> <ip_d> <port_d>\n", argv[0]);
        exit(1);
    }

    int port_s = atoi(argv[1]); // Cổng nhận dữ liệu 
    char *ip_d = argv[2];       // IP đích 
    int port_d = atoi(argv[3]); // Cổng đích

    int sockfd;
    struct sockaddr_in servaddr, destaddr;
    char buffer[BUF_SIZE];

    // Tạo socket UDP 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    // Thiết lập non-blocking 
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port_s);

    // Gán cổng port_s để nhận dữ liệu 
    bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET;
    destaddr.sin_port = htons(port_d);
    inet_pton(AF_INET, ip_d, &destaddr.sin_addr);

    printf("--- Chat UDP (Port %d) ---\n", port_s);

    fd_set readfds;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        // Sử dụng select để vừa gửi và nhận trong một ứng dụng 
        select(sockfd + 1, &readfds, NULL, NULL, NULL);

        // NHẬN 
        if (FD_ISSET(sockfd, &readfds)) {
            int n = recvfrom(sockfd, buffer, BUF_SIZE, 0, NULL, NULL);
            if (n > 0) {
                buffer[n] = '\0';
                printf("Partner: %s", buffer); 
                fflush(stdout);
            }
        }

        // GỬI 
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, BUF_SIZE, stdin) != NULL) {

                printf("You: %s", buffer);
                sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
                fflush(stdout);
            }
        }
    }

    close(sockfd);
    return 0;
}