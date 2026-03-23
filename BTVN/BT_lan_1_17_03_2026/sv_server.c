#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef struct {
    char mssv[10];
    char ho_ten[50];
    char ngay_sinh[11];
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

        SinhVien sv;
        recv(c_sk, &sv, sizeof(sv), 0);

        // Lấy thời gian hiện tại [cite: 17]
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);

        // In ra màn hình và ghi file theo định dạng ví dụ [cite: 16, 19, 20]
        char *ip = inet_ntoa(c_addr.sin_addr);
        printf("%s %s %s %s %s %.2f\n", ip, time_str, sv.mssv, sv.ho_ten, sv.ngay_sinh, sv.diem_tb);

        FILE *f = fopen(argv[2], "a");
        fprintf(f, "%s %s %s %s %s %.2f\n", ip, time_str, sv.mssv, sv.ho_ten, sv.ngay_sinh, sv.diem_tb);
        fclose(f);

        close(c_sk);
    }

    return 0;
}