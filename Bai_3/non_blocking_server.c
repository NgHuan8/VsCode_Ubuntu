/*******************************************************************************
* @file    non_blocking_server.c
* @brief   Mô tả ngắn gọn về chức năng của file
* @date    2026-03-31 07:10
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    // Chuyen socket listener sang non-blocking
    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    // Server is now listening for incoming connections
    printf("Server is listening on port 8080...\n");

    int clients[64];
    int nclients = 0;

    char buf[256];
    int len;

    while (1) {
        // Chap nhan ket noi
        int client = accept(listener, NULL, NULL);
        if (client == -1) {
            if (errno == EWOULDBLOCK) {
                // Loi do dang cho ket noi
                // Bo qua
            } else {
                // Loi khac
            }
        } else {
            printf("New client accepted: %d\n", client);
            clients[nclients] = client;
            nclients++;
            ul = 1;
            ioctl(client, FIONBIO, &ul);
        }

        // Nhan du lieu tu cac client
        for (int i = 0; i < nclients; i++) {
            len = recv(clients[i], buf, sizeof(buf), 0);
            if (len == -1) {
                if (errno == EWOULDBLOCK) {
                    // Loi do cho du lieu
                    // Bo qua 
                } else {
                    continue;
                }
            } else {
                if (len == 0)
                    continue;
                buf[len] = 0;
                printf("Received from %d: %s\n", clients[i], buf);
            }

        }
    }

    close(listener);
    return 0;
}