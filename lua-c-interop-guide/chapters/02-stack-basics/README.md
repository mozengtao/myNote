# 02. Lua Stack 基础：Inspection / Growth / Shrinking / Read / Write / Rearrangement

> 对应用户需求"第三阶段：Lua Stack 基础"。Chapter 01 已经让 Stack 在一次 `add` 调用里跑通了一遍，本章要做的是把 Stack 本身当成一个独立的数据结构讲透：它长什么样、有哪些基本操作、每个操作具体怎么改变它。

## 直觉

Lua Stack 就是一个有明确"栈底"和"栈顶"的值数组，`L` 里的每一次 Lua↔C 交互，本质上都只是这六类动作的组合：**看现在有多少个值（Inspection）、往上摞新值（Growth）、从上面拿掉值（Shrinking）、把某个值读出来（Read）、往某个位置写值（Write）、把已有的值重新排列（Rearrangement）**。记住这六个动词，比记住十个函数名更重要。

## Architecture

Stack 在 Chapter 00 的总体架构图里，是 `lua_State *L` 内部的一段区域，Lua C API 的几乎所有函数都是在读写它：

```text
    lua_State *L
        |
        v
    +---------------------------------------------+
    |               Lua Stack                     |
    |                                             |
    | index 1        2        3       ...    top  |
    | +-------+  +-------+  +-------+       +---+ |
    | |  V1   |  |  V2   |  |  V3   |  ...  |Vn | |
    | +-------+  +-------+  +-------+       +---+ |
    +---------------------------------------------+
        ^            ^                        ^
        |            |                        |
   Stack bottom   往上摞是 Growth         Stack top
                  往下拿是 Shrinking
```

本章要讲的 10 个 API，按"动作类型"分成六类：

| 类别 | 含义 | 本章覆盖的 API |
|---|---|---|
| Stack Inspection | 只读元数据，不改变内容 | `lua_gettop` |
| Stack Growth | 让 Stack 变大（C 值或已有值被放上新的栈顶） | `lua_pushinteger`、`lua_pushvalue` |
| Stack Shrinking | 让 Stack 变小（或重设大小） | `lua_settop`、`lua_pop` |
| Stack Read | 把 Stack 上某个位置的值读成 C 值，不改变 Stack | `lua_tointeger(L, 1)`、`lua_tointeger(L, -1)` |
| Stack Write | 把值写进某个已存在的槽位（覆盖），不改变 Stack 大小 | `lua_replace` |
| Stack Rearrangement | 在 Stack 内部搬动已有的值，不跨 Lua/C 边界 | `lua_remove`、`lua_insert`、`lua_replace` |

注意 `lua_replace` 同时出现在 Write 和 Rearrangement 两类里——这不是分类错误：从"往一个槽位里塞值"的角度看它是 Write；从"配合 remove/insert 完成栈内数据搬运"的角度看它又是 Rearrangement 三件套之一。后面 Stack Diagram 部分会给出它唯一一次完整图解，两类描述指向同一个函数。

## 完整代码

目录：`chapters/02-stack-basics/code/`

### `stack_basics_demo.c`

```c
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* 打印当前 Lua Stack 的完整内容，label 用来区分"调用前/调用后" */
static void dump_stack(lua_State *L, const char *label) {
    int top = lua_gettop(L);
    printf("  %s (top=%d): ", label, top);
    if (top == 0) {
        printf("<empty>");
    }
    for (int i = 1; i <= top; i++) {
        int t = lua_type(L, i);
        if (t == LUA_TNIL) {
            printf("[%d]=nil ", i);
        } else if (t == LUA_TNUMBER) {
            printf("[%d]=%g ", i, lua_tonumber(L, i));
        } else if (t == LUA_TSTRING) {
            printf("[%d]=\"%s\" ", i, lua_tostring(L, i));
        } else {
            printf("[%d]=%s ", i, lua_typename(L, t));
        }
    }
    printf("\n");
}

/* ---------- Stack Inspection ---------- */

static int demo_gettop(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_gettop(L) --\n");
    dump_stack(L, "before");
    int n = lua_gettop(L);
    printf("  lua_gettop(L) = %d\n", n);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Shrinking ---------- */

static int demo_settop(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    lua_pushinteger(L, 40);
    printf("-- lua_settop(L, 2)  [shrink] --\n");
    dump_stack(L, "before");
    lua_settop(L, 2);
    dump_stack(L, "after ");

    printf("-- lua_settop(L, 5)  [grow, 用 nil 填充] --\n");
    dump_stack(L, "before");
    lua_settop(L, 5);
    dump_stack(L, "after ");
    return 0;
}

static int demo_pop(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    lua_pushinteger(L, 40);
    lua_pushinteger(L, 50);
    printf("-- lua_pop(L, 2) --\n");
    dump_stack(L, "before");
    lua_pop(L, 2);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Growth ---------- */

static int demo_pushinteger(lua_State *L) {
    lua_settop(L, 0);
    printf("-- lua_pushinteger(L, 10) --\n");
    dump_stack(L, "before");
    lua_pushinteger(L, 10);
    dump_stack(L, "after ");
    return 0;
}

static int demo_pushvalue(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_pushvalue(L, 1)  [复制已有值到栈顶] --\n");
    dump_stack(L, "before");
    lua_pushvalue(L, 1);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Read ---------- */

static int demo_read(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 100);
    lua_pushinteger(L, 200);
    lua_pushinteger(L, 300);
    printf("-- lua_tointeger(L, 1) / lua_tointeger(L, -1) --\n");
    dump_stack(L, "before");
    lua_Integer first = lua_tointeger(L, 1);
    lua_Integer last  = lua_tointeger(L, -1);
    printf("  lua_tointeger(L, 1)  = %lld  (positive index -> 从 stack bottom 数)\n",
           (long long)first);
    printf("  lua_tointeger(L, -1) = %lld  (negative index -> 从 stack top 数)\n",
           (long long)last);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Rearrangement ---------- */

static int demo_remove(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_remove(L, 1) --\n");
    dump_stack(L, "before");
    lua_remove(L, 1);
    dump_stack(L, "after ");
    return 0;
}

static int demo_insert(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_insert(L, 1) --\n");
    dump_stack(L, "before");
    lua_insert(L, 1);
    dump_stack(L, "after ");
    return 0;
}

static int demo_replace(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_replace(L, 1) --\n");
    dump_stack(L, "before");
    lua_replace(L, 1);
    dump_stack(L, "after ");
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "demo_gettop", demo_gettop);
    lua_register(L, "demo_settop", demo_settop);
    lua_register(L, "demo_pop", demo_pop);
    lua_register(L, "demo_pushinteger", demo_pushinteger);
    lua_register(L, "demo_pushvalue", demo_pushvalue);
    lua_register(L, "demo_read", demo_read);
    lua_register(L, "demo_remove", demo_remove);
    lua_register(L, "demo_insert", demo_insert);
    lua_register(L, "demo_replace", demo_replace);

    if (luaL_dofile(L, "stack_basics_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
```

### `stack_basics_demo.lua`

```lua
print("=== Stack Inspection: lua_gettop ===")
demo_gettop()

print("=== Stack Shrinking: lua_settop ===")
demo_settop()

print("=== Stack Shrinking: lua_pop ===")
demo_pop()

print("=== Stack Growth: lua_pushinteger ===")
demo_pushinteger()

print("=== Stack Growth: lua_pushvalue ===")
demo_pushvalue()

print("=== Stack Read: lua_tointeger (positive/negative index) ===")
demo_read()

print("=== Stack Rearrangement: lua_remove ===")
demo_remove()

print("=== Stack Rearrangement: lua_insert ===")
demo_insert()

print("=== Stack Rearrangement: lua_replace ===")
demo_replace()
```

### `Makefile`

```makefile
GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common
TARGET := stack_basics_demo
SRCS := stack_basics_demo.c
include $(GUIDE_ROOT)/Makefile.rules
```

### 运行结果（已实际执行验证，完整原始输出）

```text
$ make run
=== Stack Inspection: lua_gettop ===
-- lua_gettop(L) --
  before (top=3): [1]=10 [2]=20 [3]=30
  lua_gettop(L) = 3
  after  (top=3): [1]=10 [2]=20 [3]=30
=== Stack Shrinking: lua_settop ===
-- lua_settop(L, 2)  [shrink] --
  before (top=4): [1]=10 [2]=20 [3]=30 [4]=40
  after  (top=2): [1]=10 [2]=20
-- lua_settop(L, 5)  [grow, 用 nil 填充] --
  before (top=2): [1]=10 [2]=20
  after  (top=5): [1]=10 [2]=20 [3]=nil [4]=nil [5]=nil
=== Stack Shrinking: lua_pop ===
-- lua_pop(L, 2) --
  before (top=5): [1]=10 [2]=20 [3]=30 [4]=40 [5]=50
  after  (top=3): [1]=10 [2]=20 [3]=30
=== Stack Growth: lua_pushinteger ===
-- lua_pushinteger(L, 10) --
  before (top=0): <empty>
  after  (top=1): [1]=10
=== Stack Growth: lua_pushvalue ===
-- lua_pushvalue(L, 1)  [复制已有值到栈顶] --
  before (top=3): [1]=10 [2]=20 [3]=30
  after  (top=4): [1]=10 [2]=20 [3]=30 [4]=10
=== Stack Read: lua_tointeger (positive/negative index) ===
-- lua_tointeger(L, 1) / lua_tointeger(L, -1) --
  before (top=3): [1]=100 [2]=200 [3]=300
  lua_tointeger(L, 1)  = 100  (positive index -> 从 stack bottom 数)
  lua_tointeger(L, -1) = 300  (negative index -> 从 stack top 数)
  after  (top=3): [1]=100 [2]=200 [3]=300
=== Stack Rearrangement: lua_remove ===
-- lua_remove(L, 1) --
  before (top=3): [1]=10 [2]=20 [3]=30
  after  (top=2): [1]=20 [2]=30
=== Stack Rearrangement: lua_insert ===
-- lua_insert(L, 1) --
  before (top=3): [1]=10 [2]=20 [3]=30
  after  (top=3): [1]=30 [2]=10 [3]=20
=== Stack Rearrangement: lua_replace ===
-- lua_replace(L, 1) --
  before (top=3): [1]=10 [2]=20 [3]=30
  after  (top=2): [1]=30 [2]=20
```

## Stack Diagram

下面对 10 个 API 逐一画出 Before -> 调用 -> After，图中数字与上面"运行结果"完全对应（都是从真实程序输出转写的，不是手算的）。

### Stack Inspection：`lua_gettop(L)`

只读取"当前有多少个值"，不改变任何内容：

```text
Before                          调用                         After
+------------------+       lua_gettop(L)             +------------------+
| [1]=10           |       返回 3，Stack 本身         |  [1]=10           |
| [2]=20           |       完全不受影响               |  [2]=20           |
| [3]=30      <-top|  --------------------------->  |  [3]=30      <-top|
+------------------+                                +------------------+
```

**用途**：`lua_gettop` 是几乎所有"防御性 C 代码"的起点——在做任何 Stack 操作前，先问一句"现在到底有几个值"，比假设一个固定数字安全得多。

### Stack Shrinking：`lua_settop(L, n)`

`lua_settop` 是"直接把 Stack 大小设成 n"，n 比当前小是收缩（丢弃多余的值），n 比当前大是扩张（用 `nil` 补齐）：

```text
收缩：lua_settop(L, 2)

Before                          After
+------------------+       +------------------+
| [1]=10           |       | [1]=10           |
| [2]=20           |  -->  | [2]=20      <-top|
| [3]=30           |       +------------------+
| [4]=40      <-top|
+------------------+
```

```text
扩张：lua_settop(L, 5)   （从上一步的结果继续）

Before                    After
+------------------+      +------------------+
| [1]=10           |      | [1]=10           |
| [2]=20      <-top|  --> | [2]=20           |
+------------------+      | [3]=nil          |
                          | [4]=nil          |
                          | [5]=nil     <-top|
                          +------------------+
```

**用途**：`lua_settop(L, 0)` 是最常见的写法之一——"清空 Stack 上属于我这次调用的所有内容"。但 Failure 部分会展示：如果调用它的时机不对，会把**还没读的参数**一起清空。

### Stack Shrinking：`lua_pop(L, n)`

`lua_pop(L, n)` 等价于 `lua_settop(L, -n-1)`——从栈顶丢弃 n 个值：

```text
Before                                After
+------------------+             +------------------+
| [1]=10           |             | [1]=10           |
| [2]=20           |             | [2]=20           |
| [3]=30           | lua_pop(2)  | [3]=30      <-top|
| [4]=40           |  ------->   +------------------+
| [5]=50      <-top|
+------------------+
```

**用途**：C 函数用完某些临时值后，习惯性地 `lua_pop` 掉，保持"用完即清"的 Stack Contract（详见 Chapter 19）。

### Stack Growth：`lua_pushinteger(L, 10)`

Push 系列的最基本形态：把一个 C 值转换成 Lua Value，放到新的栈顶：

```text
Before                     After
+------------------+       +------------------+
| <empty>          |  -->  | [1]=10      <-top|
+------------------+       +------------------+
```

### Stack Growth：`lua_pushvalue(L, 1)`

和 `lua_pushinteger` 不同，`lua_pushvalue` 不是从 C 值转换来的，而是**复制 Stack 上已经存在的某个值**，再放到新的栈顶——复制的是值本身，原来的槽位不受影响：

```text
Before                          After
+------------------+       +------------------+
| [1]=10           |       | [1]=10           |
| [2]=20           |  -->  | [2]=20           |
| [3]=30      <-top|       | [3]=30           |
+------------------+       | [4]=10      <-top|  <- 复制自 [1]，不是新计算出来的
                            +------------------+
```

**用途**：当你需要"再用一次某个参数"，又不想动原来的位置（比如后面要 `lua_insert`/`lua_remove` 打乱顺序），先 `lua_pushvalue` 复制一份到栈顶再操作，是标准手法。

### Stack Read：`lua_tointeger(L, 1)` 与 `lua_tointeger(L, -1)`

Read 系列只读，不改变 Stack 的大小和内容，`before` 和 `after` 完全一样：

```text
+------------------------+
| [1]=100                |  <- lua_tointeger(L, 1)  = 100（positive index，从栈底数第 1 个）
| [2]=200                |
| [3]=300          <-top |  <- lua_tointeger(L, -1) = 300（negative index，从栈顶数第 1 个）
+------------------------+
        (调用前后完全一致，Read 系列不修改 Stack)
```

正、负索引在这里刚好指向了 Stack 的两端；Chapter 03 会专门展开"什么时候该用正索引、什么时候该用负索引"。

### Stack Rearrangement：`lua_remove(L, 1)`

删掉指定位置的值，**上面所有值整体下移一格**填补空缺（Stack 会变小 1）：

```text
Before                          After
+------------------+       +------------------+
| [1]=10           |       | [1]=20           |
| [2]=20           |  -->  | [2]=30      <-top|
| [3]=30      <-top|       +------------------+
+------------------+
```

### Stack Rearrangement：`lua_insert(L, 1)`

把当前栈顶的值"拔出来"，插入到指定位置，**原来位置及以上的值整体上移一格**（Stack 大小不变）：

```text
Before                          After
+------------------+       +------------------+
| [1]=10           |       | [1]=30           |  <- 原栈顶被插到这里
| [2]=20           |  -->  | [2]=10           |
| [3]=30      <-top|       | [3]=20      <-top|
+------------------+       +------------------+
```

### Stack Rearrangement / Stack Write：`lua_replace(L, 1)`

把栈顶的值弹出，直接覆盖写入指定位置，**其余值位置不变，但 Stack 大小减 1**（因为栈顶那个值被"消耗"掉了）：

```text
Before                          After
+------------------+       +------------------+
| [1]=10           |       | [1]=30           |  <- 被覆盖
| [2]=20           |  -->  | [2]=20      <-top|
| [3]=30      <-top|       +------------------+
+------------------+
```

`lua_replace` 常被理解成 `lua_remove` 的反向操作，但注意区别：`lua_remove` 是"删掉一个位置，别人补上来"（Stack 变小，形状是"抽走一层"）；`lua_replace` 是"用栈顶的值覆盖一个位置"（Stack 也变小，但形状是"消耗栈顶去填一个坑"）。两者在这个例子里让 Stack 大小的变化结果相同，但过程和语义完全不同，不能混用。

## Data Flow

六个类别按"数据流向"可以重新归纳成三组：

```text
Inspection   -->  不跨界，只读 Stack 的元数据（个数），C 变量只是拿到一个 int
Read         -->  Stack --> C            (lua_to*，值离开 Stack 进入 C 原生变量)
Growth/Write -->  C --> Stack            (lua_push*，C 值或已有 Stack 值进入/复制到 Stack)
Rearrangement-->  Stack 内部搬运，不产生新值，也不离开 Stack
```

```text
        C 原生世界                    Lua Stack                          (不跨界)
    +----------------+          +------------------+                   +------------------+
    |  int / char* … | -push--> | 新的栈顶槽位       | <-remove/insert-> | 已有槽位重排       |
    |  a, b, result  | <-to---- | 已有槽位          |                   +------------------+
    +----------------+          +------------------+
                                        ^
                                        |
                                   lua_gettop
                                  （只读元数据，
                                   不参与数据流）
```

Rearrangement 类操作（`remove`/`insert`/`replace`）是本章最容易被忽视的一类：它们完全发生在 Stack 内部，**不触碰 C 原生世界，也不触碰 Lua VM 之外的任何东西**——之所以需要它们，是因为很多 C Function 在处理复杂逻辑时，Stack 上值的顺序会变得不再是"参数顺序"，需要重新整理成方便后续 `lua_push*`/`return N` 处理的形状。

## Lifecycle

本章目前接触到的所有 Stack 槽位都属于 Temporary value（临时值）：它们的生命周期完全绑定在**这一次 C 函数调用**内，函数一旦 `return`，Lua VM 就会清理掉这次调用在 Stack 上留下的所有痕迹，只保留 `return N` 指定的那几个值传给调用方。

有一条本教程反复强调、这里必须再次点名的规则：**Stack index 是本次调用内的临时坐标，不能当成能跨调用保存的对象引用。** 例如：如果你在一次调用里把 `lua_gettop(L)` 的结果存进一个 C 全局变量，指望"下次调用再用这个数字去读同一个值"，那是错的——下一次调用时 Stack 已经是全新的内容，同一个数字 index 现在指向的是别的东西（下面 Failure 部分的第 1 个例子就是这类错误的一个变体）。真正需要"跨调用长期持有一个 Lua Object"时，本教程会在 Chapter 09 引入 `luaL_ref(L, LUA_REGISTRYINDEX)`——那才是被设计出来对抗"Stack 生命周期太短"这个问题的机制。

## Failure

### 失败 1：`lua_settop(L, 0)` 清空了还没读的参数

```c
static int demo_broken_add(lua_State *L) {
    /* BUG: 想"重置"一下栈状态，结果把 Lua 传进来的参数一起清掉了 */
    lua_settop(L, 0);
    int a = (int)luaL_checkinteger(L, 1);
    int b = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b);
    return 1;
}
```

实际验证（`local r = demo_broken_add(10, 20)`）：

```text
lua error: ...: bad argument #1 to 'demo_broken_add' (number expected, got no value)
```

**错在哪**：进入 C Function 时，Stack 上摆的正是调用方传来的参数（`[1]=10`、`[2]=20`）。`lua_settop(L, 0)` 会把它们**连同参数一起**清空，之后再用 `luaL_checkinteger(L, 1)` 去读，读到的自然是"没有值"。这也是为什么本章示例代码里，所有 `demo_*` 函数在调用 `lua_settop(L, 0)` 之后，都是**自己重新 push 一批演示数据**，而不是先清空、再假装参数还在——真实业务代码里，`lua_settop(L, 0)` 这类"清空动作"必须放在读完所有需要的参数**之后**。

### 失败 2：`lua_pop` 弹出过多，Stack 进入未定义状态

```c
static int demo_overpop(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    printf("before top=%d\n", lua_gettop(L));
    lua_pop(L, 5);   /* 栈上只有 2 个值，却 pop 5 个 */
    printf("after top=%d\n", lua_gettop(L));
    return 0;
}
```

实际验证：

```text
before top=2
after top=-3
```

**错在哪**：Lua 官方手册明确写着"弹出比 Stack 上现有数量更多的值是未定义行为"。在我们的构建里，它没有立即崩溃，而是把 `top` 变成了负数——这本身就是一个已经损坏的 Stack 状态，任何后续对这个 Stack 的读写都可能读到垃圾内存或让程序崩溃，具体表现依赖 Lua 的构建方式（是否开启 `LUA_USE_APICHECK`）和后续代码路径，**不能依赖"这次没崩就是没事"**。永远确保 `lua_pop(L, n)` 里的 `n` 不超过 `lua_gettop(L)`。

### 失败 3：`lua_tointeger` 越界读取，静默返回 0 而不是报错

```c
static int demo_bad_read(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 42);
    /* 栈上只有 1 个值，却读 index 5 */
    lua_Integer v = lua_tointeger(L, 5);
    printf("lua_tointeger(L, 5) on 1-value stack = %lld\n", (long long)v);
    return 0;
}
```

实际验证：

```text
lua_tointeger(L, 5) on 1-value stack = 0
```

**错在哪**：`lua_tointeger` 属于 Read 系列（`lua_to*`），设计上就是"尽力转换，转换不了/取不到就返回 0"，它**不会**像 `luaL_checkinteger`（校验系列）那样在参数不存在时报错并中止脚本。如果把这个"总是能拿到一个数字"的行为，误当成"证明 index 5 上真的有值",就会把一个越界读取误判成"读到了 0 这个正常值"，制造出很隐蔽的逻辑 bug。**只要是"这个值理论上必须存在"的场景，用 `luaL_check*` 而不是 `lua_to*`**；只有"值可能不存在，不存在也有合理默认值"的场景才适合用 `lua_to*` 静默降级。

## Debugging

1. **第一反应永远是 `lua_gettop(L)`**：在可疑代码前后各打印一次 `lua_gettop(L)`，先确认"数量对不对"，再纠结"内容对不对"。
2. **怀疑越界就换 `luaL_check*`**：如果一个 `lua_to*` 读出来的值"看起来正常但结果不对"，先换成对应的 `luaL_check*` 跑一遍——如果它报错了，说明之前那个"正常值"其实是越界读取的静默默认值（通常是 0 / `NULL` / 空字符串）。
3. **`lua_settop`/`lua_pop` 之前，先确认要保留的值已经读完**：任何"清空 Stack"的调用，都要问一句"这次清空会不会把还没用的参数/中间结果一起清掉"。
4. **`lua_insert`/`lua_remove`/`lua_replace` 之后重新画一遍 Stack**：这三个函数改变的是"位置关系"，光凭想象很容易算错哪个值最终落在了哪个 index，画出来（哪怕只是在纸上或注释里）比在脑子里推演可靠得多。
5. **不要把某次调用算出来的 index 存到 C 的全局/静态变量里指望下次还能用**——参见本章 Lifecycle 一节。

## Mental Model

> Lua Stack 上所有的操作，最终都能归到六个动词：看多少（Inspection）、往上摞（Growth）、往下拿（Shrinking）、读出来（Read）、写进去（Write）、重新排（Rearrangement）——先分清一个 API 属于哪一类，再去记它的具体签名。
