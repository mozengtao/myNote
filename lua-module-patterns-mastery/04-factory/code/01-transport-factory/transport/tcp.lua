-- transport/tcp.lua
--
-- Class/Constructor Module（复用 Pattern 3 的写法）：TcpClient。
-- 这个文件只负责"一个 TCP 连接对象长什么样"，完全不知道
-- Factory、Application 的存在，也不知道还有 udp/mock 这两个兄弟实现。
--
-- 对外暴露的是传输层的统一契约（Abstraction）：
--   :send(data)  -> true | (nil, err)
--   :close()     -> true | (nil, err)
-- Factory 正是靠"所有 transport 都满足这个方法形状"才能互换使用它们。

local TcpClient = {}
TcpClient.__index = TcpClient

--- new(config) -> TcpClient 实例
-- config: { host = string, port = number }
function TcpClient.new(config)
    config = config or {}
    local self = setmetatable({}, TcpClient)
    self.host = config.host or "127.0.0.1"
    self.port = config.port or 0
    self.closed = false
    self.sent_count = 0
    return self
end

function TcpClient:send(data)
    if self.closed then
        return nil, "tcp: cannot send on a closed connection"
    end
    self.sent_count = self.sent_count + 1
    print(string.format("  [tcp %s:%d] send #%d: %s",
        self.host, self.port, self.sent_count, tostring(data)))
    return true
end

function TcpClient:close()
    if self.closed then
        return nil, "tcp: already closed"
    end
    self.closed = true
    print(string.format("  [tcp %s:%d] closed (sent %d message(s))",
        self.host, self.port, self.sent_count))
    return true
end

return TcpClient
