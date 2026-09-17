-- 完整的双向调用链：
--   Lua 调用 run(callback)
--     -> C  l_run 内部用 lua_pcall 调用这个 Lua callback
--       -> Lua callback 内部又调用了 C 的 process()
--         -> C  l_process 计算结果，返回给 callback
--       <- callback 把 process() 的结果原样返回
--     <- l_run 读到返回值，打印，再返回给最外层 Lua

local final = run(function(value)
    print("[Lua] callback: got", value, "from C, now calling process()")
    return process(value)
end)

print("[Lua] final result =", final)
