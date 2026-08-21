# FSM 设计原则汇总

这份文件是整套课程真正的"产出"。API 会过时，这些原则不会。

无论你以后用 XState、SCXML、Boost.SML，还是自己手写一个状态机，都可以回来看这份清单。

原则按课程出现顺序追加，每条都标注来源课程，方便回溯"为什么是这样"。

---

## 核心分工（贯穿全课的一句话总结）

> ✓ State 表示模式（Mode）
> ✓ Context 表示数据（Data）
> ✓ Event 表示外部输入（Input）
> ✓ Transition 表示状态变化（Flow）
> ✓ Guard 决定能否变化（Decision）
> ✓ Action 负责副作用（Effect）

后面每一课都是在给这六行做注解。

---

## 第一部分：FSM 核心概念

### 来自 [01 Machine](lessons/01-machine.md)

- Machine 是图纸（静态描述），Actor 是照图纸造出的实例（运行时）。图纸本身不能被驱动，不要对 Machine 调用 `.send()`。
- 状态图只画互斥的、有限的模式。能同时成立、独立变化的维度不属于状态图本身（后面用 Context 或 Parallel State 处理）。
- 先画图，再写代码。图错了，代码怎么写都是错的。

### 来自 [02 State](lessons/02-state.md)

- State 表示模式（Mode），不表示数据。判断标准：这个信息变化时，"当前允许做的动作集合"是否也跟着变？是 → State；否 → Context。
- 任意时刻只有一个当前 State（并行状态机例外，见第 12 课）。
- State 的取值范围必须是有限且提前枚举好的，不能是开放字符串。

### 来自 [03 Event](lessons/03-event.md)

- Event 表示外部输入（Input），状态机永远是"被事件驱动"而不是"被直接赋值"。
- 未被当前状态监听的事件会被静默忽略，这是特性不是 bug。
- 事件命名用"发生了什么"（PLAY），不要用"要做什么的指令"（SET_PLAYING）。

### 来自 [04 Transition](lessons/04-transition.md)

- Transition 表示状态变化（Flow）：没有显式写出来的转移，就当作不存在。
- 同一个事件在不同状态下可以对应不同（甚至没有）的 Transition，这是正常且常见的设计。
- 业务代码只管发送事件，不要预判或代替状态机做"能不能跳转"的判断。

### 来自 [05 Context](lessons/05-context.md)

- State 表示模式（Mode），Context 表示数据（Data）。这是整套课程里最重要的一条分工原则。
- 判断一个字段该放 State 还是 Context：取值是否有限、是否互斥、是否影响可用动作集合。是 → State；否 → Context。
- 更新 Context 必须通过 `assign(...)`，不要在函数体里直接改 `context.xxx`。
- Context 里出现多个互斥布尔值，是"该拆成 State 却没拆"的信号。

## 第二部分：状态机行为

### 来自 [06 Action](lessons/06-action.md)

- Action 表示副作用（Effect）：只负责"做事"，不负责"判断该不该变化"（那是 Guard）。
- 绑定在状态上用 `entry`/`exit`（不管走哪条路径都生效），绑定在具体转移上用 transition action（只有这条路径才生效）。优先用 `entry`/`exit`。
- 更新 Context 永远走 `assign`，普通 Action 不直接改数据。

### 来自 [07 Guard](lessons/07-guard.md)

- Guard 决定能否变化（Decision）：是转移配置的一部分，不是业务代码里的 if。
- Guard 必须是纯函数：不改数据、无副作用、只返回布尔值。
- 业务代码永远无条件发送事件，判断权完全交给状态机自己的 Guard，避免规则散落两处导致不一致或被绕过。

### 来自 [08 after](lessons/08-after.md)

- `after` 本质是内置的定时 Event，和 `on` 结构一致，只是触发源是计时器而不是外部输入。
- 状态一旦被离开，其 `after` 定时器会被自动清理，不需要手动管理生命周期。
- `after` 适合确定时长的超时/定时场景，不适合模拟轮询或替代真正的异步等待（那是 Actor 的职责）。

### 来自 [09 always](lessons/09-always.md)

- `always` 用于"纯数据驱动、不需要外部输入"的自动分流，条件判断必须轻量、无副作用。
- 需要外部输入（用户操作、外部系统响应）的转移用 `on`，不要用 `always` 冒充。
- `always` 链条必须最终收敛，避免因为互相满足条件导致死循环。

## 第三部分：复杂状态

### 来自 [10 Initial](lessons/10-initial.md)

- `initial` 是每一层 `states` 的强制字段，不存在"默认选第一个"这种隐式行为。
- "从哪个模式开始"是设计阶段就该确定的建模问题，不要和"具体数据的初始值"（该放 Context）混为一谈。
- 能被类型系统在编译期拦住的结构性错误（如 initial 指向不存在的状态），就不要留到运行时才发现。

### 来自 [11 Nested State](lessons/11-nested-state.md)

- 发现多个状态共享行为或天然从属于同一大类时，用 Nested State 表达，而不是靠命名前缀模拟层次。
- 复合状态必须有自己的 `initial`，这条规则递归成立，每一层都不例外。
- 跨父状态跳转优先用绝对路径 `#机器id.状态路径`，避免相对路径在深层嵌套下产生歧义。

### 来自 [12 Parallel State](lessons/12-parallel-state.md)

- 只有真正独立、可以任意组合的多个维度才用 Parallel State；如果两个维度存在互斥或依赖关系，应该合并或改用嵌套建模。
- 判断标准：两个状态能否同时任意成立而不产生业务上无意义的组合？能 → 并行；不能 → 不是并行。
- 并行区域数量应保持精简（通常 2-3 个），过多独立维度往往意味着该拆成多个协作的 Actor。

## 第四部分：Actor Model

### 来自 [13 Actor 思想](lessons/13-actor-model.md)

- Machine 负责流程决策，Actor 负责具体工作，两者是分工协作关系，不要用一个模型硬做另一个模型该做的事。
- 判断该不该用 Actor 的标准：这个操作的结果是否会影响流程走向？会 → Actor；不会 → 普通副作用即可。
- 连续变化的进度类数据不应该体现为状态图节点，应放进 Context 或由 Actor 内部管理。

### 来自 [14 invoke](lessons/14-invoke.md)

- `invoke` 绑定 Actor 生命周期到状态节点：进入启动、离开停止，不需要手动记录"是否已过期"之类的标记。
- 几乎所有 `invoke` 都应该同时处理 `onDone` 和 `onError`，异步工作默认是"可能失败"的。
- `invoke` 停止不等于底层操作被真正中断；真正的取消需要在 Actor 实现内部处理。

### 来自 [15 fromPromise / fromCallback](lessons/15-frompromise-fromcallback.md)

- 按工作的"形状"选构造器：一次性结果用 `fromPromise`，持续事件流用 `fromCallback`，不要用一个硬凑另一个的场景。
- `fromCallback` 必须返回清理函数，这是它对"如何停止自己"这个问题的强制回答，缺失会导致资源泄漏。
- `fromPromise` 里捕获错误后要么重新抛出、要么明确视为成功路径，不要模糊地吞掉。

### 来自 [16 spawn](lessons/16-spawn.md)

- `invoke` 用于数量固定、生命周期跟随状态的场景；`spawn` 用于数量运行时动态决定、需要单独管理的场景，两者互补而非互相替代。
- `spawn` 出来的引用要存进 Context 并配合唯一标识，方便后续单独查找和控制。
- 动态创建的 Actor 要显式设计清理/停止机制，不能只管创建不管销毁。

## 第五部分：XState v5 架构

### 来自 [17 生命周期](lessons/17-lifecycle.md)

- Machine 描述（setup + createMachine 的产物）是不可变的静态数据，可以安全地被多次 `createActor` 复用，互不干扰。
- 生命周期是单向流水线：先描述、再实例化、再激活、再驱动，顺序不能颠倒（比如不能在 start 前 send）。
- 需要完整观察一个 Actor 生命周期的所有快照时，`subscribe` 必须写在 `start()` 之前。

## 第六部分：TypeScript（仅学习 XState 所需）

### 来自 [18 Object](lessons/18-object.md)

- 对象类型用来描述数据形状（Context、Event 等），是 XState 类型标注的基础构件。
- 类型描述"长什么样"，值描述"具体是什么"，不要混淆两者出现的位置。
- 不要为了图省事用 `Record<string, any>` 之类放弃类型检查的写法，那会让 TypeScript 的价值归零。

### 来自 [19 Union](lessons/19-union.md)

- 多种可能形状的数据（尤其是事件）用 Union 类型表达，而不是用一个"万能对象+可选字段"敷衍了事。
- Union 类型的值任意时刻是"其中一种"，不是"所有分支字段的合体"，访问某分支特有字段前必须先判断收窄。
- `types.events` 的 Union 必须覆盖机器实际会用到的所有事件类型，遗漏会导致对应 `send` 调用编译报错。

### 来自 [20 Literal Type](lessons/20-literal-type.md)

- 状态名、事件名这类"有限、具体的字符串集合"应该用字面量类型 + Union 标注，而不是宽泛的 `string`。
- 同一 Union 里的判别字段（如 `type`）必须在各分支间互不相同，否则失去区分能力。
- 显式标注字面量类型（而不依赖 TS 自动推断），能确保得到精确类型而不是被拓宽成 `string`。

### 来自 [21 as](lessons/21-as.md)

- `as` 是编译期类型断言，不做运行时转换；`types` 里的 `{} as T` 是"用占位值挂载类型信息"的固定套路，不代表真实的初始数据。
- `types.context`/`types.events` 里的值仅供类型声明，真正生效的初始数据分别在 `context: {...}` 字段和运行时的 `send()` 调用里。
- 不要滥用 `as` 掩盖类型不兼容的问题，它应该只用于"你确实比编译器掌握更多信息"的场景。

### 来自 [22 Arrow Function](lessons/22-arrow-function.md)

- XState 的 Action/Guard/assign 字段统一用箭头函数表达"给定输入怎么算结果"，保持配置代码简洁。
- 单行表达式自动 return，一旦加花括号包裹函数体，必须显式写 return。
- 函数内部只读取传入的 context，不要直接修改它——始终返回全新计算出的值。

### 来自 [23 Destructuring](lessons/23-destructuring.md)

- 解构只是"取值"的语法糖，不会自动处理"字段可能不存在"的情况——那依然需要类型收窄来保证安全。
- 只解构函数体真正用得到的字段，不需要的字段不必列出，保持参数列表简洁。
- XState 用"对象参数 + 解构"而非位置参数，是为了未来能安全地扩展新字段而不破坏已有代码。

### 来自 [24 type / interface](lessons/24-type-interface.md)

- 简单类型可以内联写在 `setup({ types: {...} })` 里；复杂或需要复用的类型应该提前用 `type` 命名。
- XState 场景统一使用 `type` 而不是 `interface`，因为事件类型经常需要 `type` 才支持的 Union 表达能力。
- 类型名用 PascalCase（大写开头驼峰），变量/字段名用 camelCase（小写开头驼峰），避免混淆。

### 来自 [25 Generic](lessons/25-generic.md)

- 泛型是类型层面的参数，让工具类型能套用在任意具体类型上；优先复用官方提供的泛型工具（如 `ActorRefFrom<T>`），而不是手写等价逻辑。
- 大多数场景下让 TypeScript 自动推断泛型参数，只有推断失败或需要显式覆盖时才手写尖括号。
- 不必读懂 XState 内部泛型工具的实现细节，只需要知道它们"接收什么、产出什么"。

### 来自 [26 Optional Property](lessons/26-optional-property.md)

- 可选属性用于"同一形状下确实可能缺省"的字段；如果字段的有无代表不同的业务动作或流程分支，应该拆成 Union，而不是标可选敷衍了事。
- 使用可选字段前必须处理"缺省"的情况（判断或 `??` 默认值），不能假设它一定存在。
- 选择 TypeScript 语法之前，先想清楚字段的真实业务语义——这条原则贯穿第六部分所有 TS 知识点：语法服务于建模，不是反过来。

---

## 最后：把所有原则串起来看

读完全部 26 课，你应该能看出这些原则其实都在回答同一组问题：

| 问题 | 答案 | 对应课程 |
|---|---|---|
| 这个信息是有限的模式，还是无限的数据？ | 模式 → State；数据 → Context | 02、05 |
| 状态怎么变？ | 只能通过显式的 Event + Transition | 03、04 |
| 变化该不该发生？ | Guard 决定（纯函数、写在配置里） | 07 |
| 变化发生时要做什么？ | Action 负责（副作用、entry/exit） | 06 |
| 变化的驱动源是什么？ | 外部输入用 `on`，时间用 `after`，纯数据判断用 `always` | 03、08、09 |
| 图太复杂怎么办？ | 有共享行为用 Nested State；有独立维度用 Parallel State | 11、12 |
| 谁负责真正干活（异步、持续运行）？ | Actor（invoke 固定绑定，spawn 动态创建） | 13-16 |
| 这一切怎么落地成可运行的程序？ | setup → createMachine → createActor → start → send | 17 |
| TypeScript 怎么帮我不出错？ | 用类型精确描述"有限的集合"（Union + Literal），而不是宽泛类型敷衍 | 18-26 |

这套思维方式不依赖 XState 这个具体工具存在。以后无论是维护老项目里的 SCXML，还是用 C++ 的 Boost.SML，还是自己手写一个状态机，回到这张表，一样能推导出该怎么建模。

<!-- RULES_START -->
<!-- RULES_END -->
