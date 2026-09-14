-- app.lua
--
-- Application：require framework + 两个 dissector 插件，注册后构造
-- 几个"假 packet_context"（普通 Lua table，带 bytes/port 字段），
-- 逐个交给 framework.dispatch(...)。最后额外 dispatch 一个两个插件
-- 都无法认领的 context，验证 Framework 优雅地返回失败。

local framework = require("framework")

local http_plugin = require("plugins.http_plugin")
local dns_plugin = require("plugins.dns_plugin")

print("== Register 两个 dissector 插件 ==")
framework.register_dissector(http_plugin)
framework.register_dissector(dns_plugin)
print("已注册 dissector: " .. table.concat(framework.list_dissectors(), ", "))

local contexts = {
    { bytes = "GET / HTTP/1.1\r\n", port = 80 },
    { bytes = "\x00\x01\x01\x00example.com", port = 53 },
    { bytes = "\x16\x03\x01handshake", port = 443 }, -- 没有插件能认领
}

print("\n== Dispatch 每一个 packet_context ==")
for i, ctx in ipairs(contexts) do
    local name, result = framework.dispatch(ctx)
    if not name then
        print(string.format("context #%d (port=%d) -> %s", i, ctx.port, result))
    else
        print(string.format("context #%d (port=%d) -> claimed by '%s': %s", i, ctx.port, name, result))
    end
end
