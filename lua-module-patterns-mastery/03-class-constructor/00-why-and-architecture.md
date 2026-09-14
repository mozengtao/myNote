# 00 · Pattern 3：Class / Constructor —— WHY & Architecture

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中的核心映射：**Class / Constructor Module -> Object / State**。前两个 Pattern（Table Module / Private+Public）关心的是"Module 该暴露什么 API、隐藏什么实现"，State（如果有）属于整个 Module，被所有调用者共享；这个 Pattern 第一次让 State **脱离 Module**，归属到 Module 创建出来的每一个独立 Object 上——这是它与前两个 Pattern 最本质的分界线。

## 1. Pattern Definition

Class / Constructor Module 要回答的问题是：**"我需要创建多个互不干扰的对象，每个对象有自己的一份数据"**。它的固定形状：

```lua
local User = {}
User.__index = User

function User.new(name)
    local self = setmetatable({}, User)
    self.name = name
    return self
end

function User:greet()
end

return User
```

逐行对应到 Mental Model 的词汇表：

| 代码 | 概念 | 说明 |
|---|---|---|
| `local User = {}` | **Class Table**（类型/模板） | 一份，随 `require` 只创建一次 |
| `User.__index = User` | 元表接线 | 让"落在 User 上"的字段查找规则生效 |
| `function User.new(name)` | **Constructor**（构造函数） | 用 `.` 调用，负责"造出第一个 self" |
| `setmetatable({}, User)` | 挂接元表 | 让新表在找不到字段时 fallback 到 `User` |
| `self.name = name` | **Object State**（对象状态） | 只写在这一个新表上，不写在 `User` 上 |
| `function User:greet()` | **方法**（只有一份，挂在 Class Table 上） | 用 `:` 定义，隐式声明第一个参数是 `self` |
| `return User` | Module 边界 | `require("user")` 拿到的是 `User` 这个 Class Table，不是某个具体对象 |

`require("user")` 返回的是 **User（Class）**，不是 **alice / bob（Object）**。Module 的边界只到"类型"这一层；Object 是 Caller 在拿到 Module 之后，自己反复调用 `.new()` 创造出来的，Module 本身并不持有、也不知道一共创造了多少个 Object。

对应到整份笔记反复出现的 Module -> Type -> Constructor -> Object -> Object State 这条链：

```text
Module
  |
  v
Type            (User 这个 Class Table 本身)
  |
  v
Constructor      (User.new)
  |
  v
Object           (alice, bob, ... 每次调用 new 产生一个)
  |
  v
Object State     (alice.name, bob.name, ... 每个 Object 自己的字段)
```

## 2. Why

为什么不能像 Table Module（Pattern 1）或 Private+Public Module（Pattern 2）那样，直接在 Module 级别存一份数据就够了？因为需求变了：**需要同时存在多个"同类型但数据不同"的东西**。

- 一个进程里可能同时有 10 个 TCP 连接，各自的 `state`（`connected`/`closed`）、`buffer` 都不同，不能共享一份。
- 一个进程里可能同时管理 3 台 Device，一台在跑、两台停着，"是否在运行"这个字段必须是三份独立的数据，不是一份。
- 两个用户各自登录一个 Session，A 登出不能影响 B 的登录状态。

如果坚持用 Pattern 1/2 的思路——State 放在 Module 级别——就只能支持"全局唯一一个连接/一台设备/一个 Session"，一旦需求变成"同时管理 N 个"，Module 级 State 立刻不够用，会被迫用一堆并行数组/表去模拟"第几个对象的第几个字段"，这正是 Class / Constructor Module 要在源头上解决的问题：**把"数据"从 Module 里搬出来，搬到 Module 创建的每一个 Object 自己身上**，Module 只保留"怎么创建"和"共享的行为"这两件事。

## 3. Architecture

Application / Framework / Module / Implementation 四层里，这个 Pattern 只用到两层：**Application** 和 **Class Module**；不存在 Framework 层（没有谁在调度、管理多个实现），也不存在 Implementation 层（不存在"同一个接口多种实现"的问题，那是 Pattern 4 Factory 要解决的）。多出来的是 Module 层之下的 **Object 层**——这一层不属于四层模型里任何一层，它是 Class Module 被调用之后，在运行时新增出来的产物：

```text
Application
     |
     |  require("connection") -- 只发生一次
     v
Class Module (Connection)
     |
     |  Connection.new(...) -- 发生 N 次
     v
Object (conn_a, conn_b, ...) -- 每次调用产生一个新的、独立的表
```

题目要求的对象关系图，把"方法查找"这条隐藏的边也画出来：

```text
              User
               |
          __index
               |
       +-------+--------+
       v                v
     alice             bob
       |                |
     state             state
```

- `User` 只有一份，画在最上面。
- `__index` 是从 `alice`、`bob` 指回 `User` 的箭头方向（虚线含义：这不是"数据"上的连接，是"字段查找失败时"才会走的 fallback 路径），图中用从上往下画是为了强调"`__index` 是在 `alice`/`bob` 创建时，被 `setmetatable` 写死在它们身上的属性",而不是 `alice`/`bob` 主动去"问" `User`。
- `alice`、`bob` 是两份独立的表，各自往下挂着自己的 `state`（这里的 `state` 指广义的"实例数据"，不是某个具体字段名）。

## 4. Lifecycle

这个 Pattern 的核心区别在于：**必须把"Module load time"和"Object creation time"这两个完全不同的时间点分开看**，这是它与 Pattern 1/Pattern 2 最大的生命周期差异——Pattern 1/2 只有"Module load time"这一个时间点（Module 从加载到销毁只有一份），这个 Pattern 在此基础上多了一个可以重复发生 N 次的"Object creation time"。

```text
Module load time  (只发生一次，在 require("connection") 时)
    |
    v
local Connection = {}
    |
    v
Connection.__index = Connection      -- 接好 fallback 规则,但还没有任何 Object 存在
    |
    v
定义 Connection.new / :connect / :send / :close   -- 方法只写一次
    |
    v
return Connection                     -- Application 拿到的是"类型"，不是"某个连接"


Object creation time  (发生 N 次，每次调用 Connection.new(address) 都会重来一遍)
    |
    v
local self = setmetatable({}, Connection)   -- 造一个新的空表，接上 __index
    |
    v
self.address = address ; self.state = "closed" ; self.buffer = {}   -- 只写在 self 上
    |
    v
return self                            -- 一个独立的、带有自己数据的 Object
```

关键结论：`require("connection")` **只会触发一次** "Module load time" 流程，不管这个 Module 被 `require` 多少次（Lua 会缓存 `require` 结果）；而 `Connection.new(...)` 每被调用一次，就会完整走一遍 "Object creation time" 流程，产生一个新的、独立的表。**方法定义（`Connection:connect` 等）只在 Module load time 写一次，之后所有 Object 共用这一份，不会因为创建了 100 个连接就出现 100 份 `connect` 函数。**

## 5. Relationship

依赖方向有两条，指向的目标不一样：

```text
Caller -> Class Module        (调用 Connection.new(...) 来创建 Object)
Caller -> Object              (调用 conn:connect() / conn:send(...) 来驱动某一个具体对象)
Object -> Class Module        (通过 __index，在自己身上找不到方法时才会发生，Caller 感知不到这一步)
```

Caller（Application）看到两种不同的"引用"：一种是 `Connection` 本身（类型，只有一个，通常只在创建对象时用到）；另一种是 `conn_a`、`conn_b`（对象，可以有很多个，日常调用几乎都发生在这一层）。**Object 到 Class Module 的箭头（`__index`）是 Lua 运行时自动帮忙走的，Caller 代码里完全看不到这一步，这也是本文 Failure 一节要重点解释的"隐藏的失败点"来源。**

## 6. Data / Control Flow

以 `alice:greet()` 为例，完整走一遍 Lua 在方法调用背后做的事：

```text
alice:greet()
   |
   |  等价于: alice.greet(alice)
   v
第 1 步：在 alice 这个表里直接找 "greet" 这个 key
   |
   |  alice 是通过 setmetatable({}, User) 创建的空表 + self.name = name，
   |  从来没有人往 alice 上写过 "greet" 这个 key
   v
第 2 步：alice 本身没有 "greet" -> 查 alice 的元表(metatable)
   |
   |  alice 的元表是 User (因为 new() 时执行过 setmetatable(self, User))
   |  元表里有 __index = User
   v
第 3 步：按 __index 的规则，去 User 表里找 "greet"
   |
   |  User.greet 是存在的（Module load time 时就定义好了）
   v
第 4 步：找到 User.greet，用它作为函数，把 alice 当第一个参数传进去
   |
   v
执行 User.greet(alice)   -- 函数体里的 self 就是 alice
```

`.new()` 的调用流程则完全不同，因为这一步还没有"实例"可言：

```text
User.new("alice")
   |
   |  这是普通函数调用，没有隐式的 self
   v
函数体内部: local self = setmetatable({}, User)
   |
   |  {} 是一个全新的空表；setmetatable 把它的元表设为 User
   v
self.name = "alice"     -- 写在这个新表上
   |
   v
return self              -- Caller 把返回值赋给变量 alice
```

## 7. State

这是本 Pattern 必须讲清楚的核心问题：**State 到底放在哪张表上**。

```text
Module State（这个 Pattern 里通常不应该出现）
    位置: 直接写在 Class Table 上，例如 User.name = "x"
    归属: 所有 Object 共享同一份（因为只有一个 User 表）
    后果: alice 和 bob 会"看见"同一个 name —— 这正是本文 Anti-pattern 要展示的错误

Object State（这个 Pattern 真正应该用的地方）
    位置: 写在每个实例表上，例如 self.name = name（在 new() 内部，self 就是 alice 或 bob）
    归属: 每个 Object 独立一份，alice.name 和 bob.name 是两张不同表上的两个不同 key
    生命周期: 从 Connection.new(...)/Device.new(...)/Session.new(...) 返回开始存在，
              到这个 Object 不再被任何变量引用、被 GC 回收为止
```

一句话区分：**如果这份数据"换一个对象就应该不一样"，它就必须是 Object State，写在 `self` 上；如果这份数据"无论创建多少个对象都应该只有一份"，才可能是 Module State，写在 Class Table 上（但要非常小心，见 Anti-pattern）。** 本文只在这里做一次对比性说明，Module State / Object State / Framework State / Plugin State 的完整横向对比留给 [06-require-and-state-mental-model.md](../06-require-and-state-mental-model.md) 统一处理，这里不重复展开。

## 8. Failure

这个 Pattern 有三种典型的、容易在真实代码里踩到的失败方式：

**(1) 忘记调用 `setmetatable`**

```lua
function Connection.new(address)
    local self = {}              -- 忘了 setmetatable(self, Connection)
    self.address = address
    return self
end
```

`self` 这时候是一张没有元表的普通空表。之后调用 `conn:connect()`，等价于 `conn.connect(conn)`——Lua 在 `conn` 上直接找 `connect`，没有元表，也就没有 `__index` 可以 fallback，直接报错：`attempt to call a nil value (method 'connect')`。**这个错误只会在第一次调用方法时才暴露**，`Connection.new(...)` 本身不会报错，因为构造阶段根本没有用到 `__index`——这也是它容易被忽略的原因：写代码时先测的往往是"能不能 new 出来"，而不是"new 出来的东西能不能调方法"。

**(2) 用 `.` 调用本该用 `:` 调用的方法**

```lua
conn.connect()          -- 忘了写 conn:connect()
```

`conn.connect` 通过 `__index` 能正常找到 `Connection.connect` 这个函数，所以不会在"查找"阶段报错；问题在"调用"阶段：`conn.connect()` 没有隐式传 `self`，函数体内部的 `self` 是 `nil`。如果函数体里访问了 `self.state`，会立刻报错 `attempt to index a nil value (local 'self')`；更隐蔽的情况是函数体刚好没用到 `self` 的任何字段（比如一个纯打印日志的方法），这次调用"看起来正常跑完了"，但下一次任何依赖 `self` 状态的方法都会出问题——**这是这一类错误里最难排查的一种，因为报错的位置往往和写错的位置不在同一行、甚至不在同一次调用里**。

**(3) 对不是"合格实例"的东西用 `:` 调用**

```lua
local raw = {}
raw:connect()            -- raw 从来没有被 setmetatable(raw, Connection)
```

`raw` 只是一张普通空表，没有元表，`:connect()` 展开成 `raw.connect(raw)`，在 `raw` 上找不到 `connect`，也没有 `__index` 可以 fallback，报错和情况 (1) 一样：`attempt to call a nil value (method 'connect')`。这种情况常见于：把 `.new()` 的返回值搞错（例如误把某个中间函数的返回值当成了实例）、或者手写了一张"看起来像实例"的表却跳过了构造函数。

三种失败方式的共同点：**Lua 不会在"定义 Class"的时候检查任何东西——`__index` 只在真正发生一次字段查找失败时才会被用到，所以所有这三类错误都要等到"用错误的方式调用方法"那一刻才会暴露，而不是在创建实例、甚至加载 Module 的时候。**

## 9. Mental Model

**一句话记忆**：Class Table 只有一份，装的是所有实例共享的方法；每个 Object 是独立的一份，装的是只属于它自己的数据；`__index` 负责在"某个字段在实例自己身上找不到"的时候，把查找请求转交给 Class Table。

**核心问题**：这份数据应该写在 `User` 上，还是写在 `alice`/`bob` 上？—— 只要能不假思索地回答这个问题，就掌握了这个 Pattern。

**核心关系**：

```text
一份 Class Table (方法)  <---- __index ----  N 份 Instance Table (数据)
```

## 10. Design Trade-offs

**优点**

- 能自然地表达"同一种类型，多个独立实例"，State 天然隔离，不需要手写"第几个对象的第几个字段"这类模拟结构。
- 方法只存一份（在 Class Table 上），不管创建多少个实例，内存里都只有一份 `connect`/`send`/`close` 函数体，创建实例的开销只是"一张小表 + 挂一个元表"。
- 调用方式（`obj:method(...)`）和大多数主流 OO 语言的直觉一致，读代码的人不需要额外解释就能猜出 `self` 是谁。

**缺点 / 代价**

- 多了一层"Class Table vs Instance Table"的心智负担，字段到底该写在哪张表上，对新手不直观，容易犯 Anti-pattern 里的错误。
- 没有真正的"私有字段"机制——`alice.name` 外部完全可以直接改，Pattern 2（Private+Public）里的封闭作用域技巧在这里不能直接套用（除非额外引入 Proxy Table 之类的机制，超出本 Pattern 范围）。
- `:` 和 `.` 混用是常见的错误来源（见 Failure 一节），错误提示往往滞后到真正访问 `self` 字段的那一刻才出现，而不是调用当下。

**什么时候不要用这个 Pattern**

- 如果整个进程里这个"类型"永远只会存在一个实例（比如一个全局唯一的 Logger、一个全局唯一的 Config），用不到"多实例独立 State"这个核心价值，Pattern 1/2 更简单直接，没必要绕一圈 `setmetatable`。
- 如果需要的其实是"根据某个参数选择创建哪一种具体实现"（比如根据配置选 TCP 还是 UDP 连接），这个 Pattern 本身不解决"选择"这个问题，需要在此之上叠加 Pattern 4（Factory）。

**与相邻 Pattern 的区别**

- vs Pattern 2（Private+Public）：Private+Public 的 State（如果有）是 Module 级的一份，被所有调用者共享；Class / Constructor 的 State 是 Object 级的，可以有任意多份，互相独立。这是两者唯一但也是最关键的区别。
- vs Pattern 4（Factory）：Factory 关心的是"该 `new` 哪一个具体的 Class"，本身几乎不持有 State；Class / Constructor 关心的是"`new` 出来的每一个 Object 该怎么管理自己的 State"。Factory 往往站在 Class / Constructor 的上游，先决定用哪个 Class，再由那个 Class 的 `.new()` 产生真正的 Object。

## Anti-pattern

最常见、也最隐蔽的错误：**把本该属于某个实例的数据，误写到了 Class Table 上。**

```lua
local User = {}
User.__index = User

function User.new(name)
    local self = setmetatable({}, User)
    -- 忘了写 self.name = name，
    -- 却在别的地方（甚至是复制粘贴时）误写成了：
    User.name = "global"
    return self
end

function User:greet()
    print("hello, " .. self.name)
end

local alice = User.new("alice")
local bob = User.new("bob")

alice:greet()   -- hello, global
bob:greet()     -- hello, global   (!!)
```

逐步展开这个错误发生时的字段查找过程，解释为什么 `alice.name` 和 `bob.name`会一起"看到"同一个值：

```text
alice.name 的查找过程:
   |
   v
第 1 步: 在 alice 这个实例表里找 "name" -> 没有 (从来没人写过 self.name = "alice")
   |
   v
第 2 步: 查 alice 的元表 -> 是 User，且 User.__index = User
   |
   v
第 3 步: 按 __index 规则去 User 表里找 "name" -> 找到了，值是 "global"
   |
   v
结果: alice.name == "global"


bob.name 的查找过程完全一样:
   |
   v
第 1 步: 在 bob 这个实例表里找 "name" -> 没有
   |
   v
第 2 步: 查 bob 的元表 -> 也是 User，同一个 User.__index = User
   |
   v
第 3 步: 去 User 表里找 "name" -> 找到的还是同一个 "global"
   |
   v
结果: bob.name == "global"
```

问题的根源：`alice` 和 `bob` **从来没有自己的 `name` key**，所以每一次读 `.name` 都必然 fallback 到 `User.name` 上——而 `User` 只有一份，任何一次 `User.name = ...` 都会立刻"污染"所有实例（包括已经创建的和将来才创建的）。更危险的是，如果之后某个实例被显式赋值过 `self.name = "carol"`，它会因为"自己身上有了这个 key"而不再 fallback，从而和其他仍然共享 `User.name` 的实例表现不一致——这种"部分实例正常、部分实例读到全局值"的现象在真实项目里非常难定位，因为报错本身不会发生（`name` 永远能读到一个字符串），只是值不对。

正确写法永远是把这类"每个对象都不同"的数据写在 `self` 上（即构造函数里的 `self.name = name`），`User` 表上只放方法，绝不放任何后续会被当作"这一个实例的数据"来读的字段。
