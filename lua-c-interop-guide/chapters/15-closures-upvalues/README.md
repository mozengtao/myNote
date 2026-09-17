# 15. C Closure / Upvalue：C Function 如何拥有“私有状态”

> 对应用户需求 第十六阶段。本章不引入新的 Lua↔C 方向，重点是让一个 C Function 在多次调用之间**记住东西**。

## 15.1 直觉

到目前为止本教程里的 C Function（比如 Chapter 01 的 `l_add`）都是**无状态**的：每次被调用，能看到的东西只有这次调用传进来的 Argument，调用结束后什么都不剩。

但很多真实场景需要"这个函数记得上次的事"——一个计数器、一个迭代器、一个绑定了某个文件句柄的 `read` 函数（标准库 `io` 就是这么做的）。Lua 侧的答案是**Closure（闭包）**：

```lua
local function create_counter()
    local n = 0
    return function()
        n = n + 1
        return n
    end
end
```

这里 `n` 是外层局部变量，内层匿名函数把它捕获成了自己的 **Upvalue（上值）**——函数和它的私有状态被打包在一起，随函数值到处走。

**C 侧有没有等价物？有，叫 C Closure。** `lua_pushcclosure` 就是"C Function + 打包 N 个私有值"的组合动作。本章要做的事：让一个 C Function 记住"数到几了"，而不用任何 C 全局变量或 `static`。

## 15.2 Architecture

```text
Lua 侧闭包                          C 侧闭包（C Closure）
--------------------------          --------------------------
function() ... end                  lua_pushcclosure(L, l_counter, 1)
     |                                    |
     | 捕获外层局部变量 n                  | 打包 push 到栈顶的 N 个值
     v                                    v
Upvalue 存放在 Lua 自己的               Upvalue 存放在 Lua VM 分配的
Closure 结构里（VM 管理，              C Closure 结构里（同样由 VM
纯 Lua 世界，C 看不到）                 分配和管理，但只有这个 C
                                        Function 自己能通过
                                        lua_upvalueindex(n) 访问）
     |                                    |
     +------------ 本质相同 --------------+
        "函数 + 只属于它自己的私有状态"
        区别只是：状态存在哪、谁能读写
```

关键 API：

```text
lua_pushcclosure(L, f, n)
    把栈顶的 n 个值“收进”一个新建的 C Function f 里，
    这 n 个值从 Stack 上消失，变成 f 专属的 Upvalue(1..n)，
    最终 Stack 顶部只留下这一个 Closure 本身。

lua_upvalueindex(n)
    在 f 被调用时，返回第 n 个 Upvalue 的伪索引（pseudo-index）。
    可以像普通 Stack index 一样传给 lua_to*/lua_push*/lua_replace，
    但它不是 Stack 上的槫位，是 Closure 自带的存储。
```

`lua_pushcfunction(L, f)`（Chapter 01 用的那个）本质上是 `lua_pushcclosure(L, f, 0)` 的简写——0 个 Upvalue 的退化情况。**这也解释了为什么"C Function"和"C Closure"不是两种互斥的东西，而是同一种对象的两种取值（Upvalue 个数是 0 还是 >0）。**

## 15.3 完整代码

C 侧实现（工厂函数 + 真正被调用的函数）：

```1:55:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/15-closures-upvalues/code/counter_demo.c
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * l_counter: 真正被调用的 C Function。
 *
 * 它没有任何 C 全局变量、没有 static 计数器，
 * 计数状态全部存放在它自己的 Upvalue(1) 里
 * （由 create_counter 通过 lua_pushcclosure 绑定进来）。
 */
static int l_counter(lua_State *L) {
    lua_Integer count = lua_tointeger(L, lua_upvalueindex(1));
    count++;

    /* 把新值写回 Upvalue(1)：先 push 新值，再 lua_replace 到 upvalue 的伪索引 */
    lua_pushinteger(L, count);
    lua_replace(L, lua_upvalueindex(1));

    /* 再 push 一份作为这次调用的返回值 */
    lua_pushinteger(L, count);
    return 1;
}

/*
 * create_counter: 工厂函数，每调用一次就生产一个全新的 C Closure。
 *
 *   1. lua_pushinteger(L, 0)        —— 把初始计数值 0 push 到 Stack
 *   2. lua_pushcclosure(L, l_counter, 1)
 *      —— 把栈顶的 1 个值“打包”进新建的 C Closure，
 *         作为它的 Upvalue(1)；打包完成后原来的栈顶值被弹出，
 *         换成这个新 Closure 本身。
 */
static int l_create_counter(lua_State *L) {
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, l_counter, 1);
    return 1;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "create_counter", l_create_counter);

    if (luaL_dofile(L, "counter_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
```

Lua 侧脚本（同时用 Lua 自己的闭包做概念对照）：

```1:26:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/15-closures-upvalues/code/counter_demo.lua
-- 每次调用 create_counter() 都会在 C 侧生产一个全新的 C Closure，
-- 各自拥有独立的 Upvalue(1) 状态，互不干扰。

local counter = create_counter()

print(counter())
print(counter())
print(counter())

-- 对照：Lua 自己写一个等价的闭包，捕获的是外层局部变量 n
local function create_lua_counter()
    local n = 0
    return function()
        n = n + 1
        return n
    end
end

local lua_counter = create_lua_counter()
print(lua_counter())
print(lua_counter())

-- 证明每个 counter 都有自己独立的状态，不共享
local counter2 = create_counter()
print("counter2:", counter2())
print("counter (unaffected):", counter())
```

Makefile 沿用全书统一结构：

```1:6:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/15-closures-upvalues/code/Makefile
GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common

TARGET := counter_demo
SRCS := counter_demo.c

include $(GUIDE_ROOT)/Makefile.rules
```

`make run` 实际输出：

```text
$ make run
./counter_demo
1
2
3
1
2
counter2:	1
counter (unaffected):	4
```

对照解读：
- `counter()` 连续三次调用打印 `1 2 3`，证明状态确实被保留在 Upvalue 里，不是每次调用重新初始化。
- `counter2 = create_counter()` 是**另一个** C Closure，从 0 开始重新计数，打印 `1`；而这之后再调用第一个 `counter()`，打印 `4`（在 3 的基础上继续），证明两个 Closure 的 Upvalue 是各自独立存储的，互不影响。

## 15.4 Stack Diagram

### `create_counter()` 内部：打包 Upvalue

```text
调用 l_create_counter 时，Stack 上没有 Argument（create_counter() 不传参）

Step 1: lua_pushinteger(L, 0)
+-----+
|  0  |  <- 栈顶（这个 0 就是即将成为 Upvalue(1) 的值）
+-----+

Step 2: lua_pushcclosure(L, l_counter, 1)
   把栈顶的 1 个值“收进”新建的 Closure，Stack 上的 0 消失
+-----------------+
|  <l_counter      |  <- 栈顶（一个 C Closure 对象，
|   Closure，       |      内部私有存着 Upvalue(1) = 0，
|   Upvalue(1)=0>  |      C 代码之后再也看不到栈上那个 0 了）
+-----------------+

return 1;   -- 告诉 VM：栈顶 1 个值（这个 Closure）就是返回值
```

### `counter()` 每次调用：读写 Upvalue，Stack 上完全干净

```text
调用 l_counter 时（假设这是第 1 次调用，Upvalue(1) 当前 = 0）：

Stack（这次调用的参数区，counter() 不传参，是空的）
+-----+
|(空) |
+-----+

Upvalue 区（不在 Stack 上，是 Closure 自己的存储，
            但可以用伪索引 lua_upvalueindex(1) 当 Stack index 一样操作）
+---------------------+
| Upvalue(1) = 0       |
+---------------------+

Step 1: count = lua_tointeger(L, lua_upvalueindex(1))   -> count = 0，count++ -> 1

Step 2: lua_pushinteger(L, 1)
+-----+
|  1  |  <- Stack 栈顶
+-----+

Step 3: lua_replace(L, lua_upvalueindex(1))
   把栈顶的 1 弹出，写进 Upvalue(1)，Stack 恢复为空
+---------------------+
| Upvalue(1) = 1       |  <- 更新后
+---------------------+
+-----+
|(空) |  <- Stack
+-----+

Step 4: lua_pushinteger(L, 1)   -- 再 push 一份，作为这次调用的返回值
+-----+
|  1  |  <- 栈顶
+-----+

return 1;   -- 栈顶 1 个值（1）是返回值
```

**关键点**：整个过程 Stack 是干净的、临时的（进入时空、离开时只留 1 个返回值），真正跨调用存活的是 Upvalue，它不随每次调用清空。

## 15.5 Data Flow

```text
create_counter() 调用一次
       |
       v
  lua_pushinteger(0) -> Stack
       |
       v
  lua_pushcclosure(l_counter, 1) 把 Stack 上的 0 打包成 Upvalue(1)
       |
       v
  返回一个 C Closure 值（counter 局部变量）
       |
       | counter() 调用 1
       v
  l_counter 读 Upvalue(1)=0 -> 计算 1 -> 写回 Upvalue(1)=1 -> push 1 返回
       |
       | counter() 调用 2
       v
  l_counter 读 Upvalue(1)=1 -> 计算 2 -> 写回 Upvalue(1)=2 -> push 2 返回
       |
       | counter() 调用 3
       v
  l_counter 读 Upvalue(1)=2 -> 计算 3 -> 写回 Upvalue(1)=3 -> push 3 返回
```

Upvalue 的值只在“这个 Closure 存在期间”累积，数据从不经过 Native C World 的任何全局变量——它始终待在 Lua VM 分配、只有这一个 Closure 能访问的私有存储里。

## 15.6 Lifecycle

```text
lua_pushcclosure(L, l_counter, 1)
       |  VM 分配一个新的 Closure 对象（含 1 个 Upvalue 槽位），
       |  把当前栈顶的值搬进这个槫位，Closure 对象受 GC 管理
       v
counter 变量持有这个 Closure（Lua Object，普通局部变量）
       |
       |  只要 counter 在作用域内可达，GC 不会回收这个 Closure，
       |  Upvalue(1) 的值随 Closure 一起存活
       v
每次 counter() 调用
       |  Stack 打开一个新的调用帧（Argument 区），
       |  但 Upvalue 区不属于这次调用帧，横跨所有调用持续存在
       v
counter 不再被引用（比如脚本结束、局部变量离开作用域）
       |
       v
GC 回收这个 Closure 对象，连带 Upvalue(1) 的存储一起释放
```

对照 Lua 侧闭包 `create_lua_counter`：`n` 存在 Lua 自己的 Upvalue 结构里，生命周期规则完全一样——都是"只要持有这个函数值，它的私有状态就存活；函数值被回收，状态才消失"。两者唯一的区别是：Lua Upvalue 由 Lua VM 直接管理并对 Lua 字节码可见；C Upvalue 同样由 Lua VM 分配和管理，但**只有这个 C Function 自己**能通过 `lua_upvalueindex` 访问，Lua 脚本或其他 C 函数都看不到、碰不到它。

## 15.7 Failure

**1. 忘记 `lua_pushcclosure` 的 `n` 与实际 push 的值数量不一致**

```c
lua_pushinteger(L, 0);
lua_pushcclosure(L, l_counter, 2);   /* 错！栈顶只 push 了 1 个值，却声称要打包 2 个 */
```

`lua_pushcclosure` 会尝试从栈顶取 `n` 个值，如果 Stack 上实际值不够，会读到不属于这次调用的、之前遗留在 Stack 上的垃圾值当作 Upvalue(1)，产生类型错乱或直接崩溃。**必须让 push 的值数量与 `n` 严格一致。**

**2. 用 `lua_tointeger`/`lua_pushvalue` 等函数操作 `lua_upvalueindex(n)`，但 `n` 超出这个 Closure 实际拥有的 Upvalue 个数**

Manual 里说这是"acceptable but invalid index"——不会立刻崩，但读到的是无效值（通常是 `nil`），后续当整数用会得到 0 或错误结果。这是一类"不报错但结果全错"的隐蔽 bug，尤其在复制粘贴别的 Closure 代码、改了 `lua_pushcclosure` 的 `n` 却忘了同步改 `l_counter` 里的 `lua_upvalueindex` 编号时最容易发生。

**3. 试图跨 Closure 共享 Upvalue，误以为是"全局状态"**

```lua
local counter = create_counter()
local counter2 = create_counter()
```

`counter` 和 `counter2` 是两次独立的 `lua_pushcclosure` 调用生产出来的**两个不同的 Closure 对象**，各自拥有独立的 Upvalue(1) 存储。如果需要多个 Closure 共享同一份状态（比如共享一个连接池），Upvalue 这个机制本身做不到——那需要把共享状态包装成 Userdata（Chapter 11-14），由多个 Closure 各自持有指向同一块内存的指针。

## 15.8 Debugging

- 看到 C Closure 行为“状态没有被保留”：先确认 `lua_pushcclosure` 的 `n` 和实际 push 的值数量是否一致；再确认 `l_counter` 里的 `lua_replace(L, lua_upvalueindex(1))` 有没有漏写——只读不写，Upvalue 永远不会变化。
- 看到 Upvalue 读出来的值“莫名其妙”：数一遍 `lua_pushcclosure` 之前到底 push 了几个值，和 `lua_upvalueindex` 里用的编号是不是从 1 开始严格对应、次序一致（push 的第 1 个值对应 Upvalue(1)，第 2 个对应 Upvalue(2)……）。
- 怀疑多个"看起来同名"的 Closure 共享了状态：记住每次 `lua_pushcclosure` 调用都会生产一个**全新**的 Closure 对象，除非你只调用了一次 `create_counter` 然后把返回值赋给了多个变量（那才是真正共享同一个 Closure）。

## 15.9 Mental Model

```text
C Function + Upvalue = C Closure

    lua_pushcfunction(L, f)              == lua_pushcclosure(L, f, 0)
                                             （0 个 Upvalue 的特例）

    lua_pushcclosure(L, f, n)            打包栈顶 n 个值成为 f 专属的私有状态

    lua_upvalueindex(n)                  f 被调用时，访问第 n 个私有值的钥匙

Lua 侧 function() ... end 捕获外层局部变量
C 侧 lua_pushcclosure 捕获 push 上去的 n 个值
    —— 存储位置不同，本质相同：都是“函数 + 只属于它自己的私有状态”
```
