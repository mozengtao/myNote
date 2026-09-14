-- transport/udp.lua
--
-- Class/Constructor Module：UdpClient。
-- 与 tcp.lua 是平级的独立文件，互不 require，互不知道对方存在。
-- 只是恰好实现了同一套契约：:send(data) / :close()。

local UdpClient = {}
UdpClient.__index = UdpClient

--- new(config) -> UdpClient 实例
-- config: { host = string, port = number }
function UdpClient.new(config)
    config = config or {}
    local self = setmetatable({}, UdpClient)
    self.host = config.host or "0.0.0.0"
    self.port = config.port or 0
    self.closed = false
    self.sent_count = 0
    return self
end

function UdpClient:send(data)
    if self.closed then
        return nil, "udp: cannot send on a closed socket"
    end
    self.sent_count = self.sent_count + 1
    print(string.format("  [udp %s:%d] datagram #%d: %s",
        self.host, self.port, self.sent_count, tostring(data)))
    return true
end

function UdpClient:close()
    if self.closed then
        return nil, "udp: already closed"
    end
    self.closed = true
    print(string.format("  [udp %s:%d] closed (sent %d datagram(s))",
        self.host, self.port, self.sent_count))
    return true
end

return UdpClient
