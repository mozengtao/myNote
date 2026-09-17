/*
 * failure04_wrong_return_position.c
 *
 * Failure 4（Chapter 24）：返回 Stack 中错误的位置。
 *
 * 该 push 结果之后，又"顺手"多 push 了一个别的临时值（典型场景：
 * 调试时临时加了一行 lua_getglobal(L, "print") 之类的代码，事后
 * 忘记删）。`return 1` 永远只看"此刻的栈顶"，不关心哪个值是"故意"
 * push 的——于是拿到的其实是那个临时值，不是原本想返回的结果。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int l_bad_position(lua_State *L) {
    int a = (int)luaL_checkinteger(L, 1);

    lua_pushinteger(L, a * 2);   /* Stack: [.., a*2]  <- 这才是我们想要的结果 */

    /* BUG：调试时顺手多 push 了一个东西，忘记在 return 之前 pop 掉 */
    lua_getglobal(L, "print");    /* Stack: [.., a*2, print] */

    return 1; /* return 1 = 取栈顶 1 个值 = print 这个函数对象，不是 a*2! */
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "bad_position", l_bad_position);

    if (luaL_dofile(L, "failure04_wrong_return_position.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
