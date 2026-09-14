# 07 · 依赖方向 Mental Model —— 直接依赖 / Factory / Plugin 三种架构对比

> 前置阅读：[04-factory/00-why-and-architecture.md](04-factory/00-why-and-architecture.md)、[05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md)。这两个 Pattern 的 `00-why-and-architecture.md` 都已经各自画过"依赖方向"的局部对比图，本文把三种方案并排放在一起，讲清楚它们究竟在解决同一个问题的哪三个不同阶段。

## 同一个问题的三种答案

三种方案要回答的都是同一个问题：**"高层代码（Application/Framework）应该知道多少个具体实现的名字？"** 答案从"全部知道"到"一个都不知道"，正好对应方案 A -> B -> C。

## 方案 A：直接依赖实现

```text
Application
     |
     v
 TcpClient
```

```lua
local TcpClient = require("tcp_client")
local client = TcpClient.new(config)
```

**问题**：

```text
Application
    |
    +-- knows TCP          (知道有一种叫 "tcp" 的实现)
    +-- knows constructor  (知道构造函数叫 TcpClient.new，签名是什么)
    +-- knows implementation (知道 tcp_client.lua 这个模块存在、在哪)
```

如果后续要支持 UDP，Application 的代码必须被打开、加入 `require("udp_client")` 和一段选择逻辑——**每新增一种实现，Application 自身的源码都要改**。这是三种方案里耦合最紧的一种，也是 [04-factory/00-why-and-architecture.md](04-factory/00-why-and-architecture.md) 的 Anti-pattern 一节具体展示的写法。

## 方案 B：Factory

```text
Application
     |
     v
  Factory
     |
     v
Concrete Implementation
```

```lua
local factory = require("factory")
local client = factory.create(kind, config)   -- kind 是运行时才知道的字符串，如 "tcp"/"udp"/"mock"
```

Factory 隐藏了两件事：

```text
creation                 -- 到底调用哪个模块的 .new()
implementation selection -- 根据 kind 选中哪一个具体实现
```

Application 现在只知道"有一个 Factory，给它一个 kind 字符串，能拿到一个满足统一接口（`:send()`/`:close()` 之类）的对象"——它**不知道**、也**不需要知道** TCP/UDP/Mock 各自的模块名和构造函数签名。新增一种实现（例如 WebSocket）只需要：写一个新的实现模块 + 在 Factory 内部的映射表里加一行，Application 的源码**一行都不改**。

这一步比方案 A 好在哪：把"选择哪个实现"这个决策，从 N 个分散在 Application 各处的调用点，收敛成 Factory 内部**一个**集中的映射表——但注意，Application 依然直接 `require("factory")`，依然知道"存在一个具体叫 `factory` 的东西"，这一点在方案 C 会被进一步打破。

## 方案 C：Plugin（Framework 定义 Contract）

```text
             Framework
                 |
                 | contract
                 v
            Plugin API
                 ^
                 |
        +--------+--------+--------+
        |        |        |        |
      Plugin A Plugin B Plugin C  ...
```

```lua
-- framework.lua 从不 require 任何具体 plugin
function Framework.register(plugin)
    -- 只要求 plugin 满足契约（有 name、有约定的处理函数），不关心它是谁
end
```

```lua
-- app.lua （或者 Framework 的启动脚本）才知道具体有哪些 plugin
local framework = require("framework")
framework.register(require("plugins.http_handler"))
framework.register(require("plugins.dns_handler"))
```

这里的关键区别是：**Framework 自己的源码里，完全没有出现任何具体 Plugin 的名字**。谁来 `require` 具体的 Plugin 模块，变成了 Framework **外部**（调用 `register` 的那一层，可能是 `app.lua`，也可能是运行时扫描一个目录）的责任，Framework 只声明"契约是什么形状"，从不关心契约背后是谁实现的。这就是 [00-mental-model-overview.md](00-mental-model-overview.md) 第 3 节提到的 Dependency Inversion 在 Module 组织层面的真正样子。

## 三种方案并排对比

```text
方案 A（直接依赖）           方案 B（Factory）              方案 C（Plugin / DI）
Application                 Application                    Framework
    |                           |                              |
    v                           v                              | 只依赖 Contract
TcpClient                    Factory                           v
UdpClient                       |                          Plugin Interface
MockClient                  +---+---+                          ^
                             v   v   v                          | 实现
                            Tcp Udp Mock                +--------+--------+
                                                         A        B        C
新增实现:                    新增实现:                    新增实现:
改 Application 源码          只改 Factory 内部映射表       只加一个新文件，
（每处调用点都要改）          （调用点完全不变）             Framework 源码零改动
```

## Application -> Concrete Implementation 与 Application -> Abstraction <- Implementation 的区别

这是贯穿三种方案最核心的一条分界线：

```text
Application -> Concrete Implementation

Application 拥有指向"某一个具体东西"的箭头。这个具体东西改名字、改构造方式，Application 都要跟着改。
```

```text
Application -> Abstraction <- Implementation

Application 和 Implementation 各自拥有一个指向"同一个抽象/契约"的箭头，彼此不知道对方存在。
Abstraction 改变，两边都要跟着改；但 Implementation 的数量、名字、内部实现的变化，
完全不会传导到 Application ——这条箭头从"指向对方"变成了"两边都指向同一个稳定的中间层"。
```

方案 A 是纯粹的第一种关系；方案 B（Factory）把"选择哪个 Implementation"的箭头收拢进 Factory 内部，Application 对 Factory 依然是第一种关系（Application 依赖 Factory 这个具体模块），但 Factory 对各个 Implementation、以及 Application 对"最终拿到的产品对象的接口形状"已经变成第二种关系；方案 C（Plugin）把第二种关系落实得最彻底——Framework 和 Plugin 都只认识 Contract，互相之间没有任何直接箭头。

## 一句话收尾

```text
方案 A: Application 认识每一个具体实现的名字。
方案 B: Application 只认识 Factory 这一个名字，具体实现的名字被 Factory 藏起来。
方案 C: Framework 连"具体实现"这个概念都不需要认识，它只认识"契约长什么形状"。
```

这也解释了为什么 [00-mental-model-overview.md](00-mental-model-overview.md) 的演进链把 Plugin 放在最后一环："我不希望调用者知道具体创建哪个实现"（Factory）和"我希望第三方/外部模块实现 Framework 定义的能力"（Plugin）表面相似，但 Plugin 比 Factory 更进一步——Factory 内部依然维护着一份"知道所有实现"的映射表，Plugin 的 Framework 则完全不需要这份映射表，注册/发现机制把"知道有哪些实现"这件事，从 Framework 内部彻底移到了 Framework 外部。

下一篇 [08-pattern-composition-and-real-world-cases.md](08-pattern-composition-and-real-world-cases.md) 会展示这三种方案、五个 Pattern 在真实项目里是如何同时出现、组合使用的。
