-- drivers/sqlite.lua
--
-- Class/Constructor Module：SqliteDriver。
-- 这里不连接真正的 sqlite 文件,用一张内存表 self.rows 模拟"存储",
-- 只是为了演示 Factory 组合 Class/Constructor 的机制,
-- 不引入真实数据库依赖。
--
-- 对外契约（Abstraction）：
--   :insert(record) -> id
--   :query()        -> rows（数组）

local SqliteDriver = {}
SqliteDriver.__index = SqliteDriver

function SqliteDriver.new(config)
    config = config or {}
    local self = setmetatable({}, SqliteDriver)
    self.path = config.path or ":memory:"
    self.rows = {}
    return self
end

function SqliteDriver:insert(record)
    local id = #self.rows + 1
    self.rows[id] = record
    print(string.format("  [sqlite:%s] insert row #%d: %s", self.path, id, tostring(record.name)))
    return id
end

function SqliteDriver:query()
    print(string.format("  [sqlite:%s] query -> %d row(s)", self.path, #self.rows))
    return self.rows
end

return SqliteDriver
