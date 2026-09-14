# 02 · Example 2：User Service

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。代码位于 [`code/02-user-service/`](code/02-user-service/)。

## 1. Problem / Scenario

需要一个"用户服务"：创建用户、按 id 查找用户、删除用户。创建之前必须校验输入（名字不能为空、邮箱格式要基本合法),校验通过之后还需要做一点归一化处理（去掉名字两端空格、把邮箱统一转小写),最后分配一个自增 id。这几步——校验、归一化、生成 id——都只是"如何正确地创建一个用户"这件事的具体做法,调用者只关心 `create`/`find`/`delete` 这三个动作本身。

这个 Example 同时是本 Pattern 里**显式持有 Module 级 State** 的一个具体载体：所有用户记录存放在 Module 内部的一张表里,这张表被所有调用者共享。

## 2. Architecture

```text
                     app.lua (Caller)
                            |
                            | require("user_service")
                            v
                    +---------------------+
                    | user_service.lua    |
                    |                     |
                    |  Module State       |
                    |  users = {}         |
                    |  next_id = 1        |
                    |       ^             |
                    |       |             |
                    |  Public API         |
                    |  M.create()         |
                    |  M.find()           |
                    |  M.delete()         |
                    |       |             |
                    |       v             |
                    |  Private Impl       |
                    |  validate()         |
                    |  normalize()        |
                    |  generate_id()      |
                    +---------------------+
```

`users`/`next_id` 画在 Public API 和 Private Implementation 之间,是因为两边都能读写它——`M.create` 直接写入 `users[id] = user`,`generate_id` 直接读写 `next_id`。调用者拿不到 `users` 这张表本身,只能通过 `M.create`/`M.find`/`M.delete` 间接地操作它。

## 3. Framework / Caller

`app.lua` 依次演示：创建一个合法用户（成功)、创建两个不合法输入（邮箱格式错误、缺少名字,两者都失败并返回 `nil, err`)、按 id 查找（成功和"id 不存在"两种情况)、再创建一个用户来证明"两次 `create` 共享同一张 `users` 表"、最后删除一个用户并证明删除之后再查找会失败。

## 4. Module

```text
Public API（挂在 M 上）
    M.create(input)  -- 校验 -> 归一化 -> 分配 id -> 写入 State
    M.find(id)        -- 从 State 里读一条记录
    M.delete(id)      -- 从 State 里删一条记录

Private Implementation（local function）
    validate(input)     -- 把关：名字非空、邮箱格式合法
    normalize(input)     -- 假设 validate 已通过,做去空格/转小写
    generate_id()        -- 从 Module State 里取下一个自增 id
```

`M.create` 内部严格按 `validate -> normalize -> generate_id` 的顺序调用——这个顺序本身也是实现细节,如果哪天需要调整（比如把 `generate_id` 换成 UUID,不再依赖自增计数器),只需要改这一个函数内部,`M.create` 的签名和返回值形状不变。

## 5. Complete Lua Code

`code/02-user-service/user_service.lua`：

```lua
--
-- user_service.lua
--
-- Pattern 2: Private + Public Module (Encapsulation)
--
-- Public API : create(input), find(id), delete(id)
-- Private impl: validate(input), normalize(input), generate_id()
--
-- Module-level State: `users` and `next_id` below are created exactly once,
-- the first time this module is require()'d, and are shared by every
-- caller that does `require("user_service")` for the rest of the process
-- lifetime (Lua's `require` cache returns the same table M on every call,
-- so it is also the same closure over `users`/`next_id`). This is exactly
-- the "Module 级 State" foreshadowed in the Pattern 2 architecture doc --
-- full treatment of require()'s caching behavior belongs to the separate
-- 06-require-and-state-mental-model.md document, not here.
--

local M = {}

--------------------------------------------------------------------------
-- Module-level State
--------------------------------------------------------------------------

local users = {}   -- id (number) -> { id = ..., name = ..., email = ... }
local next_id = 1

--------------------------------------------------------------------------
-- Private Implementation
--------------------------------------------------------------------------

-- validate: the gatekeeper. Every private helper downstream of validate()
-- is allowed to assume `input.name` and `input.email` are already
-- well-formed strings -- validate() is where that guarantee is established,
-- and it is only ever reached through the public M.create() path.
local function validate(input)
    if type(input) ~= "table" then
        return false, "input must be a table"
    end
    if type(input.name) ~= "string" or #input.name == 0 then
        return false, "name must be a non-empty string"
    end
    if type(input.email) ~= "string" or not input.email:match("^[^@%s]+@[^@%s]+%.[^@%s]+$") then
        return false, "email must look like user@host.tld"
    end
    return true
end

-- normalize: assumes validate() already ran. It does not re-check types --
-- if it were called with malformed input directly (which cannot happen from
-- outside the module, since normalize is not on M), it would fail with a
-- raw Lua error instead of a clean user-facing message. That asymmetry is
-- intentional: normalize's contract is "input already passed validate()".
local function normalize(input)
    return {
        name = input.name:gsub("^%s+", ""):gsub("%s+$", ""),
        email = input.email:lower(),
    }
end

-- generate_id: pure module-internal counter logic. Nothing about "how ids
-- are generated" is part of the module's public contract -- callers only
-- ever see the resulting id on the returned user record.
local function generate_id()
    local id = next_id
    next_id = next_id + 1
    return id
end

--------------------------------------------------------------------------
-- Public API
--------------------------------------------------------------------------

-- M.create: validates first, normalizes second, then touches State.
-- This ordering is the module's real responsibility -- private helpers
-- are not required to defend themselves against bad input because
-- M.create is the only door they can be reached through.
function M.create(input)
    local ok, err = validate(input)
    if not ok then
        return nil, err
    end

    local clean = normalize(input)
    local id = generate_id()

    local user = {
        id = id,
        name = clean.name,
        email = clean.email,
    }
    users[id] = user

    return user
end

function M.find(id)
    local user = users[id]
    if not user then
        return nil, "no user with id " .. tostring(id)
    end
    return user
end

function M.delete(id)
    if not users[id] then
        return false, "no user with id " .. tostring(id)
    end
    users[id] = nil
    return true
end

return M
```

`code/02-user-service/app.lua`：

```lua
--
-- app.lua
--
-- Framework/Caller role: only ever calls user_service.create / .find / .delete.
-- validate / normalize / generate_id do not exist on the returned table.
--

local user_service = require("user_service")

print("== create: success path ==")
local alice, err = user_service.create({ name = "  Alice  ", email = "Alice@Example.com" })
if not alice then
    print("unexpected failure: " .. tostring(err))
    os.exit(1)
end
print(string.format("created user id=%d name=%q email=%q", alice.id, alice.name, alice.email))

print()
print("== create: failure path (bad email) ==")
local bad, bad_err = user_service.create({ name = "Bob", email = "not-an-email" })
print("bad, err ->", bad, bad_err)

print()
print("== create: failure path (missing name) ==")
local bad2, bad2_err = user_service.create({ email = "carol@example.com" })
print("bad2, err ->", bad2, bad2_err)

print()
print("== find: success and failure ==")
local found, find_err = user_service.find(alice.id)
print("find(alice.id) ->", found and found.name, find_err)

local missing, missing_err = user_service.find(999)
print("find(999)      ->", missing, missing_err)

print()
print("== module-level State: create another user, both share the same `users` table ==")
local dave = user_service.create({ name = "Dave", email = "dave@example.com" })
print(string.format("created user id=%d name=%q", dave.id, dave.name))
local alice_again = user_service.find(alice.id)
print("find(alice.id) still works ->", alice_again.name)

print()
print("== delete: success and failure ==")
local deleted, delete_err = user_service.delete(alice.id)
print("delete(alice.id) ->", deleted, delete_err)

local not_found, not_found_err = user_service.delete(alice.id)
print("delete(alice.id) again ->", not_found, not_found_err)

print("find(alice.id) after delete ->", user_service.find(alice.id))

-- Only M.create / M.find / M.delete exist on the returned table. Uncommenting
-- the line below would fail with "attempt to call a nil value (field
-- 'validate')" -- validate/normalize/generate_id are truly unreachable:
--
-- user_service.validate({})
```

## 6. Execution Flow

`lua5.4 app.lua` 的实际输出：

```text
== create: success path ==
created user id=1 name="Alice" email="alice@example.com"

== create: failure path (bad email) ==
bad, err ->	nil	email must look like user@host.tld

== create: failure path (missing name) ==
bad2, err ->	nil	name must be a non-empty string

== find: success and failure ==
find(alice.id) ->	Alice	nil
find(999)      ->	nil	no user with id 999

== module-level State: create another user, both share the same `users` table ==
created user id=2 name="Dave"
find(alice.id) still works ->	Alice

== delete: success and failure ==
delete(alice.id) ->	true	nil
delete(alice.id) again ->	false	no user with id 1
find(alice.id) after delete ->	nil	no user with id 1
```

一次成功创建的调用路径：

```text
app.lua
  |
  |  user_service.create({name="  Alice  ", email="Alice@Example.com"})
  v
M.create
  |
  |  validate(input)          <- Private, 把关
  v
validate  ---> ok
  |
  |  normalize(input)         <- Private, 假设已校验
  v
normalize ---> {name="Alice", email="alice@example.com"}
  |
  |  generate_id()            <- Private, 读写 next_id 这个 Module State
  v
generate_id ---> 1
  |
  |  写入 users[1] = user      <- 修改 Module State
  v
M.create
  |
  |  返回 user 记录
  v
app.lua
```

## 7. State

`users` 和 `next_id` 是**Module 级 State**：它们在 `user_service.lua` 文件顶层用 `local` 声明,在第一次 `require("user_service")` 时创建一次,之后 `M.create`/`M.find`/`M.delete`（以及它们内部调用的 `validate`/`normalize`/`generate_id`)全部共享同一份 `users`/`next_id`。输出里 `dave` 被创建之后,`alice` 依然能被 `M.find` 查到——这证明两次 `create` 操作的是同一张表,不是各自独立的一份拷贝。

这份 State 归谁：归 `user_service.lua` 这个 Module 自己;谁能修改它：只有 `M.create`/`M.delete`（Public API),`validate`/`normalize`/`generate_id`（Private,其中只有 `generate_id` 真正读写了 `next_id`)。调用者拿不到 `users` 这张表的引用,无法绕开 `M.create`/`M.delete` 直接篡改用户记录。这份 State 具体是怎么在"同一个进程里所有 `require("user_service")` 调用点"之间被共享的——完整机制留给 [06-require-and-state-mental-model.md](../06-require-and-state-mental-model.md)。

## 8. Why this Pattern fits

`create` 这个动作天然需要三个协作步骤（校验、归一化、生成 id),而且这个 Module 确实需要一份跨调用共享的 State（用户表本身)。Pattern 2 同时满足这两个需求：`local function` 让"校验/归一化/生成 id 具体怎么做"保持自由,`M.xxx` 三个函数给调用者一个稳定、清晰的契约,Module 级 State 被安全地包在闭包里,只能通过这三个公开函数间接触达。

## 9. What would go wrong with another Pattern

- **换成 Table Module（`validate`/`normalize`/`generate_id` 全部公开)**：调用者可能会在创建用户之前"抢跑"调用 `M.normalize` 自己拼一个 user 记录,绕开 `validate` 的校验,State 里就会混入没被正确校验过的脏数据——这正是 Encapsulation 存在的意义。
- **换成 Class / Constructor Module（每个 `User` 是一个独立对象)**：这个场景里"用户"不是一个需要独立行为、独立方法的对象——它更接近一条数据记录,真正需要独立 State 的是"这个服务管理的所有用户的集合"，而不是"每一个用户各自的行为”。如果每个用户是一个独立对象,`find`/`delete` 按 id 操作就变得别扭（需要维护一个 id 到对象的额外映射,而这个映射本身又变成了新的 Module 级 State,等于绕了一圈回到原地)。Pattern 2 里"State 属于整个 Module"恰好匹配"用户表是一个整体"这个语义。

## 10. Mental Model

**一句话**：调用者只需要管住"创建/查找/删除"这三个动作,校验规则、归一化细节、id 怎么生成,全部是 Module 自己的事。

**核心问题**：写入 Module State 之前,谁负责保证写进去的数据是合法的？

**核心关系**：

```text
M.create -> validate -> normalize -> generate_id -> 写入 users
M.find   -> 读 users
M.delete -> 从 users 删除
```
