# 04-2 · 示例：Database Client Factory

> Pattern：Factory Module（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/02-db-client-factory/](code/02-db-client-factory/)（`drivers/sqlite.lua` / `drivers/postgres.lua` / `drivers/memory.lua` / `factory.lua` / `app.lua`）。

## 1. Problem / Scenario

一个应用需要读写数据，但具体用哪种数据库经常在部署阶段才能确定：开发机上用 `sqlite`（单文件，不需要额外进程），生产环境用 `postgres`，跑单元测试时用完全在内存里的 `memory` 驱动（不需要真的连一个数据库）。

**Application/Framework 不应该直接依赖某一个具体的数据库实现**——这是数据库客户端场景里最常被违反、后果也最严重的一条原则。一旦 Application 代码里到处散落着 `require("drivers.postgres")`、`PostgresDriver.new(...)`，换一次数据库（哪怕只是给某个模块换成内存版本跑测试）就要去改 Application 的每一处调用点。这个示例展示如何用 Factory 让 Application 完全不知道底层是哪一种数据库。

## 2. Architecture

```text
Caller (app.lua)
   |
   v
Factory (factory.lua)
   |
   +-- drivers/sqlite.lua    (SqliteDriver)
   +-- drivers/postgres.lua  (PostgresDriver)
   +-- drivers/memory.lua    (MemoryDriver)
```

三个驱动模块都实现同一套契约（Abstraction）：`:insert(record)` / `:query()`。为了不引入真实数据库依赖，本示例的三个"驱动"内部都只是用一张内存表 `self.rows` 模拟存储——真实项目里，`sqlite.lua`/`postgres.lua` 内部会换成真正的数据库连接和 SQL 语句，但**对 Factory 和 Application 来说这层替换完全不可见**，这正是这个示例要证明的点。

## 3. Framework / Caller 代码

```lua
local factory = require("factory")

local sqlite_db, err1 = factory.create("sqlite", { path = "app.db" })
sqlite_db:insert({ name = "alice" })
sqlite_db:query()
```

`app.lua` 全程只 `require("factory")`。它不知道、也不需要知道 `"sqlite"` 这个字符串背后到底对应哪个模块——如果某一天把 `"sqlite"` 换成 `"postgres"`，`app.lua` 里的 `insert`/`query` 调用代码一个字都不用改。

## 4. Module 代码

```lua
local SqliteDriver = require("drivers.sqlite")
local PostgresDriver = require("drivers.postgres")
local MemoryDriver = require("drivers.memory")

local M = {}

local creators = {
    sqlite = SqliteDriver.new,
    postgres = PostgresDriver.new,
    memory = MemoryDriver.new,
}

function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown db driver kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
```

三个具体驱动各自是一个 Pattern 3 风格的小模块，`self.rows` 是每个驱动实例独立持有的"存储"，不同驱动实例之间（甚至同一种驱动的不同实例之间）互不共享数据。

## 5. 完整 Lua 代码

与磁盘上的文件完全一致。

`code/02-db-client-factory/drivers/sqlite.lua`：

```lua
-- drivers/sqlite.lua
--
-- Class/Constructor Module：SqliteDriver。
-- 这里不连接真正的 sqlite 文件,用一张内存表 self.rows 模拟"存储",
-- 只是为了演示 Factory 组合 Class/Constructor 的机制,
-- 不引入真实数据库依赖。
--
-- 对外契约（Abstraction）：
--   :insert(record) -> id
--   :query()        -> rows（数组）

local SqliteDriver = {}
SqliteDriver.__index = SqliteDriver

function SqliteDriver.new(config)
    config = config or {}
    local self = setmetatable({}, SqliteDriver)
    self.path = config.path or ":memory:"
    self.rows = {}
    return self
end

function SqliteDriver:insert(record)
    local id = #self.rows + 1
    self.rows[id] = record
    print(string.format("  [sqlite:%s] insert row #%d: %s", self.path, id, tostring(record.name)))
    return id
end

function SqliteDriver:query()
    print(string.format("  [sqlite:%s] query -> %d row(s)", self.path, #self.rows))
    return self.rows
end

return SqliteDriver
```

`code/02-db-client-factory/drivers/postgres.lua`：

```lua
-- drivers/postgres.lua
--
-- Class/Constructor Module：PostgresDriver。
-- 同样不连接真正的 postgres 服务端,用内存表模拟"存储"。
-- 和 sqlite.lua 互不 require、互不知道对方存在,
-- 只是恰好实现了同一套 :insert/:query 契约。

local PostgresDriver = {}
PostgresDriver.__index = PostgresDriver

function PostgresDriver.new(config)
    config = config or {}
    local self = setmetatable({}, PostgresDriver)
    self.dsn = config.dsn or "postgres://localhost/app"
    self.rows = {}
    return self
end

function PostgresDriver:insert(record)
    local id = #self.rows + 1
    self.rows[id] = record
    print(string.format("  [postgres:%s] insert row #%d: %s", self.dsn, id, tostring(record.name)))
    return id
end

function PostgresDriver:query()
    print(string.format("  [postgres:%s] query -> %d row(s)", self.dsn, #self.rows))
    return self.rows
end

return PostgresDriver
```

`code/02-db-client-factory/drivers/memory.lua`：

```lua
-- drivers/memory.lua
--
-- Class/Constructor Module：MemoryDriver。
-- 专为测试/开发场景准备的"驱动"：连内存表这层伪装都不做了,
-- 直接就是内存表本身——但对外仍然满足 :insert/:query 契约,
-- 所以 Application 换用它时不需要改一行调用代码。

local MemoryDriver = {}
MemoryDriver.__index = MemoryDriver

function MemoryDriver.new(config)
    local self = setmetatable({}, MemoryDriver)
    self.rows = {}
    return self
end

function MemoryDriver:insert(record)
    local id = #self.rows + 1
    self.rows[id] = record
    print(string.format("  [memory] insert row #%d: %s", id, tostring(record.name)))
    return id
end

function MemoryDriver:query()
    print(string.format("  [memory] query -> %d row(s)", #self.rows))
    return self.rows
end

return MemoryDriver
```

`code/02-db-client-factory/factory.lua`：

```lua
-- factory.lua
--
-- Factory Module：唯一知道有哪些具体数据库驱动的地方。
-- Application/Framework 不应该、也不需要直接依赖某一个具体的数据库实现——
-- 换数据库（sqlite -> postgres）应该只是改一个配置字符串,
-- 不应该触发 Application 代码里任何一行的修改。

local SqliteDriver = require("drivers.sqlite")
local PostgresDriver = require("drivers.postgres")
local MemoryDriver = require("drivers.memory")

local M = {}

local creators = {
    sqlite = SqliteDriver.new,
    postgres = PostgresDriver.new,
    memory = MemoryDriver.new,
}

--- create(kind, config) -> db client 实例 | (nil, err)
function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown db driver kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
```

`code/02-db-client-factory/app.lua`：

```lua
-- app.lua
--
-- Application：全程只 require("factory")。
-- 搜索这个文件找不到任何一个具体数据库驱动模块的路径字符串——
-- Application 不知道、也不关心底层到底是哪一种数据库,
-- 只知道"我要一个 db client"。

local factory = require("factory")

print("== 通过 Factory 创建 sqlite client ==")
local sqlite_db, err1 = factory.create("sqlite", { path = "app.db" })
if sqlite_db then
    sqlite_db:insert({ name = "alice" })
    sqlite_db:insert({ name = "bob" })
    sqlite_db:query()
else
    print("  创建失败: " .. err1)
end

print("\n== 通过 Factory 创建 postgres client ==")
local pg_db, err2 = factory.create("postgres", { dsn = "postgres://db.internal/app" })
if pg_db then
    pg_db:insert({ name = "carol" })
    pg_db:query()
else
    print("  创建失败: " .. err2)
end

print("\n== 通过 Factory 创建 memory client（测试场景）==")
local mem_db, err3 = factory.create("memory", {})
if mem_db then
    mem_db:insert({ name = "test-user-1" })
    mem_db:insert({ name = "test-user-2" })
    mem_db:insert({ name = "test-user-3" })
    mem_db:query()
else
    print("  创建失败: " .. err3)
end

print("\n== 请求一个未知的 db 驱动 ==")
local bad_db, bad_err = factory.create("mysql", {})
if not bad_db then
    print("  预期的失败: " .. bad_err)
else
    print("  意外：mysql 竟然创建成功了")
end
```

## 6. Execution

```bash
cd code/02-db-client-factory
lua5.4 app.lua
```

实际执行输出（已用本地 Lua 5.4 验证）：

```text
== 通过 Factory 创建 sqlite client ==
  [sqlite:app.db] insert row #1: alice
  [sqlite:app.db] insert row #2: bob
  [sqlite:app.db] query -> 2 row(s)

== 通过 Factory 创建 postgres client ==
  [postgres:postgres://db.internal/app] insert row #1: carol
  [postgres:postgres://db.internal/app] query -> 1 row(s)

== 通过 Factory 创建 memory client（测试场景）==
  [memory] insert row #1: test-user-1
  [memory] insert row #2: test-user-2
  [memory] insert row #3: test-user-3
  [memory] query -> 3 row(s)

== 请求一个未知的 db 驱动 ==
  预期的失败: factory: unknown db driver kind 'mysql'
```

## 7. Execution Flow

1. `require("factory")` 载入 `factory.lua`，它先 `require` 三个具体驱动模块，构造 `creators` 选择表，`return M`。
2. `factory.create("sqlite", {path="app.db"})`：查到 `creators.sqlite == SqliteDriver.new`，转发 `config`，得到一个 `self.path = "app.db"`、`self.rows = {}` 的新实例。
3. `sqlite_db:insert({name="alice"})`：在这一个实例的 `self.rows` 里追加一条记录，返回自增 id；`sqlite_db:insert({name="bob"})` 再追加一条，`self.rows` 现在有 2 条。
4. `sqlite_db:query()`：返回当前这个实例自己的 `self.rows`，只包含 alice 和 bob 两条——不会因为之后又创建了 `postgres`/`memory` 实例而受到任何影响，因为每个实例的 `self.rows` 是独立的表。
5. 对 `"postgres"`、`"memory"` 重复同样的"创建 -> insert -> query"流程，三个实例的 `self.rows` 互不干扰。
6. 对 `"mysql"` 调用 `factory.create`：`creators.mysql` 是 `nil`，直接返回 `(nil, "factory: unknown db driver kind 'mysql'")`，不创建任何驱动实例。

## 8. State

Factory 本身无状态：`creators` 表只在 `require("factory")` 时构造一次，`M.create()` 每次调用只做"查表 + 转发"，不保留任何跨调用的记忆。

State 全部在每一个驱动实例的 `self.rows` 上，且严格按实例隔离——`sqlite_db.rows` 和 `pg_db.rows` 是两张完全独立的表,即使两者恰好都调用了 `:insert({name="alice"})`，它们各自的 `self.rows` 里各有一条"alice"记录，互不感知对方的存在。State 归属规则完全对应 [03-class-constructor/00-why-and-architecture.md](../03-class-constructor/00-why-and-architecture.md#7-state)：只要"换一个实例这份数据就应该不一样"，这份数据就必须写在 `self` 上。

## 9. Why this Pattern fits

- 数据库实现是典型的"部署环境决定用哪一个"的场景：本地开发用 sqlite、生产用 postgres、CI 跑测试用纯内存版本，三者需要在完全不修改 Application 逻辑的前提下切换。
- 三种驱动的调用契约（`:insert`/`:query`）天然一致——数据库客户端本来就是"存进去、查出来"这两个动作，具备套用统一 Abstraction 的前提。
- Application/Framework 层直接依赖具体数据库实现是数据库场景里公认的反模式（会导致换数据库牵连大量代码、单元测试被迫依赖真实数据库），Factory 正是解决这个具体反模式的标准手段。

## 10. What would go wrong with another Pattern

- **不用 Factory，只用 Class / Constructor**：Application 需要同时 `require("drivers.sqlite")`、`require("drivers.postgres")`、`require("drivers.memory")`，且自己写 `if kind == "sqlite" then ... end`，这正是数据库客户端场景里最典型、后果最严重的耦合——测试 Application 时，即使只想测 "memory" 分支，`drivers/postgres.lua`（如果换成真实驱动，可能依赖某个只有生产环境才有的 C 扩展）也必须能被加载。
- **换成 Private+Public**：Private+Public 假设只有一种实现、只是内部步骤需要隐藏，无法表达"sqlite/postgres/memory 是三个平级的、可互换的完整实现"这个事实。
- **换成 Plugin**：如果这个项目未来真的需要支持"由第三方在不修改核心代码的情况下新增一种数据库驱动"，Plugin（Pattern 5）会比 Factory 更合适；但如果驱动集合是项目内部确定的、有限的几种，Factory 更直接，不需要引入注册机制。

## 11. Mental Model

> Factory 在这里扮演的是"统一的数据访问入口"：Application 说"给我一个 db client"，从不问"这个 client 背后连的是哪个数据库"；换数据库变成改一个配置字符串（`kind`），不是一次跨越多个文件的代码修改。
