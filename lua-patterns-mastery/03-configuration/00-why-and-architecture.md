# 00 · Why and Architecture — Pattern C: Configuration

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文档复用其中定义的词汇——Host 拥有 Mechanism / Resource / Lifecycle / Stable API；Lua 拥有 Behavior / Policy / Rules / Configuration / Extension——不重复定义，只落地到 Configuration 这一种分工方式。

## 1. WHY：Configuration 到底解决了什么问题

先纠正一个容易望文生义的误解：

**Configuration 这个 Pattern 的本质，不是"把配置文件从 `.ini`/`.toml`/`.json` 换成一个 `.lua` 文件"，而是"让配置本身具备计算能力"。**

传统 key=value 配置（`.ini`、`.env`、大多数 `.toml`/`.json`）只能表达一种东西：**Data**（数据）。它天生做不到，或者只能靠土办法（模板引擎、预处理脚本、环境变量替换）曲线做到：

- 引用运行环境（"生产环境用 80 端口，其它环境用 8080"）；
- 做条件分支（"如果这台机器是 mac，就用另一套路径"）；
- 定义并复用函数（"三个地方都要按难度缩放数值，写一个函数，别复制三份"）；
- 引用另一个字段做派生计算（"超时时间 = 心跳间隔 × 3"）。

而 Lua Configuration 天然具备表达 **Data + Expression + Function + Conditional Logic** 的能力，因为配置文件本身就是一段可以被执行的 Lua 代码：

```lua
config = {
    port = 8080,
    workers = 4,
    timeout = 30,
    debug = is_development(),
    endpoint = function(env)
        if env == "production" then
            return "https://api.example.com"
        end
        return "http://localhost:8080"
    end,
}
```

这份"配置"里，`debug` 是一次函数调用的结果，`endpoint` 干脆是一个函数值——它在"什么时候被调用"这件事上，把决定权交给了 Host。这已经不是传统意义上的"配置文件"，而是"一段在加载时执行、产出一份运行时参数表的程序"。

## 2. Architecture

```text
Host
 |
 +-- Config API
 |
 +-- Lua VM
       |
       v
   config.lua
       |
       v
 Runtime Configuration
```

逐层解释：

- **Host**：拥有主程序的 Mechanism（真正的网络监听、线程池创建、渲染管线等）。Host 决定"什么时候需要一份配置"，通常只在启动阶段（或收到 reload 信号时）向 Lua 要一次。
- **Config API**：Host 暴露给 config.lua 的、极少数用于"计算"的 C 函数（例如上面 01 示例里的 `os_default_workers()`），以及在加载脚本前注入的只读全局变量（例如 `ENV`）。这一层通常非常薄——Configuration Pattern 里 Host 几乎不需要向 Lua 暴露"做事"的能力，只需要暴露"取值/计算"的能力。这是它和 Extension Pattern 的关键区别（见第 4 节）。
- **Lua VM**：一个短生命周期的 `lua_State`——通常"加载配置 -> 读取结果 -> 立刻 `lua_close`"，不会像 Plugin 那样常驻。
- **config.lua**：脚本本身，声明若干全局变量（`server`、`player`、`weapons` ……），可以包含条件分支、函数调用、局部变量。
- **Runtime Configuration**：Host 从 Lua VM 里"拽出来"、转换成的纯 C struct/数组。转换完成后，这份数据就和 Lua 彻底脱钩了——运行时的业务逻辑读的是 C struct，不会反复回头调用 Lua。这也是 Configuration 和 Policy 的关键区别（见第 4 节）。

## 3. Static Config vs Executable Config

|            | Static Config（`.ini`/`.json`/`.toml`） | Executable Config（Lua） |
|---|---|---|
| 引用环境变量 | 不支持，需要外部预处理/模板替换 | 原生支持：`os.getenv(...)` 或 Host 注入的全局变量 |
| 条件分支 | 不支持，只能靠多份文件 + 外部选择逻辑 | 原生支持：`if/else` |
| 定义并复用函数 | 不支持 | 原生支持：`local function ...` |
| 字段间派生计算 | 不支持，容易出现"改了 A 忘记同步改 B" | 原生支持：`timeout = heartbeat * 3` |
| 语法本身出错的后果 | 解析器报"格式错误"，通常是一条清晰的错误信息 | 可能是语法错误（`luaL_loadfile` 报错，行为和静态配置一致）也可能是运行时错误（脚本执行到一半才崩，见 10-Failure 相关章节） |
| 可读性/可审计性 | 高：一眼看完就是全部事实 | 较低：需要读完脚本逻辑才能确定最终值是什么 |
| 安全边界 | 天然安全：解析器不会执行任意代码 | 需要额外考虑（不可信来源的配置 = 允许执行任意 Lua 代码，见 03-example-game-configuration.md 的风险小节） |

## 4. 本质区别：Configuration vs 其余四种 Pattern

- **vs Extension**：Extension 是"给 Host 新增一种它原本不会做的能力"（新命令、新协议解析器）；Configuration 是"描述 Host 已经会做的事情该用什么参数去做"。一句话：Extension 回答"能做什么新的事"，Configuration 回答"已有的事该用什么参数跑"。
- **vs Policy**：Policy 通常在**每一次请求/事件到达时**都会被调用一次，用当时的上下文做出一个决策；Configuration 通常只在**启动时（或极少数的 reload 时机）读一次**，读完就转成 C 数据，运行期间不再回头问 Lua。一句话：Policy 是"每次都问"，Configuration 是"问一次，记住答案"。
- **vs Rule Engine**：Rule Engine 是一份"IF 条件 THEN 动作"的匹配表，条数可能成百上千，核心是"匹配"；Configuration 是一份结构化的参数表，核心是"取值"，即便带了少量条件分支，分支数量也远小于 Rule Engine 的规则条数。
- **vs Plugin**：Plugin 是一个有自己独立生命周期（load/enable/disable/unload）、可能常驻并被反复回调的模块；Configuration 只是一次性的"数据来源"，没有独立的生命周期，加载完就可以把 Lua VM 关掉。

## 5. Host / Lua 职责划分（复用 00 的词汇）

```text
Host
 |
 +-- Mechanism      真正的网络监听/线程池/渲染管线（不由配置决定"有没有"，只由配置决定"参数是什么"）
 +-- Resource       监听 socket、文件句柄——由 Runtime Configuration 里的参数创建，但创建动作在 Host
 +-- Lifecycle      "何时读取配置"（启动时 / 收到 reload 信号时）
 +-- Stable API     Config API：极少数只读的计算函数 + 只读的注入变量
          |
          v
         Lua
          |
 +-- Configuration  声明 Data，可选地掺入 Expression / Function / Conditional Logic
```

Configuration Pattern 里，Lua 侧几乎只承担"Configuration"这一项职责，不承担 Behavior（不响应事件）、不承担 Policy（不做即时决策）、不承担 Rules（没有大量匹配规则）——这正是它在 5 种模式里边界最"薄"、最容易被误判为"只是换了个文件格式"的一种。

## 6. 导航

- [01-example-c-app-configuration.md](01-example-c-app-configuration.md) — C Application + Lua Configuration
- [02-example-neovim-configuration.md](02-example-neovim-configuration.md) — Neovim Configuration
- [03-example-game-configuration.md](03-example-game-configuration.md) — Game Configuration
