#include "network_utils.h"

int make_socket_non_blocking(int sfd) {
    int flags, s;
    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get");
        return -1;
    }
    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl set");
        return -1;
    }
    return 0;
}

int create_and_bind(char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* IPv4 或 IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP */
    hints.ai_flags = AI_PASSIVE;     /* 监听所有网络接口 */

    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;

        // 设置端口复用，防止重启服务器时出现 Address already in use 错误
        int opt = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0) break; // 绑定成功

        close(sfd);
    }

    freeaddrinfo(result);
    if (rp == NULL) return -1;

    return sfd;
}