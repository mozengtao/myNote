# 01 · Example 1：TCP Connection

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。本例展示 Class / Constructor Module 最典型的场景——同时管理多个网络连接，每个连接有自己的 `state`（`closed`/`connected`）、`address`、`buffer`，互不干扰。

## 1. Problem / Scenario

一个程序要同时维护多个 TCP 连接：连到不同地址、各自处于不同的连接状态、各自有自己待发送的数据缓冲。如果用 Table Module（Pattern 1）或 Private+Public Module（Pattern 2）的思路——State 挂在 Module 级别——就只能表达"全局唯一一个连接"，一旦要求同时存在两个、三个连接，Module 级 State 立刻不够用。需要的是：**同一种"连接类型"，可以创建任意多个独立的连接对象，每个对象的状态只属于它自己**。

## 2. Architecture

```text
Application (app.lua)
     |
     |  require("connection") -- 只发生一次，拿到 Connection 这个 Class Table
     v
Class Module (Connection)
     |
     |  Connection.new(address) -- 调用两次，分别产生 conn_a、conn_b
     v
Object (conn_a)        Object (conn_b)
   address, state,        address, state,
   buffer 各自独立          buffer 各自独立
```

对象关系图（沿用 [00-why-and-architecture.md](00-why-and-architecture.md) 里的通用图，把节点换成本例的具体对象）：

```text
            Connection
                |
           __index
                |
       +--------+--------+
       v                 v
    conn_a             conn_b
       |                 |
   address/state/     address/state/
   buffer(独立)        buffer(独立)
```

## 3. Framework / Caller

`app.lua` 扮演 Caller：先 `require("connection")` 拿到 `Connection` 这个 Class Module（只做一次），然后调用两次 `Connection.new(...)` 创建 `conn_a`、`conn_b`。之后所有操作（`:connect()`、`:send()`、`:close()`）都直接作用在某一个具体的 Object 上，Caller 从此不再直接和 `Connection` 这个 Class Table 打交道（除了偶尔比较 `conn_a.connect == conn_b.connect` 来验证方法共享）。

## 4. Module

`connection.lua` 是 Class Module，装了构造函数 `Connection.new` 和四个方法 `connect` / `send` / `close` / `describe`；它自己不持有任何"某一个具体连接"的数据，那些数据只存在于每次 `.new()` 返回的实例表上。

## 5. Complete Lua Code

`code/01-tcp-connection/connection.lua`：

```lua
-- connection.lua
--
-- Class / Constructor Module：Connection 是一个"类型"（Class Table），
-- 只在 require 时被创建一次，只装方法（connect/send/close），不装任何
-- 具体连接的数据。每一次 Connection.new(address) 才会产生一个独立的
-- Object（Instance Table），拥有自己的 address / state / buffer。
--
-- 固定结构：
--   local Connection = {}
--   Connection.__index = Connection
--   function Connection.new(...) local self = setmetatable({}, Connection) ... end
--   function Connection:method(...) ... end
--   return Connection

local Connection = {}
Connection.__index = Connection

--- Connection.new(address) -> Connection
-- 用 "." 调用，因为这里还没有一个已存在的 Instance 可以当 self——
-- new 本身就是负责"造出第一个 self"的函数。
-- setmetatable(self, Connection) 把 self 的元表设为 Connection，
-- 之后 self 上找不到的字段（典型是方法）都会 fallback 到 Connection 上找。
function Connection.new(address)
    local self = setmetatable({}, Connection)
    self.address = address
    self.state = "closed"   -- Object State：只属于这一个实例
    self.buffer = {}        -- Object State：每个实例自己的发送缓冲区
    return self
end

--- connection:connect() -- 用 ":" 调用，隐式传入 self（即调用者自己）
function Connection:connect()
    if self.state == "connected" then
        return false, "already connected"
    end
    self.state = "connected"
    return true
end

--- connection:send(data) -- 只修改 self（这个实例）的 buffer，不影响其他实例
function Connection:send(data)
    if self.state ~= "connected" then
        return false, "not connected"
    end
    table.insert(self.buffer, data)
    return true
end

function Connection:close()
    self.state = "closed"
    self.buffer = {}
    return true
end

--- connection:describe() -> string
-- 只读方法，方便 app.lua 打印某一个实例当前的状态，
-- 用来证明多个实例的状态互不干扰。
function Connection:describe()
    return string.format(
        "Connection(address=%s, state=%s, buffered=%d)",
        self.address, self.state, #self.buffer
    )
end

return Connection
```

`code/01-tcp-connection/app.lua`：

```lua
-- app.lua
--
-- Application：require("connection") 只拿到 Connection 这个 Class Module
-- 一次；随后用 Connection.new(...) 创建两个完全独立的 Object：conn_a、
-- conn_b。全程只驱动这两个 Object 各自的 connect/send/close，
-- 证明它们的 state/buffer 互不影响。

local Connection = require("connection")

print("== 创建两个独立的 Connection 实例 ==")
local conn_a = Connection.new("10.0.0.1:8080")
local conn_b = Connection.new("10.0.0.2:9090")

print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 只连接 conn_a，观察 conn_b 是否被影响 ==")
local ok, err = conn_a:connect()
print(string.format("conn_a:connect() -> ok=%s", tostring(ok)))
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 向 conn_a 发送数据，conn_b 的 buffer 必须仍然是 0 ==")
conn_a:send("GET / HTTP/1.1")
conn_a:send("Host: example.com")
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 现在连接 conn_b，并发送不同的数据 ==")
conn_b:connect()
conn_b:send("PING")
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 关闭 conn_a，conn_b 必须仍然保持 connected ==")
conn_a:close()
print("conn_a: " .. conn_a:describe())
print("conn_b: " .. conn_b:describe())

print("\n== 未连接时调用 send 会失败（state 检查） ==")
local send_ok, send_err = conn_a:send("late data")
print(string.format("conn_a:send() -> ok=%s, err=%s", tostring(send_ok), tostring(send_err)))

print("\n== 两个实例共享同一份方法（同一个 Class Table） ==")
print(string.format(
    "conn_a.connect == conn_b.connect -> %s (都 fallback 到同一个 Connection.connect)",
    tostring(conn_a.connect == conn_b.connect)
))
```

## 6. Execution Flow

用 `$HOME/.local/bin/lua5.4 app.lua` 实际跑出的输出（已验证，无报错）：

```text
== 创建两个独立的 Connection 实例 ==
conn_a: Connection(address=10.0.0.1:8080, state=closed, buffered=0)
conn_b: Connection(address=10.0.0.2:9090, state=closed, buffered=0)

== 只连接 conn_a，观察 conn_b 是否被影响 ==
conn_a:connect() -> ok=true
conn_a: Connection(address=10.0.0.1:8080, state=connected, buffered=0)
conn_b: Connection(address=10.0.0.2:9090, state=closed, buffered=0)

== 向 conn_a 发送数据，conn_b 的 buffer 必须仍然是 0 ==
conn_a: Connection(address=10.0.0.1:8080, state=connected, buffered=2)
conn_b: Connection(address=10.0.0.2:9090, state=closed, buffered=0)

== 现在连接 conn_b，并发送不同的数据 ==
conn_a: Connection(address=10.0.0.1:8080, state=connected, buffered=2)
conn_b: Connection(address=10.0.0.2:9090, state=connected, buffered=1)

== 关闭 conn_a，conn_b 必须仍然保持 connected ==
conn_a: Connection(address=10.0.0.1:8080, state=closed, buffered=0)
conn_b: Connection(address=10.0.0.2:9090, state=connected, buffered=1)

== 未连接时调用 send 会失败（state 检查） ==
conn_a:send() -> ok=false, err=not connected

== 两个实例共享同一份方法（同一个 Class Table） ==
conn_a.connect == conn_b.connect -> true (都 fallback 到同一个 Connection.connect)
```

逐段对应结论：

- `conn_a:connect()` 之后，`conn_b` 的 `state` 仍然是 `closed`——证明 `state` 是各自实例表上的字段，不是共享的。
- `conn_a:send(...)` 两次之后，`conn_a` 的 `buffered=2`，`conn_b` 的 `buffered=0` 保持不变——`buffer` 也是各自独立的。
- `conn_a:close()` 之后 `conn_b` 依然是 `connected, buffered=1`——关闭一个连接完全不触碰另一个连接的表。
- 最后一行验证了"方法只有一份"：`conn_a.connect` 和 `conn_b.connect` 通过 `__index` fallback 到的都是同一个 `Connection.connect` 函数，`==` 比较为 `true`。

## 7. State

| 字段 | 归属 | 说明 |
|---|---|---|
| `Connection.new/connect/send/close/describe` | Class Table（`Connection`），一份 | 所有实例共享，创建多少个连接都不会多出一份方法代码 |
| `conn_a.address` / `conn_a.state` / `conn_a.buffer` | `conn_a` 这个 Instance Table，独立一份 | 只属于 `conn_a`，`conn_b:close()` 不会碰到它 |
| `conn_b.address` / `conn_b.state` / `conn_b.buffer` | `conn_b` 这个 Instance Table，独立一份 | 只属于 `conn_b` |

没有任何字段写在 `Connection` 表上（除了方法和 `__index`），这正是本例要展示的"干净"状态：Class Table 只装行为，不装数据。

## 8. Why this Pattern fits

TCP 连接天然是"同一种行为规则，多个独立实例"——`connect`/`send`/`close` 的逻辑对所有连接都一样，但每个连接自己"是否已连接"、"缓冲区里有什么"必须互相隔离，这正好对应 Class Table（放共享的方法）+ Instance Table（放各自的 `state`/`buffer`）的分工。用 Table Module 或 Private+Public Module 都做不到"同时存在两个独立状态的连接"这件事，因为那两个 Pattern 的 State（如果有）都只有 Module 级别的一份。

## 9. What would go wrong with another Pattern

- 如果用 **Table Module**（Pattern 1）：`connection.lua` 里直接写 `M.state = "closed"`，那么第二次调用 `M.connect()` 会直接复用同一份 `state`，无法表达"两个连接、一个已连一个未连"，两次 `connect()` 之间会互相覆盖对方的意图。
- 如果用 **Private+Public**（Pattern 2）：即使把 `state` 藏进一个闭包变量里，闭包变量依然只有一份（跟着 Module 的生命周期，不是跟着"某次调用"），本质上和 Table Module 的问题一样——无法同时表达多个连接。
- 只有把"状态"下放到每次 `.new()` 新建的表上（Class / Constructor Module），才能让 `conn_a` 和 `conn_b` 的 `state` 互不干扰。

## 10. Mental Model

**一句话记忆**：`Connection` 只是"怎么连接、怎么发送、怎么关闭"这套行为规则的说明书，说明书本身只有一份；每一次 `Connection.new(address)` 才是真正"造出一条连接"，造几次就有几条互相独立的连接。
