# Neovim Lua Plugin：基本组成、基础设施、开发框架与心智模型

> 核心目标：
>
> 不要把 Neovim Lua Plugin 学成一堆 `vim.api.*` API。
> 应该建立一个完整 Mental Model：
>
> **Neovim 是 Host / Framework**
>
> **Lua Plugin 是运行在 Neovim Runtime 上的 Event-Driven Application**
>
> Plugin 通过：
>
> - Runtime 文件进入 Neovim
> - Command / Keymap / Autocmd 获得控制权
> - `require()` 组织 Lua Module
> - `vim.*` 调用 Neovim 提供的基础设施
> - Buffer / Window / Tabpage / Namespace / Extmark 等对象承载状态和 UI
>
> 最终形成：
>
> ```text
> User / Neovim Event
>          │
>          ▼
> Command / Keymap / Autocmd
>          │
>          ▼
>     Plugin Entry
>          │
>          ▼
>      Lua Modules
>          │
>          ▼
>    Plugin State / Logic
>          │
>          ▼
>      vim.* API
>          │
>          ▼
>      Neovim Core
> ```

---

## 1. 第一性原理：Neovim Plugin 到底是什么？

最简单的定义：

```text
Neovim Plugin
    =
在 Neovim Runtime 中注册行为
+
响应用户 / 编辑器事件
+
调用 Neovim API
+
维护自己的状态
+
提供用户功能
```

所以 Plugin 不是：

一个 `.lua` 文件

而是：

一个运行在 Neovim Host 上的 Event-Driven Application

整体架构：

```text
                         Neovim
                           │
              ┌────────────┴────────────┐
              │                         │
          Runtime                    Objects
              │                         │
       ┌──────┼──────┐          ┌───────┼────────┐
       ▼      ▼      ▼          ▼       ▼        ▼
    plugin  ftplugin lua       Buffer  Window   Tabpage
       │      │       │
       │      │       └──────────┐
       │      │                  │
       └──────┴──────────┐       │
                         ▼       ▼
                    Entry / require()
                         │
                         ▼
                    Plugin Logic
                         │
               ┌─────────┼──────────┐
               ▼         ▼          ▼
            Command    Keymap    Autocmd
               │         │          │
               └─────────┼──────────┘
                         ▼
                     State / Data
                         │
                         ▼
                      vim.*
                         │
               ┌─────────┼─────────┐
               ▼         ▼         ▼
            vim.api    vim.fn    vim.cmd
               │
               ▼
           Neovim Core
```

## 2. Neovim 提供的基础设施

可以先把 Neovim 提供给 Plugin 的能力分成 8 类。

| 基础设施 | 作用 | Mental Model |
| --- | --- | --- |
| Runtime | 发现/加载 Plugin | Plugin Loader |
| `require()` | Lua Module | Module System |
| `vim.api` | 操作 Neovim 对象 | Native API |
| `vim.fn` | 调用 Vim Functions | Legacy API |
| `vim.cmd` | 执行 Ex Command | Command Interface |
| Autocmd | 响应事件 | Event System |
| Keymap | 响应用户输入 | Input System |
| User Command | 用户命令 | Command Pattern |
| `vim.b/w/t/g` | 保存作用域状态 | Scoped State |
| `vim.uv` | 异步 / OS | Async Runtime |
| `vim.ui` | UI 抽象 | UI Framework |
| `vim.lsp` | LSP 基础设施 | Language Framework |
| `vim.treesitter` | Syntax / AST | Parsing Framework |
| `vim.diagnostic` | Diagnostic | Diagnostic Framework |
| Namespace | Plugin 隔离 | Resource Namespace |
| Extmark | 稳定文本位置 | Anchor |
| Buffer | 文本数据 | Document |
| Window | Buffer 的视图 | Viewport |
| Tabpage | 工作区 | Workspace |

最重要的是：

Plugin 不需要自己重新实现这些东西。

Plugin 的主要工作是：

```text
        Compose
          ↓
      Neovim APIs
          ↓
      Neovim Runtime
          ↓
      Editor Objects
```

## 3. Plugin 最重要的四个组成部分

一个典型 Plugin 至少可以拆成：

```text
Plugin
│
├── Entry
│
├── Modules
│
├── Integration
│
└── State
```

进一步：

```text
Entry
 ├── Command
 ├── Keymap
 └── Autocmd

Modules
 ├── config.lua
 ├── core.lua
 ├── ui.lua
 └── utils.lua

Integration
 ├── vim.api
 ├── vim.fn
 ├── vim.cmd
 ├── vim.lsp
 └── vim.treesitter

State
 ├── Plugin state
 ├── Buffer state
 ├── Window state
 └── Global state
```

## 4. `plugin/`：Plugin 的 Runtime Entry

典型目录：

```text
myplugin/
├── plugin/
│   └── myplugin.lua
│
└── lua/
    └── myplugin/
        ├── init.lua
        ├── config.lua
        └── core.lua
```

### `plugin/myplugin.lua` 的职责：

```text
Neovim Startup
      │
      ▼
plugin/myplugin.lua
      │
      ├── register command
      ├── register keymap
      └── register autocmd
```

例如：

```lua
-- plugin/myplugin.lua

vim.api.nvim_create_user_command("MyHello", function()
    require("myplugin").hello()
end, {})
```

这里最重要的 Mental Model：

```text
plugin/*.lua
    =
Runtime Registration Layer
```

而不是：

```text
plugin/*.lua
    =
所有 Plugin Logic
```

## 5. `lua/`：真正的 Module / Application Logic

例如：

```text
lua/myplugin/
├── init.lua
├── config.lua
└── core.lua
```

对应：

```text
require("myplugin")

↓

lua/myplugin/init.lua
```

而：

```text
require("myplugin.config")

↓

lua/myplugin/config.lua
```

因此：

```text
require()
    │
    ▼
Lua Module Resolver
    │
    ▼
lua/<module>.lua
```

Mental Model：

```text
plugin/
    → Neovim Runtime Entry

lua/
    → Lua Application / Module Layer
```

## 6. 示例一：最小 Command Plugin

目标：

```text
:Hello
```

输出：

```text
Hello Neovim
```

目录：

```text
hello.nvim/
├── plugin/
│   └── hello.lua
└── lua/
    └── hello/
        └── init.lua
```

### `plugin/hello.lua`

```lua
vim.api.nvim_create_user_command("Hello", function()
    require("hello").run()
end, {})
```

### `lua/hello/init.lua`

```lua
local M = {}

function M.run()
    print("Hello Neovim")
end

return M
```

运行：

```text
Neovim Startup
      │
      ▼
plugin/hello.lua
      │
      ▼
register :Hello
      │
      │
      ▼
User executes :Hello
      │
      ▼
callback()
      │
      ▼
require("hello")
      │
      ▼
lua/hello/init.lua
      │
      ▼
M.run()
      │
      ▼
print()
```

这里包含了 Plugin 的最小完整闭环：

```text
Runtime
  ↓
Entry
  ↓
Command
  ↓
require()
  ↓
Module
  ↓
Function
```

## 7. 示例二：Keymap Plugin

目标：

```text
<leader>h
```

执行：

显示 Hello

```lua
-- plugin/hello.lua

vim.keymap.set("n", "<leader>h", function()
    require("hello").run()
end, {})
```

控制流：

```text
Keyboard
   │
   ▼
Keymap
   │
   ▼
callback
   │
   ▼
require("hello")
   │
   ▼
Plugin Logic
```

Mental Model：

```text
Command
    → Explicit User Action

Keymap
    → Input Event
```

两者最终都只是：

```text
Event
  ↓
Callback
  ↓
Plugin Logic
```

## 8. 示例三：Autocmd Plugin

目标：

```text
进入 Buffer
    ↓
Plugin 自动执行
```

```lua
vim.api.nvim_create_autocmd("BufEnter", {
    callback = function(args)
        print("Entered buffer: " .. args.buf)
    end,
})
```

控制流：

```text
Neovim
   │
   │ BufEnter
   ▼
Autocmd Dispatcher
   │
   ▼
Callback
   │
   ▼
Plugin Logic
```

所以：

```text
Command
    = User → Plugin

Keymap
    = User Input → Plugin

Autocmd
    = Neovim Event → Plugin
```

这是学习 Neovim Plugin 最重要的三个入口。

## 9. 示例四：Buffer Plugin

现在让 Plugin 真正操作 Neovim。

目标：

```text
:InsertHello
```

向当前 Buffer 添加一行。

```lua
vim.api.nvim_create_user_command("InsertHello", function()
    local buf = vim.api.nvim_get_current_buf()

    vim.api.nvim_buf_set_lines(
        buf,
        -1,
        -1,
        false,
        {"Hello from plugin"}
    )
end, {})
```

这里第一次出现重要对象：

Buffer

Mental Model：

```text
Plugin
   │
   ▼
Buffer Handle
   │
   ▼
vim.api.nvim_buf_*
   │
   ▼
Neovim Buffer
```

Buffer 可以理解成：

Document / Text Storage

而不是窗口。

## 10. Buffer ≠ Window

这是 Neovim Plugin 开发中非常重要的对象模型。

```text
                  Tabpage
                     │
              ┌──────┴──────┐
              ▼             ▼
           Window A       Window B
              │             │
              ▼             ▼
           Buffer X       Buffer X
```

一个 Buffer：

可以被多个 Window 显示

因此：

```text
Buffer
    = Text / Document

Window
    = Viewport
```

例如：

```lua
local buf = vim.api.nvim_get_current_buf()
local win = vim.api.nvim_get_current_win()
```

## 11. 示例五：Floating Window Plugin

目标：

```text
:MyFloat
```

显示一个浮动窗口。

```lua
vim.api.nvim_create_user_command("MyFloat", function()
    local buf = vim.api.nvim_create_buf(false, true)

    vim.api.nvim_buf_set_lines(
        buf,
        0,
        -1,
        false,
        {
            "Hello",
            "Neovim Plugin",
        }
    )

    vim.api.nvim_open_win(buf, true, {
        relative = "editor",
        width = 30,
        height = 5,
        row = 5,
        col = 10,
        style = "minimal",
        border = "rounded",
    })
end, {})
```

这里发生：

```text
Command
   │
   ▼
Create Buffer
   │
   ▼
Write Text
   │
   ▼
Create Window
   │
   ▼
Display Buffer
```

可以建立 UI Mental Model：

```text
Buffer
    = Data

Window
    = View

Keymap
    = Input

Command
    = Action

Autocmd
    = Lifecycle Event
```

这已经非常接近一个 GUI Application 的架构。

## 12. 示例六：Configuration Plugin

真实 Plugin 通常需要配置。

```lua
local M = {}

local defaults = {
    enabled = true,
    greeting = "Hello",
}

M.config = vim.deepcopy(defaults)

function M.setup(opts)
    M.config = vim.tbl_deep_extend(
        "force",
        defaults,
        opts or {}
    )
end

function M.hello()
    print(M.config.greeting)
end

return M
```

用户：

```lua
require("myplugin").setup({
    greeting = "Hello Morris",
})
```

控制流：

```text
User Configuration
        │
        ▼
setup(opts)
        │
        ▼
Merge Defaults
        │
        ▼
Plugin State
        │
        ▼
Plugin Behavior
```

重要理解：

`setup()`

不是 Neovim 特殊关键字。

它只是 Plugin 自己定义的 Lua Function。

因此：

```text
setup()
    =
Configuration Injection
```

## 13. 示例七：Buffer-local Plugin

有些 Plugin 的状态应该属于 Buffer。

例如：

```lua
vim.b.myplugin_enabled = true
```

读取：

```lua
if vim.b.myplugin_enabled then
    ...
end
```

Mental Model：

```text
Global State
    │
    └── vim.g

Buffer State
    │
    └── vim.b

Window State
    │
    └── vim.w

Tab State
    │
    └── vim.t
```

可以理解为：

```text
vim.g
    → Global

vim.b
    → Buffer-local

vim.w
    → Window-local

vim.t
    → Tabpage-local
```

这是一种非常重要的 State Scoping 机制。

## 14. 示例八：Filetype Plugin

如果功能只针对 Python：

```text
myplugin/
└── ftplugin/
    └── python.lua
```

打开 Python 文件：

```text
BufEnter
   │
   ▼
filetype = python
   │
   ▼
ftplugin/python.lua
   │
   ▼
Python-specific setup
```

例如：

```lua
vim.keymap.set("n", "<leader>r", function()
    print("Run Python")
end, {
    buffer = true,
})
```

Mental Model：

```text
plugin/
    → Global Runtime Entry

ftplugin/
    → Filetype-specific Runtime Entry
```

## 15. 示例九：Async / External Process Plugin

Plugin 可能需要执行外部程序。

例如：

```lua
vim.system(
    {"git", "status", "--short"},
    {},
    function(result)
        vim.schedule(function()
            print(result.stdout)
        end)
    end
)
```

这里进入：

```text
Plugin
   │
   ▼
Async API
   │
   ▼
External Process
   │
   ▼
Callback
   │
   ▼
Neovim
```

Mental Model：

```text
Plugin
   │
   ├── synchronous work
   │
   └── asynchronous work
             │
             ▼
        OS / libuv
```

核心原则：

不要在 Neovim 主循环中做长时间阻塞工作。

## 16. 示例十：LSP Plugin

Neovim 已经提供：

`vim.lsp`

因此 Plugin 不需要自己重新实现 LSP Protocol。

例如：

```lua
vim.lsp.buf.definition()
```

控制关系：

```text
Your Plugin
     │
     ▼
vim.lsp
     │
     ▼
LSP Client
     │
     ▼
Language Server
```

这体现了一个非常重要的 Plugin 设计思想：

```text
Neovim Framework
       │
       │ provides infrastructure
       ▼
Plugin
       │
       │ provides policy / behavior
       ▼
User Feature
```

## 17. 示例十一：Treesitter Plugin

类似地：

`vim.treesitter`

提供：

- Parser
- AST
- Syntax Tree
- Node
- Query

Plugin 可以：

```text
Treesitter
    ↓
AST
    ↓
Plugin analyzes AST
    ↓
Plugin produces UI / action
```

因此：

```text
Plugin
    不一定需要自己实现 Parser
```

而是：

```text
Plugin
    = Consumer of Framework
```

这就是 Neovim Plugin 生态最重要的设计优势之一。

## 18. Plugin 的推荐目录结构

一个中等复杂度 Plugin：

```text
myplugin/
│
├── plugin/
│   └── myplugin.lua
│
├── ftplugin/
│   ├── python.lua
│   └── rust.lua
│
├── lua/
│   └── myplugin/
│       ├── init.lua
│       ├── config.lua
│       ├── commands.lua
│       ├── keymaps.lua
│       ├── autocmds.lua
│       ├── core.lua
│       ├── state.lua
│       ├── ui.lua
│       └── utils.lua
│
├── doc/
│   └── myplugin.txt
│
└── README.md
```

推荐职责：

```text
plugin/myplugin.lua
    ↓
Runtime Registration

lua/myplugin/init.lua
    ↓
Public API

config.lua
    ↓
Configuration

commands.lua
    ↓
Commands

keymaps.lua
    ↓
Keymaps

autocmds.lua
    ↓
Events

core.lua
    ↓
Business Logic

state.lua
    ↓
State

ui.lua
    ↓
UI

utils.lua
    ↓
Generic Utilities
```

但注意：

不要为了“看起来专业”而过度拆文件。

文件应该由职责驱动，而不是由文件数量驱动。

## 19. 推荐的 Plugin 基本 Framework

一个比较通用的架构：

```text
                         Neovim
                            │
                     Runtime Lifecycle
                            │
                            ▼
                 ┌────────────────────┐
                 │  Entry Layer       │
                 │                    │
                 │ plugin/*.lua       │
                 │ ftplugin/*.lua     │
                 └─────────┬──────────┘
                           │
                ┌──────────┼───────────┐
                ▼          ▼           ▼
             Command     Keymap      Autocmd
                │          │           │
                └──────────┼───────────┘
                           ▼
                  ┌─────────────────┐
                  │ Application     │
                  │ Layer           │
                  │                 │
                  │ lua/myplugin/*  │
                  └────────┬────────┘
                           │
                ┌──────────┼──────────┐
                ▼          ▼          ▼
              Logic      State        UI
                │          │          │
                └──────────┼──────────┘
                           ▼
                  Neovim Abstraction
                           │
          ┌────────────────┼─────────────────┐
          ▼                ▼                 ▼
       vim.api           vim.fn           vim.cmd
          │
          ├── Buffer
          ├── Window
          ├── Tabpage
          ├── Namespace
          ├── Extmark
          └── Autocmd
                           │
                           ▼
                      Neovim Core
```

## 20. Plugin 的生命周期

理解生命周期比理解 API 更重要。

```text
                    Neovim Start
                         │
                         ▼
                Runtimepath Discovery
                         │
                         ▼
                  plugin/*.lua
                         │
                         ▼
                 Register Entry
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          Command      Keymap      Autocmd
             │           │           │
             └───────────┼───────────┘
                         │
                         ▼
                    Wait / Idle
                         │
                         ▼
                  User / Neovim Event
                         │
                         ▼
                      Handler
                         │
                         ▼
                   require(module)
                         │
                         ▼
                   Plugin Logic
                         │
                 ┌───────┼────────┐
                 ▼       ▼        ▼
              Buffer   Window    UI
                 │       │        │
                 └───────┼────────┘
                         ▼
                     State
                         │
                         ▼
                  Wait for Event
                         │
                         └───────────→ ...
```

所以 Plugin 更像：

```text
register
   ↓
wait
   ↓
event
   ↓
handle
   ↓
change state
   ↓
update editor
   ↓
wait
```

而不是：

```text
start
   ↓
run everything
   ↓
exit
```

## 21. Plugin 的 Control Flow

一个完整 Plugin 的控制流可以抽象成：

```text
                  ┌─────────────┐
                  │   Neovim    │
                  └──────┬──────┘
                         │
                    Event/Input
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
       Command         Keymap        Autocmd
          │              │              │
          └──────────────┼──────────────┘
                         ▼
                      Handler
                         │
                         ▼
                     Controller
                         │
                         ▼
                       State
                         │
                         ▼
                     Core Logic
                         │
                         ▼
                   Neovim API
                         │
                         ▼
              Buffer / Window / UI
                         │
                         ▼
                  Visible Result
```

## 22. Plugin 的 Data Flow

例如一个搜索 Plugin：

```text
User Input
    │
    ▼
Command / Keymap
    │
    ▼
Search Query
    │
    ▼
Core Logic
    │
    ├── filesystem
    ├── cache
    └── external process
    │
    ▼
Results
    │
    ▼
State
    │
    ▼
UI Buffer
    │
    ▼
Floating Window
```

因此一个 Plugin 通常同时存在两条流：

### Control Flow

```text
User/Event
    ↓
Handler
    ↓
Logic
    ↓
API
```

### Data Flow

```text
Input
    ↓
State
    ↓
Transformation
    ↓
UI / Buffer
```

## 23. Plugin 的 State Mental Model

Plugin State 是复杂 Plugin 的核心。

可以把 State 分成：

```text
                  Plugin State
                       │
          ┌────────────┼─────────────┐
          ▼            ▼             ▼
       Global       Buffer        Window
          │            │             │
       config       metadata        UI state
       cache        enabled         selection
       manager      parser          cursor
```

对应：

```text
Lua local/table
    → Plugin internal state

vim.g
    → Global editor state

vim.b
    → Buffer state

vim.w
    → Window state

vim.t
    → Tabpage state
```

一个非常重要的问题：

这个状态到底属于谁？

如果状态属于：

```text
Plugin
    → local table

Buffer
    → vim.b / buffer keyed table

Window
    → vim.w

Tab
    → vim.t
```

不要把所有状态都塞进一个：

```lua
M.state = {}
```

## 24. Plugin 与 Neovim Object 的关系

建立这个 Object Model：

```text
                    Neovim
                       │
          ┌────────────┼─────────────┐
          ▼            ▼             ▼
       Buffer        Window       Tabpage
          │            │             │
          │            │             │
          ▼            ▼             ▼
        Text         View         Workspace
          │
          ├── Namespace
          ├── Extmark
          └── Highlight
```

Plugin 通常不是直接“操作 Neovim”。

而是：

```text
Plugin
   │
   ▼
Object
   │
   ▼
API
   │
   ▼
Neovim
```

例如：

```lua
vim.api.nvim_buf_get_lines(...)
```

本质是：

```text
Plugin
  ↓
Buffer
  ↓
Neovim Buffer API
```

## 25. `vim.api` / `vim.fn` / `vim.cmd` 的关系

不要把三个 API 混成一回事。

```text
                 Lua Plugin
                     │
          ┌──────────┼──────────┐
          ▼          ▼          ▼
       vim.api     vim.fn     vim.cmd
          │          │          │
          ▼          ▼          ▼
      Nvim API   Vim Function  Ex Command
```

Mental Model：

```text
vim.api
    → 操作 Neovim 对象

vim.fn
    → 调用 Vim Function

vim.cmd
    → 执行 Vim/Ex Command
```

现代 Plugin 通常优先寻找：

专门的 `vim.api` / `vim.*` API

而不是到处：

```lua
vim.cmd("...")
```

## 26. `require()` 的真正作用

很多人把：

```lua
require("xxx")
```

理解成：

“加载一个文件”

更准确的理解是：

```text
require()
    =
Module Resolution
+
Module Initialization
+
Module Caching
```

例如：

```lua
local M = {}

function M.foo()
end

return M
```

然后：

```lua
local x = require("myplugin")
```

得到：

```text
require()
    │
    ▼
Module
    │
    ▼
return M
    │
    ▼
x
```

第二次：

```lua
require("myplugin")
```

不会重新执行整个 Module，而是使用已经加载的 Module。

因此：

```text
require()
    → Module Boundary
```

## 27. 为什么推荐 Lazy Loading？

考虑：

```text
Neovim Startup
      │
      ▼
plugin/myplugin.lua
      │
      ├── require("huge_module")
      ├── require("ui")
      ├── require("parser")
      ├── require("network")
      └── require("database")
```

那么启动时就全部加载。

更好的方式：

```text
Neovim Startup
      │
      ▼
register command
      │
      ▼
wait
      │
      ▼
User invokes command
      │
      ▼
require("myplugin")
      │
      ▼
load required modules
```

因此：

```text
Entry Layer
    → Thin

Application Logic
    → Lazy Load
```

这也是 Neovim Plugin 性能设计中的重要思想。

## 28. Plugin Framework 与传统软件架构的对应关系

如果使用你熟悉的软件工程 Mental Model：

Neovim Plugin

可以近似理解成：

```text
                    Neovim
                  Host / Framework
                        │
                        ▼
                Runtime / Events
                        │
                        ▼
                 Adapter Layer
                        │
                        ▼
                 Application Layer
                        │
                 ┌──────┴──────┐
                 ▼             ▼
               State         Logic
                 │             │
                 └──────┬──────┘
                        ▼
                    Neovim API
```

对应：

```text
Neovim
    → Framework

plugin/*.lua
    → Adapter / Registration

lua/myplugin/*
    → Application

vim.api
    → Infrastructure API

Autocmd
    → Event System

Command
    → Command Interface

Keymap
    → Input Interface

State
    → Application State
```

## 29. 一个非常重要的设计原则

可以把 Neovim Plugin 设计总结为：

Framework owns the lifecycle.
Plugin owns the behavior.

也就是：

```text
Neovim
    owns:
        Runtime
        Event Loop
        Buffer
        Window
        LSP
        Treesitter
        UI
        API

Plugin
    owns:
        Feature
        Policy
        Behavior
        Plugin State
```

所以不要：

```text
Plugin
    ↓
重新实现 Neovim
```

而应该：

```text
Plugin
    ↓
Compose Neovim Infrastructure
```

## 30. 开发 Neovim Plugin 的推荐流程

不要先写代码。

按照：

```text
WHY
 ↓
WHAT
 ↓
EVENT
 ↓
OBJECT
 ↓
STATE
 ↓
MODULE
 ↓
API
 ↓
LIFECYCLE
 ↓
FAILURE
```

进行设计。

### Step 1：WHY

先问：

这个 Plugin 解决什么问题？

例如：

快速搜索项目文件

### Step 2：WHAT

定义用户看到的功能：

```text
:ProjectFiles
<leader>pf
```

### Step 3：EVENT

确定 Plugin 如何获得控制权：

- Command
- Keymap
- Autocmd
- User Event

### Step 4：OBJECT

确定 Plugin 操作什么：

- Buffer
- Window
- Tabpage
- Extmark
- Namespace
- LSP
- Treesitter

### Step 5：STATE

问：

Plugin 需要保存什么？

例如：

- current query
- results
- selected item
- window handle
- buffer handle
- cache
- configuration

### Step 6：MODULE

设计：

```text
lua/myplugin/
├── init.lua
├── config.lua
├── core.lua
├── state.lua
└── ui.lua
```

### Step 7：API

再选择：

- `vim.api`
- `vim.fn`
- `vim.cmd`
- `vim.uv`
- `vim.ui`
- `vim.lsp`
- `vim.treesitter`
- `vim.diagnostic`

### Step 8：LIFECYCLE

考虑：

```text
Startup
   ↓
Registration
   ↓
Event
   ↓
Handler
   ↓
State
   ↓
UI
   ↓
Cleanup
```

### Step 9：FAILURE

最后考虑：

```text
Plugin 没加载？
    ↓
runtimepath / plugin/*.lua

Command 不存在？
    ↓
registration

require 失败？
    ↓
lua module path

Keymap 不生效？
    ↓
mode / buffer / mapping

Autocmd 不触发？
    ↓
event / pattern

Buffer 操作失败？
    ↓
buffer handle / validity

UI 出错？
    ↓
window / buffer lifecycle

启动变慢？
    ↓
eager require / heavy initialization
```

## 31. Plugin Debugging Mental Model

遇到问题时，不要马上搜索 API。

沿着这条链定位：

```text
                    User
                     │
                     ▼
              Input / Event
                     │
                     ▼
                 Entry Point
                     │
                     ▼
                  Handler
                     │
                     ▼
                 require()
                     │
                     ▼
                 Module
                     │
                     ▼
                Plugin State
                     │
                     ▼
                 vim.api
                     │
                     ▼
              Neovim Object
                     │
                     ▼
                  Result
```

例如：

“我的 `:MyCommand` 没有反应”

应该依次检查：

```text
1. Command 是否注册？
        ↓
2. callback 是否执行？
        ↓
3. require() 是否成功？
        ↓
4. Module 是否正确？
        ↓
5. State 是否正确？
        ↓
6. vim.api 是否调用成功？
        ↓
7. Buffer / Window 是否仍然有效？
```

这比“到处加 print”更有效。

## 32. 最重要的 10 个 Mental Models

### Mental Model 1：Neovim 是 Host

```text
Neovim
    ↓
提供 Runtime + API + Objects
    ↓
Plugin
    ↓
提供 Feature
```

### Mental Model 2：Plugin 是 Event-Driven Application

```text
Event
  ↓
Handler
  ↓
State
  ↓
Effect
```

### Mental Model 3：`plugin/` 是 Entry Layer

```text
plugin/
    ↓
Register
```

不是：

```text
plugin/
    ↓
所有业务逻辑
```

### Mental Model 4：`lua/` 是 Module Layer

```text
lua/myplugin/
    ↓
Application Logic
```

### Mental Model 5：Command / Keymap / Autocmd 都是入口

```text
Command
Keymap
Autocmd
   ↓
Callback
   ↓
Plugin Logic
```

### Mental Model 6：Buffer 是数据，Window 是视图

```text
Buffer
    = Document

Window
    = Viewport
```

### Mental Model 7：`setup()` 是 Configuration Injection

```text
User Config
     ↓
setup()
     ↓
Plugin State
```

### Mental Model 8：`require()` 是 Module Boundary

```text
Entry
  ↓
require()
  ↓
Module
```

### Mental Model 9：Neovim Framework 提供能力，Plugin 组合能力

```text
Neovim
    ├── LSP
    ├── Treesitter
    ├── Diagnostics
    ├── Buffer
    ├── Window
    └── UI
          ↓
       Plugin
          ↓
      New Feature
```

### Mental Model 10：Plugin 开发本质是“注册 + 响应 + 状态 + 效果”

最终可以压缩成：

```text
              ┌─────────────┐
              │   Register  │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │    Wait     │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │    Event    │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │   Handler   │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │    State    │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │    Effect   │
              └──────┬──────┘
                     │
                     ▼
                   Neovim
```

## 33. 最终总图

把整个 Neovim Lua Plugin 压缩成一张 Mental Model：

```text
                         NEOVIM HOST
                              │
              ┌───────────────┼────────────────┐
              │               │                │
           Runtime          Events           Objects
              │               │                │
              ▼               ▼                ▼
        plugin/*.lua     Command/Keymap     Buffer
        ftplugin/*.lua   Autocmd            Window
              │          User Event          Tabpage
              │                              Extmark
              ▼                              Namespace
          Entry Layer
              │
              │ require()
              ▼
       ┌──────────────────────┐
       │    Lua Module Layer  │
       │                      │
       │ init.lua             │
       │ config.lua           │
       │ core.lua             │
       │ state.lua            │
       │ ui.lua               │
       └──────────┬───────────┘
                  │
          ┌───────┼────────┐
          ▼       ▼        ▼
        Logic   State      UI
          │       │        │
          └───────┼────────┘
                  │
                  ▼
             vim.* APIs
                  │
       ┌──────────┼───────────┐
       ▼          ▼           ▼
    vim.api     vim.fn      vim.cmd
       │
       ├── Buffer
       ├── Window
       ├── Tabpage
       ├── Namespace
       ├── Extmark
       ├── Autocmd
       └── UI
                  │
                  ▼
             Neovim Core
```

## 34. 最终一句话

Neovim Lua Plugin = Runtime Entry + Lua Modules + Event Handlers + Plugin State + Neovim Infrastructure

开发 Plugin 时不要从：

“我需要调用哪个 `vim.api`？”

开始。

应该从：

```text
WHY
 ↓
我要解决什么问题？
 ↓
USER/EVENT
 ↓
用户或 Neovim 如何触发？
 ↓
OBJECT
 ↓
我要操作哪个 Neovim Object？
 ↓
STATE
 ↓
我要维护什么状态？
 ↓
MODULE
 ↓
代码如何组织？
 ↓
INFRASTRUCTURE
 ↓
Neovim 已经提供了什么能力？
 ↓
LIFECYCLE
 ↓
什么时候注册、加载、执行、清理？
 ↓
FAILURE
 ↓
哪一个阶段、哪一个对象、哪一种状态出错？
```

最终形成一个非常稳定的心智模型：

```text
                 Neovim = Framework / Host
                            │
                 ┌──────────┴──────────┐
                 │                     │
              Lifecycle             Objects
                 │                     │
                 ▼                     ▼
           Event / Input          Buffer / Window
                 │                     │
                 └──────────┬──────────┘
                            ▼
                         Plugin
                            │
                 ┌──────────┼──────────┐
                 ▼          ▼          ▼
              Entry       State       Logic
                 │          │          │
                 └──────────┼──────────┘
                            ▼
                        vim.* API
                            │
                            ▼
                      Neovim Core
```

最值得记住的不是 API，而是这条关系：

```text
Neovim owns the Runtime and Infrastructure
                    ↓
Plugin registers itself into that Runtime
                    ↓
Event gives Plugin control
                    ↓
Plugin executes its Logic
                    ↓
State remembers what happened
                    ↓
Neovim API produces the Effect
                    ↓
Plugin waits for the next Event
```

这就是 Neovim Lua Plugin 最核心的 Architecture + Lifecycle + Control Flow + State 心智模型。
