-- debug_plugin.lua
--
-- 第三个独立插件，run() 接受任意数量的参数并把它们打印出来，用于
-- 验证 Framework.execute_command(name, ...) 的可变参数确实被原样
-- 转发到了插件自己的 run 函数——Framework 只转发，不解读参数的
-- 含义，参数的含义完全是插件的 Policy。

local M = {
    name = "debug",
}

function M.run(...)
    local args = { ... }
    local parts = {}
    for i = 1, #args do
        parts[i] = tostring(args[i])
    end
    local msg = "debug_plugin received args: [" .. table.concat(parts, ", ") .. "]"
    print("[debug_plugin] " .. msg)
    return msg
end

return M
