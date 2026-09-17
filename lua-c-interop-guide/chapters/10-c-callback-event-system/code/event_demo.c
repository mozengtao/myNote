/*
 * event_demo.c
 *
 * 在 Chapter 09 的基础上，把"Registry Reference 长期持有一个
 * Lua Function"这件事，包装成一个更完整的事件系统：
 *
 *   on_event(fn)      -- callback registration：把 fn 存进 Registry
 *   emit_event(name)  -- event source + callback invocation：
 *                        C 侧任意时刻决定"事件发生了"，
 *                        用 lua_rawgeti + lua_pcall 把 fn 取回来调用
 *   clear_handler()   -- cleanup：luaL_unref 释放引用，
 *                        并演示"不清理会怎样"
 *
 * 本例额外覆盖两个 Chapter 09 没有覆盖的点：
 *   1. error handling：如果 Lua 回调本身 error()，lua_pcall 返回
 *      非 0，C 侧必须能捕获、打印，然后继续正常运行——绝不能让
 *      "回调里的一个 bug"变成"整个宿主程序崩溃"。
 *   2. cleanup 的必要性：只要 Registry 里还有引用，Lua 那个
 *      Function 对象就不会被 GC 回收，即使 Lua 侧已经没有任何
 *      变量指向它。必须显式 luaL_unref，才能让 C 侧"放手"。
 *
 * Mental Model（本章要落地的一句话）：
 *   Callback = C 保存 Lua Function 的 Registry Reference。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* C 侧长期持有的、当前唯一的事件 handler 引用。
 * LUA_NOREF 表示"当前没有注册任何 handler"。 */
static int handler_ref = LUA_NOREF;

/*
 * on_event(f)
 *   Lua 侧：on_event(function(event) ... end)
 *
 * callback registration + callback storage：
 * 校验参数是 Function，然后把旧引用（如果有）释放掉，
 * 再把新 Function 存进 Registry。
 *
 * 注意"先 unref 旧的、再 ref 新的"这个顺序本身就是本章要强调的
 * 教学点之一：如果每次 on_event 都不释放旧引用，Registry 里会
 * 越积越多的"再也够不到、但 C 仍然占着"的槛位——这在 Failure
 * 部分会展开讲。
 */
static int l_on_event(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    if (handler_ref != LUA_NOREF) {
        printf("[C] on_event: 替换旧 handler（先释放旧的 Registry Reference "
               "= %d）\n", handler_ref);
        luaL_unref(L, LUA_REGISTRYINDEX, handler_ref);
    }

    handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    printf("[C] on_event: 已注册，Registry Reference = %d\n", handler_ref);
    return 0;
}

/*
 * emit_event(name)
 *   Lua 侧：emit_event("connected")
 *
 * event source（这里简化为"被脚本直接调用来模拟一次事件发生"）
 * + callback invocation：把当前注册的 handler 取回来、传入事件名、
 * 用 lua_pcall 触发执行。
 *
 * error handling：如果 handler 内部 error()，lua_pcall 返回非 0，
 * 我们打印错误信息、弹掉错误对象、正常 return——不崩溃、
 * 不中断后续的 emit_event 调用。
 */
static int l_emit_event(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    if (handler_ref == LUA_NOREF) {
        printf("[C] emit_event(\"%s\"): 当前没有注册任何 handler，忽略\n",
               name);
        return 0;
    }

    printf("[C] emit_event(\"%s\"): 触发 handler\n", name);

    lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref); /* GET: Registry -> Stack */
    lua_pushstring(L, name);                        /* PUSH: 参数 */

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        /* Lua 回调出错：Stack 顶部是错误对象（这里假设是字符串，
         * 更严谨的写法应该先用 lua_isstring 判断，见 Chapter 18）。
         * 打印后必须 lua_pop 把它弹掉，保持 Stack Balance，
         * 然后正常返回——绝不能让这里的错误变成 C 程序的崩溃。 */
        fprintf(stderr, "[C] emit_event(\"%s\"): handler 执行出错: %s\n",
                name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    return 0;
}

/*
 * clear_handler()
 *   Lua 侧：clear_handler()
 *
 * cleanup：显式释放 Registry Reference。
 * 这一步之后，只要 Lua 侧也没有其它变量再指向那个 Function，
 * 下一次 GC 就可能真正回收它——C 侧"放手"是必要条件之一。
 */
static int l_clear_handler(lua_State *L) {
    (void)L;
    if (handler_ref == LUA_NOREF) {
        printf("[C] clear_handler: 当前没有注册任何 handler，无需清理\n");
        return 0;
    }
    printf("[C] clear_handler: 释放 Registry Reference = %d\n", handler_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, handler_ref);
    handler_ref = LUA_NOREF;
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "on_event", l_on_event);
    lua_register(L, "emit_event", l_emit_event);
    lua_register(L, "clear_handler", l_clear_handler);

    if (luaL_dofile(L, "event_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
