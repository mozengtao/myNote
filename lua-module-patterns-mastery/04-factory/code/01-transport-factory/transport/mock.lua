-- transport/mock.lua
--
-- Class/Constructor Module：MockClient。
-- 用于测试场景：不发真实网络包，只把 send() 的内容记录到内存里的
-- self.log，供测试代码事后断言。同样满足 :send(data) / :close() 契约，
-- 所以 Factory/Application 完全不需要知道它和 tcp/udp 的实现差异。

local MockClient = {}
MockClient.__index = MockClient

--- new(config) -> MockClient 实例
-- config: 当前示例未使用任何字段，但保留同样的入参形状，
-- 让三个 transport 的构造函数签名保持一致。
function MockClient.new(config)
    local self = setmetatable({}, MockClient)
    self.closed = false
    self.log = {}
    return self
end

function MockClient:send(data)
    if self.closed then
        return nil, "mock: cannot send on a closed client"
    end
    self.log[#self.log + 1] = data
    print(string.format("  [mock] recorded message #%d: %s", #self.log, tostring(data)))
    return true
end

function MockClient:close()
    if self.closed then
        return nil, "mock: already closed"
    end
    self.closed = true
    print(string.format("  [mock] closed (recorded %d message(s) total)", #self.log))
    return true
end

--- get_log() -> table
-- 契约之外的额外能力：只有测试代码知道 MockClient 有这个方法，
-- Factory/Application 不会调用它,所以不破坏"统一契约"这个前提。
function MockClient:get_log()
    return self.log
end

return MockClient
