#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define FTP_SERVER "lebavui.io.vn"
#define FTP_PORT 21
#define USERNAME "user_20235338"
#define PASSWORD "533825" // Thay XX bằng đúng 2 chữ số ngày sinh của bạn

// Hàm đảo ngược chuỗi ký tự
void reverse_string(char *str, int len) {
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

// Hàm khởi tạo và kết nối TCP Socket
int create_tcp_socket(const char *hostname, int port) {
    struct sockaddr_in server_addr;
    struct hostent *server;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    server = gethostbyname(hostname);
    if (server == NULL) return -1;
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);
    
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

// Hàm gửi lệnh PASV và tính toán Data Port trả về
int get_passive_port(int control_sock) {
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    
    send(control_sock, "PASV\r\n", 6, 0);
    recv(control_sock, buf, sizeof(buf) - 1, 0);
    printf("[Server] %s", buf);
    
    int ip1, ip2, ip3, ip4, p1, p2;
    char *start = strchr(buf, '(');
    if (start != NULL) {
        sscanf(start, "(%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &p1, &p2);
        return (p1 * 256) + p2;
    }
    return -1;
}

int main() {
    char buffer[1024];
    char filename[128] = {0};
    char file_content[128] = {0};
    int data_port, data_sock;

    // 1. Kết nối kênh điều khiển (Control Channel)
    int control_sock = create_tcp_socket(FTP_SERVER, FTP_PORT);
    if (control_sock < 0) { perror("Kết nối thất bại"); return 1; }
    recv(control_sock, buffer, sizeof(buffer) - 1, 0);

    // 2. Đăng nhập hệ thống
    sprintf(buffer, "USER %s\r\n", USERNAME);
    send(control_sock, buffer, strlen(buffer), 0);
    recv(control_sock, buffer, sizeof(buffer) - 1, 0);

    sprintf(buffer, "PASS %s\r\n", PASSWORD);
    send(control_sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    recv(control_sock, buffer, sizeof(buffer) - 1, 0);
    printf("[Login Status] %s", buffer);

    // 3. Lấy tên file ngẫu nhiên dạng question_xxxxxx.txt
    data_port = get_passive_port(control_sock);
    data_sock = create_tcp_socket(FTP_SERVER, data_port);
    
    send(control_sock, "NLST\r\n", 6, 0);
    
    // Nhận phản hồi "150 Here comes the directory listing" từ Control Channel
    memset(buffer, 0, sizeof(buffer));
    recv(control_sock, buffer, sizeof(buffer) - 1, 0); 
    
    // Đọc chính xác tên file từ kênh DATA CHANNEL
    char data_buffer[1024] = {0};
    recv(data_sock, data_buffer, sizeof(data_buffer) - 1, 0); 
    close(data_sock); // Đóng luôn sau khi nhận xong dữ liệu trên kênh Data
    
    // Nhận nốt phản hồi "226 Directory send OK" trên Control Channel để giải phóng bộ đệm
    memset(buffer, 0, sizeof(buffer));
    recv(control_sock, buffer, sizeof(buffer) - 1, 0); 

    // Tách lấy tên file chuẩn từ dữ liệu của Data Channel
    sscanf(data_buffer, "%s", filename);
    printf("[+] Phát hiện file mục tiêu chuẩn: %s\n", filename);

    if (strlen(filename) == 0 || strstr(filename, "question") == NULL) {
        printf("[-] Lỗi: Không lấy đúng tên file mẫu. Vui lòng thử lại.\n");
        close(control_sock);
        return 1;
    }

    // 4. Tải nội dung file question về máy
    data_port = get_passive_port(control_sock);
    data_sock = create_tcp_socket(FTP_SERVER, data_port);
    
    sprintf(buffer, "RETR %s\r\n", filename);
    send(control_sock, buffer, strlen(buffer), 0);
    
    memset(buffer, 0, sizeof(buffer));
    recv(control_sock, buffer, sizeof(buffer) - 1, 0); // Đọc mã 150
    
    int bytes = recv(data_sock, file_content, sizeof(file_content) - 1, 0);
    file_content[bytes] = '\0';
    printf("[+] Nội dung gốc nhận được (100 ký tự): %s\n", file_content);
    close(data_sock);
    
    memset(buffer, 0, sizeof(buffer));
    recv(control_sock, buffer, sizeof(buffer) - 1, 0); // Đọc mã 226

    // 5. Đảo ngược nội dung chuỗi và chuẩn bị tên file trả lời
    char answer_filename[128];
    // Tìm vị trí chữ "question" để cắt chuỗi an toàn
    char *sub_name = strstr(filename, "question");
    sprintf(answer_filename, "answer_%s", sub_name + 9); 
    
    reverse_string(file_content, strlen(file_content));
    printf("[+] Nội dung sau khi đảo ngược: %s\n", file_content);

    // 6. Upload file answer lên FTP server
    data_port = get_passive_port(control_sock);
    data_sock = create_tcp_socket(FTP_SERVER, data_port);
    
    sprintf(buffer, "STOR %s\r\n", answer_filename);
    send(control_sock, buffer, strlen(buffer), 0);
    
    memset(buffer, 0, sizeof(buffer));
    recv(control_sock, buffer, sizeof(buffer) - 1, 0); // Đọc mã 150
    
    send(data_sock, file_content, strlen(file_content), 0);
    close(data_sock); // Đóng data_sock để báo hiệu kết thúc truyền dữ liệu file
    
    memset(buffer, 0, sizeof(buffer));
    recv(control_sock, buffer, sizeof(buffer) - 1, 0); // Đọc mã 226
    printf("[+] Upload thành công file: %s\n", answer_filename);

    // 7. Thoát chương trình sạch sẽ
    send(control_sock, "QUIT\r\n", 6, 0);
    close(control_sock);
    return 0;
}