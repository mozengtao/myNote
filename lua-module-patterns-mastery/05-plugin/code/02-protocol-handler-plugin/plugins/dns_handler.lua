-- dns_handler.lua
--
-- DNS 协议的 Handler Plugin，和 http_handler.lua 一样满足同一份
-- 契约（name + handle），但内部字段读取逻辑完全不同——这正是
-- "Framework 只认契约，不认具体协议"的直接证明。

local M = {
    name = "dns",
}

function M.handle(packet)
    local query = packet.query or "unknown"
    local msg = string.format("DNS query for '%s'", query)
    print("[dns_handler] " .. msg)
    return msg
end

return M
