-- plugin_a.lua
--
-- Plugin A：全篇最简单的插件实现，只做一件事——把生命周期回调和
-- 收到的事件打印成日志，不维护任何跨事件的内部状态。
-- 用来和 plugin_b.lua（维护 event_count 计数器）做对比，
-- 体现"两个插件即使实现同一套契约，State 也完全互相独立"。

function plugin_init(host_name)
    host_log(string.format("plugin_init 被调用，host 传入的标识: %s", host_name))
end

function on_event(name, payload)
    host_log(string.format(
        "收到事件 on_event(%s, %s)，Plugin A 只打印日志，不维护状态",
        name, payload))
end

function plugin_shutdown()
    host_log("plugin_shutdown 被调用，Plugin A 即将卸载")
end
