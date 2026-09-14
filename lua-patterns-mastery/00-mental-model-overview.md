# 00 · 统一 Mental Model — 为什么是 Lua，Lua 在哪里

> 本文件是整套笔记的地基。后面 5 个 Pattern（`01-extension` ~ `05-plugin`）以及横向对比文档（`06` ~ `10`）里反复出现的词汇——Mechanism / Resource / Lifecycle / Stable API / Behavior / Policy / Rules / Configuration / Extension——都在这里统一定义一次，后文不再重复解释，只引用。

## 学习目标

系统性建立一个可以迁移到任意 C/C++ 系统的心智模型，用来回答：

- 为什么这个系统需要 Lua？
- Lua 位于系统架构的什么位置？
- Host 与 Lua 分别负责什么？
- 数据如何从 Host 流向 Lua，Lua 又如何反过来影响 Host？
- Lua Script 的生命周期是什么？
- Extension / Policy / Configuration / Rule Engine / Plugin 这五种模式，本质区别在哪里？

不是记住 Lua 的 API，而是建立一个**分类框架**：拿到一个陌生的、嵌入了 Lua 的 C/C++ 项目，能立刻判断它属于哪种模式，Host/Lua 的边界画在哪里。

## 学习顺序（所有后续文档统一遵守）

```text
WHY
 |
 v
ARCHITECTURE
 |
 v
LIFECYCLE
 |
 v
RELATIONSHIP
 |
 v
DATA / CONTROL FLOW
 |
 v
STATE
 |
 v
FAILURE
 |
 v
EXAMPLES
 |
 v
COMPARISON
```

不按 Lua 官方 API 章节罗列，而是先回答"为什么"，再落到"怎么做"，最后才是"具体代码"。

## 1. 为什么 Lua 特别适合做 Host 的动态扩展层

一个 C/C++ Host 程序在运行很久之后，通常会分裂出两类需求，速度完全不同：

```text
+------------------------------------------------------------------+
| Host 内部两类需求，变化速度差 10x-100x                              |
+------------------------------------------------------------------+
| 1. 稳定需求：内存管理、并发模型、I/O、协议解析框架、渲染管线            |
|    -> 一旦定型，几年不改；改错代价极高（崩溃/内存破坏/ABI 兼容问题）    |
|                                                                    |
| 2. 易变需求：具体业务规则、用户脚本、协议插件、访问策略、配置参数        |
|    -> 每天/每周都可能改；改错代价低（顶多这一条规则失效）               |
+------------------------------------------------------------------+
```

如果第 2 类需求也用 C/C++ 实现，会得到：

- 每次改一条规则都要重新编译、重新链接、重新发布整个二进制；
- 出错时是段错误/内存破坏，而不是一条可控的错误信息；
- 无法在运行时让最终用户/运营人员自己修改行为，只能靠开发者改代码。

Lua 恰好被设计成"可以被宿主语言完全托管"的语言，具备一组少见的组合特性：

- **C API 优先设计**：Lua 从第一天起就是"为了被 C 嵌入"而设计的语言（不是先做成独立语言再补嵌入接口）；
- **单一 `lua_State`**：整个虚拟机、全局表、调用栈都封装在一个 `struct lua_State *`，创建/销毁/隔离都是一个指针的生命周期，天然适合"按需创建、按需销毁、可以有多个互不干扰的实例";
- **栈式调用协议**：C 与 Lua 之间只通过一个显式的值栈交换数据，没有隐藏的共享可变状态，边界非常清晰（详见 [06-c-lua-embedding-mental-model.md](06-c-lua-embedding-mental-model.md)）；
- **错误是可捕获的值，不是进程崩溃**：`lua_pcall` 把 Lua 侧的任何错误（语法错误、运行时错误、显式 `error()`）都转换成一次函数返回值，Host 进程不会因为脚本写错而死掉；
- **体积小、无外部依赖、可裁剪**：可以塞进任何 C 项目而不引入额外的运行时负担。

所以 Lua 不是"又一门脚本语言"，而是**专门用来吸收 Host 里变化最快、允许出错、需要运行时可改的那部分逻辑**的组件。

## 2. 统一抽象图

```text
                    Host Application
                           |
             +-------------+-------------+
             |                           |
       Stable Mechanism            Variable Behavior
             |                           |
          C / C++                    Lua
             |                           |
             +-------------+-------------+
                           |
                     Lua Binding/API
                           |
                           v
                      Lua Script
```

逐条解释图里的每一个节点：

- **Host 为什么通常用 C/C++**：Host 要直接管理内存、线程、文件描述符、网络连接、硬件资源，这些是"机制层"（Mechanism），需要确定性的性能和生命周期控制，C/C++ 是目前在这一层的默认选择。
- **Lua 为什么负责变化更快的逻辑**：见上一节的对比表；本质是"把易变、允许犯错、需要运行时改的逻辑，隔离到一个可以被沙盒化、可以被 `pcall` 兜底的解释器里"。
- **Host API 是什么**：Host 主动暴露给脚本作者的一组"稳定契约"函数/对象，例如 `register_command()`、`vim.opt`、`Proto`。它是 Host 对 Lua 单向承诺"这些能力永远存在，签名不会随便变"的边界。
- **Binding 是什么**：把 Host API 用 Lua C API（`lua_pushcfunction`、`luaL_newlib` 等）包装、注册进某个 `lua_State` 的那一层胶水代码。Binding 属于 Host 侧代码（通常也是 C/C++），不属于 Lua 脚本，也不属于 Host 的核心业务逻辑——它是两边的"翻译官"。
- **Lua VM 在哪里**：永远活在 Host 进程的地址空间里，以一个或多个 `lua_State *` 的形式存在，由 Host 决定何时创建、何时销毁、创建几个。Lua VM 不是独立进程，没有自己的调度权，只有被 Host 调用 `lua_pcall` 时才会执行。
- **Host 如何调用 Lua**：`lua_getglobal` 取出 Lua 侧的函数/表 -> 用 `lua_push*` 把参数压栈 -> `lua_pcall` 触发执行 -> 用 `lua_to*`/`lua_get*` 取回返回值。这是"Host 主动发起"的调用链。
- **Lua 如何调用 Host**：Lua 脚本调用的其实是 Binding 层注册进 `lua_State` 的 C 函数（`lua_CFunction`）。从 Lua 的视角，这只是一次普通函数调用；从 C 的视角，这是虚拟机把控制权临时交还给 C 代码。这是"Lua 主动发起，但只能发起 Host 允许它发起的调用"。
- **数据如何跨越 C/C++ 和 Lua 边界**：只通过 Lua 的值栈（number / string / boolean / table / userdata / function ...）。C struct 不能直接塞进 Lua，必须由 Binding 层显式转换（拆字段、逐个 `lua_push*`，或者包成 `userdata` + `metatable`）。详见 [06-c-lua-embedding-mental-model.md](06-c-lua-embedding-mental-model.md)。
- **谁拥有生命周期**：`lua_State` 由 Host 创建（`lua_newstate`/`luaL_newstate`）、由 Host 销毁（`lua_close`）。Lua 脚本内部的 table/闭包等对象的生命周期由 Lua 自己的 GC 管理，但整个 GC 所依附的 VM 本身的生死由 Host 决定。
- **谁拥有资源**：真正的资源（socket、文件描述符、共享内存、GPU 句柄）永远归 Host 所有；Lua 侧最多持有一个"句柄的代理"（通常是 `userdata` 或一个整数 ID），资源的分配和释放动作仍必须回调到 C 侧完成。
- **谁拥有控制权**：默认情况下 Host 拥有主循环和控制权，Lua 只在被 `lua_pcall` 调用的那段时间里临时获得 CPU；Lua 脚本不能主动抢占 Host（除非 Host 显式设计了协程/回调注册机制，让 Lua 能把自己注册进事件循环）。

### 收敛成一张 Host / Lua 职责表

```text
Host
 |
 +-- Mechanism      内存/线程/IO/网络/渲染/协议解析框架
 +-- Resource       文件描述符、socket、共享内存、硬件句柄
 +-- Lifecycle      进程/连接/请求/插件的创建与销毁时机
 +-- Stable API     Binding 暴露给 Lua 的、承诺不轻易变动的函数集合
          |
          v
         Lua
          |
 +-- Behavior       "发生某个事件时该做什么"的具体逻辑
 +-- Policy         "在已有能力之间该怎么选"的决策
 +-- Rules          "输入满足什么条件，触发什么动作"的匹配表
 +-- Configuration  "运行时参数应该是什么"的声明+少量计算
 +-- Extension      "在核心能力之外新增的行为"
```

## 3. 五种模式的本质：不是 5 个技术，而是 5 种分工方式

Extension / Policy / Configuration / Rule Engine / Plugin **用的是同一套机制**（同一个 `lua_State`、同一套 `lua_pcall`/回调机制），区别只在于：

1. Lua 代码在 Host 架构图里挂在**哪一层**（新增能力的入口？决策点？启动时的参数源？请求处理管线里的一个判断节点？一个有独立生命周期的容器？）；
2. Host 调用 Lua 的**触发时机**不同（用户输入一个命令？每次请求到达？只在启动时一次？每次有新数据到达？加载/卸载时）；
3 . Lua 返回给 Host 的**结果类型**不同（新的行为本身？一个决策值？一份配置表？一个动作列表？一组生命周期回调）。

后续每个 Pattern 文档的 `00-why-and-architecture.md` 都会先用一张对比图把该 Pattern 和其余 4 个区分开，避免读完 5 个 Pattern 之后仍然觉得"它们看起来都差不多"。

## 4. 导航

| 文档 | 内容 |
|---|---|
| [01-extension/](01-extension/00-why-and-architecture.md) | Pattern A：给 Host 增加新能力 |
| [02-policy/](02-policy/00-why-and-architecture.md) | Pattern B：在已有能力之间做决策 |
| [03-configuration/](03-configuration/00-why-and-architecture.md) | Pattern C：让配置具备计算能力 |
| [04-rule-engine/](04-rule-engine/00-why-and-architecture.md) | Pattern D：输入 -> 规则匹配 -> 动作 |
| [05-plugin/](05-plugin/00-why-and-architecture.md) | Pattern E：独立生命周期的可插拔模块 |
| [06-c-lua-embedding-mental-model.md](06-c-lua-embedding-mental-model.md) | `lua_State` / 栈 / C 调 Lua / Lua 调 C / callback / 错误边界 |
| [07-cross-pattern-comparison.md](07-cross-pattern-comparison.md) | 5 种模式横向对比表 + 组合关系 |
| [08-unified-case-network-server.md](08-unified-case-network-server.md) | 同一个 Network Server 演出 5 种角色 |
| [09-pattern-decision-tree.md](09-pattern-decision-tree.md) | Pattern 选择决策树 |
| [10-final-mental-model.md](10-final-mental-model.md) | 最终统一心智模型 + 五句话记忆 |

> 阅读建议：按 00 -> 01 -> 02 -> 03 -> 04 -> 05 -> 06 -> 07 -> 08 -> 09 -> 10 顺序读一遍；之后可以只把这份 00 和 07/09/10 当速查表，遇到具体系统时再回查对应 Pattern 的示例。
