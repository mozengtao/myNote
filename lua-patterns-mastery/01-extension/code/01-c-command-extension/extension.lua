-- extension.lua
-- 这是 Host（main.c）在启动时加载的 Lua 脚本。
-- 它的职责只有一件事：调用 Host 暴露的 register_command()，
-- 把"新命令该怎么做"这件事注册进 Host，而不修改 Host 的任何一行 C 代码。

-- 命令 1：hello
-- 用法：hello <name>
register_command("hello", function(...)
    local args = { ... }
    local name = args[1] or "world"
    print(string.format("Hello, %s! (来自 Lua 的问候)", name))
    return true
end)

-- 命令 2：add
-- 用法：add <a> <b>  -- 把两个数字相加，演示参数解析 + 简单错误处理
register_command("add", function(...)
    local args = { ... }
    local a = tonumber(args[1])
    local b = tonumber(args[2])
    if not a or not b then
        -- 用 error() 主动抛出一个 Lua 运行时错误，
        -- Host 侧的 lua_pcall 会捕获它，转换成"命令执行失败"而不会让进程崩溃。
        error("add 需要两个数字参数，例如: add 3 5")
    end
    print(string.format("%g + %g = %g", a, b, a + b))
    return true
end)

-- 命令 3：boom
-- 用法：boom  -- 故意抛出错误，用来演示"Lua 回调运行时报错"这条失败路径
register_command("boom", function(...)
    error("boom 命令故意抛出的错误：模拟脚本 bug")
end)

-- 命令 4：state
-- 用法：state  -- 演示 Lua 侧的模块级局部状态（闭包变量），每次调用递增
local call_count = 0
register_command("state", function(...)
    call_count = call_count + 1
    print(string.format("state 命令被调用了 %d 次", call_count))
    return true
end)

print("[extension.lua] 已加载，注册了 4 个命令：hello / add / boom / state")
