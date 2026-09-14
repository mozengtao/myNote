# 02 · 示例：OpenResty / Nginx 风格 Request Policy

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。代码位于 [code/02-openresty-request-policy/](code/02-openresty-request-policy/)。
>
> 运行环境说明：本示例需要安装 OpenResty（或 Nginx + lua-nginx-module）才能真正跑起来。本文只保证脚本语法正确、`ngx.*` API 用法正确、决策分支完整，不在本机搭建真实的 Nginx/OpenResty 环境执行它。

## 1. Problem（问题）

Nginx（或任何成熟的反向代理/Web 服务器）已经把"怎么收一个 HTTP 请求"这件事做得非常完善：TCP 连接管理、事件循环、HTTP 协议解析、keepalive、TLS 终止……这些全部是 C 代码里高度优化、极少改动的 Mechanism。

但几乎每个真实业务都会在"请求真正被处理之前"插入一些判断：只允许特定的 HTTP 方法、屏蔽一批已知的敏感/扫描路径、要求请求带上某个自定义 Header 才算合法。这些判断的具体内容变化很快（今天要新增一个屏蔽路径，明天要求换一个 Header 名字），如果每次改动都要改 Nginx C 模块源码再重新编译，对运维是不可接受的成本。

## 2. Why Lua（为什么用 Lua）

Nginx 通过 `lua-nginx-module`（OpenResty 的核心组件）在请求处理管线里开放了若干"阶段钩子"（`init`、`rewrite`、`access`、`content`、`header_filter`、`log` 等），`access_by_lua*` 这一组指令专门对应 access 阶段——这正是"请求该不该被继续处理"这个判断天然发生的位置。

用 Lua 写这一层判断，好处和示例 1 完全一致（改脚本不用重新编译、`pcall` 兜底避免整个 worker 崩溃），但多了一层 OpenResty 特有的优势：`ngx.*` 这一组 API 本身就是 Nginx Core 为 Lua 开的 Stable API（对应 [00-mental-model-overview.md](../00-mental-model-overview.md) 里的 Host Stable API 概念），`ngx.var`、`ngx.req`、`ngx.exit` 这些函数把"读取当前请求的 method/uri/headers"和"提前终止这个请求"都封装成了简单的函数调用，Lua 侧完全不需要知道底层 socket 或者 HTTP 解析器的任何细节。

## 3. Architecture（架构）

```text
TCP Connection
 |
 v
Nginx Core: parse HTTP request
 |
 v
[ access phase ]
 |
 v
access_by_lua_file policy.lua
 |
 +-- check method
 +-- check path
 +-- check headers
 |
 +---- 任一检查失败 ----> ngx.exit(4xx)   (提前终止，Lua 决策生效)
 |
 +---- 全部通过     ----> (什么都不做，正常返回)
                          |
                          v
                    content phase / proxy_pass 等后续阶段
```

`parse HTTP request` 完全由 Nginx Core（C）完成，Lua 运行的时候，一个结构完整的请求已经摆在 `ngx.var`/`ngx.req` 里了。`policy.lua` 从上到下顺序执行三组检查，命中黑名单条件的检查会立刻调用 `ngx.exit` 终止请求；如果三组检查都通过，脚本执行到文件末尾，`access` 阶段正常结束，请求自动进入下一阶段——**"允许"在这里的具体含义是"什么都不做"**，不是显式调用某个 `allow()` 函数。

## 4. Host responsibilities（Host 的职责）

- Nginx Core：管理 TCP 连接、事件循环、HTTP 协议解析，把一个请求的 method/uri/headers 组织好，通过 `ngx.var`/`ngx.req` 这组 API 暴露给 Lua。
- `lua-nginx-module`：在 `access` 阶节点插入一个钩子，调用 `access_by_lua_file` 指定的脚本；脚本执行期间维护好 Lua 侧能访问的请求上下文。
- 拿到 Lua 侧的 `ngx.exit(status)` 调用后，真正中断这个请求的处理管线，向客户端返回对应的 HTTP 状态码；如果 Lua 脚本没有调用 `ngx.exit`，则继续把请求送进后续阶段（`content`/`proxy_pass` 等）。
- 这些能力（HTTP 解析、连接管理、阶段调度、真正发送响应）全部是 Nginx Core 原本就有的，Lua 引入前后完全不变。

## 5. Lua responsibilities（Lua 的职责）

- 维护三组判断规则（本示例用局部 table 表达）：允许的方法集合 `allowed_methods`、路径黑名单 `denied_paths`、必须携带的 Header 名 `required_header`。
- 依次执行 method / path / header 三步检查，命中任一条拒绝规则就调用 `ngx.exit` 并 `return`，提前结束脚本。
- 全部检查通过时不做任何特殊动作（脚本正常执行到结尾），把"继续处理这个请求"的决定权隐式地交还给 Nginx。
- 不解析 HTTP 协议、不管理连接、不决定"如何"处理这个请求，只决定"要不要"处理。

## 6. Lifecycle（生命周期）

```text
Nginx worker 进程启动
    |
    v
lua_package_path / init_by_lua* (可选，本示例未使用)
    |
    v
每次有新请求到达 access 阶段
    |
    v
access_by_lua_file policy.lua   <-- 每个请求都会重新执行一遍这个脚本
    |
    +-- ngx.exit(...)   本次请求的 access 阶段在这里结束
    |
    +-- 脚本执行到结尾   本次请求正常进入下一阶段
    |
    v
Nginx worker 进程退出时，所有关联的 Lua VM 状态一并释放
```

需要特别说明：与示例 1、示例 3 里"一个长期存活的 `lua_State`，被多次调用"不同，`access_by_lua_file` 默认情况下**每个请求执行一次**这个脚本的顶层代码（OpenResty 内部会做字节码缓存优化，但脚本里写在顶层的局部变量在语义上每个请求都是全新的一份，不会在多个请求之间保留状态）。如果需要跨请求共享状态，需要用 `lua_shared_dict` 这类专门的共享内存 API，这已经超出本示例（纯 Policy 判断）的范围。

## 7. Data Flow（C -> Lua）

```text
Nginx Core 解析出的请求属性
    |
    v
ngx.var.request_method   <-- HTTP 方法
ngx.var.uri               <-- 请求路径
ngx.req.get_headers()     <-- 请求头 table
    |
    v
policy.lua 里的 method / uri / headers 局部变量
```

数据流动的方式和示例 1、示例 3 不同：不是 C 主动 `lua_push*` 把参数压栈调用一个 Lua 函数，而是 Lua 脚本主动调用 `ngx.var.xxx`/`ngx.req.get_headers()` 这组 C 侧实现的函数去"拉取"当前请求的属性。这是同一个"C/Lua 边界只通过显式 API 交换数据"的原则的另一种呈现方式：Lua 侧永远看不到 Nginx 内部真实的请求结构体，只能通过 `ngx.*` 这一组 Stable API 读取到经过封装的值。

## 8. Control Flow（Lua -> C）

```text
policy.lua 调用 ngx.exit(ngx.HTTP_FORBIDDEN)
    |
    v
lua-nginx-module 捕获这次调用
    |
    v
Nginx Core 中断当前请求的后续阶段，构造并发送 4xx 响应
```

`ngx.exit` 从 Lua 语法上看是一次普通函数调用，但它的效果是把控制权交还给 Nginx Core 并携带一个"提前终止"的指令；如果 Lua 脚本不调用 `ngx.exit`（也不调用 `ngx.redirect` 等类似的控制流函数），`access` 阶段就在脚本执行完毕后自然结束，Nginx Core 按默认行为把请求送进下一阶段——这是"不显式拒绝就是允许"的控制流约定，和示例 1 里"显式返回 true/false"的约定不同，但同样是 Lua 把一个决策结果交还给 Host 的方式。

## 9. State（状态归属）

- `allowed_methods`、`denied_paths`、`required_header` 是脚本顶层的局部变量，随每次请求重新求值一次（本示例里都是常量表，重新求值的开销可以忽略；如果这些规则很大或者需要跨请求共享，生产环境通常会挪到 `init_by_lua*` 阶段构建好，再通过模块级 `local` 变量在 worker 生命周期内复用）。
- 当前请求的 method/uri/headers 属于 Nginx Core 的请求上下文，Lua 只是只读地查询它们，不持有、不能修改这些底层数据结构。
- 没有跨请求累积的状态（比如"这个 IP 最近 1 分钟请求了多少次"），如果要加，需要引入 `lua_shared_dict`，那已经属于另一个话题（跨 worker 共享状态），不在本 Policy 示例的范围内。

## 10. Failure（失败路径）

- **脚本本身有语法错误**：Nginx 在加载配置或者第一次执行到这个 `location` 时会报错，请求会得到 500 级别的错误响应，错误信息会写进 Nginx 的 error log；这类错误应该在部署前用 `nginx -t`（配置测试）或者本地跑一次 `luac -p policy.lua`（纯语法检查）提前发现。
- **`ngx.req.get_headers()` 之类的调用在错误的阶段被调用**（比如在还没有解析完 body 的阶段访问 body 相关 API）：`lua-nginx-module` 会抛出一个运行时错误，同样会被记录到 error log，请求得到 500 响应。
- **策略判断本身写反了**（比如误把 `allowed_methods` 写成拒绝方法名单）：这不是"错误"，而是逻辑 bug，不会被任何机制自动捕获，只能靠测试和 code review 发现——这提醒我们 Policy 脚本本身也需要被当作代码认真测试，不能因为"只是配置一样的东西"而降低质量要求。
- 与示例 1、示例 3 不同，本示例没有 `lua_pcall` 意义上的显式错误处理代码，因为 `access_by_lua_file` 这一整条调用链的 `pcall` 包裹是 `lua-nginx-module` 内部完成的，脚本作者不需要（也没有机会）手写这部分。

## 11. Complete Host Code（完整 Host 代码）

本示例的"Host"是 Nginx + `lua-nginx-module`，不是本仓库自己编写的 C 代码。作为"Host 侧代码"的对应物，这里给出把 `policy.lua` 接入 Nginx 需要的最小配置片段（不在本机验证，仅作为集成参考）：

```nginx
http {
    lua_package_path "/usr/local/openresty/lualib/?.lua;;";

    server {
        listen 8080;

        location / {
            # access 阶段执行本示例的策略脚本：
            # 只允许 GET/POST，屏蔽黑名单路径，要求携带 X-Api-Key。
            access_by_lua_file /path/to/policy.lua;

            # 通过 access 检查之后，请求才会到达这里被真正处理
            proxy_pass http://backend_upstream;
        }
    }
}
```

## 12. Complete Lua Code（完整 Lua 代码）

对应文件：[code/02-openresty-request-policy/policy.lua](code/02-openresty-request-policy/policy.lua)

```lua
-- policy.lua
--
-- 运行环境说明：本脚本需要 OpenResty（或 Nginx + lua-nginx-module）才能真正运行，
-- 通过 access_by_lua_file 挂载到某个 location 的 access 阶段。
-- 本文只验证脚本逻辑正确性（语法、ngx.* API 用法、决策分支是否完整），
-- 不在本机搭建真实的 Nginx/OpenResty 环境执行它。
--
-- 职责边界（这正是本示例要说明的 Policy 本质）：
--   Nginx Core（C）已经拥有 socket、event loop、HTTP 协议解析、
--   连接管理这些 Mechanism —— 请求怎么被接收、怎么被解析成
--   method/uri/headers，全部由 C 完成，这个脚本运行的时候，
--   一个结构完整的 HTTP 请求已经摆在 ngx.var / ngx.req 里了。
--   这个脚本唯一要做的事情，是在"access"这个阶段回答一个
--   yes/no 问题："这个请求该不该被继续处理？"
--   它不解析协议、不管理连接、不新增任何 Nginx 原本没有的能力，
--   只是在已有能力（继续处理 / 提前终止）之间做一次选择。

-- 规则 1：只允许 GET / POST 两种方法
local allowed_methods = {
    GET = true,
    POST = true,
}

-- 规则 2：路径黑名单，命中直接拒绝（常见的敏感路径探测/扫描器路径）
local denied_paths = {
    ["/admin"] = true,
    ["/.env"] = true,
    ["/wp-login.php"] = true,
}

-- 规则 3：必须携带的自定义 Header（简化的 API Key 校验，
-- 真实场景通常还要校验值是否合法，这里只检查是否存在）
local required_header = "X-Api-Key"

-- ========== 1. check method ==========
local method = ngx.var.request_method
if not allowed_methods[method] then
    ngx.log(ngx.WARN, "policy: method not allowed: ", method)
    return ngx.exit(ngx.HTTP_NOT_ALLOWED)
end

-- ========== 2. check path（黑名单） ==========
local uri = ngx.var.uri
if denied_paths[uri] then
    ngx.log(ngx.WARN, "policy: path denied: ", uri)
    return ngx.exit(ngx.HTTP_FORBIDDEN)
end

-- ========== 3. check headers（必须携带 X-Api-Key） ==========
local headers = ngx.req.get_headers()
if not headers[required_header] then
    ngx.log(ngx.WARN, "policy: missing required header: ", required_header)
    return ngx.exit(ngx.HTTP_FORBIDDEN)
end

-- ========== 4. allow ==========
-- 不调用 ngx.exit，意味着 access 阶段正常结束，
-- 请求会继续往下走到后续阶段（content / proxy_pass 等），
-- 这就是"放行"在 access_by_lua 里的具体含义：什么都不做，让它继续。
ngx.log(ngx.INFO, "policy: request allowed: ", method, " ", uri)
```

## 13. Execution（运行方式）

运行环境说明：需要安装 OpenResty，本文只验证脚本逻辑正确性。真实部署步骤大致是：

```bash
# 1. 安装 OpenResty（以 Debian/Ubuntu 为例，具体请参考 OpenResty 官方文档）
#    这一步在本机未执行，仅作为流程说明。

# 2. 把 policy.lua 放到 nginx.conf 引用的路径
cp code/02-openresty-request-policy/policy.lua /path/to/policy.lua

# 3. 把上面第 11 步的 nginx.conf 片段合并进实际配置

# 4. 检查配置语法
/usr/local/openresty/nginx/sbin/nginx -t

# 5. 启动/重载
/usr/local/openresty/nginx/sbin/nginx
# 或
/usr/local/openresty/nginx/sbin/nginx -s reload
```

如果只想在没有 OpenResty 的机器上验证脚本本身没有 Lua 语法错误，可以用任意 Lua 解释器做纯语法检查（`ngx.*` 是未定义的全局变量，运行到那一行会报"attempt to call a nil value"，但语法层面能通过）：

```bash
luac -p code/02-openresty-request-policy/policy.lua
# 无输出即表示语法正确
```

## 14. Step-by-step walkthrough（逐步执行走读）

由于本示例不在本机真实运行，这里按四种典型请求，逐步说明脚本在真实 OpenResty 环境下会如何执行（等价于"预期输出"）：

1. **请求 A**：`DELETE /` —— 第一步 `check method` 就命中，`allowed_methods["DELETE"]` 为 `nil`，调用 `ngx.exit(ngx.HTTP_NOT_ALLOWED)`（405），脚本立刻结束，后面的 path/header 检查完全不会执行。
2. **请求 B**：`GET /admin`，方法通过，但 `denied_paths["/admin"]` 为 `true`，调用 `ngx.exit(ngx.HTTP_FORBIDDEN)`（403）。
3. **请求 C**：`GET /orders`（未携带 `X-Api-Key`），方法、路径都通过，`headers["X-Api-Key"]` 为 `nil`，调用 `ngx.exit(ngx.HTTP_FORBIDDEN)`（403）。
4. **请求 D**：`POST /orders`，携带 `X-Api-Key: demo-key-123`，三步检查全部通过，脚本执行到最后一行 `ngx.log(ngx.INFO, ...)`，没有调用 `ngx.exit`，`access` 阶段正常结束，请求被 `proxy_pass` 转发给后端 `backend_upstream`。

三步检查的顺序本身也值得注意：`check method` 放在最前面，因为它是判断成本最低、命中面最广的一类过滤（一次字符串比较，能挡掉绝大多数明显不合法的请求）；`check headers` 放在最后，因为 `ngx.req.get_headers()` 的开销略高于简单的字段读取——这是一个常见的性能优化习惯：便宜且高命中率的检查放前面，贵一点的检查放后面，尽量让不合法的请求尽早被拒绝，减少不必要的计算。

## 15. Mental Model（心智模型）

```text
Nginx Core
 |
 +-- Mechanism: TCP/事件循环/HTTP 解析/连接管理/真正发送响应
 |
 +-- Stable API: ngx.var / ngx.req / ngx.exit / ngx.log
         |
         v
        Lua (policy.lua，每个请求重新执行一次)
         |
         +-- check method
         +-- check path
         +-- check headers
         |
         +-- Decision: ngx.exit(4xx) 或者什么都不做
```

记忆点：`access_by_lua_file` 里的 Lua 代码，本质上是在问一句话："这个请求配得上被继续处理吗？" 回答"不配"就 `ngx.exit`，回答"配"就什么都不做——**不做任何事情本身就是一种合法的、显式的决策结果**。

## 16. Why this is this Pattern（为什么这是 Policy 而不是别的模式）

这是 Policy，不是 Extension：Nginx Core 在有没有这段 Lua 脚本的情况下，都完整拥有"处理 HTTP 请求"的能力——能接收连接、能解析协议、能转发给后端、能返回任意状态码。这段脚本没有让 Nginx 多学会任何一种新的协议或新的处理方式，唯一的作用是在已有的两种走向（继续处理 / 提前用某个状态码终止）之间做一次选择，这正是 Policy 的核心定义：**在已有能力之间做选择，而不是新增能力**。

也不是 Rule Engine：三步检查（method / path / header）是顺序执行、有依赖关系的一次性判断流程（命中即返回，不会去统计"命中了几条规则"），不是一组彼此独立、需要被遍历、需要收集"所有命中结果"的规则表。如果未来需求变成"维护一份可以动态增删的规则列表，每条规则独立判断，可能同时命中多条，各自触发不同等级的处理"，那就会演变成 [04-rule-engine/](../04-rule-engine/00-why-and-architecture.md) 里的 Rule Engine 模式。

- **如果 Lua 消失，Host 还能做什么？** Nginx Core 依然能完整地接收连接、解析请求、转发给后端——这些是它的原生能力，从来不依赖 Lua。会消失的是"运行时可调整的请求准入判断"这层：要实现同样的方法/路径/Header 检查，只能退回到 Nginx 原生的配置指令（`limit_except` 限制方法、`location` + `deny`/`allow` 做路径级别的访问控制、`map` + `if` 做一些 Header 判断），这些原生指令组合起来能覆盖本示例的大部分逻辑，但表达能力有上限（复杂的条件组合、字符串处理逻辑，原生 Nginx 配置语法很快会变得难以维护），而且同样需要 `nginx -s reload` 才能生效，运行时可编程性不如 Lua。
- **如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** "在 access 阶段做一次准入判断"这个 Pattern 本身依然成立。换成纯 C：等价于自己写一个 Nginx C 模块挂在 access 阶段，能做的判断种类不受限制，但每改一条规则都要重新编译整个模块并重启/重载 Nginx，运维成本远高于改一个 Lua 文件。换成 JSON/YAML：可以把 `allowed_methods`、`denied_paths`、`required_header` 这三份**纯数据**规则外部化成一份配置文件，用 Nginx 自带的 `map` 指令或者一个小的 C 模块去读取和判断——这对本示例这种"纯粹的白名单/黑名单查表"场景是完全可行的，而且比 Lua 更"配置化"、更不容易被误改出逻辑 bug；但一旦准入逻辑需要计算（例如"校验 Header 的值是否匹配某个签名算法""根据请求体大小动态调整策略"），JSON/YAML 就无法表达这部分逻辑，必须退回到某种可执行代码，Lua 相对纯 C 模块的优势（不需要重新编译、`pcall` 兜底、能与 Nginx 事件模型无缝配合而不阻塞 worker）就重新变得重要。
