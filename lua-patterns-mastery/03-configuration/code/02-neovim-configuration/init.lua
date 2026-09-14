-- init.lua
--
-- 一份可以直接当作 ~/.config/nvim/init.lua 使用、也可以用
-- `nvim -u init.lua` 临时加载来验证效果的最小 Neovim 配置。
--
-- 本文件严格分成三块，并在注释里标注每一块属于 Configuration
-- 还是已经越界成 Extension，参见同目录下的
-- 02-example-neovim-configuration.md 的"混合体"一节。

--------------------------------------------------------------------------
-- 第一块：纯 Configuration —— 纯粹的"运行时参数应该是什么"，
-- 没有任何"新增能力"，Host（Neovim 内核）本身已经认识这些选项，
-- 这里只是给它们赋值。
--------------------------------------------------------------------------

vim.opt.number = true         -- 显示行号
vim.opt.tabstop = 4           -- Tab 显示宽度为 4 列
vim.opt.shiftwidth = 4        -- 自动缩进宽度为 4 列
vim.opt.expandtab = true      -- 用空格代替 Tab 字符
vim.opt.termguicolors = true  -- 启用 24-bit 真彩色

--------------------------------------------------------------------------
-- 第二块：仍然是 Configuration，但带有"计算能力"——
-- 根据运行环境做条件判断，而不是写死一个值。
-- 这正是 Lua Configuration 相对于纯静态 *.ini / *.toml 的核心优势：
-- 同一份配置文件，在不同机器/不同环境下可以自动得出不同的最终参数。
--------------------------------------------------------------------------

-- 条件逻辑 1：只在 macOS 上开启一个 mac 专属选项（这里用剪贴板举例，
-- 因为 macOS 的系统剪贴板集成方式和 Linux 不同）。
if vim.fn.has("mac") == 1 then
    vim.opt.clipboard = "unnamed"
else
    vim.opt.clipboard = "unnamedplus"
end

-- 条件逻辑 2：根据环境变量决定是否开启"精简模式"
-- （例如在 SSH 远程会话里关闭一些渲染开销较大的选项）。
if vim.env.NVIM_LITE_MODE == "1" then
    vim.opt.cursorline = false
    vim.opt.signcolumn = "no"
else
    vim.opt.cursorline = true
    vim.opt.signcolumn = "yes"
end

-- 可计算配置项：用一个函数根据终端宽度算出一个"合理的" colorcolumn 位置，
-- 而不是写死 80。vim.g 在这里被当作"计算结果的存放处"，
-- 本质仍然是配置数据，只是数据来自函数调用而不是字面量。
local function compute_colorcolumn()
    local width = vim.o.columns or 80
    if width >= 120 then
        return "100"
    end
    return "80"
end

vim.g.my_colorcolumn = compute_colorcolumn()
vim.opt.colorcolumn = vim.g.my_colorcolumn

--------------------------------------------------------------------------
-- 第三块：越界示例（有意保留，仅用于对比，实际使用时可删除）——
-- 下面这几行不是在"描述参数"，而是在"注册新的行为/入口"，
-- 一旦写下去，这个 init.lua 就不再是纯 Configuration，
-- 而是混入了 Extension（新命令）和 Plugin（生命周期钩子）的成分。
-- 详见 .md 文档中的"为什么会混合"一节。
--------------------------------------------------------------------------

-- 越界成 Extension：注册了一个新命令 `:HelloConfig`，
-- 这是"新增能力"，不是"描述参数"。
vim.api.nvim_create_user_command("HelloConfig", function()
    print("Hello from init.lua's user command")
end, {})

-- 越界成 Plugin 风格：注册了一个生命周期钩子（autocmd），
-- 它有自己的"触发时机"，不是启动时读一次就结束的参数。
vim.api.nvim_create_autocmd("BufWritePre", {
    pattern = "*.lua",
    callback = function()
        print("[init.lua] 即将保存一个 .lua 文件")
    end,
})
