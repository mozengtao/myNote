-- http_handler.lua
--
-- HTTP 协议的 Handler Plugin。handle() 内部怎么解读 packet 的字段
-- （method/path/payload）完全是这个插件自己的 Policy，Framework 的
-- dispatch() 从不关心这里面具体做了什么，只负责把 packet 转发过来。

local M = {
    name = "http",
}

function M.handle(packet)
    local method = packet.method or "GET"
    local path = packet.path or "/"
    local msg = string.format("HTTP %s %s (payload=%s)", method, path, tostring(packet.payload))
    print("[http_handler] " .. msg)
    return msg
end

return M
