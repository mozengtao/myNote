/*
 * event_system_demo.c
 *
 * Project 4 · Event Callback System
 *
 * 在 Chapter 09（一个 Registry Reference 长期持有一个 Lua Function）
 * 和 Chapter 10（把它包装成最小事件系统：on_event/emit_event，
 * 单一 handler、error handling）的基础上，做成一个更接近"真实事件系统"
 * 的版本：
 *
 *   - 支持多个不同的事件类型（"connected"、"message" ……），
 *     不再是全局唯一一个 handler_ref。
 *   - 每种事件类型可以注册多个回调，而不是"注册就覆盖上一个"。
 *   - 提供显式的反注册：off(handle)，而不是只能整体 clear_handler()。
 *
 *   Lua callback
 *       |
 *       v
 *   on(event, cb)         <- Registry：luaL_ref 长期持有这个 Lua Function
 *       |
 *       v
 *   emit(event, payload)  <- C event source：C 侧任意时刻决定"事件发生了"
 *       |
 *       v
 *   lua_pcall              <- 逐个回调保护调用，一个出错不影响其它回调
 *       |
 *       v
 *   Lua callback（被调用）
 *
 * 数据结构：按事件名分桶（bucket），每个桶内是一个固定大小的槛位数组，
 * 每个槛位存一个 (handle, Registry Reference) 对。选用"固定数组 + 按
 * handle 线性查找"而不是链表，是因为：
 *   1. 桶的数量（MAX_EVENT_TYPES）和每个桶的回调数量
 *      （MAX_CALLBACKS_PER_EVENT）在教学场景下都很小，线性扫描的成本
 *      可以忽略，换来的是不需要处理链表节点的 malloc/free 生命周期。
 *   2. 槛位天然带有"空洞"语义（off() 之后槛位置空，之后可以被新的
 *      on() 复用），比链表的插入/删除更直观地体现"槛位状态机"。
 *
 * 关于 on()/off() 的设计：为什么是 off(handle) 而不是
 * off(event_name, callback)？
 *   Lua Function 值本身没有"内容相等"的比较方式——两个语法上写得
 *   一样的 `function(x) print(x) end` 在 Lua 里是两个不同的对象
 *   （每次 `function ... end` 表达式求值都会创建一个新的 Closure），
 *   即使是同一个 Lua 变量里的同一个 Function，C 侧也只能拿到它在
 *   Registry 里的引用，没有"内容"可比。就算用 lua_rawequal 做"同一个
 *   对象"的判断，也要求调用方重新构造/持有那个原始的 Function 值——
 *   如果同一个 event 下注册了两次完全相同的匿名函数字面量（两个不同的
 *   Closure，但代码长得一样），lua_rawequal 也无法区分"要删哪一个"。
 *   更彻底的做法是：on() 返回一个 C 侧生成、全局唯一的整数 handle，
 *   off(handle) 直接按这个 handle 定位槛位——O(1) 语义清晰，
 *   且不要求调用方保留任何 Function 引用，只需要保留一个整数。
 */
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAX_EVENT_TYPES          4
#define MAX_CALLBACKS_PER_EVENT  4
#define MAX_EVENT_NAME_LEN       32

/*
 * callback_slot_t: 一个槛位存一次 on() 注册的结果。
 *   handle == 0 表示这个槛位当前空闲（0 不会被当作合法 handle 分配，
 *   见 g_next_handle 从 1 开始）。
 *   ref 是 luaL_ref(L, LUA_REGISTRYINDEX) 返回的 Registry Reference，
 *   槛位空闲时它的值没有意义。
 */
typedef struct {
    int handle;
    int ref;
} callback_slot_t;

/*
 * event_bucket_t: 一个事件名对应的所有已注册回调。
 */
typedef struct {
    char name[MAX_EVENT_NAME_LEN];
    int  in_use;   /* 这个桶是否已经绑定了某个事件名 */
    callback_slot_t slots[MAX_CALLBACKS_PER_EVENT];
} event_bucket_t;

static event_bucket_t g_buckets[MAX_EVENT_TYPES];
static int g_next_handle = 1;  /* 全局单调递增，保证每个 on() 返回的 handle 全局唯一 */

/*
 * find_bucket: 按事件名找桶。create=1 时，如果没找到就分配一个空桶
 * 并初始化；找不到且 create=0 时返回 NULL（emit 一个从未注册过的
 * 事件名是合法的、无害的场景，不需要创建桶）。
 * 找不到空桶可用时（create=1 但所有桶都已被其它事件名占用）返回 NULL。
 */
static event_bucket_t *find_bucket(const char *name, int create)
{
    event_bucket_t *free_bucket = NULL;

    for (int i = 0; i < MAX_EVENT_TYPES; i++) {
        if (g_buckets[i].in_use && strcmp(g_buckets[i].name, name) == 0) {
            return &g_buckets[i];
        }
        if (!g_buckets[i].in_use && free_bucket == NULL) {
            free_bucket = &g_buckets[i];
        }
    }

    if (!create || free_bucket == NULL) {
        return NULL;
    }

    free_bucket->in_use = 1;
    strncpy(free_bucket->name, name, MAX_EVENT_NAME_LEN - 1);
    free_bucket->name[MAX_EVENT_NAME_LEN - 1] = '\0';
    for (int i = 0; i < MAX_CALLBACKS_PER_EVENT; i++) {
        free_bucket->slots[i].handle = 0;
        free_bucket->slots[i].ref = LUA_NOREF;
    }
    return free_bucket;
}

/*
 * on(event_name, callback)
 *   Lua 侧：local h = on("connected", function(payload) ... end)
 *
 * Registry Reference 的生命周期从这里开始：luaL_ref 把 Stack 顶部的
 * Lua Function 存进 Registry，只要这个 ref 还没被 luaL_unref 释放，
 * 这个 Function 就不会被 GC 回收——即便 Lua 侧后续再也没有任何变量
 * 指向它（比如脚本传进来的是一个匿名函数字面量）。
 */
static int l_on(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (strlen(name) >= MAX_EVENT_NAME_LEN) {
        luaL_error(L, "on(\"%s\"): event name too long (max %d chars)",
                   name, MAX_EVENT_NAME_LEN - 1);
    }

    event_bucket_t *bucket = find_bucket(name, 1);
    if (bucket == NULL) {
        luaL_error(L, "on(\"%s\"): too many distinct event types registered (max %d)",
                   name, MAX_EVENT_TYPES);
    }

    int slot_idx = -1;
    for (int i = 0; i < MAX_CALLBACKS_PER_EVENT; i++) {
        if (bucket->slots[i].handle == 0) {
            slot_idx = i;
            break;
        }
    }
    if (slot_idx < 0) {
        luaL_error(L, "on(\"%s\"): callback slots exhausted (max %d), "
                   "call off() on an existing handle first",
                   name, MAX_CALLBACKS_PER_EVENT);
    }

    /* Stack 此刻：[1]=event_name [2]=callback。
     * lua_pushvalue 复制一份 callback 到栈顶，luaL_ref 只消费栈顶
     * 这一份，原始的位置 2 保持不受影响（这里其实不再需要它，
     * 但養成"不隐式依赖参数原位置"的习惯，对更复杂的 C Function
     * 更安全）。 */
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);  /* Stack -> Registry：Lua Function 开始被长期持有 */

    int handle = g_next_handle++;
    bucket->slots[slot_idx].handle = handle;
    bucket->slots[slot_idx].ref = ref;

    printf("[C] on(\"%s\"): registered, handle=%d (Registry Reference=%d, slot #%d)\n",
           name, handle, ref, slot_idx);

    lua_pushinteger(L, handle);
    return 1; /* 告诉 Lua VM：栈顶 1 个值（handle）是这次调用的返回值 */
}

/*
 * off(handle)
 *   Lua 侧：off(h)
 *
 * Registry Reference 生命周期的终点（如果调用了的话）：luaL_unref
 * 把这个槛位对应的引用从 Registry 里摘掉。如果 Lua 侧也没有其它
 * 变量再指向那个 Function，它就变成 GC 可回收的对象了。
 * 找不到对应 handle（比如已经 off 过一次，或者传了一个从没存在过的
 * 数字）时，只打印提示、返回 false，不视为错误——重复 off() 应该是
 * 安全的幂等操作，而不是一个需要 pcall 才能防住的崩溃点。
 */
static int l_off(lua_State *L)
{
    int handle = (int)luaL_checkinteger(L, 1);

    for (int i = 0; i < MAX_EVENT_TYPES; i++) {
        if (!g_buckets[i].in_use) {
            continue;
        }
        for (int j = 0; j < MAX_CALLBACKS_PER_EVENT; j++) {
            if (g_buckets[i].slots[j].handle == handle) {
                luaL_unref(L, LUA_REGISTRYINDEX, g_buckets[i].slots[j].ref); /* Registry Reference 生命周期结束 */
                printf("[C] off(%d): removed from event \"%s\" slot #%d "
                       "(released Registry Reference=%d)\n",
                       handle, g_buckets[i].name, j, g_buckets[i].slots[j].ref);
                g_buckets[i].slots[j].handle = 0;
                g_buckets[i].slots[j].ref = LUA_NOREF;
                lua_pushboolean(L, 1);
                return 1;
            }
        }
    }

    printf("[C] off(%d): handle not found (already removed, or never existed)\n", handle);
    lua_pushboolean(L, 0);
    return 1;
}

/*
 * emit(event_name, payload)
 *   Lua 侧：emit("connected", "client-A")
 *
 * C event source + callback invocation：依次对该事件名下每一个仍然
 * 有效的槛位执行 lua_rawgeti（Registry -> Stack，取回 Lua Function）
 * + lua_pushvalue（复制一份 payload 作为参数）+ lua_pcall（保护调用）。
 *
 * 核心约束：任何一个回调内部 error()，只影响它自己这一次 lua_pcall
 * 的返回值，绝不能中断循环——所以错误处理写在循环体内部，
 * 打印之后 continue 到下一个槛位，而不是向外传播。
 */
static int l_emit(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    if (lua_gettop(L) < 2) {
        lua_pushnil(L); /* payload 是可选参数：补齐位置 2，后面 lua_pushvalue(L, 2) 总是有效 */
    }

    event_bucket_t *bucket = find_bucket(name, 0);
    if (bucket == NULL) {
        printf("[C] emit(\"%s\"): no callbacks registered for this event, ignored\n", name);
        return 0;
    }

    int fired = 0;
    for (int i = 0; i < MAX_CALLBACKS_PER_EVENT; i++) {
        if (bucket->slots[i].handle == 0) {
            continue; /* 空槛位：可能从来没用过，也可能是 off() 留下的洞 */
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, bucket->slots[i].ref); /* GET: Registry -> Stack */
        lua_pushvalue(L, 2);                                     /* PUSH: 给这个回调单独复制一份 payload */

        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            /* 出错也要继续：打印错误、弹掉错误对象保持 Stack Balance，
             * 然后循环 continue 到下一个槛位。 */
            fprintf(stderr, "[C] emit(\"%s\"): callback handle=%d raised an error: %s\n",
                    name, bucket->slots[i].handle, lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }
        fired++;
    }

    printf("[C] emit(\"%s\"): %d callback(s) fired successfully\n", name, fired);
    return 0;
}

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "on", l_on);
    lua_register(L, "off", l_off);
    lua_register(L, "emit", l_emit);

    if (luaL_dofile(L, "event_system_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
