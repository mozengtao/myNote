# 04-1 · 示例：Transport Factory

> Pattern：Factory Module（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/01-transport-factory/](code/01-transport-factory/)（`transport/tcp.lua` / `transport/udp.lua` / `transport/mock.lua` / `factory.lua` / `app.lua`）。

## 1. Problem / Scenario

一个网络客户端 Application 需要根据配置在多种传输方式之间切换：生产环境用真实的 `tcp`，某些低可靠性场景用 `udp`，写单元测试时用不发真实网络包的 `mock`。三种传输方式的调用方式应该完全一致——都能 `:send(data)`、都能 `:close()`——区别只在于内部到底怎么处理这次调用。

如果 Application 直接 `require` 三个具体模块、自己写 `if kind == "tcp" then ... elseif ...`，就会退化成本文 [00-why-and-architecture.md](00-why-and-architecture.md#anti-pattern) 里描述的 Anti-pattern：新增一种 transport 要改 Application、Application 要记住每个具体模块的构造函数名、测试 Application 时所有具体实现都必须能被加载。这个示例展示如何用 Factory 消除这三个问题。

## 2. Architecture

```text
Caller (app.lua)
   |
   v
Factory (factory.lua)
   |
   +-- transport/tcp.lua   (TcpClient)
   +-- transport/udp.lua   (UdpClient)
   +-- transport/mock.lua  (MockClient)
```

三个具体 transport 模块彼此互不 `require`、互不知道对方存在，只是分别实现了同一套契约（Abstraction）：`:send(data)` / `:close()`。`app.lua` 只 `require("factory")`，统一入口是：

```text
local client = factory.create(kind, config)
```

`kind` 换成 `"tcp"`/`"udp"`/`"mock"`，`app.lua` 里的调用代码一个字都不变。

## 3. Framework / Caller 代码

```lua
local factory = require("factory")

local tcp_client, err = factory.create("tcp", { host = "10.0.0.5", port = 9000 })
```

`app.lua` 扮演 Caller/Application 的角色，全程只 `require("factory")`。它不知道、也不需要知道 `factory.create("tcp", ...)` 最终转发给了哪个具体模块的哪个构造函数——它只知道自己拿到的返回值一定满足 `:send()`/`:close()` 这套契约。

## 4. Module 代码

Factory 本身（省略具体 transport 实现，见下一节完整代码）：

```lua
local TcpClient = require("transport.tcp")
local UdpClient = require("transport.udp")
local MockClient = require("transport.mock")

local M = {}

local creators = {
    tcp = TcpClient.new,
    udp = UdpClient.new,
    mock = MockClient.new,
}

function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown transport kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
```

三个具体 transport 各自是一个 Pattern 3（Class / Constructor）风格的小模块——`tcp.lua`/`udp.lua`/`mock.lua` 各自 `local X = {}; X.__index = X; function X.new(config) ... end`，Factory 只是把它们的 `.new` 函数收进一张选择表。

## 5. 完整 Lua 代码

与磁盘上的文件完全一致。

`code/01-transport-factory/transport/tcp.lua`：

```lua
-- transport/tcp.lua
--
-- Class/Constructor Module（复用 Pattern 3 的写法）：TcpClient。
-- 这个文件只负责"一个 TCP 连接对象长什么样"，完全不知道
-- Factory、Application 的存在，也不知道还有 udp/mock 这两个兄弟实现。
--
-- 对外暴露的是传输层的统一契约（Abstraction）：
--   :send(data)  -> true | (nil, err)
--   :close()     -> true | (nil, err)
-- Factory 正是靠"所有 transport 都满足这个方法形状"才能互换使用它们。

local TcpClient = {}
TcpClient.__index = TcpClient

--- new(config) -> TcpClient 实例
-- config: { host = string, port = number }
function TcpClient.new(config)
    config = config or {}
    local self = setmetatable({}, TcpClient)
    self.host = config.host or "127.0.0.1"
    self.port = config.port or 0
    self.closed = false
    self.sent_count = 0
    return self
end

function TcpClient:send(data)
    if self.closed then
        return nil, "tcp: cannot send on a closed connection"
    end
    self.sent_count = self.sent_count + 1
    print(string.format("  [tcp %s:%d] send #%d: %s",
        self.host, self.port, self.sent_count, tostring(data)))
    return true
end

function TcpClient:close()
    if self.closed then
        return nil, "tcp: already closed"
    end
    self.closed = true
    print(string.format("  [tcp %s:%d] closed (sent %d message(s))",
        self.host, self.port, self.sent_count))
    return true
end

return TcpClient
```

`code/01-transport-factory/transport/udp.lua`：

```lua
-- transport/udp.lua
--
-- Class/Constructor Module：UdpClient。
-- 与 tcp.lua 是平级的独立文件，互不 require，互不知道对方存在。
-- 只是恰好实现了同一套契约：:send(data) / :close()。

local UdpClient = {}
UdpClient.__index = UdpClient

--- new(config) -> UdpClient 实例
-- config: { host = string, port = number }
function UdpClient.new(config)
    config = config or {}
    local self = setmetatable({}, UdpClient)
    self.host = config.host or "0.0.0.0"
    self.port = config.port or 0
    self.closed = false
    self.sent_count = 0
    return self
end

function UdpClient:send(data)
    if self.closed then
        return nil, "udp: cannot send on a closed socket"
    end
    self.sent_count = self.sent_count + 1
    print(string.format("  [udp %s:%d] datagram #%d: %s",
        self.host, self.port, self.sent_count, tostring(data)))
    return true
end

function UdpClient:close()
    if self.closed then
        return nil, "udp: already closed"
    end
    self.closed = true
    print(string.format("  [udp %s:%d] closed (sent %d datagram(s))",
        self.host, self.port, self.sent_count))
    return true
end

return UdpClient
```

`code/01-transport-factory/transport/mock.lua`：

```lua
-- transport/mock.lua
--
-- Class/Constructor Module：MockClient。
-- 用于测试场景：不发真实网络包，只把 send() 的内容记录到内存里的
-- self.log，供测试代码事后断言。同样满足 :send(data) / :close() 契约，
-- 所以 Factory/Application 完全不需要知道它和 tcp/udp 的实现差异。

local MockClient = {}
MockClient.__index = MockClient

--- new(config) -> MockClient 实例
-- config: 当前示例未使用任何字段，但保留同样的入参形状，
-- 让三个 transport 的构造函数签名保持一致。
function MockClient.new(config)
    local self = setmetatable({}, MockClient)
    self.closed = false
    self.log = {}
    return self
end

function MockClient:send(data)
    if self.closed then
        return nil, "mock: cannot send on a closed client"
    end
    self.log[#self.log + 1] = data
    print(string.format("  [mock] recorded message #%d: %s", #self.log, tostring(data)))
    return true
end

function MockClient:close()
    if self.closed then
        return nil, "mock: already closed"
    end
    self.closed = true
    print(string.format("  [mock] closed (recorded %d message(s) total)", #self.log))
    return true
end

--- get_log() -> table
-- 契约之外的额外能力：只有测试代码知道 MockClient 有这个方法，
-- Factory/Application 不会调用它,所以不破坏"统一契约"这个前提。
function MockClient:get_log()
    return self.log
end

return MockClient
```

`code/01-transport-factory/factory.lua`：

```lua
-- factory.lua
--
-- Factory Module：唯一知道"到底有哪些具体 transport 实现"的地方。
-- 它 require 了全部三个具体实现模块——这是必要的、集中的耦合，
-- 换来的是 Application 永远不需要知道 transport.tcp / transport.udp /
-- transport.mock 这几个模块名的存在。
--
-- Factory 本身不持有任何跨调用的 State：creators 表在 require 时
-- 构造一次就不再变化，M.create() 每次调用都是一次独立的、无副作用的
-- "查表 + 转发"，State 全部转移到了它创建出来的 transport 对象身上。

local TcpClient = require("transport.tcp")
local UdpClient = require("transport.udp")
local MockClient = require("transport.mock")

local M = {}

-- creators：kind 字符串 -> 具体实现的构造函数。
-- 新增一种 transport 时，只需要在这里加一行,不需要改 Application。
local creators = {
    tcp = TcpClient.new,
    udp = UdpClient.new,
    mock = MockClient.new,
}

--- create(kind, config) -> transport 实例 | (nil, err)
-- kind 未知时返回 nil + 错误信息,不抛出不可预期的 error()。
function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown transport kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
```

`code/01-transport-factory/app.lua`：

```lua
-- app.lua
--
-- Application：全程只 require("factory")。
-- 这个文件里搜索不到任何一个具体 transport 实现模块的路径字符串——
-- 这正是 Factory 存在的意义: Application 只认识"创建一个 kind"这件事,
-- 不认识具体是哪个模块。

local factory = require("factory")

print("== 通过 Factory 创建 tcp 客户端 ==")
local tcp_client, err = factory.create("tcp", { host = "10.0.0.5", port = 9000 })
if not tcp_client then
    print("  创建失败: " .. err)
else
    tcp_client:send("hello-over-tcp")
    tcp_client:send("second-message")
    tcp_client:close()
end

print("\n== 通过 Factory 创建 mock 客户端（测试场景）==")
local mock_client, mock_err = factory.create("mock", {})
if not mock_client then
    print("  创建失败: " .. mock_err)
else
    mock_client:send("test-payload-1")
    mock_client:send("test-payload-2")
    mock_client:close()
end

print("\n== 请求一个未知的 transport kind ==")
local bad_client, bad_err = factory.create("websocket", {})
if not bad_client then
    print("  预期的失败: " .. bad_err)
else
    print("  意外：websocket 竟然创建成功了")
end
```

## 6. Execution

```bash
cd code/01-transport-factory
lua5.4 app.lua
```

实际执行输出（已用本地 Lua 5.4 验证）：

```text
== 通过 Factory 创建 tcp 客户端 ==
  [tcp 10.0.0.5:9000] send #1: hello-over-tcp
  [tcp 10.0.0.5:9000] send #2: second-message
  [tcp 10.0.0.5:9000] closed (sent 2 message(s))

== 通过 Factory 创建 mock 客户端（测试场景）==
  [mock] recorded message #1: test-payload-1
  [mock] recorded message #2: test-payload-2
  [mock] closed (recorded 2 message(s) total)

== 请求一个未知的 transport kind ==
  预期的失败: factory: unknown transport kind 'websocket'
```

## 7. Execution Flow

1. `require("factory")` 执行 `factory.lua`，它内部先 `require("transport.tcp")`、`require("transport.udp")`、`require("transport.mock")`，三个具体模块全部被载入内存并缓存进 `package.loaded`；随后构造 `creators` 选择表，`return M`。
2. `factory.create("tcp", {host=..., port=...})` 被调用：在 `creators` 表里查到 `creators.tcp == TcpClient.new`，把 `config` 转发给它，`TcpClient.new` 按 Pattern 3 的流程 `setmetatable({}, TcpClient)`、写入 `self.host`/`self.port`/`self.sent_count`，返回新对象。
3. `app.lua` 拿到 `tcp_client`，依次调用 `:send(...)` 两次、`:close()` 一次——这三次调用完全按方法查找规则（`__index -> TcpClient`）执行，Factory 已经在第 2 步交出返回值、不再参与。
4. 对 `"mock"` 重复同样的流程，`factory.create` 转发给 `MockClient.new`，得到一个把消息记进 `self.log` 而不是打印到网络的对象。
5. 对 `"websocket"` 调用 `factory.create`：在 `creators` 表里查不到这个 key，`if not creator then` 命中，直接 `return nil, "factory: unknown transport kind 'websocket'"`，没有创建任何对象。

## 8. State

Factory 本身（`factory.lua`）没有 State——`creators` 表在 `require` 时构造一次，之后只被读取、从不被修改，`M.create()` 每次调用的结果只取决于传入的 `kind`/`config`，不依赖任何"上一次调用"留下的痕迹。

State 全部在被创建出来的 `TcpClient`/`UdpClient`/`MockClient` 对象身上，且每个实例互不共享：`tcp_client.sent_count` 独立于 `mock_client` 内部的 `self.log`，两者是两张完全不同的表。这一层 State 的具体归属规则见 [03-class-constructor/00-why-and-architecture.md](../03-class-constructor/00-why-and-architecture.md#7-state)。

## 9. Why this Pattern fits

- 三种 transport 需要在运行时按配置切换，而不是在写代码时就固定死用哪一种——这正是 Factory 要解决的"隐藏创建决策"问题。
- 三种 transport 的调用方式（`:send()`/`:close()`）完全一致，具备了套用统一契约的前提；如果三者方法形状都不一样，Factory 也无法把它们塞进同一张 `creators` 表里互换使用。
- 测试场景（`mock`）和生产场景（`tcp`/`udp`）需要用完全相同的 Application 代码验证，Factory 让"切换实现"变成只改一个字符串参数，不需要为测试单独写一套调用逻辑。

## 10. What would go wrong with another Pattern

- **只用 Class / Constructor（不叠加 Factory）**：`app.lua` 必须自己 `require("transport.tcp")`、`require("transport.udp")`、`require("transport.mock")`，自己写 `if kind == "tcp" then ... end` 选择逻辑——退化成本文开头描述的、[00-why-and-architecture.md](00-why-and-architecture.md#anti-pattern) 里展开分析的 Anti-pattern。
- **换成 Private+Public**：Private+Public 解决的是"一个 Module 内部有需要隐藏的实现步骤"，但这里的问题是"在多个平级的、完整的实现之间选择"，不是"隐藏单个 Module 内部的步骤"，套用会文档说不清楚三个 transport 之间是什么关系。
- **换成 Plugin**：如果三种 transport 的集合是固定的、不需要被第三方在编译期之外动态扩展，Plugin 反而引入了不必要的"注册/发现"机制；Plugin 更适合"未来可能有人添加一种 Factory 源码里完全没预料到的新 transport"的场景，见 Pattern 5。

## 11. Mental Model

> `factory.create(kind, config)` 就是一个"传送带分岔口"：Application 把 `kind` 这张标签贴在包裹上放上传送带，Factory 看一眼标签就把包裹转发到对应的产线（`TcpClient`/`UdpClient`/`MockClient`），Application 从始至终不需要知道这条传送带后面到底连了几条产线、每条产线内部长什么样。
