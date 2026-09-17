#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * Chapter 06 · Failure 案例 3（Stack Balance 被破坏）
 *
 * 故意漏掉读完 "host" 之后的 lua_pop(L, 1)，然后紧接着用
 * lua_getfield(L, -1, "port") ——本意是"回到 table 里找 port"，
 * 但因为漏了 pop，此刻 -1 并不是 table，而是刚刚 push 上去的
 * host 字符串本身。
 *
 * 这就是 Stack Balance 被破坏之后最常见的后果：
 * 后续代码用相对索引（-1/-2）去引用"预期中的"栈位置，
 * 但实际栈内容已经因为漏掉的 pop 发生了偏移。
 */
static int l_configure_bug(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    printf("[C] 进入时 lua_gettop(L) = %d\n", lua_gettop(L));

    lua_getfield(L, 1, "host");
    const char *host = luaL_checkstring(L, -1);
    /* BUG: 应该在这里 lua_pop(L, 1)，但故意漏掉 */

    printf("[C] 读完 host 之后 lua_gettop(L) = %d（预期应该是 1，多出的 1 个就是没 pop 的 host）\n",
           lua_gettop(L));

    /* 本意：回到 table（index 1）里找 "port"；
     * 实际：-1 此刻是 host 字符串，不是 table！ */
    lua_getfield(L, -1, "port");
    int port = (int)luaL_checkinteger(L, -1);

    printf("[C] configure: host=%s port=%d\n", host, port);
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* 保证 stdout 与 stderr 的打印顺序在终端上一致 */

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "configure", l_configure_bug);

    if (luaL_dofile(L, "table_lua_to_c_fail_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
