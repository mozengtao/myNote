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
