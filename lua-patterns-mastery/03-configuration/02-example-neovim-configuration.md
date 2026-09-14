# 02 · Neovim Configuration

代码目录：[`code/02-neovim-configuration/`](code/02-neovim-configuration/)（`init.lua`）

## 1. Problem（问题定义）

Neovim 是一个 C 写的编辑器内核（Host），需要在启动时确定一大批运行时参数：要不要显示行号、Tab 宽度、要不要用真彩色、剪贴板集成方式……这些参数在不同用户、不同操作系统、不同使用场景下都不一样。如果 Neovim 内核把这些参数写死在 C 代码里，每个用户都得自己改源码重新编译才能定制编辑器行为，这显然不可接受。

## 2. Why Lua（为什么用 Lua）

Neovim 选择让用户用 Lua 写 `init.lua`，而不是一份纯 `.ini`/`.json` 配置文件，原因和 [00-why-and-architecture.md](00-why-and-architecture.md) 里说的一致：用户的配置经常需要"计算"，而不只是"声明"——最常见的场景就是"不同操作系统/不同环境需要不同的参数"，这一点在下面的示例里会具体展示。

## 3. Architecture（架构）

```text
Neovim (Host, C/C++ 内核)
 |
 +-- Config API: vim.opt / vim.g / vim.env / vim.fn / vim.o
 |
 +-- Lua VM（内嵌，进程启动时创建，随进程退出而销毁）
       |
       v
    init.lua
       |
       v
  Runtime Editor Options
```

和示例 1（C Application）的区别在于：这里的 Host（Neovim 内核）不是我们自己写的 C 代码，而是一个已经编译好的、体量巨大的既有程序。我们只提供作为"配置入口"的 `init.lua`，Neovim 内核在启动阶段会自动创建 Lua VM 并执行这个文件。

## 4. Host responsibilities（Host 的职责）

- 在进程启动的极早期创建内嵌的 Lua VM；
- 暴露一整套 Config API：`vim.opt`（编辑器选项，读写会同步映射到内核的 C 数据结构）、`vim.g`（用户可以自由存放任意数据的全局表）、`vim.env`（只读的环境变量视图）、`vim.fn`（调用 Neovim 内建函数）、`vim.o`（另一种读写选项的接口）；
- 按约定路径查找并执行 `init.lua`（通常是 `~/.config/nvim/init.lua`，也可以用 `-u` 参数指定其它路径）；
- 把 `vim.opt.xxx = value` 这样的赋值实时同步成内核里真正生效的编辑器状态。

## 5. Lua responsibilities（Lua 的职责）

- 声明本次启动应该采用的编辑器参数；
- 可以引用 `vim.fn.has(...)`/`vim.env.xxx` 做条件判断，让同一份 `init.lua` 在不同机器上算出不同的最终参数；
- 理论上只应该做"计算参数"这一件事——但正如下一节要讨论的，Neovim 的 Config API 同时也允许在同一个文件里做远不止"配置"的事情。

## 6. Lifecycle（生命周期）

```text
nvim 进程启动
 |
 v
内核创建内嵌 Lua VM（进程内的一个 lua_State，随进程存活）
 |
 v
按约定路径查找 init.lua（或使用 -u 指定的路径）
 |
 v
执行 init.lua：
   vim.opt.xxx = ...        -- 立即同步进编辑器状态
   if vim.fn.has(...) then  -- 条件分支立即求值
   vim.g.xxx = compute()    -- 函数立即调用，结果存进 vim.g
 |
 v
init.lua 执行完毕，Lua VM 并不会像示例 1 那样被关闭——
它会常驻，随时准备执行后续的用户命令、插件回调
 |
 v
编辑器进入正常编辑循环
```

这是 Neovim 场景和示例 1 的关键区别：`config.lua`（示例 1）用完即扔，Lua VM 随之关闭；而 `init.lua` 执行完之后，Lua VM 并不会关闭，因为 Neovim 内核本身还需要用它来运行插件、响应按键映射、执行 `:lua` 命令等——**配置阶段只是这个长生命周期 Lua VM 做的第一件事，不是它的全部生命周期**。

## 7. Data Flow（C -> Lua）

```text
Neovim 内核启动
 |
 v
创建 Lua VM，注入 vim.* 系列 Config API
 |
 v
执行 init.lua
 |
 v
读取 vim.env / vim.fn.has() 等 Host 提供的运行时信息
 |
 v
计算出最终选项值
```

## 8. Control Flow（Lua -> C）

`vim.opt.xxx = value` 本质上是一次"Lua 调 C"：`vim.opt` 是一个带元表（metatable）的特殊 table，给它的字段赋值会触发内核提供的 setter，把值同步进内核真正的编辑器选项存储里。`vim.fn.has(...)` 则是反过来，调用内核里的一个函数、拿到返回值供 Lua 侧的条件判断使用。这两类调用都是"取值/设参数"，没有产生新的"能力"，仍然落在 Configuration 的边界内。

## 9. State（状态）

- `init.lua` 执行期间用到的局部变量（比如 `compute_colorcolumn` 里的 `width`）在函数返回后就消失了；
- 真正被长期保留的状态是 `vim.g.my_colorcolumn`——它被存进了 `vim.g` 这张"跨越整个编辑器生命周期"的全局表，之后无论是这个文件的后续代码、用户在编辑器里执行的 `:lua` 命令，还是安装的插件，都能读到它。

## 10. Failure（失败处理）

- **语法错误**：如果 `init.lua` 存在 Lua 语法错误，Neovim 在编译阶段就会失败，并在启动时弹出错误信息，编辑器仍然会尽量以"没有加载配置"的降级状态继续启动，而不会让整个进程崩溃（这依赖 Neovim 内核对 `init.lua` 的加载同样包裹在类似 `lua_pcall` 的保护调用里）；
- **调用了不存在的 API**：比如手误写成 `vim.otp.number`，会在执行到那一行时抛出"attempt to index a nil value"这类运行时错误，同样会被内核捕获并提示，但这一行之后的代码可能不会继续执行（取决于错误发生的位置），因此实践中建议把配置拆成多个独立的 `require(...)` 模块，一个模块出错不会波及其它模块。

## 11. Complete Host Code（完整 Host 代码）

简化点：本示例的 Host 是 Neovim 自身的 C/C++ 内核源码，体量巨大、由 Neovim 项目维护，不是我们自己编写或编译的对象，因此这里不提供一份可编译的 C 文件。作为替代，下面列出 Host 对外暴露的、和本例相关的 Config API 概念性对照（对应关系，不是可编译代码）：

```text
Lua 侧 API           Host（Neovim 内核）里的角色
-------------------  --------------------------------------------
vim.opt.xxx = v       调用内核里对应选项的 C setter，立即生效
vim.o.xxx             读写选项的另一套接口，语义更接近 Vimscript :set
vim.g.xxx             一张跨生命周期的全局表，由内核创建并常驻
vim.env.xxx           只读地读取进程环境变量（等价于 C 的 getenv）
vim.fn.has(feature)   调用内核编译期特性开关，返回 0/1
vim.api.nvim_create_user_command   注册新命令（越界成 Extension，见下一节）
vim.api.nvim_create_autocmd        注册生命周期钩子（越界成 Plugin 风格，见下一节）
```

## 12. Complete Lua Code（完整 Lua 代码）

```lua
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
```

## 13. Execution（执行）

方式一：临时加载，不影响你现有的 Neovim 配置，用来验证效果最安全：

```bash
cd 03-configuration/code/02-neovim-configuration
nvim -u init.lua
```

方式二：当作真正的用户配置（会覆盖/合并你现有的 `~/.config/nvim/init.lua`，建议先备份）：

```bash
cp ~/.config/nvim/init.lua ~/.config/nvim/init.lua.bak 2>/dev/null
cp init.lua ~/.config/nvim/init.lua
nvim
```

进入编辑器后可以验证：

- `:set number?` 应显示 `number`（已开启）；
- `:set tabstop?` 应显示 `tabstop=4`；
- 执行 `:HelloConfig` 应打印 `Hello from init.lua's user command`（这一行是越界的 Extension 部分）；
- `:lua print(vim.g.my_colorcolumn)` 应打印根据当前终端宽度算出的 `"80"` 或 `"100"`。

## 14. Step-by-step walkthrough（逐步走读）

1. Neovim 进程启动，内核创建内嵌 Lua VM，并把 `vim.opt`/`vim.g`/`vim.env`/`vim.fn`/`vim.o`/`vim.api` 等 Config API 注入进去。
2. 内核找到 `init.lua`（或读取 `-u` 指定的路径），把它作为一段 Lua 代码执行。
3. 第一块的 5 行 `vim.opt.xxx = ...` 依次执行，每一行都立即把值同步进内核的真实编辑器状态——这一步执行完，编辑器界面上就已经能看到行号、4 格缩进等效果。
4. 第二块开始出现计算：`vim.fn.has("mac")` 在执行的这一刻才被求值，返回值决定走 `if` 还是 `else` 分支，最终只有一条 `vim.opt.clipboard = ...` 真正生效。
5. `vim.env.NVIM_LITE_MODE` 同理，是从进程环境变量里读出来的运行时输入。
6. `compute_colorcolumn()` 被调用一次，读取 `vim.o.columns`（当前终端列数）算出一个字符串，存进 `vim.g.my_colorcolumn`，再用它设置 `vim.opt.colorcolumn`。
7. 第三块的两行 `vim.api.nvim_create_user_command`/`nvim_create_autocmd` 执行时，只是"注册"了一个回调，回调函数本身的代码在这一刻**不会**运行，只有当用户真正输入 `:HelloConfig` 或触发一次 `BufWritePre` 事件时才会执行——这两行让 Lua 从"这一次执行时算出一个值"变成了"以后每次事件发生都要被调用"，这正是从 Configuration 越界到 Extension/Plugin 的分界点。
8. `init.lua` 执行完毕，但 Lua VM 不会关闭，会继续常驻，等待响应第 7 步注册的回调、按键映射、插件逻辑等。

## 15. Mental Model（心智模型）

```text
init.lua 前两块：一次性求值，产出编辑器参数（Configuration）
init.lua 第三块：注册以后会被反复调用的回调（越界成 Extension/Plugin）
同一个文件，同一个 lua_State，两种截然不同的"角色"
```

判断某一行 `init.lua` 代码属于哪一种角色，只需要问一个问题："这行代码执行完之后，它的效果是立即体现为一个参数值，还是变成了一个'以后会被再次调用'的挂钩？" 前者是 Configuration，后者是 Extension（新增能力入口）或 Plugin（生命周期钩子）。

### 为什么 Neovim 配置天然会变成"混合体"

```text
+----------------------------------------------------------+
| init.lua 是一个"万能入口文件"，Neovim 并没有强制拆分       |
| "纯配置文件" 和 "命令注册文件" 和 "插件生命周期文件"        |
+----------------------------------------------------------+
| vim.opt.number = true              <- 纯 Configuration    |
| vim.g.my_colorcolumn = compute()   <- 纯 Configuration    |
| vim.api.nvim_create_user_command   <- 越界成 Extension     |
| vim.api.nvim_create_autocmd        <- 越界成 Plugin 风格    |
+----------------------------------------------------------+
```

根本原因是 Config API（`vim.opt`/`vim.g`）和"新增能力"的 API（`vim.api.nvim_create_user_command`/`nvim_create_autocmd`）被注入进了**同一个**、**同一次**执行的 `lua_State`，Neovim 内核没有在语言层面强制隔离"这是配置阶段，只能设参数"和"这是扩展阶段，可以注册回调"。这是 Neovim 生态里绝大多数 `init.lua`/插件配置文件读起来"看似简单，实则职责混杂"的根本原因——本例中第一、二块（`vim.opt`/`vim.g` 赋值、条件分支、纯函数计算）是纯 Configuration；第三块一旦加入（`vim.api.nvim_create_user_command`/`nvim_create_autocmd`）就越界了。

## 16. Why this is this Pattern（为什么这是 Configuration 而不是别的）

- 前两块只声明"参数应该是什么"，不新增任何 Neovim 原本不具备的能力，符合 Configuration 的定义；
- 计算逻辑（`vim.fn.has`/`vim.env`/自定义函数）都是只读取值，不产生副作用，不越界成 Policy（这里没有"每次事件发生时都要重新决策"的场景，只在启动时算一次）；
- 第三块特意作为"反例"保留在示例里，用来标定"配置"和"扩展/插件"之间的边界——这条边界在 Neovim 生态里很容易被无意间跨过，值得单独强调。

**如果 Lua 消失，Host 还能做什么？** Neovim 在没有 Lua 配置的情况下，仍然可以用传统的 Vimscript `.vimrc`/`init.vim` 达到前两块的效果（`set number`、`set tabstop=4`，甚至 Vimscript 也有 `if has("mac")` 这样的条件判断），只是复用函数、组织复杂计算逻辑的能力远不如 Lua 好用；但第三块（注册命令、注册 autocmd）在 Vimscript 里同样可以做到（`command!`、`autocmd`），说明这一块的价值本来就不是"Configuration 特有"的，而是 Extension/Plugin Pattern 的能力，只是恰好也能用 Lua 写在同一个文件里。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** 换成 JSON/YAML 之后，前两块（纯参数赋值）完全可以表达，但条件分支和 `compute_colorcolumn()` 这样的计算能力会全部消失，除非引入额外的模板引擎；第三块（注册命令/autocmd）在纯数据格式里根本无法表达，因为它的本质就是"传一段可执行的代码进去"，这一点决定了它必须用某种脚本语言实现，JSON/YAML 在这里不是简化，而是能力缺失。换成纯 C，计算能力和扩展能力都能做到，但每个用户改一次配置都要重新编译整个 Neovim，这与"每个用户都能自由定制编辑器"的产品目标完全冲突。
