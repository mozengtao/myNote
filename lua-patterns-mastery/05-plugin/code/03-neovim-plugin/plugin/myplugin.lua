-- plugin/myplugin.lua
--
-- Neovim 启动时会自动 source runtimepath 下所有 plugin/*.lua 文件
-- （只执行一次）。这一步只对应 Plugin 生命周期里的 "Load" 阶段：
-- 确认模块可以被 require 到、防止重复加载、提供一个兜底命令，
-- 但**不会自动触发 Setup**。
--
-- 真正的 Setup（注册业务命令 / 快捷键 / autocmd）留给用户在自己的
-- init.lua 里显式调用 require("myplugin").setup({}) 来触发——
-- 这是 Neovim 插件生态的惯例：Load 阶段只做"让插件存在"，
-- 不做"让插件生效"，避免在用户还没决定如何配置之前就产生副作用
-- （例如意外注册命令、意外占用某个快捷键）。
--
-- Host（Neovim）拥有: Buffer / Window / Editor / Event Loop /
--                      命令系统 / 快捷键映射表 / autocmd 系统
-- Lua（本插件）拥有: setup(opts) 契约、写入次数这份 state、
--                    disable() 对应的 Unload 逻辑

-- 防止在开发时反复 :source % 导致重复注册（幂等保护）
if vim.g.loaded_myplugin then
    return
end
vim.g.loaded_myplugin = true

-- 提供一个兜底命令：允许用户完全不写配置代码，
-- 只在命令行执行一次 :MyPluginSetup 即可以默认配置完成 Setup。
vim.api.nvim_create_user_command("MyPluginSetup", function()
    require("myplugin").setup({})
end, {
    desc = "以默认配置触发 myplugin 的 Setup 阶段（等价于 require('myplugin').setup({})）",
})
