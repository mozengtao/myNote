# 03 · Example 3：Session

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。本例引入"校验结果依赖外部输入"（密码是否正确）这一变量，展示即使两个实例调用**同一个方法、传入不同参数**，最终各自的 `authenticated` 状态也完全独立，互不覆盖。

## 1. Problem / Scenario

系统里同时存在多个用户的登录会话：`alice` 用正确密码登录，`bob` 用错误密码尝试登录。两次 `login()` 调用互相独立，`bob` 登录失败绝不能影响 `alice` 已经登录成功的状态，反过来 `alice` 登出也不能影响 `bob`。

## 2. Architecture

```text
Application (app.lua)
     |
     |  require("session") -- 只发生一次
     v
Class Module (Session)
     |
     |  Session.new(user) -- 调用两次，分别产生 session_alice、session_bob
     v
Object (session_alice)         Object (session_bob)
   user="alice"                   user="bob"
   authenticated=? (独立)          authenticated=? (独立)
```

```text
              Session
                 |
            __index
                 |
       +---------+---------+
       v                   v
  session_alice        session_bob
       |                   |
   user/authenticated   user/authenticated
      (独立)                (独立)
```

## 3. Framework / Caller

`app.lua` 为两个不同用户各创建一个 `Session` 实例，分别调用 `:login(password)` 传入不同的密码（一个正确、一个错误），再分别调用 `:is_authenticated()` 验证结果，最后让 `alice` 登出，确认 `bob` 的状态不受影响。

## 4. Module

`session.lua` 装了 `login`/`logout`/`is_authenticated` 三个方法和一个模块级常量 `VALID_PASSWORD`（模拟"正确密码"的数据源）。注意 `VALID_PASSWORD` 是**只读**的模块级常量，不是"State"——它不会因为任何一次 `login()` 调用而改变，这和"State"的定义（会随调用变化的数据）不同，不违反 Class Table 不持有 Object State 的原则。

## 5. Complete Lua Code

`code/03-session/session.lua`：

```lua
-- session.lua
--
-- Class / Constructor Module：Session 是类型（Class Table），只装
-- login/logout/is_authenticated 三个方法。每个 Session.new(user)
-- 产生一个独立 Object，拥有自己的 user / password（这里用一个固定的
-- "正确密码"模拟真实校验）和 authenticated 标志。

local Session = {}
Session.__index = Session

-- 模拟的"正确密码"数据源：真实系统里这会来自用户数据库，
-- 这里简化成常量，只是为了让 login() 里有真实的成功/失败分支。
local VALID_PASSWORD = "correct-password"

function Session.new(user)
    local self = setmetatable({}, Session)
    self.user = user
    self.authenticated = false   -- Object State：只属于这一个 Session 实例
    return self
end

--- session:login(password) -> boolean, string|nil
-- 密码正确才把 self.authenticated 置为 true；这个赋值只发生在
-- 当前实例的 Instance Table 上，不会影响任何其他 Session 实例。
function Session:login(password)
    if password ~= VALID_PASSWORD then
        self.authenticated = false
        return false, "invalid password"
    end
    self.authenticated = true
    return true
end

function Session:logout()
    self.authenticated = false
end

function Session:is_authenticated()
    return self.authenticated
end

function Session:describe()
    return string.format(
        "Session(user=%s, authenticated=%s)",
        self.user, tostring(self.authenticated)
    )
end

return Session
```

`code/03-session/app.lua`：

```lua
-- app.lua
--
-- Application：为两个不同的用户各创建一个 Session 实例；一个用正确
-- 密码登录，另一个用错误密码登录。打印两者的 is_authenticated()，
-- 证明认证结果只取决于各自实例上的 authenticated 字段，互不干扰。

local Session = require("session")

print("== 创建两个独立的 Session 实例 ==")
local session_alice = Session.new("alice")
local session_bob = Session.new("bob")

print(session_alice:describe())
print(session_bob:describe())

print("\n== alice 用正确密码登录，bob 用错误密码登录 ==")
local ok_a, err_a = session_alice:login("correct-password")
local ok_b, err_b = session_bob:login("wrong-password")

print(string.format("session_alice:login() -> ok=%s", tostring(ok_a)))
print(string.format("session_bob:login()   -> ok=%s, err=%s", tostring(ok_b), tostring(err_b)))

print("\n== 分别检查 is_authenticated()，两者结果必须不同 ==")
print(string.format("session_alice:is_authenticated() -> %s", tostring(session_alice:is_authenticated())))
print(string.format("session_bob:is_authenticated()   -> %s", tostring(session_bob:is_authenticated())))

print(session_alice:describe())
print(session_bob:describe())

print("\n== alice 登出后，bob 的认证状态不应受到任何影响 ==")
session_alice:logout()
print(string.format("session_alice:is_authenticated() -> %s", tostring(session_alice:is_authenticated())))
print(string.format("session_bob:is_authenticated()   -> %s (bob 从未被 alice 的操作影响)", tostring(session_bob:is_authenticated())))

print("\n== 两个实例共享同一份方法（同一个 Class Table） ==")
print(string.format(
    "session_alice.login == session_bob.login -> %s",
    tostring(session_alice.login == session_bob.login)
))
```

## 6. Execution Flow

用 `$HOME/.local/bin/lua5.4 app.lua` 实际跑出的输出（已验证，无报错）：

```text
== 创建两个独立的 Session 实例 ==
Session(user=alice, authenticated=false)
Session(user=bob, authenticated=false)

== alice 用正确密码登录，bob 用错误密码登录 ==
session_alice:login() -> ok=true
session_bob:login()   -> ok=false, err=invalid password

== 分别检查 is_authenticated()，两者结果必须不同 ==
session_alice:is_authenticated() -> true
session_bob:is_authenticated()   -> false
Session(user=alice, authenticated=true)
Session(user=bob, authenticated=false)

== alice 登出后，bob 的认证状态不应受到任何影响 ==
session_alice:is_authenticated() -> false
session_bob:is_authenticated()   -> false (bob 从未被 alice 的操作影响)

== 两个实例共享同一份方法（同一个 Class Table） ==
session_alice.login == session_bob.login -> true
```

逐段对应结论：

- `session_alice:login("correct-password")` 成功，`session_bob:login("wrong-password")` 失败——同一个 `Session.login` 函数体，因为 `self` 不同（分别是 `session_alice`、`session_bob`）、传入参数不同，产生了不同的结果，并各自写回各自的 `self.authenticated`。
- `is_authenticated()` 分别返回 `true`/`false`，与 `describe()` 打印的结果一致。
- `session_alice:logout()` 之后，`session_bob:is_authenticated()` 仍然是 `false`（这个例子里 `bob` 从始至终没有认证成功过，这一步主要验证 `logout()` 只修改 `session_alice.authenticated`，完全不触碰 `session_bob` 的表）。
- 最后验证方法共享：`session_alice.login == session_bob.login` 为 `true`。

## 7. State

| 字段 | 归属 | 说明 |
|---|---|---|
| `VALID_PASSWORD` | Module 内的 local 常量，不属于 Class Table 也不属于任何实例 | 只读，不随调用变化，不是 State |
| `Session.new/login/logout/is_authenticated/describe` | Class Table（`Session`），一份 | 两个实例共用同一份方法代码 |
| `session_alice.user` / `session_alice.authenticated` | `session_alice` 的 Instance Table，独立一份 | 只属于 `alice` 这个会话 |
| `session_bob.user` / `session_bob.authenticated` | `session_bob` 的 Instance Table，独立一份 | 只属于 `bob` 这个会话 |

### Class Table vs Instance Table vs Instance State（本例专属分析）

把这个例子里出现的三个层次的"数据/代码"逐一对应清楚：

```text
Class Table (Session)
  内容: login / logout / is_authenticated / describe 四个函数 + __index = Session
  份数: 1 份，随 require("session") 创建
  会不会变: 不会。这四个函数体在整个程序运行期间是同一份代码，
            不会因为调用了多少次 login() 而改变

Instance Table (session_alice / session_bob)
  内容: 一张空表，被 setmetatable(self, Session) 接上 __index
  份数: 每次 Session.new(user) 产生 1 份，本例产生 2 份
  会不会变: 表本身（这个 Lua table 对象）创建之后不会被替换，
            但表里的字段值会变

Instance State (session_alice.authenticated / session_bob.authenticated)
  内容: Instance Table 里具体某个 key 对应的值
  份数: 每个 Instance Table 各有一份，值随调用变化
  会不会变: 会。这是本例里真正"动态变化"的部分——
            login() 成功后从 false 变成 true，logout() 后变回 false
```

三者的关系可以这样记：**Class Table 是"稳定不变的代码"，Instance Table 是"每个对象独立的一张纸"，Instance State 是"写在这张纸上、会被反复涂改的具体内容"**。`session_alice` 和 `session_bob` 是两张不同的纸，纸上的格式（有哪些字段）相同（都由同一个 `Session.new` 决定），但格子里写的内容（`user` 是谁、`authenticated` 是 `true` 还是 `false`）互不相同、互不影响。

## 8. Why this Pattern fits

登录会话的本质就是"同一套认证规则，作用在多个独立的会话上"——`login`/`logout`/`is_authenticated` 的判断逻辑对所有用户都一样，但"这个会话现在是否已认证"必须严格按会话隔离，`alice` 登录成功绝不能让 `bob` 也变成已认证。Class / Constructor Module 里 Class Table 装规则、Instance Table 装状态的分工，正好对应这个需求。

## 9. What would go wrong with another Pattern

- 如果用 **Table Module**：`M.authenticated = false` 只能表示"整个程序里唯一一个认证状态"，`alice` 登录成功会让 `bob:is_authenticated()`（如果硬要塞进同一个 Module 的话）读到同一个 `true`，两个用户的会话被错误地合并成了一个。
- 如果用 **Private+Public Module**：把 `authenticated` 藏进一个 upvalue 变量，问题和 Table Module 一样——upvalue 只有一份，跟着 Module 的生命周期存在，不跟着"某一次登录"存在，依然无法表达"两个用户同时有各自的登录状态"。
- 只有让每个会话对应一张独立的 Instance Table，才能让 `session_alice.authenticated` 和 `session_bob.authenticated` 成为两个真正互不相关的布尔值。

## 10. Mental Model

**一句话记忆**：`Session` 只定义"怎么判断登录成功、怎么登出、怎么查询"，这套规则本身不知道也不关心到底有多少个用户在用；每一次 `Session.new(user)` 才是"给某个用户开一张新的会话记录纸"，纸上的内容只由这个用户自己的操作决定。
