-- Chapter 14: 对比 Full Userdata（device.open() 创建）与
-- Light Userdata（get_singleton_key() 拿到的"钥匙"）。

print("== Full Userdata ==")
local dev = device.open("/dev/null")
print("type(dev)        =", type(dev))
print("dev:name()       =", dev:name())
dev:start()

print()
print("== Light Userdata ==")
local key1 = get_singleton_key()      -- &g_singleton          (struct device*)
local key2 = get_singleton_key_alias() -- &g_singleton.fd       (int*, 但地址数值相同)

print("type(key1)                 =", type(key1))
print("key1 == key2 (地址相同、C 类型不同) =", key1 == key2)

use_singleton_key(key1)
use_singleton_key(key2)

print()
print("== 对 Light Userdata 调用 luaL_checkudata：预期失败 ==")
local ok1, err1 = pcall(try_checkudata, key1)
print("try_checkudata(key1) ok  =", ok1)
print("try_checkudata(key1) err =", err1)

print()
print("== 对 Full Userdata 调用 luaL_checkudata：预期成功 ==")
local ok2 = pcall(try_checkudata, dev)
print("try_checkudata(dev) ok   =", ok2)

print()
print("== 收尾：dev 失效，触发 Full Userdata 的 __gc；key1/key2 不受影响 ==")
dev = nil
collectgarbage()
