/*
 * failure06_lua_call_unprotected.c
 *
 * Failure 6（Chapter 24）：用 lua_call() 调用一个会出错的 Lua Function，
 * 而且这一层完全没有任何 lua_pcall 兜底（不像 Chapter 04 的
 * callback_demo，那里外层还有一个 lua_pcall 负责接住穿透上来的错误）。
 *
 * boom(a, b) 在 b == 0 时 error()。main() 直接、裸地用 lua_call
 * 调用 boom(10, 0)，观察真实的进程退出行为——不猜测，实际运行记录。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static const char *BOOM_SRC =
    "function boom(a, b)\n"
    "  if b == 0 then error('boom: b is zero') end\n"
    "  return a / b\n"
    "end\n";

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dostring(L, BOOM_SRC) != LUA_OK) {
        fprintf(stderr, "load boom failed: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    printf("[C] 即将用 lua_call（没有任何 lua_pcall 保护）调用 boom(10, 0) ...\n");
    fflush(stdout);

    lua_getglobal(L, "boom");   /* Stack: [boom] */
    lua_pushinteger(L, 10);     /* Stack: [boom, 10] */
    lua_pushinteger(L, 0);      /* Stack: [boom, 10, 0] */

    lua_call(L, 2, 1);          /* boom 内部 error() -> 没有保护点可以接住 -> panic */

    printf("[C] 这一行永远不会被打印\n");
    lua_close(L);
    return 0;
}
