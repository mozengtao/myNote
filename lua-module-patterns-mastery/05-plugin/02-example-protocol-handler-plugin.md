# 02 · 示例2：Protocol Handler Plugin

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)、[01-example-command-plugin.md](01-example-command-plugin.md)。本例把 Example 1 的"按命令名分发"换成"按协议名分发",并且引入一个纯数据的 Packet 概念,展示 Plugin Pattern 在"分发一个结构化事件"场景下的样子。

## 1. Problem / Scenario

想象一个网络中间件,需要根据收到的报文属于哪种协议,把报文转交给对应的解析逻辑。协议的种类不是中间件作者一次性能穷举完的(今天是 HTTP/DNS/MQTT,明天可能要加 SIP、CoAP……),中间件本身也不应该为每一种协议写一段专属代码——这正是 Example 1"命令"换成"协议"之后的同构问题,但这里多了一个明确的"事件对象"(Packet),值得单独画一条 Data/Control Flow。

## 2. Architecture

```text
Packet
   |
   v
Framework Dispatcher
   |
   v
Plugin Handler
```

展开成完整的三层结构:

```text
                 Framework (framework.lua)
                    |
                    | defines contract: { name, handle(packet) }
                    v
              Plugin Interface
                    ^
                    | implements
          +---------+---------+---------+
          |         |         |
    http_handler  dns_handler  mqtt_handler
```

`framework.lua` 维护一张"协议名 -> handler 插件"的注册表;三个 `plugins/*.lua` 各自实现一种协议的处理逻辑;`app.lua` 构造若干"假 packet"(带 `protocol` 字段的普通 table),逐个交给 Framework 分发。

## 3. Framework

Framework 提供 `register_handler`/`unregister_handler`/`dispatch`/`list_protocols` 四个函数,契约要求插件必须有 `name` 和 `handle(packet)`:

```25:34:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/02-protocol-handler-plugin/framework.lua
local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.handle) ~= "function" then
        return false, "plugin.handle must be a function"
    end
    return true
end
```

`dispatch` 是本例的核心:它只读 `packet.protocol` 这一个字段来查表,查到就调用,查不到就返回错误——**这就是"Framework 拥有协议分发协议(Dispatch Protocol)"的完整代码**:

```63:76:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/02-protocol-handler-plugin/framework.lua
function Framework.dispatch(packet)
    if type(packet) ~= "table" or type(packet.protocol) ~= "string" then
        return nil, "dispatch: packet must be a table with a string 'protocol' field"
    end

    local plugin = registry[packet.protocol]
    if not plugin then
        return nil, "dispatch: no handler registered for protocol '" .. packet.protocol .. "'"
    end

    return plugin.handle(packet)
end
```

## 4. Module（三个 Handler Plugin）

- `http_handler`:从 `packet` 里读 `method`/`path`/`payload`,拼一句 HTTP 风格的描述。
- `dns_handler`:从 `packet` 里读 `query`,拼一句 DNS 查询描述。
- `mqtt_handler`:从 `packet` 里读 `topic`/`payload`,并维护一个 Plugin-level 的 `message_count`,每处理一条消息自增一次。

**每个插件拥有具体的协议解析行为(Protocol Behavior)**——`mqtt_handler.lua` 的 `handle` 是这一半的完整代码:

```12:22:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/02-protocol-handler-plugin/plugins/mqtt_handler.lua
function M.handle(packet)
    message_count = message_count + 1
    local topic = packet.topic or "unknown"
    local msg = string.format(
        "MQTT publish #%d on topic '%s' (payload=%s)",
        message_count, topic, tostring(packet.payload)
    )
    print("[mqtt_handler] " .. msg)
    return msg
end
```

Framework 的 `dispatch` 从不知道 `message_count` 存在,也从不知道"MQTT 报文应该读 `topic` 字段"——它只知道"这是 `mqtt` 协议对应的 handler,把 packet 转发给它的 `handle` 函数"。

## 5. Complete Lua Code

`framework.lua`:

```lua
-- framework.lua
--
-- Protocol Handler Plugin 的 Framework：维护一张"协议名 -> 插件"的
-- 注册表，dispatch(packet) 只读 packet.protocol 这一个字段，据此从
-- registry 里查出对应的插件并调用它——从不硬编码任何具体协议的
-- 名字或行为。
--
-- Contract：一个合法的 Handler Plugin 必须是一个 table，至少包含：
--   name           : string，协议名（用于日志/自描述）
--   handle(packet) : function，处理一个属于这个协议的 packet
--
-- 关键约束：这个文件永远不会出现
--     if packet.protocol == "http" then ... elseif packet.protocol == "dns" then ...
-- 这种按协议名分支的代码。新增一个协议 handler，只需要新加一个
-- plugins/*.lua 文件并在 app.lua 里注册，不需要改动这个文件。

local Framework = {}

-- registry：Framework-level State，key = 协议名字符串，value = plugin table。
local registry = {}

local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.handle) ~= "function" then
        return false, "plugin.handle must be a function"
    end
    return true
end

--- Framework.register_handler(protocol, plugin)
-- Register 步骤：校验契约 + 检查协议名冲突 + 存进 registry。
function Framework.register_handler(protocol, plugin)
    if type(protocol) ~= "string" or protocol == "" then
        error("register_handler: protocol must be a non-empty string")
    end

    local ok, err = validate_plugin(plugin)
    if not ok then
        error("register_handler: invalid plugin for protocol '" .. protocol .. "': " .. err)
    end

    if registry[protocol] then
        error("register_handler: protocol '" .. protocol .. "' already has a handler")
    end

    registry[protocol] = plugin
    return true
end

--- Framework.unregister_handler(protocol) -> ok, err
function Framework.unregister_handler(protocol)
    if not registry[protocol] then
        return false, "unregister_handler: no handler registered for '" .. tostring(protocol) .. "'"
    end
    registry[protocol] = nil
    return true
end

--- Framework.dispatch(packet) -> result, err
-- Invoke / Event 步骤：每一次 dispatch 调用都是 Framework 把"一个
-- 新事件"（一个 packet）转交给对应插件的 handle 函数去处理。
-- Framework 只负责"按 packet.protocol 查表 + 调用"，从不参与
-- packet 内部字段该怎么解读——那是插件的 Policy。
function Framework.dispatch(packet)
    if type(packet) ~= "table" or type(packet.protocol) ~= "string" then
        return nil, "dispatch: packet must be a table with a string 'protocol' field"
    end

    local plugin = registry[packet.protocol]
    if not plugin then
        return nil, "dispatch: no handler registered for protocol '" .. packet.protocol .. "'"
    end

    return plugin.handle(packet)
end

function Framework.list_protocols()
    local names = {}
    for name in pairs(registry) do
        names[#names + 1] = name
    end
    table.sort(names)
    return names
end

return Framework
```

`plugins/http_handler.lua`:

```lua
-- http_handler.lua
--
-- HTTP 协议的 Handler Plugin。handle() 内部怎么解读 packet 的字段
-- （method/path/payload）完全是这个插件自己的 Policy，Framework 的
-- dispatch() 从不关心这里面具体做了什么，只负责把 packet 转发过来。

local M = {
    name = "http",
}

function M.handle(packet)
    local method = packet.method or "GET"
    local path = packet.path or "/"
    local msg = string.format("HTTP %s %s (payload=%s)", method, path, tostring(packet.payload))
    print("[http_handler] " .. msg)
    return msg
end

return M
```

`plugins/dns_handler.lua`:

```lua
-- dns_handler.lua
--
-- DNS 协议的 Handler Plugin，和 http_handler.lua 一样满足同一份
-- 契约（name + handle），但内部字段读取逻辑完全不同——这正是
-- "Framework 只认契约，不认具体协议"的直接证明。

local M = {
    name = "dns",
}

function M.handle(packet)
    local query = packet.query or "unknown"
    local msg = string.format("DNS query for '%s'", query)
    print("[dns_handler] " .. msg)
    return msg
end

return M
```

`plugins/mqtt_handler.lua`:

```lua
-- mqtt_handler.lua
--
-- MQTT 协议的 Handler Plugin，额外演示 Plugin-level State：
-- message_count 只属于这个插件自己，跨多次 dispatch 持续累加，
-- http_handler/dns_handler 完全看不到、也不受它影响。

local M = {
    name = "mqtt",
}

local message_count = 0

function M.handle(packet)
    message_count = message_count + 1
    local topic = packet.topic or "unknown"
    local msg = string.format(
        "MQTT publish #%d on topic '%s' (payload=%s)",
        message_count, topic, tostring(packet.payload)
    )
    print("[mqtt_handler] " .. msg)
    return msg
end

return M
```

`app.lua`:

```lua
-- app.lua
--
-- Application：require framework + 三个协议 handler 插件，注册后
-- 构造几个"假 packet"（普通 Lua table，带一个 protocol 字段），
-- 逐个交给 framework.dispatch(...)。最后额外 dispatch 一个未知协议
-- 的 packet，验证 Framework 优雅返回错误而不是抛异常。

local framework = require("framework")

local http_handler = require("plugins.http_handler")
local dns_handler = require("plugins.dns_handler")
local mqtt_handler = require("plugins.mqtt_handler")

print("== Register 三个协议 handler ==")
framework.register_handler(http_handler.name, http_handler)
framework.register_handler(dns_handler.name, dns_handler)
framework.register_handler(mqtt_handler.name, mqtt_handler)
print("已注册协议: " .. table.concat(framework.list_protocols(), ", "))

local packets = {
    { protocol = "http", method = "GET", path = "/index.html", payload = "" },
    { protocol = "dns", query = "example.com" },
    { protocol = "mqtt", topic = "sensors/temp", payload = "23.5" },
    { protocol = "mqtt", topic = "sensors/humidity", payload = "48" },
    { protocol = "sip", payload = "INVITE sip:bob@example.com" }, -- 没有注册对应的 handler
}

print("\n== Dispatch 每一个 packet ==")
for i, packet in ipairs(packets) do
    local result, err = framework.dispatch(packet)
    if err then
        print(string.format("packet #%d (protocol=%s) -> dispatch failed: %s", i, packet.protocol, err))
    else
        print(string.format("packet #%d (protocol=%s) -> handled: %s", i, packet.protocol, result))
    end
end
```

## 6. Execution Flow

实际运行(`cd code/02-protocol-handler-plugin && lua5.4 app.lua`)的输出:

```text
== Register 三个协议 handler ==
已注册协议: dns, http, mqtt

== Dispatch 每一个 packet ==
[http_handler] HTTP GET /index.html (payload=)
packet #1 (protocol=http) -> handled: HTTP GET /index.html (payload=)
[dns_handler] DNS query for 'example.com'
packet #2 (protocol=dns) -> handled: DNS query for 'example.com'
[mqtt_handler] MQTT publish #1 on topic 'sensors/temp' (payload=23.5)
packet #3 (protocol=mqtt) -> handled: MQTT publish #1 on topic 'sensors/temp' (payload=23.5)
[mqtt_handler] MQTT publish #2 on topic 'sensors/humidity' (payload=48)
packet #4 (protocol=mqtt) -> handled: MQTT publish #2 on topic 'sensors/humidity' (payload=48)
packet #5 (protocol=sip) -> dispatch failed: dispatch: no handler registered for protocol 'sip'
```

逐步拆解:

1. Load + Register:三个 handler 插件被 `require` 后,依次通过 `register_handler` 存入 `registry`,`list_protocols()` 证明 `dns`/`http`/`mqtt` 三个协议均已在册。
2. 第 1、2 个 packet 分别命中 `http`/`dns`,`dispatch` 查表后把 packet 原样转发给对应的 `handle` 函数——Framework 全程没有读取 `method`/`path`/`query` 这些协议专属字段,这些字段只被对应插件读取。
3. 第 3、4 个 packet 都命中 `mqtt`,`mqtt_handler` 内部的 `message_count` 从 `1` 累加到 `2`,证明这是一个跨 dispatch 持续存在的 Plugin-level State。
4. 第 5 个 packet 的 `protocol` 是 `"sip"`,`registry` 里没有这个 key,`dispatch` 返回 `nil, err`,`app.lua` 打印失败提示,循环继续正常执行,没有中断后续代码。

## 7. State

- **Plugin-level State**:`mqtt_handler.lua` 的 `message_count`,只在处理 `mqtt` 协议的 packet 时才会自增,`http_handler`/`dns_handler` 完全不受影响,也无法读取或修改这个变量。
- **Framework-level State**:`framework.lua` 的 `registry`,记录"协议名 -> handler"的映射,由三次 `register_handler` 调用建立,`dispatch` 只读它、不修改它。
- 这两种 State 的独立性可以直接从输出验证:即使 `dns`/`http` 之间夹着两次 `mqtt` 的 dispatch,`registry` 里的三条映射关系全程没有变化,而 `mqtt_handler` 内部的计数器只在自己被调用时变化。

## 8. Why this Pattern fits

协议分发框架要支持数量和名字都不能提前穷举的"未来协议",这和 Plugin Pattern 要解决的核心问题完全一致。如果用 Factory Pattern,工厂内部必须写一张"协议名 -> handler 构造函数"的映射表,新增协议依然要改这张表;Plugin Pattern 把"注册哪些协议"这个决定权完全交给调用 `register_handler` 的代码(本例是 `app.lua`,真实系统里可能是一次启动时的插件扫描),`framework.lua` 自身永远不需要修改。

## 9. What would go wrong with another Pattern

如果把三种协议的处理逻辑写成一个 Table Module(比如 `handlers.lua` 里 `M.http`/`M.dns`/`M.mqtt` 三个函数),`dispatch` 函数必须变成 `if packet.protocol == "http" then M.http(packet) elseif ... end`——这正是 [00-why-and-architecture.md](00-why-and-architecture.md) 末尾 Anti-pattern 章节展示的写法,新增 `sip` 协议要求同时编辑 `handlers.lua`(加函数)和 `dispatch` 内部的分支链(加条件),两处都是对已有源码的修改,而不是"新增一个文件"。如果用 Factory,同样的问题会出现在工厂内部的映射表上,只是"直接依赖"换成了"通过工厂间接依赖",Framework 本质上依然要认识每一个具体协议的名字。

## 10. Mental Model

```text
Packet { protocol = "..." }
    |
    v
Framework Dispatcher（只认 packet.protocol 这个字段）
    |
    v
Plugin Handler（http_handler / dns_handler / mqtt_handler）
```

一句话记忆:**Framework 拥有协议分发协议(怎么根据 `protocol` 字段找到该由谁处理),Plugin 拥有具体的协议解析行为(找到之后具体怎么处理)**——`Framework.dispatch` 是前一半的完整代码,`mqtt_handler.handle` 是后一半的完整代码。
