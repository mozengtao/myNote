# 09 · 五 Pattern 横向对比表 + Anti-pattern 汇总

> 前置阅读：五个 Pattern 的 `00-why-and-architecture.md`。本文是速查表，不重复展开论证——每条结论背后的完整推理都能在对应 Pattern 文档里找到。

## 1. 最终横向对比表

| Pattern | Public API | Private Impl | Object | State | Factory | Extension | Dependency Direction |
|---|---|---|---|---|---|---|---|
| Table | ✓ | optional | - | optional | - | - | `Caller -> Module` |
| Private/Public | ✓ | ✓ | - | optional | - | - | `Caller -> API` |
| Class | ✓ | ✓ | ✓ | ✓ | optional | - | `Caller -> Type` |
| Factory | ✓ | ✓ | ✓ | optional | ✓ | - | `Caller -> Abstraction` |
| Plugin | Contract | ✓ | optional | optional | optional | ✓ | `Framework -> Contract <- Plugin` |

下面按行展开，每一格给出一句具体解释，而不是简单的勾选。

## 2. 逐行解释

### Table Module

- **Public API = ✓**：整张返回的 `M` 表就是公开面，没有"公开/私有"的区分层级。
- **Private Impl = optional**：可以用 `local function` 做内部工具函数，但这一步做了之后，这个 Module 在语义上已经开始向 Pattern 2 演化（见 [01-table-module/00-why-and-architecture.md](01-table-module/00-why-and-architecture.md) 第 10 节）。
- **Object = -**：不创建实例，`M` 本身是唯一的、单例的。
- **State = optional，但不推荐**：一旦引入可变字段，就变成了所有调用者共享的隐式全局状态，这正是本 Pattern 的 Anti-pattern。
- **Factory = -**：没有"创建谁"这个决策要做，因为根本不创建对象。
- **Extension = -**：没有 Contract、没有第三方实现的空间，`M` 是封闭的一整份 API。
- **Dependency Direction**：`Caller -> Module`——单向、静态、编译期就能确定，运行时不会变化。

### Private + Public

- **Public API = ✓**：`M.xxx` 形式的函数，是这个 Module 对外的正式契约。
- **Private Impl = ✓**：`local function` 因为 Lua 词法作用域，外部**物理上不可达**，这是真正的封装，不是命名约定。
- **Object = -**：依然是单例 Module，不创建多个独立实例（如果需要，说明该往 Pattern 3 走了）。
- **State = optional**：可以有 Module 级共享状态（例如一份缓存），归属和生命周期见 [06-require-and-state-mental-model.md](06-require-and-state-mental-model.md)。
- **Factory = -**：没有"选择具体实现"的问题，只有"公开契约 vs 隐藏步骤"的问题。
- **Extension = -**：契约是这个 Module 自己定的，不期望被第三方实现替换。
- **Dependency Direction**：`Caller -> API`——调用者只应该依赖 `M.xxx`，绝不应该依赖任何 `local function` 的存在或行为。

### Class / Constructor

- **Public API = ✓**：`Class:method()` 形式的实例方法，是对象对外的契约。
- **Private Impl = ✓**：可以在 `Class` 内部再放 `local function` 作为多个方法共用的私有辅助逻辑,与 Pattern 2 的封装机制完全一致，只是宿主变成了 Class 而不是普通 Module。
- **Object = ✓**：这是这个 Pattern 的核心——`.new()` 每次调用产出一个独立实例。
- **State = ✓**：State 属于每个实例（Object State），必须写在实例表上，不能写在 Class Table 上（否则就是本 Pattern 的 Anti-pattern）。
- **Factory = optional**：简单场景直接 `Class.new(...)`；如果"到底 new 哪一个具体 Class"要运行时才决定，就该引入 Pattern 4。
- **Extension = -**：Class 本身不是给第三方实现的契约，它是一个具体类型。
- **Dependency Direction**：`Caller -> Type`——调用者依赖这个具体的 `Class`（知道它的名字、`.new()` 签名），这也是它和 Factory/Plugin 依赖方向的本质区别。

### Factory

- **Public API = ✓**：`factory.create(kind, config)` 是唯一的对外入口。
- **Private Impl = ✓**：`kind -> 构造函数` 的映射表通常是内部细节，不必暴露。
- **Object = ✓**（间接）：Factory 自己不是对象，但它创建的产品几乎总是 Pattern 3 意义下的对象——这一列的"✓"属于"Factory 产出的东西"，不是 Factory 自己。
- **State = optional（通常无）**：Factory 本身几乎总是无状态的查表转发逻辑，State 转移到了产品对象身上（详见 [04-factory/00-why-and-architecture.md](04-factory/00-why-and-architecture.md) 第 7 节）。
- **Factory = ✓**：这正是这个 Pattern 的定义性特征。
- **Extension = -**：新增实现通常仍需要修改 Factory 内部的映射表（除非进一步演化成运行时 Discovery/Register，那就是 Pattern 5 了）。
- **Dependency Direction**：`Caller -> Abstraction`——调用者依赖的是"统一接口长什么样"，不依赖具体是 TCP 还是 UDP。

### Plugin

- **Public API = Contract**：这里特别标成 "Contract" 而不是普通的 "✓"，因为 Plugin 的"公开面"不是某一个具体模块的具体函数列表，而是**一个所有 Plugin 都必须满足的形状约定**（例如都要有 `name`/`can_handle`/`process`）。
- **Private Impl = ✓**：每个具体 Plugin 内部怎么实现 `process()`，对 Framework、对其它 Plugin 都是不透明的。
- **Object = optional**：Plugin 可以是一个单例 Table（最常见），也可以内部再用 Class 模式管理自己的子对象（例如一个连接池）。
- **State = optional**：每个 Plugin 可能有自己的内部状态，互相独立，也和 Framework State 分离（详见 [06-require-and-state-mental-model.md](06-require-and-state-mental-model.md) 第 2 节）。
- **Factory = optional**：Plugin 内部经常会用到 Factory（见 [08-pattern-composition-and-real-world-cases.md](08-pattern-composition-and-real-world-cases.md) 的调用链示例），但这不是 Plugin Pattern 本身的必需品。
- **Extension = ✓**：这是这个 Pattern 的定义性特征——Framework 可以在不修改自身源码的前提下，通过新增满足 Contract 的文件来获得新能力。
- **Dependency Direction**：`Framework -> Contract <- Plugin`——双方各自依赖同一个契约，互相之间没有直接箭头，这是全表里唯一一个"双箭头指向中间"的依赖方向，也是 Dependency Inversion 在这张表里的具体体现。

## 3. Anti-pattern 汇总

五个 Pattern 各自的完整 Anti-pattern 论证都在对应 `00-why-and-architecture.md` 的末尾，这里只做一句话复盘，方便快速定位"我现在写的代码是不是正在犯这个错误"：

| Pattern | Anti-pattern 一句话 | 完整论证 |
|---|---|---|
| Table Module | 把可变 State（`M.state`）、"伪私有"字段（`M.private_helper`）直接混进公开的 `M` 表 | [01-table-module/00-why-and-architecture.md](01-table-module/00-why-and-architecture.md#anti-pattern) |
| Private + Public | 把本该是内部步骤的函数（`tokenize`/`parse_value`）也挂到 `M` 上公开出去 | [02-private-public/00-why-and-architecture.md](02-private-public/00-why-and-architecture.md#anti-patterneverything-public) |
| Class / Constructor | 把 Object State 误写到 Class Table 上（`User.name = "global"`），导致所有实例意外共享 | [03-class-constructor/00-why-and-architecture.md](03-class-constructor/00-why-and-architecture.md#anti-pattern) |
| Factory | Application 直接 `require` 并 `.new()` 每一个具体实现，自己写 `if/elseif` 选择逻辑 | [04-factory/00-why-and-architecture.md](04-factory/00-why-and-architecture.md#anti-pattern) |
| Plugin | Framework 内部用 `if plugin_name == "http" then ... elseif ...` 按名字分支，而不是走统一 Contract | [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md#anti-pattern) |

**五个 Anti-pattern 的共同本质**：都是"把本该分离的两层职责压缩进同一层"——State 和 API 压缩进同一张表（Table Module）；契约和实现步骤压缩进同一份公开列表（Private+Public）；共享数据和实例数据压缩进同一张表（Class）；选择逻辑压缩进调用方（Factory）；扩展点压缩进 Framework 自身的分支逻辑（Plugin）。判断"我是不是正在犯 Anti-pattern"的通用问题只有一个：**这两件本该独立变化的东西，现在是不是被绑在了一起、改一个必须动另一个？**

下一篇 [10-pattern-decision-tree.md](10-pattern-decision-tree.md) 把这张对比表转成一个可以直接套用的决策流程。
