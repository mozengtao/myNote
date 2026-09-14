# 00 · Pattern 2：Private + Public Module —— WHY & Architecture

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中定义的词汇：Namespace / Encapsulation / Object State / Implementation Hiding / Contract。本文只展开 Pattern 2 —— **Private + Public Module -> Encapsulation**。

## 核心代码形状

Pattern 1（Table Module）解决的是"把相关函数放进一个 Namespace"。Pattern 2 在这个基础上多问一句：**Namespace 里的函数是不是全都应该被外部看到？** 大多数时候答案是"不"——Module 内部往往需要一些只服务于"如何实现"的辅助步骤，这些步骤不应该出现在调用者能触碰到的范围里。Pattern 2 的代码形状就是这个问题的答案：

```lua
local M = {}

local function helper()
end

function M.public_api()
    helper()
end

return M
```

只有两类声明，对应两种截然不同的可见性：

```text
local function -> Module Private
M.xxx           -> Module Public API
```

这就是本文档要展开的全部内容：**Private + Public Module -> Encapsulation**。

## 1. Pattern Definition —— 它是什么

Module 仍然 `return M`，这一点和 Table Module 完全一样——`require` 拿到的还是一张表。区别在于表的内容不再是"Module 里所有的函数"，而是**经过筛选的一部分**：

- `local function helper() ... end` —— 只是这个 `.lua` 文件里的一个局部变量，其作用域到文件结尾为止。它从来没有被写进 `M`，也就从来没有出现在 `require` 返回值上的任何一个字段里。
- `function M.public_api() ... end` —— 等价于 `M.public_api = function() ... end`，是 `M` 这张表上的一个字段，`require` 返回的就是这张表，所以调用者能看到它、能调用它。

Module 边界画在这里：

```text
require("module") 拿到的东西
        |
        v
  +----------------+
  |       M        |   <- Module 的"公开面"，唯一的契约
  |  public_api()  |
  +----------------+

  helper() 这个名字             <- 根本不在上面这张表里
  从物理上就不存在于             调用者拿不到这个引用，
  require 返回值的任何路径上      无法调用、无法探测、无法 hack 到
```

Framework/Caller 拿到 `M` 之后，能做的事情被严格限制在 `M` 这张表暴露的字段上——这不是"建议",是**语言层面的事实**：`helper` 这个标识符在 `local function helper()` 声明之后，只在这个文件剩余的代码里可见，`require` 的调用者根本没有任何语法手段能引用到它。

## 2. Why —— 为什么需要这个 Pattern

如果只用 Table Module（Pattern 1），最直接的做法是把 Module 需要的每一步都写成 `M.xxx`——毕竟能跑起来。问题出现在"这个 Module 的实现需要改变"的时候。

具体的设计问题是：**一旦一个函数出现在 `M` 上，它就自动变成了这个 Module 对外的契约的一部分，无论作者是否愿意。** 调用者看到 `M.parse_value` 是可以调用的，就会有人去调用它——不是因为调用者做错了什么，而是因为"能拿到、能调用"本身就是一种邀请。一旦有代码在别处调用了 `M.parse_value`，Module 作者就不能再随意修改 `parse_value` 的参数形状、返回值形状，甚至不能删除它、把它拆成两个函数——这些原本纯属"内部实现"的自由，全部因为一次不必要的暴露而永久丧失。

Pattern 2 要解决的正是这个问题：**把"这个函数是为了实现某个公开能力而存在的一个步骤"和"这个函数本身就是一个公开能力"区分开**，只让后者出现在 `M` 上。区分之后，Module 作者获得了一个明确的自由声明：

```text
M 上的字段        -> 承诺："只要参数/返回值形状不变，我不会无预警破坏你"
local function    -> 声明："这只是我实现公开能力的某一种方式，我随时可能换掉它"
```

## 3. Architecture —— 内部结构与可见性边界

四层里，Pattern 2 涉及 Application/Framework（Caller）与 Module 两层，Module 内部又进一步分成两块：

```text
                     Application / Framework (Caller)
                                  |
                                  | 只持有 require("module") 返回的 M
                                  v
                    +--------------------------------+
                    |          Module (M)            |
                    |                                |
                    |   Public API                   |
                    |   M.public_api()                |
                    |        |                        |
                    |        v                        |
                    |   Private Implementation        |
                    |   local function helper()       |
                    |                                |
                    +--------------------------------+
```

图中最关键的一条边是 `Public API -> Private Implementation`——这条箭头只能单向存在：

```text
Public API
     |
     v
Private Implementation
```

Public API 可以调用 Private Implementation（`M.public_api` 内部调用 `helper`），反过来永远不成立：Private Implementation 不知道、也不需要知道是哪个 Public API 调用了自己,更不可能反过来调用一个还没被定义、或者根本不属于这个 Module 的 Public API。这条单向箭头就是"实现细节服务于公开契约,而不是反过来"的架构表达。

**为什么这是真正的封装，而不是一种约定？** 对比一下另一种语言里常见的做法——Python 里用下划线前缀 `_private_helper` 表示"这是私有的,请不要调用"。那只是一个**命名约定**：语言本身完全允许外部代码写 `module._private_helper()`,能不能这么做全靠调用者自觉遵守团队规范,IDE 可能给个警告,但运行时不会阻止。

Lua 这里不同。`local function helper()` 里的 `local` 是**词法作用域（lexical scoping）**的声明,`helper` 这个名字的可见范围在编译期就已经确定,只到这个 `.lua` 文件结束为止。它从来没有被赋值给 `M` 的任何字段,`require` 拿到的返回值里根本不存在一条能走到 `helper` 的路径。这不是"建议调用者不要访问",这是**调用者在语法上不可能访问到**——不存在 `M._helper`、不存在任何字符串索引能摸到它,因为它压根不是任何表的字段,只是这个文件执行时留在闸包（closure）里的一个上值（upvalue）。

```text
Python 的 _private            Lua 的 local function
        |                              |
        v                              v
   命名约定                        词法作用域规则
   （运行时不阻止,                  （运行时无法访问,
    全靠自觉遵守）                   语法层面不存在路径）
```

## 4. Lifecycle —— require 时发生了什么

`load -> require -> initialize -> use -> state change -> shutdown` 对应到 Pattern 2：

1. **load**：Lua 加载 `module.lua` 的源码。
2. **require**：`require("module")` 第一次被调用时,整个文件从头执行一遍——`local M = {}` 创建表,`local function helper` 创建一个闭包并绑定到当前文件的局部变量,`function M.public_api` 把另一个闭包写进 `M` 的字段,最后 `return M` 把这张表交回去,同时 Lua 把这张表存进内部的模块缓存。
3. **initialize**：如果 Module 需要"Module 级 State"（下面第 7 节展开）,这一步通常也在文件顶层完成——例如声明一个空的 `local cache = {}`,这个 `cache` 会被 `helper` 和 `M.public_api` 共同闭包捕获。
4. **use**：之后每一次 `require("module")` 都直接从缓存拿到同一张 `M`,不会重新执行文件——这意味着 `helper`、`M.public_api`、以及可能存在的 `cache`,在整个进程生命周期里只被创建一次,被所有调用者共享。
5. **state change**：调用者调用 `M.public_api()`,如果这个 Module 持有 State,某次调用可能会读写这个共享的 State（比如更新 `cache` 里的一条记录）。
6. **shutdown**：进程结束,Module 及其 State 随进程一起消失,Lua 没有显式的"卸载 Module"步骤。

需要特别强调第 3 步——这里只做一次**foreshadow（预告）**,不展开讲:Pattern 2 的 Module **可以**持有 Module 级 State（例如一个 Parser 内部的 token 缓存、一个 Service 的内存数据表）,这个 State 在 `require()` 第一次执行文件时创建,之后被所有调用者共享,不属于"某一次调用"。这个 State 具体如何被创建、如何被多个调用者并发共享、和 Class Module 里"每个实例独立 State"有什么本质区别——完整的对比留给横向整合文档 [06-require-and-state-mental-model.md](../06-require-and-state-mental-model.md) 处理,本文档不重复展开,只在本 Pattern 内部按需提及（第 7 节、Example 2）。

## 5. Relationship —— 依赖方向

只有一个方向,且只存在一层依赖:

```text
Caller -> M (Public API)
```

`Caller` 依赖 `M` 暴露出来的字段,`M` 的 Public API 内部依赖 Private Implementation,但 Private Implementation **不反向依赖** Caller,也不知道 Caller 是谁:

```text
Caller
  |
  v
Public API  (M.public_api)
  |
  v
Private Implementation  (helper)
```

这条链不能反过来读。如果哪天发现 Private Implementation 里的某个 `local function` 需要知道"是谁在调用我"才能正常工作,这通常是一个信号——说明这部分逻辑其实不应该是纯粹的内部步骤,可能需要把调用者关心的信息作为参数显式传进来,而不是让 Private Implementation 反向打探 Caller 的身份。

## 6. Data / Control Flow —— 一次调用的完整路径

以核心代码形状为例,画出一次 `M.public_api()` 调用的完整路径:

```text
Caller
  |
  |  M.public_api()
  v
M.public_api  ---------------------------+
  |                                      |
  |  内部调用 helper()                    | (这一步 Caller 看不到,
  v                                      |  也不需要知道)
helper                                   |
  |                                      |
  |  返回结果给 M.public_api              |
  v                                      |
M.public_api  <--------------------------+
  |
  |  return 结果
  v
Caller
```

Caller 只感知到"调用了 `M.public_api()`,拿到了一个结果"——中间是否调用了 `helper`、调用了几次、`helper` 内部又调用了别的 `local function`,这些全部是这次调用路径上对 Caller 不可见的部分。这正是"隐藏实现步骤"在控制流图上的样子:**外部只看到一进一出,内部可以是任意复杂度的调用链**。

## 7. State —— Module 级 State 归谁

Pattern 2 的 State 是**可选的**,与 Table Module（通常无 State）的核心区别在这里——一旦一个 Table Module 开始持有可变 State,它在语义上已经在向 Pattern 2 演化。

如果 Pattern 2 的 Module 持有 State（比如一个 Parser 的缓存、一个 Service 的内存表）,这个 State 具备三个特征:

```text
State 的归属                 State 的创建时机              State 的可见范围
     |                             |                            |
     v                             v                             v
属于整个 Module,           require() 第一次执行             对所有调用者共享,
不属于某一次调用             文件时创建一次                    不是每个调用者独立一份
```

State 通常声明为文件顶层的 `local` 变量,和 `helper` 一样是这个文件的一个上值——`M.public_api` 和其它 Public API 函数因为都在同一个闭包环境里定义,所以能共同读写这同一份 State,但 Caller 拿到的 `M` 上不会有任何字段直接暴露这份 State 本身,Caller 只能通过 Public API 间接地读写它（例如通过 `M.create` 间接往内部表里插入一条记录,通过 `M.find` 间接读出一条记录）。

State 谁拥有、谁修改：**Module 自己拥有,Module 的 Public API 是唯一被允许修改它的代码路径。** Private Implementation 可以读写它（因为在同一个闭包里）,但只应该在被某个 Public API 调用的过程中才这么做——这依然是第 3 节那条"Public API -> Private Implementation"单向箭头的体现,State 的修改权限沿着同一条箭头传递,不会绕开 Public API 直接从外部触达。

这里再次只做 foreshadow：这份 State 在多个调用者之间到底是怎么"共享"的（是不是线程安全、`require` 缓存的具体机制、和 Class Module 每个对象独立 State 的对比表）,完整讨论见 [06-require-and-state-mental-model.md](../06-require-and-state-mental-model.md)。

## 8. Failure —— 失败模式与谁负责恢复

先排除一个不存在的失败模式：**从外部误用 Private 实现细节** 这件事在 Pattern 2 里根本不会发生——第 3 节已经说明,`helper` 这个名字在语法层面就不存在于 `M` 上,外部代码没有任何手段"误用"一个它连引用都拿不到的东西。这是 Pattern 2 相对于"只靠命名约定"的方案在 Failure 这一维度上的直接优势：**少了一整类原本可能发生的失败模式**。

真正会发生的失败模式在 Module **内部**：Private Implementation 被 Public API 用一个不符合预期的参数调用。举例:

```text
Caller 传入畸形数据
        |
        v
M.public_api(bad_input)   <- 入口:必须在这里做输入校验
        |
        | 如果 M.public_api 没有先校验就直接往下传
        v
helper(bad_input)          <- helper 假设参数已经是合法的
        |
        v
运行时错误 / 产生错误结果   <- helper 自己不做防御性校验
```

这里的责任划分是明确的：**Public API 负责在调用 Private Implementation 之前完成输入校验,Private Implementation 有权假设自己收到的参数已经是合法的。** 这不是"helper 应该更健壮一点"的问题,而是一个有意的设计取舍——如果每一个 `local function` 都要重新校验一遍"我的参数真的合法吗",校验逻辑会在调用链上重复好几层,而且 Private Implementation 本来就只应该被同一个文件里的 Public API 调用,校验一次就足够覆盖所有到达它的路径。

对应到本文档后面三个 Example：`user_service.lua` 里 `validate()` 在 `M.create()` 内部第一个被调用,`normalize()`、`generate_id()` 完全不再重复检查 `input.name`/`input.email` 的合法性;`parser.lua` 里 `parse_value`/`parse_object` 也从不检查"这个 token 数组是不是本来就有效",因为它们只能通过已经做过基本校验的 `M.parse` 触达。谁发现失败：Public API 内部的校验逻辑。如何传播：一般走 Lua 里"返回 `nil, err`"的惯用法,而不是 `error()` 抛出异常（除非是内部真正不该发生的不变量被打破,那种情况允许用 `error()`,因为那已经是 Module 自身的 bug,不是调用者的错误输入）。谁负责恢复：Caller 拿到 `nil, err` 后决定怎么处理,Module 自身不擅自决定"校验失败了要不要重试"。

## 9. Mental Model —— 一句话记忆

**一句话**：`M` 是契约,`local function` 是自由——契约不能轻易改,自由随时可以改。

**核心问题**：这个函数是"Module 对外承诺的能力",还是"Module 这次选择用来实现承诺的其中一种手段"？

**核心关系**：

```text
local function -> Module Private
M.xxx          -> Module Public API
```

```text
Private + Public Module
    -> Encapsulation
```

## 10. Design Trade-offs —— 优点/代价/何时不要用

**优点**：

- Module 作者获得了"随时重写内部实现"的自由,只要 Public API 的参数/返回值形状不变,内部怎么重构都不会破坏任何调用者。
- 调用者不会被内部实现的复杂度打扰——只需要理解 `M.parse`/`M.create`/`M.encode` 这几个入口,不需要理解 `tokenize`/`normalize`/`encode_header` 内部怎么写。
- 封装是语言机制级别的,不依赖任何人自觉遵守命名约定,不存在"有人手滑调用了不该调用的函数"这种失败模式。

**代价**：

- 需要 Module 作者在写代码之前先做一次判断——"这个函数到底该不该暴露",这比"全部写成 `M.xxx` 图省事"多了一步决策成本。
- 调试时如果确实需要临时观察某个 Private 函数的行为,不能像 Public API 那样直接从外部 `require` 之后调用,需要在文件内部临时加日志或者临时改成 Public 再改回去。

**什么时候不要用**：如果 Module 本身就是一组无状态的纯工具函数,内部没有任何值得隐藏的"实现步骤"（例如一组数学函数,每个都独立、互不调用）,直接用 Table Module（Pattern 1）就够了,强行拆出 Private 部分只会增加不必要的文件内部结构。

**与相邻 Pattern 的区别**：和 Table Module 的区别是"是否存在需要隐藏的内部步骤";和 Class/Constructor Module 的区别是 State（如果有）的归属——Pattern 2 的 State 属于整个 Module,被所有调用者共享,Class Module 的 State 属于每一个独立创建的对象,互不干扰。当发现"我需要给每个调用者一份独立的、互不干扰的 State"时,说明该往 Pattern 3 演化了,继续留在 Pattern 2 会导致所有调用者的数据混在同一张共享表里。

## Anti-pattern：Everything Public

反面例子用 Example 1 的 JSON Parser 来演示。假设作者偷懒,把内部的每一步都直接挂到 `M` 上:

```lua
-- 反面示例：不要这样写
local M = {}

function M.tokenize(str)
    -- ... 把字符串切成 token 数组
end

function M.parse_value(tokens, pos)
    -- ... 根据当前 token 类型分派
end

function M.parse_object(tokens, pos)
    -- ... 解析一个 { ... } 对象
end

function M.parse(str)
    local tokens = M.tokenize(str)
    local value = M.parse_value(tokens, 1)
    return value
end

return M
```

某个调用者一开始只是想"debug 一下,看看 token 长什么样",于是这么写:

```lua
local parser = require("parser")

-- 调用者绕开了 parse(),直接拿 tokenize 的结果自己处理
local tokens = parser.tokenize(raw_string)
for _, tok in ipairs(tokens) do
    if tok.type == "STRING" then
        -- 调用者开始基于 token 的内部结构做自己的逻辑
        process_string_token(tok.value)
    end
end
```

这段调用者代码一旦合并进主分支,后果是具体且立刻发生的：

- **调用者被耦合到内部算法结构上**——`process_string_token` 依赖的是 `tokenize` 产出的 token 形状（`{type=..., value=...}`）,这本来只是 Parser 内部"如何一步步识别语法"的实现细节,现在变成了调用者代码要依赖的东西。
- **Module 作者失去了更换分词策略的自由**——哪天作者想把 `tokenize` 从"一次性生成完整 token 数组"改成"流式、按需生成 token"（性能优化,常见的重构方向),`tokens` 的形状、甚至“是否存在一个数组”这件事本身都可能变,而这原本应该是纯内部决策——现在因为有调用者直接依赖了 `M.tokenize` 的返回值形状,这次重构变成了一次**破坏性变更（breaking change）**,必须先找出所有直接调用 `M.tokenize` 的地方,一个一个确认改动是否影响它们。
- **Module 的"实际契约"变得模糊**——按照文档,这个 Parser 的契约应该只是"给我一个字符串,还我解析结果"（`parse`/`validate`）;但因为 `tokenize`/`parse_value`/`parse_object` 也挂在 `M` 上、也被调用者用过,Module 的**实际**契约变成了"以上所有函数,包括它们的参数形状、返回值形状、调用顺序",没有人能再简单地回答"这个 Module 到底承诺了什么"。

对比本文档的做法（`01-example-json-parser.md` 里的实现）：只有 `M.parse`、`M.validate` 存在,`tokenize`/`parse_value`/`parse_object` 是 `local function`,调用者在语法上就不可能写出 `parser.tokenize(...)`——如果确实需要观察 token,只能通过修改 `parser.lua` 这个文件本身临时加日志,而不能从调用方悄悄伸手进 Module 内部。这正是本文档反复强调的一句话的落地：**契约不能轻易改,自由随时可以改**——Everything Public 把本应保持自由的部分,变成了谁都能依赖、谁都不能轻易改的契约。
