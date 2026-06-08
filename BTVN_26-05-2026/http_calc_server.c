#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 4096

// HTML Header có chứa CSS để phóng to và làm đẹp giao diện
const char *html_header = 
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
    "<html><head><meta charset='UTF-8'><title>Máy tính HTTP</title>"
    "<style>"
    "  body { font-family: Arial, sans-serif; font-size: 18px; line-height: 1.6; max-width: 800px; margin: 30px auto; padding: 0 20px; }"
    "  h2 { font-size: 24px; color: #333; margin-top: 20px; }"
    "  input[type='number'], select { font-size: 18px; padding: 8px 12px; margin: 5px; border: 1px solid #ccc; border-radius: 4px; }"
    "  input[type='submit'] { font-size: 18px; padding: 8px 16px; background-color: #007BFF; color: white; border: none; border-radius: 4px; cursor: pointer; margin-left: 5px; }"
    "  input[type='submit']:hover { background-color: #0056b3; }"
    "  .result-box { font-size: 20px; font-weight: bold; margin: 15px 5px; padding: 12px; border-radius: 4px; display: inline-block; }"
    "  .success { color: #155724; background-color: #d4edda; border: 1px solid #c3e6cb; }"
    "  .error { color: #721c24; background-color: #f8d7da; border: 1px solid #f5c6cb; }"
    "  hr { border: 0; height: 1px; background: #ccc; margin: 30px 0; }"
    "</style>"
    "</head><body>";

const char *html_get_form = 
    "<h2>Phép tính qua lệnh GET</h2>"
    "<form method='GET' action='/'>"
    "Toán hạng 1: <input type='number' name='a' required> "
    "Phép toán: <select name='op'>"
    "<option value='add'>Cộng (+)</option>"
    "<option value='sub'>Trừ (-)</option>"
    "<option value='mul'>Nhân (*)</option>"
    "<option value='div'>Chia (/)</option>"
    "</select> "
    "Toán hạng 2: <input type='number' name='b' required> "
    "<input type='submit' value='Tính bằng GET'>"
    "</form>";

const char *html_post_form = 
    "<h2>Phép tính qua lệnh POST</h2>"
    "<form method='POST' action='/'>"
    "Toán hạng 1: <input type='number' name='a' required> "
    "Phép toán: <select name='op'>"
    "<option value='add'>Cộng (+)</option>"
    "<option value='sub'>Trừ (-)</option>"
    "<option value='mul'>Nhân (*)</option>"
    "<option value='div'>Chia (/)</option>"
    "</select> "
    "Toán hạng 2: <input type='number' name='b' required> "
    "<input type='submit' value='Tính bằng POST'>"
    "</form>";

const char *html_footer = "</body></html>";

// Hàm trích xuất giá trị từ chuỗi truy vấn (query string)
void get_param_value(const char *query, const char *param, char *value) {
    char *pos = strstr(query, param);
    if (pos) {
        pos += strlen(param) + 1; // Bỏ qua "param="
        int i = 0;
        while (pos[i] != '&' && pos[i] != ' ' && pos[i] != '\0' && pos[i] != '\r') {
            value[i] = pos[i];
            i++;
        }
        value[i] = '\0';
    } else {
        value[0] = '\0';
    }
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        char method[10], path[1024];
        sscanf(buffer, "%s %s", method, path);

        char a_str[20], b_str[20], op[10];
        int has_data = 0;
        int is_get = 0;
        int is_post = 0;

        // Xử lý lệnh GET
        if (strcmp(method, "GET") == 0 && strchr(path, '?') != NULL) {
            char *query = strchr(path, '?') + 1;
            get_param_value(query, "a", a_str);
            get_param_value(query, "b", b_str);
            get_param_value(query, "op", op);
            has_data = 1;
            is_get = 1;
        }
        // Xử lý lệnh POST
        else if (strcmp(method, "POST") == 0) {
            char *body = strstr(buffer, "\r\n\r\n");
            if (body) {
                body += 4;
                get_param_value(body, "a", a_str);
                get_param_value(body, "b", b_str);
                get_param_value(body, "op", op);
                has_data = 1;
                is_post = 1;
            }
        }

        // Chuỗi chứa kết quả riêng biệt cho từng form
        char get_result_html[1024] = "";
        char post_result_html[1024] = "";

        // Nếu có dữ liệu tính toán, xử lý và gán vào đúng form gửi lên
        if (has_data && strlen(a_str) > 0 && strlen(b_str) > 0) {
            double a = atof(a_str);
            double b = atof(b_str);
            double result = 0;
            char operator_char = '?';
            char error_msg[100] = "";

            if (strcmp(op, "add") == 0) { result = a + b; operator_char = '+'; }
            else if (strcmp(op, "sub") == 0) { result = a - b; operator_char = '-'; }
            else if (strcmp(op, "mul") == 0) { result = a * b; operator_char = '*'; }
            else if (strcmp(op, "div") == 0) {
                if (b == 0) strcpy(error_msg, "Lỗi: Không thể chia cho 0!");
                else { result = a / b; operator_char = '/'; }
            }

            // Tạo chuỗi HTML kết quả tạm thời
            char temp_result[512] = "";
            if (strlen(error_msg) > 0) {
                snprintf(temp_result, sizeof(temp_result),
                         "<div class='result-box error'>%s</div>", error_msg);
            } else {
                snprintf(temp_result, sizeof(temp_result),
                         "<div class='result-box success'>Kết quả: %.2f %c %.2f = %.2f</div>",
                         a, operator_char, b, result);
            }

            // Đổ dữ liệu vào đúng vị trí của phương thức yêu cầu
            if (is_get) {
                strcpy(get_result_html, temp_result);
            } else if (is_post) {
                strcpy(post_result_html, temp_result);
            }
        }

        // Gộp toàn bộ các phần theo đúng thứ tự: Header -> Form GET -> Kết quả GET -> Dòng kẻ -> Form POST -> Kết quả POST -> Footer
        char full_response[BUFFER_SIZE * 2];
        snprintf(full_response, sizeof(full_response), "%s%s%s<hr>%s%s%s", 
                 html_header, 
                 html_get_form, get_result_html, 
                 html_post_form, post_result_html, 
                 html_footer);
        
        send(client_sock, full_response, strlen(full_response), 0);
    }
    
    close(client_sock);
    pthread_exit(NULL);
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sock, 10);
    printf("Máy tính HTTP Server chạy tại: http://localhost:%d\n", PORT);

    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, new_sock);
        pthread_detach(tid);
    }
    return 0;
}