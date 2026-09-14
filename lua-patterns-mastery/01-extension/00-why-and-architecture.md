# 01 · Extension Pattern —— WHY / Architecture / Lifecycle

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中定义的词汇：Host 拥有 Mechanism / Resource / Lifecycle / Stable API；Lua 拥有 Behavior / Policy / Rules / Configuration / Extension。本文只展开其中的 **Extension** 这一格。

## 1. WHY —— Extension 为什么存在

Host 程序在设计之初，会先固化一套"核心能力"：内存管理、并发模型、I/O、协议解析框架、渲染管线、事件循环……这些能力一旦定型，就应该尽量少改，因为改错的代价是崩溃、内存破坏、ABI 不兼容。

但真实世界里，用户几乎必然会提出 Host 设计者没有预料到的新能力需求：

- 一个命令行程序，用户想要一个全新的子命令；
- 一个协议分析工具，用户想要它认识一种它原本完全不知道的协议；
- 一个编辑器，用户想要一个全新的命令 + 快捷键组合。

这些需求的共同特征是："**Host 核心代码里原本不存在这个能力，需要凭空新增一个**"。如果每次新增能力都要求 Host 重新编译、重新发布，那么：

- 能力增长速度被开发者的发布节奏卡住；
- 用户自己完全没有能力扩展 Host；
- Host 核心代码库会被大量"一次性能力"污染，长期难以维护。

Extension Pattern 的解法：Host 保留一个**稳定的核心 API**，允许 Lua 脚本调用这个 API **注册一段新的行为**，Host 在合适的时机把控制权交给这段新行为。Host 核心代码本身完全不需要知道这段新行为具体做了什么。

核心模型：

```text
             Host
              |
       Stable Core API
              |
              v
            Lua
              |
       New Behavior
              |
              v
        Extended System
```

逐层解释：

- Host 提供的是"入口"，不是"实现"——`Stable Core API` 是一组签名不轻易变动的注册函数（例如 `register_command`），Host 承诺这些函数永远可以调用，但不承诺、也不关心调用者传进来的是什么行为。
- Lua 负责填充"具体是什么新行为"——`New Behavior` 完全是 Lua 侧的产物，可以在不重新编译 Host 的前提下随时增删。
- `Extended System` 表示：系统运行到这一步，能力边界已经比 Host 原生编译产物更大，而这个"更大"完全是运行时叠加上去的。

### Core capability vs Extension capability

| 维度 | Core capability（Host 核心能力） | Extension capability（Lua 新增能力） |
|---|---|---|
| 归属 | Host 用 C/C++ 实现，编译进二进制 | Lua 脚本实现，运行时加载 |
| 例子 | 内存分配、TCP 连接管理、Wireshark 的 Tvb/Pinfo 框架、Neovim 的 Buffer/Window 模型 | 一个具体的 `hello` 命令、一个能解析 MyProto 的 dissector、一个 `:HelloCount` 命令 |
| 能否被脚本改 | 不能。这是 Host 对外的稳定契约，改了会破坏所有依赖它的 Extension | 能。这正是 Extension 存在的意义——随时可增删、可替换 |
| 改错的代价 | 高：可能段错误、内存破坏、影响所有用户 | 低：`lua_pcall` 兜底，最多这一个新能力失效 |
| 发布方式 | 跟随 Host 二进制发布，需要重新编译 | 跟随脚本文件发布，替换文件即可生效 |

一句话区分：**Core capability 回答"Host 能做什么"，Extension capability 回答"Host 现在多了什么它原本不会做的事"**。

## 2. Architecture —— 通用架构

```text
Application
    |
    +-- Core
    |
    +-- API
    |
    +-- Lua Runtime
           |
           +-- extension.lua
```

- `Application`：整个进程，例如一个 C 命令行工具、Wireshark、Neovim。
- `Core`：Host 的核心机制层，负责真正的资源和主循环（对应 00 篇里的 Mechanism / Resource / Lifecycle）。
- `API`：Host 显式暴露给 Lua 的绑定层（对应 00 篇里的 Stable API），是 Core 和 Lua Runtime 之间唯一合法的桥梁。
- `Lua Runtime`：一个或多个 `lua_State`，由 Core 创建、销毁。
- `extension.lua`：运行在 Lua Runtime 里的脚本，通过 `API` 反向注册新行为。

### Host 提供什么？

一组具名、签名固定的注册函数或对象（例如 `register_command`、`Proto`/`ProtoField`/`DissectorTable`、`vim.api.nvim_create_user_command`）。这些函数是 Host 对 Lua 的"能力出口"：Lua 只能通过这些出口影响系统，出口之外的一切（Host 内部数据结构、私有状态、其它模块的内存）对 Lua 不可见。

### Lua 可以访问什么？

- Host 显式通过 API/Binding 暴露的函数、对象、常量；
- 标准 Lua 语言本身的能力（table、closure、标准库）；
- Lua 脚本自己创建的局部/模块级状态（用来实现 Extension 自身的行为和状态，例如命令的调用计数器）。

### Lua 不能访问什么？

- Host 内部未暴露的 C 数据结构和内存（例如 `main.c` 里的 `command_entry_t` 数组，Lua 完全看不到它的存在，只能通过 `register_command` 间接往里面追加一条记录）；
- Host 的私有资源句柄（文件描述符、socket、GPU 句柄），除非 Host 主动把某个操作包装成 API 暴露出来；
- Host 主循环的调度权——Lua 不能"抢"CPU，只能在 Host 主动调用 `lua_pcall` 的那一小段时间里执行。

### Extension 是主动注册，还是被 Host 调用？

**两步都有，顺序不能颠倒**：

1. **启动阶段：Extension 主动注册**——`extension.lua` 加载时立刻调用 `register_command(...)`、`DissectorTable:add(...)`、`vim.api.nvim_create_user_command(...)`，把自己的存在告知 Host。这一步的发起方是 Lua。
2. **运行阶段：Host 被动/按事件调用 Extension**——真正触发新行为执行的时刻（用户敲了这个命令、Wireshark 遇到了匹配的包、用户按了快捷键），发起方是 Host，Host 通过之前保存的引用把控制权临时交给 Lua。

一句话：**注册是 Lua 发起的单次动作，调用是 Host 发起的重复动作**。

### Extension 如何影响 Host？

Extension 从不直接修改 Host 的内存或控制流，它只能通过两种方式产生影响：

1. **往 Host 维护的表里追加一条记录**（命令表、DissectorTable、用户命令表），从而在未来某个时间点被 Host 查到并调用；
2. **在被调用期间，通过 Host 暴露的另一组 API 反过来操作 Host 状态**（例如往 Packet Details 树里添加节点、调用 `vim.notify` 弹出通知）。

Extension 的影响力边界，完全由 Host 在 `API` 这一层选择暴露了什么来决定。

## 3. Lifecycle

```text
Start
  |
  v
Create Lua VM
  |
  v
Load Script
  |
  v
Register Extension
  |
  v
Host Runs
  |
  v
Host invokes Lua
  |
  v
Lua extends behavior
  |
  v
Unload
```

- `Create Lua VM`：Host 调用 `luaL_newstate` / Wireshark 和 Neovim 内部等价的初始化动作，得到一个空的 `lua_State`。
- `Load Script`：把 `extension.lua`（或等价文件）编译并执行一次。这一次执行**只做注册**，不做真正的业务逻辑。
- `Register Extension`：脚本执行过程中调用 Host API，把"名字 -> 行为"的映射写进 Host 侧的某张表。
- `Host Runs`：Host 进入自己的主循环/主流程，此时 Extension 处于"待命"状态，尚未被调用。
- `Host invokes Lua`：某个触发条件成立（用户输入命令、匹配到某个包、按下快捷键），Host 查表拿到对应的 Lua 函数引用，`lua_pcall` 调用它。
- `Lua extends behavior`：Lua 函数体真正执行，完成"新增的那部分行为"。
- `Unload`：Host 退出或显式卸载时，销毁 `lua_State`（例如 `lua_close`），所有注册表引用、闭包随之释放。

## 4. 本质区别：Extension 与其余四种模式

- **Extension** = 新增能力：Host 原本不会做的事，因为 Lua 而存在了。
- **Policy** = 做选择：Host 已有多种能力/分支，Lua 只是决定"这一次该走哪一个分支"，不创造新能力。
- **Configuration** = 描述参数：Lua 返回一份数据/少量计算结果，供 Host 初始化时使用，不改变 Host 的行为逻辑本身。
- **Rule Engine** = 匹配规则到动作：输入满足某个条件时触发某个（Host 已知的）动作，重点在"匹配"而不是"新增"。
- **Plugin** = 独立生命周期的封装：把一组 Extension/Policy/Configuration 打包成一个可以独立加载/卸载、有自己生命周期钩子的单元，关注的是"整体管理"而不是"单点新增能力"。

后续三篇示例（C 命令行 / Wireshark Dissector / Neovim 扩展）都会在各自第 16 步回到这条判断标准，验证"为什么它是 Extension 而不是别的模式"。
