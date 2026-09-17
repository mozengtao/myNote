/*
 * failure01_wrong_stack_index.c
 *
 * Failure 1（Chapter 24）：错误的 Stack index。
 *
 * Lua 侧只传了 2 个参数（Stack 上只有 [1] [2]），C 侧却去读
 * Stack 位置 3——一个当前调用里从未存在过的坐标。
 *
 * 这里不猜测会发生什么，实际编译运行、把 lua_type / lua_tostring /
 * lua_tointeger / lua_isnone 在越界索引上的真实返回值全部打印出来。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int l_bad_index(lua_State *L) {
    printf("[C] 进入时 lua_gettop(L) = %d\n", lua_gettop(L));
    printf("[C] index 1: type = %s\n", luaL_typename(L, 1));
    printf("[C] index 2: type = %s\n", luaL_typename(L, 2));
    printf("[C] index 3（越界，本次调用根本不存在这个参数!): type = %s\n",
           luaL_typename(L, 3));

    const char *s = lua_tostring(L, 3);
    printf("[C] lua_tostring(L, 3) = %s\n", s ? s : "(NULL)");

    int isnum = 0;
    lua_Integer n = lua_tointegerx(L, 3, &isnum);
    printf("[C] lua_tointegerx(L, 3) = %lld, isnum = %d\n", (long long)n, isnum);

    printf("[C] lua_isnone(L, 3) = %d\n", lua_isnone(L, 3));
    printf("[C] lua_isnil(L, 3)  = %d  (注意：none 和 nil 在 lua_is* 上表现不同)\n",
           lua_isnil(L, 3));

    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "bad_index", l_bad_index);

    if (luaL_dofile(L, "failure01_wrong_stack_index.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
