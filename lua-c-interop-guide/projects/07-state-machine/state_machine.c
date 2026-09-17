/*
 * state_machine.c
 *
 * Project 7: C State Machine Exposed to Lua
 *
 * 本案例把全书反复出现的几个核心概念串成一个真实可用的"连接状态机"：
 *
 *   1. Userdata + Metatable —— struct fsm 被 lua_newuserdata 包装成一个
 *      Lua Object，fsm:xxx() 方法调用全部走 __index 派发（对照 Chapter 11/12）。
 *   2. Registry Reference   —— on_transition() 传进来的 Lua Function 被
 *      长期存进"这个 fsm 实例自己的" callback_ref 字段（对照 Chapter 09）。
 *   3. lua_pcall             —— 每次状态转移发生后，C 主动"回头"调用 Lua
 *      回调，这是本例里唯一一次 C -> Lua 方向的调用（对照 Chapter 10/17）。
 *   4. luaL_error            —— 非法转移由 C 判定并直接抛 Lua Error，
 *      交给 Lua 侧的 pcall 兜底（对照 Chapter 18）。
 *   5. __gc                  —— fsm 被 GC 回收前释放它占住的 Registry
 *      Reference，避免回调闭包被 Registry 永久钉住（对照 Chapter 13）。
 *
 * Mental Model（本例要体现的分工）：
 *   C   = mechanism：真正维护 state，用一张状态转移表判断某个 event 在
 *         当前 state 下是否合法，合法就切换状态。
 *   Lua = policy：决定"收到一次状态变化之后要做什么"——打印日志、上报
 *         监控、触发别的逻辑——这段逻辑完全写在 on_transition 传进来的
 *         那个闭包里，C 从头到尾不关心闭包内部在做什么。
 */
#include <stdio.h>
#include <string.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define FSM_MT "state_machine.fsm"

/* 连接状态机的 5 + 1 个状态：INIT 是 fsm 刚被 create() 出来、尚未 start()
 * 之前的"占位状态"，不出现在用户需求原文的图里，但能让 start() 本身也
 * 是一次真正的状态转移（而不是一次什么都不做的空调用），从而完整覆盖
 * 用户需求里 "state_machine:start()" 这一步的 "C FSM -> event ->
 * state transition -> Lua callback" 全流程。 */
enum fsm_state {
    FSM_INIT = 0,
    FSM_IDLE,
    FSM_CONNECTING,
    FSM_CONNECTED,
    FSM_DISCONNECTING,
    FSM_ERROR,
    FSM_STATE_COUNT
};

static const char *state_names[FSM_STATE_COUNT] = {
    "INIT", "IDLE", "CONNECTING", "CONNECTED", "DISCONNECTING", "ERROR",
};

/* struct fsm：C 原生世界里真正的状态机。
 * 这块内存本身由 lua_newuserdata 分配、由 Lua GC 管理，
 * 但它的字段布局、读写方式完全是普通 C struct 的规则，Lua 并不理解它。 */
struct fsm {
    int state;        /* 当前状态：enum fsm_state 之一 */
    int callback_ref;  /* on_transition 回调的 Registry Reference；
                        * LUA_NOREF 表示还没有注册任何回调 */
};

/* 状态转移表：(from, event) -> to，用最朴素的 C 数组 + 线性查找实现。
 * 这张表就是 "mechanism" 本身——合法的转移路径由 C 一次性写死在这里，
 * Lua 侧永远无法绕过它直接篡改 state。 */
struct transition {
    int from;
    const char *event;
    int to;
};

static const struct transition transitions[] = {
    {FSM_INIT, "start", FSM_IDLE},
    {FSM_IDLE, "connect", FSM_CONNECTING},
    {FSM_CONNECTING, "connected", FSM_CONNECTED},
    {FSM_CONNECTING, "fail", FSM_ERROR},
    {FSM_CONNECTED, "disconnect", FSM_DISCONNECTING},
    {FSM_DISCONNECTING, "done", FSM_IDLE},
    {FSM_ERROR, "reset", FSM_IDLE},
};

#define NUM_TRANSITIONS ((int)(sizeof(transitions) / sizeof(transitions[0])))

/* check_fsm：从 Stack index idx 校验并取回一个 "state_machine.fsm" 类型的
 * Userdata。类型不对（比如传了个普通 table）luaL_checkudata 内部直接
 * luaL_error，不会把控制权交还给我们。 */
static struct fsm *check_fsm(lua_State *L, int idx)
{
    return (struct fsm *)luaL_checkudata(L, idx, FSM_MT);
}

/* find_transition：在 transitions 表里线性查找 (from, event)。
 * 找不到返回 -1——这正是 "C 判断某次转移是否合法" 这件事发生的地方。 */
static int find_transition(int from, const char *event)
{
    int i;
    for (i = 0; i < NUM_TRANSITIONS; i++) {
        if (transitions[i].from == from &&
            strcmp(transitions[i].event, event) == 0) {
            return transitions[i].to;
        }
    }
    return -1;
}

/* notify_transition：如果这个 fsm 实例注册过 on_transition，就用它的
 * Registry Reference 把当年存进去的 Lua Function 重新 Push 到 Stack，
 * 传入 (from_name, to_name) 两个字符串参数，lua_pcall 触发它执行。
 * 这是本例里唯一一次 "C 主动调用 Lua" 的时刻，其余全是 Lua 调 C。 */
static void notify_transition(lua_State *L, struct fsm *fsm, int from, int to)
{
    if (fsm->callback_ref == LUA_NOREF) {
        return;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, fsm->callback_ref); /* [.., callback] */
    lua_pushstring(L, state_names[from]);
    lua_pushstring(L, state_names[to]);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        fprintf(stderr, "[state_machine] on_transition 回调出错: %s\n",
                lua_tostring(L, -1));
        lua_pop(L, 1); /* 弹掉错误对象，保持 Stack Balance */
    }
}

/* do_fire：fire()/start() 共用的核心逻辑：
 *   查表 -> 合法就切换 state -> 通知回调；不合法直接 luaL_error。
 * luaL_error 会经由 longjmp 直接中止当前这次 C 函数调用，不会执行到
 * 后面任何一行——这正是"非法转移绝不能被静默忽略"的保证方式。 */
static int do_fire(lua_State *L, struct fsm *fsm, const char *event)
{
    int to = find_transition(fsm->state, event);
    if (to < 0) {
        return luaL_error(L,
                           "illegal transition: cannot fire '%s' from state '%s'",
                           event, state_names[fsm->state]);
    }

    int from = fsm->state;
    fsm->state = to;
    notify_transition(L, fsm, from, to);
    return 0;
}

static int l_fsm_start(lua_State *L)
{
    struct fsm *fsm = check_fsm(L, 1);
    return do_fire(L, fsm, "start");
}

static int l_fsm_fire(lua_State *L)
{
    struct fsm *fsm = check_fsm(L, 1);
    const char *event = luaL_checkstring(L, 2);
    return do_fire(L, fsm, event);
}

/* l_fsm_on_transition：注册回调。如果之前已经注册过，先 luaL_unref 旧的，
 * 避免每次重新注册都在 Registry 里留下一个再也用不到的槛位（引用泄漏，
 * 对照 Chapter 09 Failure 3）。 */
static int l_fsm_on_transition(lua_State *L)
{
    struct fsm *fsm = check_fsm(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (fsm->callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, fsm->callback_ref);
    }

    lua_pushvalue(L, 2); /* 把 Function 复制一份到栈顶，luaL_ref 只认栈顶 */
    fsm->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int l_fsm_current_state(lua_State *L)
{
    struct fsm *fsm = check_fsm(L, 1);
    lua_pushstring(L, state_names[fsm->state]);
    return 1;
}

/* __gc：fsm 被 GC 回收前触发。释放它占住的 Registry Reference，
 * 否则 on_transition 传入的那个闭包会被 Registry 永久"钉住"，
 * 永远不会被回收——即使这个 fsm 实例本身早就没人引用了。 */
static int l_fsm_gc(lua_State *L)
{
    struct fsm *fsm = check_fsm(L, 1);
    if (fsm->callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, fsm->callback_ref);
        fsm->callback_ref = LUA_NOREF;
    }
    return 0;
}

/* state_machine.create()：创建一个新的 fsm 实例。
 * 初始 state 是 FSM_INIT，callback_ref 是 LUA_NOREF（尚未注册回调）。 */
static int l_state_machine_create(lua_State *L)
{
    struct fsm *fsm = (struct fsm *)lua_newuserdata(L, sizeof(struct fsm));
    fsm->state = FSM_INIT;
    fsm->callback_ref = LUA_NOREF;

    luaL_getmetatable(L, FSM_MT); /* [userdata, metatable] */
    lua_setmetatable(L, -2);      /* 挂到 userdata 上，metatable 自动弹出 */
    return 1;
}

static const luaL_Reg fsm_methods[] = {
    {"start", l_fsm_start},
    {"fire", l_fsm_fire},
    {"on_transition", l_fsm_on_transition},
    {"current_state", l_fsm_current_state},
    {NULL, NULL},
};

static const luaL_Reg state_machine_lib[] = {
    {"create", l_state_machine_create},
    {NULL, NULL},
};

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    /* 建一张全局唯一的 "state_machine.fsm" metatable：
     *   __index 指向方法表（fsm:start()/:fire()/... 全部走这里派发）
     *   __gc    挂清理钩子（释放 Registry Reference） */
    luaL_newmetatable(L, FSM_MT);

    lua_newtable(L);
    luaL_setfuncs(L, fsm_methods, 0);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_fsm_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); /* metatable 建好了，暂时不需要再放在 Stack 上 */

    /* 注册全局 table：state_machine = { create = l_state_machine_create } */
    lua_newtable(L);
    luaL_setfuncs(L, state_machine_lib, 0);
    lua_setglobal(L, "state_machine");

    if (luaL_dofile(L, "state_machine.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
