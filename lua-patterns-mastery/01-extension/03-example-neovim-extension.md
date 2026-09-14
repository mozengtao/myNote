# 01-3 · 示例：Neovim Lua Extension

> Pattern：Extension（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/03-neovim-extension/](code/03-neovim-extension/)（`plugin/myext.lua` / `lua/myext/init.lua`）。

## 1. Problem

Neovim 原生没有 `:HelloCount` 这个命令，也没有 `<leader>hc` 这个快捷键——这是我们想要凭空新增的能力。传统上如果要给编辑器加一个新命令，需要修改编辑器本体源码、重新编译；我们希望的效果是：只在配置目录里放两个 `.lua` 文件，Neovim 启动时自动"学会"这个新命令和新快捷键，不改 Neovim 本体一行代码。

## 2. Why Lua

- Neovim 自 0.5 起把 Lua 作为一等公民的配置/扩展语言，`vim.api.*` 是官方维护的稳定 C API 绑定（对应 00 篇的 Stable API），专门设计用来让用户在不碰 Neovim C 源码的前提下新增命令、快捷键、自动命令等能力；
- Lua 的模块系统（`require`）天然契合"注册代码"和"业务逻辑"分离到不同文件的需求：`plugin/` 目录下的文件负责启动时自动执行的注册动作，`lua/<name>/init.lua` 负责可以被 `require` 复用、可以被其他插件依赖的实际逻辑；
- Neovim 的事件循环是单线程、协作式的，Lua 回调在被事件触发时执行一小段代码后立刻把控制权还给 Neovim，这与 Lua"轻量级、可反复短时间借用 CPU"的定位完全契合；Lua 运行时错误会被 Neovim 捕获并在消息区提示，不会导致编辑器崩溃。

## 3. Architecture

```text
Neovim (Host)
    |
    +-- Core: Buffer / Window / Editor / Event Loop
    |
    +-- API: vim.api.nvim_create_user_command / vim.keymap.set / vim.notify
    |
    +-- Lua Runtime
           |
           +-- plugin/myext.lua   (启动时自动 source，负责注册)
                    |
                    +-- require("myext")
                            |
                            v
           +-- lua/myext/init.lua (真正的逻辑 + 状态)
```

Host owns：Buffer（文本内容）、Window（视图布局）、Editor（全局状态、命令系统、快捷键映射表）、Event Loop（键盘输入、定时器、异步任务调度）。
Lua owns：Extension behavior（`:HelloCount` 具体打印什么）、Extension state（内部计数器 `call_count`）。

## 4. Host responsibilities

- 启动时自动扫描并执行 `runtimepath` 下所有 `plugin/*.lua` 文件（这一步等价于示例 1 里 Host 加载 `extension.lua`）；
- 提供 `vim.api.nvim_create_user_command(name, callback, opts)`：把一个新命令名注册进 Neovim 的命令系统；
- 提供 `vim.keymap.set(mode, lhs, callback, opts)`：把一个按键序列映射到一个回调；
- 维护 `package.path`/`runtimepath`，让 `require("myext")` 能正确找到 `lua/myext/init.lua`（Neovim 特有的模块查找规则，等价于示例 1 里 C 侧的 `find_command` 查表机制）；
- 在用户真正执行 `:HelloCount` 或按下 `<leader>hc` 时，从命令表/keymap 表里取出之前注册的 Lua 函数并调用它；
- 用内部的错误捕获机制包裹这次调用，脚本报错只在消息区显示一条错误，不影响编辑器继续运行。

## 5. Lua responsibilities

- `plugin/myext.lua`：在加载时立即调用 `vim.api.nvim_create_user_command` 和 `vim.keymap.set`，把"新命令""新快捷键"这两件事登记进 Host；用 `vim.g.loaded_myext` 做一次简单的幂等保护，避免重复 `:source` 导致重复注册；
- `lua/myext/init.lua`：维护模块级私有状态 `call_count`（一个 Lua 闭包变量），提供 `hello_count()`：每次调用递增计数器并用 `vim.notify` 显示"这是第 N 次调用"；同时提供 `get_count()`/`reset()` 两个辅助函数用于调试和测试。

## 6. Lifecycle

```text
Neovim 启动
  |
  v
扫描 runtimepath 下 plugin/*.lua
  |
  v
执行 plugin/myext.lua（一次）
  |
  v
require("myext")  <-- 触发加载并执行 lua/myext/init.lua（一次，结果被模块缓存）
  |
  v
nvim_create_user_command("HelloCount", ...)
vim.keymap.set("n", "<leader>hc", ...)
  |
  v
Neovim 进入正常编辑状态（Extension 待命）
  |
  v
用户执行 :HelloCount 或按 <leader>hc
  |
  v
Host 调用之前注册的回调 -> myext.hello_count() 执行
  |
  v
（重复触发，call_count 持续累加）
  |
  v
Neovim 退出，整个 Lua 运行时随进程一起结束
```

## 7. Data Flow（C -> Lua，这里用"Host -> Lua"泛化）

Neovim 本体是 C 实现，`vim.api.*` 是对底层 C 函数的 Lua 绑定，所以这里的"C -> Lua"对应"Neovim Host -> Lua Extension"：

1. Neovim 启动阶段扫描到 `plugin/myext.lua`，把文件内容编译执行；
2. 执行过程中，Lua 侧读取不到任何 Host 内部状态（这一步是纯"注册"，没有真正的数据从 Host 流向 Lua）；
3. 真正的数据流发生在用户触发命令时：Host 把触发上下文（对于 `nvim_create_user_command` 是一个包含 `args`/`fargs`/`bang` 等字段的 table）作为参数传给回调——本示例的 `HelloCount` 没有用到这些参数，但架构上这条通道是存在的，等价于示例 1 里"C 把命令行参数压栈传给 Lua"。

## 8. Control Flow（Lua -> Host）

- `vim.api.nvim_create_user_command(...)`：Lua 主动调用 Host API，把一条新记录写进 Neovim 的命令表——这是"注册阶段"的 Control Flow；
- `vim.keymap.set(...)`：同理，写进 Neovim 的按键映射表；
- `vim.notify(msg, level, opts)`：在 `hello_count()` 执行期间，Lua 主动调用 Host API 在编辑器界面弹出一条通知——这是"运行阶段"的 Control Flow，Lua 借助 Host 暴露的 UI 能力把结果"显示"出来，而不是简单地 `return` 一个值。

## 9. State

- **Host 侧 state**：Neovim 内部维护的命令表、keymap 表（记录着"HelloCount"名字和"<leader>hc"按键分别对应哪个 Lua 函数引用），这部分对 Lua 不可见，只能通过 API 间接追加；
- **Lua 侧 state**：`lua/myext/init.lua` 里的 `call_count`，是模块级闭包变量。因为 Lua 的 `require` 结果会被缓存（`package.loaded.myext`），所以只要 Neovim 进程不重启、模块不被显式 `unload`，`call_count` 就会一直存活并持续递增，这正是题目要求的"计数递增"效果的状态基础；
-`plugin/myext.lua` 里的 `vim.g.loaded_myext` 是全局变量形式的状态，用来防止重复注册，这是"Extension 自我保护"的一种常见写法。

## 10. Failure

- **重复加载/重复 `:source %`**：如果没有 `vim.g.loaded_myext` 保护，重复执行 `plugin/myext.lua` 会导致 `nvim_create_user_command` 被调用两次；Neovim 对同名命令重新注册通常是"覆盖"而不是报错，但仍然可能导致 keymap 重复绑定的混乱，本示例用一个全局标志位提前 `return` 规避；
- **回调内部抛出 Lua 错误**：如果 `hello_count()` 内部出错（例如误传了不存在的 `vim.log.levels` 字段），Neovim 会在 `:messages` 里显示一条 `E5108: Error executing lua ...` 形式的错误，命令执行失败，但编辑器本身、当前 Buffer 内容、其他插件都不受影响，可以继续正常编辑；
- **`require("myext")` 找不到模块**：如果 `lua/myext/init.lua` 没有被放进正确的 `runtimepath` 下的 `lua/myext/` 目录，`require` 会抛出 `module 'myext' not found` 错误，`plugin/myext.lua` 加载失败，`:HelloCount` 命令不会被注册，但 Neovim 本身正常启动，只是缺少了这个 Extension 的能力。

## 11. Complete Host Code

本示例的 Host 就是 Neovim 本体（C 实现，不在本仓库范围内重新贴出源码）。Host 侧的行为契约已经在第 3~8 节通过 `vim.api.*` 完整描述；本示例的"Host 代码"角色，由 Neovim 内置的 `vim.api`/`vim.keymap`/`vim.notify` 承担。

## 12. Complete Lua Code

`plugin/myext.lua` 与 [code/03-neovim-extension/plugin/myext.lua](code/03-neovim-extension/plugin/myext.lua) 完全一致：

```lua
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
```

`lua/myext/init.lua` 与 [code/03-neovim-extension/lua/myext/init.lua](code/03-neovim-extension/lua/myext/init.lua) 完全一致：

```lua
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
```

## 13. Execution

**部署步骤**（把这两个文件放进 Neovim 的 `runtimepath`）：

```bash
mkdir -p ~/.config/nvim/plugin ~/.config/nvim/lua/myext
cp code/03-neovim-extension/plugin/myext.lua   ~/.config/nvim/plugin/myext.lua
cp code/03-neovim-extension/lua/myext/init.lua ~/.config/nvim/lua/myext/init.lua
```

目录结构效果：

```text
~/.config/nvim/
  plugin/
    myext.lua
  lua/
    myext/
      init.lua
```

**加载方式**：

- 重启 Neovim：Neovim 启动时会自动 source `plugin/myext.lua`，无需任何额外配置；
- 或者在已经打开的 Neovim 里，编辑完 `plugin/myext.lua` 后执行 `:source %` 手动重新加载当前文件（因为有 `vim.g.loaded_myext` 保护，重复 `:source` 是安全的，不会重复注册）；
- 如果修改的是 `lua/myext/init.lua`，因为 `require` 结果会被缓存，需要先 `:lua package.loaded.myext = nil` 清缓存，再重新 `:source` 一次 `plugin/myext.lua`，或者直接重启 Neovim。

**手动测试**：

```text
:HelloCount
-- 消息区/notify 弹出: 这是第 1 次调用

:HelloCount
-- 消息区/notify 弹出: 这是第 2 次调用

" 按下 <leader>hc（假设 leader 是空格键，即按 空格 h c）
-- 消息区/notify 弹出: 这是第 3 次调用
```

可以看到命令和快捷键触发的是同一个 `hello_count()`，共享同一份 `call_count` 状态，计数持续递增而不是各自独立计数。

## 14. Step-by-step walkthrough

以"启动 Neovim -> 执行两次 `:HelloCount`"为例：

1. Neovim 启动，扫描 `runtimepath`，发现 `plugin/myext.lua`，执行它；
2. `vim.g.loaded_myext` 当前是 `nil`，条件不成立，继续执行；
3. `vim.g.loaded_myext = true` 设置标志位；
4. `require("myext")` 第一次被调用，Neovim 按 `lua/myext/init.lua` 路径找到并执行该文件，得到返回的模块表 `M`（此时 `call_count = 0`），并把这个结果缓存进 `package.loaded.myext`；
5. `vim.api.nvim_create_user_command("HelloCount", ...)` 把命令名和一个闭包（内部调用 `myext.hello_count()`）注册进 Neovim 的命令表；
6. `vim.keymap.set("n", "<leader>hc", ...)` 把按键序列和另一个等价的闭包注册进 keymap 表；
7. 用户在普通模式下输入 `:HelloCount<CR>`，Neovim 命令系统查表命中，调用之前注册的闭包；
8. 闭包内部调用 `myext.hello_count()`，`call_count` 从 0 变为 1，`vim.notify` 弹出"这是第 1 次调用"；
9. 用户再次输入 `:HelloCount<CR>`，重复步骤 7~8，但这次 `call_count` 已经是 1，递增后变成 2，弹出"这是第 2 次调用"；
10. 两次调用共享同一个 `call_count` 变量，因为 `require("myext")` 第二次及以后只会从 `package.loaded.myext` 缓存里直接取回同一个模块表，不会重新执行 `init.lua`、不会重置计数器。

## 15. Mental Model

```text
Host(Neovim) = "一栋大楼的物业(电梯/门禁/广播系统)"
plugin/*.lua = "住户去物业登记处报备的登记单"
lua/<name>/  = "住户自己家里的家具和生活习惯"

物业只认registrations（命令名、按键序列对应哪个电话号码），
住户家里怎么布置、攒了多少积分，物业完全不关心也看不到。
```

## 16. Why this is this Pattern

这是 Extension，而不是其余四种模式，因为：

- `:HelloCount` 命令和 `<leader>hc` 快捷键，是 Neovim 原本完全不存在的新能力，符合"新增能力"的核心定义；
- 不是 Policy：没有在 Neovim 已有的多个行为之间做选择，是纯粹新增了一个从未存在过的行为分支；
- 不是 Configuration：这不是在描述"某个已有功能的参数应该是什么"（例如 `tabstop` 设成几），而是新增了一段会被执行的逻辑代码；
- 不是 Rule Engine：没有"某个事件满足什么条件就触发什么动作"的匹配表，命令名和按键序列是直接的一一映射，不涉及条件匹配；
- 不是 Plugin：本示例没有独立的启用/禁用生命周期钩子（没有 `setup()`/健康检查/懒加载配置等 Plugin 管理器常见的机制），`plugin/` 目录下的文件本身就是 Neovim 原生"自动加载脚本"机制的一部分，和 Host 共享同一次启动生命周期，不是一个可以被插件管理器单独管理生死的独立单元。

**如果 Lua 消失，Host 还能做什么？**
Neovim 依然是一个功能完整的编辑器：能打开/编辑/保存文件，有所有内置命令（`:w`、`:q` 等）和默认快捷键，Buffer/Window/Event Loop 都正常工作——但永远不会有 `:HelloCount` 这个命令，`<leader>hc` 这个按键序列在普通模式下什么也不会做（或者走到默认的无操作/其它插件占用的行为）。编辑器的核心机制丝毫不受影响，只是少了这一份被 Lua 凭空加上的能力。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？**
- 换成 JSON/YAML：不成立。JSON/YAML 可以声明"存在一个叫 HelloCount 的命令"，但无法描述"每次调用要把计数器加一并格式化字符串通知"这种带状态、带副作用的行为逻辑；顶多能配合一个 Neovim 内置的、写死的通用命令（比如"执行系统命令并打印其 stdout"）来模拟极简版效果，但那样"计数递增"这种真正的逻辑仍然要靠某处的可执行代码实现，JSON/YAML 本身做不到；
- 换成纯 C：Pattern 本身成立（这本质就是 Neovim/Vim 传统 C 插件或 `if_lua`/`if_perl` 之类扩展机制的思路），但要求编写符合 Neovim 插件 ABI 的 C 代码、编译成动态库、处理跨版本兼容性，失去了"编辑一个 `.lua` 文件、`:source %` 立即生效"的开发体验，也失去了 Lua 运行时错误被 Neovim 安全捕获、不影响编辑器稳定性的保护。
