-- lua/myplugin/init.lua
--
-- 被 plugin/myplugin.lua 里 require("myplugin") 引用的逻辑模块。
-- Neovim 不会自动加载 lua/ 目录下的文件，只有代码主动 require 时
-- 才会按 package.path 规则找到并执行一次（之后走 module cache）。
--
-- 这个模块负责 Plugin 生命周期里除 Load 之外的全部阶段：
--   Setup             -> M.setup(opts)
--   Register Commands -> M.setup 内部创建 :MyPluginStats
--   Handle Events      -> M.setup 内部注册 BufWritePost autocmd
--   Maintain State      -> 模块级私有表 state（写入次数）
--   Unload / Disable   -> M.disable()

local M = {}

-- 模块级私有状态：只能通过 M.setup()/M.disable()/M.get_write_count()
-- 间接访问或修改，外部代码拿不到 state 本身。
local state = {
    enabled = false,
    write_count = 0,
    augroup = nil,
}

local defaults = {
    notify_on_write = true,
}

local opts = nil

--- BufWritePost 的事件处理函数：每次写入 buffer 后累加计数器。
local function on_buf_write_post()
    state.write_count = state.write_count + 1
    if opts.notify_on_write then
        vim.notify(
            string.format("[myplugin] 已记录第 %d 次写入", state.write_count),
            vim.log.levels.INFO
        )
    end
end

--- 打印当前写入次数统计，供 :MyPluginStats 命令和快捷键调用。
local function print_stats()
    print(string.format("[myplugin] 写入次数: %d", state.write_count))
end

--- Setup 阶段：注册命令、快捷键、autocmd，完成后插件进入 RUNNING。
-- @param user_opts table|nil 用户配置，支持 notify_on_write（boolean）
function M.setup(user_opts)
    if state.enabled then
        vim.notify("[myplugin] 已经处于 enabled 状态，忽略重复 setup", vim.log.levels.WARN)
        return
    end

    opts = vim.tbl_deep_extend("force", defaults, user_opts or {})

    ------------------------------------------------------------
    -- Register Commands
    ------------------------------------------------------------
    vim.api.nvim_create_user_command("MyPluginStats", print_stats, {
        desc = "显示 myplugin 记录的写入次数",
    })

    ------------------------------------------------------------
    -- Register Keymap
    ------------------------------------------------------------
    vim.keymap.set("n", "<leader>ms", print_stats, {
        desc = "myplugin: 显示写入次数统计",
        noremap = true,
        silent = true,
    })

    ------------------------------------------------------------
    -- Register autocmd（Handle Events）
    ------------------------------------------------------------
    state.augroup = vim.api.nvim_create_augroup("MyPluginWriteTracker", { clear = true })
    vim.api.nvim_create_autocmd("BufWritePost", {
        group = state.augroup,
        pattern = "*",
        callback = on_buf_write_post,
        desc = "myplugin: 每次写入 buffer 后累加计数器",
    })

    state.enabled = true
    vim.notify("[myplugin] setup 完成，Plugin 已进入 RUNNING 状态", vim.log.levels.INFO)
end

--- Unload / Disable 阶段：清理 autocmd/command/keymap，写入计数保留。
-- 简化点：真实 Neovim 插件生态里很少有人写这一步（进程退出即等价于
-- 卸载），这里作为教学示例演示"如果要做，应该怎么做"。
function M.disable()
    if not state.enabled then
        vim.notify("[myplugin] 当前未 enabled，无需 disable", vim.log.levels.WARN)
        return
    end

    if state.augroup then
        pcall(vim.api.nvim_del_augroup_by_id, state.augroup)
        state.augroup = nil
    end
    pcall(vim.api.nvim_del_user_command, "MyPluginStats")
    pcall(vim.keymap.del, "n", "<leader>ms")

    state.enabled = false
    vim.notify(
        "[myplugin] 已 disable：autocmd/command/keymap 均已清理（写入计数保留）",
        vim.log.levels.INFO
    )
end

--- 供调试/测试用：读取当前写入计数，不修改状态。
function M.get_write_count()
    return state.write_count
end

--- 供调试/测试用：查询当前是否处于 enabled（RUNNING）状态。
function M.is_enabled()
    return state.enabled
end

return M
