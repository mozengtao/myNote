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
