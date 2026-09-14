-- lua/myext/init.lua
--
-- 这是被 plugin/myext.lua 里 require("myext") 引用的逻辑模块。
-- Neovim 不会自动加载 lua/ 目录下的文件，只有代码主动 require 时
-- 才会按 package.path 规则找到并执行一次（之后走 module cache）。
--
-- 这个模块负责:
--   1. 维护一个内部计数器（State，闭包变量，模块级私有）
--   2. 提供 hello_count() 函数，每次调用递增计数器并通过
--      vim.notify 显示"这是第 N 次调用"

local M = {}

-- 模块级私有状态：只能通过 M.hello_count() 间接修改，
-- 外部代码拿不到这个变量本身（体现"Lua 侧自己管理 Extension state"）。
local call_count = 0

--- 每次调用递增计数器，并把结果通知给用户。
function M.hello_count()
    call_count = call_count + 1
    vim.notify(string.format("这是第 %d 次调用", call_count), vim.log.levels.INFO, {
        title = "myext.HelloCount",
    })
end

--- 供调试/测试用：读取当前计数值，不修改状态。
function M.get_count()
    return call_count
end

--- 供调试/测试用：重置计数器。
function M.reset()
    call_count = 0
end

return M
