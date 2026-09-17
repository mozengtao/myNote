/*
 * Chapter 13: __gc 与 C Object 生命周期
 *
 * 在 Chapter 12 的基础上（同一个 struct device、同一套 start/stop/name 方法）
 * 增量演进：加上 __gc，把"C 资源何时真正被释放"这件事讲透。
 *
 * 核心观点：Lua GC 管理的是 Lua Object（这块 userdata 本身的内存），
 * 它不理解、也不会自动帮你关掉里面藏着的 fd 这种 C 资源。
 * 必须靠 "Lua GC + __gc + C destructor" 三者配合，才能让 C 资源跟着
 * Lua Object 的生死一起被正确释放。
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define DEVICE_MT "device"

/* 与 Chapter 11/12 完全相同的 C Object 定义 */
struct device {
    int fd;
    char name[64];
};

/* 全局计数器：用来"看得见"__gc 是否真的被触发了。
 * 生产代码里更常见的做法是打日志/更新监控指标，这里用一个整数 + printf
 * 是为了让效果在终端输出里一眼可见。 */
static int g_open_count = 0;

static struct device *check_device(lua_State *L, int idx)
{
    void *ud = luaL_checkudata(L, idx, DEVICE_MT);
    return (struct device *)ud;
}

/*
 * device_close()：真正的 C destructor。
 * 特意写成一个独立的普通函数（不是 lua_CFunction），
 * 因为"关闭一个 device"这件事本身是纯 C 世界的操作，
 * 跟"它是不是被 Lua GC 触发的"无关——__gc 只是众多调用它的场景之一。
 */
static void device_close(struct device *dev)
{
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
        g_open_count--;
        printf("[device] closed: name=%s (open_count=%d)\n", dev->name, g_open_count);
    }
    /* fd 已经是 -1（可能是之前已经关过），避免重复关闭/重复计数 */
}

/* device.open()：与 Chapter 12 相比多了 g_open_count++ 和一行 open 日志 */
static int l_device_open(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);

    struct device *dev = (struct device *)lua_newuserdata(L, sizeof(struct device));

    dev->fd = open(path, O_RDONLY);
    strncpy(dev->name, path, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';

    if (dev->fd >= 0) {
        g_open_count++;
    }
    printf("[device] open: name=%s fd=%d (open_count=%d)\n", dev->name, dev->fd, g_open_count);

    luaL_getmetatable(L, DEVICE_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static int l_device_start(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    if (dev->fd < 0) {
        return luaL_error(L, "device '%s' has no valid fd, cannot start", dev->name);
    }

    printf("[device] start: name=%s fd=%d\n", dev->name, dev->fd);
    lua_pushboolean(L, 1);
    return 1;
}

static int l_device_stop(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    printf("[device] stop: name=%s fd=%d\n", dev->name, dev->fd);
    lua_pushboolean(L, 1);
    return 1;
}

static int l_device_name(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    lua_pushstring(L, dev->name);
    return 1;
}

static int l_device_tostring(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    lua_pushfstring(L, "device(%s, fd=%d)", dev->name, dev->fd);
    return 1;
}

/*
 * __gc：Lua GC 认定这个 userdata 已经不可达（unreachable）之后，
 * 在真正释放它占用的内存之前，会调用这个 Metamethod。
 * 它的函数签名和普通方法完全一样（int (*)(lua_State*)），
 * 第 1 个参数就是这个即将被回收的 userdata 自己。
 *
 * 这一行就是用户需求原文给出的实现：
 *
 *   static int l_device_gc(lua_State *L)
 *   {
 *       struct device *dev = check_device(L, 1);
 *       device_close(dev);
 *       return 0;
 *   }
 */
static int l_device_gc(lua_State *L)
{
    struct device *dev = check_device(L, 1);
    device_close(dev);
    return 0;
}

static const luaL_Reg device_lib[] = {
    {"open", l_device_open},
    {NULL, NULL},
};

static const luaL_Reg device_methods[] = {
    {"start", l_device_start},
    {"stop", l_device_stop},
    {"name", l_device_name},
    {NULL, NULL},
};

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_newmetatable(L, DEVICE_MT); /* Stack: [mt] */

    lua_newtable(L);
    luaL_setfuncs(L, device_methods, 0);
    lua_setfield(L, -2, "__index"); /* mt.__index = methods */

    lua_pushcfunction(L, l_device_tostring);
    lua_setfield(L, -2, "__tostring");

    /* 本章新加的一行：mt.__gc = l_device_gc */
    lua_pushcfunction(L, l_device_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);

    lua_newtable(L);
    luaL_setfuncs(L, device_lib, 0);
    lua_setglobal(L, "device");

    if (luaL_dofile(L, "device_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* lua_close 本身也会触发所有还活着的 Lua Object 的 __gc，
     * 这是保证"哪怕脚本忘了 collectgarbage，进程退出前依然会清理"的最后一道防线。 */
    lua_close(L);
    return 0;
}
