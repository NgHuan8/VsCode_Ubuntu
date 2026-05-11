#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PORT 8000
#define BUFFER_SIZE 4096


void handle_client(int client) {
    char buf[BUFFER_SIZE];

    // Nhận request từ client
    int ret = recv(client, buf, sizeof(buf) - 1, 0);
    if (ret <= 0) {
        close(client);
        exit(0);
    }

    buf[ret] = '\0';
    char method[16];
    char path[256];

    sscanf(buf, "%s %s", method, path);

    if (strcmp(path, "/favicon.ico") == 0)
    {
        close(client);
        exit(0);
    }

    printf("[PID %d] %s %s\n",getpid(), method, path);

    // Nội dung HTML trả về
    char *html = "<html>"
                 "<body>"
                 "<h1>Xin chao cac ban</h1>"
                 "<h2>HTTP Multiprocessing Server</h2>"
                 "</body>"
                 "</html>";

    // Xây dựng HTTP Response Header và Body
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n"
             "\r\n"
             "%s",
             strlen(html), html);

    send(client, response, strlen(response), 0);
    close(client);

    printf("Client processed by PID: %d\n", getpid());
    exit(0);
}


int main() {
    int listener;
    struct sockaddr_in server_addr;

    // 1. Tạo socket
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket");
        return 1;
    }

    // Tùy chọn: Cho phép tái sử dụng port ngay lập tức (tránh lỗi Address already in use)
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Cấu hình địa chỉ server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 3. Bind
    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    // 4. Listen
    if (listen(listener, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("HTTP Server listening on port %d...\n", PORT);

    // 5. Server loop
    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            perror("accept");
            continue;
        }

        printf("New client connected: %d\n", client);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client);
            continue;
        }

        if (pid == 0) {
            // Tiến trình con (Child)
            close(listener); // Con không cần giữ socket lắng nghe
            handle_client(client);
        } else {
            // Tiến trình cha (Parent)
            close(client); // Cha bàn giao client cho con, đóng socket này lại
            
            // Dọn dẹp các tiến trình con đã kết thúc (tránh zombie)
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(listener);
    return 0;
}