#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* =========================================================================
 * 示例 1：忘记 pop 导致 Stack 随"反复调用"无限增长
 *
 * 关键点：frame_tick_bad / frame_tick_good 不是通过 lua_CFunction 签名注册
 * 给 Lua 调用的函数，而是宿主 C 代码自己的"每帧/每包处理"辅助函数，直接接
 * 收 lua_State *L 使用，被宿主自己的循环反复调用。
 *
 * 这一点很重要：Lua 的 CALL/RETURN 机制（VM 触发的、或者 lua_pcall/
 * lua_call 触发的调用）在"函数返回"这一刻，会由 Lua 自动把 Stack 收敛回
 * 调用前的水位——这是 Lua C API 对函数返回值机制的保证，不需要你手动清理。
 * 但 frame_tick_* 根本不经过那条"调用/返回"边界，它只是普通 C 函数，
 * 如果内部忘记 pop，没有任何机制会替它清理，Stack 会跨越多次调用持续增长，
 * 直至 lua_checkstack 失败或者达到 Lua 的硬性栈上限而报 "stack overflow"。
 * ========================================================================= */

static void frame_tick_bad(lua_State *L, int config_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, config_ref); /* +1: config table */
    lua_getfield(L, -1, "name");                   /* +1: name，忘记 pop */
    lua_getfield(L, -2, "age");                    /* +1: age，忘记 pop */
    /* bug: 应该在这里 lua_pop(L, 3)，但完全没有清理 */
}

static void frame_tick_good(lua_State *L, int config_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, config_ref); /* +1: config table */
    lua_getfield(L, -1, "name");                   /* +1: name */
    lua_getfield(L, -2, "age");                    /* +1: age */
    lua_pop(L, 3); /* 用完立即清理，Stack 回到进入前的水位 */
}

/* =========================================================================
 * 示例 2a：push 了 2 个值却 return 1
 * ——多余的值被悄悄丢弃，且丢弃的很可能正是调用者真正想要的那一个。
 * ========================================================================= */
static int l_bad_over_push_under_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b); /* [top-1] 调用者真正想要的"和" */
    lua_pushinteger(L, a * b); /* [top]   "积"，多算的一个值 */
    return 1; /* bug: 只声明 1 个返回值 -> Lua 只把栈顶的"积"当结果，"和"被丢弃 */
}

static int l_good_over_push_under_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b); /* [top-1] 和 */
    lua_pushinteger(L, a * b); /* [top]   积 */
    return 2; /* 如实声明：2 个值都要返回 */
}

/* =========================================================================
 * 示例 2b：只 push 了 1 个值却 return 2
 * ——多出来的那个"返回值"其实是调用参数本身，被误当成了合法结果。
 * ========================================================================= */
static int l_bad_under_push_over_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1); /* [1] 参数 a，此时仍留在 Stack 上 */
    lua_pushinteger(L, a * 2);               /* [2] 真正想返回的值 */
    return 2; /* bug: 只有 1 个值是我们 push 的，"第二个返回值"其实是 [1] 位置的参数 a */
}

static int l_good_under_push_over_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_pushinteger(L, a);     /* [2] 显式 push 想返回的第一个值（这里刻意回显 a 本身作对照）*/
    lua_pushinteger(L, a * 2); /* [3] 第二个返回值 */
    return 2; /* 如实：Stack 上确实有 2 个我们自己 push 的值 */
}

/* =========================================================================
 * 示例 3：lua_error / luaL_error 之后的代码是死代码
 *
 * luaL_error 内部调用 lua_error(L)，本质是 longjmp 到最近一层 lua_pcall 的
 * setjmp 点。这一行代码之后，本次 C 函数调用里剩下的所有语句都不会被执行
 * 到——不是"跳过"，而是这个 C 函数的整个栈帧都已经被跳过。
 * ========================================================================= */
static int l_bad_after_error(lua_State *L) {
    lua_Integer x = luaL_checkinteger(L, 1);
    if (x < 0) {
        luaL_error(L, "bad_after_error: x must be >= 0, got %d", (int)x);
        /* ---- 以下代码在 x < 0 时【永远不会执行】---- */
        printf("[bad_after_error] 这一行绝不会被打印\n");
        lua_pushinteger(L, x * 2);
        return 1;
    }
    lua_pushinteger(L, x * 2);
    return 1;
}

static int l_good_after_error(lua_State *L) {
    lua_Integer x = luaL_checkinteger(L, 1);
    if (x < 0) {
        /* 用 "return luaL_error(...)" 这个写法收尾，形态上也提醒读者：
         * 这里就是这条分支的终点，之后不会再有语句。 */
        return luaL_error(L, "good_after_error: x must be >= 0, got %d", (int)x);
    }
    lua_pushinteger(L, x * 2);
    return 1;
}

/* =========================================================================
 * 示例 4：把某次调用的 Stack index 存成全局变量，跨调用复用
 *
 * Stack index 只在"当前这次 C 函数调用"内有意义。下次进入这个（或另一个）
 * C 函数时，Stack 上 1、2、3……这些位置早已被新的参数覆盖，旧 index 指向
 * 的是"当前调用里恰好在那个位置的东西"，语义上和上次完全无关。
 * ========================================================================= */
static int g_bad_stashed_index = 0;

static int l_bad_stash_index(lua_State *L) {
    g_bad_stashed_index = 1; /* 这次调用里，参数 table 在位置 1 */
    lua_getfield(L, g_bad_stashed_index, "value");
    printf("[bad_stash_index] 本次调用 table.value = %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}

static int l_bad_use_stashed_index(lua_State *L) {
    /* 这次调用的参数已经变了（比如这次传的是数字，不是 table），但代码仍
     * 然沿用上次存的 g_bad_stashed_index —— 它凑巧还是 1，可这次位置 1
     * 上根本不是 table。 */
    printf("[bad_use_stashed_index] 复用上次存的 index=%d，本次该位置实际类型 = %s\n",
           g_bad_stashed_index, luaL_typename(L, g_bad_stashed_index));
    if (lua_istable(L, g_bad_stashed_index)) {
        lua_getfield(L, g_bad_stashed_index, "value");
        printf("  value = %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    } else {
        printf("  不是 table，按上次的假设读取会拿到错误类型/产生崩溃风险，这里安全地跳过\n");
    }
    return 0;
}

/* 正确做法：不要跨调用保存 Stack index，需要跨调用持有 Lua Object 时用
 * luaL_ref(L, LUA_REGISTRYINDEX) 把"值本身"存进 Registry，拿到的是一个
 * 稳定的整数引用，与任何一次调用的 Stack 布局都无关。 */
static int g_good_registry_ref = LUA_NOREF;

static int l_good_store_ref(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 1); /* 复制一份要保存的值到栈顶 */
    g_good_registry_ref = luaL_ref(L, LUA_REGISTRYINDEX); /* luaL_ref 会自己 pop 掉这个值 */
    printf("[good_store_ref] 已将 table 存入 Registry，引用编号 = %d\n", g_good_registry_ref);
    return 0;
}

static int l_good_use_ref(lua_State *L) {
    if (g_good_registry_ref == LUA_NOREF) {
        printf("[good_use_ref] 尚未存过任何引用\n");
        return 0;
    }
    /* 无论这次调用传了什么参数，取回的都是当初存入的那个 table，
     * 因为用的是 Registry Reference，而不是某次调用里的 Stack index。 */
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_good_registry_ref);
    printf("[good_use_ref] 取回引用 %d，类型 = %s\n", g_good_registry_ref, luaL_typename(L, -1));
    lua_getfield(L, -1, "value");
    printf("  value = %s\n", lua_tostring(L, -1));
    lua_pop(L, 2);
    return 0;
}

/* =========================================================================
 * 示例 5：lua_next 循环里忘记 pop
 *
 * 参见 Chapter 08 的正确写法，这里独立重写一遍，专门从 Stack Balance 角度
 * 分析后果：正确用法是每次处理完 value 后 lua_pop(L, 1)，只弹出 value，
 * 把 key 留在栈顶给下一次 lua_next 当"上一个 key"用。如果完全不 pop，
 * 下一次 lua_next 会把当前栈顶的 value（而不是 key）当成"上一个 key"去
 * 查表——多数情况下这个 value 根本不是这张表的合法 key，会直接触发
 * "invalid key to 'next'" 的 Lua Error。
 * ========================================================================= */
static int l_bad_lua_next_leak(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L); /* 第一个 key：nil 表示"从头开始" */
    int iterations = 0;
    while (lua_next(L, 1) != 0) {
        iterations++;
        printf("  [bad_lua_next_leak] iter=%d, stack top=%d (key、value 都没 pop)\n",
               iterations, lua_gettop(L));
        /* bug: 完全没有 lua_pop，下一轮 lua_next 会把 value 误当成 key */
    }
    return 0;
}

static int l_good_lua_next_ok(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    int iterations = 0;
    while (lua_next(L, 1) != 0) {
        iterations++;
        printf("  [good_lua_next_ok] iter=%d, stack top=%d (处理完 value 后立即 pop)\n",
               iterations, lua_gettop(L));
        lua_pop(L, 1); /* 只 pop value，把 key 留给下一次 lua_next */
    }
    printf("  [good_lua_next_ok] 遍历结束，stack top=%d（应等于遍历前）\n", lua_gettop(L));
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    /* ---- 示例 1：先在纯 C 侧演示"反复调用导致 Stack 无限增长" ---- */
    printf("== 示例 1: frame_tick_bad 被反复调用（模拟每帧调用一次） ==\n");
    lua_newtable(L);
    lua_pushstring(L, "demo");
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "age");
    int config_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    printf("调用前 stack top = %d\n", lua_gettop(L));
    for (int frame = 1; frame <= 5; frame++) {
        frame_tick_bad(L, config_ref);
        printf("  frame %d 结束后 stack top = %d（每帧 +3，说明在持续泄漏）\n",
               frame, lua_gettop(L));
    }
    lua_settop(L, 0); /* 演示到此为止，手动清理，避免影响后面的示例 */

    printf("\n改用 frame_tick_good 之后：\n");
    for (int frame = 1; frame <= 5; frame++) {
        frame_tick_good(L, config_ref);
        printf("  frame %d 结束后 stack top = %d（保持不变）\n", frame, lua_gettop(L));
    }
    luaL_unref(L, LUA_REGISTRYINDEX, config_ref);

    /* ---- 注册示例 2-5 用到的 C 函数，交给配套 .lua 脚本调用 ---- */
    lua_register(L, "bad_over_push_under_return", l_bad_over_push_under_return);
    lua_register(L, "good_over_push_under_return", l_good_over_push_under_return);
    lua_register(L, "bad_under_push_over_return", l_bad_under_push_over_return);
    lua_register(L, "good_under_push_over_return", l_good_under_push_over_return);
    lua_register(L, "bad_after_error", l_bad_after_error);
    lua_register(L, "good_after_error", l_good_after_error);
    lua_register(L, "bad_stash_index", l_bad_stash_index);
    lua_register(L, "bad_use_stashed_index", l_bad_use_stashed_index);
    lua_register(L, "good_store_ref", l_good_store_ref);
    lua_register(L, "good_use_ref", l_good_use_ref);
    lua_register(L, "bad_lua_next_leak", l_bad_lua_next_leak);
    lua_register(L, "good_lua_next_ok", l_good_lua_next_ok);

    printf("\n== 示例 2-5：交给 stack_balance_demo.lua 驱动 ==\n");
    if (luaL_dofile(L, "stack_balance_demo.lua") != LUA_OK) {
        fprintf(stderr, "unexpected top-level lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
