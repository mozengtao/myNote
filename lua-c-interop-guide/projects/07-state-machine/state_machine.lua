-- state_machine.lua
--
-- Project 7 的脚本侧只做一件事：扮演 "policy"。
-- 它完全不知道、也不需要知道 C 内部那张状态转移表长什么样，
-- 只是在 on_transition 里声明"每次状态变化时我要做什么"。

print("=== Project 7: C State Machine Exposed to Lua ===")
print()

local fsm = state_machine.create()
print("刚 create() 之后的状态:", fsm:current_state()) -- INIT

fsm:on_transition(function(from, to)
    print(string.format("[lua] transition: %s -> %s", from, to))
end)

print()
print("-- 正常路径：INIT -> IDLE -> CONNECTING -> CONNECTED -> DISCONNECTING -> IDLE --")
fsm:start()             -- INIT -> IDLE
fsm:fire("connect")     -- IDLE -> CONNECTING
fsm:fire("connected")   -- CONNECTING -> CONNECTED
fsm:fire("disconnect")  -- CONNECTED -> DISCONNECTING
fsm:fire("done")        -- DISCONNECTING -> IDLE
print("现在的状态:", fsm:current_state())

print()
print("-- ERROR 状态与恢复：CONNECTING 阶段失败 -> ERROR -> reset 回到 IDLE --")
fsm:fire("connect") -- IDLE -> CONNECTING
fsm:fire("fail")    -- CONNECTING -> ERROR
print("失败后的状态:", fsm:current_state())
fsm:fire("reset")   -- ERROR -> IDLE
print("reset 之后的状态:", fsm:current_state())

print()
print("-- 非法转移：在 IDLE 状态直接 fire('disconnect') --")
-- C 侧 do_fire() 查表失败会直接 luaL_error()，这里用 pcall 兜底，
-- 这正是 "Lua = policy" 的体现：由脚本自己决定"报错之后要不要继续、
-- 要不要重试、要不要上报"，C 只负责"判断这次转移合法不合法"。
local ok, err = pcall(function()
    fsm:fire("disconnect")
end)
print("ok  =", ok)
print("err =", err)
print("非法转移之后的状态（应保持不变）:", fsm:current_state())

print()
print("=== done ===")
