# 01 · Example 1：JSON-like Parser

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。代码位于 [`code/01-json-parser/`](code/01-json-parser/)。

## 1. Problem / Scenario

需要一个函数,把一段 JSON 风格的字符串（对象、字符串、数字、布尔、`null`、数组）解析成 Lua 表。实现这件事天然需要好几个步骤：先把字符串切成 token,再根据当前 token 的类型决定"这一段是个对象还是一个值",遇到对象要递归处理它的每一个字段。调用者根本不关心这些步骤——调用者只想要"给我一个字符串,还我解析好的数据"这一件事。

## 2. Architecture

```text
                     app.lua (Caller)
                            |
                            | require("parser")
                            v
                    +------------------+
                    |   parser.lua     |
                    |                  |
                    |  Public API      |
                    |  M.parse()       |
                    |  M.validate()    |
                    |       |          |
                    |       v          |
                    |  Private Impl    |
                    |  tokenize()      |
                    |  parse_value()   |
                    |  parse_object()  |
                    |  parse_array()   |
                    +------------------+
```

`app.lua` 只 `require` 到 `parser.lua` 返回的 `M`,`M` 上只有 `parse`/`validate` 两个字段——`tokenize`/`parse_value`/`parse_object`/`parse_array` 全部是 `local function`,从语法上就不存在于 `M` 上。

## 3. Framework / Caller

`app.lua` 扮演 Caller：构造一段样例 JSON 字符串,调用 `parser.parse()` 拿到 Lua 表并用一个手写的 pretty-printer 打印出来,再调用 `parser.validate()` 分别验证一份合法输入和一份损坏输入。Caller 完全不知道内部用的是"先分词再递归下降"这种实现方式,如果作者哪天把分词逻辑整个换掉,`app.lua` 不需要改一行。

## 4. Module

`parser.lua` 内部的可见性划分：

```text
Public API（挂在 M 上,require 之后可见）
    M.parse(str)      -- 唯一的解析入口
    M.validate(str)   -- 建立在 M.parse 之上,复用它而不是绕开它

Private Implementation（local function,require 之后不可见）
    tokenize(str)              -- 字符串 -> token 数组
    parse_value(tokens, pos)   -- 分派器：根据当前 token 决定怎么解析
    parse_object(tokens, pos)  -- 解析一个 { ... }
    parse_array(tokens, pos)   -- 解析一个 [ ... ]（数组支持,额外的私有步骤）
```

`parse_value`/`parse_object`/`parse_array` 三者互相递归调用（对象的字段值可能又是一个对象或数组）,这种递归结构本身就是"只对内部有意义、对外部毫无意义"的实现细节——调用者需要知道的只是最终结果,不需要知道递归是怎么发生的。

## 5. Complete Lua Code

`code/01-json-parser/parser.lua`：

```lua
--
-- parser.lua
--
-- Pattern 2: Private + Public Module (Encapsulation)
--
-- Public API : parse(str), validate(str)
-- Private impl: tokenize(str), parse_value(tokens, pos), parse_object(tokens, pos),
--               parse_array(tokens, pos)   (array support is a small extra private helper)
--
-- Callers only ever see M.parse / M.validate. The tokenizer and the
-- recursive-descent helpers are `local function`, closed over by this file's
-- upvalues, and never reachable from outside -- this is real encapsulation,
-- not a naming convention.
--

local M = {}

--------------------------------------------------------------------------
-- Private Implementation
--------------------------------------------------------------------------

-- tokenize: turn a raw JSON-like string into a flat array of tokens.
-- Token shape: { type = "STRING"|"NUMBER"|"BOOL"|"NULL"|"{"|"}"|"["|"]"|":"|",",
--                value = <lua value, only meaningful for STRING/NUMBER/BOOL> }
local function tokenize(str)
    local tokens = {}
    local i = 1
    local n = #str

    local function skip_ws()
        while i <= n do
            local c = str:sub(i, i)
            if c == " " or c == "\t" or c == "\n" or c == "\r" then
                i = i + 1
            else
                break
            end
        end
    end

    while true do
        skip_ws()
        if i > n then
            break
        end

        local c = str:sub(i, i)

        if c == "{" or c == "}" or c == "[" or c == "]" or c == ":" or c == "," then
            tokens[#tokens + 1] = { type = c }
            i = i + 1

        elseif c == "\"" then
            local j = i + 1
            local buf = {}
            local closed = false
            while j <= n do
                local cj = str:sub(j, j)
                if cj == "\"" then
                    closed = true
                    break
                elseif cj == "\\" and j < n then
                    -- Minimal escape handling: \" \\ \n \t
                    local nextc = str:sub(j + 1, j + 1)
                    if nextc == "n" then
                        buf[#buf + 1] = "\n"
                    elseif nextc == "t" then
                        buf[#buf + 1] = "\t"
                    else
                        buf[#buf + 1] = nextc
                    end
                    j = j + 2
                else
                    buf[#buf + 1] = cj
                    j = j + 1
                end
            end
            if not closed then
                error("tokenize: unterminated string starting at position " .. i)
            end
            tokens[#tokens + 1] = { type = "STRING", value = table.concat(buf) }
            i = j + 1

        elseif c:match("[%-%d]") then
            local j = i
            while j <= n and str:sub(j, j):match("[%-%d%.eE%+]") do
                j = j + 1
            end
            local numstr = str:sub(i, j - 1)
            local num = tonumber(numstr)
            if not num then
                error("tokenize: invalid number literal '" .. numstr .. "'")
            end
            tokens[#tokens + 1] = { type = "NUMBER", value = num }
            i = j

        elseif str:sub(i, i + 3) == "true" then
            tokens[#tokens + 1] = { type = "BOOL", value = true }
            i = i + 4

        elseif str:sub(i, i + 4) == "false" then
            tokens[#tokens + 1] = { type = "BOOL", value = false }
            i = i + 5

        elseif str:sub(i, i + 3) == "null" then
            tokens[#tokens + 1] = { type = "NULL" }
            i = i + 4

        else
            error("tokenize: unexpected character '" .. c .. "' at position " .. i)
        end
    end

    return tokens
end

-- Forward declaration: parse_value and parse_object/parse_array are mutually
-- recursive (an object's value can itself be an object/array, and vice versa).
local parse_value
local parse_object
local parse_array

-- parse_object: assumes tokens[pos].type == "{" (checked by parse_value,
-- the only caller). Private helpers may assume already-validated input --
-- validating "is this really an object" is parse_value's job, not this
-- function's job.
parse_object = function(tokens, pos)
    local obj = {}
    pos = pos + 1 -- consume "{"

    if tokens[pos] and tokens[pos].type == "}" then
        return obj, pos + 1
    end

    while true do
        local key_tok = tokens[pos]
        if not key_tok or key_tok.type ~= "STRING" then
            error("parse_object: expected string key at token " .. tostring(pos))
        end
        local key = key_tok.value
        pos = pos + 1

        local colon_tok = tokens[pos]
        if not colon_tok or colon_tok.type ~= ":" then
            error("parse_object: expected ':' after key '" .. key .. "'")
        end
        pos = pos + 1

        local value, next_pos = parse_value(tokens, pos)
        obj[key] = value
        pos = next_pos

        local sep = tokens[pos]
        if sep and sep.type == "," then
            pos = pos + 1
        elseif sep and sep.type == "}" then
            pos = pos + 1
            break
        else
            error("parse_object: expected ',' or '}' at token " .. tostring(pos))
        end
    end

    return obj, pos
end

-- parse_array: same trust contract as parse_object -- only ever called by
-- parse_value after it has already confirmed tokens[pos].type == "[".
parse_array = function(tokens, pos)
    local arr = {}
    pos = pos + 1 -- consume "["

    if tokens[pos] and tokens[pos].type == "]" then
        return arr, pos + 1
    end

    while true do
        local value, next_pos = parse_value(tokens, pos)
        arr[#arr + 1] = value
        pos = next_pos

        local sep = tokens[pos]
        if sep and sep.type == "," then
            pos = pos + 1
        elseif sep and sep.type == "]" then
            pos = pos + 1
            break
        else
            error("parse_array: expected ',' or ']' at token " .. tostring(pos))
        end
    end

    return arr, pos
end

-- parse_value: the dispatcher. This is the only function allowed to look at
-- a token and decide "what kind of value is this" -- parse_object/parse_array
-- never re-check the token type they were dispatched on.
parse_value = function(tokens, pos)
    local tok = tokens[pos]
    if not tok then
        error("parse_value: unexpected end of input")
    end

    if tok.type == "{" then
        return parse_object(tokens, pos)
    elseif tok.type == "[" then
        return parse_array(tokens, pos)
    elseif tok.type == "STRING" or tok.type == "NUMBER" or tok.type == "BOOL" then
        return tok.value, pos + 1
    elseif tok.type == "NULL" then
        return nil, pos + 1
    else
        error("parse_value: unexpected token '" .. tostring(tok.type) .. "'")
    end
end

--------------------------------------------------------------------------
-- Public API
--------------------------------------------------------------------------

-- M.parse: the module's only entry point for turning a string into data.
-- Responsible for input validation (is `str` even a string?) before handing
-- off to the private helpers, which assume that contract is already met.
function M.parse(str)
    if type(str) ~= "string" then
        return nil, "parse: expected a string, got " .. type(str)
    end

    local ok, tokens_or_err = pcall(tokenize, str)
    if not ok then
        return nil, tokens_or_err
    end
    local tokens = tokens_or_err

    if #tokens == 0 then
        return nil, "parse: empty input"
    end

    local ok2, value_or_err, pos_or_nil = pcall(parse_value, tokens, 1)
    if not ok2 then
        return nil, value_or_err
    end

    local value, pos = value_or_err, pos_or_nil
    if pos <= #tokens then
        return nil, "parse: trailing tokens after position " .. pos
    end

    return value
end

-- M.validate: built on top of M.parse, not on the private helpers directly.
-- Even the module's own public functions prefer to call other public
-- functions when possible -- this is what keeps the private surface free
-- to change.
function M.validate(str)
    local value, err = M.parse(str)
    if err then
        return false, err
    end
    return true
end

return M
```

`code/01-json-parser/app.lua`：

```lua
--
-- app.lua
--
-- Framework/Caller role: only ever touches parser.parse / parser.validate.
-- Never touches parser.tokenize / parser.parse_value / parser.parse_object
-- because those names simply do not exist on the returned table M.
--

local parser = require("parser")

-- A small manual pretty-printer -- no external library needed. This lives
-- in app.lua (the caller), not in parser.lua, because "how to print a
-- result" is not the module's concern.
local function pretty(value, indent)
    indent = indent or ""
    local t = type(value)

    if t == "table" then
        -- Distinguish "array-ish" (1..n integer keys) from "object-ish".
        local is_array = true
        local n = 0
        for k in pairs(value) do
            n = n + 1
            if type(k) ~= "number" then
                is_array = false
            end
        end
        if is_array and n == #value then
            if n == 0 then
                io.write("[]")
                return
            end
            io.write("[\n")
            for i, v in ipairs(value) do
                io.write(indent .. "  ")
                pretty(v, indent .. "  ")
                if i < n then io.write(",") end
                io.write("\n")
            end
            io.write(indent .. "]")
        else
            local keys = {}
            for k in pairs(value) do keys[#keys + 1] = k end
            table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
            if #keys == 0 then
                io.write("{}")
                return
            end
            io.write("{\n")
            for idx, k in ipairs(keys) do
                io.write(indent .. "  " .. tostring(k) .. " = ")
                pretty(value[k], indent .. "  ")
                if idx < #keys then io.write(",") end
                io.write("\n")
            end
            io.write(indent .. "}")
        end
    elseif t == "string" then
        io.write(string.format("%q", value))
    elseif t == "nil" then
        io.write("null")
    else
        io.write(tostring(value))
    end
end

local sample = [[
{
    "name": "sensor-01",
    "active": true,
    "reading": -12.5,
    "tags": ["temp", "outdoor"],
    "calibration": null,
    "meta": {
        "vendor": "Acme",
        "version": 3
    }
}
]]

print("== parsing sample JSON ==")
local value, err = parser.parse(sample)
if not value then
    print("parse failed: " .. tostring(err))
    os.exit(1)
end

pretty(value)
print()

print()
print("== validate() checks ==")
print("valid sample     ->", parser.validate(sample))
print("valid broken json ->", parser.validate("{ \"a\": }"))

-- Only M.parse / M.validate exist on the returned table. Uncommenting the
-- line below would fail with "attempt to call a nil value (field
-- 'tokenize')" -- proof that the private helpers are not part of the
-- module's public surface:
--
-- parser.tokenize(sample)
```

## 6. Execution Flow

`lua5.4 app.lua` 的实际输出：

```text
== parsing sample JSON ==
{
  active = true,
  meta = {
    vendor = "Acme",
    version = 3
  },
  name = "sensor-01",
  reading = -12.5,
  tags = [
    "temp",
    "outdoor"
  ]
}

== validate() checks ==
valid sample     ->	true
valid broken json ->	false	./parser.lua:214: parse_value: unexpected token '}'
```

流程图：

```text
app.lua
  |
  |  parser.parse(sample)
  v
M.parse
  |
  |  校验 str 是字符串
  |  调用 tokenize(str)
  v
tokenize                    <- Private, app.lua 看不到这一步
  |
  |  返回 token 数组
  v
M.parse
  |
  |  调用 parse_value(tokens, 1)
  v
parse_value                 <- Private, 分派器
  |
  |  遇到 "{" -> 递归调用 parse_object
  v
parse_object  <----+         <- Private, 内部又会递归调用 parse_value
  |                |         (对象字段值本身可能又是对象/数组)
  +----------------+
  |
  |  返回完整的 Lua 表
  v
M.parse
  |
  |  返回给 app.lua
  v
app.lua
```

`calibration` 字段的值是 `null`,`M.parse` 把它转换成 Lua 的 `nil`——这是一个值得留意的小细节：Lua 表里 `t[k] = nil` 等价于"删掉这个 key",所以打印出来的表里看不到 `calibration` 这个字段,这是 Lua 语言本身的行为,不是 Parser 的 bug,也不是这个 Pattern 要解决的问题。

## 7. State

这个 Parser **没有 Module 级 State**——`M.parse` 每次调用都会创建一个全新的 `tokens` 数组、一个全新的结果表,调用结束之后这些数据就不再被任何东西引用。两次不同的 `parser.parse(a)` 和 `parser.parse(b)` 之间没有任何共享的可变数据。这是一个提醒：**Pattern 2 的 State 是可选的**,这个 Example 展示的是"不需要 State"的那一半;Example 2（User Service）会展示需要 State 的那一半。

## 8. Why this Pattern fits

Parser 天然需要多个互相配合的内部步骤（分词、分派、递归解析对象/数组）,这些步骤之间的协作方式（token 数组长什么样、递归怎么传递 `pos`）是纯粹的实现细节,和"调用者应该怎么用这个 Parser"完全无关。Pattern 2 恰好提供了"多步骤协作、但只暴露一个入口"的形状——`M.parse`/`M.validate` 两个函数就是全部的对外契约,内部四个 `local function` 可以按需要拆分、合并、改写调用顺序,只要 `M.parse`/`M.validate` 的行为不变,这个 Module 对外就是稳定的。

## 9. What would go wrong with another Pattern

- **换成 Table Module（把 `tokenize`/`parse_value`/`parse_object` 全部挂到 `M` 上)**：这正是 [00-why-and-architecture.md](00-why-and-architecture.md) 里 Anti-pattern 一节演示的情况——分词器会被泄漏给调用者,一旦有人开始直接调用 `M.tokenize`,作者就失去了更换分词策略的自由。
- **换成 Class / Constructor Module（给每次解析创建一个 Parser 对象)**：这个场景完全不需要"每个调用者拥有一份独立、互相隔离的状态"——`parse(str)` 是一个纯函数式的操作,输入一个字符串,输出一个结果,两次调用之间没有任何东西需要被"记住"。强行引入对象和构造函数只会多出一个没有实际用途的 `new_parser()` 步骤,是不必要的复杂度。

## 10. Mental Model

**一句话**：调用者只需要知道"给我字符串,还我数据",不需要知道"怎么分词、怎么递归"。

**核心问题**：解析这件事需要几步完成？这几步之间的协作方式,是不是调用者不应该关心的实现细节？

**核心关系**：

```text
M.parse / M.validate
        |
        v
tokenize -> parse_value -> parse_object / parse_array
```
