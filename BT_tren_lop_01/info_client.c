#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>

void send_data(int sock, const char *name, long size, int is_dir) {
    unsigned char name_len = (unsigned char)strlen(name);
    
    // 1. Gửi độ dài tên (1 byte)
    send(sock, &name_len, 1, 0);
    
    // 2. Gửi nội dung tên (chỉ gửi đúng số byte của tên, không dư thừa)
    send(sock, name, name_len, 0);
    
    // 3. Nếu là file thì mới gửi kích thước (8 bytes), thư mục có thể mặc định là 0 hoặc bỏ qua
    if (!is_dir) {
        send(sock, &size, sizeof(long), 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Sử dụng: %s <IP> <Port>\n", argv[0]);
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

    // 1. Lấy và gửi tên thư mục hiện tại
    char cwd[256]; // Buffer để lưu đường dẫn thư mục hiện tại
    if (getcwd(cwd, sizeof(cwd)) != NULL) { 
        char *dir_name = strrchr(cwd, '/'); // Lấy phần tên sau dấu / cuối cùng
        if (dir_name) dir_name++; else dir_name = cwd;
        
        printf("Đang gửi tên thư mục: %s\n", dir_name);
        send_data(client_sk, dir_name, 0, 1); 
    }


    // 2. Duyệt và gửi danh sách tập tin
    DIR *d = opendir("."); 
    struct dirent *dir;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Chỉ xử lý các tập tin thông thường (không phải thư mục con, . hoặc ..)
            if (dir->d_type == DT_REG) {
                struct stat st;
                if (stat(dir->d_name, &st) == 0) {
                    printf("Gửi file: %s (%ld bytes)\n", dir->d_name, st.st_size);
                    send_data(client_sk, dir->d_name, st.st_size, 0);
                }
            }
        }
        closedir(d);
    }

    // Gửi một byte 0 để báo hiệu kết thúc danh sách (Optional)
    unsigned char stop = 0;
    send(client_sk, &stop, 1, 0);

    printf("Đã gửi xong.\n");
    close(client_sk);
    return 0;
}