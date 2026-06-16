/*
 * chat_client.c - Chat Client theo giao thuc trong slide
 *
 * Bien dich: gcc -o chat_client chat_client.c -lpthread
 * Chay:      ./chat_client <server_ip> <port>
 * Vi du:     ./chat_client 127.0.0.1 5000
 *
 * Giao thuc (text, ket thuc moi thong diep bang '\n'):
 *   Lenh gui tu client len server:
 *     JOIN <nickname>
 *     MSG  <room message>
 *     PMSG <nickname> <message>
 *     OP   <nickname>
 *     KICK <nickname>
 *     TOPIC <topic name>
 *     QUIT
 *   Phan hoi tu server: ma so + mo ta (100 OK, 200, 201, 202, 203, 999...)
 *   Thong diep server chu dong gui: JOIN/MSG/PMSG/OP/KICK/TOPIC/QUIT ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUF_SIZE 4096

static int   sockfd        = -1;
static int   running       = 1;   /* co bao hieu chuong trinh con chay         */
static char  my_nick[64]   = "";  /* nickname da JOIN thanh cong               */
static int   joined        = 0;   /* da JOIN phong chat chua                   */

/* ------------------------------------------------------------------ */
/* Tien ich                                                            */
/* ------------------------------------------------------------------ */

/* Loai bo ky tu xuong dong cuoi chuoi */
static void trim_newline(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

/* Gui nguyen mot dong (tu dong them '\n') len server */
static int send_line(const char *line)
{
    char buf[BUF_SIZE];
    int  len = snprintf(buf, sizeof(buf), "%s\n", line);
    if (len < 0) return -1;
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;

    int sent = 0;
    while (sent < len) {
        int k = send(sockfd, buf + sent, len - sent, 0);
        if (k <= 0) return -1;
        sent += k;
    }
    return 0;
}

/* Dien giai ma phan hoi tu server thanh van ban de doc */
static const char *explain_code(int code)
{
    switch (code) {
        case 100: return "OK";
        case 200: return "NICKNAME IN USE";
        case 201: return "INVALID NICKNAME";
        case 202: return "UNKNOWN NICKNAME";
        case 203: return "DENIED";
        case 999: return "UNKNOWN ERROR";
        default:  return "";
    }
}

/* ------------------------------------------------------------------ */
/* Xu ly thong diep nhan tu server                                     */
/* ------------------------------------------------------------------ */

static void handle_server_message(char *line)
{
    trim_newline(line);
    if (line[0] == '\0') return;

    /* Phan hoi dang ma so: bat dau bang chu so */
    if (line[0] >= '0' && line[0] <= '9') {
        int code = atoi(line);
        const char *desc = explain_code(code);

        if (code == 100) {
            /* Neu dang cho ket qua JOIN thi danh dau da vao phong */
            if (!joined && my_nick[0] != '\0') {
                joined = 1;
                printf("\r[+] Da tham gia phong chat voi nickname '%s'.\n",
                       my_nick);
            } else {
                printf("\r[server] %s\n", line);
            }
        } else {
            /* Loi -> neu dang cho JOIN thi xoa nickname tam */
            if (!joined && my_nick[0] != '\0')
                my_nick[0] = '\0';
            if (desc[0])
                printf("\r[server] %s  (%s)\n", line, desc);
            else
                printf("\r[server] %s\n", line);
        }
        fflush(stdout);
        printf("> ");
        fflush(stdout);
        return;
    }

    /* Thong diep server chu dong gui (su kien trong phong chat) */
    char cmd[32] = "", rest[BUF_SIZE] = "";
    sscanf(line, "%31s", cmd);
    const char *p = line + strlen(cmd);
    while (*p == ' ') p++;
    strncpy(rest, p, sizeof(rest) - 1);

    if (strcmp(cmd, "JOIN") == 0) {
        printf("\r*** %s da tham gia phong chat.\n", rest);
    } else if (strcmp(cmd, "MSG") == 0) {
        /* MSG <nickname> <room message> */
        char who[64] = "";
        sscanf(rest, "%63s", who);
        const char *msg = rest + strlen(who);
        while (*msg == ' ') msg++;
        printf("\r[phong] %s: %s\n", who, msg);
    } else if (strcmp(cmd, "PMSG") == 0) {
        /* PMSG <nickname> <message> */
        char who[64] = "";
        sscanf(rest, "%63s", who);
        const char *msg = rest + strlen(who);
        while (*msg == ' ') msg++;
        printf("\r[rieng tu] %s: %s\n", who, msg);
    } else if (strcmp(cmd, "OP") == 0) {
        /* OP <nickname> : ban duoc chuyen quyen chu phong */
        printf("\r*** Ban da duoc chuyen quyen chu phong (tu %s).\n", rest);
    } else if (strcmp(cmd, "KICK") == 0) {
        /* KICK <kicked nickname> <op nickname> */
        char kicked[64] = "", op[64] = "";
        sscanf(rest, "%63s %63s", kicked, op);
        printf("\r*** %s da bi %s duoi khoi phong chat.\n", kicked, op);
        if (strcmp(kicked, my_nick) == 0) {
            printf("\r[!] Ban da bi duoi khoi phong chat.\n");
            joined = 0;
        }
    } else if (strcmp(cmd, "TOPIC") == 0) {
        /* TOPIC <op nickname> <topic> */
        char op[64] = "";
        sscanf(rest, "%63s", op);
        const char *topic = rest + strlen(op);
        while (*topic == ' ') topic++;
        printf("\r*** %s da dat chu de phong: %s\n", op, topic);
    } else if (strcmp(cmd, "QUIT") == 0) {
        /* QUIT <nickname> */
        printf("\r*** %s da roi khoi phong chat.\n", rest);
    } else {
        /* Khong ro dinh dang -> in nguyen van */
        printf("\r[server] %s\n", line);
    }

    fflush(stdout);
    printf("> ");
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Luong nhan du lieu tu server                                        */
/* ------------------------------------------------------------------ */

static void *recv_thread(void *arg)
{
    (void)arg;
    char  buf[BUF_SIZE];
    char  acc[BUF_SIZE * 2];   /* bo dem tich luy de tach theo dong */
    int   acc_len = 0;

    while (running) {
        int n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("\r[!] Mat ket noi voi server.\n");
            running = 0;
            break;
        }
        buf[n] = '\0';

        /* Noi vao bo dem tich luy */
        if (acc_len + n >= (int)sizeof(acc)) acc_len = 0; /* tran -> reset */
        memcpy(acc + acc_len, buf, n);
        acc_len += n;
        acc[acc_len] = '\0';

        /* Tach va xu ly tung dong hoan chinh */
        char *start = acc, *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            handle_server_message(start);
            start = nl + 1;
        }
        /* Don phan con du chua co '\n' ve dau bo dem */
        acc_len = strlen(start);
        memmove(acc, start, acc_len + 1);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* In huong dan su dung lenh                                           */
/* ------------------------------------------------------------------ */

static void print_help(void)
{
    printf(
        "\n=============== HUONG DAN LENH ===============\n"
        "  JOIN <nickname>          Tham gia phong chat\n"
        "  MSG  <noi dung>          Gui tin nhan cho ca phong\n"
        "  PMSG <nick> <noi dung>   Gui tin nhan rieng\n"
        "  OP   <nick>              Chuyen quyen chu phong (chu phong)\n"
        "  KICK <nick>              Duoi nguoi dung (chu phong)\n"
        "  TOPIC <chu de>           Dat chu de phong (chu phong)\n"
        "  QUIT                     Thoat phong chat\n"
        "  HELP                     Hien thi huong dan\n"
        "==============================================\n\n");
}

/* ------------------------------------------------------------------ */
/* Xu ly lenh nguoi dung nhap                                          */
/* ------------------------------------------------------------------ */

static void process_user_input(char *line)
{
    trim_newline(line);
    if (line[0] == '\0') return;

    /* Lenh tien ich noi bo cua client (khong gui len server) */
    if (strcmp(line, "HELP") == 0 || strcmp(line, "help") == 0) {
        print_help();
        return;
    }

    /* Tach ten lenh (tu dau tien) */
    char cmd[32] = "";
    sscanf(line, "%31s", cmd);
    const char *arg = line + strlen(cmd);
    while (*arg == ' ') arg++;

    /* Kiem tra cu phap toi thieu o phia client cho than thien;
     * con lai gui nguyen van dong nguoi dung nhap xuong server,
     * dung dinh dang giao thuc trong slide.                       */

    if (strcmp(cmd, "JOIN") == 0) {
        if (arg[0] == '\0') { printf("[!] Cu phap: JOIN <nickname>\n"); return; }
        /* Ghi nho nickname de hien thi khi nhan 100 OK */
        char nick[64] = "";
        sscanf(arg, "%63s", nick);
        strncpy(my_nick, nick, sizeof(my_nick) - 1);
        my_nick[sizeof(my_nick) - 1] = '\0';
        send_line(line);

    } else if (strcmp(cmd, "MSG") == 0) {
        if (!joined) { printf("[!] Ban can JOIN truoc.\n"); return; }
        if (arg[0] == '\0') { printf("[!] Cu phap: MSG <noi dung>\n"); return; }
        send_line(line);

    } else if (strcmp(cmd, "PMSG") == 0) {
        if (!joined) { printf("[!] Ban can JOIN truoc.\n"); return; }
        char nick[64] = "";
        sscanf(arg, "%63s", nick);
        const char *msg = arg + strlen(nick);
        while (*msg == ' ') msg++;
        if (nick[0] == '\0' || msg[0] == '\0') {
            printf("[!] Cu phap: PMSG <nick> <noi dung>\n"); return;
        }
        send_line(line);

    } else if (strcmp(cmd, "OP") == 0) {
        if (arg[0] == '\0') { printf("[!] Cu phap: OP <nick>\n"); return; }
        send_line(line);

    } else if (strcmp(cmd, "KICK") == 0) {
        if (arg[0] == '\0') { printf("[!] Cu phap: KICK <nick>\n"); return; }
        send_line(line);

    } else if (strcmp(cmd, "TOPIC") == 0) {
        if (arg[0] == '\0') { printf("[!] Cu phap: TOPIC <chu de>\n"); return; }
        send_line(line);

    } else if (strcmp(cmd, "QUIT") == 0) {
        send_line("QUIT");
        running = 0;

    } else {
        printf("[!] Lenh khong hop le. Go HELP de xem huong dan.\n");
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Cach dung: %s <server_ip> <port>\n", argv[0]);
        fprintf(stderr, "Vi du:     %s 127.0.0.1 5000\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int         port      = atoi(argv[2]);

    /* Tao socket TCP */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(port);
    if (inet_pton(AF_INET, server_ip, &serv.sin_addr) <= 0) {
        fprintf(stderr, "[!] Dia chi IP khong hop le: %s\n", server_ip);
        close(sockfd);
        return 1;
    }

    /* Ket noi toi server */
    if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("=== Da ket noi toi %s:%d ===\n", server_ip, port);
    print_help();
    printf("[i] Hay bat dau bang lenh: JOIN <nickname>\n");

    /* Khoi tao luong nhan du lieu */
    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create");
        close(sockfd);
        return 1;
    }

    /* Vong lap doc lenh nguoi dung */
    char line[BUF_SIZE];
    printf("> ");
    fflush(stdout);
    while (running && fgets(line, sizeof(line), stdin) != NULL) {
        process_user_input(line);
        if (!running) break;
        printf("> ");
        fflush(stdout);
    }

    running = 0;
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    pthread_join(tid, NULL);

    printf("\n=== Da thoat ung dung chat. Tam biet! ===\n");
    return 0;
}