# 03 · 示例3：Wireshark-like Dissector Plugin

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)、[01-example-command-plugin.md](01-example-command-plugin.md)、[02-example-protocol-handler-plugin.md](02-example-protocol-handler-plugin.md)。本例模拟一个简化版的 Wireshark 风格 Packet Dissector Framework:和前两个示例"按一个 key 直接查表"不同,这里 Framework 不知道该把 packet_context 交给谁,要遍历所有插件、逐个"问一遍"才能确定。

## 1. Problem / Scenario

想象一个报文解析引擎:它收到的报文可能属于任意协议,引擎本身不想、也不能提前知道"这个字节序列属于哪种协议"——它只能把报文交给一组已注册的解析器(Dissector),让每个解析器自己判断"这个报文是不是我认识的类型"。谁先声明"我能处理",谁就拿到解析权。这正是真实的 Wireshark Dissector 机制的核心思路(参见 `lua-patterns-mastery/05-plugin/02-example-wireshark-plugin.md` 对真实 Wireshark 生态的介绍),本例用一个足够小的 Lua 原型复现同样的分发方式。

## 2. Architecture

```text
                 Framework (framework.lua)
                    |
                    | defines contract: { name, can_handle(ctx), dissect(ctx) }
                    v
              Plugin Interface
                    ^
                    | implements
              +---------+---------+
              |         |
        http_plugin  dns_plugin
```

和前两个示例最大的结构差异:`registry` 从"按 key 直接查表"变成一个有序数组,`dispatch` 内部要遍历这个数组,依次调用每个插件的 `can_handle(ctx)`,第一个返回 `true` 的插件才会被调用 `dissect(ctx)`。

## 3. Framework

契约要求插件必须同时具备 `name`、`can_handle(ctx)`、`dissect(ctx)` 三个字段:

```20:33:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/03-dissector-plugin/framework.lua
local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.can_handle) ~= "function" then
        return false, "plugin.can_handle must be a function"
    end
    if type(plugin.dissect) ~= "function" then
        return false, "plugin.dissect must be a function"
    end
    return true
end
```

`dispatch` 是"遍历 + 询问 + 认领"分发方式的完整代码:

```65:75:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/03-dissector-plugin/framework.lua
function Framework.dispatch(packet_context)
    for _, plugin in ipairs(registry) do
        if plugin.can_handle(packet_context) then
            local result = plugin.dissect(packet_context)
            return plugin.name, result
        end
    end
    return nil, "dispatch: no dissector claimed this packet_context"
end
```

## 4. Module（两个 Dissector Plugin）

- `http_plugin`:`can_handle` 判断 `ctx.port` 是否为 `80` 或 `8080`;`dissect` 打印并返回一句"按 HTTP 解析"的描述。
- `dns_plugin`:`can_handle` 判断 `ctx.port` 是否为 `53`;`dissect` 打印并返回一句"按 DNS 解析"的描述。

两个插件的 `can_handle` 判断条件完全独立,Framework 从不参与"什么条件算能处理"这个判断逻辑——它只是按注册顺序,把这个判断权交给每一个插件自己:

```12:19:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/03-dissector-plugin/plugins/http_plugin.lua
function M.can_handle(ctx)
    return ctx.port == 80 or ctx.port == 8080
end

function M.dissect(ctx)
    local msg = string.format("parsed %d byte(s) on port %d as HTTP", #ctx.bytes, ctx.port)
    print("[http_plugin] " .. msg)
    return msg
end
```

## 5. Complete Lua Code

`framework.lua`:

```lua
-- framework.lua
--
-- Dissector Plugin 的 Framework：不像 01/02 那样用"名字直接查表"来
-- 分发,这里 Framework 事先并不知道一个 packet_context 该交给谁——
-- 它遍历所有已注册的插件,依次调用每个插件自己的 can_handle(ctx),
-- 第一个返回 true 的插件获得 dissect(ctx) 的调用权。这是"插件数量、
-- 身份、顺序对 Framework 完全不可见"的另一种落地方式：Framework 从
-- 不知道、也不需要知道到底有几个 dissector、它们叫什么名字。
--
-- Contract：一个合法的 Dissector Plugin 必须是一个 table，至少包含：
--   name              : string，插件名（日志/自描述用）
--   can_handle(ctx)   : function，返回 true/false，表示"我能不能
--                        认领这个 packet_context"
--   dissect(ctx)      : function，真正的解析行为，只有在
--                        can_handle(ctx) 返回 true 之后才会被调用

local Framework = {}

-- registry：这里用数组而不是像 01/02 那样用按名字建的 hash 表,
-- 因为"谁能处理"这件事本身需要在运行时按注册顺序逐个询问插件,
-- 不是靠一个固定 key 直接查出来的。这是 Framework-level State。
local registry = {}

local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.can_handle) ~= "function" then
        return false, "plugin.can_handle must be a function"
    end
    if type(plugin.dissect) ~= "function" then
        return false, "plugin.dissect must be a function"
    end
    return true
end

--- Framework.register_dissector(plugin)
-- Register 步骤：校验契约 + 检查重名 + 追加进 registry 数组末尾
-- （数组顺序 = 询问 can_handle 的顺序）。
function Framework.register_dissector(plugin)
    local ok, err = validate_plugin(plugin)
    if not ok then
        error("register_dissector: invalid plugin: " .. err)
    end

    for _, existing in ipairs(registry) do
        if existing.name == plugin.name then
            error("register_dissector: dissector '" .. plugin.name .. "' already registered")
        end
    end

    registry[#registry + 1] = plugin
    return true
end

--- Framework.unregister_dissector(name) -> ok, err
function Framework.unregister_dissector(name)
    for i, plugin in ipairs(registry) do
        if plugin.name == name then
            table.remove(registry, i)
            return true
        end
    end
    return false, "unregister_dissector: dissector '" .. tostring(name) .. "' not registered"
end

--- Framework.dispatch(packet_context) -> name, result  或  nil, err
-- Invoke / Event 步骤：依次询问每个已注册插件的 can_handle(ctx)，
-- 第一个回答 true 的插件拿到 dissect(ctx) 的调用权，其结果连同该
-- 插件的 name 一起返回。如果没有任何插件认领，优雅地返回
-- (nil, err)，而不是抛异常或崩溃——对应 00-why-and-architecture.md
-- Failure 一节里"没有插件认领"这个分支。
function Framework.dispatch(packet_context)
    for _, plugin in ipairs(registry) do
        if plugin.can_handle(packet_context) then
            local result = plugin.dissect(packet_context)
            return plugin.name, result
        end
    end
    return nil, "dispatch: no dissector claimed this packet_context"
end

function Framework.list_dissectors()
    local names = {}
    for _, plugin in ipairs(registry) do
        names[#names + 1] = plugin.name
    end
    return names
end

return Framework
```

`plugins/http_plugin.lua`:

```lua
-- http_plugin.lua
--
-- HTTP 的 Dissector Plugin：can_handle 只关心"这个 packet_context 是
-- 不是我认识的类型"（这里简化成按端口号判断），dissect 才是真正的
-- 解析行为。两者都是插件自己的 Policy，Framework 只负责按顺序调用
-- can_handle 直到有人认领。

local M = {
    name = "http",
}

function M.can_handle(ctx)
    return ctx.port == 80 or ctx.port == 8080
end

function M.dissect(ctx)
    local msg = string.format("parsed %d byte(s) on port %d as HTTP", #ctx.bytes, ctx.port)
    print("[http_plugin] " .. msg)
    return msg
end

return M
```

`plugins/dns_plugin.lua`:

```lua
-- dns_plugin.lua
--
-- DNS 的 Dissector Plugin，满足和 http_plugin.lua 相同的契约
-- （name + can_handle + dissect），但认领条件和解析逻辑完全不同——
-- Framework 的 dispatch() 不需要知道这个区别，只需要依次调用
-- can_handle 就能让正确的插件认领正确的 packet_context。

local M = {
    name = "dns",
}

function M.can_handle(ctx)
    return ctx.port == 53
end

function M.dissect(ctx)
    local msg = string.format("parsed %d byte(s) on port %d as DNS", #ctx.bytes, ctx.port)
    print("[dns_plugin] " .. msg)
    return msg
end

return M
```

`app.lua`:

```lua
-- app.lua
--
-- Application：require framework + 两个 dissector 插件，注册后构造
-- 几个"假 packet_context"（普通 Lua table，带 bytes/port 字段），
-- 逐个交给 framework.dispatch(...)。最后额外 dispatch 一个两个插件
-- 都无法认领的 context，验证 Framework 优雅地返回失败。

local framework = require("framework")

local http_plugin = require("plugins.http_plugin")
local dns_plugin = require("plugins.dns_plugin")

print("== Register 两个 dissector 插件 ==")
framework.register_dissector(http_plugin)
framework.register_dissector(dns_plugin)
print("已注册 dissector: " .. table.concat(framework.list_dissectors(), ", "))

local contexts = {
    { bytes = "GET / HTTP/1.1\r\n", port = 80 },
    { bytes = "\x00\x01\x01\x00example.com", port = 53 },
    { bytes = "\x16\x03\x01handshake", port = 443 }, -- 没有插件能认领
}

print("\n== Dispatch 每一个 packet_context ==")
for i, ctx in ipairs(contexts) do
    local name, result = framework.dispatch(ctx)
    if not name then
        print(string.format("context #%d (port=%d) -> %s", i, ctx.port, result))
    else
        print(string.format("context #%d (port=%d) -> claimed by '%s': %s", i, ctx.port, name, result))
    end
end
```

## 6. Execution Flow

实际运行(`cd code/03-dissector-plugin && lua5.4 app.lua`)的输出:

```text
== Register 两个 dissector 插件 ==
已注册 dissector: http, dns

== Dispatch 每一个 packet_context ==
[http_plugin] parsed 16 byte(s) on port 80 as HTTP
context #1 (port=80) -> claimed by 'http': parsed 16 byte(s) on port 80 as HTTP
[dns_plugin] parsed 15 byte(s) on port 53 as DNS
context #2 (port=53) -> claimed by 'dns': parsed 15 byte(s) on port 53 as DNS
context #3 (port=443) -> dispatch: no dissector claimed this packet_context
```

逐步拆解:

1. `register_dissector` 被调用两次,`http_plugin` 先进 `registry` 数组下标 1,`dns_plugin` 后进,下标 2——这个顺序在本例里不影响结果(两个插件的 `can_handle` 条件互斥),但如果两个插件的判断条件有重叠,注册顺序会决定谁先拿到认领权,这是数组式 registry 相比按 key 查表的 registry 多出来的一个需要显式考虑的因素。
2. 第 1 个 context 的 `port` 是 `80`:`dispatch` 先问 `http_plugin.can_handle(ctx)`,返回 `true`,立即调用 `http_plugin.dissect(ctx)`,不再继续问 `dns_plugin`。
3. 第 2 个 context 的 `port` 是 `53`:`dispatch` 先问 `http_plugin.can_handle(ctx)`,返回 `false`,继续问 `dns_plugin.can_handle(ctx)`,返回 `true`,调用 `dns_plugin.dissect(ctx)`。
4. 第 3 个 context 的 `port` 是 `443`:两个插件的 `can_handle` 都返回 `false`,循环走完整个 `registry` 都没有命中,`dispatch` 返回 `nil, err`,`app.lua` 打印"no dissector claimed"提示,程序正常继续(没有第 4 个 context 了,循环自然结束)。

## 7. State

- **Plugin-level State**:本例的两个插件都没有维护跨调用的可变状态(`can_handle`/`dissect` 都是纯函数式的,相同输入产生相同输出),这是刻意的简化——如果需要,完全可以像 Example 2 的 `mqtt_handler` 一样加一个内部计数器,不会影响 Framework 的任何代码。
- **Framework-level State**:`registry` 这个数组,记录当前有哪些 dissector、以及它们被询问的顺序。这份 State 由 `register_dissector`/`unregister_dissector` 维护,`dispatch` 只读取它,不修改它。
- 和前两个示例的共同结论:Framework-level State(注册了哪些插件)与 Plugin-level State(某个插件内部维护了什么)是完全独立的两层,即便本例的 Plugin-level State 是空的,这个区分本身依然成立且必须成立——"当前没有状态"不等于"这一层概念不存在"。

## 8. 为什么这个场景更适合 Plugin Architecture,而不是普通的 Utility Module

假设有人提出"不需要 Plugin 这么复杂,写一个 Utility Module 就够了"——比如一个 `dissect_utils.lua`,里面放 `dissect_utils.parse_http(ctx)`、`dissect_utils.parse_dns(ctx)` 两个函数。这个提议在本场景下站不住脚,原因有三点,都能直接对照代码验证:

1. **多个 dissector 必须共存,并且每个都要有机会认领报文**——`can_handle` 这个函数本身就是"认领"这个动作的载体,一个 Utility Module 里的普通函数(`parse_http`)没有"我要不要认领这个报文"的概念,它只会在被调用时无条件执行。本例的 `Framework.dispatch` 依赖的正是"遍历 + 询问"这个机制,如果换成 Utility Module,这个机制无处安放。
2. **Framework 事先不知道、也不需要知道有多少个/哪些 dissector 存在**——`registry` 数组的长度和内容完全由运行时的 `register_dissector` 调用决定,`framework.lua` 本身从头到尾没有出现 `http`/`dns` 这两个字符串。Utility Module 场景下,调用者必须显式知道"这个报文该调用 `parse_http` 还是 `parse_dns`"——这份知识必须写在某个地方,而 Utility Module 自己不提供"根据报文特征自动选择该调用哪个函数"的机制。
3. **一个 Utility Module 会要求 Framework 硬编码"哪种报文该调用哪个工具函数"**——如果坚持用 `dissect_utils.lua` 这种形式,`dispatch` 就必须写成:

    ```lua
    -- 假设 dispatch 试图配合一个 Utility Module 使用
    function Framework.dispatch(ctx)
        if ctx.port == 80 or ctx.port == 8080 then
            return "http", dissect_utils.parse_http(ctx)
        elseif ctx.port == 53 then
            return "dns", dissect_utils.parse_dns(ctx)
        else
            return nil, "no dissector claimed this packet_context"
        end
    end
    ```

    这正是 [00-why-and-architecture.md](00-why-and-architecture.md) 末尾 Anti-pattern 章节展示的写法——Framework 重新变成了认识每一种具体协议名字和判断条件的角色,新增一种协议(比如 `mqtt`)要求同时编辑 `dissect_utils.lua`(加函数)和这段 `dispatch`(加分支)。对比本例真实的 `Framework.dispatch`,它的循环体永远只有 `plugin.can_handle(ctx)` 和 `plugin.dissect(ctx)` 两次调用,不出现任何具体协议的名字或端口号——这就是 Plugin Architecture 和"看起来省事但退化成 Anti-pattern 的 Utility Module 用法"之间可验证的分界线。

## 9. What would go wrong with another Pattern

如果用 Factory Pattern 替代:工厂内部需要一个"根据 `ctx.port` 判断该创建/调用哪个 dissector"的映射或判断逻辑,新增一种协议依然要修改工厂内部的这份逻辑——工厂的"选择哪个已知实现"式设计,天然假设"有几种实现"在编写工厂时就已知,这与本例"Framework 不知道有多少个/哪些 dissector"的前提直接冲突。如果用 Class/Constructor Pattern 把每个 dissector 写成一个"类",本质上只是把 `plugins/*.lua` 换成了"每次 `dispatch` 都 `new` 一个对象再调用方法"——徒增一层构造成本,并不能解决"Framework 如何在不知道具体协议的情况下找到该由谁处理"这个核心问题,`can_handle`/`dissect` 这套契约依然需要存在,只是被套上了一层不必要的 Object 外壳。

## 10. Mental Model

```text
Framework(framework.lua)
    |
    | 遍历 registry,依次询问
    v
plugin.can_handle(ctx) -> true/false
    |
    | 第一个返回 true 的插件
    v
plugin.dissect(ctx) -> result
```

一句话记忆:**Framework 拥有"遍历 + 询问 + 认领"这套分发协议,Plugin 拥有"我能不能处理"和"怎么处理"这两个具体判断/行为**——`Framework.dispatch` 是前一半的完整代码,`http_plugin.can_handle` + `http_plugin.dissect` 是后一半的完整代码。这也是本文档要证明的核心论点在第三个示例里的最后一次落地:Plugin Pattern 的本质始终是 Contract + Discovery/Registration + Invocation,不是 `require()`——本例的 `require("plugins.http_plugin")`/`require("plugins.dns_plugin")` 只完成了 Load,真正让"插件"成立的是 `can_handle`/`dissect` 这份契约,以及 `register_dissector`/`dispatch` 这套注册与调用机制。
