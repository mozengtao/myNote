/*
 * Chapter 12: Userdata + Metatable + Method
 *
 * 在 Chapter 11 的基础上（同一个 struct device、同一个 check_device）
 * 增量演进：完整实现 dev:start() / dev:stop() / dev:name()，
 * 并解释 dev:start() 实际上是什么：
 *
 *   dev:start()
 *       |
 *       v
 *   dev["start"]              -- dev:start() 只是 dev.start(dev) 的语法糖
 *       |
 *       v
 *   dev 本身没有 "start" 字段，Lua 去它的 Metatable 找 __index
 *       |
 *       v
 *   Metatable.__index -> 方法表（device_methods）
 *       |
 *       v
 *   方法表["start"] = l_device_start   -- 一个 C Function
 *       |
 *       v
 *   调用时第一个参数就是 dev 自己，C 侧用 luaL_checkudata 取回 struct device*
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define DEVICE_MT "device"

/* 与 Chapter 11 完全相同的 C Object 定义，贯穿本教程 Chapter 11-14 */
struct device {
    int fd;
    char name[64];
};

/* 与 Chapter 11 完全相同：校验并取回 struct device* */
static struct device *check_device(lua_State *L, int idx)
{
    void *ud = luaL_checkudata(L, idx, DEVICE_MT);
    return (struct device *)ud;
}

/* 与 Chapter 11 完全相同：device.open(path) */
static int l_device_open(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);

    struct device *dev = (struct device *)lua_newuserdata(L, sizeof(struct device));

    dev->fd = open(path, O_RDONLY);
    strncpy(dev->name, path, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';

    luaL_getmetatable(L, DEVICE_MT);
    lua_setmetatable(L, -2);

    return 1;
}

/*
 * dev:start()  即  dev.start(dev)
 *
 * 第一个参数（idx 1）永远是 dev 自己——这就是 `:` 语法糖多传的那个隐藏参数。
 * 用 C struct 里的 function pointer 类比：这里的 "第一个参数是 self" 就相当于
 * C 里手写 OOP 时常见的 `vtable->start(self)` 调用约定，只是 Lua 帮你把
 * "补上 self 参数" 这一步用 `:` 语法自动做掉了。
 */
static int l_device_start(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    if (dev->fd < 0) {
        /* fd 在 open() 那一步就失败了（Chapter 11 Failure 3 留下的坑），
         * 这里终于把它暴露出来。 */
        return luaL_error(L, "device '%s' has no valid fd, cannot start", dev->name);
    }

    printf("[device] start: name=%s fd=%d\n", dev->name, dev->fd);
    lua_pushboolean(L, 1);
    return 1;
}

/* dev:stop()  即  dev.stop(dev) */
static int l_device_stop(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    printf("[device] stop: name=%s fd=%d\n", dev->name, dev->fd);
    lua_pushboolean(L, 1);
    return 1;
}

/* dev:name()  即  dev.name(dev) */
static int l_device_name(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    lua_pushstring(L, dev->name);
    return 1;
}

/* __tostring：让 print(dev) / tostring(dev) 打印出有意义的信息，
 * 而不是默认的 "device: 0x...." */
static int l_device_tostring(lua_State *L)
{
    struct device *dev = check_device(L, 1);

    lua_pushfstring(L, "device(%s, fd=%d)", dev->name, dev->fd);
    return 1;
}

static const luaL_Reg device_lib[] = {
    {"open", l_device_open},
    {NULL, NULL},
};

/*
 * "方法表"：本质上很像 C 里的
 *
 *   struct device_vtable {
 *       int (*start)(struct device *);
 *       int (*stop)(struct device *);
 *       const char *(*name)(struct device *);
 *   };
 *
 * 区别在于：C 的 vtable 是编译期就固定好的结构体偏移（`vtable->start` 是一次
 * 指针解引用 + 固定偏移的内存访问）；这张 Lua 方法表是运行时按字符串
 * "start"/"stop"/"name" 去查表（一次哈希查找），查找方式不同，但"给一类对象
 * 挂一组统一的操作接口"这个设计目的是一样的。
 */
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

    /* 1. 建（或取回）全局唯一的 "device" metatable */
    luaL_newmetatable(L, DEVICE_MT); /* Stack: [mt] */

    /* 2. 建一张独立的方法表，把 device_methods 塞进去 */
    lua_newtable(L);                     /* Stack: [mt, methods] */
    luaL_setfuncs(L, device_methods, 0); /* 填充 methods 表 */

    /* 3. mt.__index = methods
     *    dev:start() 找不到 dev["start"] 时，Lua 会去 mt.__index 里找；
     *    __index 指向的正是这张方法表，"start" 在这张表里就是 l_device_start。 */
    lua_setfield(L, -2, "__index"); /* Stack: [mt] （methods 被消耗掉） */

    /* 4. mt.__tostring = l_device_tostring */
    lua_pushcfunction(L, l_device_tostring);
    lua_setfield(L, -2, "__tostring"); /* Stack: [mt] */

    lua_pop(L, 1); /* Stack: [] */

    /* 注册 device.open() */
    lua_newtable(L);
    luaL_setfuncs(L, device_lib, 0);
    lua_setglobal(L, "device");

    if (luaL_dofile(L, "device_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
