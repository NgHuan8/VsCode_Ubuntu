#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    // 1. Kiểm tra tham số dòng lệnh (phải có đủ 4 tham số) 
    if (argc != 4) {
        printf("Usage: %s <Port> <GreetingFile> <LogFile>\n", argv[0]);
        return 1;
    }

    // 2. Tạo socket cho Server 
    int server_sk = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // Chấp nhận kết nối từ mọi IP của máy
    addr.sin_port = htons(atoi(argv[1])); // Cổng lấy từ tham số thứ nhất 

    // 3. Gắn Socket với cổng và bắt đầu lắng nghe 
    bind(server_sk, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sk, 5); 

    printf("Server đang đợi kết nối ở cổng %s...\n", argv[1]);

    // 4. Chấp nhận kết nối từ Client
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sk = accept(server_sk, (struct sockaddr *)&client_addr, &client_len);
    printf("Có client kết nối từ IP: %s\n", inet_ntoa(client_addr.sin_addr));

    // 5. Gửi xâu chào từ tệp tin chỉ định 
    FILE *f_greet = fopen(argv[2], "r");
    if (f_greet != NULL) {
        char greet_buf[1024];
        // Đọc nội dung file chào và gửi sang client
        while (fgets(greet_buf, sizeof(greet_buf), f_greet) != NULL) {
            send(client_sk, greet_buf, strlen(greet_buf), 0);
        }
        fclose(f_greet);
    } else {
        perror("Không mở được file câu chào");
    }

    // 6. Ghi nội dung client gửi đến vào một tệp tin khác 
    FILE *f_log = fopen(argv[3], "w"); // Mở file log để ghi
    if (f_log != NULL) {
        char recv_buf[1024];
        int bytes_received;
        
        while ((bytes_received = recv(client_sk, recv_buf, sizeof(recv_buf) - 1, 0)) > 0) {
            recv_buf[bytes_received] = '\0'; // Kết thúc chuỗi
            fprintf(f_log, "%s", recv_buf);  // Ghi vào file log 
            fflush(f_log);                   // Đảm bảo dữ liệu được ghi ngay lập tức
            printf("Đã nhận và ghi: %s", recv_buf);
        }
        fclose(f_log);
    }

    // 7. Đóng kết nối
    close(client_sk);
    close(server_sk);
    return 0;
}