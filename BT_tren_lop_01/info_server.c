/* Ứng dụng info_client lấy tên thư mục hiện tại, danh
sách các tập tin và kích thước (trên máy client). Các
dữ liệu này sau đó được đóng gói và chuyển sang
info_server.
• Ứng dụng info_server nhận dữ liệu từ info_client,
tách các dữ liệu và in ra màn hình.
• Yêu cầu kích thước dữ liệu được gửi là nhỏ nhất
(không truyền các dữ liệu dư thừa). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Sử dụng: %s <Port>\n", argv[0]);
        return 1;
    }

    int server_sk = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in s_addr;
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = INADDR_ANY;
    s_addr.sin_port = htons(atoi(argv[1]));

    if (bind(server_sk, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1) {
        perror("Bind thất bại");
        return 1;
    }

    listen(server_sk, 5);
    printf("Server đang đợi kết nối tại cổng %s...\n", argv[1]);

    struct sockaddr_in c_addr;
    socklen_t c_len = sizeof(c_addr);
    int client_sk = accept(server_sk, (struct sockaddr *)&c_addr, &c_len);
    if (client_sk == -1) {
        perror("Accept thất bại");
        return 1;
    }

    //1. NHẬN TÊN THƯ MỤC
    unsigned char name_len;
    if (recv(client_sk, &name_len, 1, 0) > 0) {
        char dir_name[256];
        recv(client_sk, dir_name, name_len, 0);
        dir_name[name_len] = '\0';
        printf("--- THƯ MỤC HIỆN TẠI: %s ---\n", dir_name);
        printf("%-30s | %s\n", "Tên tập tin", "Kích thước (bytes)");
    }

    //2. NHẬN DANH SÁCH TẬP TIN
    while (recv(client_sk, &name_len, 1, 0) > 0) {
        if (name_len == 0) break; // Byte 0 báo hiệu kết thúc danh sách

        // Nhận tên file dựa trên độ dài đã đọc
        char file_name[256];
        recv(client_sk, file_name, name_len, 0);
        file_name[name_len] = '\0';

        // Nhận kích thước file (8 bytes - kiểu long)
        long file_size;
        recv(client_sk, &file_size, sizeof(long), 0);

        printf("%-30s | %ld\n", file_name, file_size);
    }

    printf("Đã nhận xong toàn bộ dữ liệu.\n");

    close(client_sk);
    close(server_sk);
    return 0;
}