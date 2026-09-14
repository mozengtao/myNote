# 03 · 示例3：Neovim Lua Plugin

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。本文用 Neovim 展示 Plugin 六步生命周期里，除 Wireshark 之外的另一种落地方式——这次 Unload/Disable 阶段是**可以真正写出来的**，只是生态里很少有人这么做。

## 1. Problem

写 Neovim 配置时，简单的行为（改一个选项、设一个变量）用 [03-configuration/02-example-neovim-configuration.md](../03-configuration/02-example-neovim-configuration.md) 里的纯 `vim.opt` 赋值就够了。但如果想做的事情更复杂——需要一个可以带参数配置的入口函数、需要自己维护一段跨事件持续存在的状态、需要在不同场景下"启用"或"关闭"这部分行为——单纯的选项赋值就不够用了，需要一个真正意义上的、**有独立生命周期的模块**。

## 2. Why Lua

Neovim 从 0.5 开始把 Lua 作为一等公民集成进编辑器内核：`vim.api`、`vim.keymap`、`vim.notify` 这些都是 Neovim 用 C 实现、通过 Lua Binding 暴露出来的能力。Neovim 有自己的 `require` 机制（基于 `runtimepath` 找 `lua/` 目录下的模块），也有自己的自动加载机制（`plugin/` 目录下的文件启动时自动 `source`），这两者结合起来，天然支持"一个目录结构就是一个可以被发现、被加载的插件"，不需要额外发明任何 Discovery/Load 机制。

## 3. Architecture

```text
plugin/
  myplugin.lua
```

配合：

```text
lua/
  myplugin/
    init.lua
```

`plugin/myplugin.lua` 对应 Load 阶段（Neovim 启动时自动执行一次）；`lua/myplugin/init.lua` 对应这个插件真正的逻辑模块，只有被 `require("myplugin")` 时才会执行——这是 Neovim 插件生态里"两段式加载"的标准结构：`plugin/` 下的文件负责"存在感"（尽量轻，不做重的初始化），`lua/<name>/` 下的模块负责"实际能力"（重的逻辑放这里，惰性加载）。

## 4. Host responsibilities

- **Discovery + Load**：Neovim 启动时自动扫描 `runtimepath` 下所有插件目录的 `plugin/*.lua`，逐一 `source`（等价于执行一次这个 Lua 文件），不需要用户显式调用任何加载函数。
- **提供 `require` 机制**：当 `plugin/myplugin.lua` 里出现 `require("myplugin")` 或 `require("myplugin").setup(...)` 时，Neovim 按 `runtimepath` 里的 `lua/myplugin/init.lua` 找到并执行这个模块的顶层代码，且只执行一次（后续 `require` 直接返回缓存的模块对象）。
- **提供 Stable API**：`vim.api.nvim_create_user_command`、`vim.keymap.set`、`vim.api.nvim_create_autocmd`/`nvim_create_augroup`、`vim.notify` 等，都是 Neovim 承诺"签名稳定"的 Binding 函数。
- Host 本身不知道、也不关心 `myplugin` 内部维护了什么状态、注册了哪个具体命令名——它只提供"注册的机制"。

## 5. Lua responsibilities

- `plugin/myplugin.lua`：做幂等保护（避免重复 source 导致重复注册），提供一个兜底的 `:MyPluginSetup` 命令，但**不主动调用 `setup`**——这是刻意的设计，把"是否启用、怎么配置"的决定权完全留给使用者。
- `lua/myplugin/init.lua`：
  - `M.setup(opts)`：合并用户配置与默认配置，注册命令、快捷键、autocmd，把内部状态 `enabled` 置为 `true`。
  - 模块级私有表 `state`：维护 `write_count`（写入次数）和 `augroup`（autocmd 组 ID，用于后续清理）。
  - `M.disable()`：清理已注册的 autocmd/command/keymap，把 `enabled` 置为 `false`，但保留 `write_count`——体现"Unload 清理的是注册痕迹，不一定要清空业务数据"这个设计选择。

## 6. Lifecycle

```text
Load
 |  Neovim 启动，自动 source plugin/myplugin.lua
 v
Setup
 |  用户在 init.lua 里调用 require("myplugin").setup({})
 v
Register Commands
 |  :MyPluginStats 命令、<leader>ms 快捷键
 v
Handle Events
 |  BufWritePost autocmd 持续触发
 v
Maintain State
 |  state.write_count 持续累加
 v
Unload / Disable
    调用 M.disable()，清理 autocmd/command/keymap
```

对应到代码：`plugin/myplugin.lua` 只负责第一格 Load；`lua/myplugin/init.lua` 的 `M.setup` 一次性完成 Setup + Register Commands + Handle Events（注册 autocmd）三格；Maintain State 贯穿整个运行期，不是某个单独的函数调用；`M.disable()` 对应最后一格。

## 7. Data Flow（C -> Lua）

- Neovim 核心（C 实现）在用户执行 `:w`（写入文件）后，触发内部的 `BufWritePost` 事件，把当前 buffer 的编号等上下文信息通过 autocmd 回调的参数传给 Lua 侧的 `on_buf_write_post`（本示例简化为不使用参数，只是单纯计数，真实场景可以从回调参数里的 `buf`/`file` 字段读出具体是哪个文件被写入）。
- `M.setup(opts)` 里的 `opts` 是用户从 `init.lua` 传进来的配置表，这是"外部数据流进 Lua 模块"的入口。

## 8. Control Flow（Lua -> C）

- `vim.api.nvim_create_user_command(...)`、`vim.keymap.set(...)`、`vim.api.nvim_create_autocmd(...)` 都是 Lua 主动调用 Neovim 提供的 C Binding，把"以后遇到 X 请调用我的 Y 函数"这件事注册进 Neovim 内核——这是插件反向影响 Host 行为的唯一途径，和示例1里插件调用 `host_log`、示例2里插件调用 `DissectorTable:add` 是同一类模式。
- `M.disable()` 里调用 `nvim_del_augroup_by_id`/`nvim_del_user_command`/`vim.keymap.del`，则是"反向撤销之前注册过的内容"，这是本示例区别于前两个示例的地方——前两个示例都没有演示"撤销注册"这个方向的控制流。

## 9. State

- **Host 侧 State**：Neovim 内部维护的命令表、快捷键映射表、autocmd 注册表，`myplugin` 只是往里增加/删除了几条记录，看不到、也不需要看到其他插件注册了什么。
- **Lua 侧 State**：`lua/myplugin/init.lua` 里的模块级私有表 `state`，外部代码（包括 `plugin/myplugin.lua`）拿不到这个表本身，只能通过 `M.get_write_count()`/`M.is_enabled()` 间接读取。`state.write_count` 是跨越整个 Neovim 会话持续存在的（只要没有重启 Neovim），`state.augroup` 则是"当前这次 enable 期间"的一次性 ID，`disable()` 之后会被置为 `nil`，下次 `setup()` 时重新创建。

## 10. Failure

- **`setup(opts)` 被调用两次**：本示例显式检测 `state.enabled`，如果已经是 `true`，直接 `vim.notify` 一条警告并 `return`，不会重复注册两份相同的 command/autocmd（重复注册的常见后果是同一个事件被处理两次，或者后一次注册悄悄覆盖前一次，具体行为取决于 Neovim 对同名注册的处理方式，属于容易被忽视的坑）。
- **`disable()` 在未 `enabled` 时被调用**：同样做了防御性检查，避免对不存在的 `augroup`/命令做删除操作抛错。
- **`disable()` 内部清理逻辑本身出错**：`nvim_del_augroup_by_id`/`nvim_del_user_command`/`keymap.del` 都用 `pcall` 包裹——如果某一步失败（例如用户手动删过这个命令），不会导致整个 `disable()` 中断、也不会导致其他清理步骤被跳过，这是 Plugin 模式"某一步失败不影响其他步骤"这条原则在 Unload 阶段的具体体现，呼应示例1里 Event 阶段的错误隔离设计。
- **`opts.notify_on_write` 为 `nil` 时的容错**：用 `vim.tbl_deep_extend("force", defaults, user_opts or {})` 合并，保证即使用户传 `nil` 或空表也有合理默认值，不会在 `on_buf_write_post` 里因为访问不存在的字段而报错。

## 11. Complete Host Code

Host 是 Neovim 自身（C 实现的编辑器内核），其源码不在本笔记讨论范围内。Neovim 对 Lua 插件暴露的是一整套 Stable API（`vim.api.*`、`vim.keymap.*`、`vim.notify`、`vim.tbl_deep_extend` 等），这些函数背后对应的是 Neovim C 代码里的具体实现，插件作者只需要按文档调用，不需要关心其内部实现。

## 12. Complete Lua Code

`plugin/myplugin.lua`（Load 阶段，Neovim 启动时自动执行）：

```lua
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
```

`lua/myplugin/init.lua`（Setup + Register Commands + Handle Events + Maintain State + Unload/Disable）：

```lua
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
```

## 13. Execution

目录结构放进 `~/.config/nvim/`：

```text
~/.config/nvim/
  init.lua
  plugin/
    myplugin.lua
  lua/
    myplugin/
      init.lua
```

`plugin/` 下的文件启动时自动加载；`lua/myplugin/` 下的模块只有被 `require` 时才惰性加载。在 `init.lua` 里显式触发 Setup 阶段：

```lua
-- ~/.config/nvim/init.lua
require("myplugin").setup({
    notify_on_write = true,
})
```

三条路径的手动测试方式：

- **Command 路径**：执行 `:MyPluginStats`，应该打印 `[myplugin] 写入次数: 0`（或当前累计值）。
- **Keymap 路径**：在 Normal 模式下按 `<leader>ms`（默认 leader 键是 `\`，视个人配置而定），效果应该和执行 `:MyPluginStats` 完全一致。
- **Autocmd 路径**：打开任意一个文件，随便改动内容后执行 `:w` 保存，应该看到 `vim.notify` 弹出的提示"已记录第 1 次写入"；再保存一次会看到"第 2 次写入"，此时执行 `:MyPluginStats` 应该显示 `2`。
- **Disable 路径**：执行 `:lua require("myplugin").disable()`，之后再执行 `:w` 不会再弹出写入提示，执行 `:MyPluginStats` 会报错"命令不存在"（因为命令已被删除），但如果重新 `:lua require("myplugin").setup({})`，会看到写入计数从 disable 前的值继续累加，而不是从 0 重新开始——验证了"Unload 清理的是注册痕迹，不清空业务数据"这一设计。

## 14. Step-by-step walkthrough

1. Neovim 启动，`runtimepath` 扫描到 `plugin/myplugin.lua`，自动执行：检查 `vim.g.loaded_myplugin`（第一次是 `nil`），设置为 `true`，注册 `:MyPluginSetup` 兜底命令。此时 `lua/myplugin/init.lua` 还完全没有被执行过。
2. Neovim 继续执行用户的 `init.lua`，遇到 `require("myplugin").setup({...})`：`require` 第一次找到 `lua/myplugin/init.lua`，执行它的顶层代码（定义 `state`/`defaults`/`M` 上的各个函数），返回模块表 `M`；随即调用 `M.setup(opts)`。
3. `M.setup` 判断 `state.enabled == false`，继续往下走：合并 `opts`，依次调用 `nvim_create_user_command`、`vim.keymap.set`、`nvim_create_augroup` + `nvim_create_autocmd`，最后把 `state.enabled` 置为 `true`，弹出一条"setup 完成"的通知。
4. 用户执行 `:w` 保存当前文件：Neovim 内核触发 `BufWritePost` 事件，找到 `MyPluginWriteTracker` 这个 augroup 下注册的回调 `on_buf_write_post`，调用它——`state.write_count` 从 0 变成 1，`opts.notify_on_write` 为真，弹出通知。
5. 用户执行 `:MyPluginStats`：这是通过 `nvim_create_user_command` 注册进 Neovim 命令系统的命令，触发时直接调用 `print_stats`，打印当前 `state.write_count`（此时是 1）。
6. 用户按下 `<leader>ms`：这是通过 `vim.keymap.set` 注册的快捷键，效果和上一步的命令完全一样（两者共用同一个 `print_stats` 函数）。
7. 用户再保存一次文件，`state.write_count` 变成 2；再执行一次 `:MyPluginStats` 确认输出变为 2，证明状态在多次事件之间正确累积。
8. 用户执行 `require("myplugin").disable()`：`pcall` 依次清理 augroup、user command、keymap，`state.enabled` 置回 `false`，但 `state.write_count` 保持为 2 不变。
9. 用户再执行 `:w`：由于 augroup 已被删除，`on_buf_write_post` 不会再被触发，写入计数停留在 2；此时执行 `:MyPluginStats` 会报"命令不存在"的错误，因为该命令已在第 8 步被删除。

## 15. Mental Model

```text
Host（Neovim 编辑器内核，C 实现）
 |
 +-- Mechanism   : Buffer/Window 管理、Event Loop、命令系统、快捷键系统
 +-- Resource    : autocmd 组 ID、命令注册表条目、keymap 表条目
 +-- Lifecycle   : Load（自动 source plugin/）与 require 缓存机制
 +-- Stable API  : vim.api.* / vim.keymap.* / vim.notify / vim.tbl_deep_extend
          |
          v
         Lua（myplugin：plugin/myplugin.lua + lua/myplugin/init.lua）
          |
 +-- Behavior    : on_buf_write_post / print_stats 具体做什么
 +-- State       : state.write_count（跨事件持续存在）、state.enabled、state.augroup
 +-- Extension   : MyPluginStats 命令、<leader>ms 快捷键都是新增给 Neovim 的能力
```

对比 [03-configuration/02-example-neovim-configuration.md](../03-configuration/02-example-neovim-configuration.md)：那份文档里 Lua 侧几乎只有"值"（对 `vim.opt.xxx` 赋值），没有独立的 `setup(opts)` 契约、没有需要跨事件维护的内部状态、也没有"启用/关闭"这种生命周期概念——纯 Configuration 更接近"声明式地告诉 Neovim 一组参数应该是什么"。而这里的 `myplugin` 有自己的入口函数契约（`setup(opts)`）、自己的内部状态（`write_count`）、自己的启停生命周期函数（`setup`/`disable`），这正是让它归类为 **Plugin** 而不是单纯 Configuration 的关键区别——它不是在"设置一个值"，而是在"启动一个会持续产生行为的模块"。

## 16. Why this is this Pattern

这个例子归类为 **Plugin** 的关键证据：拥有独立于 Load 阶段的显式 `setup(opts)` 契约（不是加载完就自动生效）、拥有跨事件持续存在且外部不可直接修改的 State（`write_count`）、拥有命令 + 快捷键 + autocmd 三种不同类型的 Register 动作、拥有可以真正执行的 `disable()`（对应 Unload），这四点合在一起，比单纯的 [01-extension/03-example-neovim-extension.md](../01-extension/03-example-neovim-extension.md)（只新增一个命令+快捷键，没有 setup 契约、没有 disable）更完整，也比 03-configuration 的纯选项赋值更"模块化"。

**如果 Lua 消失，Host 还能做什么？** Neovim 核心的 Buffer/Window/Event Loop/命令系统/快捷键系统全部是 C 实现，完全不依赖任何具体插件的 Lua 代码；Lua 消失后，Neovim 依然是一个功能完整的编辑器，只是失去了`myplugin`这类第三方扩展出来的具体行为（写入计数统计、`:MyPluginStats` 命令）。用户仍然可以用最原始的 `:autocmd`/`:command`（Vimscript）达到类似效果，只是失去了 Lua 版本更清晰的模块化组织方式（`setup(opts)`、闭包私有状态）。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** 如果换成 JSON/YAML：完全无法表达——JSON/YAML 只能声明"BufWritePost 时执行哪个命令字符串"这种静态映射，没有办法维护 `write_count` 这种运行时累加的状态，也没有办法表达"disable 时按什么顺序清理什么资源"这种过程性逻辑，Setup/Handle Events/Maintain State 这三格会全部消失，退化成一份纯粹的静态映射表（顶多算是最简单的 Rule Engine 或 Configuration）。如果换成纯 C（把 Neovim 编译进一个内置 C 插件），Pattern 本身依然成立（`setup`/`disable`/State 这些概念在 C 里同样可以实现），但代价是每次调整行为都要重新编译整个 Neovim，完全违背了"用户/插件作者应该能在不重新编译编辑器的前提下扩展它"这个 Neovim 插件生态存在的初始理由——这也是为什么 Neovim 选择把 Lua 作为插件开发的第一语言，而不是要求插件作者写 C。
