print("== Step 1: socket.pair() 创建一对已连接的本地 socket ==")
local client, server = socket.pair()

print("== Step 2: client:send() -> server:recv() ==")
local msg = "hello from client\n"
local sent = client:send(msg)
print("sent bytes =", sent)

local got = server:recv(#msg)
print("server got =", string.format("%q", got))
assert(got == msg, "content mismatch! send/recv 不一致")

print("== Step 3: server:send() -> client:recv()（反向验证） ==")
local reply = "ack from server\n"
server:send(reply)
local got2 = client:recv(#reply)
print("client got =", string.format("%q", got2))
assert(got2 == reply, "reply mismatch! send/recv 不一致")

print("== Step 4: 已经 close 过的 userdata 再次 :close()，必须是安全的 no-op ==")
client:close()
client:close()

print("== Step 5: 对已关闭的 socket 继续读写，必须报错而不是崩溃 ==")
local ok, err = pcall(function() return client:send("oops") end)
print("pcall ok =", ok, "err =", err)
assert(not ok, "对已关闭 socket 调用 :send() 本应报错")

print("== Step 6: 忘记调用 :close()，__gc 必须兜底关闭 fd，不能永久泄漏 ==")
do
    local leaked_client, leaked_server = socket.pair()
    -- 故意不调用 :close()，让两个 userdata 在 do-block 结束后失去唯一引用
    leaked_client = nil
    leaked_server = nil
end
collectgarbage("collect") -- 强制触发一次完整 GC；日志里应看到两行 [gc]

server:close()

print("== all checks passed ==")
