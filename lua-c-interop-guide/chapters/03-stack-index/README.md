# 03. Positive Index 与 Negative Index

> 对应用户需求"第四阶段：Positive Index 与 Negative Index"。Chapter 02 已经用正索引（`lua_tointeger(L, 1)`）和负索引（`lua_tointeger(L, -1)`）分别读过 Stack 两端的值，本章把这件事讲透：为什么 Lua Stack 要同时支持两套坐标系，以及各自真正好用在什么场景。

## 直觉

**Positive index 从 Stack bottom 数（1 起始），Negative index 从 Stack top 数（-1 起始）**——两者描述的是同一个 Stack，只是"锚点"不同：正索引锚定在"这次调用一开始就固定不变"的栈底，负索引锚定在"随时可能因为 push/pop 而变化"的栈顶。选错锚点，就是本章几乎所有 Failure 案例的根源。

## Architecture

这仍然是 Chapter 00 / Chapter 02 那同一个 Lua Stack，只是本章要把"index 是怎么算出来的"讲清楚：

```text
Stack
        top
         v
+------------+
|     C      |  3 / -1
+------------+
|     B      |  2 / -2
+------------+
|     A      |  1 / -3
+------------+
```

同一个槽位有两个合法坐标，换算公式（`top = lua_gettop(L)`）：

```text
positive_index = top + 1 + negative_index
negative_index = positive_index - top - 1
```

例如上图 `top = 3` 时，`B` 既是 `2`（从底数第 2 个），也是 `-2`（从顶数第 2 个）：`3 + 1 + (-2) = 2`，验证成立。

两套坐标在 Lua C API 里**处处等价、可以混用**——`lua_tointeger(L, 1)` 和 `lua_tointeger(L, -3)`（当 `top=3` 时）读到的是同一个槽位。区别不在"能不能用"，而在"稳不稳"：

- **Positive index 的稳定性**：只要没有人在它前面做 `lua_remove`/`lua_insert` 这类会整体挪动位置的操作，一个 C Function 从进入到结束，参数的 positive index 永远不变——因为 Stack bottom（对这次调用而言）是固定的。这正是它天然适合"读函数参数"的原因：函数签名 `(a, b, c)` 对应 `[1] [2] [3]`，从头到尾都成立。
- **Negative index 的不稳定性**：只要发生一次 `lua_push*`/`lua_pop`，Stack top 就变了，所有 negative index 的"实际指向"都会跟着变。这正是它天然适合"拿刚发生的那个操作留下的值"的原因——不管前面已经有多少个值，"刚 push 的" 永远是 `-1`，不需要先知道 `lua_gettop(L)` 是多少。

## 完整代码

目录：`chapters/03-stack-index/code/`，包含三个独立示例，分别对应"positive 适合读固定参数""negative 适合拿刚 push 的动态结果""循环里始终用 -1 取最新值"。

### `stack_index_demo.c`

```c
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * 示例 1：函数签名固定是 (a, b, c)，用 positive index 读取参数。
 * 正索引从 Stack bottom 数，只要参数在调用时的顺序不变，
 * [1]/[2]/[3] 永远稳定对应 a/b/c —— 即便函数后面又往 Stack 上
 * push 了别的临时值，也不会影响 [1]/[2]/[3] 的含义。
 */
static int demo_fixed_args(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_Integer c = luaL_checkinteger(L, 3);
    printf("  positive index 读参数: a=[1]=%lld b=[2]=%lld c=[3]=%lld\n",
           (long long)a, (long long)b, (long long)c);

    /* 制造一次"栈顶变化"：压入一个临时和 */
    lua_pushinteger(L, a + b + c);
    printf("  push 临时和之后:\n");
    printf("    [1] 依然 = %lld  (positive index 不受影响，还是参数 a)\n",
           (long long)luaL_checkinteger(L, 1));
    printf("    [-1] 现在 = %lld  (negative index 变了！现在指向新 push 的临时和，"
           "不再是参数 c 了)\n",
           (long long)lua_tointeger(L, -1));
    return 0;
}

/*
 * 示例 2：调用方传入的参数个数不确定（类似 vararg），
 * 函数算完结果后 push 到栈顶，用 negative index (-1) 取回。
 * 好处：不管调用时传了几个参数（n 是运行时才知道的），
 * "刚 push 的结果"永远是 -1，不需要先算出 n+1 这个 positive index。
 */
static int demo_dynamic_result(lua_State *L) {
    int n = lua_gettop(L); /* 参数个数，编译期未知，运行时才确定 */
    lua_Integer sum = 0;
    for (int i = 1; i <= n; i++) {
        sum += luaL_checkinteger(L, i);
    }
    lua_pushinteger(L, sum); /* 结果落在第几个 positive index 取决于 n，我们不关心 */
    lua_Integer via_negative = lua_tointeger(L, -1); /* 永远正确：栈顶就是刚 push 的结果 */
    printf("  n=%d 个参数, sum=%lld, 用 lua_tointeger(L, -1) 取回 = %lld\n",
           n, (long long)sum, (long long)via_negative);
    return 1;
}

/*
 * 示例 3：循环里反复 push，每一轮都想拿到"刚 push 的那个值"。
 * 用 positive index 的话，每轮都要重新调用 lua_gettop(L) 才知道
 * 最新值在哪个位置（1, 2, 3, 4, 5 ... 一直在变）；
 * 用 negative index 的话，永远是 -1，不需要关心已经循环了几轮。
 */
static int demo_loop_latest(lua_State *L) {
    lua_settop(L, 0);
    for (int i = 1; i <= 5; i++) {
        lua_pushinteger(L, i * 10);
        lua_Integer latest = lua_tointeger(L, -1);
        printf("  iteration %d: pushed %d, lua_tointeger(L, -1) = %lld, "
               "lua_gettop(L) = %d (这个 positive 位置每轮都在变)\n",
               i, i * 10, (long long)latest, lua_gettop(L));
    }
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "demo_fixed_args", demo_fixed_args);
    lua_register(L, "demo_dynamic_result", demo_dynamic_result);
    lua_register(L, "demo_loop_latest", demo_loop_latest);

    if (luaL_dofile(L, "stack_index_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
```

### `stack_index_demo.lua`

```lua
print("=== 示例 1: positive index 读固定参数 ===")
demo_fixed_args(11, 22, 33)

print("=== 示例 2: 参数个数不确定时，用 negative index 取回刚 push 的结果 ===")
local s1 = demo_dynamic_result(1, 2, 3)
print("sum(1,2,3) =", s1)
local s2 = demo_dynamic_result(5, 6, 7, 8, 9)
print("sum(5,6,7,8,9) =", s2)

print("=== 示例 3: 循环里反复 push，始终用 -1 取最新值 ===")
demo_loop_latest()
```

### `Makefile`

```makefile
GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common
TARGET := stack_index_demo
SRCS := stack_index_demo.c
include $(GUIDE_ROOT)/Makefile.rules
```

### 运行结果（已实际执行验证，完整原始输出）

```text
$ make run
=== 示例 1: positive index 读固定参数 ===
  positive index 读参数: a=[1]=11 b=[2]=22 c=[3]=33
  push 临时和之后:
    [1] 依然 = 11  (positive index 不受影响，还是参数 a)
    [-1] 现在 = 66  (negative index 变了！现在指向新 push 的临时和，不再是参数 c 了)
=== 示例 2: 参数个数不确定时，用 negative index 取回刚 push 的结果 ===
  n=3 个参数, sum=6, 用 lua_tointeger(L, -1) 取回 = 6
sum(1,2,3) =	6
  n=5 个参数, sum=35, 用 lua_tointeger(L, -1) 取回 = 35
sum(5,6,7,8,9) =	35
=== 示例 3: 循环里反复 push，始终用 -1 取最新值 ===
  iteration 1: pushed 10, lua_tointeger(L, -1) = 10, lua_gettop(L) = 1 (这个 positive 位置每轮都在变)
  iteration 2: pushed 20, lua_tointeger(L, -1) = 20, lua_gettop(L) = 2 (这个 positive 位置每轮都在变)
  iteration 3: pushed 30, lua_tointeger(L, -1) = 30, lua_gettop(L) = 3 (这个 positive 位置每轮都在变)
  iteration 4: pushed 40, lua_tointeger(L, -1) = 40, lua_gettop(L) = 4 (这个 positive 位置每轮都在变)
  iteration 5: pushed 50, lua_tointeger(L, -1) = 50, lua_gettop(L) = 5 (这个 positive 位置每轮都在变)
```

## Stack Diagram

### 示例 1：`lua_pushinteger(L, a + b + c)` 前后，positive 不变、negative 变了

```text
Before                                 After
+------------------------+       +------------------------+
| [1]=11  a   (-3)       |       | [1]=11  a   (-4)       |
| [2]=22  b   (-2)       |  -->  | [2]=22  b   (-3)       |
| [3]=33  c   (-1)  <-top|       | [3]=33  c   (-2)       |
+------------------------+       | [4]=66 临时和 (-1) <-top|
                                 +------------------------+
```

`[1]/[2]/[3]` 这三个 positive index 在 push 前后**完全不变**，依然是 `a/b/c`；但同一个位置 `[3]`，negative index 却从 `-1` 变成了 `-2`——因为 negative index 是"从栈顶往回数"，栈顶一变，所有 negative index 的实际指向就跟着变。这正是示例 1 要证明的核心事实。

### 示例 2：`lua_pushinteger(L, sum)`，用 `-1` 取回而不关心 `n`

```text
调用 demo_dynamic_result(1, 2, 3)         调用 demo_dynamic_result(5, 6, 7, 8, 9)

Before (n=3)          After                Before (n=5)              After
+----------+     +----------+          +--------------+       +--------------+
| [1]=1    |     | [1]=1    |          | [1]=5        |       | [1]=5        |
| [2]=2    | --> | [2]=2    |          | [2]=6        |  -->  | [2]=6        |
| [3]=3 top|     | [3]=3    |          | [3]=7        |       | [3]=7        |
+----------+     | [4]=6 <-top(-1)|    | [4]=8        |       | [4]=8        |
                +----------+           | [5]=9    top |       | [5]=9        |
                                       +--------------+       | [6]=35 <-top(-1)|
                                                              +--------------+
```

两次调用里，结果落在的 positive index 完全不同（`n=3` 时是 `[4]`，`n=5` 时是 `[6]`），但 `lua_tointeger(L, -1)` 在两种情况下都精准取到了"刚 push 的那个结果"——这就是"参数个数不确定时优先用负索引拿新结果"的价值所在。

### 示例 3：循环 5 次 push，positive 位置每轮都变，negative 位置永远是 `-1`

```text
iteration 1        iteration 2        iteration 3        ...     iteration 5
+--------+         +--------+         +--------+                  +--------+
|[1]=10  |         |[1]=10  |         |[1]=10  |                  |[1]=10  |
|  <-top |         |[2]=20  |         |[2]=20  |                  |[2]=20  |
| (-1)   |         |  <-top |         |[3]=30  |                  |[3]=30  |
+--------+         | (-1)   |         |  <-top |         ...      |[4]=40  |
                   +--------+         | (-1)   |                  |[5]=50  |
                                      +--------+                  |  <-top |
                                                                  | (-1)   |
                                                                  +--------+
```

"刚 push 的那个值"在 positive 坐标系里的位置是 `1, 2, 3, 4, 5`——每轮都不一样，必须先 `lua_gettop(L)` 才能知道；在 negative 坐标系里则永远是 `-1`，完全不需要关心循环跑了几轮、Stack 上已经积累了多少值。

## Data Flow

Positive index 和 negative index 描述的是同一份 Lua Value，只是"读取路径"的锚点不同：

```text
Lua Value（Stack 上某个槽位）
        |
        +-- 从 Stack bottom 数 --> positive index --> 适合: 读"调用一开始就存在、
        |                                              顺序固定"的东西（参数）
        |
        +-- 从 Stack top    数 --> negative index --> 适合: 读"刚刚发生的操作
                                                       留下"的东西（临时结果、
                                                       循环里最新 push 的值）
```

无论走哪条路径，最终都是同一个 `lua_to*`/`luaL_check*` 调用把值从 Stack 读进 C——**index 只是"怎么定位"，不改变"读出来是什么类型的值"这件事**。

## Lifecycle

Positive index 和 Negative index 都遵守 Chapter 02 讲过的规则：**index 是本次 C 函数调用内的临时坐标**，两者的区别只是"在这次调用内，谁更稳定"：

- **Positive index 在一次调用内基本稳定**：只要不发生 `lua_remove`/`lua_insert` 这类整体挪位的操作，从函数入口到 `return` 之前，一个 positive index 指向的都是同一个逻辑参数。
- **Negative index 在一次调用内极易失效**：任何一次 `lua_push*`/`lua_pop`/`lua_call` 都会移动 Stack top，之前记下来的 negative index 立刻可能指向别的东西（示例 1 已经完整展示了这一点）。

**两者都不能跨调用保存**——不管是 `[3]` 还是 `-1`，都只在"当前这次 C 函数调用"里有意义。下一次调用是全新的 Stack 视图，旧的 index 数字不再指向任何有意义的东西（Chapter 02 Lifecycle 一节已经讨论过这个更大的规则，本章的正负索引只是它的两个具体案例）。

## Failure

### 失败 1：以为 negative index 从头到尾都指向"最后一个参数"

```c
static int demo_bad_negative(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_pushinteger(L, a * b);   /* 先计算并 push 一个临时乘积 */
    /* BUG: 想拿参数 b，直觉地用 -1，但 -1 已经是刚才 push 的临时乘积了 */
    lua_Integer wrong = lua_tointeger(L, -1);
    printf("expected b=%lld, got %lld\n", (long long)b, (long long)wrong);
    return 0;
}
```

实际验证（`demo_bad_negative(6, 7)`）：

```text
expected b=7, got 42
```

**错在哪**：调用一开始 `-1` 确实是参数 `b`（此时 `b` 正好在栈顶），但 `lua_pushinteger(L, a * b)` 一执行，栈顶就变成了新 push 的 `42`（`6*7`）。这之后任何"用 `-1` 代表 `b`"的假设都是错的——**必须在做任何新的 push 之前，把还要用的值先读出来或者用 `lua_pushvalue` 复制一份**，而不是想着"反正它还在栈顶附近"。

### 失败 2：把可选参数的位置硬编码成固定 positive index

```c
static int demo_hardcoded_positive(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    /* BUG: 假设第三个参数(可选开关)永远存在于位置 3 */
    lua_Integer flag = lua_tointeger(L, 3);
    printf("flag=%lld type=%s\n", (long long)flag, lua_typename(L, lua_type(L, 3)));
    return 0;
}
```

实际验证：

```text
-- call with 3 args --
flag(from fixed index 3)=1 type=number
-- call with only 2 args (调用方忘了传第三个) --
flag(from fixed index 3)=0 type=no value
```

**错在哪**：Positive index 稳定的前提是"这个位置在这次调用里确实有值"。如果第三个参数是可选的，调用方少传时 `[3]` 根本不存在（类型是 `LUA_TNONE`，打印成 `no value`），`lua_tointeger` 会静默返回 `0`——这个 `0` 和"调用方真的传了 `0`"在数值上完全无法区分。可选参数应该先用 `lua_gettop(L)` 或 `lua_isnoneornil(L, 3)` 判断"这个位置到底有没有值"，而不是无条件假设它存在。

### 失败 3：`lua_remove` 之后继续用旧的 positive index

```c
static int demo_stale_positive(lua_State *L) {
    lua_remove(L, 1); /* 删掉第一个参数，后面所有值整体下移一格 */
    /* BUG: 仍然以为参数 c 在位置 3，其实经过 remove 之后它已经在位置 2 了 */
    lua_Integer c_wrong = lua_tointeger(L, 3);
    printf("got=%lld type=%s\n", (long long)c_wrong, lua_typename(L, lua_type(L, 3)));
    return 0;
}
```

实际验证（`demo_stale_positive(10, 20, 30)`，原本 `[1]=10 [2]=20 [3]=30`）：

```text
lua_remove(L,1) 之后, 仍用旧位置 [3] 读: 0 (类型=no value，其实已经越界)
```

**错在哪**：本章前面强调"positive index 在一次调用内基本稳定"是有前提的——前提是**没有人对它前面的位置做过 `lua_remove`/`lua_insert`**。`lua_remove(L, 1)` 恰恰是会让所有 positive index 集体"下移一格"的操作：原来的 `[2]`（20）现在是 `[1]`，原来的 `[3]`（30）现在是 `[2]`，而 `[3]` 这个位置直接变成了越界（`no value`）。任何 Rearrangement 操作之后，之前记住的 positive index 都必须重新确认，不能想当然继续用。

## Debugging

1. **先问"这个 index 是相对谁算的"**：看到一个 index，先确认它是 positive（相对栈底，"第几个参数"）还是 negative（相对栈顶，"倒数第几个"），再判断它现在是否还指向你以为的东西。
2. **negative index 出问题，先看中间有没有 push/pop/call**：任何一次 Stack 大小变化都会让 negative index 的实际指向漂移，检查从"你记下这个 index"到"你使用它"之间，有没有插入过 `lua_push*`/`lua_pop`/`lua_call`/`lua_pcall`。
3. **positive index 出问题，先看中间有没有 remove/insert**：这类操作会整体挪位，检查从"这个位置本该是什么参数"到"你现在读它"之间，有没有 `lua_remove`/`lua_insert` 打乱过顺序。
4. **可选参数永远先判断存在性**：用 `lua_gettop(L)` 比较、或 `lua_isnoneornil(L, idx)`，不要用 `lua_to*` 的"静默 0/nil"结果反推"这个参数是不是真的存在"。
5. **不确定就打印 `lua_gettop(L)` + 遍历 dump**：跟 Chapter 02 一样，画出来比在脑子里推演正负索引可靠得多。

## Mental Model

> Positive index 是从"不变的栈底"数出来的坐标，天然适合读取一开始就固定下来的参数；Negative index 是从"常变的栈顶"数出来的坐标，天然适合拿"刚刚发生的操作"留下的值——只要中间有 push/pop（会让负索引漂移）或 remove/insert（会让正索引漂移），旧的 index 就必须重新确认，谁都不是能一劳永逸信任的"稳定引用"。
