-- 每次调用 create_counter() 都会在 C 侧生产一个全新的 C Closure，
-- 各自拥有独立的 Upvalue(1) 状态，互不干扰。

local counter = create_counter()

print(counter())
print(counter())
print(counter())

-- 对照：Lua 自己写一个等价的闭包，捕获的是外层局部变量 n
local function create_lua_counter()
    local n = 0
    return function()
        n = n + 1
        return n
    end
end

local lua_counter = create_lua_counter()
print(lua_counter())
print(lua_counter())

-- 证明每个 counter 都有自己独立的状态，不共享
local counter2 = create_counter()
print("counter2:", counter2())
print("counter (unaffected):", counter())
