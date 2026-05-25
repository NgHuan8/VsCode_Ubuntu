#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h> // Sử dụng thư viện pthread thay cho sys/wait.h

#define PORT 8000
#define BUFFER_SIZE 4096

// Đổi chữ ký hàm để phù hợp với yêu cầu của pthread_create
void *handle_client(void *arg) {
    // 1. Lấy socket id từ con trỏ arg và giải phóng bộ nhớ ngay lập tức
    int client = *(int *)arg;
    free(arg);

    char buf[BUFFER_SIZE];

    // Nhận request từ client
    int ret = recv(client, buf, sizeof(buf) - 1, 0);
    if (ret <= 0) {
        close(client);
        pthread_exit(NULL); // Thoát luồng an toàn thay vì exit(0)
    }

    buf[ret] = '\0';
    char method[16];
    char path[256];

    sscanf(buf, "%s %s", method, path);

    // Bỏ qua request xin icon mặc định của trình duyệt
    if (strcmp(path, "/favicon.ico") == 0) {
        close(client);
        pthread_exit(NULL);
    }

    // In ra Thread ID thay vì Process ID (PID)
    printf("[Thread %lu] %s %s\n", (unsigned long)pthread_self(), method, path);

    // Nội dung HTML trả về (Đổi nội dung để nhận biết server đa luồng)
    char *html = "<html>"
                 "<body>"
                 "<h1>Xin chao cac ban</h1>"
                 "<h2>HTTP Multithreading Server</h2>"
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

    printf("Client processed by Thread: %lu\n", (unsigned long)pthread_self());
    return NULL; // Kết thúc luồng
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

    // Tùy chọn: Cho phép tái sử dụng port ngay lập tức
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

        // Cấp phát động biến con trỏ để truyền vào luồng
        // Việc này ngăn chặn Race Condition khi nhiều client kết nối cùng lúc
        int *client_ptr = malloc(sizeof(int));
        *client_ptr = client;

        pthread_t tid;
        // Tạo luồng xử lý
        if (pthread_create(&tid, NULL, handle_client, (void *)client_ptr) != 0) {
            perror("pthread_create");
            free(client_ptr);
            close(client);
            continue;
        }

        // Tách luồng (detach) để OS tự động thu hồi tài nguyên sau khi luồng kết thúc,
        // không cần dùng waitpid() như mô hình tiến trình.
        pthread_detach(tid);
        
        // KHÔNG close(client) ở vòng lặp main vì main thread và child thread chia sẻ chung file descriptor.
    }

    close(listener);
    return 0;
}