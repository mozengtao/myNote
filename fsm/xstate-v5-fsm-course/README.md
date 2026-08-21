# XState v5 FSM 建模课

这不是一份 API 手册，是一套**有限状态机（FSM）建模思维**训练课，用 XState v5 作为练习工具。

## 学习目标

学完之后，你应该能：

- 读懂任何一份 XState v5 Machine 定义
- 独立把一个真实需求拆成 State / Event / Context
- 写出没有"垃圾状态"（State 爆炸）的高质量状态机
- 看懂官方文档，而不是每次都要查 Stack Overflow
- 看懂 XState 源码/类型定义里常见的 TypeScript 写法

**不是**目标：让你成为 TypeScript 专家。第六部分只挑 XState 需要的 9 个语法点，够用就停。

## 为什么优先级是 FSM 思维 > XState API > TypeScript 语法

- API 会变（v4 → v5 已经变过一次），**建模思维不会变**。
- 你以后即使换成 SCXML、Boost.SML，或者自己手写一个状态机，这套思维一样适用。
- TypeScript 在这里只是"帮你在编译期发现建模错误的工具"，不是学习目标本身。

## 怎么用这套课程

1. **严格按顺序读，不要跳课。** 后面的知识点默认你已经理解前面的。比如第 5 课 Context 之前，所有例子都不会出现 `context`；第 6 课之前不会出现 `entry`/`exit`。这是故意的——一次只加一个概念。
2. **每课都有 12 个固定栏目**，读的时候按顺序走：
   1. 为什么需要
   2. 核心思想（一句话）
   3. ASCII 图
   4. JS 思维（先不用 TS）
   5. XState v5 写法（完整代码）
   6. TypeScript 语法拆解
   7. 常见错误
   8. 为什么官方这样设计
   9. 最佳实践
   10. 一句话总结
   11. 设计思考（Design Thinking）：同一个需求，方案 A vs 方案 B，为什么 A 对 B 错
   12. 设计原则（Design Rules）：本课新增的原则，会汇总进 [`DESIGN-RULES.md`](DESIGN-RULES.md)
3. **代码都要亲手敲一遍再往下走。** 光看不敲，建模直觉建立不起来。
4. **读完全部课程后，回头把 [`DESIGN-RULES.md`](DESIGN-RULES.md) 通读一遍。** 那份文件才是你真正要带走的东西，不是某个 API 怎么调。

## 课程目录

### 第一部分：FSM 核心概念

| # | 课程 | 文件 |
|---|------|------|
| 01 | Machine：状态机的"总图" | [lessons/01-machine.md](lessons/01-machine.md) |
| 02 | State：为什么需要"当前模式" | [lessons/02-state.md](lessons/02-state.md) |
| 03 | Event：驱动状态变化的唯一入口 | [lessons/03-event.md](lessons/03-event.md) |
| 04 | Transition：状态怎么变 | [lessons/04-transition.md](lessons/04-transition.md) |
| 05 | Context：State 装不下的数据放哪里 | [lessons/05-context.md](lessons/05-context.md) |

### 第二部分：状态机行为

| # | 课程 | 文件 |
|---|------|------|
| 06 | Action：副作用什么时候执行 | [lessons/06-action.md](lessons/06-action.md) |
| 07 | Guard：为什么不能用 if | [lessons/07-guard.md](lessons/07-guard.md) |
| 08 | after：定时器也是 Event | [lessons/08-after.md](lessons/08-after.md) |
| 09 | always：什么时候自动跳转 | [lessons/09-always.md](lessons/09-always.md) |

### 第三部分：复杂状态

| # | 课程 | 文件 |
|---|------|------|
| 10 | Initial：为什么必须有起点 | [lessons/10-initial.md](lessons/10-initial.md) |
| 11 | Nested State：为什么要拆层 | [lessons/11-nested-state.md](lessons/11-nested-state.md) |
| 12 | Parallel State：什么时候需要并行 | [lessons/12-parallel-state.md](lessons/12-parallel-state.md) |

### 第四部分：Actor Model

| # | 课程 | 文件 |
|---|------|------|
| 13 | Actor 思想：流程 vs 工作 | [lessons/13-actor-model.md](lessons/13-actor-model.md) |
| 14 | invoke：把工作委托出去 | [lessons/14-invoke.md](lessons/14-invoke.md) |
| 15 | fromPromise / fromCallback：两种常见工作类型 | [lessons/15-frompromise-fromcallback.md](lessons/15-frompromise-fromcallback.md) |
| 16 | spawn：动态创建的 Actor | [lessons/16-spawn.md](lessons/16-spawn.md) |

### 第五部分：XState v5 架构

| # | 课程 | 文件 |
|---|------|------|
| 17 | 生命周期：从 setup() 到 Snapshot | [lessons/17-lifecycle.md](lessons/17-lifecycle.md) |

### 第六部分：TypeScript（仅学习 XState 所需）

| # | 课程 | 文件 |
|---|------|------|
| 18 | Object | [lessons/18-object.md](lessons/18-object.md) |
| 19 | Union | [lessons/19-union.md](lessons/19-union.md) |
| 20 | Literal Type | [lessons/20-literal-type.md](lessons/20-literal-type.md) |
| 21 | as | [lessons/21-as.md](lessons/21-as.md) |
| 22 | Arrow Function | [lessons/22-arrow-function.md](lessons/22-arrow-function.md) |
| 23 | Destructuring | [lessons/23-destructuring.md](lessons/23-destructuring.md) |
| 24 | type / interface | [lessons/24-type-interface.md](lessons/24-type-interface.md) |
| 25 | Generic | [lessons/25-generic.md](lessons/25-generic.md) |
| 26 | Optional Property | [lessons/26-optional-property.md](lessons/26-optional-property.md) |

## 贯穿全课的例子

交通灯、门、播放器、电梯、TCP 连接、订单、登录、下载器——这些例子会反复出现，因为它们最容易建立 FSM 直觉：都是"有限个模式 + 明确的触发条件"。

## 代码约定

全部代码使用 **XState v5**：`setup()` + `createMachine()` + `createActor()`。不使用 v4 的 `Machine()` / `interpret()` / 顶层 `spawn()` import。

```bash
npm install xstate@^5
```
