/*
 * Chapter 11: Userdata Basics
 *
 * 目标：只把 "C Object -> Lua Userdata" 这一步做完整。
 *   - lua_newuserdata()   在 Lua Stack 上分配一块由 GC 管理的内存
 *   - luaL_newmetatable() 建一张全局唯一、以字符串命名的 Metatable
 *   - luaL_getmetatable() 按名字取回那张 Metatable
 *   - lua_setmetatable()  把 Metatable 挂到某个 Userdata 上
 *   - luaL_checktype()    校验 Stack 上某个位置是不是"某一大类"类型
 *   - luaL_checkudata()   校验并取回"某个具体类型"的 Userdata 裸内存
 *
 * dev:start()/stop()/name() 的方法派发链留给 Chapter 12。
 * 这里只演示：拿到 userdata，从里面把 struct device* 读出来。
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

/* Metatable 的注册名：Registry 里用这个字符串当 key，全局唯一 */
#define DEVICE_MT "device"

/* 贯穿本教程 Chapter 11-14 的同一个 C Object 定义 */
struct device {
    int fd;
    char name[64];
};

/*
 * check_device()
 *   从 Stack index idx 校验并取回一个 "device" 类型的 Userdata，
 *   返回它裸内存起始地址（即 struct device*）。
 *   如果 idx 位置不是 userdata，或者 metatable 名字不是 DEVICE_MT
 *   （比如传进来一个别的类型的 userdata，或者一个 table），
 *   luaL_checkudata 内部会直接 luaL_error()，不会把控制权交还给我们。
 */
static struct device *check_device(lua_State *L, int idx)
{
    void *ud = luaL_checkudata(L, idx, DEVICE_MT);
    return (struct device *)ud;
}

/*
 * device.open(path)
 *
 * Stack Before : [ path ]
 * Stack After  : [ path, userdata ]   -- 但只有栈顶 1 个值会被当作返回值
 */
static int l_device_open(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);

    /* 1. 申请一块由 Lua GC 管理、大小正好是 sizeof(struct device) 的内存，
     *    并把它 Push 到 Stack 顶部。这块内存目前还没有 metatable，
     *    Lua 只知道"这是一个 userdata"，不知道它是什么类型。 */
    struct device *dev = (struct device *)lua_newuserdata(L, sizeof(struct device));

    /* 2. 在这块内存上做真正的 C 侧初始化：打开一个真实的文件描述符。
     *    用 /dev/null 而不是伪造整数，是为了让"资源"这个概念看得见摸得着，
     *    fd 可能成功（>=0）也可能失败（-1），两种情况在 Chapter 12/13 都会用到。 */
    dev->fd = open(path, O_RDONLY);
    strncpy(dev->name, path, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';

    /* 3. 取回全局唯一的 "device" metatable（由 main() 里
     *    luaL_newmetatable 提前建好），挂到刚创建的 userdata 上。
     *    此后这个 userdata 才真正"变成"一个类型为 device 的 Lua Object。 */
    luaL_getmetatable(L, DEVICE_MT); /* Stack: [path, userdata, metatable] */
    lua_setmetatable(L, -2);         /* 把栈顶的 metatable 挂到 userdata(-2) 上，
                                       * 挂完 metatable 自动从栈顶弹出 */
    /* Stack: [path, userdata] */

    return 1; /* 告诉 Lua VM：栈顶 1 个值（userdata）是这次调用的返回值 */
}

/*
 * device_dump(dev)
 *   只读地演示"能拿到 userdata 并从里面读出 struct device*"。
 *   先用 luaL_checktype 确认第 1 个参数至少是某种 userdata（分类校验），
 *   再用 check_device()（内部是 luaL_checkudata）确认它具体是 "device" 类型
 *   （防止类型混淆：传一个别的模块的 userdata 进来会在这里被挡住）。
 */
static int l_device_dump(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TUSERDATA); /* 只看"大类"：是不是 userdata */
    struct device *dev = check_device(L, 1); /* 精确到"是不是 device 类型" */

    printf("[device_dump] fd=%d name=%s\n", dev->fd, dev->name);
    return 0; /* 没有返回值：栈顶 0 个值 */
}

static const luaL_Reg device_lib[] = {
    {"open", l_device_open},
    {NULL, NULL},
};

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    /* 提前建好全局唯一的 "device" metatable。
     * luaL_newmetatable 内部等价于：
     *   1. 检查 Registry[DEVICE_MT] 是否已存在；
     *   2. 不存在就创建一张新 table，存进 Registry[DEVICE_MT]，并 Push 到 Stack；
     *   3. 已存在就直接把已有的那张 Push 到 Stack。
     * 这一章暂时不往这张 metatable 里塞任何 metamethod（留给 Chapter 12），
     * 建好之后就直接弹出，不用一直占着 Stack。 */
    luaL_newmetatable(L, DEVICE_MT);
    lua_pop(L, 1);

    /* 注册 device.open() */
    lua_newtable(L);
    luaL_setfuncs(L, device_lib, 0);
    lua_setglobal(L, "device");

    /* 注册一个全局函数 device_dump()，方便脚本里直接调用观察 */
    lua_register(L, "device_dump", l_device_dump);

    if (luaL_dofile(L, "device_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
