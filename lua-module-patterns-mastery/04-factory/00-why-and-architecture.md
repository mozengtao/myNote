# 00 · Pattern 4：Factory —— WHY & Architecture

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中的核心映射：**Factory Module -> Abstraction / Implementation Hiding**。前一个 Pattern（Class / Constructor）解决的是"一个类型如何创建多个拥有独立 State 的对象"；这个 Pattern 解决的是完全不同层次的问题——**当同一个"契约"存在多种具体实现时，谁来决定这一次该造哪一个**。Factory 本身几乎不持有 State（State 在它创建出来的产品对象身上），它是一层专门负责"隐藏创建决策"的间接层。

## 1. Pattern Definition

Factory Module 要回答的问题是：**"调用者只想要一个满足某个契约的对象，不想知道、也不应该知道具体是哪一个实现类"**。它的固定形状：

```lua
local ConcreteA = require("concrete_a")
local ConcreteB = require("concrete_b")

local M = {}

local creators = {
    a = ConcreteA.new,
    b = ConcreteB.new,
}

function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, "factory: unknown kind '" .. tostring(kind) .. "'"
    end
    return creator(config)
end

return M
```

逐行对应到 Mental Model 的词汇表：

| 代码 | 概念 | 说明 |
|---|---|---|
| `require("concrete_a")` / `require("concrete_b")` | **具体实现模块** | Factory 是全局唯一集中 require 所有实现的地方 |
| `creators` 表 | **选择表** | `kind` 字符串 -> 构造函数,新增实现只需要加一行 |
| `M.create(kind, config)` | **统一入口** | 调用者永远用同一个函数签名,不管最终造出哪个实现 |
| `return nil, "..."` | 失败路径 | 未知 `kind` 不抛出不可预期的 `error()`，走 Lua 惯用的 `nil, err` |
| `return M` | Module 边界 | `require("factory")` 拿到的只是"创建能力"，不是任何具体实现 |

`require("factory")` 返回的是 **一个只会做"查表 + 转发"的选择函数**，不是 `ConcreteA`、也不是 `ConcreteB`。调用者拿到 Factory 之后,唯一的入口是 `factory.create(kind, config)`——不管这一次内部转发给了哪个具体实现,调用方式完全一样：

```text
local client = factory.create(kind, config)
```

这就是本文题目里强调的"统一入口":`kind` 换成 `"tcp"`/`"udp"`/`"mock"`,调用者代码一个字都不用改。

## 2. Why

**Factory 的价值不是"帮你调用 `new()`"，而是隐藏"具体实现选择"这个决策本身。** 如果只是想少打几个字,完全不需要 Factory——`TcpClient.new(config)` 比 `factory.create("tcp", config)` 更短、更直接。Factory 存在的真正理由是:当"该创建哪个实现"这件事需要对调用者不可见时,才需要这一层。

具体的设计问题是这样出现的:一个 Application 需要根据配置(或运行环境)在多个实现之间切换——测试时用 `mock`,生产环境用 `tcp`,某些场景用 `udp`。**没有 Factory 时**,Application 只能这样写:

```text
Application
     |
     +-- TcpClient.new()
     +-- UdpClient.new()
     +-- MockClient.new()
```

Application 必须:

1. `require` 每一个具体实现模块(哪怕这次运行根本用不到 `UdpClient`);
2. 知道每一个具体实现的构造函数名字(`TcpClient.new` 还是 `TcpClient.create`? 每加一个新实现都要重新学一遍);
3. 自己写 `if kind == "tcp" then ... elseif kind == "udp" then ...` 这段选择逻辑——而且这段逻辑很可能在项目里被复制粘贴到不止一个地方(哪里要创建 client,哪里就要重复一遍这个 `if/elseif`)。

**有 Factory 之后**:

```text
Application
     |
     v
Factory
     |
     +-- TcpClient
     +-- UdpClient
     +-- MockClient
```

Application 只 `require("factory")`,只调用 `factory.create(kind, config)`,从头到尾不知道 `TcpClient`/`UdpClient`/`MockClient` 这几个模块名的存在。"该 require 哪个实现""该调用哪个构造函数""选择逻辑该写在哪"——这三件事被集中搬到了 Factory 内部,只有一份,不会在项目里散落出第二份。新增一种实现(比如 `websocket`)时,只需要修改 Factory 内部的选择表,Application 代码不需要动一行。

## 3. Architecture

Application / Framework / Module / Implementation 四层模型里，这个 Pattern 用到三层：**Application**、扮演中间层的 **Factory**、以及可能有多个的 **Implementation**（每一个具体实现本身往往就是一个 Pattern 3 的 Class / Constructor Module）：

```text
Caller
   |
   v
Factory
   |
   +-- Concrete A
   +-- Concrete B
   +-- Concrete C
```

统一入口：不管 Factory 内部一共登记了多少种具体实现，调用者永远用同一行代码触发创建：

```text
local client = factory.create(kind, config)
```

`kind` 是运行时才决定的字符串（可能来自配置文件、环境变量、命令行参数），`Factory` 在这一刻才去查表、决定转发给哪一个 Concrete 实现的构造函数。**Factory 内部虽然 require 了全部具体实现，但对外只暴露 `create` 这一个函数**——三个 Concrete 模块名本身完全不出现在 Factory 的公开返回值里。

## 4. Lifecycle

Factory 的生命周期建立在 Pattern 3（Class / Constructor）的生命周期之上，多了一层"载入期集中 require、调用期查表转发"的结构：

```text
Module load time  (只发生一次，在 require("factory") 时)
    |
    v
require("concrete_a")   -- 依次载入全部具体实现模块
require("concrete_b")
require("concrete_c")
    |
    v
构造 creators 选择表      -- { a = ConcreteA.new, b = ConcreteB.new, c = ConcreteC.new }
    |
    v
定义 M.create(kind, config)   -- 函数体只写一次
    |
    v
return M                 -- Application 拿到的是"选择能力"，不是任何具体实现


Product creation time  (发生 N 次，每次调用 factory.create(kind, config) 都会重来一遍)
    |
    v
在 creators 表里查 kind
    |
    +-- 查到 --> 调用对应的构造函数 ConcreteX.new(config)
    |                  |
    |                  v
    |            产品对象诞生 (它自己的生命周期见 Pattern 3：
    |            Object creation time -> Object State -> GC)
    |
    +-- 没查到 --> return nil, err   (不创建任何东西，不抛出异常)
```

关键结论：`require("factory")` 只会触发一次"Module load time"流程，这一次会把全部具体实现**提前**载入内存（不管这次运行到底会不会用到其中某一个）；而 `factory.create(kind, config)` 每被调用一次，就只是一次"查表 + 转发"，本身不产生任何新的持久状态——真正"活下来"的是它转发出去之后、由具体实现的构造函数造出来的产品对象，那个对象的生命周期完全按 Pattern 3 的规则走。

## 5. Relationship

**没有 Factory 时的依赖方向**——Application 直接依赖每一个具体实现：

```text
Application
     |
     +-- TcpClient.new()
     +-- UdpClient.new()
     +-- MockClient.new()
```

Application 同时依赖三个具体模块，新增一个实现，Application 的依赖数量线性增长。

**有 Factory 之后的依赖方向**——Application 只依赖 Factory：

```text
Application
     |
     v
Factory
     |
     +-- TcpClient
     +-- UdpClient
     +-- MockClient
```

依赖被"收拢"到了 Factory 一个点上，Application 对具体实现的数量、名字完全不敏感。

**这背后真正发生的是 Dependency Inversion**，把上面的图换一个画法（把"具体实现"和"高层逻辑"分别抽到 Abstraction 两侧）：

```text
High-level logic
       |
       v
   Abstraction
       ^
       |
Concrete Implementation
```

- **High-level logic**（Application）只认识 Abstraction——也就是"每一种 transport 都有 `:send()`/`:close()`"这个统一的方法形状约定，不认识 `TcpClient`/`UdpClient`/`MockClient` 这几个具体名字。
- **Concrete Implementation**（`TcpClient`、`UdpClient`、`MockClient`）只需要满足 Abstraction 定义的方法形状，不需要知道谁在调用自己、调用者是不是 Application。
- **两边都不直接依赖对方**：Application 依赖的是"任何满足 Abstraction 的东西"这个抽象承诺，Concrete Implementation 依赖的是"这个方法形状本身"这份契约，双方都不知道对方的存在。
- **Factory 是把两边接起来的那个人**：它在创建时刻决定"这一次让哪一个 Concrete Implementation 去填补 Abstraction 这个位置"，这一步发生在 Factory 内部，Application 感知不到、也不需要感知。

对照关系一目了然：Application 处于 High-level logic 的位置，`:send()`/`:close()` 这套方法形状是 Abstraction，`TcpClient`/`UdpClient`/`MockClient` 是 Concrete Implementation，Factory 站在 Abstraction 和 Concrete Implementation 之间做"这一次该选谁"的决定。

## 6. Data / Control Flow

以 `factory.create("tcp", config)` 为例，完整走一遍调用链：

```text
factory.create("tcp", config)
   |
   v
第 1 步：在 creators 表里查 creators["tcp"]
   |
   |  creators 表在 Module load time 就已经构造好，
   |  这一步只是普通的表查找，不涉及任何 I/O 或副作用
   v
第 2 步：查到 creators["tcp"] == TcpClient.new
   |
   v
第 3 步：调用 TcpClient.new(config)
   |
   |  这一步完全按 Pattern 3（Class / Constructor）的规则走：
   |  setmetatable({}, TcpClient) -> 写入 self.host/self.port/... -> return self
   v
第 4 步：TcpClient.new 返回一个新的 TcpClient 实例
   |
   v
第 5 步：factory.create 把这个实例原样返回给调用者
   |
   v
调用者拿到 client，此后调用 client:send(data) / client:close()
   -- 这两次调用完全按 Pattern 3 的方法查找规则走（__index -> TcpClient）,
   -- Factory 已经在第 5 步交出返回值、退出调用栈，不再参与
```

如果 `kind` 是未知值（例如 `"websocket"`），流程在第 1 步就会分岔：

```text
factory.create("websocket", config)
   |
   v
第 1 步：在 creators 表里查 creators["websocket"] -> 查不到，得到 nil
   |
   v
第 2 步：`if not creator then` 命中，直接 return nil, "factory: unknown ... kind 'websocket'"
   |
   v
调用者拿到 (nil, err)，必须自己判断第一个返回值是不是 nil 才能继续
```

## 7. State

**Factory 模块本身通常是无状态的**——`M.create(kind, config)` 是一个纯粹的"查表 + 转发"函数：给定同样的 `kind` 和 `config`，每次调用都只是找到同一个构造函数、把 `config` 原样转发过去，Factory 内部的 `creators` 表在 Module load time 构造完成后就不再变化，不存在"上一次调用影响下一次调用结果"的情况。

**真正的 State 转移到了 Factory 创建出来的产品对象身上**——这些产品对象往往本身就是 Pattern 3（Class / Constructor）意义下的对象：每一个 `TcpClient`/`UdpClient`/`MockClient` 实例都有自己独立的 `self.closed`、`self.sent_count` 等字段，State 的归属、生命周期、创建时机完全按 [03-class-constructor/00-why-and-architecture.md](../03-class-constructor/00-why-and-architecture.md) 里已经讲清楚的规则走，本文不重复展开，只强调一次分工：

```text
Factory Module   -- 无状态，只做一次性的"这次选哪个"决策
    |
    |  create(kind, config)
    v
Product Object   -- 有状态，State 属于这一个实例，独立于其他同类型实例
```

一句话记忆这条分工线：**"选择"这件事本身不需要记住任何东西，"被选出来的东西"才需要记住东西。**

## 8. Failure

Factory 这一层有两类必须分开处理的失败，混在一起处理是最常见的错误来源：

**(1) 请求了一个未知的 `kind`**——这是"正常但没有匹配到"的情况，不是程序 bug：

```lua
function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown transport kind '%s'", tostring(kind))
    end
    return creator(config)
end
```

习惯用 Lua 惯用的 `nil, err` 返回，而不是 `error("unknown kind")` 直接抛出异常——因为"调用者传了一个 Factory 不认识的字符串"是一种**可预期的、调用者应该自己判断并处理**的结果（例如配置文件里的 `transport_type` 拼错了），不应该表现成一次"程序运行出错崩溃"。调用者拿到 `(nil, err)` 之后，是选择打日志退出、还是 fallback 到默认实现，是调用者自己的决策，Factory 不应该替调用者做这个决定（也就不应该用 `error()` 强行中断调用栈）。

**(2) `kind` 是已知的，但对应的具体实现构造失败**——这是"匹配到了，但构造过程本身出问题"的情况：

```lua
function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown kind '%s'", tostring(kind))
    end
    return creator(config)   -- 如果 creator(config) 内部 error() 了，这里不做任何拦截
end
```

这里 Factory **必须让错误原样传播回调用者**，不能因为"这是我内部转发出去的调用"就用 `pcall` 悄悄吞掉。原因很直接：如果具体实现的构造函数因为参数不合法而 `error()`（例如 `file logger` 在 `io.open` 失败时 `error("file logger: cannot open ...")`），这个错误信息里包含了诊断这次失败真正需要的上下文（哪个路径打不开、什么原因），Factory 如果 `pcall` 之后只返回一个笼统的 `"factory: creation failed"`，调用者反而失去了排查问题所需要的信息。**Factory 在这一步的正确角色是"透明的转发者"，不是"错误的翻译者"**——它可以在自己能判断的第 (1) 类失败（`kind` 查不到）上主动把错误转成 `nil, err`，但对第 (2) 类失败（具体实现内部的错误），应该让它保持原样穿透过去，让调用者看到最原始的失败原因。

两类失败的责任边界：

```text
kind 未知           -> Factory 自己能判断 -> Factory 负责转成 nil, err
具体实现构造失败      -> Factory 无法判断细节 -> Factory 只负责不拦截，原样传播
```

## 9. Mental Model

**一句话记忆**：Factory 不是"帮你少打几个字的 `new()` 快捷方式"，而是"让调用者永远不需要知道有几种具体实现、分别叫什么名字"的一层遮罩。

**核心问题**：如果没有这个 Factory，Application 需要额外知道多少个具体模块的名字、多少个构造函数的签名？——如果答案是"不止一个"，Factory 就有存在的理由。

**核心关系**：

```text
Application -> Factory -> ( Concrete A | Concrete B | Concrete C )
```

Application 只朝 Factory 这一个方向依赖，圆括号里的选择在运行时才发生，且发生在 Factory 内部，对 Application 不可见。

## 10. Design Trade-offs

**优点**

- 把"该创建哪个具体实现"这个决策从 Application 里搬出来、集中到一处，新增一种实现时只需要改 Factory 内部的选择表，Application 代码不需要变。
- Application 只需要 `require` 一个模块（Factory），不需要为了"可能用得到"而把所有具体实现都 `require` 进来——尤其在具体实现较重（比如真的要连接数据库驱动）的场景下，这能省掉不必要的加载开销。
- 天然支持"测试用 mock 实现、生产用真实实现"这种切换——调用者代码完全不变，只是配置里的 `kind` 字符串不同。

**缺点 / 代价**

- 多了一层间接调用——读代码的人看到 `factory.create("tcp", config)`，如果不打开 Factory 内部的选择表，不能立刻知道最终执行的是哪一份构造函数逻辑，调试时要多跳一次。
- Factory 内部必须集中 `require` 所有具体实现，如果具体实现之间本来毫无关系、只是"恰好都要被 Factory 管理"，会让 Factory 文件看起来像一个不相关模块的大杂烩。
- 如果这一次运行只可能用到唯一一种实现（不存在真正的"选择"场景），引入 Factory 反而是纯粹的过度设计——直接 `require` 那一个具体实现更直接。

**什么时候不要用这个 Pattern**

- 只存在一种实现、未来也不打算增加第二种：直接 `require` 具体模块即可，Factory 的"隐藏选择"价值无从体现。
- 调用者本来就必须知道、也应该知道具体是哪个实现（例如调用者需要用到某个实现独有的、不在统一契约里的方法）：这种场景下强行套 Factory，只会让调用者绕一圈又把具体类型转换/断言回来，反而更绕。

**与相邻 Pattern 的区别**

- vs Pattern 3（Class / Constructor）：Class / Constructor 关心"`new` 出来的每一个 Object 该怎么管理自己的 State"；Factory 关心"这一次该调用哪一个 Class 的 `new`"。Factory 往往站在多个 Class / Constructor 模块的上游,自己不持有 State,只做一次性的选择。
- vs Pattern 5（Plugin）：Factory 在 Module load time 就已经知道全部候选实现有哪些（选择表是写死在代码里的),新增实现要改 Factory 源码；Plugin 允许在 Module load time 之后,由外部代码动态注册新的实现,Framework 自身代码不需要为了支持新实现而修改。两者都在做"Abstraction 两侧互不知道对方"的 Dependency Inversion,区别在于"候选集合是不是在编译/加载时就已经封闭"。

## Anti-pattern

最常见的错误：Application 直接 `require` 每一个具体实现,自己写选择逻辑。

```lua
local TcpClient = require("tcp_client")
local UdpClient = require("udp_client")
-- ...

local client
if kind == "tcp" then
    client = TcpClient.new(config)
elseif kind == "udp" then
    client = UdpClient.new(config)
end
```

这段代码看起来"能跑"，但具体地分析会发现三个真实的代价：

**(1) 新增一种 transport 需要编辑 Application 代码。** 如果要支持 `"mock"`，必须在 Application 里再加一个 `require("mock_client")` 和一段 `elseif kind == "mock" then`。如果这段选择逻辑在项目里出现在不止一个地方（比如既在启动流程里创建一次，又在某个测试辅助函数里创建一次），每新增一种实现都要同步改多处，很容易漏改一处导致两处行为不一致。

**(2) Application 必须知道每一个具体实现的构造函数名字。** 这段代码假设所有实现都叫 `.new`；一旦某个具体实现的作者用了 `.create`（或者构造函数需要的参数顺序和其他实现不一样），Application 里的这段选择逻辑就要跟着改，Application 被迫了解每一个具体实现内部的构造约定，而不是只了解一份统一契约。

**(3) 测试 Application 时，所有具体实现模块都必须能被加载，即使这次测试只用得到一个。** `require("tcp_client")` 和 `require("udp_client")` 会在文件顶部无条件执行——哪怕这次测试只想验证 `"mock"` 分支的行为，`tcp_client.lua`/`udp_client.lua` 也必须存在且能被正常 `require`（不能因为测试环境没有真实网络库、或者故意想在这次测试里排除掉某个尚未实现完的模块而跳过）。这意味着测试 Application 的"选择逻辑"这一件事，被意外地和"所有具体实现是否都能正常加载"绑在了一起。

把这段逻辑收进 Factory 之后，以上三个问题同时消失：新增实现只改 Factory 内部一张表；Application 永远只调用 `factory.create(kind, config)` 这一个统一签名；测试 Application 时只需要 Factory 能被加载（如果连 Factory 内部的具体实现加载都想避免，还可以在测试里用依赖注入替换掉 Factory 本身——这是 Pattern 5 Plugin 要解决的更进一步的问题）。
