-- mqtt_handler.lua
--
-- MQTT 协议的 Handler Plugin，额外演示 Plugin-level State：
-- message_count 只属于这个插件自己，跨多次 dispatch 持续累加，
-- http_handler/dns_handler 完全看不到、也不受它影响。

local M = {
    name = "mqtt",
}

local message_count = 0

function M.handle(packet)
    message_count = message_count + 1
    local topic = packet.topic or "unknown"
    local msg = string.format(
        "MQTT publish #%d on topic '%s' (payload=%s)",
        message_count, topic, tostring(packet.payload)
    )
    print("[mqtt_handler] " .. msg)
    return msg
end

return M
