#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// 创建、绑定并监听套接字
int create_and_bind(char *port);

// 设置套接字为非阻塞模式
int make_socket_non_blocking(int sfd);

#endif