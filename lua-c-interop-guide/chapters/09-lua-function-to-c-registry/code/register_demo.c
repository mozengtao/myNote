/*
 * register_demo.c
 *
 * 本章要证明的唯一一件事：
 *   一个 Lua Function 如果只靠"记住它在 Stack 上的位置（Stack index）"，
 *   是不可能被 C 跨调用长期持有的——Stack index 只在"当前这次 C 函数
 *   调用"内有效。C 必须把它存进 Registry（LUA_REGISTRYINDEX 指向的
 *   那张 C 私有 Table），拿到一个 Registry Reference（一个整数），
 *   才能在任意未来时刻、任意一次全新的"进入 C"里把它取回来调用。
 *
 * 本例分两步验证："跨调用"到底跨到什么程度：
 *   1. register_handler(fn) 在 luaL_dofile 执行 Lua 脚本期间被调用一次
 *      （这是第一次"进入 C"），把 fn 存进 Registry，只留下一个整数 ref。
 *   2. fire_handler("...") 在同一次脚本执行里被 Lua 主动调用一次
 *      （这是第二次"进入 C"），验证 ref 依然指向同一个 Function。
 *   3. luaL_dofile 完全返回之后（脚本已经跑完，Lua 那次调用早就结束、
 *      Stack 上当时的参数区早就被覆盖过很多次），main() 直接用普通 C
 *      代码（call_handler_from_c）第三次"进入 C 世界访问这个 Function"，
 *      证明 Registry Reference 跨越了"脚本运行期"这个更大的边界依然有效。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* 全局：C 侧长期持有的 Registry Reference。
 * 初始值 LUA_NOREF 表示"目前没有注册任何 handler"。
 * 注意：这里存的是一个整数（Registry 里的槽位编号），
 * 不是 Stack index，也不是任何形式的裸指针。 */
static int handler_ref = LUA_NOREF;

/*
 * register_handler(f)
 *   Lua 侧：register_handler(function(event) ... end)
 *
 * 校验 Stack 位置 1 上的参数确实是一个 Lua Function，然后把它从
 * "Stack 上的临时值"提升为"Registry 里的长期引用"。
 */
static int l_register_handler(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    /* 如果之前已经注册过一个 handler，先把旧引用释放掉，
     * 避免每次 register_handler 都在 Registry 里留下一个再也
     * 用不到的槽位（这属于本章 Failure 部分要讲的"引用泄漏"）。 */
    if (handler_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, handler_ref);
    }

    /* luaL_ref 做两件事：
     *   1. 把 Stack 顶部的值（此刻正是 Stack 位置 1 上的那个 Function，
     *      luaL_checktype 并不会移动它，它还在栈顶）搬进 Registry Table；
     *   2. 把它从 Stack 上弹出，返回一个整数——这个整数就是以后
     *      找回它的"钥匙"。
     */
    handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    printf("[C] register_handler: 已保存，Registry Reference = %d\n",
           handler_ref);
    return 0; /* 告诉 Lua VM：这次调用没有返回值 */
}

/*
 * 真正"取回并调用"那个 Lua Function 的核心逻辑，
 * 被 l_fire_handler（供 Lua 调用）和 call_handler_from_c
 * （供 main() 直接调用）共用，证明这套机制跟"谁在调用"无关，
 * 只跟"手里有没有 handler_ref 这个 Registry Reference"有关。
 */
static void invoke_handler(lua_State *L, const char *msg) {
    if (handler_ref == LUA_NOREF) {
        printf("[C] 尚未注册任何 handler，跳过\n");
        return;
    }

    /* 第 1 步：GET —— 用 Registry Reference 把当年存进去的
     * Lua Function 重新 Push 到 Stack 顶部。此刻它才重新拥有一个
     * 有效的 Stack index，但这个 index 只在接下来几行代码内有效。 */
    lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref);

    /* 第 2 步：把参数 Push 到 Stack 上，函数和参数必须紧邻。 */
    lua_pushstring(L, msg);

    /* 第 3 步：CALL —— 用保护调用触发 Lua VM 执行这个 Function。
     * 1 个参数、期望 0 个返回值。如果 Lua 侧 error()，pcall 会把
     * 错误对象留在 Stack 顶部并返回非 0，而不会让整个宿主程序崩溃。 */
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        fprintf(stderr, "[C] handler 执行出错: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); /* 把错误对象弹掉，保持 Stack Balance */
    }
}

/*
 * fire_handler(msg)
 *   Lua 侧：fire_handler("hello")
 * 是 invoke_handler 的 Lua 入口：证明"注册"和"触发"可以是两次
 * 完全独立的、先后发生的"进入 C"。
 */
static int l_fire_handler(lua_State *L) {
    const char *msg = luaL_checkstring(L, 1);
    invoke_handler(L, msg);
    return 0;
}

/*
 * call_handler_from_c
 *   不注册给 Lua，只在 main() 里直接调用。
 * 用来证明：即使 luaL_dofile 已经完全返回（脚本那次调用早就结束，
 * 当时的 Stack 参数区早已被后续操作反复覆盖），handler_ref 依然
 * 指向正确的 Function——因为它活在 Registry 里，跟"某次调用的
 * Stack 状态"没有任何关系。
 */
static void call_handler_from_c(lua_State *L, const char *msg) {
    invoke_handler(L, msg);
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "register_handler", l_register_handler);
    lua_register(L, "fire_handler", l_fire_handler);

    printf("[C] ---- 开始执行 register_demo.lua ----\n");
    if (luaL_dofile(L, "register_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    printf("[C] ---- register_demo.lua 执行完毕，脚本那次"
           "'进入 C' 早已结束 ----\n");

    /* 这里是第三次、完全独立的一次"进入 Lua 世界"：
     * 没有任何脚本正在运行，也没有任何 Lua 侧代码调用我们，
     * 纯粹是 main() 主动决定"现在该触发一次事件了"。
     * 如果 handler_ref 存的是当年那个 Stack index，这里必然
     * 已经失效；但因为它是 Registry Reference，依然有效。 */
    call_handler_from_c(L, "hello from C, long after the script returned");

    lua_close(L);
    return 0;
}
