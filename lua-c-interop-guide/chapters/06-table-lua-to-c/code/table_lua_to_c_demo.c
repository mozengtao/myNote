#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * l_configure: 一个只接受一个 Table 参数的 C Function。
 *
 * Lua 侧调用：configure({ host = ..., port = ..., enabled = ... })
 *
 * 进入时 Stack 只有一件东西：
 *   [1] = 那张 Table（Lua Object 的"句柄"，不是它的内容拷贝）
 *
 * 之后每读一个字段，都是一次完整的
 *   lua_getfield（Table -> Stack）-> luaL_check*（Stack -> C）-> lua_pop（清理临时值）
 * 三步循环。
 */
static int l_configure(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "host");
    const char *host = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "port");
    int port = (int)luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "enabled");
    int enabled = lua_toboolean(L, -1);
    lua_pop(L, 1);

    printf("[C] configure: host=%s port=%d enabled=%s\n",
           host, port, enabled ? "true" : "false");

    /* 没有值要交回 Lua：栈顶 0 个值是返回值 */
    return 0;
}

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "configure", l_configure);

    if (luaL_dofile(L, "table_lua_to_c_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
