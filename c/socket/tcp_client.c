
// client.c
// tcp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server;
    char buffer[1024];

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Set up server address
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    // 3. Connect
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // 4. Communicate
    send(sock, "Hello Server!", 13, 0);
    recv(sock, buffer, sizeof(buffer), 0);
    printf("Received: %s\n", buffer);

    close(sock);
    return 0;
}