# Project 7：C State Machine Exposed to Lua

> 完整可运行代码见 [`state_machine.c`](state_machine.c) / [`state_machine.lua`](state_machine.lua)（`make run` 已验证）。

## 1. 场景简介

很多真实系统里都有这样一个角色分工：**C 负责维护一台状态机的"合法性"**（哪些转移允许发生、当前到底处于哪个状态），**Lua 负责决定"状态变化之后要做什么"**（打印日志、上报监控、触发下一步业务逻辑）。前者是 mechanism（机制），后者是 policy（策略）——这正是全书 Chapter 00 反复强调的"Lua VM 和 C 是两个完全不同的世界，Lua C API 是唯一的合法通道"这句话在一个具体场景里的落地。

本例实现一个"连接状态机"：

```text
IDLE -> CONNECTING -> CONNECTED -> DISCONNECTING -> IDLE
                 \
                  -> ERROR -> IDLE（reset 恢复）
```

用 `struct fsm { int state; int callback_ref; }` 包一层 Userdata 暴露给 Lua：

```lua
local fsm = state_machine.create()
fsm:on_transition(function(from, to)
    print(string.format("transition: %s -> %s", from, to))
end)
fsm:start()
fsm:fire("connect")     -- IDLE -> CONNECTING
fsm:fire("connected")   -- CONNECTING -> CONNECTED
fsm:fire("disconnect")  -- CONNECTED -> DISCONNECTING
fsm:fire("done")        -- DISCONNECTING -> IDLE
```

C 侧用一张 `(from, event) -> to` 的转移表判断某次 `fire()` 是否合法；一旦发生转移，就通过 Registry Reference 取回 `on_transition` 回调，`lua_pcall` 通知 Lua。非法转移（比如在 `IDLE` 直接 `fire("disconnect")`）由 C 用 `luaL_error` 直接拒绝，Lua 侧用 `pcall` 兜底决定怎么处理这次失败。

## 2. Architecture

```text
Lua
 |
 | fsm:start() / fsm:fire(event)
 v
C Function (l_fsm_start / l_fsm_fire)
 |
 | do_fire(): 查转移表
 v
   合法？
   |          \
  是           否
   |            \
   v             v
更新 state    luaL_error()
   |             |
   v             v
notify_transition()   Lua 侧 pcall 捕获
   |
   | lua_rawgeti(Registry, callback_ref) + lua_pcall
   v
Lua on_transition(from, to)   <- policy：打印日志 / 上报
```

`state_machine.fsm` 这个 Lua Object 的方法派发（`fsm:xxx()`）走 Metatable 的 `__index`：

```text
fsm:fire("connect")
     |
     v
Lua VM 发现 fsm 是 Userdata，没有直接的 "fire" 字段
     |
     v
查 __index（就是 fsm_methods 表）
     |
     v
找到 fsm_methods.fire == l_fsm_fire
     |
     v
CALL l_fsm_fire(fsm, "connect")
```

## 3. 完整代码

`struct fsm` 与状态转移表——这是 mechanism 本身，纯 C 数据结构，Lua 完全看不见它的布局：

```57:80:projects/07-state-machine/state_machine.c
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
```

`do_fire()`：查表、切换 state、通知回调、非法转移直接 `luaL_error`：

```127:144:projects/07-state-machine/state_machine.c
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
```

`notify_transition()`：用 Registry Reference 取回 `on_transition` 回调并 `lua_pcall`：

```106:125:projects/07-state-machine/state_machine.c
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
```

`on_transition()` 注册回调（先释放旧引用，防止泄漏）与 `__gc` 清理钩子：

```159:194:projects/07-state-machine/state_machine.c
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
```

`main()` 里建 Metatable（`__index` + `__gc`）并注册 `state_machine` 全局 table：

```222:250:projects/07-state-machine/state_machine.c
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
```

完整文件另见 [`state_machine.c`](state_machine.c)（含 `l_state_machine_create`、`check_fsm`、`find_transition` 等辅助函数）与配套脚本 [`state_machine.lua`](state_machine.lua)。

## 4. Stack Diagram

**`fsm:on_transition(function(from, to) ... end)` 调用期间**：

```text
调用刚开始（Lua VM 已把 self 和闭包放上去）：
+-----+---------------------------+
| idx | value                     |
+-----+---------------------------+
|  1  | fsm（Userdata，即 self）   |
|  2  | Function（那个闭包）       |
+-----+---------------------------+

lua_pushvalue(L, 2) 执行后：
+-----+---------------------------+
|  1  | fsm                       |
|  2  | Function                  |
|  3  | Function（复制的一份）     |  <- luaL_ref 只认栈顶
+-----+---------------------------+

luaL_ref(L, LUA_REGISTRYINDEX) 执行后：
+-----+---------------------------+
|  1  | fsm                       |
|  2  | Function                  |
+-----+---------------------------+
   <- 位置 3 上的那份被搬进 Registry 并弹出

C 原生世界：
fsm->callback_ref = 5   （举例；具体数值由 Lua 实现内部分配）
```

**`fsm:fire("connect")` 触发一次合法转移（`notify_transition` 内部）**：

```text
do_fire() 决定合法、已经把 fsm->state 从 IDLE 改成 CONNECTING 之后：
+-----+---------------------------+
| idx | value                     |
+-----+---------------------------+
|  1  | fsm                       |
|  2  | "connect"                 |
+-----+---------------------------+

lua_rawgeti(L, LUA_REGISTRYINDEX, callback_ref) 执行后：
+-----+---------------------------+
|  1  | fsm                       |
|  2  | "connect"                 |
|  3  | Function（重新取回的回调） |
+-----+---------------------------+

lua_pushstring x2（"IDLE"、"CONNECTING"）执行后：
+-----+---------------------------+
|  1  | fsm                       |
|  2  | "connect"                 |
|  3  | Function                  |
|  4  | "IDLE"                    |
|  5  | "CONNECTING"              |
+-----+---------------------------+

lua_pcall(L, 2, 0, 0) 执行后（0 个返回值）：
+-----+---------------------------+
|  1  | fsm                       |
|  2  | "connect"                 |
+-----+---------------------------+
   <- Function 和它的 2 个参数都被 pcall 内部消费掉了
```

**`fsm:fire("disconnect")` 在 `IDLE` 状态下触发非法转移**：

```text
do_fire() 里 find_transition(FSM_IDLE, "disconnect") 返回 -1
        |
        v
luaL_error(L, "illegal transition: ...")
        |
        v
内部执行 lua_error() -> longjmp，直接跳回最近的 lua_pcall
（这里是 Lua 脚本里包住 fsm:fire("disconnect") 的那个 pcall）
        |
        v
Stack 顶部只留一个字符串：错误消息
```

## 5. Lifecycle（Registry Reference 如何管理回调）

```text
Lua 里出现一个 on_transition 闭包
        |
        v
fsm:on_transition(fn) 被调用   -- 此刻 fn 同时"活"在：
        |                        Lua 变量、Stack 位置 2
        |
        | 如果 fsm->callback_ref 已经指向旧回调
        v   （比如脚本反复调用 on_transition 换了个新闭包）
先 luaL_unref 释放旧引用       -- 避免旧闭包被 Registry 永久钉住
        |
        v
lua_pushvalue + luaL_ref       -- 新闭包被搬进 Registry，
        |                        fsm->callback_ref 记下这个整数
        v
fsm->callback_ref 存在 struct fsm 里 -- 跟随这个 fsm 实例的
        |                        C 原生世界生命周期，
        |                        不受任何一次函数调用的 Stack 影响
        v
每次 fire() 触发合法转移       -- lua_rawgeti + lua_pcall
        |                        把闭包重新请回 Lua VM 执行
        v
fsm 这个 Userdata 不再被任何    -- 触发 __gc（l_fsm_gc）
Lua 变量引用，GC 决定回收它     |
        v                       v
                          luaL_unref(callback_ref)
                          释放 Registry 里的槛位，
                          此后闭包才可能真正被回收
```

关键点：**每个 `fsm` 实例的回调是"实例私有"的**——`callback_ref` 是 `struct fsm` 的一个字段，不是像 Chapter 09 示例那样的全局变量。如果脚本创建两个 `fsm`，它们的 `on_transition` 回调各自独立存在 Registry 的不同槛位里，互不干扰；`__gc` 保证每个实例被回收时都清理掉自己那一份，不会有槛位残留。

## 6. Failure

### 非法转移：在 `IDLE` 状态直接 `fire("disconnect")`

```lua
local ok, err = pcall(function()
    fsm:fire("disconnect")
end)
print("ok  =", ok)
print("err =", err)
```

实际运行输出：

```text
ok  =	false
err =	state_machine.lua:40: illegal transition: cannot fire 'disconnect' from state 'IDLE'
```

**为什么必须由 C 拒绝，而不是"让转移发生、让 Lua 自己判断对不对"？** 因为"当前状态是什么、哪些转移合法"是这台状态机的核心不变量（invariant），一旦让 Lua 侧有能力绕过转移表直接把 `fsm->state` 改成任意值，C 侧维护的这份状态就不再可信——任何依赖它做决策的代码（包括这个例子里的 `notify_transition`）都可能收到一个从未真正发生过的"转移"。`luaL_error` 保证非法请求在进入 `fsm->state = to` 那一行之前就被拦下，`fsm->state` 永远只能经过转移表里写死的路径变化。

**为什么 Lua 侧要用 `pcall` 包住它，而不是让程序直接崩溃？** 这正是 policy 该管的事：调用方最清楚"这次非法转移之后该怎么办"——可能是记一条日志继续跑，可能是上报告警，可能是重试。C 只负责"拒绝"，不负责"拒绝之后怎么办"，那是 Lua 的职责。

## 7. Mental Model

```text
C   = mechanism   -- 状态转移表 + luaL_error：谁有权限决定"能不能这样转移"
Lua = policy       -- on_transition 闭包：决定"转移发生之后要做什么"

fsm:fire(event)
     |
     v
do_fire(): 查表判断合法性（C 的职责边界，到此为止）
     |
     v
合法 -> 更新 state -> notify_transition -> lua_pcall -> Lua 闭包执行
     |
     v
不合法 -> luaL_error -> 交还给 Lua 侧的 pcall（Lua 的职责边界，从此开始）

回调的生命周期 = Registry Reference 的生命周期
     on_transition() 存进去，__gc 时释放，
     跟 fsm 这个 Userdata 实例本身的生命周期严格绑定。
```

> **一句话**：C 用一张写死的转移表守住"状态机不变量"这条底线，Lua 用一个存在 Registry 里的闭包决定"每次合法变化之后世界要发生什么"——谁都不越界，谁都不需要理解对方的实现细节，只通过 Lua Stack 和 Registry Reference 打交道。
