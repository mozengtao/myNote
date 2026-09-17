/*
 * Project 5: File / FD Wrapper
 *
 * 教学目的：把一个真实的 OS 文件描述符（int fd）包装成 Lua Object，
 * 覆盖 Userdata + Metatable + __gc 的标准套路：
 *
 *   Lua (file.open / f:write / f:read / f:close)
 *     |
 *   Userdata (struct file_wrap*，由 Lua GC 堆管理)
 *     |
 *   C struct file_wrap { int fd; }
 *     |
 *   系统调用 open()/write()/read()/close()
 *
 * 关键设计点：
 *   1. fd 关闭后立刻置成 -1，close()/__gc 都先检查 fd >= 0，
 *      这样"手动 close 过的 userdata 又被 GC 调用 __gc"不会重复关闭出错。
 *   2. __gc 是最后一道防线：即便 Lua 脚本忘记调用 :close()，
 *      只要这个 userdata 被 GC 回收，fd 就一定会被关闭，不会永久泄漏。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define FILE_MT "file_fd_wrapper.File"

struct file_wrap {
    int fd;
};

/* ---- 工具函数 ---- */

static struct file_wrap *check_file(lua_State *L, int idx) {
    return (struct file_wrap *)luaL_checkudata(L, idx, FILE_MT);
}

/* ---- file.open(path, mode) ---- */

static int l_file_open(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    const char *mode = luaL_optstring(L, 2, "r");

    int flags;
    if (mode[0] == 'r') {
        flags = O_RDONLY;
    } else if (mode[0] == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (mode[0] == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        return luaL_error(L, "file.open: unsupported mode '%s' (use \"r\"/\"w\"/\"a\")", mode);
    }

    int fd = open(path, flags, 0644);
    if (fd < 0) {
        return luaL_error(L, "file.open(\"%s\", \"%s\") failed: %s",
                           path, mode, strerror(errno));
    }

    /* Native C World -> Userdata：lua_newuserdata 在 Lua GC 堆上分配
     * sizeof(struct file_wrap) 字节，返回的指针既是普通 C 指针，
     * 也已经被 Push 到 Stack 顶（这次调用的返回值）。 */
    struct file_wrap *fw = (struct file_wrap *)lua_newuserdata(L, sizeof(*fw));
    fw->fd = fd;

    luaL_getmetatable(L, FILE_MT);
    lua_setmetatable(L, -2);

    return 1; /* 栈顶 1 个值：这个新 userdata */
}

/* ---- f:write(data) ---- */

static int l_file_write(lua_State *L) {
    struct file_wrap *fw = check_file(L, 1);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);

    if (fw->fd < 0) {
        return luaL_error(L, "write: file already closed (fd = -1)");
    }

    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fw->fd, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return luaL_error(L, "write failed: %s", strerror(errno));
        }
        total += (size_t)n;
    }

    lua_pushinteger(L, (lua_Integer)total);
    return 1;
}

/* ---- f:read() ---- 读到 EOF 为止，整体作为一个 Lua string 返回 */

static int l_file_read(lua_State *L) {
    struct file_wrap *fw = check_file(L, 1);

    if (fw->fd < 0) {
        return luaL_error(L, "read: file already closed (fd = -1)");
    }

    luaL_Buffer b;
    luaL_buffinit(L, &b);

    char chunk[256];
    for (;;) {
        ssize_t n = read(fw->fd, chunk, sizeof(chunk));
        if (n < 0) {
            if (errno == EINTR) continue;
            return luaL_error(L, "read failed: %s", strerror(errno));
        }
        if (n == 0) break; /* EOF */
        luaL_addlstring(&b, chunk, (size_t)n);
    }

    luaL_pushresult(&b);
    return 1;
}

/* ---- f:close()：显式关闭，幂等 ---- */

static int l_file_close(lua_State *L) {
    struct file_wrap *fw = check_file(L, 1);

    if (fw->fd >= 0) {
        printf("[close] explicit :close() -> close(fd=%d)\n", fw->fd);
        close(fw->fd);
        fw->fd = -1;
    } else {
        printf("[close] :close() called again on an already-closed file, no-op\n");
    }
    return 0;
}

/* ---- __gc：最后一道防线，兜底关闭忘记 close 的 fd ---- */

static int l_file_gc(lua_State *L) {
    /* __gc 是 Lua VM 在回收这个 userdata 时自己触发的，
     * 不是脚本主动调用，所以这里用 luaL_checkudata 而不是假设参数一定合法。 */
    struct file_wrap *fw = (struct file_wrap *)luaL_checkudata(L, 1, FILE_MT);
    if (fw->fd >= 0) {
        printf("[gc] __gc caught a leaked fd=%d, closing it now\n", fw->fd);
        close(fw->fd);
        fw->fd = -1;
    }
    return 0;
}

static const luaL_Reg file_methods[] = {
    {"write", l_file_write},
    {"read",  l_file_read},
    {"close", l_file_close},
    {NULL, NULL}
};

/* 标准套路：luaL_newmetatable + __index 指向自身 + __gc */
static void register_file_api(lua_State *L) {
    luaL_newmetatable(L, FILE_MT);          /* mt */
    luaL_setfuncs(L, file_methods, 0);      /* mt.write / mt.read / mt.close */

    lua_pushcfunction(L, l_file_gc);
    lua_setfield(L, -2, "__gc");            /* mt.__gc = l_file_gc */

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");         /* mt.__index = mt（方法表就是元表自己） */

    lua_pop(L, 1);                          /* 弹出 mt，Registry 里仍持有一份 */

    lua_newtable(L);                        /* file = {} */
    lua_pushcfunction(L, l_file_open);
    lua_setfield(L, -2, "open");            /* file.open = l_file_open */
    lua_setglobal(L, "file");
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    register_file_api(L);

    if (luaL_dofile(L, "file_fd_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
