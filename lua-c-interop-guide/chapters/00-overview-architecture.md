# 00. 总体架构：Lua VM / lua_State / Lua Stack

> 对应用户需求 Stage 0-2。本章不含独立 `code/`，因为它是"地图"，具体"路"从 Chapter 01 开始每章都会给出可编译代码。

## 0.1 为什么 Lua 需要 C API？（Why）

Lua 被设计成一个**可嵌入的脚本语言**：它自己不直接管文件、网络、驱动、GUI，这些能力永远来自宿主程序（C/C++ 写的应用）。于是天然出现两个方向的需求：

1. **宿主想把自己的能力暴露给脚本**：比如 Wireshark 想让 `.lua` 插件能够解析协议字段——这些字段的底层数据来自 C 写的 dissector engine，脚本自己没有能力直接读内存。
2. **脚本想反过来驱动宿主**：比如一个网络库希望"连接建立后回调到 Lua 里的处理函数"。

Lua VM 和 C 程序是两个**完全不同的运行时**：一个解释字节码、管理自己的 GC 堆；另一个是普通编译后的机器码，用自己的调用约定和栈。它们之间没有共享的"调用约定"，Lua VM 也不可能理解任意一个 C 函数指针的参数类型、调用约定、返回值语义——这些信息在编译后的机器码里根本不存在。

**Lua C API 存在的意义**：定义一套双方都遵守的"协议"，让 Lua VM 和 C 代码可以在不了解对方内部实现的前提下，安全地传递参数、调用函数、传递返回值、共享对象、管理生命周期。

```text
Lua 世界                         C 世界
（字节码 + GC 堆 + 自己的栈）      （机器码 + malloc/free + C 调用栈）
        |                              |
        |        互相不理解对方内部结构   |
        |                              |
        +------------ Lua C API -------+
              （双方都遵守的协议）
```

## 0.2 总体架构（Architecture）

```text
Lua Source（.lua 脚本）
        |
        v
     Lua VM                 <- 执行字节码
        |
        | Lua C API         <- 唯一合法的跨界通道
        v
   lua_State *L             <- 一次解释器实例的完整上下文
        |
        v
    Lua Stack                <- 参数 / 临时值 / 返回值都放这里
        |
        +-- Arguments
        +-- Temporary Values
        +-- Lua Objects（Table / Function / Userdata 的"句柄"）
        +-- Return Values
        |
        v
    C Function               <- 用 Lua C API 读写 Stack 的普通 C 函数
        |
        v
   Native C World             <- malloc 出来的结构体、文件描述符、socket……
```

整个体系只有一条"国境线"：**Lua C API**。凡是数据要从 Lua 世界进入 C 世界，或者反过来，都必须经过 Stack 这道"海关"，不存在任何"直接把 Lua 变量地址传给 C 指针"的合法路径（Full Userdata 的原始内存除外，但拿到那块内存本身也要经过 Stack）。

## 0.3 Lua VM

**直觉**：Lua VM 是一台只认识"字节码指令"的虚拟 CPU，`print`、`+`、`function` 调用在它眼里都是几条固定的字节码指令（`CALL`、`ADD`、`GETTABUP` ……）。

**位置**：整个架构的最顶端，`.lua` 源码被 `luaL_loadstring`/`luaL_dofile` 编译成字节码后，由它逐条执行。

**Lua function call 在 VM 内部发生什么？**

```text
local r = add(10, 20)
```

编译后大致是：

```text
GETGLOBAL   R0, "add"     ; 把全局 add 放进 VM 自己的寄存器/栈槽
LOADK       R1, 10
LOADK       R2, 20
CALL        R0, 2, 2      ; 调用 R0，2 个参数，期望 1 个返回值
```

如果 `add` 是一个 **Lua Function**，VM 直接跳转执行它的字节码，全程在"VM 内部"完成，不涉及 Lua C API。

如果 `add` 是一个 **C Function**（通过 `lua_register`/`lua_pushcfunction` 注册），VM 在 `CALL` 指令里发现这个"函数对象"实际上是一个 `lua_CFunction` 指针，于是：

1. VM 把参数按顺序放进 **Lua Stack**（对 VM 来说，这本来就是它内部栈的一部分——重点：Lua Stack 不是一个额外发明的结构，而是 VM 执行栈本身的一段，Lua C API 只是给了 C 代码"合法访问这段栈"的能力）。
2. VM 暂停自己的字节码执行，改为跳转执行这个 C 函数指针（这才是真正跨到"C 调用栈"上）。
3. C 函数用 Lua C API 读 Stack、算结果、把结果 Push 回 Stack、`return N`。
4. VM 从 Stack 顶部取回 N 个值，当作这次 `CALL` 的返回值，继续执行后面的字节码。

**Lua VM 为什么不能直接理解任意 C function？**

因为 C 函数编译后只是一段机器码地址，VM 不知道：
- 它需要几个参数、每个参数是什么类型；
- 它的调用约定（参数怎么传、返回值怎么取）；
- 它内部会不会长时间阻塞、会不会崩溃。

Lua C API 通过强制约定"C 函数签名必须是 `int (*)(lua_State *L)`，参数和返回值都通过 Stack 交换"，把一个本来无法理解的任意函数，收敛成 VM 只需要认识**一种**签名的"外部指令"。这就是为什么本教程反复出现：

```c
static int l_xxx(lua_State *L) { ... }
```

——这是 Lua VM 能够调用的**唯一**一种 C 函数形态，术语叫 `lua_CFunction`。

**Failure**：如果不小心用普通 C 函数指针（签名不是 `int(*)(lua_State*)`）去注册，编译器通常会报类型不匹配的警告/错误；即使强行转换通过编译，运行时 VM 按 `lua_CFunction` 的方式去调用会读到错乱的参数，直接崩溃或产生垃圾结果。

**Debugging**：C 函数签名写错是最容易被编译器直接挡住的错误——出现 `-Wincompatible-pointer-types` 一定要认真看，不要强转掩盖。

**Mental Model**：`Lua VM 只认识一种"外部函数"：签名为 int(*)(lua_State*)、只通过 Stack 交换数据的 C Function。`

## 0.4 `lua_State *L` 到底代表什么？

不要停留在"lua_State 是 Lua 状态"这种同义反复的说法。从系统角度看：

```text
lua_State *L
    |
    +-- 当前 Lua 执行上下文（正在跑到哪一行字节码、调用栈帧信息）
    +-- Lua C API 的操作入口（几乎每个 API 函数第一个参数都是它）
    +-- Lua Stack 的访问入口（push/pop/get/set 全部对 L 操作）
    +-- Registry（LUA_REGISTRYINDEX 指向的私有 Table，C 用来长期存东西）
    +-- Global Table（`_G`）
    +-- GC 相关状态（堆、分配器、GC 阶段）
```

**直觉**：`L` 就是"这一个 Lua 解释器实例的遥控器"。你手里没有 `L`，就无法对这个解释器做任何事情——读不了它的 Stack，调不了它的函数，创建不了它的对象。

**生命周期**：

```text
luaL_newstate()   -- 创建一个全新的、空的 lua_State
      |
      v
luaL_openlibs(L)  -- 加载标准库（string/table/math/io ……），可选，但几乎总要做
      |
      v
   注册 C API      -- lua_register / luaL_setfuncs，把 C 能力挂到 Lua 全局/Table
      |
      v
  运行脚本 / 反复调用
      |
      v
   lua_close(L)    -- 触发所有对象的 __gc，释放整个解释器（含它管理的所有内存）
```

**Failure**：`lua_close` 之后继续使用 `L`（悬空句柄），或者跨线程共享同一个 `L` 却没有做任何同步，都是典型崩溃源。一个 `lua_State*` 默认只能被一个操作系统线程在同一时刻使用（多线程需要 Lua 自己的 coroutine 机制或额外加锁，不在本教程范围）。

**Mental Model**：`C API 基本上都围绕 lua_State *L 工作——没有 L，就没有和 Lua 世界的任何交互能力。`

## 0.5 Lua Stack

**直觉**：Lua Stack 是 **Lua 与 C 之间的数据交换工作台**——不是"C 语言调用栈"，也不是"某种通用容器"，它专门、且只用来在一次 Lua↔C 边界穿越时传递值。

```text
Lua
 |
 | add(10, 20)   参数
 v
Lua Stack
 |
 | Lua C API（luaL_checkinteger 等）
 v
C
```

```text
C
 |
 | lua_pushinteger(L, 30)
 v
Lua Stack
 |
 | return N   告诉 VM："栈顶 N 个值是返回值"
 v
Lua
```

**Lua Stack 与 C Call Stack、Lua VM 内部执行栈的关系（重点，容易混淆）**：

```text
+------------------+     +------------------+     +------------------+
|   C Call Stack    |     |  Lua Stack (L)    |     | Lua VM 执行帧栈    |
|  (操作系统/编译器)  |     |  (lua_State 内部) |     |  (VM 字节码层面)   |
+------------------+     +------------------+     +------------------+
| 每次 C 函数调用    |     | 每次跨 Lua/C 边界  |     | 每次 Lua Function  |
| 都会 push 一帧     |     | 都通过它交换数据    |     | 调用都会有一帧     |
| （由 CPU/ABI 管理）|     | （由 lua_State 管理）|    | （由 Lua VM 管理） |
+------------------+     +------------------+     +------------------+
```

三者是三个不同的"栈"，只是都叫"栈"：
- **C Call Stack**：操作系统/编译器管理，你写的每个 C 函数调用另一个 C 函数都会用到，跟 Lua 完全无关，普通 C 程序员天天在用但看不见。
- **Lua VM 执行帧栈**：Lua 内部管理 Lua Function 调用链的结构，纯 Lua 世界的事，本教程不深入。
- **Lua Stack（`L` 里的那个）**：本教程的主角，Lua C API 操作的对象，本质是 `L` 内部一段 `TValue` 数组，被 VM 执行帧和 C API 共享访问。

**为什么必须存在 Lua Stack？** 因为 C 变量（`int`、`char*`、`struct foo*`）和 Lua Value（可能是 Table、Closure、Userdata，且受 GC 管理）是两套完全不同的内存模型。C 不可能直接读懂一个 Lua Table 的内部布局（那是 Lua 实现细节，且版本间会变），Lua VM 也不可能直接理解一个 `struct foo*`。Stack 提供了一层"值语义的转换层"：每个 Stack 槽位是一个统一大小的 `TValue`（tag + union），无论它代表整数、字符串、Table 还是 Userdata，C 侧都通过统一的 `lua_to*`/`lua_push*`/`lua_is*` 函数来读写，不需要知道 Lua 内部结构。

**Mental Model**：`Lua Stack = Lua 与 C 之间的数据交换工作台，是唯一合法的跨界通道，且与"C 调用栈"是完全不同的两回事。`

## 0.6 本章与全书的关系

后续每一章都是在这张地图上"放大一个点"：

```text
Chapter 01   -- 走一遍最简单的 Lua -> C 调用（add）
Chapter 02-03 -- 把 Lua Stack 本身讲透（增删查改、正负索引）
Chapter 04-05 -- C -> Lua、以及基础类型双向转换
Chapter 06-08 -- Table 的双向传递与遍历
Chapter 09-10 -- Lua Function 被 C 长期持有（Registry）+ 事件回调
Chapter 11-14 -- Userdata + Metatable：把 C Object 包装成 Lua Object
Chapter 15-17 -- Closure/Upvalue、C 生成 Lua 可调用对象、双向调用
Chapter 18-20 -- 错误处理、Stack Balance、完整 API 分类地图
Chapter 21-25 -- 把整套机制串成生命周期 / 数据流 / 状态机 / 常见 Failure / Debugging Checklist
Chapter 26     -- 最终 5 个概念的收束
projects/      -- 8 个完整可运行案例，把以上全部知识点组合到真实场景里
```

**Debugging 提示（贯穿全书）**：遇到任何 Lua/C 交互的 bug，第一反应永远是问："此刻 Stack 上有什么？" 而不是"这个 API 我是不是用错了"。绝大多数问题一旦把 Stack 状态画出来，答案就会自己出现——这也是为什么每一章都会强制画 Before/After 的 Stack Diagram。
