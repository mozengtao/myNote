# 01. 第一次调用：`add(10, 20)`

> 对应用户需求"第二阶段：Lua -> C 最简单调用"。本章目标只有一个：把 Chapter 00 画的那张大地图，落实成一次可以实际编译运行的 Lua -> C 调用，并把其中每一步都讲清楚。

## 直觉

`add(10, 20)` 在 Lua 里看起来就是一次普通函数调用，但它背后跨越了两个完全不同的运行时——参数从 Lua VM 手里,经由 Lua Stack 这个"数据交换工作台"，被搬到 C 变量里；C 算完之后又把结果通过同一条通道送回去。**没有任何"直接传参数"的捷径，一切都要经过 Stack。**

## Architecture

这一章只是在 Chapter 00 那张总体架构图上，放大最中间那一段：

```text
Lua Source（.lua 脚本）
        |
        v
     Lua VM                 <- 执行到 CALL 字节码，发现 add 是 C Function
        |
        | Lua C API
        v
   lua_State *L
        |
        v
    Lua Stack                <- 本章的主角：参数怎么进来，结果怎么出去
        |
        v
    C Function                <- 本章要写的 l_add(lua_State *L)
        |
        v
    Native C World            <- 普通的 C int 变量 a, b
```

`add` 之所以能被 Lua VM 调用，是因为它满足唯一合法的 C Function 签名：

```c
int (*)(lua_State *L)
```

VM 在执行 `CALL` 字节码时，只要发现被调用对象是这种签名的 C Function（通过 `lua_register` 注册），就会：

1. 把调用参数（`10`、`20`）按顺序放进 Lua Stack；
2. 跳转执行 `l_add`，把 `L` 交给它；
3. 等 `l_add` 用 `return N;` 告诉它"栈顶 N 个值是返回值"；
4. 从 Stack 顶部取回这 N 个值，继续执行后面的字节码。

这一整套协议就是 Lua C API，本章只是第一次亲手走一遍。

## 完整代码

目录：`chapters/01-first-call-add/code/`

### `add_demo.c`

```c
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

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "add", l_add);
    if (luaL_dofile(L, "add_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
```

### `add_demo.lua`

```lua
local result = add(10, 20)
print("result =", result)
```

### `Makefile`

```makefile
GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common
TARGET := add_demo
SRCS := add_demo.c
include $(GUIDE_ROOT)/Makefile.rules
```

### 运行结果（已实际执行验证）

```text
$ make run
gcc -std=gnu99 -Wall -Wextra -g -O0 -I.../vendor/lua-5.4.7/include -c -o add_demo.o add_demo.c
gcc -std=gnu99 -Wall -Wextra -g -O0 -I.../vendor/lua-5.4.7/include -o add_demo add_demo.o -L.../vendor/lua-5.4.7/lib -llua -lm -ldl
./add_demo
result =	30
```

`30` 正是 `add(10, 20)` 的结果，说明参数从 Lua 流入 C、结果从 C 流回 Lua 的整条链路是通的。

## Stack Diagram

`l_add` 从进入到返回，一共经历三次 Stack 状态变化。**注意：前两次（参数入栈）不是我们自己调用 API 做的，而是 Lua VM 在执行 `CALL` 字节码时自动完成的**——这是初学者最容易忽略的一点：C Function 被调用时，Stack 上已经摆好了参数，不需要（也不能）自己重新 push 一遍。

### 状态 0：VM 调用 `add(10, 20)`，参数已经在 Stack 上

```text
+------------------------+
| Lua Stack              |
|                        |
|  [1] 10                |
|  [2] 20          <-top |
+------------------------+
```

这一步由 Lua VM 完成，`l_add` 函数体开始执行时看到的就是这个状态。

### 状态 1：`luaL_checkinteger(L, 1)` / `luaL_checkinteger(L, 2)` —— 只读，不改变 Stack

```text
Before                        After
+------------------------+    +------------------------+
| Lua Stack              |    | Lua Stack              |
|                        |    |                        |
|  [1] 10                | -> |  [1] 10                |
|  [2] 20          <-top |    |  [2] 20          <-top |
+------------------------+    +------------------------+

a = 10   （从 [1] 读出，C 变量，不影响 Stack）
b = 20   （从 [2] 读出，C 变量，不影响 Stack）
```

`luaL_checkinteger` 属于"校验系列"（`luaL_check*`），它只是把 Stack 上某个位置的值**读出来并转换成 C 值**，同时做类型校验；Stack 本身的内容和大小完全不变。

### 状态 2：`lua_pushinteger(L, a + b)` —— 把结果压回 Stack

```text
Before                        After
+------------------------+    +------------------------+
| Lua Stack              |    | Lua Stack              |
|                        |    |                        |
|  [1] 10                |    |  [1] 10                |
|  [2] 20          <-top |    |  [2] 20                |
+------------------------+    |  [3] 30          <-top |
                               +------------------------+
```

`lua_pushinteger` 属于"Push 系列"，把一个 C `int`（`a + b = 30`）转换成 Lua Value 并放到 Stack 新的栈顶。此时 Stack 上有 3 个值：两个原始参数 + 1 个新结果。

### 状态 3：`return 1;` —— 告诉 VM 从栈顶数 1 个值作为返回值

```text
+------------------------+
| Lua Stack              |
|                        |
|  [1] 10                |
|  [2] 20                |
|  [3] 30          <-top |  <- 从这里往下数 1 个，就是返回值
+------------------------+
        |
        | VM 取走 [3]=30 作为返回值，收尾清理 Stack
        v
+------------------------+
|          Lua           |
|   result = 30          |
+------------------------+
```

`l_add` 函数体本身**不需要**手动清理 `[1]` `[2]` 这两个参数——`return N;` 之后，VM 会负责把这次调用在 Stack 上留下的所有痕迹（含参数和多余的值）清理掉，调用方看到的只是"函数返回了 1 个值：30"。这个"C 函数对 Stack 的进入前/离开后约定"就是 Stack Contract（栈契约），本教程会在 Chapter 19 专门展开，这里先只需要知道：**你负责把返回值 push 好并且报对个数 N，剩下的清理工作交给 VM。**

## Data Flow

### 参数方向：Lua Value -> Stack -> C

```text
Lua                      Lua Stack                 C
+-----------+       +-------------------+     +-------------+
| add(10,20)| ----> | [1]=10  [2]=20    | --> | a=10  b=20  |
+-----------+       +-------------------+     +-------------+
   VM 把调用参数        Lua C API 的                luaL_checkinteger
   放进 Stack           "Stack Read"                把 Stack 值转换成
   （lua_CFunction      能力（luaL_check*）          C 原生变量
   调用约定的一部分）
```

### 返回值方向：C -> Stack -> Lua Value

```text
C                    Lua Stack                    Lua
+-------------+   +-------------------+     +----------------+
| a + b = 30  |-->| [3]=30      <-top |---->| result = 30    |
+-------------+   +-------------------+     +----------------+
  lua_pushinteger     Push 系列把 C值          VM 按 return N
  把 C 原生值转换       变成 Stack 上的          从栈顶取回 N 个值，
  成 Lua Value         一个 TValue 槽位          绑定给调用处的变量
```

两条流向合起来就是一次完整的 Lua -> C -> Lua 往返：**没有 Stack，这两条箭头都无法画出来。**

## Lifecycle

本章的 `add` 是一个无状态的纯函数调用，不持有任何跨调用的对象或资源，因此没有额外的生命周期问题需要讨论——参见 Chapter 00 关于 `lua_State` 生命周期（`luaL_newstate` -> `luaL_openlibs` -> 注册 -> 运行 -> `lua_close`）的通用说明即可。真正涉及"对象活多久"的话题，从 Chapter 09（Lua Function 被 C 长期持有）和 Chapter 11 起（Userdata）才会展开。

## Failure

### 失败 1：`return 0;` —— push 了值，却告诉 VM "0 个返回值"

```c
static int l_add_buggy(lua_State *L) {
    int a = (int)luaL_checkinteger(L, 1);
    int b = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, a + b);   /* 值确实压栈了 */
    return 0;                   /* 但告诉 VM：这次调用没有返回值 */
}
```

实际验证（把上面的函数注册后跑 `local r = add(10, 20); print(r, type(r))`）：

```text
r =	nil	nil
```

**错在哪**：`return N` 里的 `N` 才是"从栈顶数几个值当返回值"的唯一依据，`lua_pushinteger` 是否执行过、Stack 上是否真的有值，VM 并不关心。`N=0` 意味着 VM 认为这次调用没有任何返回值，Lua 侧拿到的自然是 `nil`，不会因为你"确实 push 过"就侥幸拿到 30。

### 失败 2：把 C 变量的值当成"返回值个数 N"（对 `return N` 语义的根本性误解）

```c
static int l_add_wrong(lua_State *L) {
    int a = (int)luaL_checkinteger(L, 1);
    int b = (int)luaL_checkinteger(L, 2);
    int result = a + b;
    lua_pushinteger(L, result);
    return result;   /* BUG: 以为这是"把 result 返回给 Lua" */
}
```

实际验证（`add(1, 2)`，此时 `result = 3`）：

```text
r =	1
```

**错在哪**：这是本章要强调的核心陷阱。`return result;` 里的 `result` 恰好是 `3`，于是 VM 读到的指令变成"从栈顶数 3 个值作为返回值"。而此刻 Stack 上确实有 3 个值：`[1]=1`（参数 a）、`[2]=2`（参数 b）、`[3]=3`（push 的结果）。VM 把这 3 个值原样交还给调用方，`local r = add(1, 2)` 用多重赋值规则只取第一个，于是 `r` 被赋成了参数 `a=1`，而不是期望的 `3`。**`return` 后面的数字永远是"个数"，不是"值"**——这也是为什么本教程反复强调 `return N;` 要读成"从栈顶数 N 个值"，而不是"把 N 返回给 Lua"。

### 失败 3：Lua 侧少传参数

```lua
local r = add(10)   -- 只传了一个参数
```

实际验证：

```text
lua error: [string "local r = add(10)..."]:1: bad argument #2 to 'add' (number expected, got no value)
```

**错在哪**：`luaL_checkinteger(L, 2)` 试图从 Stack 位置 `[2]` 读取一个整数，但调用时 Stack 上根本没有第二个参数（该位置是"越界"的，效果等价于 `nil`）。`luaL_check*` 系列在校验失败时会直接调用 `luaL_error`，触发一次 Lua Error 中止当前脚本执行（不会返回到 `l_add` 剩下的代码），这也是为什么校验永远应该放在函数最前面——一旦校验通过，后面的代码可以放心假设参数类型是对的。

## Debugging

遇到 `add` 结果不对时，按下面顺序检查，而不是瞎猜 API 用错了：

1. **确认 Stack 上参数的位置**：`l_add` 入口处 `[1]` `[2]` 是不是真的是你以为的 `a`、`b`？如果调用方传参顺序变了，`luaL_checkinteger(L, 1)` 拿到的就不是你以为的那个值。
2. **确认 push 了几次、push 了什么**：在 `return` 之前打印 `lua_gettop(L)`，看看此刻 Stack 上到底有几个值——这比猜测更可靠（`lua_gettop` 会在 Chapter 02 详细讲）。
3. **确认 `return N` 里的 `N` 是不是"字面意义上的个数"**：永远问自己"我想让 VM 从栈顶拿几个值？"，而不是"我想返回什么值"——想返回什么值应该用 `lua_push*` 系列先放到 Stack 上。
4. **看编译警告**：如果 `l_add` 的签名不是 `int (*)(lua_State *)`，`lua_register` 那一行通常会有类型不匹配的警告，不要忽略。

## Mental Model

> `add(10, 20)` 之所以能工作，是因为参数经 Lua C API 被放上 Lua Stack、C 用 `luaL_check*` 读出来计算、再用 `lua_push*` 把结果放回 Stack，最后用 `return N;`——一个"从栈顶数 N 个值"的指令——告诉 Lua VM 这次调用交回了什么，而不是像普通 C 函数那样直接 `return` 一个值。
