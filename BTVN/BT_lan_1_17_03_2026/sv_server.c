#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef struct {
    char mssv[20];
    char ho_ten[50];
    char ngay_sinh[20];
    float diem_tb;
} SinhVien;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <Port> <LogFile>\n", argv[0]);
        return 1;
    }

    int server_sk = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(argv[1]));

    bind(server_sk, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sk, 5);

    while (1) {
        struct sockaddr_in c_addr;
        socklen_t len = sizeof(c_addr);
        int c_sk = accept(server_sk, (struct sockaddr *)&c_addr, &len);
        char *ip = inet_ntoa(c_addr.sin_addr);

        while (1) {
            SinhVien sv;
            int bytes = recv(c_sk, &sv, sizeof(sv), 0);

            // Thoát vòng lặp nếu client ngắt hoặc gửi "exit"
            if (bytes <= 0 || strcmp(sv.mssv, "exit") == 0) {
                printf("Client %s đã ngắt kết nối.\n", ip);
                break;
            }

            // Lấy thời gian hiện tại 
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);

            // Ghi vào file log 
            FILE *f = fopen(argv[2], "a");
            if (f) {
                fprintf(f, "%s %s %s %s %s %.2f\n", ip, time_str, sv.mssv, sv.ho_ten, sv.ngay_sinh, sv.diem_tb);
                fclose(f);
            }
            printf("Đã ghi log cho SV: %s\n", sv.mssv);
        }
        close(c_sk);
    }
    close(server_sk);
    return 0;
}