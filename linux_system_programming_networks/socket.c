#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("-1 socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("-1 bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) == -1) {
        perror("-1 listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("server port %d...\n", PORT);
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("-1 accept");
            continue;
        }
        printf("client_addr %s\n", inet_ntoa(client_addr.sin_addr));
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read > 0) {
            printf("in: %s", buffer);
            write(client_fd, buffer, bytes_read);
        }
        close(client_fd);
        printf("close \n\n");
    }
    close(server_fd);
    return 0;
}
