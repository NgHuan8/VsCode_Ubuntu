#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        printf("Usage: %s <IP> <Port>\n", argv[0]);
        return 1;
    }

    int client_sk = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in s_addr;
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr(argv[1]);
    s_addr.sin_port = htons(atoi(argv[2]));

    if (connect(client_sk, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1) {
        perror("Kết nối thất bại");
        return 1;
    }

    while (1) {
        SinhVien sv;
        memset(&sv, 0, sizeof(sv));

        printf("\nNhập MSSV (gõ 'exit' để dừng): ");
        scanf("%s", sv.mssv);

        // Nếu gõ exit, gửi tín hiệu thoát cho server rồi break
        if (strcmp(sv.mssv, "exit") == 0) {
            send(client_sk, &sv, sizeof(sv), 0);
            break;
        }

        getchar(); // Xóa bộ đệm
        printf("Nhập Họ tên: ");
        fgets(sv.ho_ten, sizeof(sv.ho_ten), stdin);
        sv.ho_ten[strcspn(sv.ho_ten, "\n")] = 0; // Xóa ký tự xuống dòng

        printf("Nhập Ngày sinh (YYYY-MM-DD): ");
        scanf("%s", sv.ngay_sinh);

        printf("Nhập Điểm TB: ");
        scanf("%f", &sv.diem_tb);

        // Gửi toàn bộ struct sang Server 
        send(client_sk, &sv, sizeof(sv), 0);
    }

    printf("Đã đóng kết nối.\n");
    close(client_sk);
    return 0;
}