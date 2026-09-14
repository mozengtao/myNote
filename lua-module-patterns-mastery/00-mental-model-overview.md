# 00 · 统一 Mental Model —— Module 到底在承担什么架构职责

> 本文件是整套笔记的地基。后面 5 个 Pattern（`01-table-module` ~ `05-plugin`）以及横向整合文档（`06` ~ `11`）里反复出现的词汇——Namespace / Encapsulation / Object State / Implementation Hiding / Contract / Dependency Inversion——都在这里统一定义一次，后文只引用，不重复解释。

## 学习目标

这份笔记要建立的不是"Lua Module 有哪些写法"的语法清单，而是一个可以迁移到任何工程（C/C++/Go/Rust 一样适用）的 **Module Architecture Mental Model**，学完之后，看到一个陌生的 Lua Module，能立刻回答：

- 这个 Module 的职责是什么？
- 为什么采用这种 Module 组织方式？
- Module 对外暴露什么？内部隐藏什么？
- 谁创建 Module？谁调用 Module？
- Module 是否持有 State？State 归谁？
- Framework 与 Module 的依赖方向是什么？
- 如果实现发生变化，谁需要修改？如果要扩展新实现，应该改哪里？
- 这个 Pattern 的主要优点和代价是什么？

## 学习顺序（所有后续文档统一遵守）

```text
WHY
 |
 v
ARCHITECTURE
 |
 v
LIFECYCLE
 |
 v
RELATIONSHIP
 |
 v
DATA / CONTROL FLOW
 |
 v
STATE
 |
 v
FAILURE
 |
 v
MENTAL MODEL
 |
 v
DESIGN TRADE-OFFS
 |
 v
EXAMPLES (>= 3)
 |
 v
ANTI-PATTERN
```

不按 Lua 官方文档章节罗列语法，而是先回答"为什么需要这种组织方式"，再看架构图、生命周期、依赖关系，最后才落到具体代码。每个 Pattern 的 `00-why-and-architecture.md` 都严格按这个顺序写。

## 1. 五个 Pattern 是同一个问题的五种答案

这五个 Pattern 回答的都是同一个问题：**"一段 Lua 代码应该以什么形状组织，才能匹配它在架构里承担的职责？"** 职责不同，Module 的形状就不同：

| Pattern | Module 本质 | 核心设计思想 | 主要解决的问题 | State | 典型场景 |
|---|---|---|---|---|---|
| Table Module | API Table | Namespace / API | 组织相关函数和常量 | 通常无 | Utility、Protocol 常量、Config Helper |
| Private + Public | API + Hidden Implementation | Encapsulation | 隐藏实现细节，防止调用者依赖内部步骤 | 可选（Module 级） | Parser、Service、Library |
| Class / Constructor | Object Type | Object / State | 创建多个拥有独立状态的对象 | 每个对象独立 | Connection、Device、Session |
| Factory | Creation Abstraction | Implementation Hiding | 隐藏"具体创建哪个实现"这一决策 | 通常由产品对象持有，Factory 本身通常无状态 | Client、Backend、Driver、Logger |
| Plugin | Contract Implementation | Extension / Dependency Inversion | 让 Framework 能被外部模块扩展，且不需要修改自身代码 | 可选（Plugin 级） | Command Plugin、Protocol Handler、Dissector |

对每一行都追问一次"为什么是这个 State 列"：

- **Table Module 通常无 State**——它提供的是无状态的纯函数集合（`split`、`trim`），或者只读常量（HTTP 方法名）。一旦引入可变 State，它就已经在向下一个 Pattern 演化。
- **Private + Public 的 State 是可选的、Module 级的**——如果 Module 需要缓存（例如 Parser 内部的 token 缓存），State 属于整个 Module，被所有调用者共享，不属于某一次调用。
- **Class / Constructor 的 State 属于每一个实例**——`alice` 和 `bob` 是两个完全独立的 `User` 对象，State 互不干扰,这是它与前两个 Pattern 最本质的区别。
- **Factory 本身几乎不持有 State**——它只是"选择哪个构造函数"的一次性决策逻辑，State 转移到了它创建出来的产品对象身上。
- **Plugin 的 State 属于每个插件自己**——Framework 不应该、也不需要知道某个插件内部维护了什么数据。

## 2. 五个 Pattern 的演进关系

```text
Table Module
    |
    | "我只需要提供一组 API"
    v
Private + Public
    |
    | "API 背后存在需要隐藏的实现"
    v
Class / Constructor
    |
    | "我需要创建多个拥有独立 State 的对象"
    v
Factory
    |
    | "我不希望调用者知道具体创建哪个实现"
    v
Plugin
    |
    | "我希望第三方/外部模块实现 Framework 定义的能力"
    v
Extension Architecture
```

**必须强调的误解澄清**：这条链不是严格的技术难度升级路线，不是说 Plugin "比 Table Module 更高级"。一个项目里，`http.lua`（Table Module，装 HTTP 常量）和 `plugins/dns.lua`（Plugin，实现协议解析）可以是平级的、同时存在的两个文件——没有谁取代谁。这条链表示的是：**当 Module 需要承担的架构职责逐渐变化（从"我提供 API"到"我允许被扩展"），它的组织形状也必须随之改变**。选错 Pattern 不是"写法落后",而是"形状和职责不匹配",后续会在每个 Pattern 的 Anti-pattern 小节具体展示这种不匹配会导致什么问题。

对应关系表（贯穿全文档的核心映射）：

```text
Table Module
    -> Namespace / API

Private + Public Module
    -> Encapsulation

Class / Constructor Module
    -> Object / State

Factory Module
    -> Abstraction / Implementation Hiding

Plugin Module
    -> Extension / Dependency Inversion
```

## 3. 统一的五层 Mental Model

五个 Pattern 最终可以折叠成三条纵深不同的关注点——**API / Object / Extension**——Factory 横跨在 Object 和 Extension 之间，专门负责"隐藏创建过程"这一件事：

```text
                     Module Design
                           |
         +-----------------+-----------------+
         |                 |                 |
        API             Object            Extension
         |                 |                 |
         v                 v                 v
     Namespace           State           Contract
         |                 |                 |
         v                 v                 v
   Table Module      Class Module          Plugin
         |
         v
   Encapsulation
         |
         v
   Private/Public
```

Factory 不在这张主链上，因为它不是"新增一种 Module 形状"，而是"决定该实例化哪一个 Class / 该加载哪一个 Plugin"的中间层，画出来是这样：

```text
Application
     |
     v
 Abstraction
     ^
     |
Implementation
```

Factory 站在 Abstraction 和 Implementation 之间，两边都不知道对方的存在——Application 只认识 Abstraction（统一接口），Implementation（TcpClient / UdpClient）只需要满足这个接口，不需要知道 Application 是谁。这就是 **Dependency Inversion** 在 Module 组织层面的样子，07 号文档会展开讲这三种依赖方向的完整对比。

## 4. 统一分析框架（每个 Pattern 必须遵守）

每个 `NN-pattern/00-why-and-architecture.md` 都严格按以下 10 步分析，缺一不可：

1. **Pattern Definition** —— 它是什么？Module 返回什么？Framework/Caller 得到什么？Module 边界在哪里？
2. **Why** —— 为什么需要这种 Pattern？必须回答"它解决的具体设计问题"，不能只说"比较方便"。
3. **Architecture** —— ASCII 架构图，明确画出 Application / Framework / Module / Implementation 四层中哪些层存在。
4. **Lifecycle** —— `load -> require -> initialize -> use -> state change -> shutdown`，逐条回答 `require()` 做了什么、Module 是否只初始化一次、State/Object/Plugin 分别在什么时候创建。
5. **Relationship** —— 明确依赖方向：`Caller -> Module` 还是 `Framework -> Contract <- Plugin`。
6. **Data / Control Flow** —— 至少一次完整调用流程图。
7. **State** —— Module 有没有 State？放在哪里？共享还是独立？谁拥有、谁修改、生命周期是什么？
8. **Failure** —— 典型失败方式、谁发现、如何传播、谁负责恢复。
9. **Mental Model** —— 一句话记忆 + 核心问题 + 核心关系。
10. **Design Trade-offs** —— 优点、缺点、什么时候不要用、与相邻 Pattern 的区别。

紧接着是 **3 个完整可运行的 Example**（每个都包含 Problem / Architecture / Framework+Module / 完整代码 / 执行流程 / State / 为什么适合这个 Pattern / 换成其他 Pattern 会怎样 / Mental Model），最后是至少 1 个 **Anti-pattern**。

## 5. 导航

| 文档 | 内容 |
|---|---|
| [01-table-module/](01-table-module/00-why-and-architecture.md) | Pattern 1：Table Module —— Namespace / API |
| [02-private-public/](02-private-public/00-why-and-architecture.md) | Pattern 2：Private + Public —— Encapsulation |
| [03-class-constructor/](03-class-constructor/00-why-and-architecture.md) | Pattern 3：Class / Constructor —— Object / State |
| [04-factory/](04-factory/00-why-and-architecture.md) | Pattern 4：Factory —— Implementation Hiding |
| [05-plugin/](05-plugin/00-why-and-architecture.md) | Pattern 5：Plugin —— Extension / Dependency Inversion |
| [06-require-and-state-mental-model.md](06-require-and-state-mental-model.md) | `require()` 运行时模型 + Module/Object/Framework/Plugin State 对比 |
| [07-dependency-direction.md](07-dependency-direction.md) | 直接依赖 / Factory / Plugin 三种依赖方向对比 + Dependency Inversion |
| [08-pattern-composition-and-real-world-cases.md](08-pattern-composition-and-real-world-cases.md) | Pattern 组合 Mental Model + 3 个真实工程案例 |
| [09-cross-pattern-comparison.md](09-cross-pattern-comparison.md) | 五 Pattern 横向对比表 + Anti-pattern 汇总 |
| [10-pattern-decision-tree.md](10-pattern-decision-tree.md) | Pattern 选择决策树 |
| [11-final-mental-model.md](11-final-mental-model.md) | 最终统一心智模型 + 五句话记忆 + Code Reading Checklist |

> 阅读建议：先读完这份 00，再按 01 -> 02 -> 03 -> 04 -> 05 顺序把五个 Pattern 各读一遍（每个 Pattern 内先读 `00-why-and-architecture.md` 再看 3 个 Example）；之后读 06 -> 07 -> 08 -> 09 -> 10 -> 11 把知识横向打通。之后可以只把 00/09/10/11 当速查表用。
