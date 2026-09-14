# 04-3 · 示例：Logger Factory

> Pattern：Factory Module（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/03-logger-factory/](code/03-logger-factory/)（`loggers/console.lua` / `loggers/file.lua` / `loggers/null.lua` / `factory.lua` / `app.lua`）。

## 1. Problem / Scenario

一个应用需要在不同场景下用不同方式记录日志：开发时打印到终端（`console`）方便实时观察，生产环境写入磁盘文件（`file`）方便事后排查，某些性能敏感的测试场景想彻底关掉日志（`null`）而不需要在调用点写一堆 `if logger then logger:info(...) end` 判断。三种 logger 需要暴露完全一致的接口：`:info(msg)` / `:error(msg)` / `:debug(msg)`。

## 2. Architecture

```text
Abstraction
     ^
     |
 Factory
     |
     +-- ConsoleLogger
     +-- FileLogger
     +-- NullLogger
```

`Abstraction` 是三个 logger 共同满足的方法形状约定：`:info(msg)` / `:error(msg)` / `:debug(msg)`。`Factory` 站在 Abstraction 和三个具体实现之间——它一边知道全部三个具体实现的构造函数，另一边只对外承诺"我给你的东西一定满足 Abstraction"。放进完整的四层视角：

```text
Caller (app.lua)
   |
   v
Factory (factory.lua)
   |
   +-- loggers/console.lua  (ConsoleLogger)
   +-- loggers/file.lua     (FileLogger)
   +-- loggers/null.lua     (NullLogger)
```

## 3. Framework / Caller 代码

```lua
local factory = require("factory")

local console_logger = factory.create("console", {})
console_logger:info("service started")
```

`app.lua` 全程只 `require("factory")`，对三个具体 logger 实现的模块路径一无所知。

## 4. Module 代码

```lua
local ConsoleLogger = require("loggers.console")
local FileLogger = require("loggers.file")
local NullLogger = require("loggers.null")

local M = {}

local creators = {
    console = ConsoleLogger.new,
    file = FileLogger.new,
    null = NullLogger.new,
}

function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown logger kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
```

`FileLogger` 是三者中唯一真正有"副作用"的实现——它在构造时打开一个磁盘文件；`NullLogger` 是三者中唯一"什么都不做"的实现。两者对 Factory 来说没有任何区别，都只是 `creators` 表里的一个构造函数。

## 5. 完整 Lua 代码

与磁盘上的文件完全一致。

`code/03-logger-factory/loggers/console.lua`：

```lua
-- loggers/console.lua
--
-- Class/Constructor Module：ConsoleLogger。
-- 对外契约（Abstraction）：
--   :info(msg) / :error(msg) / :debug(msg)
-- 三个 logger 实现（console/file/null）都满足这个方法形状,
-- Factory 才能在它们之间自由互换。

local ConsoleLogger = {}
ConsoleLogger.__index = ConsoleLogger

function ConsoleLogger.new(config)
    local self = setmetatable({}, ConsoleLogger)
    return self
end

function ConsoleLogger:info(msg)
    print("[INFO ] " .. msg)
end

function ConsoleLogger:error(msg)
    print("[ERROR] " .. msg)
end

function ConsoleLogger:debug(msg)
    print("[DEBUG] " .. msg)
end

return ConsoleLogger
```

`code/03-logger-factory/loggers/file.lua`：

```lua
-- loggers/file.lua
--
-- Class/Constructor Module：FileLogger。
-- 真实写入磁盘文件（默认路径 /tmp/lua-module-patterns-logger-factory-example.log），
-- 用来证明这不是一个只打印字符串的假模块——文件确实被写入了。
--
-- 构造时以 "w" 模式截断旧内容,之后每条日志用 :write() + :flush()
-- 立即落盘,这样即使调用者从不调用任何"关闭"方法,app.lua 事后
-- 用另一个文件句柄读取同一个文件时也能看到完整内容。

local FileLogger = {}
FileLogger.__index = FileLogger

function FileLogger.new(config)
    config = config or {}
    local self = setmetatable({}, FileLogger)
    self.path = config.path or "/tmp/lua-module-patterns-logger-factory-example.log"

    local file, err = io.open(self.path, "w")
    if not file then
        error("file logger: cannot open '" .. self.path .. "': " .. tostring(err))
    end
    self.file = file
    return self
end

local function write_line(self, level, msg)
    self.file:write(string.format("[%s] %s\n", level, msg))
    self.file:flush()
end

function FileLogger:info(msg)
    write_line(self, "INFO", msg)
end

function FileLogger:error(msg)
    write_line(self, "ERROR", msg)
end

function FileLogger:debug(msg)
    write_line(self, "DEBUG", msg)
end

return FileLogger
```

`code/03-logger-factory/loggers/null.lua`：

```lua
-- loggers/null.lua
--
-- Class/Constructor Module：NullLogger。
-- 什么都不做的实现——常用于"暂时不想要日志,但调用方代码不应该
-- 因此加一堆 if logger then ... end 判断"的场景。
-- 依然满足 :info/:error/:debug 契约,只是方法体是空的。

local NullLogger = {}
NullLogger.__index = NullLogger

function NullLogger.new(config)
    local self = setmetatable({}, NullLogger)
    return self
end

function NullLogger:info(msg)
    -- 有意什么都不做
end

function NullLogger:error(msg)
    -- 有意什么都不做
end

function NullLogger:debug(msg)
    -- 有意什么都不做
end

return NullLogger
```

`code/03-logger-factory/factory.lua`：

```lua
-- factory.lua
--
-- Factory Module：唯一知道有哪些具体 logger 实现的地方。
--
-- 依赖方向（Dependency Inversion）：
--
--     Abstraction
--          ^
--          |
--      Factory
--          |
--          +-- ConsoleLogger
--          +-- FileLogger
--          +-- NullLogger
--
-- Factory 和三个具体 Logger 都朝着同一个 Abstraction（info/error/debug
-- 方法形状）对齐,但 Application 从不直接触碰 ConsoleLogger/FileLogger/
-- NullLogger 这几个类,只触碰 Factory 交回来的、满足 Abstraction 的实例。

local ConsoleLogger = require("loggers.console")
local FileLogger = require("loggers.file")
local NullLogger = require("loggers.null")

local M = {}

local creators = {
    console = ConsoleLogger.new,
    file = FileLogger.new,
    null = NullLogger.new,
}

--- create(kind, config) -> logger 实例 | (nil, err)
function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown logger kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
```

`code/03-logger-factory/app.lua`：

```lua
-- app.lua
--
-- Application：全程只 require("factory")。
-- 搜索这个文件找不到任何一个具体 logger 实现模块的路径字符串。

local factory = require("factory")

local LOG_PATH = "/tmp/lua-module-patterns-logger-factory-example.log"

print("== console logger ==")
local console_logger = factory.create("console", {})
console_logger:info("service started")
console_logger:error("connection refused")
console_logger:debug("retry count = 3")

print("\n== null logger（应当没有任何输出）==")
local null_logger = factory.create("null", {})
null_logger:info("this should never be printed")
null_logger:error("this should never be printed either")
null_logger:debug("nor this")
print("  (以上两行之间没有任何 [INFO]/[ERROR]/[DEBUG] 输出，证明 null logger 确实什么都没做)")

print("\n== file logger（真实写入磁盘）==")
local file_logger = factory.create("file", { path = LOG_PATH })
file_logger:info("service started")
file_logger:error("connection refused")
file_logger:debug("retry count = 3")
print("  已通过 file logger 写入: " .. LOG_PATH)

print("\n== 读回磁盘文件内容，证明确实写盘了 ==")
local reader = io.open(LOG_PATH, "r")
if not reader then
    print("  读取失败：文件不存在")
else
    print("  --- " .. LOG_PATH .. " 内容开始 ---")
    for line in reader:lines() do
        print("  " .. line)
    end
    print("  --- 内容结束 ---")
    reader:close()
end

print("\n== 请求一个未知的 logger kind ==")
local bad_logger, bad_err = factory.create("syslog", {})
if not bad_logger then
    print("  预期的失败: " .. bad_err)
else
    print("  意外：syslog 竟然创建成功了")
end
```

## 6. Execution

```bash
cd code/03-logger-factory
lua5.4 app.lua
```

实际执行输出（已用本地 Lua 5.4 验证，且随后独立用 `cat` 命令核实了 `/tmp/lua-module-patterns-logger-factory-example.log` 磁盘文件的内容与下方"读回磁盘文件内容"一节完全一致）：

```text
== console logger ==
[INFO ] service started
[ERROR] connection refused
[DEBUG] retry count = 3

== null logger（应当没有任何输出）==
  (以上两行之间没有任何 [INFO]/[ERROR]/[DEBUG] 输出，证明 null logger 确实什么都没做)

== file logger（真实写入磁盘）==
  已通过 file logger 写入: /tmp/lua-module-patterns-logger-factory-example.log

== 读回磁盘文件内容，证明确实写盘了 ==
  --- /tmp/lua-module-patterns-logger-factory-example.log 内容开始 ---
  [INFO] service started
  [ERROR] connection refused
  [DEBUG] retry count = 3
  --- 内容结束 ---

== 请求一个未知的 logger kind ==
  预期的失败: factory: unknown logger kind 'syslog'
```

## 7. Execution Flow

1. `require("factory")` 载入 `factory.lua`，它先 `require` 三个具体 logger 模块，构造 `creators` 表，`return M`。
2. `factory.create("console", {})`：转发给 `ConsoleLogger.new`，得到一个只封装了三个 `print` 方法的实例；三次 `:info/:error/:debug` 调用直接打印到终端。
3. `factory.create("null", {})`：转发给 `NullLogger.new`，三次方法调用命中的都是空函数体，没有任何可观察的副作用。
4. `factory.create("file", {path=LOG_PATH})`：转发给 `FileLogger.new`——构造函数内部用 `io.open(path, "w")` 打开（并截断）磁盘文件，若失败会 `error()`；构造成功后，三次 `:info/:error/:debug` 调用各自往这个文件句柄 `write()` 一行、`flush()` 一次，确保内容立即落盘而不是停留在缓冲区里。
5. `app.lua` 用**另一个独立的文件句柄** `io.open(LOG_PATH, "r")` 把文件内容读回来、逐行打印——这一步和 `file_logger` 自己持有的写句柄完全无关，只依赖磁盘上真实存在的文件内容，因此能真正证明"写盘"这件事发生了，不是靠信任 `FileLogger` 自己的说法。
6. 对 `"syslog"` 调用 `factory.create`：`creators.syslog` 是 `nil`，返回 `(nil, "factory: unknown logger kind 'syslog'")`。

## 8. State

Factory 本身无状态，规则与前两个示例完全一致：`creators` 表只在加载时构造一次，`M.create()` 每次调用只是查表转发。

三个 logger 实例的 State 归属方式不同，正好覆盖三种情况：

```text
ConsoleLogger 实例  -- 几乎不持有 State，每次调用都是"当场打印，不留痕迹"
NullLogger 实例     -- 完全不持有 State，方法体是空的
FileLogger 实例     -- 持有真实的外部资源 State：self.file（一个打开的文件句柄）
```

`FileLogger` 是这个示例里唯一持有"需要被妥善管理的资源"的实例——它的 `self.file` 不是一份普通数据，而是一个操作系统级的文件句柄。这提醒了一个 Pattern 3 State 讨论里没有展开的点：Object State 不仅可以是普通数据（数字、字符串、表），也可以是外部资源的引用，资源的生命周期管理（这里简化为"每次写入后立即 flush"，没有显式 `:close()`）同样是这个实例自己的责任，不是 Factory 的责任。

## 9. Why this Pattern fits

- `console`/`file`/`null` 是"记日志"这同一件事的三种做法，调用方在业务代码里想要的只是"记一条日志"，不想在每个调用点都判断"现在是哪种 logger、该怎么调"。
- `null` 实现本身就是为了消除"要不要打日志"这个判断分支而存在的——如果没有统一契约，`if logger then logger:info(...) end` 这种判断会散布在整个代码库里；有了 Factory + 统一契约，`null_logger:info(...)` 永远是安全的空操作，业务代码不需要关心 logger 是否"真的存在"。
- 三种 logger 的构造复杂度差异很大（`ConsoleLogger`/`NullLogger` 几乎不需要参数，`FileLogger` 需要打开真实文件、可能失败），Factory 把这种复杂度差异隔离在自己内部，Application 面对的始终是同一个 `factory.create(kind, config)` 调用形状。

## 10. What would go wrong with another Pattern

- **不用 Factory**：业务代码需要在每个记日志的地方判断"当前配置的 logger 类型"，或者干脆到处 `require("loggers.console")`、`require("loggers.file")`，把"该用哪个 logger"这个跨越整个项目的决策，散落成无数个局部决策，一旦要全局切换（比如把 `console` 换成 `file`）需要改动的地方和 Application 里散落 `require` 的次数一样多。
- **换成 Private+Public**：如果只有一种 logger 实现，Private+Public（把"往文件写"这类实现细节隐藏在 Public API 后面）已经够用；但这里明确需要在多种平级实现之间切换，Private+Public 不解决"选择哪个实现"的问题。
- **换成 Plugin**：如果这个项目允许第三方注册全新的 logger 后端（比如某个团队想加一个"写到远程日志服务"的 logger，且不想修改这个项目的源码），Plugin（Pattern 5）会比 Factory 更合适；但如果 logger 的种类是项目内部封闭、确定的几种，Factory 的选择表已经足够，不需要额外的注册机制。

## 11. Mental Model

> Factory + 统一契约在这里的作用是"把 logger 的具体形态从调用点上抹掉"：业务代码只知道自己手上有一个东西满足 `:info/:error/:debug`，不知道、也不关心这背后是在打印到终端、写进磁盘、还是什么都不做。
