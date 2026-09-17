/*
 * Chapter 14: Light Userdata vs Full Userdata
 *
 * 在 Chapter 13 的基础上（同一个 struct device、同一套 open/start/stop/name/__gc）
 * 增量演进：额外引入一个"全局单例"，用它对比 lua_pushlightuserdata() 和
 * lua_newuserdata()。
 *
 * Full Userdata（本章前面的 struct device，由 device.open() 创建）：
 *   - Lua 真正分配、拥有这块内存
 *   - 可以挂 Metatable，可以有 __gc，参与 Lua GC lifecycle
 *   - 用 luaL_checkudata 可以做类型安全校验
 *   - 适合"这块内存本来就该由 Lua 的生命周期决定何时释放"的场景
 *
 * Light Userdata（本章新增的 g_singleton "钥匙"）：
 *   - 只是一个 C pointer 值本身，Lua 不分配任何额外内存
 *   - 不参与 GC（Lua 甚至不知道它指向的内存有多大、是什么）
 *   - 不能挂 Metatable，无法用 luaL_checkudata 做类型校验
 *   - 两个指向不同类型但数值相同的指针，在 Lua 看来是"相等"的 light userdata
 *   - 适合"只是想把一个已有的、生命周期由别处管理的 C 指针塞进 Lua 传来传去"
 *     的场景，比如给一个全局单例做一个 Lua 侧的"钥匙"
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define DEVICE_MT "device"

/* 与 Chapter 11/12/13 完全相同的 C Object 定义 */
struct device {
    int fd;
    char name[64];
};

static int g_open_count = 0;

/*
 * g_singleton：一个"生命周期完全由 C/main() 管理"的全局单例，
 * 不由 device.open() 创建，也不会被 Lua GC 收走——
 * 它从进程启动就存在，到进程退出前才由 main() 显式关闭。
 * Light Userdata 存在的意义，就是让 Lua 脚本能拿到"指向它的一个钥匙"，
 * 而不需要、也不应该拥有它。
 */
static struct device g_singleton;

static struct device *check_device(lua_State *L, int idx)
{
    void *ud = luaL_checkudata(L, idx, DEVICE_MT);
    return (struct device *)ud;
}

static void device_close(struct device *dev)
{
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
        g_open_count--;
        printf("[device] closed: name=%s (open_count=%d)\n", dev->name, g_open_count);
    }
}

/* ---- Full Userdata 部分：与 Chapter 13 完全相同 ---- */

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

static int l_device_gc(lua_State *L)
{
    struct device *dev = check_device(L, 1);
    device_close(dev);
    return 0;
}

/* ---- Light Userdata 部分：本章新增 ---- */

/*
 * get_singleton_key()
 *   把 g_singleton 的地址，作为一个 Light Userdata Push 到 Stack 上。
 *   注意：这里没有 lua_newuserdata，没有任何内存分配——
 *   Lua Stack 那一格里存的，就是 &g_singleton 这个指针值本身。
 */
static int l_get_singleton_key(lua_State *L)
{
    lua_pushlightuserdata(L, &g_singleton);
    return 1;
}

/*
 * get_singleton_key_alias()
 *   故意用一个"不同 C 类型"的指针，取同一个地址：
 *   &g_singleton.fd 是 int*，而 fd 恰好是 struct device 的第一个成员，
 *   所以它的地址在数值上与 &g_singleton（struct device*）完全相同。
 *   这就是用户需求里说的"两个指向不同类型但数值相同的指针，
 *   在 Lua 看来是相等的 light userdata"——Lua 只看指针数值，
 *   完全不知道、也不关心原来的 C 类型是 struct device* 还是 int*。
 */
static int l_get_singleton_key_alias(lua_State *L)
{
    lua_pushlightuserdata(L, &g_singleton.fd);
    return 1;
}

/*
 * use_singleton_key(key)
 *   拿到一个 light userdata"钥匙"，转换回 struct device* 来使用。
 *   luaL_checktype 只能确认"这是一个 light userdata"这个大类，
 *   没有任何机制能确认它到底指向什么类型——这是 Light Userdata 天生的限制，
 *   使用者必须凭"约定"（比如函数名、模块文档）自己保证类型正确。
 */
static int l_use_singleton_key(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    struct device *dev = (struct device *)lua_touserdata(L, 1);

    printf("[singleton] name=%s fd=%d\n", dev->name, dev->fd);
    return 0;
}

/*
 * try_checkudata(anything)
 *   故意对传进来的任意值调用 luaL_checkudata(L, 1, DEVICE_MT)。
 *   如果传进来的是 Full Userdata（挂了 "device" metatable），会成功。
 *   如果传进来的是 Light Userdata（哪怕数值上正好等于一个真实的
 *   struct device 地址），会失败——因为 Light Userdata 根本没有、
 *   也不可能有 Metatable，luaL_checkudata 第一步"是否是 userdata 且
 *   带着某张 metatable"就通不过。
 */
static int l_try_checkudata(lua_State *L)
{
    struct device *dev = check_device(L, 1);
    printf("[try_checkudata] name=%s fd=%d\n", dev->name, dev->fd);
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

    /* 全局单例：生命周期完全由 main() 管理，不经过 device.open() */
    g_singleton.fd = open("/dev/null", O_RDONLY);
    strncpy(g_singleton.name, "singleton", sizeof(g_singleton.name) - 1);
    g_singleton.name[sizeof(g_singleton.name) - 1] = '\0';

    luaL_newmetatable(L, DEVICE_MT);

    lua_newtable(L);
    luaL_setfuncs(L, device_methods, 0);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_device_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, l_device_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);

    lua_newtable(L);
    luaL_setfuncs(L, device_lib, 0);
    lua_setglobal(L, "device");

    lua_register(L, "get_singleton_key", l_get_singleton_key);
    lua_register(L, "get_singleton_key_alias", l_get_singleton_key_alias);
    lua_register(L, "use_singleton_key", l_use_singleton_key);
    lua_register(L, "try_checkudata", l_try_checkudata);

    int rc = 0;
    if (luaL_dofile(L, "device_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        rc = 1;
    }

    lua_close(L);

    /* g_singleton 不属于 Lua GC 管理范围，它的生命周期由 main() 自己负责，
     * 必须在这里手动关闭——这正是 Light Userdata 场景下"资源由别处管理"的
     * 含义：Lua 从头到尾都不会、也不应该帮你关闭它。 */
    if (g_singleton.fd >= 0) {
        close(g_singleton.fd);
        printf("[main] singleton closed manually (not via Lua GC)\n");
    }

    return rc;
}
