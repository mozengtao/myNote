-- factory.lua
--
-- Factory Module：唯一知道"到底有哪些具体 transport 实现"的地方。
-- 它 require 了全部三个具体实现模块——这是必要的、集中的耦合，
-- 换来的是 Application 永远不需要知道 transport.tcp / transport.udp /
-- transport.mock 这几个模块名的存在。
--
-- Factory 本身不持有任何跨调用的 State：creators 表在 require 时
-- 构造一次就不再变化，M.create() 每次调用都是一次独立的、无副作用的
-- "查表 + 转发"，State 全部转移到了它创建出来的 transport 对象身上。

local TcpClient = require("transport.tcp")
local UdpClient = require("transport.udp")
local MockClient = require("transport.mock")

local M = {}

-- creators：kind 字符串 -> 具体实现的构造函数。
-- 新增一种 transport 时，只需要在这里加一行,不需要改 Application。
local creators = {
    tcp = TcpClient.new,
    udp = UdpClient.new,
    mock = MockClient.new,
}

--- create(kind, config) -> transport 实例 | (nil, err)
-- kind 未知时返回 nil + 错误信息,不抛出不可预期的 error()。
function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown transport kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
