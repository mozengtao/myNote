-- connection.lua
--
-- Class / Constructor Module：Connection 是一个"类型"（Class Table），
-- 只在 require 时被创建一次，只装方法（connect/send/close），不装任何
-- 具体连接的数据。每一次 Connection.new(address) 才会产生一个独立的
-- Object（Instance Table），拥有自己的 address / state / buffer。
--
-- 固定结构：
--   local Connection = {}
--   Connection.__index = Connection
--   function Connection.new(...) local self = setmetatable({}, Connection) ... end
--   function Connection:method(...) ... end
--   return Connection

local Connection = {}
Connection.__index = Connection

--- Connection.new(address) -> Connection
-- 用 "." 调用，因为这里还没有一个已存在的 Instance 可以当 self——
-- new 本身就是负责"造出第一个 self"的函数。
-- setmetatable(self, Connection) 把 self 的元表设为 Connection，
-- 之后 self 上找不到的字段（典型是方法）都会 fallback 到 Connection 上找。
function Connection.new(address)
    local self = setmetatable({}, Connection)
    self.address = address
    self.state = "closed"   -- Object State：只属于这一个实例
    self.buffer = {}        -- Object State：每个实例自己的发送缓冲区
    return self
end

--- connection:connect() -- 用 ":" 调用，隐式传入 self（即调用者自己）
function Connection:connect()
    if self.state == "connected" then
        return false, "already connected"
    end
    self.state = "connected"
    return true
end

--- connection:send(data) -- 只修改 self（这个实例）的 buffer，不影响其他实例
function Connection:send(data)
    if self.state ~= "connected" then
        return false, "not connected"
    end
    table.insert(self.buffer, data)
    return true
end

function Connection:close()
    self.state = "closed"
    self.buffer = {}
    return true
end

--- connection:describe() -> string
-- 只读方法，方便 app.lua 打印某一个实例当前的状态，
-- 用来证明多个实例的状态互不干扰。
function Connection:describe()
    return string.format(
        "Connection(address=%s, state=%s, buffered=%d)",
        self.address, self.state, #self.buffer
    )
end

return Connection
