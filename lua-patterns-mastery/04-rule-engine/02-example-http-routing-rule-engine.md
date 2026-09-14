# 02 · 示例：HTTP Routing Rule Engine（纯 Lua 规则表 + 自带 driver）

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)、[01-example-firewall-rule-engine.md](01-example-firewall-rule-engine.md)。本示例展示 Rule Engine 加入 **Priority** 和 **附加 Condition** 后的更完整形态，并对比 First Match 与 Multiple Match 两种语义。完整代码见 [code/02-http-routing-rule-engine/rules.lua](code/02-http-routing-rule-engine/rules.lua)，可以直接用 `lua rules.lua` 跑起来，不需要额外的 C Host。

## 1. Problem

一个 HTTP 反向代理/网关，需要把不同的请求（按路径、方法、Header、查询参数）转发到不同的后端（backend）。路由规则天然是"一组结构统一的条目"：每条规则描述"什么样的请求应该走哪个后端"，规则之间有优先级（更具体的路径应该比更宽泛的路径先匹配），还可能带附加条件（例如"只有带管理员 Token 的请求才能走管理后端，否则退回普通只读接口"）。如果把这些路由逻辑写成一长串嵌套 `if/else`，新增一条路由规则、调整优先级都会牵动一大段已有逻辑。

## 2. Why Lua

路由规则的核心诉求和 Firewall 示例一样是 `Input -> Predicate -> Match -> Action`，但多了两个真实网关都需要的维度：**Priority**（同样匹配路径前缀的多条规则，谁先谁后）和**附加 Condition**（除了路径/方法之外，还要看 Header、查询参数）。这两者用 Lua 表达都很自然：

- Priority 就是规则 table 里的一个普通数字字段，排序用标准库 `table.sort` 一行解决；
- Condition 是另一个可选的 Lua 函数字段，逻辑上和 `matcher` 是同一种东西（都是"给定 `req`，返回布尔值"），只是语义上区分"路径层面的匹配"和"附加的业务条件"；
- 因为 Lua 是被 OpenResty/Nginx 原生支持的嵌入语言，这份规则表可以不经改动直接搬进 `access_by_lua`，这是选择 Lua 而不是自造 DSL 的现实原因（详见第 13 节）。

## 3. Architecture

```text
Request
 |
 v
Lua rules
 |
 v
/api/*
 |
 v
backend-A

/admin/*
 |
 v
backend-B
```

更完整的内部结构：

```text
                    req { method, path, headers, query }
                              |
                              v
                     按 priority 从高到低排序
                              |
              +---------------+---------------+
              |               |               |
     admin-api(100)     api-read/write(50)  admin-ui(40)
              |               |               |
     matcher + condition   matcher only     matcher only
              |               |               |
              +---------------+---------------+
                              v
                        First Match 命中
                              |
                              v
                        backend-X（转发目标）
```

## 4. Host responsibilities

本示例故意不绑定具体 Host（不强制配 C 代码），因为路由规则表这一层在真实系统里几乎总是运行在别人已经写好的 Host 里（Nginx worker 进程）。但"Host 职责"依然清晰存在，只是换成了 Nginx/OpenResty 承担：

- **Mechanism**：真正建立 upstream 连接、转发请求字节流、处理超时重试——这些永远是 Nginx C 核心的事，Lua 从不接触 socket。
- **Resource**：upstream 连接池、keepalive 连接，由 Nginx 管理。
- **Lifecycle**：`access_by_lua`/`content_by_lua` 在每个请求到达时被调用一次，规则表本身随 worker 进程启动时被 `require` 加载一次，常驻内存直到 worker 重启/reload。
- **Stable API**：Nginx 暴露给 Lua 的 `ngx.var`、`ngx.req.get_headers()` 等 API，这些是 OpenResty 对 Lua 的稳定契约（本示例用一个手工构造的 `req` table 模拟这些 API 的返回结果，重点在规则匹配逻辑本身，不在 OpenResty API 的细节）。

## 5. Lua responsibilities

- `routes` 表：Rules 职责的核心，每条规则是 `{name, priority, matcher, condition, backend}`。
- `rule_matches`/`match_first`/`match_all`：规则匹配的调度逻辑，决定"如何遍历、如何组合命中结果"。
- 本示例额外加了一段可以独立运行的 `run_demo()` driver，纯粹用来在没有真实 Nginx 环境时演示匹配过程，不是路由 Pattern 本身的必要部分。

## 6. Lifecycle

```text
lua rules.lua 被执行（或被 require("rules") 引入）
  |
  v
定义 routes 表（模块加载时执行一次）
  |
  v
定义 rule_matches / match_first / match_all（函数体只是定义，不执行）
  |
  v
run_demo() 被调用：构造模拟请求，逐个跑 match_first / match_all
  |
  v
脚本正常结束（纯 Lua 解释器进程退出）；
若接入 Nginx，则 routes 表常驻 worker 进程内存，
match_first 会在每个请求到达时被 access_by_lua 反复调用
```

## 7. Data Flow（C -> Lua）

本示例没有 C Host，所以严格意义上没有"C -> Lua"的转换代码，但等价的数据流仍然存在：`run_demo()` 里手工构造的 `demo_requests` table 就是在模拟"Host（Nginx）把一个真实 HTTP 请求转换成 Lua table"这一步。如果接入真实 Nginx，这一步会变成：

```lua
local req = {
    method  = ngx.req.get_method(),
    path    = ngx.var.uri,
    headers = ngx.req.get_headers(),
    query   = ngx.req.get_uri_args(),
}
```

字段形状（`method`/`path`/`headers`/`query`）与本示例规则表期望的完全一致，这不是巧合——本示例的 `req` table 设计本身就是照着 OpenResty 真实 API 的返回形状来的。

## 8. Control Flow（Lua -> C）

同样地，本示例没有真实的"Lua 调用 C"环节。等价的位置在真实 Nginx 里是：`match_first(req)` 返回命中规则的 `backend` 字段后，Lua 侧要调用 `ngx.exec()` / 设置 `ngx.var.backend` 变量交给 `proxy_pass` 决定真正转发到哪——这一步是 Lua 主动调用 Nginx 暴露的 Stable API，把"决策结果"交还给 C 核心去执行真正的网络转发。

## 9. State

- `routes` 是模块级的 `local` 变量，在真实 Nginx 部署里，这份表在每个 worker 进程里只在启动时构造一次，之后所有请求共享同一份只读规则表，不会有并发写冲突（前提是规则表本身在运行期不被修改）。
- `match_first`/`match_all` 每次调用都会新建一个 `sorted`/`hits` 临时数组，调用结束后即可被 GC 回收，不会跨请求残留状态。
- 每条规则的 `matcher`/`condition` 同样是无状态纯函数，只读 `req` 参数。

## 10. Failure

- 本示例没有引入 `pcall`，因为它是一个独立可运行的演示脚本，不是被 C Host 通过 `lua_pcall` 调用的场景；如果接入真实 Nginx，`access_by_lua` 内部的调用本身已经处于 OpenResty 的错误捕获机制之下，规则脚本里的运行时错误会被转换成 500 错误并记录到 Nginx error log，不会导致 worker 进程崩溃。
- 规则设计层面的"失败"更值得关注：如果两条规则 `priority` 相同又同时匹配同一个请求，`table.sort` 的排序结果在 Lua 里对相同比较结果的元素不保证稳定顺序——这是一个真实的坑，规则设计时应该避免让优先级相同的规则同时匹配交叠的路径。
- `default-fallback`（`priority=0`，`matcher` 恒为 `true`）保证任何请求都有一个确定的兜底后端，避免"没有规则命中，Nginx 不知道转发到哪"的情况。

## 11. Complete Host Code

本示例不提供独立的 C Host（详见第 4 节的说明），Host 侧职责由 Nginx/OpenResty 承担，其真实调用方式已在第 7/8 节给出等价代码片段。

## 12. Complete Lua Code

`code/02-http-routing-rule-engine/rules.lua`：

```lua
-- rules.lua
-- HTTP Routing Rule Engine
--
-- 这份文件不需要独立的 C Host：既是"规则表定义"，也是一份可以直接用
-- `lua rules.lua` 跑起来的自描述 driver（文件末尾的 `if not ...` 部分）。
-- 如果接入真实的 OpenResty/Nginx，规则表和 match_request/match_all 这两个
-- 函数会被搬进 access_by_lua_by_lua_file（或 content_by_lua_file），
-- driver 部分则替换成 Nginx 传进来的真实请求上下文，具体见本文件末尾的说明
-- 以及 02-example-http-routing-rule-engine.md 的第 13 节。
--
-- 每条规则统一长成：
--   {
--     name      = "<规则名>",
--     priority  = <number>,        -- 数字越大越先匹配
--     matcher   = function(req) ... end,   -- 路径前缀 + 方法匹配
--     condition = function(req) ... end,   -- 可选的附加条件（Header/查询参数）
--     backend   = "<转发目标>",
--   }

local routes = {
    {
        name = "admin-api",
        priority = 100,
        -- 更具体的路径要给更高的 priority，否则会被更宽泛的规则先截走
        matcher = function(req)
            return req.method == "GET" and req.path:match("^/api/admin/") ~= nil
        end,
        condition = function(req)
            return req.headers["X-Admin-Token"] ~= nil
        end,
        backend = "backend-admin"
    },
    {
        name = "api-read",
        priority = 50,
        matcher = function(req)
            return req.method == "GET" and req.path:match("^/api/") ~= nil
        end,
        condition = nil, -- 没有附加条件，matcher 命中即算命中
        backend = "backend-A"
    },
    {
        name = "api-write",
        priority = 50,
        matcher = function(req)
            return (req.method == "POST" or req.method == "PUT")
                and req.path:match("^/api/") ~= nil
        end,
        condition = function(req)
            -- 附加条件示例：要求请求带 Content-Type: application/json
            return req.headers["Content-Type"] == "application/json"
        end,
        backend = "backend-A"
    },
    {
        name = "admin-ui",
        priority = 40,
        matcher = function(req)
            return req.path:match("^/admin/") ~= nil
        end,
        condition = nil,
        backend = "backend-B"
    },
    {
        name = "static-assets",
        priority = 10,
        -- 附加条件示例：查询参数 ?debug=1 时改走调试后端
        matcher = function(req)
            return req.path:match("^/static/") ~= nil
        end,
        condition = function(req)
            return req.query and req.query.debug ~= "1"
        end,
        backend = "backend-static"
    },
    {
        name = "static-assets-debug",
        priority = 20, -- 比 static-assets 优先级更高，debug=1 时优先命中
        matcher = function(req)
            return req.path:match("^/static/") ~= nil
        end,
        condition = function(req)
            return req.query and req.query.debug == "1"
        end,
        backend = "backend-static-debug"
    },
    {
        name = "default-fallback",
        priority = 0, -- 最低优先级，兜底
        matcher = function(_)
            return true
        end,
        condition = nil,
        backend = "backend-default"
    }
}

--- 判断单条规则 route 是否命中请求 req：matcher 和 condition（如果存在）都要成立
local function rule_matches(route, req)
    if not route.matcher(req) then
        return false
    end
    if route.condition and not route.condition(req) then
        return false
    end
    return true
end

--[[
 match_first(req) —— First Match 语义
 路由场景通常用 First Match：按 priority 从高到低排序后，第一条命中的规则
 就决定最终转发目标，找到即返回，不再看后面的规则。
 返回命中的规则（table），若无命中返回 nil。
--]]
local function match_first(req)
    local sorted = {}
    for _, r in ipairs(routes) do
        table.insert(sorted, r)
    end
    table.sort(sorted, function(a, b) return a.priority > b.priority end)

    for _, route in ipairs(sorted) do
        if rule_matches(route, req) then
            return route
        end
    end
    return nil
end

--[[
 match_all(req) —— Multiple Match 语义
 如果需要"收集所有命中的规则，再按优先级合并"（例如同时要做路由转发 +
 附加一个日志规则 + 附加一个限流规则，三者互不冲突，都应该生效），
 就不能在第一条命中后提前退出，而是把所有命中的规则按 priority
 从高到低收集进一个数组返回，由调用方决定如何合并（取第一个当作转发目标，
 其余当作附加动作）。
--]]
local function match_all(req)
    local hits = {}
    for _, r in ipairs(routes) do
        if rule_matches(r, req) then
            table.insert(hits, r)
        end
    end
    table.sort(hits, function(a, b) return a.priority > b.priority end)
    return hits
end

-- 导出给其他模块 require 使用（例如接入真实 Nginx 时）
local M = {
    routes = routes,
    match_first = match_first,
    match_all = match_all,
}

--------------------------------------------------------------------------
-- Driver：可以直接用 `lua rules.lua` 跑一遍，演示匹配过程
--
-- 简化点：真实项目里通常会把 routes 表和 match_first/match_all 单独放进
-- 一个模块文件（例如 routes.lua），用 `local routes = require("routes")`
-- 引入到 Nginx 的 access_by_lua 阶段，driver 部分只在独立跑的时候存在。
-- 这里为了满足"一份文件里同时给出规则表和可运行 driver"的要求，两者写在
-- 同一个文件里；如果这个文件被 `require("rules")` 引入而不是直接执行，
-- run_demo() 仍然会被执行一次并打印演示输出，这是本文件的已知简化点。
--------------------------------------------------------------------------

local function print_request(req)
    io.write(string.format("Request: %s %s", req.method, req.path))
    if req.query and next(req.query) then
        io.write("?")
        local first = true
        for k, v in pairs(req.query) do
            if not first then io.write("&") end
            io.write(k .. "=" .. v)
            first = false
        end
    end
    io.write("\n")
end

local function run_demo()
    -- 构造几个模拟 Request 表
    local demo_requests = {
        {
            method = "GET",
            path = "/api/admin/users",
            headers = { ["X-Admin-Token"] = "secret123" },
            query = {},
        },
        {
            method = "GET",
            path = "/api/admin/users",
            headers = {}, -- 没带 admin token，会被 admin-api 拒绝，退回 api-read
            query = {},
        },
        {
            method = "POST",
            path = "/api/orders",
            headers = { ["Content-Type"] = "application/json" },
            query = {},
        },
        {
            method = "GET",
            path = "/admin/dashboard",
            headers = {},
            query = {},
        },
        {
            method = "GET",
            path = "/static/app.js",
            headers = {},
            query = {},
        },
        {
            method = "GET",
            path = "/static/app.js",
            headers = {},
            query = { debug = "1" },
        },
        {
            method = "GET",
            path = "/healthz",
            headers = {},
            query = {},
        },
    }

    print("=== HTTP Routing Rule Engine：First Match 演示 ===\n")
    for i, req in ipairs(demo_requests) do
        io.write(string.format("[%d] ", i))
        print_request(req)
        local route = match_first(req)
        if route then
            print(string.format("    -> 命中规则 %-20s priority=%-4d backend=%s\n",
                route.name, route.priority, route.backend))
        else
            print("    -> 无规则命中\n")
        end
    end

    print("=== 同一批请求用 Multiple Match 重新跑一遍（对比） ===\n")
    -- 只挑第 5/6 个请求演示：static-assets 和 static-assets-debug
    -- 在 First Match 下互斥，但如果规则设计成允许叠加（例如都命中时
    -- 都要生效），Multiple Match 可以把两条都收集出来。
    for i = 5, 6 do
        local req = demo_requests[i]
        io.write(string.format("[%d] ", i))
        print_request(req)
        local hits = match_all(req)
        io.write("    -> 命中 " .. #hits .. " 条规则: ")
        for _, r in ipairs(hits) do
            io.write(r.name .. "(" .. r.backend .. ") ")
        end
        io.write("\n\n")
    end
end

run_demo()

return M
```

## 13. Execution

直接用标准 Lua 解释器跑（不需要 Makefile/C 编译）：

```bash
cd code/02-http-routing-rule-engine
lua rules.lua
```

实际运行输出：

```text
=== HTTP Routing Rule Engine：First Match 演示 ===

[1] Request: GET /api/admin/users
    -> 命中规则 admin-api            priority=100  backend=backend-admin

[2] Request: GET /api/admin/users
    -> 命中规则 api-read             priority=50   backend=backend-A

[3] Request: POST /api/orders
    -> 命中规则 api-write            priority=50   backend=backend-A

[4] Request: GET /admin/dashboard
    -> 命中规则 admin-ui             priority=40   backend=backend-B

[5] Request: GET /static/app.js
    -> 命中规则 static-assets        priority=10   backend=backend-static

[6] Request: GET /static/app.js?debug=1
    -> 命中规则 static-assets-debug  priority=20   backend=backend-static-debug

[7] Request: GET /healthz
    -> 命中规则 default-fallback     priority=0    backend=backend-default

=== 同一批请求用 Multiple Match 重新跑一遍（对比） ===

[5] Request: GET /static/app.js
    -> 命中 2 条规则: static-assets(backend-static) default-fallback(backend-default)

[6] Request: GET /static/app.js?debug=1
    -> 命中 2 条规则: static-assets-debug(backend-static-debug) default-fallback(backend-default)
```

**如果接入真实 OpenResty/Nginx，这份规则表应该挂在哪个阶段？** `match_first(req)` 只做"决定转发到哪个 backend"这件事，不涉及请求体内容，属于典型的**访问控制/路由决策**，应该挂在 `access_by_lua*` 阶段（在 `content_by_lua`/`proxy_pass` 之前执行）：

```nginx
location / {
    access_by_lua_block {
        local rules = require("rules")
        local req = {
            method  = ngx.req.get_method(),
            path    = ngx.var.uri,
            headers = ngx.req.get_headers(),
            query   = ngx.req.get_uri_args(),
        }
        local route = rules.match_first(req)
        if not route then
            ngx.exit(404)
        end
        ngx.var.backend = route.backend
    }
    proxy_pass http://$backend;
}
```

如果规则里的 Condition 需要读取请求体（例如按 body 里的某个字段路由），则要挂在 `content_by_lua*` 或者在 `access_by_lua` 里先 `ngx.req.read_body()`，因为 `access_by_lua` 阶段默认请求体还未被读取。

## 14. Step-by-step walkthrough

以请求 #2（`GET /api/admin/users`，`headers = {}`，没有带 `X-Admin-Token`）为例：

1. `match_first(req)` 先把 `routes` 复制一份到 `sorted`，按 `priority` 从高到低排序：`admin-api(100) > api-read(50) = api-write(50) > admin-ui(40) > static-assets-debug(20) > static-assets(10) > default-fallback(0)`。
2. 依次调用 `rule_matches`：第一条是 `admin-api`。`matcher(req)`：`req.method=="GET"` 成立，`req.path:match("^/api/admin/")` 对 `"/api/admin/users"` 匹配成功，`matcher` 返回 `true`。
3. 因为 `admin-api.condition` 存在，继续判断：`req.headers["X-Admin-Token"] ~= nil`。这次 `headers = {}`，取不到该字段，`condition` 返回 `false`。
4. `rule_matches` 因为 `condition` 不成立返回 `false`，`admin-api` 未命中——**即使 matcher 命中了，只要 condition 不满足，这条规则整体也算未命中**，继续看下一条。
5. 下一条（同为 priority=50）是 `api-read`（数组里排在 `api-write` 前面，`table.sort` 对相等的比较结果保持输入顺序不保证，但两条互不冲突不影响结果）。`matcher`：`method=="GET"` 且路径匹配 `^/api/`，成立；`condition` 为 `nil`，直接算命中。
6. `match_first` 返回 `api-read` 规则，`backend = "backend-A"`。
7. `run_demo` 打印：`命中规则 api-read priority=50 backend=backend-A`——验证了"同一个路径前缀，根据附加条件是否满足，会退回到优先级更低但更宽松的规则"这一路由设计意图。

## 15. Mental Model

```text
Request (req table)
      |
      v
sort routes by priority desc
      |
      v
for each route in sorted:
    matcher(req)?  --false--> 下一条
        |
       true
        |
        v
    condition(req)? (若存在)  --false--> 下一条
        |
       true (或 condition 为 nil)
        |
        v
    命中，返回这条 route.backend
```

记忆点：**matcher 决定"这条规则原则上适用吗"，condition 决定"这次适用的前提条件满足了吗"**，两者都通过才算真正命中；priority 只决定"多条都可能命中时，先看谁"。

## 16. Why this is this Pattern

这是 Rule Engine 而不是 Policy：路由决策由一组结构统一、可以独立增删的规则驱动（每条规则是数据，不是散落的 `if`），新增一个 `/v2/api/*` 路由只需要在 `routes` 表里加一条，不需要改 `match_first`/`match_all` 的调度逻辑。如果整个路由逻辑写成一个 `decide_backend(req)` 函数，内部塞满专门为各个路径手写的判断分支，就退化成了 Policy。

这也不是 Configuration：Configuration 描述的是"运行时参数应该是什么"（例如"默认后端是谁"这种单一值），而这里描述的是"IF 路径匹配 AND 条件满足 THEN 转发到某后端"这种 IF/THEN 逻辑关系，重点在条件判断，不是参数取值。

**如果 Lua 消失，Host 还能做什么？** Nginx 原生的 `location` + 正则匹配本身就是一个不需要 Lua 的、内建的"路由规则引擎"（`location ~ ^/api/admin/ { ... }`），大部分静态路由需求用 Nginx 原生配置就能解决。Lua 消失后失去的是：动态附加条件（按 Header/查询参数二次判断）、运行时可编程的优先级排序逻辑、以及把规则表当作数据在多个环境间复用/测试的能力（就像本文件末尾的 `run_demo()`，可以完全脱离 Nginx 单独跑单元测试）。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** 完全成立，而且这恰恰是很多真实网关的做法：
- 换成 **JSON/YAML**：Kubernetes Ingress、Traefik、Envoy 的路由规则本质上都是 YAML/JSON 描述的"路径匹配 + 后端"表，`priority` 和简单的路径前缀/Header 存在性判断用声明式配置完全够用，这也是这类系统的主流选择——因为它们的 Condition 通常足够简单，不需要图灵完备的语言。
- 换成 **纯 C**：Nginx 核心的 `location` 匹配本身就是纯 C 实现的前缀树匹配，性能远超 Lua 逐条遍历，但代价是新增一种匹配语义（比如本示例的"查询参数等于某值"）需要改 Nginx 源码重新编译。

本示例选 Lua 的理由是"Condition 复杂到 YAML 表达力不够，但又没到需要新增 Nginx 核心匹配语义的程度"——这正是 OpenResty 存在的意义：把纯 C 核心的高性能匹配和 Lua 的灵活条件判断结合在同一个请求处理管线里。
