# 01-3 · 示例：Configuration Helper

> Pattern：Table Module（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/03-config-helper/](code/03-config-helper/)（`config.lua` / `app.lua`）。

## 1. Problem / Scenario

项目里通常会有一批"默认配置"和"feature flag"：应用名、超时时间、日志级别、若干开关（是否启用暗色模式、是否启用实验性缓存……）。我们想要一个统一的地方：

- 存放这些默认值，让项目里任何地方都能查（`config.get("timeout_ms")`）；
- 查不到某个 key 时返回调用方指定的兜底值，而不是直接报错或返回 `nil` 让调用方到处判空；
- 提供一个语义清晰的 `is_enabled(flag)`，而不是让调用方到处写 `config.feature_flags[flag] == true` 这种重复的判断逻辑。

## 2. Architecture

```text
Application (app.lua)
    |
    v
+----------------------------+
|      Module: config        |
|                             |
|   M.defaults (table)       |
|   M.feature_flags (table)  |
|   M.get(key, fallback)     |
|   M.is_enabled(flag)       |
+----------------------------+
```

和 `http.lua` 的结构类似：两张静态数据表 + 两个围绕它们的查询函数，没有任何写入型函数（没有 `config.set(...)`）——这是本例要特别强调的一点，见第 9 节。

## 3. Framework / Caller 代码

```lua
local config = require("config")
```

`app.lua` 依次演示：读取一个已知配置项、读取一个不存在的 key（带 fallback）、检查几个 feature flag（包括一个不存在的 flag）。

## 4. Module 代码

```lua
local M = {}

M.defaults = {
    app_name = "myapp",
    max_connections = 100,
    timeout_ms = 3000,
    log_level = "info",
}

M.feature_flags = {
    dark_mode = true,
    experimental_cache = false,
    verbose_logging = false,
}

function M.get(key, fallback)
    local value = M.defaults[key]
    if value == nil then
        return fallback
    end
    return value
end

function M.is_enabled(flag)
    return M.feature_flags[flag] == true
end

return M
```

`M.is_enabled` 用 `== true` 而不是直接 `return M.feature_flags[flag]`，是为了保证未知 flag（值是 `nil`）和显式关闭的 flag（值是 `false`）都统一返回 `false`，调用方不需要区分"没有这个 flag"和"这个 flag 是 false"这两种情况。

## 5. 完整 Lua 代码

与磁盘上的文件完全一致。

`code/03-config-helper/config.lua`：

```lua
-- config.lua
--
-- Table Module：配置读取的 Namespace。
-- 内部只保存"静态默认值"（defaults / feature_flags），不追踪
-- "谁在什么时候改过某个配置项"——一旦需要追踪修改历史，
-- 就已经超出 Table Module 的边界，会被推向 Pattern 2/3。

local M = {}

--- 默认配置：只读的静态数据，随模块加载一次性确定
M.defaults = {
    app_name = "myapp",
    max_connections = 100,
    timeout_ms = 3000,
    log_level = "info",
}

--- 默认 feature flag：只读静态数据
M.feature_flags = {
    dark_mode = true,
    experimental_cache = false,
    verbose_logging = false,
}

--- get(key, fallback) -> any
-- 从 defaults 中查找 key，找不到时返回调用者传入的 fallback。
function M.get(key, fallback)
    local value = M.defaults[key]
    if value == nil then
        return fallback
    end
    return value
end

--- is_enabled(flag) -> boolean
-- 查询某个 feature flag 是否开启，未知 flag 视为未开启。
function M.is_enabled(flag)
    return M.feature_flags[flag] == true
end

return M
```

`code/03-config-helper/app.lua`：

```lua
-- app.lua
--
-- Framework/Application：require("config") 拿到配置读取 API，
-- 只通过 get()/is_enabled() 访问配置，不直接改 config.defaults。

local config = require("config")

print("== 读取已知配置项 ==")
print("  app_name: " .. config.get("app_name"))
print("  timeout_ms: " .. config.get("timeout_ms"))

print("\n== 读取未知配置项（带 fallback 默认值）==")
print("  retry_count (fallback=3): " .. config.get("retry_count", 3))

print("\n== Feature Flag 检查 ==")
local flags_to_check = { "dark_mode", "experimental_cache", "unknown_flag" }
for _, flag in ipairs(flags_to_check) do
    print(string.format("  %s -> %s", flag, tostring(config.is_enabled(flag))))
end
```

## 6. Execution

```bash
cd code/03-config-helper
lua5.4 app.lua
```

实际执行输出（已用本地 Lua 5.4 验证）：

```text
== 读取已知配置项 ==
  app_name: myapp
  timeout_ms: 3000

== 读取未知配置项（带 fallback 默认值）==
  retry_count (fallback=3): 3

== Feature Flag 检查 ==
  dark_mode -> true
  experimental_cache -> false
  unknown_flag -> false
```

## 7. Execution Flow

1. `require("config")` 执行 `config.lua`，创建并填充 `M.defaults`、`M.feature_flags` 两张表，挂上 `get`/`is_enabled` 两个函数，`return M`；
2. `config.get("app_name")` 查 `M.defaults.app_name`，命中，直接返回 `"myapp"`；
3. `config.get("retry_count", 3)` 查 `M.defaults.retry_count`，得到 `nil`，触发 fallback 分支，返回调用方传入的 `3`；
4. `config.is_enabled("dark_mode")` 查 `M.feature_flags.dark_mode`，值是 `true`，`== true` 成立，返回 `true`；
5. `config.is_enabled("unknown_flag")` 查到 `nil`，`nil == true` 为 `false`，返回 `false`——和"显式关闭"的 flag 表现一致，调用方不需要额外判断。

## 8. State

无（严格意义上的"运行时可变状态"为零）。`M.defaults` 和 `M.feature_flags` 是静态默认值，**这份示例故意只提供 `get`，不提供 `set`**——一旦配置需要在运行期被修改（比如用户在 UI 里切换了一个开关），这个 Module 就不再只是"默认值的静态快照"，而是需要变成一个持有可变运行时状态的东西。

> **关键边界（本例要点）**：只要 `config.lua` 里只有"默认值 + 只读查询函数"，它就仍然是 Table Module。但如果未来需求变成"谁在什么时候修改了哪个 flag"（例如需要审计日志、需要支持运行时 `config.set("dark_mode", false)` 并记录修改人和修改时间），这份配置数据就从"静态常量"变成了"需要被追踪、被保护的可变状态"——这正是 [00-why-and-architecture.md](00-why-and-architecture.md) 第 7 节讲的越界信号：一旦出现这种需求，`config.lua` 就应该演化成 Pattern 2（Private + Public，把可变状态关进 `local` 变量，只通过 `set`/`get` 函数访问，`set` 内部顺带记录审计信息），甚至如果需要"多套互相独立的配置实例"（比如多租户，每个租户有自己独立的配置），就要进一步演化成 Pattern 3（Class/Constructor，每个租户对应一个独立的配置对象）。

## 9. Why this Pattern fits

- 目前的需求就是"读默认值 + 查 flag"，没有任何写入/修改的需求，天然符合 Table Module"无状态、纯查询"的定位；
- `defaults`/`feature_flags` 是项目级共享的一份数据，本来就应该是单例，`require` 的缓存语义正好提供这种单例保证，不需要额外写单例模式的样板代码。

## 10. What would go wrong with another Pattern

- **换成 Class/Constructor**：`local cfg = Config.new(); cfg:get("app_name")` ——但默认配置对整个项目应该是唯一的一份，构造多个 `Config` 实例反而制造了"到底该用哪个实例的配置"这种不必要的疑惑，纯属过度设计。
- **换成 Private+Public（现在就换，而不是等真的需要 `set` 时再换）**：会提前引入一层没有必要的封装——`defaults`/`feature_flags` 目前就是设计上允许被外部只读访问的数据，没有需要对调用者隐藏的实现步骤，提前封装除了多写代码没有任何好处。等到真的出现"需要审计修改记录"的需求时再重构成 Private+Public，是更合适的时机。
- **换成 Factory**：Factory 解决"选择具体实现"的问题，而这里读取配置的逻辑始终只有一种实现（查表 + fallback），不存在"根据情况选择不同配置读取策略"的需求。

## 11. Mental Model

> `config.lua` 就是项目的"出厂默认设置说明书"：你可以随时翻开来查（`get`/`is_enabled`），但说明书本身不会因为你查了几次就发生变化——一旦有人想在说明书上直接涂改、还要求系统记住是谁改的、什么时候改的，那就已经不是"说明书"该干的事了。
