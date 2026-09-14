-- drivers/postgres.lua
--
-- Class/Constructor Module：PostgresDriver。
-- 同样不连接真正的 postgres 服务端,用内存表模拟"存储"。
-- 和 sqlite.lua 互不 require、互不知道对方存在,
-- 只是恰好实现了同一套 :insert/:query 契约。

local PostgresDriver = {}
PostgresDriver.__index = PostgresDriver

function PostgresDriver.new(config)
    config = config or {}
    local self = setmetatable({}, PostgresDriver)
    self.dsn = config.dsn or "postgres://localhost/app"
    self.rows = {}
    return self
end

function PostgresDriver:insert(record)
    local id = #self.rows + 1
    self.rows[id] = record
    print(string.format("  [postgres:%s] insert row #%d: %s", self.dsn, id, tostring(record.name)))
    return id
end

function PostgresDriver:query()
    print(string.format("  [postgres:%s] query -> %d row(s)", self.dsn, #self.rows))
    return self.rows
end

return PostgresDriver
