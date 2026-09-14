-- hello_plugin.lua
--
-- 一个符合 Command Plugin 契约的插件：name + run(...)。run() 内部
-- 具体做什么完全是这个插件自己的 Policy（这里只是拼一句问候语并
-- 打印），Framework 完全不知道、也不关心这里面的实现细节——它只
-- 知道 hello_plugin 有 name 和 run 这两个契约要求的字段。

local M = {
    name = "hello",
}

function M.run(who)
    who = who or "world"
    local msg = "Hello, " .. who .. "!"
    print("[hello_plugin] " .. msg)
    return msg
end

return M
