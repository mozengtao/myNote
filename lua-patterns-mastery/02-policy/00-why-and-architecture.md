# 00 · Pattern B：Policy —— WHY & Architecture

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中定义的词汇：Host 拥有 Mechanism / Resource / Lifecycle / Stable API；Lua 拥有 Behavior / Policy / Rules / Configuration / Extension。本文只展开其中的 **Policy**。

## WHY：Policy 回答的是哪个问题

Pattern A（Extension）里 Lua 负责"新增一种 Host 原来没有的能力"。Policy 完全不同：Host 已经把"怎么做"这件事想清楚了，代码也写好了，Host 唯一缺的是"应该怎么选"。

用一句话区分：

- **Mechanism（机制）** 回答："这件事该怎么做？" —— 由 C/C++ 实现，稳定、性能敏感、一旦写错代价高（崩溃、内存破坏）。
- **Policy（决策）** 回答："在已有的几种做法里，这一次该选哪一种？" —— 由 Lua 实现，易变、允许出错、需要在不重新编译的情况下随时调整。

三者的关系是一条单向链：

```text
Mechanism
    |
    v
Policy
    |
    v
Decision
```

`Mechanism` 提供"能做的事情的集合"（例如：执行一次请求、拒绝一次请求，这两个动作 Host 都已经实现好了）；`Policy` 是一段决策逻辑，输入当前场景的事实，输出"选哪一个 Mechanism"；`Decision` 是这次决策的具体结果（一个布尔值、一个枚举字符串），交还给 Host 去驱动对应的 Mechanism。

一个具体的例子——Network Server：

```text
accept connection
    |
    v
parse request
    |
    v
[ Lua Policy: 这次请求该放行吗？ ]
    |
    +---- ALLOW ----> execute request
    |
    +---- DENY  ----> reject request
```

`accept connection`、`parse request`、`execute request`、`reject request` 全部是 C 侧已经实现好的 Mechanism——accept 怎么做、socket 怎么读、请求怎么真正被处理，Lua 从头到尾都不参与。Lua 唯一插入的位置，是中间那个二选一的分支：ALLOW 还是 DENY。Lua 消失，Host 依然知道怎么 accept、怎么 parse、怎么 execute——只是不知道"这一次该不该 execute"。

## Architecture：Policy 在 Host 架构图里的位置

```text
                Host
                 |
        +--------+--------+
        |                 |
    Mechanism           Policy API
        |                 |
        +--------+--------+
                 v
                Lua
                 |
             Decision
                 |
                 v
              Host Action
```

逐层解释：

- **Host**：同时拥有 Mechanism（真正执行动作的代码：真正放行一个请求、真正丢弃一个包、真正触发攻击动画）和一个专门为 Policy 开的 Policy API（一个 Host 主动调用的 Lua 函数入口，例如 `allow_request(user, resource)`）。这两者都属于 Host 一侧，Mechanism 是"能力"，Policy API 是"决策入口的调用协议"。
- **Lua**：只在被 Host 调用 Policy API 的那一刻短暂运行，读取传入的场景事实，跑一段决策逻辑，返回一个 `Decision`。Lua 侧不持有任何真实资源，不直接触碰 socket/动画系统/寻路系统。
- **Decision**：一个轻量的值——布尔值（ALLOW/DENY）、一个枚举字符串（`"ATTACK"`/`"FLEE"`/`"PATROL"`）、有时附带一段说明文字（拒绝原因）。它只是"选哪个 Mechanism"的凭证，不是 Mechanism 本身。
- **Host Action**：Host 拿到 Decision 后，回到自己的 Mechanism 代码里，执行对应的分支。这一步 Lua 完全不参与，Lua 已经在上一步交出决策后退出了调用栈。

重点：**Policy 通常不会给 Host 增加新能力**，Host 在引入 Lua 之前就已经能 ALLOW、能 DENY、能 ATTACK、能 FLEE、能 PATROL。Policy 只是把"在这些已有能力之间选哪一个"这件本来写死在 C 代码里的 if/else，挪到了一个可以在运行时替换、不需要重新编译的 Lua 脚本里。

## 本质区别：Policy vs 其余四种模式

- **vs Extension**：Extension 让 Host 获得一种它原本不存在的能力（新命令、新协议解析器）；Policy 不新增能力，只是在 Host 已有的能力之间做一次选择。
- **vs Configuration**：Configuration 回答"运行时参数应该取什么值"（一份声明式的数据 + 少量派生计算，通常只在启动时读一次）；Policy 回答"这一次具体场景该怎么决策"（每次事件发生都要重新跑一次，输出是一个当场生效的判断，不是一份静态参数表）。
- **vs Rule Engine**：Policy 是单次决策，内部逻辑可以是任意形状的判断（if/else、查表、算分），不要求统一结构；Rule Engine 是一组结构统一的 `Condition -> Action` 规则做批量匹配，关注"用同一套匹配框架处理 N 条规则"，而不是"这一次该怎么判断"。
- **vs Plugin**：Plugin 是一个有独立生命周期（加载、初始化、运行、卸载）的功能模块，颗粒度是"一整块可插拔的功能"；Policy 只是请求处理管线里的一个决策点，没有独立的加载/卸载概念，随 Host 主流程的调用而生、随调用返回而消亡。

## 核心不是 Lua，而是这个抽象

Policy 的核心抽象是：

```text
Mechanism (已具备的能力集合) -> Decision Point (在能力之间选择) -> Selected Mechanism
```

这个抽象和 Lua 没有必然联系——任何"策略与机制分离"（Policy/Mechanism Separation，操作系统教科书里调度器与调度策略分离就是同一个思想的另一个实例）的系统都在做同一件事，只是承载 Decision 逻辑的载体不同。Lua 是这里一个特别方便的载体：决策函数可以直接写成一个 Lua 函数，返回值直接是一个布尔值或字符串，不需要额外的 DSL、不需要重新编译 Host 就能修改决策逻辑。如果 Lua 消失，这个"机制与决策分离"的抽象依然成立，只是决策逻辑需要换成别的载体（写死在 C 里、一个配置文件驱动的简单表达式引擎、甚至一个外部服务），具体讨论见后面三个示例第 16 步。
