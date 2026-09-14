-- plugin/myext.lua
--
-- Neovim 启动时会自动 source runtimepath 下所有 plugin/*.lua 文件
-- （只执行一次）。这个文件的职责是"注册"：告诉 Neovim 新增了
-- 一个用户命令和一个快捷键，具体逻辑委托给 lua/myext/init.lua。
--
-- Host（Neovim）拥有: Buffer / Window / Editor / Event Loop /
--                      命令系统 / 快捷键映射表
-- Lua（本插件）拥有: Extension behavior（HelloCount 具体做什么）
--                    以及内部计数器这份 state

-- 防止在开发时反复 :source % 导致重复注册（可选的幂等保护）
if vim.g.loaded_myext then
    return
end
vim.g.loaded_myext = true

-- 真正的业务逻辑放在 lua/myext/init.lua 里，这里只 require 它
local myext = require("myext")

------------------------------------------------------------
-- 1. 注册用户命令 :HelloCount
------------------------------------------------------------
vim.api.nvim_create_user_command("HelloCount", function()
    myext.hello_count()
end, {
    desc = "打印这是第 N 次调用（演示 Neovim Lua Extension 状态维护）",
})

------------------------------------------------------------
-- 2. 注册快捷键 <leader>hc，触发同样的行为
------------------------------------------------------------
vim.keymap.set("n", "<leader>hc", function()
    myext.hello_count()
end, {
    desc = "HelloCount: 打印这是第 N 次调用",
    noremap = true,
    silent = true,
})
