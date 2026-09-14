-- http_plugin.lua
--
-- HTTP 的 Dissector Plugin：can_handle 只关心"这个 packet_context 是
-- 不是我认识的类型"（这里简化成按端口号判断），dissect 才是真正的
-- 解析行为。两者都是插件自己的 Policy，Framework 只负责按顺序调用
-- can_handle 直到有人认领。

local M = {
    name = "http",
}

function M.can_handle(ctx)
    return ctx.port == 80 or ctx.port == 8080
end

function M.dissect(ctx)
    local msg = string.format("parsed %d byte(s) on port %d as HTTP", #ctx.bytes, ctx.port)
    print("[http_plugin] " .. msg)
    return msg
end

return M
