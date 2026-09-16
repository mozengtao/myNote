# Neovim `vim.*` Namespaces 日常开发速查与 Mental Model

## 0. 总体 Mental Model

Neovim Lua Plugin 不应该把 `vim.*` 理解成一堆零散 API。

更好的理解方式是：

```text
                         Lua Plugin
                             │
                             ▼
                         vim.*
                             │
       ┌─────────────┬───────┼────────┬──────────┐
       │             │       │        │          │
       ▼             ▼       ▼        ▼          ▼
    Editor         Nvim    Vim      Runtime    State
    Config         API     Bridge   / OS       Storage
       │             │       │        │          │
       ▼             ▼       ▼        ▼          ▼
   vim.opt       vim.api  vim.fn   vim.uv    vim.g/b/w
   vim.o         vim.cmd  vim.keymap
   vim.bo/wo
```

可以记成：

```text
vim.opt
    → 配置 Editor

vim.fn
    → 调用已有 Vim Function

vim.api
    → 直接操作 Neovim 对象

vim.keymap
    → 定义 User Input

vim.cmd
    → 执行 Ex Command

vim.env
    → 与 OS Environment 交互

vim.uv
    → 与 Event Loop / OS / Network / FS 交互

vim.g / vim.b / vim.w
    → 保存不同生命周期范围的状态
```

## 1. `vim.opt` —— Editor Configuration

### 作用

`vim.opt`

用于设置 Neovim/Vim 的 Editor Options。

Mental Model：

```text
vim.opt
   │
   ▼
:set
   │
   ▼
Editor Configuration
```

它是 Lua-friendly 的 `:set` 接口。

官方文档中，`vim.opt` 特别适合 list/map/set 类型 option，并提供 `append()` / `prepend()` / `remove()` 等操作。

### 常用 API

#### Boolean

```lua
vim.opt.number = true
vim.opt.relativenumber = true
vim.opt.wrap = false
vim.opt.expandtab = true
vim.opt.ignorecase = true
```

对应：

```vim
:set number
:set relativenumber
:set nowrap
:set expandtab
:set ignorecase
```

#### String

```lua
vim.opt.numberwidth = 4
vim.opt.tabstop = 4
vim.opt.shiftwidth = 4
vim.opt.fileencoding = "utf-8"
```

#### List

```lua
vim.opt.wildignore = {
    "*.o",
    "*.a",
    "__pycache__",
}
```

#### Map-like option

```lua
vim.opt.listchars = {
    space = "·",
    tab = ">-",
    eol = "↲",
}
```

#### Append / Remove

```lua
vim.opt.wildignore:append({
    "node_modules",
    ".git",
})

vim.opt.wildignore:remove({
    "*.o",
})
```

对应 Vimscript：

```vim
:set wildignore+=node_modules
:set wildignore-=*.o
```

### 读取

注意：

```lua
vim.opt.number
```

得到的是 Option 对象，而不是最终值。

读取：

```lua
vim.opt.number:get()
```

例如：

```lua
print(vim.opt.number:get())
```

### Mental Model

```text
vim.opt
   │
   │ "我想修改编辑器行为"
   ▼
Editor Option
   │
   ├── number
   ├── tabstop
   ├── shiftwidth
   ├── wrap
   ├── ignorecase
   └── ...
```

一句话：

`vim.opt` = 配置 Neovim 的行为。

## 2. `vim.fn` —— Vim Function Bridge

### 作用

`vim.fn`

用于从 Lua 调用已有的 Vimscript Function。

Mental Model：

```text
Lua
 │
 ▼
vim.fn
 │
 ▼
Vim Function
 │
 ▼
Result
```

官方文档将 `vim.fn` 与 `vim.cmd` 一起归入继承自 Vim 的 Vim API。

### 常用示例

#### `getcwd()`

```lua
local cwd = vim.fn.getcwd()

print(cwd)
```

相当于：

```vim
:echo getcwd()
```

#### `expand()`

```lua
local file = vim.fn.expand("%:p")

print(file)
```

获得当前文件的绝对路径。

#### `glob()`

```lua
local files = vim.fn.glob("*.c", false, true)

for _, file in ipairs(files) do
    print(file)
end
```

#### `executable()`

```lua
if vim.fn.executable("git") == 1 then
    print("git exists")
end
```

#### `system()`

```lua
local result = vim.fn.system("git status --short")

print(result)
```

### 为什么需要 `vim.fn`？

因为 Vim 本身已经拥有大量成熟函数：

```text
Vim Function
    │
    ├── getcwd()
    ├── expand()
    ├── glob()
    ├── executable()
    ├── system()
    ├── filereadable()
    └── ...
```

Lua 不需要重新实现。

直接：

```lua
vim.fn.xxx(...)
```

即可调用。

### Mental Model

```text
vim.fn
   │
   ▼
"我需要一个 Vim 已经提供的 Function"
```

一句话：

`vim.fn` = Lua 调用 Vimscript Function 的桥。

## 3. `vim.api` —— Neovim Core API

这是 Plugin 开发中最重要的一层。

`vim.api`

表示：

直接调用 Neovim 的 Nvim API。

官方文档将它称为 Nvim API，原本就是为远程插件、GUI 等程序化访问 Nvim Core 而设计的。

Mental Model：

```text
    Lua Plugin
         │
         ▼
     vim.api
         │
         ▼
    Nvim Core
         │
    ┌────┼───────┬──────┐
    ▼    ▼       ▼      ▼
Buffer Window Tabpage  UI
```

### 最常用的一组 API

#### Buffer

```lua
vim.api.nvim_get_current_buf()

vim.api.nvim_get_current_line()

vim.api.nvim_buf_get_lines(bufnr, 0, -1, false)

vim.api.nvim_buf_set_lines(
    bufnr,
    0,
    -1,
    false,
    {
        "hello",
        "world",
    }
)
```

#### Window

```lua
local win = vim.api.nvim_get_current_win()

vim.api.nvim_win_get_cursor(win)

vim.api.nvim_win_set_cursor(win, { 1, 0 })
```

#### Tabpage

```lua
local tab = vim.api.nvim_get_current_tabpage()

local wins = vim.api.nvim_tabpage_list_wins(tab)
```

#### Command

```lua
vim.api.nvim_create_user_command(
    "Hello",
    function()
        print("hello")
    end,
    {}
)
```

#### Autocmd

```lua
vim.api.nvim_create_autocmd("BufWritePost", {
    pattern = "*.c",
    callback = function()
        print("C file saved")
    end,
})
```

#### Namespace

```lua
local ns = vim.api.nvim_create_namespace("my-plugin")
```

Namespace 常用于管理：

- Extmark
- Highlight
- Virtual Text
- Decoration

#### Extmark

```lua
vim.api.nvim_buf_set_extmark(
    0,
    ns,
    0,
    0,
    {
        virt_text = {
            { " ← here", "Comment" }
        },
    }
)
```

### `vim.api` 的一个重要特点

很多 API 名称直接暴露了它操作的对象：

```text
nvim_buf_*
nvim_win_*
nvim_tabpage_*
nvim_create_*
nvim_get_*
nvim_set_*
```

因此可以先从名字判断：

```text
nvim_buf_
    → Buffer

nvim_win_
    → Window

nvim_tabpage_
    → Tabpage

nvim_create_
    → 创建对象/注册行为

nvim_get_
    → 获取状态

nvim_set_
    → 修改状态
```

### Mental Model

```text
vim.api
    │
    ▼
"我需要直接操作 Neovim 的内部 Editor Object"
    │
    ├── Buffer
    ├── Window
    ├── Tabpage
    ├── Namespace
    ├── Extmark
    ├── Command
    ├── Autocmd
    └── ...
```

一句话：

`vim.api` = Plugin 与 Neovim Core 之间的主要程序化接口。

## 4. `vim.keymap` —— User Input

`vim.keymap`

负责管理 Key Mapping。

最常用：

```lua
vim.keymap.set()
vim.keymap.del()
```

官方文档提供了 Lua function 作为 RHS 的直接映射方式。

### 基本示例

```lua
vim.keymap.set(
    "n",
    "<Leader>w",
    "<cmd>w<CR>"
)
```

或者直接绑定 Lua function：

```lua
vim.keymap.set(
    "n",
    "<Leader>h",
    function()
        print("hello")
    end
)
```

### 多模式

```lua
vim.keymap.set(
    { "n", "v" },
    "<Leader>y",
    '"+y'
)
```

### Buffer-local

```lua
vim.keymap.set(
    "n",
    "<Leader>x",
    function()
        print("buffer local")
    end,
    {
        buffer = 0,
    }
)
```

### 删除

```lua
vim.keymap.del(
    "n",
    "<Leader>x"
)
```

### 常用特殊键写法

`<Leader>` 不是键盘上印着 “Leader” 的键，而是一个可配置前缀。

默认是 `\`。

很多人会改成空格或逗号：

```lua
vim.g.mapleader = " "
-- 或
vim.g.mapleader = ","
```

因此：

```text
"<Leader>" .. "1"
    → <Leader>1
    → 默认就是 \ 再按 1
```

还有一个 `<LocalLeader>`，专门给 buffer-local 映射用，默认也是 `\`。

查看当前值：

```vim
:echo mapleader
:echo maplocalleader
```

映射里常用的特殊键：

| 写法 | 实际按键 |
| --- | --- |
| `<Leader>` | 自定义前缀，默认 `\` |
| `<LocalLeader>` | buffer 本地前缀，默认 `\` |
| `<CR>` / `<Enter>` / `<Return>` | 回车 |
| `<Esc>` | Esc |
| `<Space>` | 空格 |
| `<Tab>` / `<S-Tab>` | Tab / Shift+Tab |
| `<BS>` | Backspace |
| `<Del>` | Delete |
| `<Up>` `<Down>` `<Left>` `<Right>` | 方向键 |
| `<Home>` `<End>` `<PageUp>` `<PageDown>` | 编辑区导航键 |
| `<F1>` … `<F12>` | 功能键 |
| `<C-x>` | Ctrl+x，例如 `<C-w>`、`<C-n>` |
| `<A-x>` / `<M-x>` | Alt/Meta+x |
| `<S-x>` | Shift+x，例如 `<S-CR>` |
| `<D-x>` | Super/Cmd+x（macOS） |
| `<Bar>` | `\|` |
| `<Bslash>` | `\` |
| `<lt>` | `<` |
| `<Cmd>` | 不切模式执行命令，例如 `<Cmd>w<CR>` |
| `<Nop>` | 空操作，用来取消已有映射 |

修饰键可以叠：

```text
<C-S-l>
<A-CR>
<C-w>
```

前面示例里的：

```lua
vim.keymap.set("n", "<Leader>w", "<cmd>w<CR>")
```

可以读成：

```text
<Leader>w
    → 先按 Leader，再按 w

<cmd>w<CR>
    → 不切模式执行 :w，然后回车
```

### Mental Model

```text
Keyboard Input
      │
      ▼
Keymap Registry
      │
      ▼
Lua Function / Command
      │
      ▼
Plugin Behavior
```

所以：

`vim.keymap` = 把用户输入连接到 Plugin 行为。

它属于 Plugin 的 Input Layer。

## 5. `vim.cmd` —— Ex Command Bridge

`vim.cmd`

用于从 Lua 执行 Vim/Neovim 的 Ex Command。

Mental Model：

```text
Lua
 │
 ▼
vim.cmd
 │
 ▼
:Ex command
 │
 ▼
Neovim
```

例如：

```lua
vim.cmd("colorscheme habamax")
```

等价于：

```vim
:colorscheme habamax
```

官方文档还支持多行 Vimscript，以及结构化 command 参数。

### 多行

```lua
vim.cmd([[
    set number
    set relativenumber
    colorscheme habamax
]])
```

### Command-as-function

可以：

```lua
vim.cmd.write({
    args = { "test.txt" },
})
```

也可以：

```lua
vim.cmd.write("test.txt")
```

### 和 `vim.api` 的区别

例如创建 command：

```lua
vim.api.nvim_create_user_command(...)
```

这是：

Lua → Nvim API

而：

```lua
vim.cmd("command! Hello echo 'hello'")
```

是：

Lua → Ex command

所以：

```text
vim.api
    → 程序化 API

vim.cmd
    → 命令语言接口
```

### Mental Model

`vim.cmd` = 我已经知道 Vim/Neovim 的 Ex 命令，直接从 Lua 执行它。

## 6. `vim.env` —— Process Environment

`vim.env`

访问 Neovim 当前进程的环境变量。

官方文档说明它读写的是当前 editor process 的 environment variables。

### 读取

```lua
local home = vim.env.HOME

print(home)
```

### 设置

```lua
vim.env.MY_PLUGIN_MODE = "debug"
```

然后：

```lua
print(vim.env.MY_PLUGIN_MODE)
```

### 和 `vim.g` 的区别

这是一个非常重要的区别：

```text
vim.g
   ↓
Neovim/Vim 的 g: variable

vim.env
   ↓
OS process environment
```

也就是：

```text
vim.g
   → Editor State

vim.env
   → Process / OS State
```

### Mental Model

```text
Neovim Process
      │
      ├── vim.env.HOME
      ├── vim.env.PATH
      ├── vim.env.SHELL
      └── vim.env.MY_PLUGIN_MODE
```

一句话：

`vim.env` = Plugin 与进程环境之间的接口。

## 7. `vim.uv` —— Event Loop / OS Infrastructure

`vim.uv`

是 Neovim 暴露的 libuv/Luv bindings。

官方文档明确说明它用于：

- Networking
- Filesystem
- Process Management
- Event Loop
- Timers

并且 `vim.uv` 对应 Neovim 使用的 libuv event loop。

### Timer 示例

```lua
local timer = vim.uv.new_timer()

timer:start(
    1000,
    0,
    vim.schedule_wrap(function()
        print("1 second later")
        timer:stop()
        timer:close()
    end)
)
```

### 文件系统示例

```lua
local stat = vim.uv.fs_stat("test.txt")

if stat then
    print(stat.size)
end
```

### 获取当前工作目录

```lua
local cwd = vim.uv.cwd()

print(cwd)
```

### 为什么 `vim.uv` 很重要？

因为 Plugin 经常需要：

```text
Plugin
 │
 ├── Timer
 ├── File IO
 ├── Socket
 ├── Process
 └── Async Event
```

这些并不是 Editor API 本身。

它们属于：

OS / Runtime Infrastructure

所以：

```text
vim.api
    → Neovim Editor

vim.uv
    → OS / Event Loop
```

### 一个非常重要的注意点

`vim.uv` callback 运行时，不能随便直接调用普通 `vim.api`。

例如：

```lua
vim.uv.fs_read(..., function(...)
    vim.api.nvim_buf_set_lines(...)
end)
```

这种跨异步 callback 直接操作 Editor State 的方式需要特别小心。

通常使用：

```lua
vim.schedule(function()
    vim.api.nvim_buf_set_lines(...)
end)
```

或者：

```lua
vim.schedule_wrap(...)
```

把操作重新调度到 Neovim 主循环。

官方文档明确指出，在 `vim.uv` callback 中直接调用普通 `vim.api` 是错误的；需要通过调度机制回到主线程。

### Mental Model

```text
vim.uv
   │
   ▼
libuv Event Loop
   │
   ├── Timer
   ├── FS
   ├── Network
   └── Process
          │
          ▼
       callback
          │
          │ schedule
          ▼
    Neovim Main Loop
          │
          ▼
       vim.api
```

一句话：

`vim.uv` = Plugin 与异步 Runtime/OS 基础设施之间的接口。

## 8. `vim.g` —— Global State

`vim.g`

对应 Vimscript：

```text
g:
```

生命周期：

整个 Neovim instance

例如：

```lua
vim.g.my_plugin_enabled = true

print(vim.g.my_plugin_enabled)
```

### 适合什么？

适合保存：

- 全局 Plugin State
- 全局用户配置
- 与其他 Vimscript/Plugin 共享的全局变量

例如：

```lua
vim.g.my_plugin_debug = true
```

## 9. `vim.b` —— Buffer-local State

`vim.b`

对应：

```text
b:
```

作用域：

Current Buffer

例如：

```lua
vim.b.my_plugin_enabled = true
```

假设：

```text
Buffer 1 → C source
Buffer 2 → Python source
```

可以：

```lua
vim.b[1].language = "c"
vim.b[2].language = "python"
```

官方文档支持通过 buffer number 对指定 buffer 的变量进行访问。

### Mental Model

```text
Neovim
│
├── Buffer 1
│   └── vim.b[1]
│
├── Buffer 2
│   └── vim.b[2]
│
└── Buffer 3
    └── vim.b[3]
```

一句话：

`vim.b` = 跟着 Buffer 走的 Plugin State。

## 10. `vim.w` —— Window-local State

`vim.w`

对应：

```text
w:
```

作用域：

Window

例如：

```lua
vim.w.my_plugin_mode = "preview"
```

指定 Window：

```lua
local win = vim.api.nvim_get_current_win()

vim.w[win].my_plugin_mode = "preview"
```

### 为什么需要 `vim.w`？

因为：

Buffer ≠ Window

一个 Buffer 可以：

```text
Buffer A
   │
   ├── Window 1
   │
   └── Window 2
```

所以：

```text
vim.b
   → "这个文件/Buffer是什么状态？"

vim.w
   → "这个窗口怎么显示这个 Buffer？"
```

## 11. `vim.g` / `vim.b` / `vim.w` 的核心关系

这是最值得建立 Mental Model 的地方：

```text
                    Neovim
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼
     Global          Buffer         Window
        │              │              │
        ▼              ▼              ▼
     vim.g           vim.b          vim.w
        │              │              │
        │              │              │
    Plugin-wide     File-local     View-local
      state           state          state
```

例如一个 LSP Plugin：

```text
vim.g
    → Plugin 是否启用

vim.b
    → 当前 Buffer 是否已经初始化 LSP

vim.w
    → 当前 Window 是否显示某种 UI
```

## 12. 七/八个 Namespace 的职责边界

| Namespace | 管什么 | Mental Model |
| --- | --- | --- |
| `vim.opt` | Editor options | 配置 Editor |
| `vim.fn` | Vim functions | 调用已有 Vim Function |
| `vim.api` | Nvim objects/API | 操作 Neovim Core |
| `vim.keymap` | Key mappings | User Input → Action |
| `vim.cmd` | Ex commands | Lua → Vim Command |
| `vim.env` | Environment | Plugin → Process Environment |
| `vim.uv` | libuv | Plugin → OS/Event Loop |
| `vim.g` | Global variables | Plugin-wide State |
| `vim.b` | Buffer variables | Buffer-local State |
| `vim.w` | Window variables | Window-local State |

## 13. 从 Plugin Architecture 看这些 API

一个实际 Plugin 可以把它们组织成：

```text
                         Plugin
                           │
             ┌─────────────┼──────────────┐
             │             │              │
             ▼             ▼              ▼
          Input         State          Action
             │             │              │
             ▼             ▼              ▼
        vim.keymap     vim.g/b/w       vim.api
                           │              │
                           │              │
             ┌─────────────┘              │
             │                            │
             ▼                            ▼
        Configuration                Nvim Core
             │
             ▼
         vim.opt
             │
             │
             ├────────── vim.cmd ──────────► Ex
             │
             ├────────── vim.fn  ──────────► Vim Functions
             │
             ├────────── vim.env ─────────► OS Environment
             │
             └────────── vim.uv  ─────────► Runtime / OS
```

这比记：

```text
vim.xxx
vim.yyy
vim.zzz
```

更有用。

## 14. 一个完整的小 Plugin

下面把几个 Namespace 串起来。

```lua
local M = {}

-- Configuration
vim.opt.number = true

-- Global plugin state
vim.g.demo_plugin_enabled = true

-- Keymap
vim.keymap.set("n", "<leader>d", function()
    M.run()
end)

function M.run()
    -- Buffer state
    vim.b.demo_plugin_used = true

    -- Read current buffer
    local line = vim.api.nvim_get_current_line()

    -- Call Vim function
    local cwd = vim.fn.getcwd()

    -- Environment
    local shell = vim.env.SHELL or "unknown"

    print(string.format(
        "cwd=%s shell=%s line=%s",
        cwd,
        shell,
        line
    ))
end

return M
```

运行：

```text
<Leader>d
    │
    ▼
vim.keymap
    │
    ▼
M.run()
    │
    ├── vim.b
    │      ↓
    │   Buffer State
    │
    ├── vim.api
    │      ↓
    │   Current Buffer
    │
    ├── vim.fn
    │      ↓
    │   Vim Function
    │
    └── vim.env
           ↓
       OS Environment
```

## 15. 真实案例：tmuxsend.nvim

上面第 14 节是为了串 Namespace 而写的最小示例。

下面用一个真实 Plugin 回看同一套分类：把当前行发送到指定 tmux pane。

### 目录与加载方式

```text
~/.config/nvim/lua/plugins/
├── tmuxsend.nvim.lua
└── tmuxsend.nvim/
    ├── lua/
    │   └── tmuxsend/
    │       └── init.lua
    └── plugin/
        └── tmuxsend.lua
```

`tmuxsend.nvim.lua` 是 lazy.nvim 的本地 Plugin spec：

```lua
return {
  dir = vim.fn.stdpath("config") .. "/lua/plugins/tmuxsend.nvim",
  name = "tmuxsend.nvim",
}
```

这里只用了一个 Namespace：

```text
vim.fn.stdpath("config")
    → Call
    → 让 Vim Function 告诉 Plugin：config 目录在哪
```

`plugin/` 是 Runtime Entry，`lua/` 是 Application Logic。

```text
tmuxsend.nvim.lua
    → lazy.nvim 发现并加载本地 Plugin

plugin/tmuxsend.lua
    → 注册 Command / Keymap

lua/tmuxsend/init.lua
    → 真正发送当前行到 tmux
```

### Entry：`plugin/tmuxsend.lua`

```lua
local tmuxsend = require("tmuxsend")

vim.api.nvim_create_user_command("SendToTmux", function(opts)
        tmuxsend.send_current_line_to_tmux(opts.args)
end, { nargs = 1, desc = "Send current line to tmux pane, e.g :SendToTmux .1" })

local keymap = vim.keymap.set
local targets = { ".0", ".1", ".2", ".3", ".4" }

for idx, pane in ipairs(targets) do
        local key = "<Leader>" .. tostring(idx)
        keymap("n", key, function()
                tmuxsend.send_current_line_to_tmux(pane)
        end, { desc = string.format("Send current line to tmux pane %s", pane) })
end
```

这一层几乎不做业务，只做两件事：

```text
vim.api.nvim_create_user_command
    → Operate
    → :SendToTmux .1

vim.keymap.set
    → Bind
    → <Leader>1 / <Leader>2 / ...
```

所以 Entry 的职责是：

```text
User
 │
 ├── :SendToTmux .1
 │         │
 │         ▼
 │    vim.api
 │
 └── <Leader>1
           │
           ▼
      vim.keymap
           │
           ▼
require("tmuxsend")
           │
           ▼
M.send_current_line_to_tmux(pane)
```

Command 和 Keymap 都只是入口。

真正做事的是 `lua/tmuxsend/init.lua`。

### Logic：`lua/tmuxsend/init.lua`

```lua
local M = {}

---@param target_pane string
function M.send_current_line_to_tmux(target_pane)
  local line = vim.api.nvim_get_current_line()
  local cmd = vim.trim(line)
  if cmd == "" then
    return
  end

  local escaped_cmd = vim.fn.shellescape(cmd)
  local tmux_shell =
    string.format("tmux send-keys -t %s -l %s ; tmux send-keys -t %s Enter &", target_pane, escaped_cmd, target_pane)

  -- 修复点：直接调用完整shell命令
  vim.cmd("silent !" .. tmux_shell)
  vim.cmd.redraw({ bang = true })

  vim.notify(string.format("Sent to Tmux Pane [%s]: %s", target_pane, cmd), vim.log.levels.INFO)
end

return M
```

按“我要做什么？”把每一步拆开：

```text
vim.api.nvim_get_current_line()
    → Operate
    → 读当前 Buffer 的当前行

vim.trim(line)
    → 去掉首尾空白
    → 空行直接返回，不发送

vim.fn.shellescape(cmd)
    → Call
    → 让已有 Vim Function 做 shell 转义

vim.cmd("silent !" .. tmux_shell)
    → Command
    → 通过 :! 把完整 shell 命令交给外部 tmux

vim.cmd.redraw({ bang = true })
    → Command
    → 执行完 :! 后刷新屏幕

vim.notify(..., vim.log.levels.INFO)
    → 给用户一个可见反馈
```

### 控制流

```text
<Leader>1  或  :SendToTmux .1
        │
        ▼
plugin/tmuxsend.lua
        │
        ├── vim.api.nvim_create_user_command
        └── vim.keymap.set
        │
        ▼
require("tmuxsend")
        │
        ▼
M.send_current_line_to_tmux(".1")
        │
        ├── vim.api
        │      ↓
        │   当前行
        │
        ├── vim.fn
        │      ↓
        │   shellescape
        │
        ├── vim.cmd
        │      ↓
        │   :silent !tmux send-keys ...
        │      ↓
        │   tmux pane
        │
        └── vim.notify
               ↓
           提示发送成功
```

### 这个 Plugin 用到了哪些 Boundary，没用到哪些

对照第 12 节的职责表：

| Namespace | 在 tmuxsend.nvim 里做什么 | 动词 |
| --- | --- | --- |
| `vim.fn` | `stdpath()` 找 config 目录；`shellescape()` 转义命令 | Call |
| `vim.api` | 注册 `:SendToTmux`；读取当前行 | Operate |
| `vim.keymap` | `<Leader>1` … `<Leader>5` 绑到不同 pane | Bind |
| `vim.cmd` | `:silent !tmux ...` 发键；`:redraw!` 刷新 | Command |

它没有用：

```text
vim.opt
    → 不需要改 Editor Option

vim.env
    → 不需要读写环境变量

vim.uv
    → 不需要 Timer / FS / Network callback

vim.g / vim.b / vim.w
    → 不需要跨调用保存 Plugin State
```

这正是第 0 节 / 第 16 节那张决策树的用法：

不是先背 API，而是先问“这一步跨的是哪条边界”。

### 为什么这里用 `vim.cmd`，而不是 `vim.uv` / `vim.fn.system`

这个 Plugin 的外部动作是：

```text
tmux send-keys -t .1 -l <escaped_cmd>
tmux send-keys -t .1 Enter
```

它可以走三条路：

```text
vim.fn.system(...)
    → Call Vim Function，拿回 stdout

vim.uv.spawn(...)
    → Async / OS，自己管理 process / callback

vim.cmd("silent !...")
    → Command，直接执行 :! shell
```

当前实现选择的是第三条：

```text
vim.cmd
    → 我已经知道要执行一条 Ex / shell 命令
    → 直接从 Lua 执行它
```

并且用：

```lua
vim.cmd.redraw({ bang = true })
```

也就是结构化的 `vim.cmd.xxx({ bang = true })`，对应第 5 节的 Command-as-function。

`vim.notify` / `vim.log` 不在前面 8 个核心 Namespace 里。

可以暂时把它们理解成：

```text
vim.notify
    → 给用户看结果

vim.log.levels
    → notify 的级别
```

它们属于 UI 反馈，不是 Editor Object，也不是 OS Event Loop。

### 从这个 Plugin 压出来的 Mental Model

```text
                         tmuxsend.nvim
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
           Entry            Logic            Effect
              │                │                │
        plugin/*.lua     lua/tmuxsend/*     tmux pane
              │                │                │
              ▼                ▼                ▼
        vim.api            vim.api          vim.cmd
        vim.keymap         vim.fn           :silent !
                           vim.trim
                           vim.notify
```

一句话：

tmuxsend.nvim 不是“调用了一堆 `vim.*`”，而是：

```text
Bind / Operate 获得控制权
        ↓
Operate 读当前行
        ↓
Call 做 shell 转义
        ↓
Command 把命令交给 tmux
        ↓
notify 告诉用户结果
```

## 16. 最终 Mental Model：把 `vim.*` 当成系统边界

不要把它们记成：

```text
vim.opt = xxx
vim.fn = xxx
vim.api = xxx
...
```

而应该记成：

```text
                         Lua Plugin
                             │
                             ▼
                          vim.*
                             │
      ┌──────────────────────┼──────────────────────┐
      │                      │                      │
      ▼                      ▼                      ▼
   Editor                 Runtime                 State
      │                      │                      │
      │                      │                      │
  vim.opt                 vim.uv                 vim.g
  vim.cmd                 vim.env                vim.b
  vim.fn                                         vim.w
      │
      │
      ▼
   Nvim Core
      │
      ▼
   vim.api
      │
      ▼
 Buffer / Window / Tab / UI / Extmark / ...
```

最终可以压缩成 8 个动词：

```text
vim.opt
    → Configure

vim.fn
    → Call

vim.api
    → Operate

vim.keymap
    → Bind

vim.cmd
    → Command

vim.env
    → Environment

vim.uv
    → Async / OS

vim.g/b/w
    → Store State
```

### 一句话心智模型

`vim.*` 不是一组杂乱的 API，而是 Lua Plugin 与 Neovim Editor、Vim Runtime、OS Runtime 以及 Plugin State 之间的几个不同 Boundary。

当你不知道应该用哪个 API 时，可以先问：

```text
我要做什么？
     │
     ├── 改编辑器配置？      → vim.opt
     ├── 调现成 Vim 函数？    → vim.fn
     ├── 操作 Buffer/Window？ → vim.api
     ├── 响应快捷键？         → vim.keymap
     ├── 执行 :xxx？          → vim.cmd
     ├── 读写环境变量？       → vim.env
     ├── Timer/FS/Network？   → vim.uv
     └── 保存 Plugin 状态？   → vim.g/b/w
```

这套分类比单独记 API 名称更适合日常 Plugin 开发
