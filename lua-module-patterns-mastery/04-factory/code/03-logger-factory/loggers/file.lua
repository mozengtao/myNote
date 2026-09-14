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
