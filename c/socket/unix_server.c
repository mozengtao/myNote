// unix domain socket
// server.c
// unix_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/demo_socket"

int main() {
    int server_fd, client_fd;
    struct sockaddr_un addr;
    char buffer[100];

    unlink(SOCKET_PATH); // Remove old socket file

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 5);
    printf("UNIX server listening on %s...\n", SOCKET_PATH);

    client_fd = accept(server_fd, NULL, NULL);
    read(client_fd, buffer, sizeof(buffer));
    printf("Received: %s\n", buffer);
    write(client_fd, "Hi UNIX client!", 15);

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}
