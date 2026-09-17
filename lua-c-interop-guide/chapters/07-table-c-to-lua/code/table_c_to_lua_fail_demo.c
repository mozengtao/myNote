#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * Chapter 07 · Failure 案例 1（lua_setfield 的相对索引写错）
 *
 * lua_newtable 之后 Push 了字符串值，此刻 [1]=table [2]="localhost"，
 * table 位于 -2，字符串本身位于 -1。正确写法是
 *   lua_setfield(L, -2, "host");
 * 这里故意写成 -1（本意以为"table 就在栈顶附近，随便填个负数"），
 * 于是变成了：把 -1（也就是字符串本身）当成"要写字段的对象"，
 * 相当于执行 "localhost".host = "localhost"。
 */
static int l_get_config_bug(lua_State *L)
{
    lua_newtable(L);                 /* [1] = table */

    printf("[C] lua_newtable 之后 lua_gettop(L) = %d\n", lua_gettop(L));

    lua_pushstring(L, "localhost");  /* [1]=table [2]="localhost" */

    printf("[C] push 字符串之后 lua_gettop(L) = %d，-2 应该是 table，-1 是字符串\n",
           lua_gettop(L));

    /* BUG: 应该是 -2（table），这里误写成 -1（字符串本身） */
    lua_setfield(L, -1, "host");

    return 1;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* 保证 stdout 与 stderr 的打印顺序在终端上一致 */

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "get_config", l_get_config_bug);

    if (luaL_dofile(L, "table_c_to_lua_fail_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
