-- get_hello_function() 从 C 侧拿回一个"函数值"，
-- 它背后是 lua_pushcfunction 包出来的 C Function，
-- 但 Lua 侧完全无法（也不需要）区分它和一个普通 Lua Function。

local f = get_hello_function()

local function lua_hello()
    print("[Lua] lua_hello: hello from Lua!")
end

print("type(f) =", type(f))
print("type(lua_hello) =", type(lua_hello))

-- 两者都用同一个 CALL 语法调用，Lua 侧写法上没有任何区别
f()
lua_hello()

-- f 也可以像任何 Lua Function 一样被继续传递、赋值
local g = f
g()
