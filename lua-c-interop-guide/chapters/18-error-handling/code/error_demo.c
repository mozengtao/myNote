#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * native_operation：故意在特定条件下报错的 C Function。
 *
 * 参数：divisor（整数）。
 *   - divisor == 0 时，用 luaL_error 抛出 Lua Error（内部就是 lua_error(L)，
 *     本质是 longjmp，本函数从这一行开始不会再往下执行）。
 *   - 否则把 100 / divisor 的结果 push 到 Stack，return 1（告诉 Lua VM：
 *     栈顶 1 个值就是这次调用的返回值）。
 */
static int l_native_operation(lua_State *L) {
    lua_Integer divisor = luaL_checkinteger(L, 1);

    if (divisor == 0) {
        /* luaL_error 内部：格式化消息 -> lua_pushstring -> lua_error(L)。
         * 写成 "return luaL_error(...)" 只是习惯写法，luaL_error 实际上
         * 永远不会把控制权正常交还给这里的 return 语句。 */
        return luaL_error(L, "native_operation: divisor is zero");
    }

    lua_pushinteger(L, 100 / divisor);
    return 1;
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "native_operation", l_native_operation);

    printf("== Step 1: Lua 用 pcall 包裹调用会出错的 native_operation ==\n");
    if (luaL_dofile(L, "error_demo.lua") != LUA_OK) {
        fprintf(stderr, "unexpected top-level lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    printf("\n== Step 2: C 用 lua_pcall 直接调用一个会出错的 Lua Function ==\n");
    lua_getglobal(L, "risky_lua_function"); /* [1] function */
    lua_pushinteger(L, 0);                  /* [2] 参数 divisor = 0，会触发错误 */
    int status = lua_pcall(L, 1, 1, 0);     /* 保护调用：1 个参数，期望 1 个返回值 */
    if (status != LUA_OK) {
        printf("lua_pcall status = %d（不是 LUA_OK）\n", status);
        printf("stack top 上的错误信息 = %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); /* 清理错误信息，恢复 Stack 平衡 */
    } else {
        printf("risky_lua_function 意外返回了 %lld\n", (long long)lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    printf("\n== Step 3: 再用 lua_pcall 调用一次不会出错的输入，验证正常路径 ==\n");
    lua_getglobal(L, "risky_lua_function");
    lua_pushinteger(L, 5);
    status = lua_pcall(L, 1, 1, 0);
    if (status == LUA_OK) {
        printf("risky_lua_function(5) = %lld\n", (long long)lua_tointeger(L, -1));
        lua_pop(L, 1);
    } else {
        printf("意外出错: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    /*
     * 危险演示：只有显式传入 --crash-lua-call 才会执行。
     * 这里故意用 lua_call（普通调用）去调用一个会出错的 Lua Function，
     * 且外层没有任何 lua_pcall/lua_pmain 兜底。
     *
     * lua_call 出错时走的是 lua_error -> longjmp 这条路径：它会一路跳过
     * 所有 C 调用栈帧，直到跳到"最近一层用 setjmp 建立好保护点的地方"。
     * 如果整个调用链上根本没有任何 lua_pcall（或者 lua_pmain），
     * Lua 找不到可以跳回去的 setjmp 点，就会调用当前注册的 panic 函数——
     * 默认的 panic 函数会打印错误信息，然后直接 abort()/exit() 掉整个进程。
     *
     * 对一个像 Wireshark 这样长期运行的宿主程序来说，这意味着一次脚本层面
     * 的小错误就会让整个抓包/分析进程崩溃，这是不可接受的。这就是为什么
     * C binding 里几乎永远应该用 lua_pcall 而不是 lua_call 去调用 Lua 代码。
     */
    if (argc > 1 && strcmp(argv[1], "--crash-lua-call") == 0) {
        printf("\n== Step 4（危险演示，仅在传入 --crash-lua-call 时执行）==\n");
        printf("用 lua_call 直接调用会出错的 Lua Function，外层没有 lua_pcall 兜底：\n");
        fflush(stdout);
        lua_getglobal(L, "risky_lua_function");
        lua_pushinteger(L, 0);
        lua_call(L, 1, 1); /* 普通调用：如果这里出错，本行之后的代码不会被执行到 */
        printf("这一行不会被打印到（如果上面真的出错的话）\n");
    }

    lua_close(L);
    printf("\n程序正常退出：所有错误都被 lua_pcall 正确捕获，没有 longjmp 穿透到 main。\n");
    return 0;
}
