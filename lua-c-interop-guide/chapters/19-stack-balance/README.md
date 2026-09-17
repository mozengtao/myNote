# 19. Stack Balance：每个 C Function 都应该有清晰的 Stack Contract

> 对应用户需求"第二十阶段：Stack Balance"。这是全书最重要的一章之一：
> 几乎所有 Lua/C 交互相关的疑难 bug，追到根子上都是某个 C 函数没有
> 遵守自己对 Stack 的承诺。

## 1. 直觉

写一个 C Function 去操作 Lua Stack，本质上是在做一件很像"借还"的事：
你从 Stack 上"借"一些临时槽位来完成计算，用完之后必须"还"回去——
Stack 在你的函数**进入前**和**离开后**应该处于你自己承诺的状态，这
就是 **Stack Contract**（栈契约）。

```text
Input:
    [1] table
Operation:
    lua_getfield()
Temporary:
    [2] value
Cleanup:
    lua_pop()
Output:
    no result
```

如果一个 C 函数不遵守自己的 Stack Contract——该 pop 的没 pop、该 push
1 个却 push 了 2 个、把某次调用里的 index 当成永久坐标——就会出现
**Stack Balance 错误**。这类错误的可怕之处在于：它们往往不会立刻崩溃，
而是让 Stack 悄悄"变形"，直到某个看似无关的后续操作读到错乱的数据，
或者 Stack 无限增长最终耗尽内存。

## 2. Architecture

Stack Contract 的标准形状：

```text
Before（调用前）
    |
    v
Operation（该函数要做的核心操作：get/set/call/...）
    |
    v
Temporary Values（操作过程中产生的、只在函数内部有意义的临时值）
    |
    v
Cleanup（把 Temporary Values 全部清理掉：lua_pop）
    |
    v
After（调用后：Stack 恰好等于"Before + 承诺要 push 的返回值"）
```

用一条公式概括：

```text
Stack Contract = Before -> Operation -> Temporary Values -> Cleanup -> After
```

任何一个环节没做对，`After` 就不等于预期，Stack 就"失衡"了。本章
Section 7 会逐一拆解 5 类真实存在、可编译、可运行的失衡场景。

## 3. 完整代码

本章代码在 [`code/`](code/) 目录，可直接 `make run` 验证。全部 5 个
Failure 示例的"bad / good"版本都在同一个可执行文件里依次演示。

`code/stack_balance_demo.c`：

```c
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* 示例 1：忘记 pop 导致 Stack 随"反复调用"无限增长 */
static void frame_tick_bad(lua_State *L, int config_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, config_ref); /* +1: config table */
    lua_getfield(L, -1, "name");                   /* +1: name，忘记 pop */
    lua_getfield(L, -2, "age");                    /* +1: age，忘记 pop */
}

static void frame_tick_good(lua_State *L, int config_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, config_ref);
    lua_getfield(L, -1, "name");
    lua_getfield(L, -2, "age");
    lua_pop(L, 3); /* 用完立即清理 */
}

/* 示例 2a：push 了 2 个值却 return 1 */
static int l_bad_over_push_under_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b); /* 想要的"和" */
    lua_pushinteger(L, a * b); /* 多算的"积" */
    return 1; /* bug: "和"被丢弃 */
}

/* 示例 2b：只 push 了 1 个值却 return 2 */
static int l_bad_under_push_over_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1); /* [1] 参数仍在 Stack 上 */
    lua_pushinteger(L, a * 2);               /* [2] 真正想返回的值 */
    return 2; /* bug: [1] 位置的参数被误当成第一个返回值 */
}

/* 示例 3：lua_error 之后的代码是死代码 */
static int l_bad_after_error(lua_State *L) {
    lua_Integer x = luaL_checkinteger(L, 1);
    if (x < 0) {
        luaL_error(L, "bad_after_error: x must be >= 0, got %d", (int)x);
        printf("[bad_after_error] 这一行绝不会被打印\n"); /* 永远不会执行 */
        lua_pushinteger(L, x * 2);
        return 1;
    }
    lua_pushinteger(L, x * 2);
    return 1;
}

/* 示例 4：把 Stack index 存成全局变量，跨调用复用 */
static int g_bad_stashed_index = 0;

static int l_bad_stash_index(lua_State *L) {
    g_bad_stashed_index = 1;
    lua_getfield(L, g_bad_stashed_index, "value");
    lua_pop(L, 1);
    return 0;
}

static int l_bad_use_stashed_index(lua_State *L) {
    /* 下次调用时，位置 1 可能已经是完全不同类型的值 */
    if (lua_istable(L, g_bad_stashed_index)) {
        lua_getfield(L, g_bad_stashed_index, "value");
        lua_pop(L, 1);
    }
    return 0;
}

/* 示例 5：lua_next 循环里忘记 pop */
static int l_bad_lua_next_leak(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        /* bug: 完全没有 lua_pop，下一轮 lua_next 会把 value 误当成 key */
    }
    return 0;
}
```

完整的 5 个示例（含全部 good/bad 版本、main 驱动逻辑）见
[`code/stack_balance_demo.c`](code/stack_balance_demo.c)，配套脚本见
[`code/stack_balance_demo.lua`](code/stack_balance_demo.lua)。

`code/Makefile`：

```makefile
GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common
TARGET := stack_balance_demo
SRCS := stack_balance_demo.c
include $(GUIDE_ROOT)/Makefile.rules
```

`make run` 的实际输出（节选，完整输出见 Section 7 各小节）：

```text
== 示例 1: frame_tick_bad 被反复调用（模拟每帧调用一次） ==
调用前 stack top = 0
  frame 1 结束后 stack top = 3（每帧 +3，说明在持续泄漏）
  ...
  frame 5 结束后 stack top = 15（每帧 +3，说明在持续泄漏）

改用 frame_tick_good 之后：
  frame 1 结束后 stack top = 0（保持不变）
  ...
```

## 4. Stack Diagram

Stack Contract 的通用图示（对照 Section 1 的例子）：

```text
Before:
    [1] table

Operation: lua_getfield(L, 1, "name")
    [1] table
    [2] name          <- Temporary

Cleanup: lua_pop(L, 1)
    [1] table

After:
    [1] table          (与 Before 完全一致，no result)
```

一旦 Cleanup 缺失或数量不对，`After` 就会和承诺的不一致——这正是
Section 7 每个 Failure 示例要具体分析的内容。

## 5. Data Flow

```text
调用者（Lua 或 C）准备好 Before 状态的 Stack
        |
        v
   C Function 被调用
        |
        +-- 读取输入（luaL_check* / lua_to*）
        |
        +-- 执行 Operation，可能产生 Temporary Values
        |
        +-- Cleanup：清理所有 Temporary Values
        |
        +-- Push 承诺的返回值，return N
        |
        v
   Stack 回到"Before + N 个返回值"的状态
        |
        v
   调用者读取返回值，继续往下走
```

**任何一步跳过 Cleanup，或者 push/return 的数量与承诺不符，这条
Data Flow 从这个函数往后的所有假设都会开始出错**——不一定立刻报错，
很多时候是悄悄传递错误数据，很久之后才在完全无关的地方爆发。

## 6. Lifecycle

以"被反复调用的 C 函数"为例，看 Stack Balance 如何影响它的生命周期：

```text
第 1 次调用
    Before: top = T0
    ...
    After:  top = T0 + Δ   (Δ 应该等于承诺的返回值个数)
        |
        v
第 2 次调用
    Before: top = T0 + Δ   (如果 Δ 符合承诺，这里和第一次调用前的相对状态一致)
    ...
        |
        v
       ...
        |
        v
第 N 次调用
    如果每次 Δ 都对，Stack 水位始终可预期
    如果每次都多出一点没清理的 Temporary Value，
    Stack 水位会随 N 线性增长，直至：
        - lua_checkstack 检测到空间不足而失败，或
        - 触及 Lua 的硬性栈上限，运行时抛出 "stack overflow"，或
        - 进程内存被逐渐耗尽
```

这就是为什么"每帧调用一次"这种长期运行、高频调用的函数，对 Stack
Balance 的要求比"只调用一次的函数"高得多——小泄漏乘以高频次，就是
大问题。

## 7. Failure（5 个真实可编译的 Stack Balance 错误）

### Failure 1：忘记 pop，Stack 随反复调用无限增长

```c
static void frame_tick_bad(lua_State *L, int config_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, config_ref); /* +1 */
    lua_getfield(L, -1, "name");                   /* +1，忘记 pop */
    lua_getfield(L, -2, "age");                     /* +1，忘记 pop */
}
```

**预期 Stack 状态 vs 实际 Stack 状态**：

```text
预期（每次调用后应该回到调用前的水位）：
    frame 1 后: top = T0
    frame 2 后: top = T0
    frame 3 后: top = T0

实际（每次多留 3 个临时值）：
    frame 1 后: top = T0 + 3
    frame 2 后: top = T0 + 6
    frame 3 后: top = T0 + 9
    ...
```

`make run` 实测输出完全印证了这一点：

```text
调用前 stack top = 0
  frame 1 结束后 stack top = 3（每帧 +3，说明在持续泄漏）
  frame 2 结束后 stack top = 6（每帧 +3，说明在持续泄漏）
  frame 3 结束后 stack top = 9（每帧 +3，说明在持续泄漏）
  frame 4 结束后 stack top = 12（每帧 +3，说明在持续泄漏）
  frame 5 结束后 stack top = 15（每帧 +3，说明在持续泄漏）
```

**为什么危险**：这里的 `frame_tick_bad` 是宿主自己直接调用的 C 辅助
函数（不是通过 Lua 的 CALL/RETURN 机制被调用），所以 Lua **不会**在
它"返回"时自动帮它清理 Stack（这种自动清理只发生在真正跨越 Lua 调用
边界、即通过 `lua_call`/`lua_pcall`/Lua `CALL` 字节码触发的调用上）。
如果这是一个真实场景里"每处理一个包调用一次"的函数，Stack 会随着抓
包数量线性增长，用不了多久就会触发 `lua_checkstack` 失败或者让整个
进程的内存持续上涨。更隐蔽的是：即使这个泄漏函数本身**是**通过
`lua_pcall` 被调用的普通 Lua-callable 函数，只要它在**一次调用内部**
用循环反复 `lua_getfield`/`lua_rawgeti` 又不 pop（例如处理一个很大的
table 的每一项），同样会在**这一次调用返回之前**把 Stack 撑爆——
"返回时自动清理"救不了"还没返回就已经用超限"的情况。

**修复后（Cleanup 补上）**：

```c
static void frame_tick_good(lua_State *L, int config_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, config_ref);
    lua_getfield(L, -1, "name");
    lua_getfield(L, -2, "age");
    lua_pop(L, 3); /* 3 个 Temporary Value 全部清理 */
}
```

实测输出：

```text
改用 frame_tick_good 之后：
  frame 1 结束后 stack top = 0（保持不变）
  frame 2 结束后 stack top = 0（保持不变）
  frame 3 结束后 stack top = 0（保持不变）
  frame 4 结束后 stack top = 0（保持不变）
  frame 5 结束后 stack top = 0（保持不变）
```

### Failure 2：函数返回了错误数量的值

**2a：实际 push 了 2 个值，却 `return 1`**

```c
static int l_bad_over_push_under_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b); /* [top-1] 和：调用者真正想要的 */
    lua_pushinteger(L, a * b); /* [top]   积：多算的一个值 */
    return 1; /* bug */
}
```

**预期 Stack 状态 vs 实际 Stack 状态**（以 `bad_over_push_under_return(3, 4)` 为例）：

```text
预期：调用者拿到 1 个返回值，且这个值应该是"和"（如果本意就是只返回和）
    结果: 7

实际：Lua 只认"栈顶 N 个值"是返回值，N=1 时只有栈顶那个被保留，
      也就是"积"，"和"被静默丢弃：
    结果: 12
```

实测输出：

```text
bad_over_push_under_return(3,4) 只拿到 1 个返回值 =	12	（期望的『和』7 被悄悄丢弃，这其实是『积』12）
```

**为什么危险**：这不是崩溃，而是**悄悄返回了错误的值**——比死代码
更难发现，因为程序看起来"正常运行"，只是结果是错的。

**修复后**：

```c
static int l_good_over_push_under_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b);
    lua_pushinteger(L, a * b);
    return 2; /* 如实声明 push 了几个 */
}
```

```text
good_over_push_under_return(3,4) =	7	12	（和, 积 都拿到了）
```

**2b：实际只 push 了 1 个值，却 `return 2`**

```c
static int l_bad_under_push_over_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1); /* [1] 参数，此时仍在 Stack 上 */
    lua_pushinteger(L, a * 2);               /* [2] 真正想返回的值 */
    return 2; /* bug */
}
```

**预期 Stack 状态 vs 实际 Stack 状态**（以 `bad_under_push_over_return(5)` 为例）：

```text
预期：只 push 了 1 个新值，理应只 return 1，结果应该是: 10

实际：return 2 意味着"栈顶 2 个值都是返回值"，
      而栈顶往下数第 2 个位置，其实是参数 a 本身（还没被清理掉）：
    Stack（return 前）: [1] a=5   [2] a*2=10
    result1 = [1] = 5   (参数 a 被误当成第一个返回值！)
    result2 = [2] = 10
```

实测输出：

```text
bad_under_push_over_return(5) =	5	10	（第一个返回值 x 其实是参数 a 本身，不是有意 push 的结果！）
```

**为什么危险**：这个例子比 2a 更隐蔽——它"看起来"确实返回了 2 个
值，数量对得上心理预期，但其中一个根本不是有意计算出来的结果，而是
"凑巧还留在 Stack 上的旧数据"。如果参数本身恰好和预期结果长得很像
（比如都是整数），这种 bug 可能长期潜伏不被发现。

**修复后**：

```c
static int l_good_under_push_over_return(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_pushinteger(L, a);     /* 显式 push 第一个想返回的值 */
    lua_pushinteger(L, a * 2); /* 显式 push 第二个想返回的值 */
    return 2; /* Stack 上确实有 2 个我们自己 push 的值 */
}
```

### Failure 3：`lua_error`/`luaL_error` 之后继续写代码，以为它会"正常返回"

```c
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
```

**预期 Stack 状态 vs 实际 Stack 状态**：

```text
如果误以为 luaL_error 会"正常返回"，会以为执行顺序是：
    luaL_error(...) 打印/记录错误
        |
        v
    printf("这一行绝不会被打印")   <- 误以为这行会执行
        |
        v
    lua_pushinteger(L, x * 2)      <- 误以为这里会 push 一个值
        |
        v
    return 1                       <- 误以为最终返回了 x*2

实际执行顺序（longjmp 语义）：
    luaL_error(...) 内部调用 lua_error(L)
        |
        v
    longjmp 直接跳到最近一层 lua_pcall 的 setjmp 点
        |
        v
    本次调用剩下的所有语句（包括 printf 和 push）全部不会执行
        |
        v
    调用者（这里是 pcall）拿到 (false, 错误信息字符串)
```

实测输出证实了这一点——"这一行绝不会被打印"确实没有出现：

```text
pcall(bad_after_error, -1) -> ok=	false	err=	bad_after_error: x must be >= 0, got -1
（注意上面没有打印『这一行绝不会被打印』，证明 luaL_error 之后的代码确实没有执行）
```

**为什么危险**：如果心智模型上把 `luaL_error` 当成普通函数调用
（调用完会"回来"接着往下执行），就会写出两类问题代码：一类是像本例
这样，`luaL_error` 后面跟着永远执行不到的死代码（编译器通常不会报警，
因为 `luaL_error` 的函数签名就是普通的 `int`，没有 `noreturn` 标注）；
另一类更危险——以为某个分支"出错后还会继续往下走"，于是在后面写了
"清理逻辑"或者"兜底逻辑"，结果这些逻辑在真正出错时根本不会跑，
资源没被正确释放。

**修复思路**：不是"修复"死代码本身（它本来就不会执行，不会造成
运行时错误），而是**修复心智模型和代码风格**——把 `luaL_error` 写成
一个分支的"终点"，不要在它后面继续写任何本分支的逻辑：

```c
static int l_good_after_error(lua_State *L) {
    lua_Integer x = luaL_checkinteger(L, 1);
    if (x < 0) {
        return luaL_error(L, "good_after_error: x must be >= 0, got %d", (int)x);
        /* 这个分支到此为止，不再写任何代码 */
    }
    lua_pushinteger(L, x * 2);
    return 1;
}
```

### Failure 4：把某次调用的 Stack index 存成全局变量，下次复用

```c
static int g_bad_stashed_index = 0;

static int l_bad_stash_index(lua_State *L) {
    g_bad_stashed_index = 1; /* 这次调用里，参数 table 在位置 1 */
    lua_getfield(L, g_bad_stashed_index, "value");
    printf("[bad_stash_index] 本次调用 table.value = %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}

static int l_bad_use_stashed_index(lua_State *L) {
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
```

**预期 Stack 状态 vs 实际 Stack 状态**（先调用
`bad_stash_index({value="first-call-table"})`，再调用
`bad_use_stashed_index(42)`）：

```text
预期（如果 Stack index 真的能跨调用保持含义）：
    第二次调用时，位置 1 "应该"还是那张 table
    lua_istable(L, 1) == true
    可以继续 lua_getfield(L, 1, "value")

实际：
    第二次调用时，Lua 传入的实参是数字 42，
    位置 1 现在是 number 类型，不是 table
    g_bad_stashed_index（值仍是 1）指向的东西已经完全不同
```

实测输出：

```text
[bad_stash_index] 本次调用 table.value = first-call-table
[bad_use_stashed_index] 复用上次存的 index=1，本次该位置实际类型 = number
  不是 table，按上次的假设读取会拿到错误类型/产生崩溃风险，这里安全地跳过
```

**为什么危险**：这里特意让 `lua_istable` 检查兜底，才没有直接崩溃或
读到垃圾数据；如果代码里没有这层防御，直接对一个 number 位置调用
`lua_getfield`（对非 table 值取字段，Lua 5.4 在没有 `__index`
metamethod 时会直接抛出"attempt to index a number value"这样的
Lua Error），或者用 `lua_touserdata`/`lua_topointer` 之类更"信任"
调用者的 API，后果可能是拿到无意义的指针甚至崩溃。根本问题在于：
**Stack index 是"本次调用"的临时坐标，不是可以长期持有的对象引用**。

**修复思路**：需要跨调用持有 Lua Object 时，用
`luaL_ref(L, LUA_REGISTRYINDEX)` 把"值本身"存进 Registry，拿到一个
和调用时机、Stack 布局完全无关的稳定整数引用：

```c
static int g_good_registry_ref = LUA_NOREF;

static int l_good_store_ref(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 1);
    g_good_registry_ref = luaL_ref(L, LUA_REGISTRYINDEX); /* luaL_ref 自己会 pop 掉这个值 */
    return 0;
}

static int l_good_use_ref(lua_State *L) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_good_registry_ref); /* 无论这次参数是什么，取回的都是当初那个 table */
    lua_getfield(L, -1, "value");
    lua_pop(L, 2);
    return 0;
}
```

实测输出（`good_use_ref(42)` 传入的参数完全不影响取回结果）：

```text
[good_store_ref] 已将 table 存入 Registry，引用编号 = 4
[good_use_ref] 取回引用 4，类型 = table
  value = stable-value
```

### Failure 5：`lua_next` 循环里忘记 pop

（可参照 Chapter 08 的正确写法，这里从 Stack Balance 角度独立重写。）

```c
static int l_bad_lua_next_leak(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L); /* 第一个 key：nil 表示"从头开始" */
    int iterations = 0;
    while (lua_next(L, 1) != 0) {
        iterations++;
        printf("  [bad_lua_next_leak] iter=%d, stack top=%d (key、value 都没 pop)\n",
               iterations, lua_gettop(L));
        /* bug: 完全没有 lua_pop */
    }
    return 0;
}
```

**预期 Stack 状态 vs 实际 Stack 状态**（表 `{a=1, b=2, c=3}`）：

```text
预期（正确用法：每轮处理完 value 后 lua_pop(L, 1)）：
    iter 1: push [key1, value1] -> 处理 -> pop value1 -> 栈上只剩 [key1]
    iter 2: lua_next 用 key1 找下一个 key -> push [key2, value2] -> pop value2
    ...
    循环结束时 lua_next 返回 0，自己已经把最后的 key 弹掉，
    栈恰好回到"只有 table"的状态

实际（完全不 pop）：
    iter 1: push [key1, value1]，栈顶现在是 value1，不是 key1
    第 2 次调用 lua_next(L, 1) 时，Lua 会把当前栈顶（value1）
    当成"上一个 key"去查表 —— 但 value1（这里是数字 1）
    根本不是这张表的合法 key（这张表的 key 都是字符串），
    于是 lua_next 直接抛出 "invalid key to 'next'" 的 Lua Error
```

实测输出：

```text
  [bad_lua_next_leak] iter=1, stack top=3 (key、value 都没 pop)
pcall(bad_lua_next_leak, t) -> ok=	false	err=	invalid key to 'next'
（第 1 轮之后栈顶变成了 value 而不是 key，第 2 次 lua_next 触发 'invalid key to next' 错误）
```

**为什么危险**：这是 Failure 1（无限增长）的一个"更快触发的特化版
本"——不用等到 Stack 真的爆掉，语义错乱（`value` 被误当成 `key`）
会在下一轮循环立刻引发一次 Lua Error。如果调用方没有用 `pcall`
保护，这次错误还会像 Chapter 18 讨论的那样，沿调用链一路
`longjmp` 上去。

**修复后**：

```c
static int l_good_lua_next_ok(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        /* 处理 key(-2)/value(-1) */
        lua_pop(L, 1); /* 只 pop value，把 key 留给下一次 lua_next */
    }
    return 0;
}
```

实测输出：

```text
  [good_lua_next_ok] iter=1, stack top=3 (处理完 value 后立即 pop)
  [good_lua_next_ok] iter=2, stack top=3 (处理完 value 后立即 pop)
  [good_lua_next_ok] iter=3, stack top=3 (处理完 value 后立即 pop)
  [good_lua_next_ok] 遍历结束，stack top=1（应等于遍历前）
```

## 8. Debugging

排查 Stack Balance 问题的通用步骤：

1. **先画 Before/After**：任何怀疑有 Stack Balance 问题的函数，
   先把"进入前 Stack 长什么样""离开后 Stack 应该长什么样"画出来，
   再对照实际代码逐行核对每个 `push`/`get`/`pop`。
2. **善用 `lua_gettop(L)` 打点**：像本章代码一样，在关键位置打印
   `lua_gettop(L)`，反复调用时观察它是否按预期"回到原点"。
3. **区分"跨调用自动清理"和"调用内部自己泄漏"**：`lua_call`/
   `lua_pcall`/Lua `CALL` 字节码触发的调用，在返回时会自动把 Stack
   收敛为"Before + N 个返回值"；但如果泄漏发生在**同一次调用内部**
   （比如一个大循环），这个自动清理帮不上任何忙——问题会在这次调用
   还没返回之前就已经发生。
4. **不要相信被存成变量的 Stack index**：只要看到一个 `int`/全局变量
   被当成"某个 Stack 位置"跨函数、跨调用使用，第一反应就是检查它是
   不是应该换成 `luaL_ref(L, LUA_REGISTRYINDEX)`。
5. **`lua_next` 循环必须配对 `lua_pop`**：这是最容易犯、也最容易通过
   代码 review 一眼看出的模式——凡是看到 `while (lua_next(...))` 循环
   体里没有恰好一次 `lua_pop(L, 1)`，几乎可以肯定有问题。

## 9. Mental Model

```text
Stack Contract = Before -> Operation -> Temporary Values -> Cleanup -> After

每个 C 函数都对 Stack 有一份隐含的契约：
    进入前是什么样，离开后就应该恰好是"原样 + 承诺的返回值"。

Stack Balance 错误的共同本质：
    Temporary Value 没有在 Cleanup 阶段被清理干净，
    或者 push 的数量与 return 声明的数量对不上，
    或者把"本次调用"的临时坐标当成了可以跨调用使用的永久引用。

记住一句话：
    Stack index 是坐标，不是身份证；
    需要长期持有 Lua Object，用 Registry Reference，不要用 Stack index。
```
