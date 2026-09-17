/*
 * failure10_stale_registry_ref.c
 *
 * Failure 10（Chapter 24）：C callback 已经被 Lua GC 回收（或者说，
 * 引用已经被显式释放），但 C 仍然尝试调用。
 *
 * register_handler 用 luaL_ref 存一个 Lua Function；unregister_handler
 * 用 luaL_unref 释放这个引用，但故意"忘记"把 handler_ref 重置成
 * LUA_NOREF；fire_stale 继续拿着这个已经失效的整数去
 * lua_rawgeti + lua_pcall——观察真实取回的是什么、调用会得到什么错误。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int handler_ref = LUA_NOREF;

static int l_register_handler(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    printf("[C] register_handler: ref = %d\n", handler_ref);
    return 0;
}

static int l_unregister_handler(lua_State *L) {
    (void)L;
    luaL_unref(L, LUA_REGISTRYINDEX, handler_ref);
    printf("[C] unregister_handler: 已经 luaL_unref(%d)，"
           "但 BUG 是这里忘记把 handler_ref 重置为 LUA_NOREF\n", handler_ref);
    /* 正确写法应该是紧接着一行：handler_ref = LUA_NOREF;
     * 这里故意不做，模拟"引用已失效但变量还留着旧值"的真实 bug。 */
    return 0;
}

static int l_fire_stale(lua_State *L) {
    (void)L;
    printf("[C] fire_stale: 继续用已经 unref 过的 ref = %d 尝试取回并调用\n",
           handler_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref); /* 这个槽位已经被释放 */
    printf("[C] lua_rawgeti 取回的类型 = %s\n", luaL_typename(L, -1));

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        printf("[C] 调用失败（这正是我们要观察的真实报错）: %s\n",
               lua_tostring(L, -1));
        lua_pop(L, 1);
    } else {
        printf("[C] 调用成功？不符合预期\n");
    }
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "register_handler", l_register_handler);
    lua_register(L, "unregister_handler", l_unregister_handler);
    lua_register(L, "fire_stale", l_fire_stale);

    const char *src =
        "register_handler(function() print('[lua] handler ran') end)\n"
        "unregister_handler()\n"
        "fire_stale()\n";

    if (luaL_dostring(L, src) != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
