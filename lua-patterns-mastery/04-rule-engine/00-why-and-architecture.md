# 00 · Pattern D：Rule Engine —— WHY & Architecture

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中定义的词汇：Host 拥有 Mechanism / Resource / Lifecycle / Stable API；Lua 拥有 Behavior / Policy / Rules / Configuration / Extension。本文只展开其中的 **Rules**。

## WHY：Rule Engine 回答的是哪个问题

在 Host/Lua 分工表里，Policy 和 Rules 是紧挨着的两行，很容易被混为一谈。区分点在于两者回答的问题不同：

- **Policy** 回答的是："现在（这一次）应该怎么做？" —— 单次决策，输入通常是一个具体场景，输出通常是一个动作或一个值。决策逻辑可以是任意复杂的 if/else，甚至完全不需要"规则"这个中间概念。
- **Rule Engine** 回答的是："这个输入满足哪些规则？每条命中的规则应该触发什么 Action？" —— 核心是一组**结构统一**的规则（每条规则都长成 `Condition -> Action` 的形状），Host 把输入喂给这一组规则，Lua 负责挨个（或并行）判断哪些规则命中，再把命中规则对应的 Action 交还给 Host。

换句话说，Policy 关心"这一次的答案"，Rule Engine 关心"用同一套匹配框架去处理一批结构相似的规则"。Rule Engine 的典型数据流是：

```text
Input
  |
  v
Rule Matching
  |
  v
Matched Rules
  |
  v
Action
```

`Input` 是 Host 传进来的一个事实（一个数据包、一个 HTTP 请求、一轮设备指标）；`Rule Matching` 是 Lua 侧遍历规则表、对每条规则的 `match`/`Condition` 求值；`Matched Rules` 是这一轮命中的规则子集（可能是 0 条、1 条，也可能是多条）；`Action` 是命中规则各自携带的处理逻辑，最终交还给 Host 执行。

## Architecture：Lua Rule Engine 在 Host 架构图里的位置

```text
             Host
               |
               v
             Input
               |
               v
          Lua Rule Engine
               |
        +------+------+------+
        |      |      |
      Rule1  Rule2  Rule3
        |      |      |
        +------+------+------+
               v
             Action
               |
               v
              Host
```

逐层解释：

- **Host**：拥有 Mechanism（网络收包、HTTP 服务器主循环、设备指标采集线程）和 Resource（socket、连接、传感器读数），负责在恰当的时机把一份 Input 转换成 Lua table，调用 Lua 侧的规则引擎入口函数。
- **Input**：Host 侧的一个结构化事实，转换规则是 Binding 层的职责——把 C struct 拆字段、`lua_setfield` 逐个塞进一个 Lua table。
- **Lua Rule Engine**：一段遍历规则表的 Lua 代码（通常是一个 `for` 循环 + `pcall` 保护），不是某一条具体规则，而是"如何遍历、如何组合命中结果"这套调度逻辑本身。
- **Rule1 / Rule2 / Rule3**：规则表里的具体条目，每条规则都遵循同一个形状——一个 `match`（或者更细分的 Matcher + Condition）加一个 `action`。规则之间理论上互相独立，可以并行判断，顺序只在"要不要 First Match 提前退出"时才有意义。
- **Action**：命中规则返回的处理结果，可能是一个简单字符串（`"ALLOW"`），也可能是一个结构化 table（`{level="ALARM", message="..."}`）。Action 本身不执行副作用，只是"应该做什么"的描述，真正的副作用（丢包、转发请求、发邮件）由 Host 执行。
- **Host（下）**：拿到 Action（或 Action 列表）后，负责真正调用 Mechanism 层去执行（真正丢弃这个包、真正把请求转发到某个 backend、真正发送一封告警邮件）。Lua 全程不直接接触 socket/邮件系统等真实资源。

## 本质区别：Rule Engine vs 其余四种模式

- **vs Policy**：Policy 是单次决策（"这一次该怎么做"，逻辑可以是任意形状的 if/else）；Rule Engine 是一组结构统一的规则做批量匹配（"用同一个 `match+action` 框架处理 N 条规则，可能多条同时命中"）。
- **vs Extension**：Extension 是给 Host 新增一种它原来没有的能力（新命令、新协议解析器）；Rule Engine 不新增能力，只是在 Host 已有的能力范围内，对已经存在的输入做条件匹配。
- **vs Configuration**：Configuration 描述的是"运行时参数应该是什么"（值 + 少量派生计算）；Rule Engine 描述的是"IF 条件 THEN 动作"的逻辑关系，重点在条件判断本身，不是参数取值。
- **vs Plugin**：Plugin 是一个有独立生命周期（加载/初始化/卸载）的模块，颗粒度是"整个功能单元"；Rule Engine 里的每条规则没有独立生命周期，只是同一批规则表里的一条数据，颗粒度是"一条 IF/THEN"。

## 核心不是 Lua，而是这个抽象

Rule Engine 的核心抽象是：

```text
Input -> Predicate -> Match -> Action
```

这四步和 Lua 本身没有必然联系——传统 Rule Engine（Drools、各种防火墙的规则语法、Nginx 的 `location` 匹配）都在做同一件事，只是 Predicate 和 Action 的载体不同（DSL、正则、专用配置语法、字节码……）。Lua 只是这里一个特别方便的载体：`match` 和 `action` 都可以直接写成 Lua 函数（闭包），规则表本身也可以直接是一个 Lua table，不需要额外发明一套规则 DSL 和对应的 parser。如果 Lua 消失，这个四步抽象依然成立，只是 Predicate/Action 需要换成别的载体（详见后面三个示例第 16 步的讨论）。
