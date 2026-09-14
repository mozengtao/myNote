-- app.lua
--
-- Application：创建 3 个 Device 实例，只启动其中两个、停止其中一个，
-- 逐一打印 status()，证明每个实例的 running 状态完全由自己的
-- Instance Table 决定，不受其他实例、也不受 Device 这个 Class Table
-- 本身状态的影响（因为 Class Table 从来没有存过 running 字段）。

local Device = require("device")

print("== 创建 3 个独立的 Device 实例 ==")
local dev1 = Device.new("dev-1")
local dev2 = Device.new("dev-2")
local dev3 = Device.new("dev-3")

print(dev1:status())
print(dev2:status())
print(dev3:status())

print("\n== 启动 dev1 和 dev3，dev2 保持不动 ==")
dev1:start()
dev3:start()

print(dev1:status())
print(dev2:status())
print(dev3:status())

print("\n== 再停止 dev1，dev3 必须仍然是 running ==")
dev1:stop()

print(dev1:status())
print(dev2:status())
print(dev3:status())

print("\n== 重复启动同一个 Device 会失败（体现 state 只属于这一个实例） ==")
local ok, err = dev3:start()
print(string.format("dev3:start() -> ok=%s, err=%s", tostring(ok), tostring(err)))

print("\n== 三个实例共享同一份方法，但各自的 running 字段互不相同 ==")
print(string.format(
    "dev1.start == dev2.start == dev3.start -> %s (方法只有一份,来自同一个 Class Table)",
    tostring(dev1.start == dev2.start and dev2.start == dev3.start)
))
print(string.format(
    "最终各自的 running: dev1=%s, dev2=%s, dev3=%s (三份独立的 Instance State,互不相同)",
    tostring(dev1.running), tostring(dev2.running), tostring(dev3.running)
))
