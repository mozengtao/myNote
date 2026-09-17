-- event_system_demo.lua
--
-- 演示一个支持多事件类型、每种事件类型多个回调的事件系统：
--   1. 给两个不同事件各注册 2 个回调，emit 验证全部触发。
--   2. off() 掉其中一个 handle，再次 emit 验证它确实不再被调用。
--   3. 注册一个故意 error() 的回调，验证它不会影响同事件下其它回调。
--   4. 额外验证：off() 一个不存在的 handle 不会崩溃；
--      连续注册超过槛位上限会被 luaL_error 清晰地拒绝。

print("== 注册 connected 事件的 2 个回调 ==")
local h1 = on("connected", function(payload)
    print("  [lua] callback#1 (connected):", payload)
end)
local h2 = on("connected", function(payload)
    print("  [lua] callback#2 (connected):", payload)
end)

print("\n== 注册 message 事件的 2 个回调 ==")
local h3 = on("message", function(payload)
    print("  [lua] callback#3 (message):", payload)
end)
local h4 = on("message", function(payload)
    print("  [lua] callback#4 (message):", payload)
end)

print("\n== 第一次 emit：两个事件都应该触发各自的 2 个回调 ==")
emit("connected", "client-A")
emit("message", "hello world")

print("\n== off(h1)：反注册 connected 的第一个回调 ==")
off(h1)

print("\n== 再次 emit(\"connected\")：现在应该只剩 callback#2 触发 ==")
emit("connected", "client-B")

print("\n== 注册一个故意出错的回调到 message 事件 ==")
local h5 = on("message", function(payload)
    error("boom in callback#5 while handling: " .. tostring(payload))
end)

print("\n== emit(\"message\")：callback#5 会报错，但 callback#3/#4 应该都还正常执行 ==")
emit("message", "second wave")

print("\n== off() 一个不存在的 handle，验证不会崩溃 ==")
local ok_off = off(9999)
print("  off(9999) returned:", ok_off)

print("\n== 演示回调槛位耗尽的 Failure：连续注册超过上限个回调 ==")
on("overflow-test", function() end)
on("overflow-test", function() end)
on("overflow-test", function() end)
on("overflow-test", function() end)
local ok_overflow, err_overflow = pcall(on, "overflow-test", function() end)
print("  5th on(\"overflow-test\", ...) => ok=" .. tostring(ok_overflow)
    .. " err=" .. tostring(err_overflow))

print("\n[lua] 脚本执行完毕，程序没有崩溃")

-- 清理掉本次仍然存活的 handle（h2, h3, h4, h5），避免"忘记 off() 导致
-- Registry Reference 泄漏"——这一点会在 README 的 Lifecycle 部分展开讲。
off(h2)
off(h3)
off(h4)
off(h5)
