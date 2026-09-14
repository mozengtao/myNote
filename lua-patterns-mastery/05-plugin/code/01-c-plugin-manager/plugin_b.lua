-- plugin_b.lua
--
-- Plugin B：与 plugin_a.lua 共用同一份 Host API（host_log），
-- 但维护了一个模块级私有状态 event_count，每次 on_event 累加。
-- 这体现"每个插件的 lua_State 互相独立，State 也互相独立"——
-- 即使 Manager 同时给 plugin_a 和 plugin_b 派发同一个事件，
-- plugin_a 完全不知道 plugin_b 内部计数器的存在，反之亦然。
--
-- on_event 里还故意对 name == "boom" 抛出一个错误，用来配合
-- plugin_manager.c 演示 Event 阶段的错误隔离：这一次 dispatch
-- 对 plugin_b 失败，但不影响 plugin_a 继续收到同一个事件，
-- 也不影响 plugin_b 在下一次事件里恢复正常。

local event_count = 0

function plugin_init(host_name)
    event_count = 0
    host_log(string.format(
        "plugin_init 被调用，host 传入的标识: %s，计数器已初始化为 0",
        host_name))
end

function on_event(name, payload)
    if name == "boom" then
        -- 简化点：这里用 error() 模拟插件内部的 bug（例如访问了一个 nil 字段），
        -- 真实场景中错误可能来自任何一行 Lua 代码，不一定是显式 error()。
        error("Plugin B 故意在 on_event 里抛出的错误，用于演示 Manager 的错误隔离")
    end
    event_count = event_count + 1
    host_log(string.format(
        "收到事件 on_event(%s, %s)，这是第 %d 次收到事件",
        name, payload, event_count))
end

function plugin_shutdown()
    host_log(string.format(
        "plugin_shutdown 被调用，Plugin B 共处理了 %d 次事件后卸载",
        event_count))
end
