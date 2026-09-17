/*
 * main.c
 *
 * Project 8: Plugin Architecture —— 全书的压轴案例。
 *
 * 架构（用户需求原文）：
 *
 *   Host Application
 *          |
 *          v
 *       Lua VM
 *          |
 *          v
 *    Lua Plugin
 *          |
 *          v
 *      C API
 *          |
 *          v
 *   Host Services
 *
 * 这个 main.c 就是 "Host Application"：它创建 Lua VM，构造一个
 * host table（= "C API" 这一层，里面既有普通字段也有 C Function），
 * 加载 plugin.lua，然后按 init -> on_event（2~3 次）-> shutdown 的
 * 顺序驱动这个插件，全程用 lua_pcall 兜住任何一步可能出现的 Lua Error，
 * 保证宿主程序本身永远不会因为插件出错而崩溃。
 *
 * 用到的核心机制（详见 README「知识点回顾」一节，对照 Chapter 00-25）：
 *   - Table 双向传递：host 这个 table 由 C 构造，塞进去 Lua 能读的字段和
 *     C Function，再整个交给 Lua 侧的 plugin.* 函数使用。
 *   - C 创建 Lua Function：host.log / host.services.transform 都是
 *     lua_pushcfunction 生成的、对 Lua 脚本而言跟普通函数没有区别的值。
 *   - Registry Reference：host table 本身、以及 plugin.init/on_event/
 *     shutdown 三个 Lua Function，全部用 luaL_ref 长期持有，脱离任何
 *     一次具体调用的 Stack 状态。
 *   - lua_pcall：dispatch_event() 与 call_with_host() 是本例里所有
 *     "C 主动调用 Lua" 的唯一入口，全部走保护调用。
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

/* ------------------------------------------------------------------ */
/* Host API：host.version / host.log() / host.services.transform()    */
/* ------------------------------------------------------------------ */

/* l_host_log：host.log(msg)。最朴素的一个 C Function：
 * 从 Stack 读一个字符串，printf 前缀 "[host] "。 */
static int l_host_log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    printf("[host] %s\n", msg);
    return 0; /* 告诉 Lua VM：这次调用没有返回值 */
}

/* l_host_services_transform：host.services.transform(x)。
 * 根据 payload 的 Lua 类型做不同的"变换"：
 *   字符串 -> 转大写
 *   数字   -> 翻倍
 *   其它类型（比如 table）-> 视为插件用法错误，luaL_error 拒绝。
 * 这一条 luaL_error 分支就是本案例 Failure 一节要触发的场景。 */
static int l_host_services_transform(lua_State *L)
{
    int type = lua_type(L, 1);

    if (type == LUA_TSTRING) {
        size_t len;
        const char *s = lua_tolstring(L, 1, &len);
        char *upper = (char *)malloc(len + 1);
        size_t i;
        for (i = 0; i < len; i++) {
            upper[i] = (char)toupper((unsigned char)s[i]);
        }
        upper[len] = '\0';
        lua_pushstring(L, upper);
        free(upper);
        return 1;
    }

    if (type == LUA_TNUMBER) {
        lua_Number n = lua_tonumber(L, 1);
        lua_pushnumber(L, n * 2);
        return 1;
    }

    return luaL_error(L, "host.services.transform: 不支持的 payload 类型 '%s'",
                       lua_typename(L, type));
}

/* build_host_table：构造 host = { version=.., log=.., services={transform=..} }，
 * 并用 luaL_ref 把这张 table 长期存进 Registry，返回它的 Registry Reference。
 *
 * 为什么要存进 Registry？因为 init/on_event/shutdown 分别在 main() 里
 * 相隔很远的几行代码里被调用，每次调用都要重新把 host 这个 Lua Object
 * push 到 Stack 上传给插件——Stack index 不能跨调用保存（Chapter 09），
 * 必须用 Registry Reference。 */
static int build_host_table(lua_State *L)
{
    lua_newtable(L); /* host = {} */

    lua_pushstring(L, "1.0.0");
    lua_setfield(L, -2, "version"); /* host.version */

    lua_pushcfunction(L, l_host_log);
    lua_setfield(L, -2, "log"); /* host.log */

    lua_newtable(L); /* host.services = {} */
    lua_pushcfunction(L, l_host_services_transform);
    lua_setfield(L, -2, "transform"); /* host.services.transform */
    lua_setfield(L, -2, "services");  /* host.services = {...} */

    /* Stack 顶部现在是完整的 host table，luaL_ref 把它搬进 Registry 并弹出 */
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

/* ------------------------------------------------------------------ */
/* payload：dispatch_event() 用到的、多类型事件负载                     */
/* ------------------------------------------------------------------ */

enum payload_kind {
    PAYLOAD_STRING,
    PAYLOAD_NUMBER,
    PAYLOAD_UNSUPPORTED /* 故意构造一个 transform() 处理不了的类型，演示 Failure */
};

struct payload {
    enum payload_kind kind;
    const char *str;
    lua_Number num;
};

/* push_payload：把一个 payload 变量按它的 kind push 到 Stack 上。
 * PAYLOAD_UNSUPPORTED 这里选择 push 一个空 table——对 Lua 脚本来说完全
 * 合法（能被 tostring()），但 host.services.transform 明确不认识它。 */
static void push_payload(lua_State *L, const struct payload *p)
{
    switch (p->kind) {
    case PAYLOAD_STRING:
        lua_pushstring(L, p->str);
        break;
    case PAYLOAD_NUMBER:
        lua_pushnumber(L, p->num);
        break;
    case PAYLOAD_UNSUPPORTED:
        lua_newtable(L);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* 生命周期驱动：init / on_event / shutdown                             */
/* ------------------------------------------------------------------ */

static int host_ref = LUA_NOREF;
static int init_ref = LUA_NOREF;
static int on_event_ref = LUA_NOREF;
static int shutdown_ref = LUA_NOREF;

/* grab_function_ref：假设 Stack 顶部是 plugin.<field_name>，
 * 校验它是 Function，luaL_ref 把它搬进 Registry（同时把它从 Stack 上弹出）。
 * 如果插件没有定义这个生命周期函数（类型不对/是 nil），打印警告、返回
 * LUA_NOREF，交给调用方决定"跳过这一步、继续跑后面的步骤"——这正是
 * "单个生命周期函数缺失/出错不能拖垮整个宿主"这条要求的一部分。 */
static int grab_function_ref(lua_State *L, const char *field_name)
{
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr,
                "[host] plugin.%s 不是一个 Function（实际类型: %s），跳过\n",
                field_name, lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
        return LUA_NOREF;
    }
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

/* load_plugin_lifecycle：跑完 plugin.lua 之后，从全局 table `plugin`
 * 里取出 init/on_event/shutdown 三个 Function，各自存一个 Registry
 * Reference。取的过程本身也可能失败（比如插件根本没定义 plugin 这个
 * table），失败就返回 0，交给 main() 决定要不要继续往下走。 */
static int load_plugin_lifecycle(lua_State *L)
{
    lua_getglobal(L, "plugin"); /* [plugin] */
    if (!lua_istable(L, -1)) {
        fprintf(stderr, "[host] plugin.lua 没有定义全局 table 'plugin'（实际类型: %s）\n",
                lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
        return 0;
    }

    lua_getfield(L, -1, "init"); /* [plugin, init] */
    init_ref = grab_function_ref(L, "init"); /* 弹出 init，栈回到 [plugin] */

    lua_getfield(L, -1, "on_event");
    on_event_ref = grab_function_ref(L, "on_event");

    lua_getfield(L, -1, "shutdown");
    shutdown_ref = grab_function_ref(L, "shutdown");

    lua_pop(L, 1); /* 弹掉 plugin table，Stack 回到调用前的状态 */
    return 1;
}

/* call_with_host：init(host) / shutdown(host) 共用的调用管道：
 *   取回 ref 对应的 Function -> push host（同样用 Registry Reference
 *   取回）-> lua_pcall(1, 0, 0)。任何 Lua Error 都在这里被捕获打印，
 *   不会向上传播、不会中止 main() 里后续的步骤。 */
static void call_with_host(lua_State *L, int ref, const char *phase)
{
    if (ref == LUA_NOREF) {
        fprintf(stderr, "[host] plugin.%s 未定义，跳过\n", phase);
        return;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);      /* [fn] */
    lua_rawgeti(L, LUA_REGISTRYINDEX, host_ref); /* [fn, host] */

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        fprintf(stderr, "[host] plugin.%s(host) 出错: %s\n", phase,
                lua_tostring(L, -1));
        lua_pop(L, 1); /* 弹掉错误对象，保持 Stack Balance */
    }
}

/* dispatch_event：通用事件分发管道，本例里唯一一个"取 ref + push host +
 * push name + push payload + lua_pcall(3,1,..) + 读返回值"的完整流程。
 * 无论 plugin.on_event 内部（包括它调用的 host.services.transform）
 * 出不出错，dispatch_event 都会把这一步稳稳地收住，绝不让错误穿透到
 * main() 之外。 */
static void dispatch_event(lua_State *L, const char *name, const struct payload *payload)
{
    if (on_event_ref == LUA_NOREF) {
        fprintf(stderr, "[host] plugin.on_event 未定义，忽略事件 '%s'\n", name);
        return;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, on_event_ref); /* [on_event] */
    lua_rawgeti(L, LUA_REGISTRYINDEX, host_ref);     /* [on_event, host] */
    lua_pushstring(L, name);                         /* [on_event, host, name] */
    push_payload(L, payload);                        /* [on_event, host, name, payload] */

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        fprintf(stderr, "[host] plugin.on_event('%s') 出错: %s\n", name,
                lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    if (lua_type(L, -1) == LUA_TSTRING || lua_type(L, -1) == LUA_TNUMBER) {
        printf("[host] on_event('%s') 返回值: %s\n", name, lua_tostring(L, -1));
    } else {
        printf("[host] on_event('%s') 返回值类型: %s\n", name,
               lua_typename(L, lua_type(L, -1)));
    }
    lua_pop(L, 1);
}

int main(void)
{
    /* stdout 默认是"全缓冲"（fully buffered）当它不是一个交互式终端时
     * （比如被 make run 重定向到文件/管道）；而下面 fprintf(stderr, ...)
     * 用的 stderr 默认无缓冲。如果不强制 stdout 改成"行缓冲"，printf 和
     * fprintf(stderr, ...) 的输出顺序在重定向场景下会乱序交错，
     * 干扰阅读——这跟 Lua/C 交互本身无关，只是一个常见的 C 标准库坑，
     * 但会直接影响下面演示 Failure 时"谁先谁后"的可读性，所以先处理掉。 */
    setvbuf(stdout, NULL, _IOLBF, 0);

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    printf("== Project 8: Plugin Architecture ==\n\n");

    host_ref = build_host_table(L);

    printf("-- load plugin.lua --\n");
    if (luaL_dofile(L, "plugin.lua") != LUA_OK) {
        fprintf(stderr, "[host] 加载 plugin.lua 失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    if (!load_plugin_lifecycle(L)) {
        fprintf(stderr, "[host] 插件生命周期函数缺失，直接退出\n");
        lua_close(L);
        return 1;
    }

    printf("\n-- phase: init --\n");
    call_with_host(L, init_ref, "init");

    printf("\n-- phase: on_event (3 个事件) --\n");
    {
        struct payload greet = {.kind = PAYLOAD_STRING, .str = "hello"};
        dispatch_event(L, "greet", &greet);

        struct payload tick = {.kind = PAYLOAD_NUMBER, .num = 21};
        dispatch_event(L, "tick", &tick);

        /* 故意用一个 transform() 处理不了的 payload 触发插件内部出错，
         * 验证 on_event 出错不会影响后面 shutdown 依然被调用。 */
        struct payload boom = {.kind = PAYLOAD_UNSUPPORTED};
        dispatch_event(L, "boom", &boom);
    }

    printf("\n-- phase: shutdown --\n");
    call_with_host(L, shutdown_ref, "shutdown");

    printf("\n-- cleanup --\n");
    if (init_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, init_ref);
    }
    if (on_event_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, on_event_ref);
    }
    if (shutdown_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, shutdown_ref);
    }
    if (host_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, host_ref);
    }

    lua_close(L);
    printf("\n== done ==\n");
    return 0;
}
