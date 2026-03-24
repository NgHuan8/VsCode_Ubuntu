#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char input_buf[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) return -1; // Chuyển địa chỉ IP từ dạng text sang binary
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) return -1; // Kết nối đến server

    printf("Đã kết nối. Hãy nhập dữ liệu theo từng dòng:\n");

    int count = 1;
    while (1) {
        printf("Lần gửi thứ %d: ", count);
        if (fgets(input_buf, BUFFER_SIZE, stdin) == NULL) break;

        // Xóa ký tự xuống dòng ]
        input_buf[strcspn(input_buf, "\r\n")] = 0;

        if (strcmp(input_buf, "exit") == 0) break;

        if (strlen(input_buf) > 0) {
            send(sock, input_buf, strlen(input_buf), 0);
        }

        count++;
    }

    close(sock);
    return 0;
}