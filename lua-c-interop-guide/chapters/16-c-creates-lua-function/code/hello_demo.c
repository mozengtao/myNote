#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * l_hello: 一个普通的 C Function（0 个 Upvalue）。
 * 它本身不会被 lua_register 成全局变量，
 * 而是由 get_hello_function 动态 push 出来，
 * 交给 Lua 侧当一个普通的函数值使用。
 */
static int l_hello(lua_State *L) {
    (void)L;
    printf("[C] l_hello: hello from C!\n");
    return 0;
}

/*
 * get_hello_function: 唯一暴露给 Lua 的入口。
 * 它的返回值不是一个"结果数据"，而是一个"函数值"本身——
 * 这个函数值背后指向的是一段 C 代码（l_hello），
 * 但对 Lua 调用方来说，它跟任何一个普通 Lua Function 长得一样。
 */
static int l_get_hello_function(lua_State *L) {
    lua_pushcfunction(L, l_hello);
    return 1;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "get_hello_function", l_get_hello_function);

    if (luaL_dofile(L, "hello_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
