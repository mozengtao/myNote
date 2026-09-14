# 01 · 示例1：Command Plugin

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。本例是三个示例里最基础的一个：Framework 维护一张"命令名 -> 插件"的注册表,Application 通过命令名触发调用,不同插件互不知道对方存在。

## 1. Problem / Scenario

想象一个命令行风格的小工具:用户输入一个命令名(`hello`/`status`/`debug`),工具执行对应的行为。工具作者希望:

- 新增一个命令,不需要改工具本身的分发逻辑代码。
- 每个命令的实现细节(要不要维护状态、要打印什么)完全由命令自己决定,工具本身不关心。
- 输入一个不存在的命令名,工具要给出清晰的错误提示,而不是崩溃。

这正是 Command Plugin 要解决的问题:工具本身(Framework)只提供"注册命令"和"执行命令"两个机制,具体命令(Plugin)各自独立实现。

## 2. Architecture

```text
                 Framework (framework.lua)
                    |
                    | defines contract: { name, run(...) }
                    v
              Plugin Interface
                    ^
                    | implements
          +---------+---------+---------+
          |         |         |
    hello_plugin status_plugin debug_plugin
```

`framework.lua` 是 Framework,内部维护一张按命令名索引的 `registry`;`plugins/hello_plugin.lua`、`plugins/status_plugin.lua`、`plugins/debug_plugin.lua` 是三个独立的 Plugin,各自 `return` 一个满足 `{ name, run(...) }` 契约的 table;`app.lua` 是 Application,负责把它们连接起来。

## 3. Framework

Framework 提供两个核心函数,外加一个用于 Unload 的辅助函数和一个只读查询函数:

- `Framework.register_command(name, plugin)` —— Register 步骤:校验 `plugin` 是否满足契约(`validate_plugin`),校验通过后存入 `registry[name]`。
- `Framework.execute_command(name, ...)` —— Invoke 步骤:从 `registry` 查出对应插件,调用它的 `run(...)`,把返回值原样交还调用者;命令不存在时返回 `nil, err`,不抛异常。
- `Framework.unregister_command(name)` —— Unload 步骤:把某个命令从 `registry` 里移除。
- `Framework.list_commands()` —— 只读地列出当前已注册的命令名(排序后返回),不暴露 `registry` 本身。

**Framework 拥有命令协议(Command Protocol),即"一个命令必须有 `name` 和 `run` 两个字段"这件事**——这条规则完整地写在 `validate_plugin` 函数里:

```12:26:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/01-command-plugin/framework.lua
local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.run) ~= "function" then
        return false, "plugin.run must be a function"
    end
    return true
end
```

这个函数从不检查 `plugin.run` 内部的代码写了什么,只检查"形状"——这就是 Framework 侧的契约。

## 4. Module（三个 Plugin）

- `hello_plugin`:`run(who)` 拼一句问候语并打印。
- `status_plugin`:`run()` 不接受参数,内部维护一个 Plugin-level 的 `run_count`,每调用一次自增一次。
- `debug_plugin`:`run(...)` 接受任意数量参数,把它们拼成字符串打印,用于验证可变参数被原样转发。

**每个插件拥有命令行为(Command Behavior),即"这个命令具体做什么"这件事**——`status_plugin.lua` 里的 `run` 就是这一半的完整体现:

```13:19:/home/morrism/myNote/lua-module-patterns-mastery/05-plugin/code/01-command-plugin/plugins/status_plugin.lua
function M.run()
    run_count = run_count + 1
    local msg = string.format("status_plugin has been invoked %d time(s)", run_count)
    print("[status_plugin] " .. msg)
    return run_count
end
```

Framework 的 `execute_command` 从不知道 `run_count` 这个变量存在,只知道"调用 `plugin.run(...)`,拿到返回值"。这就是"Framework 拥有命令协议,Plugin 拥有命令行为"这句话在代码里的具体样子。

## 5. Complete Lua Code

`framework.lua`:

```lua
-- framework.lua
--
-- Plugin Module (Extension / Dependency Inversion)：这是 Command Plugin
-- 示例里的 Framework（Plugin Manager）。它只认识"契约"（Contract）——
-- 任何插件必须是一个 table，至少包含：
--   name        : string，命令名，用于日志/自描述
--   run(...)    : function，执行这个命令时的具体行为
--
-- Framework 的职责边界（严格对照 00-why-and-architecture.md 里的六步）：
--   define contract   -> validate_plugin
--   load plugin       -> 不在这里，由 app.lua 通过 require() 完成
--   validate plugin   -> register_command 内部调用 validate_plugin
--   register plugin   -> register_command 把 plugin 存进 registry
--   invoke plugin     -> execute_command 从 registry 取出 plugin，调用 plugin.run
--   unload plugin     -> unregister_command
--
-- 关键约束：这个文件永远不会出现
--     if name == "hello" then ... elseif name == "status" then ...
-- 这种按插件身份分支的代码。新增一个命令插件，只需要在 plugins/
-- 目录下新加一个文件，在 app.lua 里 require + register，
-- 不需要改动这个文件一行代码。

local Framework = {}

-- registry：Framework 唯一的内部 State —— 当前已注册的插件集合。
-- key = 命令名，value = 插件 table 本身。这是 Framework-level State，
-- 和任何单个插件内部维护的 State 是完全不同的两件事（后者见
-- plugins/status_plugin.lua 的 run_count）。
local registry = {}

--- validate_plugin(plugin) -> ok, err
-- 契约本身的落地：一个 table 要成为合法的 Command Plugin，必须满足
-- 这里检查的形状。这个函数只关心"形状对不对"，完全不关心
-- "这是谁"、"它内部怎么实现"——那是 Plugin 自己的事，Framework 管不到
-- 也不应该管。
local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.run) ~= "function" then
        return false, "plugin.run must be a function"
    end
    return true
end

--- Framework.register_command(name, plugin)
-- Register 步骤：Framework 只做三件事——校验契约、检查命名冲突、
-- 存进 registry。它从不检查 plugin.run 里面的代码具体做了什么。
function Framework.register_command(name, plugin)
    if type(name) ~= "string" or name == "" then
        error("register_command: name must be a non-empty string")
    end

    local ok, err = validate_plugin(plugin)
    if not ok then
        error("register_command: invalid plugin for '" .. name .. "': " .. err)
    end

    if registry[name] then
        error("register_command: command '" .. name .. "' is already registered")
    end

    registry[name] = plugin
    return true
end

--- Framework.unregister_command(name) -> ok, err
-- Unload 步骤：把插件从 registry 里移除。Framework 不负责、也无法
-- 知道插件内部是否还有资源需要清理——如果插件需要清理逻辑，应该由
-- 插件自己的 shutdown 钩子负责（本示例的插件足够简单，不需要）。
function Framework.unregister_command(name)
    if not registry[name] then
        return false, "unregister_command: command '" .. tostring(name) .. "' is not registered"
    end
    registry[name] = nil
    return true
end

--- Framework.execute_command(name, ...) -> result, err
-- Invoke 步骤：Framework 唯一知道如何跟插件打交道的方式，就是调用
-- 它已经校验过的 plugin.run(...)。Framework 不知道、也不需要知道
-- hello_plugin.run 内部具体做了什么，只负责"找到它、按契约调用它、
-- 把返回值原样交还调用者"。
function Framework.execute_command(name, ...)
    local plugin = registry[name]
    if not plugin then
        return nil, "execute_command: unknown command '" .. tostring(name) .. "'"
    end
    return plugin.run(...)
end

--- Framework.list_commands() -> array of names
-- 只读地暴露当前 Framework-level State 的一份快照（已排序，方便打印
-- 稳定输出），不直接暴露 registry 表本身——避免外部代码绕过
-- register_command/unregister_command 直接改写内部 State。
function Framework.list_commands()
    local names = {}
    for name in pairs(registry) do
        names[#names + 1] = name
    end
    table.sort(names)
    return names
end

return Framework
```

`plugins/hello_plugin.lua`:

```lua
-- hello_plugin.lua
--
-- 一个符合 Command Plugin 契约的插件：name + run(...)。run() 内部
-- 具体做什么完全是这个插件自己的 Policy（这里只是拼一句问候语并
-- 打印），Framework 完全不知道、也不关心这里面的实现细节——它只
-- 知道 hello_plugin 有 name 和 run 这两个契约要求的字段。

local M = {
    name = "hello",
}

function M.run(who)
    who = who or "world"
    local msg = "Hello, " .. who .. "!"
    print("[hello_plugin] " .. msg)
    return msg
end

return M
```

`plugins/status_plugin.lua`:

```lua
-- status_plugin.lua
--
-- 演示 Plugin-level State：run_count 是这个插件自己私有的数据，每次
-- 被 Framework.execute_command 调用一次就自增一次。这个变量对
-- Framework 的 registry、对 hello_plugin/debug_plugin 都完全不可见——
-- State 归这个插件自己所有，是"每个 Plugin 独立持有，通常不与其他
-- 插件或 Framework 共享"的具体例子。

local M = {
    name = "status",
}

local run_count = 0

function M.run()
    run_count = run_count + 1
    local msg = string.format("status_plugin has been invoked %d time(s)", run_count)
    print("[status_plugin] " .. msg)
    return run_count
end

return M
```

`plugins/debug_plugin.lua`:

```lua
-- debug_plugin.lua
--
-- 第三个独立插件，run() 接受任意数量的参数并把它们打印出来，用于
-- 验证 Framework.execute_command(name, ...) 的可变参数确实被原样
-- 转发到了插件自己的 run 函数——Framework 只转发，不解读参数的
-- 含义，参数的含义完全是插件的 Policy。

local M = {
    name = "debug",
}

function M.run(...)
    local args = { ... }
    local parts = {}
    for i = 1, #args do
        parts[i] = tostring(args[i])
    end
    local msg = "debug_plugin received args: [" .. table.concat(parts, ", ") .. "]"
    print("[debug_plugin] " .. msg)
    return msg
end

return M
```

`app.lua`:

```lua
-- app.lua
--
-- Application：先 require framework（拿到 Framework 这个 Plugin
-- Manager），再逐个 require 三个插件模块（Load 每一个插件），然后
-- 通过 framework.register_command(...) 把它们注册进去（Register）。
-- 之后只通过 framework.execute_command(name, ...) 触发调用（Invoke /
-- Event），app.lua 本身从不直接调用 hello_plugin.run(...) 这种具体
-- 插件函数——那样会绕开 Framework，破坏 Dependency Inversion（那时
-- Application 就变成直接依赖某个具体插件，而不是依赖 Framework 提供
-- 的统一契约）。

local framework = require("framework")

local hello_plugin = require("plugins.hello_plugin")
local status_plugin = require("plugins.status_plugin")
local debug_plugin = require("plugins.debug_plugin")

print("== Register 三个插件 ==")
framework.register_command(hello_plugin.name, hello_plugin)
framework.register_command(status_plugin.name, status_plugin)
framework.register_command(debug_plugin.name, debug_plugin)
print("已注册命令: " .. table.concat(framework.list_commands(), ", "))

print("\n== Invoke：依次执行三个命令 ==")
framework.execute_command("hello", "Lua")
framework.execute_command("status")
framework.execute_command("status")
framework.execute_command("debug", 1, "two", true)

print("\n== Invoke 一个不存在的命令：Framework 必须优雅地返回错误，而不是抛异常/崩溃 ==")
local result, err = framework.execute_command("restart")
print(string.format("execute_command('restart') -> result=%s, err=%s", tostring(result), tostring(err)))

print("\n== Unload：卸载 debug 插件，之后再调用它必须优雅地失败 ==")
local unreg_ok = framework.unregister_command("debug")
print(string.format("unregister_command('debug') -> ok=%s", tostring(unreg_ok)))
print("卸载后已注册命令: " .. table.concat(framework.list_commands(), ", "))

local result2, err2 = framework.execute_command("debug", "still there?")
print(string.format("execute_command('debug') after unload -> result=%s, err=%s", tostring(result2), tostring(err2)))
```

## 6. Execution Flow

实际运行(`cd code/01-command-plugin && lua5.4 app.lua`)的输出:

```text
== Register 三个插件 ==
已注册命令: debug, hello, status

== Invoke：依次执行三个命令 ==
[hello_plugin] Hello, Lua!
[status_plugin] status_plugin has been invoked 1 time(s)
[status_plugin] status_plugin has been invoked 2 time(s)
[debug_plugin] debug_plugin received args: [1, two, true]

== Invoke 一个不存在的命令：Framework 必须优雅地返回错误，而不是抛异常/崩溃 ==
execute_command('restart') -> result=nil, err=execute_command: unknown command 'restart'

== Unload：卸载 debug 插件，之后再调用它必须优雅地失败 ==
unregister_command('debug') -> ok=true
卸载后已注册命令: hello, status
execute_command('debug') after unload -> result=nil, err=execute_command: unknown command 'debug'
```

逐步拆解:

1. `require("framework")` 和三次 `require("plugins.xxx")` 完成 Load——四个文件的顶层代码各自执行一次,`framework.lua` 得到一个空的 `registry`,三个插件模块各自得到自己的初始 table。
2. 三次 `register_command` 依次把三个插件的 `name` 和 table 本身存进 `registry`,`list_commands()` 证明三个命令都已经在册(按字母序排列)。
3. 四次 `execute_command` 调用分别路由到 `hello_plugin.run`、两次 `status_plugin.run`(第二次调用时 `run_count` 已经从上一次调用的 `1` 累加到 `2`)、`debug_plugin.run`——每一次 Framework 做的事情完全一样:查表、调用、转发返回值。
4. `execute_command("restart")` 在 `registry` 里查不到对应插件,返回 `nil, "unknown command 'restart'"`,`app.lua` 判断到 `err` 不为空后打印提示,程序没有中断。
5. `unregister_command("debug")` 把 `debug` 从 `registry` 移除,`list_commands()` 证明它已经消失;再调用 `execute_command("debug", ...)` 得到和第 4 步一样的"unknown command"错误——证明 Unload 生效后,这个命令彻底不可达。

## 7. State

- **Plugin-level State**:`status_plugin.lua` 里的 `run_count` 是唯一一个跨调用持续存在的可变状态,属于这个模块自己的 `local` 变量。因为 Lua 的 `require` 结果会被缓存,`status_plugin` 只会被 `require` 一次,`run_count` 也只会被初始化一次,后续两次 `execute_command("status")` 看到的是同一份、持续累加的状态——这就是本文档从 `1` 到 `2` 的输出证据。
- **Framework-level State**:`framework.lua` 里的 `registry`,记录三个命令名到插件 table 的映射。它的生命周期完全由 `register_command`/`unregister_command` 决定,和任何单个插件内部的状态互不干扰——卸载 `debug` 不会影响 `status_plugin.run_count` 的值(如果之后重新 `register_command("debug", debug_plugin)`,`debug_plugin` 内部本来就没有状态,不构成反例;但可以确认的是,`status_plugin` 全程没有被卸载,它的计数器不受 `debug` 的注册/卸载影响)。
- `hello_plugin` 和 `debug_plugin` 都没有维护任何模块级可变状态,它们的 `run` 都是纯函数式的:相同输入,相同输出(除了打印这个副作用)。

## 8. Why this Pattern fits

命令行工具"允许新增命令,数量和名字预先未知"这个需求,正好匹配 Plugin Pattern 想解决的核心问题——如果用 Table Module,`hello`/`status`/`debug` 三个函数会写在同一个文件里,新增命令必须编辑这个文件;如果用 Factory,工厂函数内部要维护一张"命令名 -> 构造函数"的映射,新增命令依然要改工厂内部的映射表。Plugin Pattern 把"有哪些命令"这个决定权完全下放给 `app.lua`(以及未来任何调用 `register_command` 的代码),`framework.lua` 自身永远不需要知道"当前一共有几个命令、叫什么名字"。

## 9. What would go wrong with another Pattern

如果把三个命令合并成一个 Table Module(比如 `commands.lua` 里 `M.hello`/`M.status`/`M.debug` 三个函数),新增一个 `restart` 命令,必须打开 `commands.lua` 加一个新函数——这个文件会随着命令数量线性增长,而且没有一个统一的"执行"入口,调用者需要自己知道具体函数名(`commands.hello(...)`),这退化成了直接依赖具体实现,不再有 Framework 层做校验和统一分发。如果用 Factory,工厂内部必须写死"合法命令名有哪些"这份列表,新增命令依然要改工厂源码,只是把"直接依赖"换成了"通过工厂间接依赖",没有解决"Framework 要不要认识每一个具体命令"这个根本问题。

## 10. Mental Model

```text
Framework(framework.lua)
    |
    | 只依赖
    v
Contract: { name, run(...) }
    ^
    | 实现
    |
hello_plugin / status_plugin / debug_plugin
(数量、名字对 Framework 编写时完全未知)
```

一句话记忆:**Framework 管"有没有这个命令、怎么调用它",Plugin 管"这个命令具体做什么"**——`validate_plugin` 是前一半的完整代码,`status_plugin.run` 是后一半的完整代码。
