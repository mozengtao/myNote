local PATH = "/tmp/lua-c-guide-demo.txt"

print("== Step 1: file.open(path, \"w\") + f:write() + f:close() ==")
local w = file.open(PATH, "w")
w:write("hello\n")
w:close()

print("== Step 2: file.open(path, \"r\") + r:read() ==")
local r = file.open(PATH, "r")
local content = r:read()
print("content =", string.format("%q", content))
assert(content == "hello\n", "content mismatch! write/read 不一致")
r:close()

print("== Step 3: 已经 close 过的 userdata 再次 :close()，必须是安全的 no-op ==")
r:close()

print("== Step 4: 对已关闭的文件继续读写，必须报错而不是崩溃/读到垃圾 ==")
local ok, err = pcall(function() return r:read() end)
print("pcall ok =", ok, "err =", err)
assert(not ok, "对已关闭文件调用 :read() 本应报错")

print("== Step 5: 忘记调用 :close()，__gc 必须兜底关闭 fd，不能永久泄漏 ==")
do
    local leaked = file.open(PATH, "r")
    -- 故意不调用 leaked:close()，让这个 userdata 在 do-block 结束后失去唯一引用
    leaked = nil
end
collectgarbage("collect") -- 强制触发一次完整 GC；日志里应看到 [gc] 行

print("== all checks passed ==")
