#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * Chapter 08 · Failure 案例 1（lua_next 循环里漏掉 lua_pop，致命反例）
 *
 * lua_next 的契约是"栈顶必须是上一次的 key"。如果处理完 value 之后
 * 不 pop，栈顶在下一轮开始时会是 value，而不是 key——
 * lua_next 会把这个 value 当成"key"去 table 里查找，
 * 如果这个 value 恰好不是 table 里任何一个已存在的 key，
 * Lua 会直接报错："invalid key to 'next'"。
 */
static int l_process_bug(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_pushnil(L);

    int iter = 0;
    while (lua_next(L, 1) != 0) {
        iter++;
        printf("[C] 第 %d 轮 lua_next 返回后 lua_gettop(L) = %d "
               "（正常应该是 3：table+key+value）\n",
               iter, lua_gettop(L));

        const char *key = lua_tostring(L, -2);
        printf("[C]   key = %s\n", key ? key : "(non-string key)");

        /* BUG: 应该在这里 lua_pop(L, 1)，只弹 value；这里故意漏掉，
         * 于是栈顶在下一轮 lua_next 调用时是 value，不是 key */
    }

    printf("[C] 遍历结束（如果走到这里，说明没有触发 Failure）\n");
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* 保证 stdout 与 stderr 的打印顺序在终端上一致 */

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "process", l_process_bug);

    if (luaL_dofile(L, "table_iteration_fail_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
