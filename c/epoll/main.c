#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "network_utils.h"

#define MAX_EVENTS 64
#define BUFFER_SIZE 512

void handle_new_connection(int efd, int listen_fd) {
    while (1) {
        struct sockaddr in_addr;
        socklen_t in_len = sizeof(in_addr);
        int infd = accept(listen_fd, &in_addr, &in_len);

        if (infd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept"); break;
        }

        printf("[Info] Accepted connection on FD %d\n", infd);
        make_socket_non_blocking(infd);

        struct epoll_event event;
        event.data.fd = infd;
        event.events = EPOLLIN | EPOLLET; // 读事件 + 边缘触发
        if (epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event) == -1) {
            perror("epoll_ctl");
            abort();
        }
    }
}

void handle_client_data(int client_fd) {
    char buf[BUFFER_SIZE];
    int done = 0;

    while (1) {
        ssize_t count = read(client_fd, buf, sizeof(buf));
        if (count == -1) {
            if (errno != EAGAIN) {
                perror("read error");
                done = 1;
            }
            break;
        } else if (count == 0) {
            done = 1; // 客户端关闭了连接
            break;
        }

        // 打印收到的数据并原样写回（Echo Server）
        printf("[Data] From FD %d: %.*s", client_fd, (int)count, buf);
        write(client_fd, buf, count);
    }

    if (done) {
        printf("[Info] Closed connection on FD %d\n", client_fd);
        close(client_fd); 
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listen_fd = create_and_bind(argv[1]);
    if (listen_fd == -1) abort();

    if (make_socket_non_blocking(listen_fd) == -1) abort();

    if (listen(listen_fd, SOMAXCONN) == -1) {
        perror("listen");
        abort();
    }

    int efd = epoll_create1(0);
    struct epoll_event event = { .data.fd = listen_fd, .events = EPOLLIN | EPOLLET };
    epoll_ctl(efd, EPOLL_CTL_ADD, listen_fd, &event);

    struct epoll_event *events = calloc(MAX_EVENTS, sizeof(event));
    printf("Server started on port %s. Waiting for connections...\n", argv[1]);

    while (1) {
        int n = epoll_wait(efd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                fprintf(stderr, "epoll error on FD %d\n", events[i].data.fd);
                close(events[i].data.fd);
                continue;
            } else if (listen_fd == events[i].data.fd) {
                handle_new_connection(efd, listen_fd);
            } else {
                handle_client_data(events[i].data.fd);
            }
        }
    }

    free(events);
    close(listen_fd);
    return EXIT_SUCCESS;
}