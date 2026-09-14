# 05 · Pattern 5：Plugin —— WHY & Architecture

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中定义的词汇：Plugin 的核心设计思想是 **Extension / Dependency Inversion**，主要解决"让 Framework 能被外部模块扩展，且不需要修改自身代码"这个问题，State 归"每个插件自己"（可选、Plugin 级）。

## 1. Pattern Definition

Plugin Module 不是"一个 Module"，而是**两个角色 + 一份契约**：

- **Framework（Plugin Manager）**：一段固定的代码，定义"插件必须长什么样"（Contract），并提供 Load / Register / Invoke / Unload 这套通用机制。它是本笔记里第一次出现"需要被扩展、但自身不能被随便修改"的角色。
- **Plugin**：任意数量的、独立的 Lua Module，每一个都 `return` 一个满足 Framework 契约的 table。Framework 编译/编写完成的那一刻，并不知道最终会有几个 Plugin、叫什么名字——这是 Plugin Pattern 和前四个 Pattern 最本质的区别：前四个 Pattern 里，"有哪些实现"在写 Module 的时候通常已经确定；Plugin 里，"有哪些实现"要留到 Framework 写完之后，甚至运行时才确定。

`require()` 在这里只负责"把 Plugin 的源文件变成一个 Lua table"，这是 Load 阶段的具体实现，不是 Plugin Pattern 的核心。**本文要反复论证的核心论点是：Plugin Pattern 的本质是 Contract + Discovery/Registration + Invocation，不是 `require()`。** 只用 `require()` 而没有 Contract、没有统一的 Register/Invoke 机制，得到的只是"一堆互相独立的 Table Module"，谈不上 Plugin。

Module 边界：Framework 文件的边界，是它对外暴露的 `register_*`/`unregister_*`/`execute_*`/`dispatch` 几个函数，内部的 registry 完全不对外暴露；每个 Plugin 文件的边界，是它 `return` 的那个 table 必须满足 Framework 定义的契约形状，内部实现细节（比如插件自己的计数器）完全不对 Framework、也不对其他 Plugin 暴露。

## 2. Why

为什么不能就用 Factory（Pattern 4）解决"多种实现"的问题？Factory 解决的是"调用者不知道该 `require` 哪个具体实现，把这个决策权交给 Factory"，但 Factory 内部仍然要在写代码的时候**知道**一共有哪几种实现（`transport/tcp.lua`、`transport/udp.lua`……),新增一种实现,Factory 内部的映射表也要跟着改一行。

Plugin 要解决的是更进一步的问题：**Framework 写完之后，完全不知道、也不需要知道未来会有多少个 Plugin、它们叫什么名字**。这在下面这些场景里是刚需：

- 一个命令行工具允许第三方给它加子命令，工具作者不可能预先知道所有子命令的名字。
- 一个协议分发框架要支持未来才会出现的新协议,不能要求"每次新增协议都要改分发框架的源码"。
- 一个报文解析引擎（本文档 Example 3 的原型）需要同时挂载多个互相独立的解析器,数量和种类由使用者决定,不是框架作者决定。

如果没有 Plugin Pattern,这三类需求都只能退化成"Framework 源码里堆 `if/elseif`"——这正是本文末尾 Anti-pattern 要展示、也是本文档要证明"错在哪"的写法。Plugin Pattern 用一份**契约**替代了"Framework 认识每一个具体实现"这件事,把"添加新能力"这个动作,从"改 Framework 源码"变成了"新增一个满足契约的文件"。

## 3. Architecture

```text
                 Framework
                    |
                    | defines contract
                    v
              Plugin Interface
                    ^
                    | implements
          +---------+---------+
          |         |         |
       Plugin A  Plugin B  Plugin C
```

对照 Application / Framework / Module / Implementation 四层：

- **Application**（本文档的 `app.lua`）：负责把 Framework 和若干 Plugin 都 `require` 进来,依次调用 Framework 的 `register_*` 把 Plugin 接进去,再驱动 Framework 的 `execute_*`/`dispatch`。Application 不直接调用任何具体 Plugin 的函数。
- **Framework**：即上图的 `Framework` 节点,拥有 Mechanism（内部 registry、注册/校验/分发逻辑）,只依赖 `Plugin Interface` 这一层抽象。
- **Plugin Interface（Contract）**：不是一个真实存在的 Lua 文件,而是 Framework 里 `validate_plugin` 这个函数隐含定义的"形状"——本例是"必须有 `name` 字段 + 特定签名的处理函数"。这一层是整张图里唯一被 Framework 和所有 Plugin **同时依赖**的东西。
- **Plugin A / B / C（Implementation）**：具体的 `plugins/*.lua` 文件,各自实现契约要求的字段和函数,内部怎么实现完全自由。

Framework 的职责按六步拆开(这是本文档要反复回到的分解方式):

```text
Framework
    |
    +-- define contract
    +-- load plugin
    +-- validate plugin
    +-- register plugin
    +-- invoke plugin
    +-- unload/shutdown plugin
```

一个 Plugin 的形状则统一是:

```text
Plugin
    |
    +-- metadata
    +-- init()
    +-- process()
    +-- shutdown()
```

三个示例里,`metadata` 落地为 `name`(以及 Example 3 的隐式身份);`init()` 在本文档的三个纯 Lua 示例里被简化成"require 时的顶层代码执行",没有单独的显式初始化函数(真实系统里如果 Plugin 需要读配置文件、建立连接池,这里就会出现一个显式的 `init()`);`process()` 落地为 `run`/`handle`/`dissect`;`shutdown()` 只在 Example 1 里通过 Framework 侧的 `unregister_command` 间接体现(见第 4 节)。

## 4. Lifecycle

```text
Discovery
    |
    v
   Load
    |
    v
 Register
    |
    v
 Lifecycle
    |
    v
  Event
    |
    v
  Unload
```

逐条对照三个示例的真实代码回答:

- **Discovery**:三个示例里,"发现有哪些插件"这一步被简化成 `app.lua` 顶部显式写出的几行 `require("plugins.xxx")`——这是本文档必须诚实承认的简化:真实的插件系统通常会扫描一个目录、读一份插件清单来自动发现插件,而不是像这里一样把文件名写死在 Application 代码里。这个简化不影响 Contract + Register + Invoke 这套核心机制的成立,但意味着本文档的示例更接近"Framework 支持任意数量、任意名字的插件"这个能力的最小证明,而不是一套完整的自动发现机制。
- **Load**:等价于 Lua 的 `require()` 本身——把 `.lua` 文件的顶层代码执行一次,拿到它 `return` 的 table。Load 完全由 Lua 运行时负责,Framework 的代码里完全没有出现"读文件"这类逻辑,这也是为什么第 3 节的六步职责列表里"load plugin"在三个 `framework.lua` 里都没有对应函数——它被委托给了 Host 语言自带的模块加载机制。
- **Register**:三个示例分别是 `Framework.register_command(name, plugin)`(Example 1)、`Framework.register_handler(protocol, plugin)`(Example 2)、`Framework.register_dissector(plugin)`(Example 3)。**一个 Plugin 被注册的确切时刻,就是这几个函数被调用、且 `validate_plugin` 校验通过、写入内部 registry 的那一刻**——在这一刻之前,Framework 完全不知道这个 Plugin 存在。
- **Lifecycle**:本文档三个示例里都没有一个显式的"插件状态机"(运行中/已失败等状态字段),这是刻意的简化——真实系统里,Framework 可能会给每个已注册的 Plugin 附加一个状态字段(比如"是否初始化成功")。三个示例用**是否存在于 registry 里**这一个二值状态代表了简化版的 Lifecycle:在 registry 里,就是"已注册、可被调用";被移除,就是"已卸载、不可再被调用"。
- **Event**:**一个 Plugin 被"调用"的确切时刻**,是 Framework 的 `execute_command`/`dispatch` 被 Application 调用、且从 registry 里成功查到对应 Plugin 的那一刻——Example 1/2 是 `execute_command(name, ...)`/`dispatch(packet)` 里那次 `plugin.run(...)`/`plugin.handle(packet)` 调用;Example 3 是 `dispatch(packet_context)` 里,某个 Plugin 的 `can_handle(ctx)` 返回 `true` 之后紧接着的 `plugin.dissect(ctx)` 调用。三个示例的 `app.lua` 都会多次触发 Event,每一次都是独立的一次调用。
- **Unload**:**这些示例里,插件是否会被卸载/关闭是不一样的**——`code/01-command-plugin/app.lua` 显式演示了 `Framework.unregister_command("debug")`,调用之后 `debug` 从 registry 里被移除,后续再 `execute_command("debug", ...)` 会得到"unknown command"的优雅错误,这就是本文档对"Unload"这一步唯一的完整落地。Example 2 和 Example 3 的 `framework.lua` 都提供了对应的 `unregister_handler`/`unregister_dissector` 函数(契约层面 Framework 支持卸载),但它们的 `app.lua` 没有调用它——这些示例里注册的插件集合从 Register 完成到进程退出为止保持不变,诚实地说,这两个示例只验证了"Unload 机制存在、可用",没有在 `app.lua` 里演示"卸载后重新验证行为"这一步(Example 1 已经演示过这个动作,不重复堆砌)。

## 5. Relationship

这是 Plugin Pattern 与前四个 Pattern 依赖方向上最本质的区别,必须把两种依赖方向摆在一起对比:

```text
直接依赖（前四个 Pattern 的默认状态）:

Application -> Concrete Implementation


依赖反转（Plugin Pattern 要求的状态）:

Application -> Abstraction <- Implementation
```

映射到本文档的 Framework/Plugin Interface/Plugin A-B-C 结构:

```text
                 Framework
                    |
                    | defines contract
                    v
              Plugin Interface
                    ^
                    | implements
          +---------+---------+
          |         |         |
       Plugin A  Plugin B  Plugin C
```

- **Framework 依赖箭头只指向 `Plugin Interface`,从不指向任何一个具体 Plugin**。翻遍三份 `framework.lua`,里面从未出现 `hello_plugin`、`http_handler`、`dns_plugin` 这些具体名字——`validate_plugin` 检查的永远是"有没有 `name` 字段"、"是不是函数"这种**形状**,不是"这个名字是不是我认识的某个插件"。
- **具体 Plugin 依赖(实现)的也是同一个 `Plugin Interface`**,箭头方向和 Framework 相反——`hello_plugin.lua`/`http_handler.lua`/`http_plugin.lua` 各自要保证自己 `return` 的 table 满足契约形状,但它们互相之间、以及它们和 Framework 之间,都不知道对方内部代码写了什么。
- 这就是 **Dependency Inversion** 在 Module 组织层面的样子:高层(Framework)和低层(具体 Plugin)不再互相依赖,而是共同依赖一个中间的抽象(Contract),对比 `07-dependency-direction.md` 里 Factory 的 `Application -> Abstraction <- Implementation` 结构,Plugin 这里的差别是——Factory 场景下"有几种 Implementation"在 Factory 编写时就已经固定(`tcp`/`udp`/`mock`);Plugin 场景下"有几个 Plugin"对 Framework 永远是开放的、运行时才确定的数量。

**Framework 代码本身对新增 Plugin 保持零修改**是这一节要验证的具体结论,可以直接用代码验证:给 `code/01-command-plugin/plugins/` 目录新增一个 `restart_plugin.lua`(满足 `name`+`run` 契约),只需要在 `app.lua` 里多写一行 `require` + 一行 `register_command`,`framework.lua` 一个字符都不需要改——这正是"Framework 应该对修改关闭、对扩展开放"的可验证证据,不是一句无法验证的口号。

## 6. Data / Control Flow

以 Example 2(Protocol Handler Plugin)的一次完整调用为例:

```text
Packet
   |
   v
Framework Dispatcher
   |
   v
Plugin Handler
```

展开成具体调用序列:

```text
app.lua
  -> framework.dispatch(packet)
       -> 读 packet.protocol 字段
       -> 从 registry 按 protocol 查出对应 plugin
       -> plugin.handle(packet)              [Framework -> Plugin,控制流跨越边界]
            -> print(...)                    [Plugin 自己的内部逻辑]
            -> return msg                     [Plugin -> Framework,数据流跨越边界]
       -> return msg                          [Framework -> app.lua]
  <- result, err
```

Framework 在这条链路上做的事情,永远只是"查表 + 调用 + 转发返回值",从查表到调用之间,不插入任何"认识具体协议"的逻辑。Example 3 的流程多一步:Framework 在"查表"这一步换成了"遍历 registry,逐个调用 `can_handle`,直到有一个返回 `true`",找到之后再调用该插件的 `dissect`——同样没有任何一步是"认识具体插件名字"。

## 7. State

Plugin Pattern 里必须区分两种彼此独立的 State:

- **Plugin-level State**:每个插件可以维护自己的内部数据,例如 `code/01-command-plugin/plugins/status_plugin.lua` 里的 `run_count`、`code/02-protocol-handler-plugin/plugins/mqtt_handler.lua` 里的 `message_count`。这些变量是各自文件里的 `local` 变量,是这个 Plugin Module 自己的 Module-level State(回顾 Pattern 2 的说法),对其他 Plugin、对 Framework 完全不可见——`hello_plugin` 不知道 `status_plugin` 内部有一个计数器,Framework 的 `execute_command` 也从不读取它。
- **Framework-level State**:三份 `framework.lua` 里的 `registry` 表,记录"当前有哪些插件被注册、注册成什么名字"。这份 State 由 Framework 独占,只能通过 `register_*`/`unregister_*` 修改,任何单个 Plugin 都不能、也不应该直接修改它——插件唯一能"影响" registry 的方式,是通过 Framework 提供的注册/卸载函数,而不是直接拿到 registry 表本身操作它(三份 `framework.lua` 都没有把 `registry` 这个 local 变量暴露出去)。
- 这两种 State 在生命周期上也不同步:Framework-level State(某个协议名有没有 handler)的生命周期由 Register/Unregister 决定;Plugin-level State(某个插件内部计数到多少了)的生命周期从这个插件模块第一次被 `require` 开始,一直存在到进程退出(因为 Lua 的 `require` 结果会被缓存,模块级 `local` 变量只会被初始化一次),不随 Framework 的 Register/Unregister 动作而重置——即便对某个 Plugin 调用了 `unregister_command`,它内部的 `local` 计数器变量依然留在内存里(只是不再有任何 Framework 入口可以触发它自增)。
- 这个"Plugin State 独立于 Framework State、也独立于其他 Plugin 的 State"的结论,会在横向整合文档 `06-require-and-state-mental-model.md` 里和 Table Module/Class Object/Factory 的 State 放在一起做完整对比(本文档不重复展开,只在这里把结论立住)。

## 8. Failure

三个示例统一遵循"Framework 侧的失败必须优雅、可判断,不能让 Application 崩溃"这个原则,具体分三类:

- **注册期失败**:`register_command`/`register_handler`/`register_dissector` 在契约校验不通过(缺 `name`、缺处理函数、类型不对)或者命名冲突时,统一用 Lua 的 `error(...)` 让注册失败——这是"配置错误"级别的失败,发生在 Application 启动阶段,选择直接 `error` 而不是返回 `nil, err`,是因为一个不满足契约的 Plugin 属于开发期就应该发现的编程错误,不应该被 Application 悄悄忽略过去。
- **调用期失败(未知目标)**:Example 1 的 `execute_command("restart")`(命令不存在)、Example 2 的 `dispatch({protocol="sip", ...})`(协议没有 handler)、Example 3 的 `dispatch({port=443, ...})`(没有 dissector 认领),这三种都统一用**返回 `nil, err`** 的方式处理,不使用 `error()`——因为"调用一个不存在的命令/协议/无法识别的报文"在运行期是完全正常、可预期的场景(想象一个真实的命令行工具被输入了打错的子命令名),Framework 不应该让整个 Application 因为一次这样的调用而崩溃,而是把"如何应对找不到"的决定权交还给 Application(三份 `app.lua` 都对这个返回值做了判断并打印了对应的提示)。
- **插件内部失败**:三个示例的插件实现都很简单,没有主动构造插件内部报错的场景,但结构上如果某个 `plugin.run(...)`/`plugin.handle(...)`/`plugin.dissect(...)` 内部抛出 Lua error,这个错误会直接从 Framework 的调用栈里向上传播到 Application——本文档的 Framework 没有对插件调用做 `pcall` 包裹,这是一个明确的设计取舍,不是遗漏:三个 `framework.lua` 都选择"只保证契约层面的校验(注册时检查形状),不保证插件内部实现不出错",如果要做到"一个插件跑挂了不影响其他插件"这种更强的隔离,需要在 `execute_command`/`dispatch` 内部用 `pcall` 包一层再决定要不要把错误传播出去——这是一个刻意留出来的、下一步可以做的加固点,不在本文档的三个 Example 里实现。

## 9. Mental Model

**一句话记忆**:Plugin 不是"文件之间互相 `require`",而是"Framework 定义一份契约,任意数量的 Plugin 各自实现这份契约,Framework 通过统一的注册表调用它们"。

**核心问题**:如果要新增一种能力,应该改 Framework 的源码,还是新增一个满足契约的文件?——答案永远应该是后者,如果是前者,说明这段代码已经退化成 Anti-pattern(见第 10 节)。

**核心关系**:

```text
Framework
    |
    | 只依赖
    v
Plugin Interface（Contract）
    ^
    | 实现
    |
N 个 Plugin（N 在 Framework 编写时未知）
```

## 10. Design Trade-offs

**优点**:

- Framework 对扩展开放、对修改关闭——新增能力永远是"加文件",不是"改已有文件",第 5 节已经用可执行的方式验证过这一点。
- 多个 Plugin 之间天然隔离:契约只约束"暴露出来的形状",不约束内部实现,一个 Plugin 的实现细节变化不会影响其他 Plugin,也不会影响 Framework。
- Framework 的核心逻辑(注册、校验、分发)只需要写一次、测一次,后续所有 Plugin 都复用同一套机制,不会出现"每加一种能力就要重新写一遍分发逻辑"的重复劳动。

**缺点/代价**:

- 契约本身需要提前设计好,而且一旦有多个 Plugin 已经实现了这份契约,契约就很难再改——修改 `register_command` 期望的 Plugin 形状,意味着所有已有的 `plugins/*.lua` 都要跟着改。这是 Plugin Pattern 相比前四个 Pattern 多付出的"前期设计成本"。
- 出错时的调用栈更深、更间接:一次 `app.lua` 里的 `framework.execute_command("hello", "Lua")`,中间经过 registry 查找、契约校验(注册时已经做过,调用时不重复),才真正落到 `hello_plugin.run`——比起直接 `hello_plugin.run("Lua")`,调试时要多看一层。
- 如第 8 节所说,本文档的 Framework 没有在调用期做 `pcall` 隔离,如果需要"一个插件崩溃不影响其他插件"这种更强的容错,需要额外的加固,不是 Plugin Pattern 自带的免费保证。

**什么时候不要用**:如果一开始就明确只会有一种实现、不存在"被外部扩展"的需求,直接把这一种实现写成 Table Module 或 Class/Constructor Module 即可——引入 Framework + Contract + Registry 这一整套机制,对只有一种实现的场景是纯粹的过度设计。

**与相邻 Pattern 的区别**:Factory(Pattern 4)解决的是"隐藏该创建哪个已知实现",写 Factory 的人在写代码时就知道一共有几种实现;Plugin 解决的是"允许未来出现任意数量、当前完全未知的实现",这是本笔记五个 Pattern 里唯一一个"实现的数量对 Framework 永远开放"的 Pattern。

---

## Anti-pattern

以下是本文档反复强调"绝不能出现"的写法——如果 `framework.lua` 变成这样:

```lua
-- 反面示例：不要这样写 framework.lua
function Framework.dispatch(packet)
    if packet.protocol == "http" then
        -- http 的处理逻辑直接写在这里
        print("handling http packet: " .. tostring(packet.payload))
    elseif packet.protocol == "dns" then
        -- dns 的处理逻辑直接写在这里
        print("handling dns query: " .. tostring(packet.query))
    else
        print("no handler for protocol: " .. tostring(packet.protocol))
    end
end
```

这不是"风格不好看",而是从架构根本上违背了 Plugin Pattern 想解决的问题,具体体现在三点:

1. **新增一个协议,要求编辑 Framework 自己的源文件**——想支持 `mqtt`,必须在这个 `if/elseif` 链上再加一个分支,意味着改动 `framework.lua` 本身。这直接违反"Framework 应该对修改关闭、对扩展开放"这条设计原则(第 5/10 节反复强调的结论),第 5 节里"新增一个 `restart_plugin.lua` 不需要动 `framework.lua`"这个可验证的性质,在这种写法下完全不成立。
2. **Framework 从"只依赖 Contract 形状"退化成"依赖具体协议的名字和行为"**——`packet.protocol == "http"` 这一行代码,让 Framework 知道了"http 该怎么处理"这个属于 Plugin 的 Policy,第 5 节画的 `Framework -> Plugin Interface <- Plugin A/B/C` 依赖反转结构在这里彻底崩塌,变回了本文档第 5 节开头画的 `Application -> Concrete Implementation` 直接依赖——只是这里连"Application"这一层都省了,Framework 自己就是那个直接依赖具体实现的角色。
3. **这和完全不用 Plugin 系统没有任何区别**——如果 Framework 最终还是要在自己的源码里为每一种协议写一段专属逻辑,那么引入 `register_handler`/`plugins/` 目录/契约校验这一整套机制的意义就消失了,还不如直接写一个没有任何插件概念的、纯粹靠 `if/elseif` 分发的函数——这正是"选错 Pattern 不是写法落后,而是形状和职责不匹配"(`00-mental-model-overview.md` 第 2 节的说法)在 Plugin Pattern 上的具体体现:看起来"装了一个插件系统的壳子",内核却和没有插件系统完全一样。

对比本文档三份真实的 `framework.lua`——它们的 `dispatch`/`execute_command` 内部永远只出现"查 registry"、"调用 `plugin.run/handle/can_handle/dissect`"这两类操作,不出现任何具体插件名字的字符串比较,这正是区分"真正的 Plugin Pattern"和"套了壳子的 Anti-pattern"的可执行标准。
