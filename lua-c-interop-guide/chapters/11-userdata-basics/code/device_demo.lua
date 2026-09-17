-- Chapter 11: 只演示 device.open() 创建 userdata，以及从中读回 struct device*。
-- dev:start() / dev:stop() / dev:name() 的方法派发链，从 Chapter 12 开始才有。

local dev = device.open("/dev/null")

print("type(dev)      =", type(dev))
print("tostring(dev)  =", tostring(dev)) -- 还没挂 __tostring，只会打印默认的 "userdata: 0x..."

device_dump(dev)

-- 故意传一个 table 进去，制造类型混淆，演示 luaL_checkudata 的保护作用。
local ok, err = pcall(device_dump, {})
print("device_dump({}) ok  =", ok)
print("device_dump({}) err =", err)
