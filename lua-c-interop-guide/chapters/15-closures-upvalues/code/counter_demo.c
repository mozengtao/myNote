#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * l_counter: 真正被调用的 C Function。
 *
 * 它没有任何 C 全局变量、没有 static 计数器，
 * 计数状态全部存放在它自己的 Upvalue(1) 里
 * （由 create_counter 通过 lua_pushcclosure 绑定进来）。
 */
static int l_counter(lua_State *L) {
    lua_Integer count = lua_tointeger(L, lua_upvalueindex(1));
    count++;

    /* 把新值写回 Upvalue(1)：先 push 新值，再 lua_replace 到 upvalue 的伪索引 */
    lua_pushinteger(L, count);
    lua_replace(L, lua_upvalueindex(1));

    /* 再 push 一份作为这次调用的返回值 */
    lua_pushinteger(L, count);
    return 1;
}

/*
 * create_counter: 工厂函数，每调用一次就生产一个全新的 C Closure。
 *
 *   1. lua_pushinteger(L, 0)        —— 把初始计数值 0 push 到 Stack
 *   2. lua_pushcclosure(L, l_counter, 1)
 *      —— 把栈顶的 1 个值“打包”进新建的 C Closure，
 *         作为它的 Upvalue(1)；打包完成后原来的栈顶值被弹出，
 *         换成这个新 Closure 本身。
 */
static int l_create_counter(lua_State *L) {
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, l_counter, 1);
    return 1;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "create_counter", l_create_counter);

    if (luaL_dofile(L, "counter_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
