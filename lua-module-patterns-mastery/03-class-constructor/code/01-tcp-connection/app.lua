-- app.lua
--
-- Application：require("connection") 只拿到 Connection 这个 Class Module
-- 一次；随后用 Connection.new(...) 创建两个完全独立的 Object：conn_a、
-- conn_b。全程只驱动这两个 Object 各自的 connect/send/close，
-- 证明它们的 state/buffer 互不影响。

local Connection = require("connection")

print("== 创建两个独立的 Connection 实例 ==")
local conn_a = Connection.new("10.0.0.1:8080")
local conn_b = Connection.new("10.0.0.2:9090")

print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 只连接 conn_a，观察 conn_b 是否被影响 ==")
local ok, err = conn_a:connect()
print(string.format("conn_a:connect() -> ok=%s", tostring(ok)))
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 向 conn_a 发送数据，conn_b 的 buffer 必须仍然是 0 ==")
conn_a:send("GET / HTTP/1.1")
conn_a:send("Host: example.com")
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 现在连接 conn_b，并发送不同的数据 ==")
conn_b:connect()
conn_b:send("PING")
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 关闭 conn_a，conn_b 必须仍然保持 connected ==")
conn_a:close()
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 未连接时调用 send 会失败（state 检查） ==")
local send_ok, send_err = conn_a:send("late data")
print(string.format("conn_a:send() -> ok=%s, err=%s", tostring(send_ok), tostring(send_err)))

print("\n== 两个实例共享同一份方法（同一个 Class Table） ==")
print(string.format(
    "conn_a.connect == conn_b.connect -> %s (都 fallback 到同一个 Connection.connect)",
    tostring(conn_a.connect == conn_b.connect)
))
