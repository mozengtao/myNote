# 01-1 · 示例：String Utility

> Pattern：Table Module（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/01-string-utils/](code/01-string-utils/)（`string_utils.lua` / `app.lua`）。

## 1. Problem / Scenario

几乎每个项目都会需要一组基础字符串处理函数：把一行 CSV 切成字段（`split`）、去掉用户输入两端多余的空白（`trim`）、把处理完的字段重新拼回一行（`join`）。这些操作：

- 彼此独立，互不依赖对方的内部实现；
- 都是纯函数——给定输入，永远得到相同输出，不依赖任何"上一次调用"留下的痕迹；
- 会被项目里很多不同的地方反复调用（解析配置文件、处理命令行参数、格式化日志……）。

我们想要的效果是：把这三个函数收纳成一个统一的、可以用 `require("string_utils")` 整体引入的单元，而不是让每个使用它们的文件各自重复实现一遍，或者散落成三个互不相关的全局函数。

## 2. Architecture

```text
Application (app.lua)
    |
    v
+---------------------------+
|   Module: string_utils    |
|                           |
|   M.split(str, sep)       |
|   M.trim(str)             |
|   M.join(list, sep)       |
+---------------------------+
```

`app.lua` 扮演 Framework/Caller 的角色，`string_utils.lua` 是 Table Module。两者之间没有第三层——没有独立的实现文件，`string_utils.lua` 里声明和实现是同一份代码。

## 3. Framework / Caller 代码

```lua
local string_utils = require("string_utils")
```

`app.lua` 只做三件事：拿到 Module 引用、按顺序调用 `split` -> `trim` -> `join`、打印每一步的中间结果，用来验证 Module 的行为符合预期。

## 4. Module 代码

```lua
local M = {}

function M.split(str, sep)
    sep = sep or ","
    local result = {}
    local start = 1
    local sep_start, sep_end = string.find(str, sep, start, true)
    while sep_start do
        table.insert(result, string.sub(str, start, sep_start - 1))
        start = sep_end + 1
        sep_start, sep_end = string.find(str, sep, start, true)
    end
    table.insert(result, string.sub(str, start))
    return result
end

function M.trim(str)
    return (str:gsub("^%s+", ""):gsub("%s+$", ""))
end

function M.join(list, sep)
    sep = sep or ","
    return table.concat(list, sep)
end

return M
```

`M.split` 用 `string.find(str, sep, start, true)` 的 `plain=true` 模式做纯文本查找，而不是 Lua 模式匹配——这样即使分隔符本身含有 `.`、`%` 这类模式特殊字符，也能按字面意思切分，不会出现"意外匹配"的 bug。

## 5. 完整 Lua 代码

与磁盘上的文件完全一致。

`code/01-string-utils/string_utils.lua`：

```lua
-- string_utils.lua
--
-- Table Module：一组无状态的字符串处理函数，供 Framework/Application 调用。
-- 只暴露 API（split/trim/join），不持有任何运行时状态。
-- 结构固定：local M = {} ... return M。

local M = {}

--- split(str, sep) -> table
-- 按分隔符 sep（默认逗号）把字符串切成一个数组。
-- 用 plain=true 的 string.find 而不是模式匹配，避免 sep 本身含有 Lua
-- 模式特殊字符（例如 "." "%"）时出现匹配错误。
function M.split(str, sep)
    sep = sep or ","
    local result = {}
    local start = 1
    local sep_start, sep_end = string.find(str, sep, start, true)
    while sep_start do
        table.insert(result, string.sub(str, start, sep_start - 1))
        start = sep_end + 1
        sep_start, sep_end = string.find(str, sep, start, true)
    end
    table.insert(result, string.sub(str, start))
    return result
end

--- trim(str) -> string
-- 去掉字符串首尾空白字符（空格/Tab/换行）。
function M.trim(str)
    return (str:gsub("^%s+", ""):gsub("%s+$", ""))
end

--- join(list, sep) -> string
-- 用分隔符 sep（默认逗号）把数组拼接成一个字符串。
function M.join(list, sep)
    sep = sep or ","
    return table.concat(list, sep)
end

return M
```

`code/01-string-utils/app.lua`：

```lua
-- app.lua
--
-- Framework/Application：通过 require("string_utils") 拿到 Module 的引用，
-- 只调用它暴露的 API，不知道、也不关心 split/trim/join 内部怎么实现。

local string_utils = require("string_utils")

local csv_line = "  alice , bob ,charlie  "
print(string.format("原始字符串: %q", csv_line))

print("\n== split 后的原始片段 ==")
local raw_parts = string_utils.split(csv_line, ",")
for i, p in ipairs(raw_parts) do
    print(string.format("  [%d] %q", i, p))
end

print("\n== 逐个 trim 后 ==")
local trimmed_parts = {}
for i, p in ipairs(raw_parts) do
    trimmed_parts[i] = string_utils.trim(p)
    print(string.format("  [%d] %q", i, trimmed_parts[i]))
end

print("\n== join 结果 ==")
local joined = string_utils.join(trimmed_parts, " | ")
print("  " .. joined)
```

## 6. Execution

```bash
cd code/01-string-utils
lua5.4 app.lua
```

实际执行输出（已用本地 Lua 5.4 验证）：

```text
原始字符串: "  alice , bob ,charlie  "

== split 后的原始片段 ==
  [1] "  alice "
  [2] " bob "
  [3] "charlie  "

== 逐个 trim 后 ==
  [1] "alice"
  [2] "bob"
  [3] "charlie"

== join 结果 ==
  alice | bob | charlie
```

## 7. Execution Flow

1. `require("string_utils")` 定位并执行 `string_utils.lua`，得到 table `M`，缓存进 `package.loaded`；
2. `string_utils.split(csv_line, ",")` 被调用，在 `Module` 内部循环查找 `,` 的位置，逐段 `table.insert` 进结果数组，返回 `{"  alice ", " bob ", "charlie  "}`；
3. `app.lua` 遍历这个数组，对每一项调用 `string_utils.trim(p)`，得到去掉首尾空白的版本；
4. `string_utils.join(trimmed_parts, " | ")` 把清理后的数组用 `table.concat` 拼接成一行字符串；
5. 每一步的中间结果都被打印出来，控制权全程留在 `app.lua`（Application）手里，`string_utils` 内部函数只是被动地"调用一次、返回一次"。

## 8. State

无。`string_utils.lua` 里没有任何 `local` 模块级变量用于跨调用累积数据，`split`/`trim`/`join` 三个函数都是纯函数：相同输入永远得到相同输出，多次调用之间互不影响，也不会因为调用顺序不同而产生不同结果。

## 9. Why this Pattern fits

- 三个函数之间没有共享的、需要隐藏的实现细节——不需要 Private+Public 那种"内部步骤不能被绕过"的封装边界；
- 不需要维护任何跨调用的状态——不需要 Class/Constructor 那种"每个实例独立状态"的能力；
- 只是需要一个方便整体引用、不污染全局命名空间的容器——这正是 Table Module 解决的核心问题。

## 10. What would go wrong with another Pattern

- **换成 Class/Constructor**：会变成 `local su = StringUtils.new()`，然后 `su:split(...)`。但 `split`/`trim`/`join` 根本不需要"实例"这个概念——它们不依赖任何实例状态，每次调用都要先构造一个毫无意义的对象，纯属过度设计，还会让调用方误以为这里存在某种需要维护的状态。
- **换成 Factory**：Factory 解决的是"选择具体创建哪个实现"的问题，而这里从始至终只有一种实现（一种切分字符串的方式），不存在"选择"的需求，引入 Factory 只会多一层没有意义的间接调用。
- **换成 Plugin**：Plugin 解决的是"允许外部扩展、Framework 不需要预先知道有哪些实现"的问题。`split`/`trim`/`join` 是项目内部固定的基础设施，不存在"第三方注册一个新的字符串处理算法"这种扩展场景，套用 Plugin 纯属杀鸡用牛刀。

## 11. Mental Model

> `string_utils` 就是一份"字符串工具箱"：打开箱子（`require`）看到的是固定的几件工具（`split`/`trim`/`join`），工具之间互不干扰，用完放回去也不会留下任何痕迹。
