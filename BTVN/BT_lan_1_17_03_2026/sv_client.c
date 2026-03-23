#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        printf("Usage: %s <IP> <Port>\n", argv[0]);
        return 1;
    }

    int client_sk = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_port = htons(atoi(argv[2]));

    connect(client_sk, (struct sockaddr *)&addr, sizeof(addr));

    SinhVien sv;
    printf("MSSV: "); scanf("%s", sv.mssv); getchar();
    printf("Họ tên: "); fgets(sv.ho_ten, sizeof(sv.ho_ten), stdin);
    sv.ho_ten[strcspn(sv.ho_ten, "\n")] = 0; // Xóa ký tự xuống dòng
    printf("Ngày sinh (YYYY-MM-DD): "); scanf("%s", sv.ngay_sinh);
    printf("Điểm TB: "); scanf("%f", &sv.diem_tb);

    // Gửi toàn bộ struct (đóng gói dữ liệu) [cite: 14]
    send(client_sk, &sv, sizeof(sv), 0);

    close(client_sk);
    return 0;
}