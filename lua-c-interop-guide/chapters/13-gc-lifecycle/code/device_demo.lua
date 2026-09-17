-- Chapter 13: 观察 __gc 真的被触发，C 资源真的被释放。

print("== 第 1 段：dev 在 do...end 块内，块结束后立刻不可达 ==")
do
    local dev = device.open("/dev/null")
    dev:start()
    dev:stop()
    -- do...end 结束后，局部变量 dev 离开作用域；
    -- 但这只是"不可达"，不代表内存立刻被释放——GC 什么时候真正跑，由 GC 自己决定。
end

print("-- 手动触发一次完整 GC：collectgarbage() --")
collectgarbage()
print("-- collectgarbage() 结束 --")
print()

print("== 第 2 段：显式把变量置为 nil，也是让对象变得不可达的一种方式 ==")
local dev2 = device.open("/dev/null")
dev2:start()
dev2 = nil -- 唯一持有这个 userdata 的引用被清空
collectgarbage()
print()

print("== 第 3 段：不手动 collectgarbage，交给 lua_close 兜底 ==")
local dev3 = device.open("/dev/null")
dev3:start()
-- 这里故意不调用 collectgarbage()，也不置 nil；
-- 脚本结束、宿主程序调用 lua_close(L) 时依然会触发它的 __gc。
