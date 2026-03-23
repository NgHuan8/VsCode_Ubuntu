#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    // Kiểm tra đối số dòng lệnh = 3 (tên chương trình, địa chỉ IP, cổng)
    if (argc != 3) {
        printf("Usage: %s <IP Address> <Port>\n", argv[0]);
        return 1;
    }

    int client_sk = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr; 
    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2])); 

    int res = connect(client_sk, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (res < 0) {
        printf("Connect failed");
        return 1;
    }

    char buffer[1024];
    while (1) {
        printf("Nhập dữ liệu gửi đến server: ");
        fgets(buffer, sizeof(buffer), stdin);
        send(client_sk, buffer, strlen(buffer), 0); 
        if (strncmp(buffer, "exit", 4) == 0) break;
    }

    close(client_sk);
    return 0;
}