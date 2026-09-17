#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * l_process: 第二个 C Function，会被 Lua 回调在执行期间反过来调用。
 * 它自己不知道、也不关心是谁调用了它——它只认 Stack。
 */
static int l_process(lua_State *L) {
    lua_Integer value = luaL_checkinteger(L, 1);
    printf("[C]   l_process: received %lld from Lua callback\n", (long long)value);
    lua_pushinteger(L, value * 2);
    return 1;
}

/*
 * l_run: 接收一个 Lua Function 作为唯一 Argument。
 *
 * 注意：这个 Lua Function 只在本次 l_run 调用内部被使用（拿来 lua_pcall），
 * 全程没有离开这次 C 函数调用，所以用 Stack index 1 直接引用它是安全的——
 * 不需要像 Chapter 09 那样把它存进 Registry（Registry 只在“需要跨调用长期
 * 持有 Lua Function”时才是必须的，例如事件回调要在很久之后才被触发）。
 */
static int l_run(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);  /* Stack index 1：调用方传进来的回调 */

    printf("[C]   l_run: about to call the Lua callback\n");

    lua_pushvalue(L, 1);       /* 把回调复制一份到栈顶，准备当函数调用 */
    lua_pushinteger(L, 21);    /* 传给回调的参数 */

    /* lua_pcall(L, nargs, nresults, msgh):
     *   从栈顶往下数 1 个 Argument + 再往下数 1 个函数值，一起弹出，
     *   调用这个函数，调用完成后把 1 个返回值重新 push 回 Stack。
     */
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        /* 保护调用失败：栈顶此刻是 Lua Error 对象，继续向上传播 */
        return lua_error(L);
    }

    lua_Integer result = luaL_checkinteger(L, -1);
    printf("[C]   l_run: callback returned %lld\n", (long long)result);

    lua_pop(L, 1);              /* 用完就弹掉，保持 Stack Contract 干净 */
    lua_pushinteger(L, result); /* 作为 l_run 自己的返回值重新 push 一次 */
    return 1;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "run", l_run);
    lua_register(L, "process", l_process);

    if (luaL_dofile(L, "bidir_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
