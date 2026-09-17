# 21. 完整 Lifecycle：把 Chapter 01-19 串成一条时间线

> 对应用户需求 Stage 23。本章不引入任何新 API，只做一件事：把前面
> 19 章里分散讲过的每一个动作，按时间先后顺序重新串成两条完整的
> 调用链路——**Lua -> C** 和 **C -> Lua**。读完本章，你应该能不看
> 目录，就把"某个具体 API 属于这条时间线上的哪一步"answer 出来。

## 21.1 为什么需要这一条时间线（Why）

Chapter 01-19 每一章都在"放大"这条时间线上的某一个点：Chapter 02-03
放大了"Arguments visible through Stack"这一步；Chapter 05 放大了
"read values"这一步；Chapter 18-19 放大了"return N / Stack Balance"
这一步……这种"逐点放大"的讲法有一个副作用：读完全部章节之后，
知识点是分散的，读者手里有一堆放大镜，但没有一张把它们全部摆回
原位的地图。

本章就是那张地图：先给出完整链路图，再逐段标注"这一步对应第几章
的哪些 API"，最后用两个前面章节里**已经编译验证过**的真实例子，
把地图和代码对上号。

## 21.2 Lua -> C 完整链路

```text
Lua source
    |
    v
Lua VM
    |
    v
Resolve C function
    |
    v
Create call frame
    |
    v
Arguments visible through Stack
    |
    v
C function
    |
    +-- inspect Stack
    +-- validate arguments
    +-- read values
    +-- perform C operation
    +-- push results
    +-- return N
    |
    v
Lua VM
    |
    v
Lua receives results
```

逐段对照表：

| 时间线上的这一步 | 实际发生的事 | 涉及的 Lua C API | 对应章节 |
|---|---|---|---|
| Lua source | `.lua` 脚本里写下一次函数调用，如 `add(10, 20)` | 无（纯 Lua 语法） | Chapter 00 |
| Lua VM | 字节码执行到 `CALL` 指令 | 无（VM 内部） | Chapter 00 |
| Resolve C function | VM 发现被调用对象是一个 `lua_CFunction` 指针（由 `lua_register`/`luaL_setfuncs` 注册过） | `lua_register`、`lua_pushcfunction` | Chapter 00, 01 |
| Create call frame | VM 暂停自己的字节码执行，跳转到这个 C 函数指针，为这次调用建立新的 Stack 区间 | 无（VM 内部机制，C 侧不可见） | Chapter 00 |
| Arguments visible through Stack | 参数已经出现在 Stack 位置 1、2、3…… | `lua_gettop`（确认有多少个）、正/负索引 | Chapter 02, 03 |
| inspect Stack | 在动手之前，先看看 Stack 上到底有什么、有几个 | `lua_gettop`、`lua_type`、`lua_is*` | Chapter 02, 03, 05 |
| validate arguments | 校验类型/数量是否符合预期，不符合就直接报错退出 | `luaL_checktype`、`luaL_checkinteger`、`luaL_checkstring`、`luaL_argerror` | Chapter 05, 06, 18 |
| read values | 把 Stack 上的 Lua Value 转换成真正的 C 变量 | `lua_tointeger`、`lua_tonumber`、`lua_tolstring`、`lua_toboolean` | Chapter 05 |
| perform C operation | 纯 C 世界的逻辑：加法、拼字符串、访问硬件…… | 无（普通 C 代码） | Native C World |
| push results | 把 C 世界算出来的结果重新变成 Lua Value，放上 Stack | `lua_pushinteger`、`lua_pushstring`、`lua_pushnil` 等 Push 系列 | Chapter 01, 05 |
| return N | 告诉 Lua VM："栈顶 N 个值就是这次调用的返回值" | `return N;`（C 函数的普通 `return`，但语义是"Stack 契约声明"） | Chapter 01, 19 |
| Lua VM | VM 从 Stack 顶部取回 N 个值，销毁这次调用的 call frame | 无（VM 内部） | Chapter 00 |
| Lua receives results | `local r = add(10, 20)` 里的 `r` 拿到了结果 | 无（纯 Lua 语法） | Chapter 00 |

### 用 Chapter 01 的 `add` 走一遍（真实代码）

```1:11:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/01-first-call-add/code/add_demo.c
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int l_add(lua_State *L) {
    int a = (int)luaL_checkinteger(L, 1);
    int b = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b);
    return 1;
}
```

对照上面的表格逐行看：

- `luaL_checkinteger(L, 1)` / `luaL_checkinteger(L, 2)` 同时完成了
  **inspect Stack**（隐含检查了位置 1、2 确实存在且是数字）和
  **validate arguments + read values**（校验失败直接 `luaL_error`，
  成功则拿到 C 的 `int`）——这是 Chapter 05/06/18 讲的校验系列
  一步做了三件事的典型写法。
- `a + b` 就是 **perform C operation**，纯 C 语法，跟 Lua C API
  完全无关。
- `lua_pushinteger(L, a + b)` 是 **push results**。
- `return 1;` 是 **return N**：告诉 Lua VM"栈顶 1 个值是返回值"。

## 21.3 C -> Lua 完整链路

```text
C
 |
 +-- get Lua function
 +-- push arguments
 |
 v
Lua Stack
 |
 v
lua_pcall
 |
 v
Lua VM
 |
 v
Lua function
 |
 v
return values
 |
 v
Lua Stack
 |
 v
C reads results
```

逐段对照表：

| 时间线上的这一步 | 实际发生的事 | 涉及的 Lua C API | 对应章节 |
|---|---|---|---|
| get Lua function | 把一个 Lua Function 重新 Push 到 Stack 顶部——可能来自全局变量，也可能来自 Registry Reference | `lua_getglobal`（全局变量）、`lua_rawgeti(L, LUA_REGISTRYINDEX, ref)`（长期持有的引用） | Chapter 04（全局）, 09, 10（Registry） |
| push arguments | 把要传给这个 Lua Function 的参数依次 Push 到 Stack，紧跟在函数对象之后 | `lua_push*` | Chapter 01, 05 |
| Lua Stack | 此刻 Stack 上依次是：函数对象、参数 1、参数 2…… | 无（状态描述） | Chapter 02, 03 |
| `lua_pcall` | 触发 Lua VM 执行这个 Function；出错时不让错误穿透，而是转成状态码返回 | `lua_pcall`（保护调用）；对比 `lua_call`（普通调用，出错直接 `longjmp` 穿透） | Chapter 04, 18 |
| Lua VM | VM 开始执行这个 Lua Function 的字节码 | 无（VM 内部） | Chapter 00 |
| Lua function | Lua 侧的业务逻辑，可能又反过来调用别的 C Function（形成 Chapter 17 讲的"双向嵌套"） | 无（纯 Lua 代码） | Chapter 17 |
| return values | Lua Function 执行 `return ...`，把结果放上它自己的执行帧顶部 | 无（Lua 语法） | Chapter 00 |
| Lua Stack | VM 把返回值搬回 `lua_pcall` 调用之前那段 Stack 区间 | 无（VM 内部机制） | Chapter 00 |
| C reads results | C 侧用 `lua_to*` 把返回值读成 C 变量，用完后 `lua_pop` 清理，保持 Stack Balance | `lua_tointeger`、`lua_tostring`、`lua_pop` | Chapter 05, 19 |

### 用 Chapter 09/10 的 registry callback 走一遍（真实代码）

Chapter 09 的 `register_demo.c` 和 Chapter 10 的 `event_demo.c` 都是
"C -> Lua"链路里最典型、也最容易在长期持有场景下出错的版本：函数
对象不是来自 `lua_getglobal`，而是来自 Registry Reference。

```68:89:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/09-lua-function-to-c-registry/code/register_demo.c
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
```

三步对照表格里的三行：

1. `lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref)` = **get Lua
   function**，只是"从哪里 get"变成了 Registry 而不是全局变量。
2. `lua_pushstring(L, msg)` = **push arguments**。
3. `lua_pcall(L, 1, 0, 0)` = 触发 **Lua VM -> Lua function ->
   return values** 这整段，Lua 侧的执行细节对 C 完全不可见，
   C 只在 `lua_pcall` 返回之后才重新拿到控制权——如果返回非
   `LUA_OK`，说明"return values"这一步实际上被 Lua 的
   `error()` 替换成了"错误对象"，这正是 Chapter 18 讲的错误传播。

## 21.4 两条链路合并看：谁在跨越国境线

把 21.2 和 21.3 两张图叠在一起看，会发现一个规律：**不管方向是
Lua -> C 还是 C -> Lua，中间必然经过"Lua Stack"这一站**，而且
"谁在调用谁"永远由"当前控制权在哪一侧"决定：

```text
控制权在 Lua VM 手里
        |
        |  CALL 字节码遇到 lua_CFunction
        v
    跳到 C（Lua -> C 链路）
        |
        |  C 主动 lua_pcall / lua_call
        v
    跳回 Lua VM（C -> Lua 链路）
        |
        |  Lua Function 内部又调用另一个 C Function
        v
    再跳到 C……
```

Chapter 17（`bidirectional-calls`）讲的正是这种"控制权来回跳"的
嵌套场景——本质上还是这两条链路，只是嵌套发生了而已，没有第三种
链路。

## 21.5 Mental Model

> **一次 Lua/C 交互，不管从哪个方向发起，永远是"控制权跳到对方 ->
> 通过 Lua Stack 交换 Arguments/Return values -> 控制权跳回来"这同
> 一套时间线的两种走向；Chapter 01-19 学过的每一个 API，都能在这
> 条时间线上找到自己唯一的位置。**
