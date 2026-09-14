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
