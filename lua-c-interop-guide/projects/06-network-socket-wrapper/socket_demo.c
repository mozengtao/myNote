/*
 * Project 6: Network Socket Wrapper
 *
 * 教学目的：跟 Project 5（file fd）几乎完全同构，只是把"文件描述符"
 * 换成"socket 描述符"，进一步证明 Userdata + Metatable + __gc 这套
 * 骨架能包住任意一种"需要手动释放的 OS 资源"：
 *
 *   Lua object (socket.pair() / sock:send/recv/close)
 *     |
 *   Userdata (struct sock_wrap*，由 Lua GC 堆管理)
 *     |
 *   struct sock_wrap { int fd; }
 *     |
 *   C networking APIs: socketpair()/send()/recv()/close()
 *
 * 关于"为什么用 socketpair 而不是真的 socket()+bind()+listen()+accept()+connect()"：
 *   socketpair(AF_UNIX, SOCK_STREAM, 0, fds) 会一次性返回两个已经互相连接好的
 *   本地 socket fd，语义上完全等价于"一次成功的 TCP 三次握手之后，服务端 accept()
 *   拿到的连接 fd + 客户端 connect() 用的 fd"——区别只是不需要真的绑定端口、
 *   不需要处理"谁先谁后""端口被占用""等待 accept"这些跟本章教学目标无关的
 *   时序问题。sock:send()/sock:recv()/sock:close() 这三个方法内部就是标准
 *   POSIX socket API（send/recv/close），如果换成一个真正的 TCP fd，
 *   这三个方法可以不改一行代码直接复用。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define SOCK_MT "network_socket_wrapper.Socket"

struct sock_wrap {
    int fd;
};

/* ---- 工具函数 ---- */

static struct sock_wrap *check_sock(lua_State *L, int idx) {
    return (struct sock_wrap *)luaL_checkudata(L, idx, SOCK_MT);
}

static void push_socket(lua_State *L, int fd) {
    struct sock_wrap *sw = (struct sock_wrap *)lua_newuserdata(L, sizeof(*sw));
    sw->fd = fd;
    luaL_getmetatable(L, SOCK_MT);
    lua_setmetatable(L, -2);
}

/* ---- socket.pair()：一次性创建两个已连接的本地 socket ---- */

static int l_socket_pair(lua_State *L) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        return luaL_error(L, "socketpair failed: %s", strerror(errno));
    }
    push_socket(L, fds[0]); /* 习惯上当 "client" */
    push_socket(L, fds[1]); /* 习惯上当 "server" */
    return 2; /* 栈顶 2 个值：client, server */
}

/* ---- sock:send(data) ---- */

static int l_sock_send(lua_State *L) {
    struct sock_wrap *sw = check_sock(L, 1);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);

    if (sw->fd < 0) {
        return luaL_error(L, "send: socket already closed (fd = -1)");
    }

    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sw->fd, data + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return luaL_error(L, "send failed: %s", strerror(errno));
        }
        total += (size_t)n;
    }

    lua_pushinteger(L, (lua_Integer)total);
    return 1;
}

/* ---- sock:recv(maxlen) ---- */

static int l_sock_recv(lua_State *L) {
    struct sock_wrap *sw = check_sock(L, 1);
    lua_Integer maxlen = luaL_checkinteger(L, 2);

    if (sw->fd < 0) {
        return luaL_error(L, "recv: socket already closed (fd = -1)");
    }
    if (maxlen <= 0) {
        return luaL_error(L, "recv: maxlen must be > 0");
    }

    char *buf = (char *)malloc((size_t)maxlen);
    if (buf == NULL) {
        return luaL_error(L, "recv: out of memory (maxlen=%d)", (int)maxlen);
    }

    ssize_t n;
    for (;;) {
        n = recv(sw->fd, buf, (size_t)maxlen, 0);
        if (n < 0 && errno == EINTR) continue;
        break;
    }
    if (n < 0) {
        int saved_errno = errno;
        free(buf);
        return luaL_error(L, "recv failed: %s", strerror(saved_errno));
    }

    lua_pushlstring(L, buf, (size_t)n); /* n == 0 表示对端已关闭连接（EOF） */
    free(buf);
    return 1;
}

/* ---- sock:close()：显式关闭，幂等 ---- */

static int l_sock_close(lua_State *L) {
    struct sock_wrap *sw = check_sock(L, 1);

    if (sw->fd >= 0) {
        printf("[close] explicit :close() -> close(fd=%d)\n", sw->fd);
        close(sw->fd);
        sw->fd = -1;
    } else {
        printf("[close] :close() called again on an already-closed socket, no-op\n");
    }
    return 0;
}

/* ---- __gc：兜底关闭忘记 close 的 socket fd ---- */

static int l_sock_gc(lua_State *L) {
    struct sock_wrap *sw = (struct sock_wrap *)luaL_checkudata(L, 1, SOCK_MT);
    if (sw->fd >= 0) {
        printf("[gc] __gc caught a leaked socket fd=%d, closing it now\n", sw->fd);
        close(sw->fd);
        sw->fd = -1;
    }
    return 0;
}

static const luaL_Reg sock_methods[] = {
    {"send",  l_sock_send},
    {"recv",  l_sock_recv},
    {"close", l_sock_close},
    {NULL, NULL}
};

static void register_socket_api(lua_State *L) {
    luaL_newmetatable(L, SOCK_MT);
    luaL_setfuncs(L, sock_methods, 0);

    lua_pushcfunction(L, l_sock_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);

    lua_newtable(L);                     /* socket = {} */
    lua_pushcfunction(L, l_socket_pair);
    lua_setfield(L, -2, "pair");         /* socket.pair = l_socket_pair */
    lua_setglobal(L, "socket");
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    register_socket_api(L);

    if (luaL_dofile(L, "socket_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
