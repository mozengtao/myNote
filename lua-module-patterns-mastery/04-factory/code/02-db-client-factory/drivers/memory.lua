-- drivers/memory.lua
--
-- Class/Constructor Module：MemoryDriver。
-- 专为测试/开发场景准备的"驱动"：连内存表这层伪装都不做了,
-- 直接就是内存表本身——但对外仍然满足 :insert/:query 契约,
-- 所以 Application 换用它时不需要改一行调用代码。

local MemoryDriver = {}
MemoryDriver.__index = MemoryDriver

function MemoryDriver.new(config)
    local self = setmetatable({}, MemoryDriver)
    self.rows = {}
    return self
end

function MemoryDriver:insert(record)
    local id = #self.rows + 1
    self.rows[id] = record
    print(string.format("  [memory] insert row #%d: %s", id, tostring(record.name)))
    return id
end

function MemoryDriver:query()
    print(string.format("  [memory] query -> %d row(s)", #self.rows))
    return self.rows
end

return MemoryDriver
