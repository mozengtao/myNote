#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* ---- Demo 1: lua_call 演示一次"正常返回"的 C -> Lua 调用 ---- */
static void demo_lua_call_success(lua_State *L) {
    printf("\n=== Demo 1: lua_call 调用正常返回的 add(10, 20) ===\n");

    lua_getglobal(L, "add");      /* Stack: [add] */
    lua_pushinteger(L, 10);       /* Stack: [add, 10] */
    lua_pushinteger(L, 20);       /* Stack: [add, 10, 20] */

    lua_call(L, 2, 1);            /* Stack: [30] -- add 与 2 个参数被弹出，1 个返回值被 Push 回来 */

    int result = (int)lua_tointeger(L, -1);
    printf("result = %d\n", result);

    lua_pop(L, 1);                /* Stack: [] -- 手动清理，保持栈平衡 */
}

/*
 * ---- Demo 2: lua_call 遇到出错的 Lua Function 会"穿透"当前 C 栈帧 ----
 *
 * 这个 C Function 本身故意不做任何保护：直接用 lua_call 调用一个会
 * error() 的 Lua Function（boom(10, 0)，b == 0 触发 error()）。
 * boom 内部一旦 error()，lua_call 会立刻 longjmp——注意这不是普通的
 * "函数返回后继续执行下一行"，而是直接跳走：lua_call 那一行之后的
 * printf 永远不会被执行，这个 C 栈帧上的任何清理代码都会被跳过。
 *
 * 但因为 main() 是通过 lua_pcall 来调用本函数的（见下面
 * demo_lua_call_propagates_through_pcall），这次 longjmp 会一直穿透
 * 到那个 lua_pcall 建立的保护边界才停下——它穿透的是"没有保护的 C 栈帧"，
 * 直到遇到"最近一层由 lua_pcall/luaL_dofile 建立的保护点"为止。
 */
static int l_call_boom_unprotected(lua_State *L) {
    printf("[l_call_boom_unprotected] 即将用 lua_call 调用 boom(10, 0) ...\n");

    lua_getglobal(L, "boom");     /* Stack: [boom] */
    lua_pushinteger(L, 10);       /* Stack: [boom, 10] */
    lua_pushinteger(L, 0);        /* Stack: [boom, 10, 0] */

    lua_call(L, 2, 1);            /* boom 内部 error() -> longjmp，下面这行不会执行 */

    printf("[l_call_boom_unprotected] 这一行永远不会被打印！\n");
    return 1;
}

static void demo_lua_call_propagates_through_pcall(lua_State *L) {
    printf("\n=== Demo 2: lua_call 出错时 longjmp 穿透 C 栈帧，被外层 lua_pcall 兜住 ===\n");

    lua_pushcfunction(L, l_call_boom_unprotected); /* Stack: [l_call_boom_unprotected] */

    int status = lua_pcall(L, 0, 0, 0);
    if (status != LUA_OK) {
        printf("外层 lua_pcall 捕获到错误: %s\n", lua_tostring(L, -1));
        printf("（l_call_boom_unprotected 里 lua_call 之后那行 printf 确实没有被执行）\n");
        lua_pop(L, 1); /* 清理错误对象，Stack: [] */
    } else {
        printf("没有出错？这不符合预期。\n");
    }
}

/*
 * ---- Demo 3: 直接用 lua_pcall 调用同一个会出错的 Lua Function ----
 *
 * 与 Demo 2 对比：这次没有额外的 C Function 包一层，lua_pcall 直接、
 * 就地捕获 boom(10, 0) 抛出的错误，Stack 上留下一个错误对象（这里是
 * 字符串），并返回一个非 LUA_OK 的状态码——没有任何 longjmp 穿透到
 * main() 之外，程序继续正常往下走。
 */
static void demo_lua_pcall_direct(lua_State *L) {
    printf("\n=== Demo 3: lua_pcall 直接调用 boom(10, 0)，就地捕获错误 ===\n");

    lua_getglobal(L, "boom");     /* Stack: [boom] */
    lua_pushinteger(L, 10);       /* Stack: [boom, 10] */
    lua_pushinteger(L, 0);        /* Stack: [boom, 10, 0] */

    int status = lua_pcall(L, 2, 1, 0); /* 出错 -> Stack: [error object]，status != LUA_OK */

    if (status != LUA_OK) {
        printf("lua_pcall 返回状态码 = %d（LUA_OK = %d）\n", status, LUA_OK);
        printf("Stack 顶部的错误对象: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); /* 清理错误对象，保持栈平衡 */
    } else {
        int result = (int)lua_tointeger(L, -1);
        printf("没有出错，result = %d\n", result);
        lua_pop(L, 1);
    }
}

/*
 * ---- Demo 4: lua_pcall 捕获"非字符串"错误对象 ----
 *
 * error() 的参数可以是任意 Lua Value。boom_with_table() 抛出一个
 * Table，这里演示：lua_pcall 之后不能想当然地对 Stack 顶部调用
 * lua_tostring，而要先用 lua_istable / lua_type 判断错误对象的真实类型。
 */
static void demo_lua_pcall_non_string_error(lua_State *L) {
    printf("\n=== Demo 4: lua_pcall 捕获非字符串错误对象（Table） ===\n");

    lua_getglobal(L, "boom_with_table");  /* Stack: [boom_with_table] */
    int status = lua_pcall(L, 0, 0, 0);   /* 出错 -> Stack: [error object(Table)] */

    if (status != LUA_OK) {
        printf("lua_pcall 返回状态码 = %d\n", status);
        if (lua_istable(L, -1)) {
            printf("错误对象是一个 Table，不是字符串：\n");
            lua_getfield(L, -1, "code");      /* Stack: [table, code] */
            printf("  code = %d\n", (int)lua_tointeger(L, -1));
            lua_pop(L, 1);                     /* Stack: [table] */

            lua_getfield(L, -1, "message");   /* Stack: [table, message] */
            printf("  message = %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);                     /* Stack: [table] */
        } else {
            printf("错误对象: %s\n", lua_tostring(L, -1));
        }
        lua_pop(L, 1); /* 清理错误对象本身，Stack: [] */
    }
}

/*
 * ---- 可选：真实崩溃演示（不在默认 make run 流程里，见 README「Failure」一节）----
 *
 * 用法：./callback_demo --crash
 * 直接在 main() 里、没有任何 lua_pcall 包裹的情况下对 boom(10, 0) 使用
 * lua_call。这次 longjmp 找不到任何保护边界，最终触发 lua_State 的默认
 * panic 函数——打印错误信息后中止整个进程（非 0 退出码）。
 */
static void demo_real_crash(lua_State *L) {
    printf("\n=== Demo 5（危险）: 无保护的 lua_call 直接中止整个宿主程序 ===\n");
    fflush(stdout);

    lua_getglobal(L, "boom");
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 0);
    lua_call(L, 2, 1); /* 没有任何 lua_pcall 兜底 -> 触发 panic -> 进程中止 */

    printf("这一行不会被打印\n");
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, "callback_demo.lua") != LUA_OK) {
        fprintf(stderr, "加载 callback_demo.lua 失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "--crash") == 0) {
        demo_real_crash(L);
        lua_close(L); /* 正常情况下不会执行到这里 */
        return 0;
    }

    demo_lua_call_success(L);
    demo_lua_call_propagates_through_pcall(L);
    demo_lua_pcall_direct(L);
    demo_lua_pcall_non_string_error(L);

    lua_close(L);
    return 0;
}
