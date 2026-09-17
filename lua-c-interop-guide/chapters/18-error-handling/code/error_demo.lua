-- risky_lua_function：一个会调用 native_operation 的 Lua Function。
-- native_operation 在 C 侧报错时，错误会像普通 Lua Error 一样，
-- 沿着调用链从 C 传播回 Lua——对 Lua 使用者来说，感觉不出这是一次
-- "跨越了 C 边界"的错误。
function risky_lua_function(divisor)
    return native_operation(divisor)
end

print("-- 用 pcall 包裹调用 native_operation(0)（会触发 luaL_error）--")
local ok, err = pcall(function()
    native_operation(0)
end)
print("ok  =", ok)
print("err =", err)

print()
print("-- 用 pcall 包裹调用 native_operation(10)（正常路径）--")
local ok2, result = pcall(function()
    return native_operation(10)
end)
print("ok2    =", ok2)
print("result =", result)
