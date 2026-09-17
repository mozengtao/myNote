#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int l_add(lua_State *L) {
    int a = (int)luaL_checkinteger(L, 1);
    int b = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b);
    return 1;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "add", l_add);
    if (luaL_dofile(L, "add_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
