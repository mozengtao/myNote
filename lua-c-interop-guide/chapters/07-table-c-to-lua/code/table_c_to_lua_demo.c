#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * l_get_config: 不接受任何参数，构造一张 Table 交回给 Lua。
 *
 * 全程只有一件事：从 C 原生世界（字符串常量、int 常量）
 * 把值 Push 到 Stack，再用 lua_setfield（Set 系列：Stack -> Table）
 * 写进正在构造的 table 里。
 */
static int l_get_config(lua_State *L)
{
    lua_newtable(L);                 /* [1] = 新的空 table */

    lua_pushstring(L, "localhost");  /* [1]=table [2]="localhost" */
    lua_setfield(L, -2, "host");     /* 弹出 [2]，写入 table.host；[1]=table */

    lua_pushinteger(L, 8080);        /* [1]=table [2]=8080 */
    lua_setfield(L, -2, "port");     /* 弹出 [2]，写入 table.port；[1]=table */

    /* 此刻栈顶就是这张构造好的 table：栈顶 1 个值是返回值 */
    return 1;
}

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "get_config", l_get_config);

    if (luaL_dofile(L, "table_c_to_lua_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
