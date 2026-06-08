#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#define PORT 8080

// Xác định loại file dựa theo phần mở rộng (dùng strrchr để khớp đúng đuôi file)
const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    // Text
    if (!strcasecmp(ext, ".html") || !strcasecmp(ext, ".htm")) return "text/html";
    if (!strcasecmp(ext, ".txt")  || !strcasecmp(ext, ".c")
     || !strcasecmp(ext, ".h")   || !strcasecmp(ext, ".py"))  return "text/plain";
    // Ảnh
    if (!strcasecmp(ext, ".jpg")  || !strcasecmp(ext, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(ext, ".png"))  return "image/png";
    if (!strcasecmp(ext, ".gif"))  return "image/gif";
    if (!strcasecmp(ext, ".bmp"))  return "image/bmp";
    if (!strcasecmp(ext, ".webp")) return "image/webp";
    // Audio
    if (!strcasecmp(ext, ".mp3"))  return "audio/mpeg";
    if (!strcasecmp(ext, ".wav"))  return "audio/wav";
    if (!strcasecmp(ext, ".ogg"))  return "audio/ogg";
    if (!strcasecmp(ext, ".aac"))  return "audio/aac";
    // Video
    if (!strcasecmp(ext, ".mp4"))  return "video/mp4";
    if (!strcasecmp(ext, ".webm")) return "video/webm";
    if (!strcasecmp(ext, ".avi"))  return "video/x-msvideo";
    if (!strcasecmp(ext, ".mkv"))  return "video/x-matroska";
    return "application/octet-stream";
}

// Xử lý gửi danh sách thư mục (Thư mục gốc hoặc thư mục con)
void serve_directory(int client_sock, const char *url_path, const char *real_path) {
    DIR *dir = opendir(real_path);
    if (!dir) return;

    // 1. Gửi Header
    char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    write(client_sock, header, strlen(header));

    // 2. Gửi phần đầu trang Web (kèm CSS tăng cỡ chữ)
    char buffer[8192];
    snprintf(buffer, sizeof(buffer), 
        "<html>"
        "<head>"
        "<style>"
            "body { font-size: 26px; font-family: Arial, sans-serif; line-height: 1.6; } "
            "h2 { font-size: 36px; color: #333; border-bottom: 2px solid #ccc; padding-bottom: 10px; } "
            "ul { list-style-type: none; padding-left: 20px; } "
            "li { margin-bottom: 12px; } "
            "a { text-decoration: none; color: #0066cc; } "
            "a:hover { text-decoration: underline; color: #ff0000; } "
        "</style>"
        "</head>"
        "<body>"
        "<h2>Danh muc cua: %s</h2>"
        "<ul>", 
        url_path);
    write(client_sock, buffer, strlen(buffer));

    // 3. Đọc từng file/thư mục và tạo thẻ HTML tương ứng
    struct dirent *entry;
    struct stat file_stat;
    char full_path[4096];
    char link[4096];

    // Hiển thị liên kết quay lại thư mục cha (nếu không phải thư mục gốc)
    if (strcmp(url_path, "/") != 0) {
        char parent[4096];
        strncpy(parent, url_path, sizeof(parent));
        int plen = strlen(parent);
        if (plen > 1 && parent[plen - 1] == '/') parent[--plen] = '\0'; // bỏ '/' cuối nếu có
        char *last_slash = strrchr(parent, '/');
        if (last_slash) *(last_slash + 1) = '\0'; // giữ lại '/' của thư mục cha
        snprintf(buffer, sizeof(buffer),
            "<li><a href=\"%s\">[.. Quay lai thu muc cha]</a></li>", parent);
        write(client_sock, buffer, strlen(buffer));
    }

    while ((entry = readdir(dir)) != NULL) {
        // Bỏ qua '.' và '..' vì đã có nút quay lại ở trên
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", real_path, entry->d_name);
        stat(full_path, &file_stat);

        // Tạo đường dẫn liên kết cho thẻ href
        if (strcmp(url_path, "/") == 0) {
            snprintf(link, sizeof(link), "/%s", entry->d_name);
        } else {
            snprintf(link, sizeof(link), "%s/%s", url_path, entry->d_name);
        }

        // YÊU CẦU: In đậm đối với thư mục, in nghiêng đối với file
        // Thêm '/' vào cuối href thư mục để server nhận dạng đúng là thư mục
        if (S_ISDIR(file_stat.st_mode)) {
            snprintf(buffer, sizeof(buffer), "<li><a href=\"%s/\"><b>%s/</b></a></li>", link, entry->d_name);
        } else {
            snprintf(buffer, sizeof(buffer), "<li><a href=\"%s\"><i>%s</i></a></li>", link, entry->d_name);
        }
        write(client_sock, buffer, strlen(buffer));
    }

    // 4. Gửi phần cuối trang Web
    char footer[] = "</ul></body></html>";
    write(client_sock, footer, strlen(footer));
    
    closedir(dir);
}

// Xử lý gửi nội dung file (text, ảnh, video...)
void serve_file(int client_sock, const char *real_path) {
    int fd = open(real_path, O_RDONLY);
    if (fd < 0) return;

    struct stat file_stat;
    stat(real_path, &file_stat);

    char header[2048];
    snprintf(header, sizeof(header), 
             "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", 
             get_mime_type(real_path), file_stat.st_size);
    write(client_sock, header, strlen(header));

    char buffer[8192];
    int bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        write(client_sock, buffer, bytes_read);
    }
    close(fd);
}

// Hàm phục vụ Client
void handle_client(int client_sock) {
    char request[4096];
    read(client_sock, request, sizeof(request) - 1);

    char method[16], url_path[2048];
    sscanf(request, "%s %s", method, url_path);

    if (strcmp(method, "GET") != 0) {
        close(client_sock);
        return;
    }

    // YÊU CẦU: Xử lý thư mục gốc và thư mục con
    char real_path[4096];
    snprintf(real_path, sizeof(real_path), ".%s", url_path);

    // Bỏ dấu '/' cuối để stat() nhận dạng đúng trên mọi hệ thống
    int rlen = strlen(real_path);
    if (rlen > 2 && real_path[rlen - 1] == '/') real_path[rlen - 1] = '\0';

    struct stat path_stat;
    if (stat(real_path, &path_stat) == 0) {
        if (S_ISDIR(path_stat.st_mode)) {
            serve_directory(client_sock, url_path, real_path);
        } else if (S_ISREG(path_stat.st_mode)) {
            serve_file(client_sock, real_path);
        }
    }

    close(client_sock);
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 10);

    printf("Server HTTP (Co chu to, In dam/In nghieng) dang chay tai http://localhost:%d\n", PORT);

    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (fork() == 0) {
            close(server_sock);
            handle_client(client_sock);
            exit(0);
        }
        close(client_sock);
    }
    return 0;
}