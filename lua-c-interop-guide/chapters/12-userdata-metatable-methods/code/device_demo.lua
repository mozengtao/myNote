-- Chapter 12: dev:start() / dev:stop() / dev:name() 全部真实可调用。

local dev = device.open("/dev/null")

print("tostring(dev) =", tostring(dev)) -- 用上了 Chapter 12 新加的 __tostring

print("name:", dev:name())
dev:start()
dev:stop()

-- dev:start() 只是 dev.start(dev) 的语法糖，两种写法效果完全一样：
local ok = dev.start(dev)
print("dev.start(dev) 等价调用, ok =", ok)

-- 打开一个不存在的路径，制造 fd = -1，验证 start() 里补上的检查。
local bad = device.open("/path/does/not/exist")
local ok2, err2 = pcall(function() bad:start() end)
print("bad:start() ok  =", ok2)
print("bad:start() err =", err2)
