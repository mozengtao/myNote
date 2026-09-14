# 01 · Table Module —— Namespace / API

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中定义的分析框架（第 4 节，10 步），只展开 **Table Module** 这一格：`Module -> Namespace -> {Function, Function, Constant, Type}`。

## 1. Pattern Definition

Table Module 是所有 Lua Module 组织方式里最基础的一种：一个 Lua 文件返回**一张 table**，这张 table 的字段是一组相关的函数（可能还有常量），调用者拿到这张 table 之后，把它当作一个"命名空间"来使用。

核心代码形状固定，后面所有例子都是它的变体：

```lua
local M = {}

function M.foo() end
function M.bar() end

return M
```

- **Module 返回什么？** 一张普通 table，字段是函数（`M.foo`、`M.bar`）和/或常量（`M.VERSION`）。
- **Framework/Caller 得到什么？** 一个可以用 `.` 访问字段的命名空间对象：`local api = require("api"); api.foo(); api.bar()`。调用者拿到的和 Module 内部持有的，是**同一张 table**，不是拷贝。
- **Module 边界在哪里？** 边界就是这张 table 暴露出来的字段集合。凡是挂在 `M` 上的字段都是"公开的"；没有挂在 `M` 上、只存在于文件内部的 `local` 变量/函数，才是真正不可见的部分（但 Table Module 通常不刻意区分这一层，这是下一个 Pattern 要解决的问题）。

## 2. Why

为什么 Lua 里"一组相关函数"要装进一张 table，而不是像 C 那样直接声明一组全局函数？

- **Lua 只有一种"打包"手段：table。** Lua 语言本身没有 `module`、`namespace`、`package` 这类专门的关键字或类型，唯一的复合数据结构就是 table。要把 `split`/`trim`/`join` 这几个函数"打包成一个东西"，唯一可用的容器就是 table。
- **`require()` 只接受一个返回值。** Lua 的 `require(name)` 语义是：加载并执行目标文件，把文件 `return` 的那个值当作模块的值缓存起来、返回给调用者。如果一个模块想暴露多个函数，要么每个函数单独 `require`（不现实），要么把它们全部装进一个值里一次性 `return`——table 是唯一能装下"多个具名函数"的值类型。
- **Table 天然提供命名空间，不污染全局环境。** 如果不用 table，退回到 C 早期写法——直接声明一堆全局函数 `function split() end`、`function trim() end`——会立刻遇到命名冲突：两个不同文件都想叫 `trim`，全局环境里只能存在一个。用 `M.trim` 代替全局 `trim`，本质上是用 table 的字段访问语法，手工实现了其他语言里 `namespace`/`package` 关键字才能提供的能力。
- **对应到 C header 的心智模型完全一致。** 一个 C header 里的

    ```c
    // foo.h
    void foo(void);
    void bar(void);
    #define VERSION 1
    ```

    做的事情是"声明一组相关的函数签名 + 常量，作为一个逻辑单元暴露给使用者"。Table Module 做的是同一件事，只是载体从"一份被 `#include` 的文本"变成了"一张被 `require` 返回的 table"：`M.foo`/`M.bar` 对应 `foo()`/`bar()` 的声明，`M.VERSION = 1` 对应 `#define VERSION 1`。区别在于 C header 只是编译期的文本粘贴，真正的实现在别的 `.c` 文件里；Lua 的 table 则是运行时唯一的载体，声明和实现（`function M.foo() ... end`）就长在一起。

一句话：**Table Module 之所以长这个样子，是 Lua 语言"只有一个返回值 + 只有 table 一种复合类型 + 没有专门的模块关键字"这三个语言事实共同决定的必然结果，不是某种编码习惯的偏好。**

## 3. Architecture

```text
Application
    |
    v
+-----------------------+
|        Module         |
|   (foo.lua / api.lua) |
|                       |
|   M.foo()             |
|   M.bar()             |
|   M.VERSION           |
+-----------------------+
```

只有两层：`Application`（调用方，可以是 Framework，也可以直接是最外层脚本）和 `Module`（一张 table）。没有独立的 `Implementation` 层——Table Module 的声明和实现是同一份代码，不像 Factory/Plugin 那样把"接口"和"具体实现"拆成两个文件。

### 与 C header 的对照

```text
C 版本                          Lua 版本
------------------------      ------------------------
// foo.h                       -- api.lua
void foo(void);                local M = {}
void bar(void);                function M.foo() end
#define VERSION 1              function M.bar() end
                               M.VERSION = 1
                               return M

// main.c                      -- app.lua
#include "foo.h"                local api = require("api")
foo();                          api.foo()
bar();                          api.bar()
```

两边的调用方都只依赖"这组名字存在、签名是什么"，不关心背后怎么实现。区别是：C 的 `foo.h` 只是给编译器看的文本契约，真正的符号是链接期才绑定到 `foo.c` 里的机器码；Lua 的 `api.lua` 本身既是契约也是实现，`require` 完成的是运行时的"查找 + 执行 + 缓存"，不存在单独的链接步骤。

## 4. Lifecycle

```text
load
  |
  v
require
  |
  v
initialize
  |
  v
use
  |
  v
state change
  |
  v
shutdown
```

逐条对应到 Table Module：

- **load**：Lua 解释器根据 `require("api")` 的模块名，在 `package.path` 里定位到 `api.lua` 文件，读入源码文本。
- **require**：编译并执行这份源码——`local M = {}` 创建一张新 table，`function M.foo() ... end` 依次把函数挂到这张 table 上，最后 `return M` 把这张 table 作为 `require` 的返回值。**这一整段执行只发生一次**：Lua 的 `require` 内部会把模块名和它的返回值记进 `package.loaded`，同一个进程里之后再 `require("api")`，直接返回缓存的同一张 table，不会重新执行 `api.lua`。
- **initialize**：对 Table Module 而言，"初始化"和"load/require"是同一件事——`M.foo`/`M.bar` 在文件被执行的那一刻就已经就绪，没有额外的"构造"步骤（这一点和 Class/Constructor Pattern 完全不同，那里 `require` 只拿到"造对象的方法"，真正的对象要调用者显式再构造一次）。
- **use**：调用者拿着 `require` 返回的 table，用 `api.foo()` 这种字段访问 + 调用的语法，反复调用其中的函数。
- **state change**：**Table Module 的理想情况下这一步为空**——见下面第 7 节 State。
- **shutdown**：Lua 没有显式的"卸载模块"操作；`api.lua` 对应的 table 会随着持有它的 `lua_State`（或整个进程）一起被 GC/销毁，没有专门的清理钩子。

### 再 `require()` 一次会重新加载吗？

不会。只要模块名相同、`package.loaded["api"]` 里已经有值，重复的 `require("api")` 直接返回同一张 table 的引用，**不会重新执行文件、不会得到一张新 table**。这意味着：如果两个不同的文件都 `require("api")`，它们拿到的是同一份 `M`，对其中任何字段的修改（例如误写 `api.foo = nil`）会被所有调用者共享看到。这正是第 7 节要强调"Table Module 应该保持无状态"的现实基础——`require` 的缓存语义决定了这张 table 是**进程级单例**，不是每次调用都重新生成的临时对象。

> `require()` 的完整运行时细节（`package.path` 查找规则、`package.loaded` 缓存机制、C 模块与 Lua 模块加载器的区别）属于横向整合文档 [06-require-and-state-mental-model.md](../06-require-and-state-mental-model.md) 的范围，本文只在"够用"的程度上提及，不展开。

## 5. Relationship

```text
Caller --> depends on --> Module
```

依赖方向是单向的：`Caller`（Application/Framework）知道 `Module` 的名字和它暴露的字段，主动 `require` 它、调用它；`Module` 完全不知道谁在调用它、调用了多少次、调用方是什么类型的程序。这是最简单的一种依赖关系——没有反向调用（不像 Extension Pattern 里 Host 会反过来调用 Lua），没有中间的抽象层（不像 Factory Pattern 里还隔着一层 Abstraction）。

## 6. Data / Control Flow

一次完整的调用，从 Application 发起到拿到结果：

```text
Application
    |
    | require("api")
    v
  require()
    |
    | 定位/执行 api.lua，得到 table M
    v
  Module (M)
    |
    | api.foo()
    v
   call
    |
    | M.foo 函数体执行
    v
  return
    |
    | 函数返回值沿调用栈原路传回
    v
Application
```

整个流程里，`Module` 从不主动发起任何动作——它永远处于"被调用"的被动位置，控制权自始至终握在 `Application` 手里。这也是判断"这是不是一个 Table Module"的实操标准之一：如果 Module 内部会反过来调用 Application 传进来的回调，或者需要 Application 先构造出一个"实例"再调用，那就已经不是纯粹的 Table Module 了。

## 7. State

**Table Module 的默认/理想状态是"无 State"。**

- 它提供的应该是纯函数集合（给定相同输入，永远返回相同输出，例如 `split("a,b", ",")`）或只读常量（`http.METHODS.GET`），不依赖、也不修改任何"上一次调用遗留下来的数据"。
- 结合第 4 节的结论——`require` 返回的是进程级单例 table——如果真的往 `M` 上挂了可变字段（例如 `M.call_count = 0`，每次调用递增），这个字段会被**所有**调用者共享。调用者 A 和调用者 B 完全不知道对方的存在，却在无意中互相影响对方看到的 `M.call_count` 值，这种"隐式共享可变状态"是许多难以复现的 bug 的根源。
- 所以"Table Module 无 State"不是一条语法限制（Lua 完全不阻止你往 `M` 上挂可变字段），而是一条**设计纪律**：只要这条纪律被打破，这个 Module 实际上已经在扮演下一个 Pattern（Private + Public，把可变状态"关"在 `local` 变量里，只通过函数暴露读写入口）的角色，只是形状上还留在 Table Module 阶段——第 8 节的 Anti-pattern 会具体展开这一步"越界"是怎么发生的。

## 8. Failure

Table Module 的失败模式很窄，因为它本身没有生命周期、没有隐藏状态，能出错的地方只剩下"函数调用"这一层：

- **调用不存在的字段**：`api.nonexistent_field()` 会先在 table 里查到 `nil`，然后对 `nil` 做函数调用，抛出 `attempt to call a nil value (field 'nonexistent_field')`。这类错误在 Lua 里**不会**提前于调用发生，因为 table 字段访问本身不会报错（`api.nonexistent_field` 只是安静地得到 `nil`），真正报错的是"对 `nil` 做调用"这一步。
- **参数类型不对**：例如把一个 table 传给期望字符串的 `split`，错误会发生在函数体内部第一次对参数做字符串操作的地方（例如 `string.find` 报 `bad argument`），而不是在调用的瞬间——Lua 是动态类型语言，Table Module 通常不会在入口处做严格的类型校验（这是"轻量 API"的代价，不是设计缺陷）。
- **没有专门的错误处理边界**：Table Module 就是普通的函数调用，Lua 的默认行为是"错误直接向上抛，中断当前调用链"，需要调用者自己决定要不要用 `pcall`/`xpcall` 包一层。这一点和 Extension Pattern（`lua_pcall` 是 Host 强制的安全网）不同——Table Module 本身没有义务提供这种边界，因为它假设调用者和它处在**同一个受信任的 Lua 代码库内部**，不是跨越 Host/Extension 那种信任边界。
- **谁发现、谁恢复？** 完全由调用方决定。Table Module 只是把错误"如实抛出"，不吞掉、不重试、不降级——这是它保持无状态、无副作用的自然延伸：Module 自己没有能力判断"这次失败该不该重试"，因为它根本不知道调用方的业务上下文。

## 9. Mental Model

> 一句话 Mental Model: Table Module 就是一张"被 `require` 找到的、装了一堆函数和常量的 table"，等价于 C 里的一份 header + 实现文件的合体。
>
> 核心问题: 一组相关的函数/常量，怎么打包成一个可以被整体引用、不污染全局环境的单元？
>
> 核心关系: `Caller -> depends on -> Module`，单向、无隐藏状态、无反向调用。

## 10. Design Trade-offs

**优点：**

- 心智负担最低：读到 `local M = {}` ... `return M` 就知道这是一个 Namespace，不需要理解 metatable、继承链、生命周期钩子。
- 天然避免全局命名污染：所有导出内容都挂在 `M` 下面，不会和其他模块的同名函数冲突。
- 和 `require()` 的缓存语义完美契合：无状态意味着"单例"这件事完全不会引发副作用，多处 `require` 同一个模块是绝对安全的。

**缺点：**

- 没有封装边界：`M` 上挂的所有字段，不管本意是不是"内部实现细节"，只要挂了就是公开的、可被外部读写的（第 11 节 Anti-pattern 具体展开）。
- 不适合需要"多个独立状态"的场景：如果同一份逻辑需要同时服务多个互不干扰的调用者（比如两个独立的数据库连接），Table Module 的单例本质会导致状态互相踩踏。
- 不适合需要隐藏实现步骤的场景：如果 Module 内部有多步骤的、调用顺序有讲究的私有逻辑，全部堆在一张公开 table 里会让调用者有能力（即使不是本意）绕过正确的调用顺序，直接调用本该是"内部步骤"的函数。

**什么时候不要用：**

- 一旦发现需要往 `M` 上挂"这次调用之间需要共享、且会被修改"的数据，就不要再用纯 Table Module——应该转向 Private + Public（把可变数据关进 `local` 变量）。
- 一旦发现同一份逻辑需要支持"多个独立实例，状态互不干扰"（例如同时连接多个数据库），就应该转向 Class/Constructor Pattern。

**Table Module vs Private + Public：**

两者返回的都是一张 table，从 `require()` 调用方的视角几乎看不出区别（拿到的都是 `local m = require("xxx")`）。真正的区别在于文件内部：Table Module 里"挂在 `M` 上的字段"就是全部的逻辑；Private + Public 里，文件内部还存在一批不挂在 `M` 上的 `local` 函数/变量（真正的实现细节），`M` 上只挂"经过筛选、有意暴露"的一小部分入口函数。换句话说，Private + Public 是在 Table Module 的基础上，多做了一次"哪些该公开、哪些该隐藏"的设计决策；而 Table Module 默认的立场是"没有需要隐藏的东西，所有函数本来就是给外部用的"。

## Anti-pattern

把可变状态、"伪私有"辅助函数和公开 API 全部混进同一张 table：

```lua
-- everything mixed together
M.state = {}
M.private_helper = function() end
M.internal_counter = 0
```

这样写具体会导致三个问题：

1. **调用者可以从外部随意篡改 `M.state`。** `M.state` 只是 `M` 的一个普通字段，`require` 拿到的又是同一张 table 的引用，所以任何一处代码只要写一行 `require("m").state = {}`（或者更隐蔽地 `require("m").state.foo = "corrupted"`），就能在完全不经过 Module 自己任何函数的情况下，直接改写它的内部数据。Module 自己写的所有函数都可能因为这次"外部偷改"而处理到不一致的数据，且找不到是谁改的——因为改动根本没有经过任何一个函数入口，无法加日志、加校验、加断言。
2. **没有封装边界，`private_helper` 名不副实。** 命名里的 `private_` 只是一种"君子协定"，Lua 语言层面完全不认识这个前缀。任何调用者都可以直接 `m.private_helper()`，一旦有人这么用了，这个"内部辅助函数"就悄悄变成了事实上的公开 API——将来想改它的参数签名或删掉它，就会破坏那些本不该依赖它的调用者，而 Module 作者可能完全不知道有人在依赖它。
3. **`M.internal_counter` 同时触发第 7 节讲的"隐式共享可变状态"问题。** 它既是可变的，又是全局单例共享的，还挂在公开 table 上可以被外部直接赋值清零——三个风险叠在一起，是最典型的"看起来是工具函数集合，实际上悄悄变成了一个设计粗糙的单例对象"的写法。

这正是第 10 节"什么时候不要用 Table Module"里提到的那条边界：一旦一个 Module 里同时出现"需要隐藏的实现细节"和"需要保护的可变状态"，就已经越过了 Table Module 该承担的职责，这种压力正是推动它演化成 **Pattern 2：Private + Public**（用 `local` 变量真正隐藏实现，只在 `M` 上暴露筛选过的入口函数）的直接原因——具体做法见 [02-private-public/00-why-and-architecture.md](../02-private-public/00-why-and-architecture.md)。
