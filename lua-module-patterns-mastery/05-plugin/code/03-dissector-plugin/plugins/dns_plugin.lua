-- dns_plugin.lua
--
-- DNS 的 Dissector Plugin，满足和 http_plugin.lua 相同的契约
-- （name + can_handle + dissect），但认领条件和解析逻辑完全不同——
-- Framework 的 dispatch() 不需要知道这个区别，只需要依次调用
-- can_handle 就能让正确的插件认领正确的 packet_context。

local M = {
    name = "dns",
}

function M.can_handle(ctx)
    return ctx.port == 53
end

function M.dissect(ctx)
    local msg = string.format("parsed %d byte(s) on port %d as DNS", #ctx.bytes, ctx.port)
    print("[dns_plugin] " .. msg)
    return msg
end

return M
