#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define SHARED_DIR "./shared_files"

// Hàm xử lý từng client riêng biệt
void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg); // Giải phóng bộ nhớ con trỏ socket ngay lập tức

    DIR *d;
    struct dirent *dir;
    int file_count = 0;
    char file_list[BUFFER_SIZE] = "";
    char response[BUFFER_SIZE + 100];

    // 1. Quét thư mục và đếm số lượng file
    d = opendir(SHARED_DIR);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Chỉ lấy file thông thường (bỏ qua thư mục . và ..)
            if (dir->d_type == DT_REG) {
                file_count++;
                strcat(file_list, dir->d_name);
                strcat(file_list, "\r\n");
            }
        }
        closedir(d);
    }

    // 2. Xử lý kịch bản không có file nào
    if (file_count == 0) {
        char *err_msg = "ERROR No files to download\r\n";
        send(client_sock, err_msg, strlen(err_msg), 0);
        close(client_sock);
        printf("[Luồng %lu] Không có file. Đã ngắt kết nối.\n", (unsigned long)pthread_self());
        pthread_exit(NULL);
    }

    // 3. Gửi danh sách file nếu có
    snprintf(response, sizeof(response), "OK %d\r\n%s\r\n", file_count, file_list);
    send(client_sock, response, strlen(response), 0);

    // 4. Vòng lặp chờ client yêu cầu tải file
    char filename[256];
    char filepath[512];
    
    while (1) {
        memset(filename, 0, sizeof(filename));
        int bytes_received = recv(client_sock, filename, sizeof(filename) - 1, 0);
        
        if (bytes_received <= 0) {
            printf("[Luồng %lu] Client ngắt kết nối.\n", (unsigned long)pthread_self());
            break; // Thoát nếu client đóng kết nối
        }

        // Xóa ký tự xuống dòng ở cuối tên file do client gửi lên
        filename[strcspn(filename, "\r\n")] = 0;
        
        // Tạo đường dẫn tuyệt đối tới file
        snprintf(filepath, sizeof(filepath), "%s/%s", SHARED_DIR, filename);

        struct stat file_stat;
        // Kiểm tra xem file có tồn tại không
        if (stat(filepath, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            // File tồn tại, gửi kích thước trước
            char ok_msg[128];
            snprintf(ok_msg, sizeof(ok_msg), "OK %ld\r\n", file_stat.st_size);
            send(client_sock, ok_msg, strlen(ok_msg), 0);

            // Mở file và gửi nội dung thành từng khối (chunk)
            int fd = open(filepath, O_RDONLY);
            if (fd >= 0) {
                char file_buf[BUFFER_SIZE];
                ssize_t bytes_read;
                while ((bytes_read = read(fd, file_buf, sizeof(file_buf))) > 0) {
                    send(client_sock, file_buf, bytes_read, 0);
                }
                close(fd);
                printf("[Luồng %lu] Đã gửi file '%s' thành công. Đóng kết nối.\n", (unsigned long)pthread_self(), filename);
            }
            
            break; // Đóng kết nối theo yêu cầu đề bài sau khi tải xong file
        } else {
            // File không tồn tại, gửi lỗi và yêu cầu gửi lại (vòng lặp tiếp tục)
            char *not_found_msg = "ERROR File not found. Vui long gui lai ten file:\r\n";
            send(client_sock, not_found_msg, strlen(not_found_msg), 0);
            printf("[Luồng %lu] Client yêu cầu file không tồn tại: '%s'\n", (unsigned long)pthread_self(), filename);
        }
    }

    close(client_sock);
    pthread_exit(NULL);
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Khởi tạo thư mục shared_files nếu chưa có (quyền 0777)
    mkdir(SHARED_DIR, 0777);

    // 1. Tạo socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Lỗi tạo socket");
        return 1;
    }

    // Tái sử dụng port
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Cấu hình địa chỉ
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 3. Bind
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi Bind");
        return 1;
    }

    // 4. Listen
    if (listen(server_sock, 10) < 0) {
        perror("Lỗi Listen");
        return 1;
    }

    printf("=== FILE SERVER ĐANG CHẠY TRÊN PORT %d ===\n", PORT);
    printf("Thư mục chia sẻ: %s\n", SHARED_DIR);

    // 5. Vòng lặp chờ kết nối
    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Lỗi Accept");
            continue;
        }

        // Cấp phát bộ nhớ cho biến client_sock để tránh Race Condition
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t thread_id;
        // Tạo luồng xử lý
        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) != 0) {
            perror("Lỗi tạo luồng");
            free(new_sock);
            close(client_sock);
            continue;
        }

        // Tách luồng (Detach) để tự dọn dẹp RAM
        pthread_detach(thread_id);
    }

    close(server_sock);
    return 0;
}