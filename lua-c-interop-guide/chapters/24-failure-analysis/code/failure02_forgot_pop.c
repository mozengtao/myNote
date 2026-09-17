/*
 * failure02_forgot_pop.c
 *
 * Failure 2（Chapter 24）：lua_getfield(...) 后不清理（忘记 pop）。
 *
 * 读完 table 的 "port" 字段之后，本该 lua_pop(L, 1) 把它弹掉、
 * 再回到 table（Stack 位置 1）去读下一个字段。这里故意漏掉那个
 * lua_pop，然后继续用相对索引 -1 去 lua_getfield——但此刻 -1
 * 已经不是 table，而是刚刚读出来的 port（一个数字）。
 *
 * 实际编译运行，观察 lua_gettop 的真实变化，以及对一个数字调用
 * lua_getfield 会得到什么真实的 Lua Error 文本。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int l_configure_leaky(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    printf("[C] 进入时 lua_gettop(L) = %d\n", lua_gettop(L));

    lua_getfield(L, 1, "port");           /* Stack: [table, port] */
    int port = (int)luaL_checkinteger(L, -1);
    printf("[C] port = %d，读完之后 lua_gettop(L) = %d"
           "（预期应该是 1，多出来的这 1 个就是没 pop 的 port）\n",
           port, lua_gettop(L));

    /* BUG：应该在上面先 lua_pop(L, 1)，再回到 table（index 1）读 host。
     * 忘了 pop，于是这里的 -1 实际是数字 port，不是 table。 */
    lua_getfield(L, -1, "host");
    const char *host = lua_tostring(L, -1);
    printf("[C] host = %s\n", host ? host : "(NULL)");

    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "configure_leaky", l_configure_leaky);

    if (luaL_dofile(L, "failure02_forgot_pop.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
