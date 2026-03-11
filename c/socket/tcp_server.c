// tcp
// server.c
// tcp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[1024];

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a port
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 3. Listen
    listen(server_fd, 5);
    printf("Server listening on port 8080...\n");

    // 4. Accept
    client_fd = accept(server_fd, NULL, NULL);
    printf("Client connected!\n");

    // 5. Communicate
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Received: %s\n", buffer);
    send(client_fd, "Hello Client!", 13, 0);

    close(client_fd);
    close(server_fd);
    return 0;
}