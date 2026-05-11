#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PORT 8800
#define BUFFER_SIZE 4096

//check login
int check_login(char *user, char *pass)
{
    FILE *fp;
    char file_user[100];
    char file_pass[100];

    fp = fopen("users.txt", "r");

    if (fp == NULL)
    {
        return 0;
    }

    while (fscanf(fp, "%s %s", file_user, file_pass) != EOF)
    {
        if (strcmp(user, file_user) == 0 &&
            strcmp(pass, file_pass) == 0)
        {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

// xử lý client
void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE];
    char username[100];
    char password[100];

    memset(buffer, 0, sizeof(buffer));
    send(client_socket, "Username: ", 10, 0);
    recv(client_socket, username, sizeof(username), 0);
    username[strcspn(username, "\r\n")] = 0;

    send(client_socket, "Password: ", 10, 0);
    recv(client_socket, password, sizeof(password), 0);
    password[strcspn(password, "\r\n")] = 0;

    if (!check_login(username, password))
    {
        send(client_socket,"Login failed!\n", 14, 0);
        close(client_socket);
        exit(0);
    }
    send(client_socket, "Login successful!\n", 19, 0);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        send(client_socket, "\ncmd> ", 6, 0);
        int bytes =recv(client_socket,buffer, sizeof(buffer), 0);
        if (bytes <= 0)
            break;
        buffer[strcspn(buffer, "\r\n")] = 0;
        if (strcmp(buffer, "exit") == 0)
            break;

        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "%.4000s> out.txt", buffer);
        system(command);
        FILE *fp = fopen("out.txt", "r");

        if (fp == NULL)
        {
            send(client_socket, "Cannot open output file\n", 24, 0);
            continue;
        }

        char result[BUFFER_SIZE];

        while (fgets(result, sizeof(result), fp) != NULL)
        {
            send(client_socket, result, strlen(result), 0);
        }
        fclose(fp);
    }

    close(client_socket);
    exit(0);
}

int main()
{
    int server_socket;
    int client_socket;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("Socket failed");
        return 1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    listen(server_socket, 5);
    printf("Server listening on port %d...\n", PORT);

    while (1)
    {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0){
            perror("Accept failed");
            continue;
        }

        printf("Client connected\n");
        printf("Child PID: %d\n", getpid());
        pid_t pid = fork();
        if (pid == 0){
            close(server_socket);
            handle_client(client_socket);
        }
        else{
            close(client_socket);
        }
    }
    close(server_socket);
    return 0;
}