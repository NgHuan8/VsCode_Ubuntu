#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 1024

void process_client(int client_sock);

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

    printf("Time Server is running on port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        // Tạo tiến trình con để xử lý client
        pid_t pid = fork();
        if (pid == 0) { 
            // Tiến trình con
            close(server_sock); 
            process_client(client_sock);
            exit(0); 
        } else if (pid > 0) {
            // Tiến trình cha
            close(client_sock); 
        } else {
            perror("Fork failed");
            close(client_sock);
        }
    }

    return 0;
}

void process_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    char cmd[20], format[50], response[150]; // Tăng kích thước buffer response
    pid_t my_pid = getpid(); // Lấy ID của tiến trình con hiện tại

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Process [%d]: Client disconnected.\n", my_pid);
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("Process [%d] received: %s\n", my_pid, buffer);

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
                sprintf(response, "[Process %d]: Invalid format!\n", my_pid);
                send(client_sock, response, strlen(response), 0);
                continue;
            }

            // Gửi kèm thông tin PID để chứng minh multiprocessing
            sprintf(response, "[Process %d] Result: %s\n", my_pid, time_str);
            send(client_sock, response, strlen(response), 0);
        } else {
            sprintf(response, "[Process %d]: Invalid command!\n", my_pid);
            send(client_sock, response, strlen(response), 0);
        }
    }
    close(client_sock);
}