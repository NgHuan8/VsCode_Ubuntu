#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h> // Thêm thư viện quản lý luồng

#define PORT 8800
#define BUFFER_SIZE 4096

// Hàm kiểm tra login (giữ nguyên, chỉ cần đảm bảo đọc file an toàn)
int check_login(char *user, char *pass)
{
    FILE *fp;
    char file_user[100];
    char file_pass[100];

    fp = fopen("users.txt", "r");

    if (fp == NULL)
    {
        return 0;
    }

    while (fscanf(fp, "%s %s", file_user, file_pass) != EOF)
    {
        if (strcmp(user, file_user) == 0 &&
            strcmp(pass, file_pass) == 0)
        {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

// Hàm xử lý client đổi thành con trỏ hàm để truyền vào pthread_create
void *handle_client(void *arg)
{
    // Lấy client_socket từ con trỏ void* và giải phóng bộ nhớ
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char username[100];
    char password[100];

    memset(buffer, 0, sizeof(buffer));
    send(client_socket, "Username: ", 10, 0);
    recv(client_socket, username, sizeof(username), 0);
    username[strcspn(username, "\r\n")] = 0;

    send(client_socket, "Password: ", 10, 0);
    recv(client_socket, password, sizeof(password), 0);
    password[strcspn(password, "\r\n")] = 0;

    if (!check_login(username, password))
    {
        send(client_socket, "Login failed!\n", 14, 0);
        close(client_socket);
        // Thay thế exit(0) bằng pthread_exit để không làm sập toàn bộ server
        pthread_exit(NULL); 
    }
    send(client_socket, "Login successful!\n", 19, 0);

    // Tạo tên file output độc lập cho từng luồng (dựa trên client_socket descriptor)
    char out_filename[50];
    snprintf(out_filename, sizeof(out_filename), "out_%d.txt", client_socket);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        send(client_socket, "\ncmd> ", 6, 0);
        int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
        
        if (bytes <= 0)
            break;
            
        buffer[strcspn(buffer, "\r\n")] = 0;
        if (strcmp(buffer, "exit") == 0)
            break;

        char command[BUFFER_SIZE];
        // Ghi kết quả vào file riêng của client này
        snprintf(command, sizeof(command), "%.4000s > %s 2>&1", buffer, out_filename);
        system(command);
        
        FILE *fp = fopen(out_filename, "r");

        if (fp == NULL)
        {
            send(client_socket, "Cannot open output file\n", 24, 0);
            continue;
        }

        char result[BUFFER_SIZE];
        while (fgets(result, sizeof(result), fp) != NULL)
        {
            send(client_socket, result, strlen(result), 0);
        }
        fclose(fp);
    }

    // Dọn dẹp trước khi đóng luồng
    remove(out_filename); // Xóa file rác sau khi client thoát
    close(client_socket);
    printf("Client disconnected. Socket: %d\n", client_socket);
    pthread_exit(NULL);
}

int main()
{
    int server_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("Socket failed");
        return 1;
    }
    
    // Thêm SO_REUSEADDR để tránh lỗi "Address already in use" khi chạy lại server
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    listen(server_socket, 5);
    printf("Telnet Server listening on port %d...\n", PORT);

    while (1)
    {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected on socket: %d\n", client_socket);

        // Cấp phát động bộ nhớ cho client_socket để truyền vào thread an toàn
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        pthread_t thread_id;
        // Tạo luồng mới xử lý client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) != 0) {
            perror("Could not create thread");
            free(new_sock);
            close(client_socket);
            continue;
        }

        // Tách luồng (detach) để hệ điều hành tự động thu hồi tài nguyên khi luồng kết thúc
        pthread_detach(thread_id);
        
        // KHÔNG close(server_socket) hay close(client_socket) ở main loop nữa 
        // vì luồng con dùng chung không gian bộ nhớ với main.
    }
    
    close(server_socket);
    return 0;
}