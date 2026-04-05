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
    char buffer[BUFFER_SIZE];
    char input[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        return -1;
    }

    printf("--- Da ket noi den Server ĐHBK ---\n");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = recv(sock, buffer, BUFFER_SIZE, 0);
        if (valread <= 0) {
            printf("\nMat ket noi voi Server.\n");
            break;
        }

        // Hien thi cau hoi tu Server 
        printf("%s", buffer);
        fflush(stdout); 

        // Neu Server yeu cau nhap (co chua tu khoa NHAP hoặc MOI)
        if (strstr(buffer, "Moi ban") != NULL || strstr(buffer, "Nhap") != NULL) {
            if (fgets(input, BUFFER_SIZE, stdin) != NULL) {
                send(sock, input, strlen(input), 0);
            }
        }
    }

    close(sock);
    return 0;
}