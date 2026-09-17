/*
 * failure03_wrong_return_count.c
 *
 * Failure 3（Chapter 24）：错误的 return count。
 *
 *   lua_pushinteger(L, 10);
 *   lua_pushinteger(L, 20);
 *   return 1;
 *
 * Stack 从栈底到栈顶依次是 10, 20。`return 1` 的语义是"告诉 Lua VM：
 * 从当前 Stack 顶部数 1 个值就是这次调用的返回值"——不是"第 1 个
 * push 的值"。到底 Lua 最终拿到的是 10 还是 20？这里不猜，实际
 * 编译运行来确认。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int l_two_values(lua_State *L) {
    (void)L;
    lua_pushinteger(L, 10);   /* Stack: [10] */
    lua_pushinteger(L, 20);   /* Stack: [10, 20] */
    return 1;                 /* 取栈顶 1 个值 = 20，10 被留在 Stack 上（脏数据） */
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "two_values", l_two_values);

    if (luaL_dofile(L, "failure03_wrong_return_count.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
