-- app.lua
--
-- Application：require framework + 三个协议 handler 插件，注册后
-- 构造几个"假 packet"（普通 Lua table，带一个 protocol 字段），
-- 逐个交给 framework.dispatch(...)。最后额外 dispatch 一个未知协议
-- 的 packet，验证 Framework 优雅返回错误而不是抛异常。

local framework = require("framework")

local http_handler = require("plugins.http_handler")
local dns_handler = require("plugins.dns_handler")
local mqtt_handler = require("plugins.mqtt_handler")

print("== Register 三个协议 handler ==")
framework.register_handler(http_handler.name, http_handler)
framework.register_handler(dns_handler.name, dns_handler)
framework.register_handler(mqtt_handler.name, mqtt_handler)
print("已注册协议: " .. table.concat(framework.list_protocols(), ", "))

local packets = {
    { protocol = "http", method = "GET", path = "/index.html", payload = "" },
    { protocol = "dns", query = "example.com" },
    { protocol = "mqtt", topic = "sensors/temp", payload = "23.5" },
    { protocol = "mqtt", topic = "sensors/humidity", payload = "48" },
    { protocol = "sip", payload = "INVITE sip:bob@example.com" }, -- 没有注册对应的 handler
}

print("\n== Dispatch 每一个 packet ==")
for i, packet in ipairs(packets) do
    local result, err = framework.dispatch(packet)
    if err then
        print(string.format("packet #%d (protocol=%s) -> dispatch failed: %s", i, packet.protocol, err))
    else
        print(string.format("packet #%d (protocol=%s) -> handled: %s", i, packet.protocol, result))
    end
end
