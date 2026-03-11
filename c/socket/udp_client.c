
// client.c
// udp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];
    socklen_t addr_len = sizeof(server_addr);

    // 1. Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Define server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. Send datagram
    sendto(sockfd, "Hello UDP Server!", 18, 0,
           (struct sockaddr*)&server_addr, addr_len);

    // 4. Receive response
    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&server_addr, &addr_len);
    buffer[n] = '\0';
    printf("Server reply: %s\n", buffer);

    close(sockfd);
    return 0;
}