# 01-2 · 示例：HTTP Constants / API

> Pattern：Table Module（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/02-http-constants/](code/02-http-constants/)（`http.lua` / `app.lua`）。

## 1. Problem / Scenario

任何和 HTTP 打交道的代码，都需要一批"协议本身规定好的、不会变的知识"：方法名字符串（`"GET"`、`"POST"`）、状态码到状态短语的映射（`200 -> "OK"`）、header 行的拼接格式。这些知识：

- 是协议规范定义的常量，不是某次请求特有的数据；
- 会被项目里请求构造、响应处理、日志打印等很多地方反复用到；
- 配套少量围绕这些常量的工具函数（拼 header、查状态短语、判断是否 2xx 成功），但这些函数本身也不依赖任何请求相关的运行时状态。

我们想要一个单一的地方，把"HTTP 协议知识"整体收纳起来，调用方 `require` 一次就能拿到全部方法常量、状态码表和配套工具函数。

## 2. Architecture

```text
Application (app.lua)
    |
    v
+---------------------------+
|      Module: http         |
|                            |
|   M.METHODS (table)       |
|   M.STATUS  (table)       |
|   M.build_header(n, v)    |
|   M.status_text(code)     |
|   M.is_success(code)      |
+---------------------------+
```

`http.lua` 同时装了两类东西：纯数据（`METHODS`、`STATUS` 两张常量表）和纯函数（`build_header`/`status_text`/`is_success`）——这正好对应 `Module -> Namespace -> {Function, Function, Constant, Type}` 这条心智模型链里 `Constant` 和 `Function` 两个分支同时出现的情况。

## 3. Framework / Caller 代码

```lua
local http = require("http")
```

`app.lua` 依次演示三类用法：遍历方法常量表、用 `build_header` 拼出几行请求头、用 `status_text`/`is_success` 检查一批状态码。

## 4. Module 代码

```lua
local M = {}

M.METHODS = {
    GET = "GET", POST = "POST", PUT = "PUT", DELETE = "DELETE", PATCH = "PATCH",
}

M.STATUS = {
    [200] = "OK", [201] = "Created", [204] = "No Content",
    [301] = "Moved Permanently", [400] = "Bad Request",
    [401] = "Unauthorized", [404] = "Not Found", [500] = "Internal Server Error",
}

function M.build_header(name, value)
    return string.format("%s: %s", name, value)
end

function M.status_text(code)
    return M.STATUS[code] or "Unknown Status"
end

function M.is_success(code)
    return code >= 200 and code < 300
end

return M
```

`M.STATUS[code] or "Unknown Status"` 是典型的"表驱动 + 兜底默认值"写法：查不到的状态码不会报错，而是退化成一个明确的"未知"标记，调用方可以安全地打印，不需要提前判断状态码是否存在。

## 5. 完整 Lua 代码

与磁盘上的文件完全一致。

`code/02-http-constants/http.lua`：

```lua
-- http.lua
--
-- Table Module：HTTP 协议相关的常量集合 + 少量配套的纯函数。
-- 对应一个 C header 里 `#define GET "GET"` / `#define STATUS_OK 200`
-- 这类常量定义，外加几个和这些常量搭配使用的工具函数。
-- 不持有任何运行时状态，METHODS/STATUS 都是加载时就固定的只读数据。

local M = {}

--- HTTP 方法常量
M.METHODS = {
    GET = "GET",
    POST = "POST",
    PUT = "PUT",
    DELETE = "DELETE",
    PATCH = "PATCH",
}

--- HTTP 状态码 -> 状态短语
M.STATUS = {
    [200] = "OK",
    [201] = "Created",
    [204] = "No Content",
    [301] = "Moved Permanently",
    [400] = "Bad Request",
    [401] = "Unauthorized",
    [404] = "Not Found",
    [500] = "Internal Server Error",
}

--- build_header(name, value) -> string
-- 拼出一行合法的 HTTP header 文本。
-- 例如 build_header("Content-Type", "application/json")
-- 返回 "Content-Type: application/json"
function M.build_header(name, value)
    return string.format("%s: %s", name, value)
end

--- status_text(code) -> string
-- 查状态码对应的短语，查不到已知状态码时返回 "Unknown Status"。
function M.status_text(code)
    return M.STATUS[code] or "Unknown Status"
end

--- is_success(code) -> boolean
-- 判断状态码是否落在 2xx 区间。
function M.is_success(code)
    return code >= 200 and code < 300
end

return M
```

`code/02-http-constants/app.lua`：

```lua
-- app.lua
--
-- Framework/Application：require("http") 拿到常量 + 工具函数的集合，
-- 直接当作一个只读的"HTTP 协议知识包"来用。

local http = require("http")

print("== HTTP 方法常量 ==")
local method_order = { "GET", "POST", "PUT", "DELETE", "PATCH" }
for _, name in ipairs(method_order) do
    print(string.format("  %s -> %s", name, http.METHODS[name]))
end

print("\n== 构建请求 Header ==")
local headers = {
    http.build_header("Content-Type", "application/json"),
    http.build_header("Authorization", "Bearer token123"),
}
for _, h in ipairs(headers) do
    print("  " .. h)
end

print("\n== 状态码检查 ==")
local codes_to_check = { 200, 201, 301, 404, 500 }
for _, code in ipairs(codes_to_check) do
    print(string.format("  %d %s -> is_success=%s",
        code, http.status_text(code), tostring(http.is_success(code))))
end
```

## 6. Execution

```bash
cd code/02-http-constants
lua5.4 app.lua
```

实际执行输出（已用本地 Lua 5.4 验证）：

```text
== HTTP 方法常量 ==
  GET -> GET
  POST -> POST
  PUT -> PUT
  DELETE -> DELETE
  PATCH -> PATCH

== 构建请求 Header ==
  Content-Type: application/json
  Authorization: Bearer token123

== 状态码检查 ==
  200 OK -> is_success=true
  201 Created -> is_success=true
  301 Moved Permanently -> is_success=false
  404 Not Found -> is_success=false
  500 Internal Server Error -> is_success=false
```

## 7. Execution Flow

1. `require("http")` 执行 `http.lua`：先创建空 table `M`，依次挂上 `METHODS`、`STATUS` 两张常量表，再挂上三个函数，最后 `return M`；这一整套准备工作只在第一次 `require` 时发生一次；
2. `app.lua` 用固定顺序的 `method_order` 数组遍历 `http.METHODS`，逐个打印——这里特意不用 `pairs(http.METHODS)` 直接遍历，因为 table 的哈希部分遍历顺序不保证稳定，用一个显式的顺序数组能让示例输出可复现；
3. 依次调用两次 `http.build_header`，把返回的字符串收集进本地数组再打印；
4. 对每个待检查状态码调用 `http.status_text` 和 `http.is_success`，两个函数各自独立查表/计算，互不依赖对方的返回值。

## 8. State

无。`M.METHODS` 和 `M.STATUS` 虽然是"数据"，但都是加载时一次性写死的**只读**常量表——没有任何函数会在运行期修改它们的内容。调用方也不应该修改它们（例如 `http.METHODS.GET = "get"` 在语法上是合法的，但这样做违反了这个 Module 的设计意图，等同于第 00 篇 Anti-pattern 里描述的"从外部篡改公开字段"）。

## 9. Why this Pattern fits

- `METHODS`/`STATUS` 是协议规范定义的常量，天然就应该是只读、全局共享的一份数据，用单例 table 存放正合适；
- `build_header`/`status_text`/`is_success` 都是围绕这些常量的纯函数，不需要任何实例化步骤；
- 没有任何"内部实现步骤，调用顺序有讲究"的逻辑需要隐藏，不需要引入 Private+Public 那层封装。

## 10. What would go wrong with another Pattern

- **换成 Class/Constructor**：`local h = Http.new(); h:status_text(200)` ——但状态码到短语的映射是协议全局共享的常量，不存在"每个 `Http` 实例应该有自己独立的一份 `STATUS` 表"这种需求，强行实例化只会让每次使用都多一次毫无意义的对象构造。
- **换成 Factory**：Factory 用来隐藏"具体创建哪个实现"，但 HTTP 协议的方法名和状态码表是唯一、确定的知识，不存在多个互相竞争的"实现"需要被 Factory 选择。
- **换成 Plugin**：如果真的需要支持"不同协议版本有不同的状态码表"，那可能是 Factory（选择 HTTP/1.1 或 HTTP/2 的常量集）该出场的场景，而不是 Plugin——这里没有"允许第三方注册自定义状态码"的扩展需求，Plugin 依然是不匹配的。

## 11. Mental Model

> `http.lua` 就是把 RFC 里那几张表（方法名、状态码）抄成 Lua table，再配几个趁手的小工具函数——它是"协议知识的静态快照"，不是"一次 HTTP 请求"，请求本身的动态数据（URL、body、实际收到的状态码）从来不该存在这里。
