-- event_demo.lua
-- 演示一个最小可用的"事件系统"：注册一个 handler，
-- 然后触发若干次事件；再演示 handler 内部出错时 C 侧如何
-- 安全捕获；最后演示 clear_handler 的 cleanup 效果。

on_event(function(event)
    print("event:", event)
end)

emit_event("connected")
emit_event("disconnected")

-- 演示 error handling：换一个故意出错的 handler，
-- 验证 lua_pcall 能捕获错误、C 侧不会崩溃、程序能继续往下走。
on_event(function(event)
    error("boom in handler while handling event: " .. event)
end)
emit_event("will-error")

-- 出错之后，程序应该还活着，emit_event 依然可以正常工作
-- （只是这一次的 handler 还是那个会 error 的，所以特意在这里
--  先清理掉，再验证"清理之后没有 handler 时 emit_event 的行为"）
clear_handler()
emit_event("after-clear")

print("[lua] 脚本执行完毕，程序没有崩溃")
