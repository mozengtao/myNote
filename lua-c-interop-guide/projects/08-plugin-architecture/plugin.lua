-- plugin.lua
--
-- Project 8 的 Lua 侧：一个最小但完整的"插件"，只依赖 host 提供的三样东西：
--   host.version              (字符串字段)
--   host.log(msg)             (C Function)
--   host.services.transform(x) (嵌套 table 里的 C Function)
-- 插件本身只暴露三个生命周期 Function：init / on_event / shutdown，
-- 具体什么时候调用、传什么参数，完全由 C Host（main.c）决定——
-- 插件不知道、也不需要知道自己"被谁、按什么顺序调用"。

plugin = {}

function plugin.init(host)
    print("plugin init, host version:", host.version)
    host.log("plugin initialized")
end

function plugin.on_event(host, name, payload)
    host.log("plugin got event: " .. name .. " = " .. tostring(payload))
    -- 故意不做任何 pcall：如果 payload 类型 host.services.transform 处理不了，
    -- 这里会直接向上抛错，交给 C Host 的 dispatch_event() 用 lua_pcall 兜住
    -- （见 README Failure 一节）。
    return host.services.transform(payload)
end

function plugin.shutdown(host)
    host.log("plugin shutting down")
end
