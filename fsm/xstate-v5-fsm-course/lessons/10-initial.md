# 10 Initial：为什么必须有起点

> 上一课：[09 always](09-always.md) · 下一课：[11 Nested State](11-nested-state.md)

## 1 为什么需要

第 1 课就用过 `initial` 字段，但没细讲为什么它是必须的。想象一下：如果一个状态机没有指定"从哪个状态开始"，第一次调用 `getSnapshot().value` 应该返回什么？这不是一个可以留白的问题——状态机的核心承诺是"任意时刻都有一个确定的当前状态"（第 2 课），而这个承诺必须从**创建的那一刻**就成立，不能有"还没决定是哪个状态"的中间态。

## 2 核心思想

**`initial` 是状态图的入口，没有它，"当前状态"这个概念在状态机刚诞生的那一刻就是未定义的，整套状态机的核心承诺就崩塌了。**

## 3 ASCII 图

```
        initial
           |
           v
      +--------+
      |  idle  |  <- 起点：任何一个 actor 从这里开始
      +--------+
```

没有起点的图是不完整的，就像一张地图没有标出"你在这里"。

## 4 JS 思维

```js
// 没有 initial 概念的版本：current 靠猜、靠约定，谁也不能保证
let current; // undefined —— 这台"机器"还没有确定的当前状态

function isValid() {
  return current !== undefined; // 任何用到 current 的代码都要先做这个防御性检查
}
```

有了明确起点之后：

```js
const INITIAL_STATE = 'idle'; // 显式声明起点
let current = INITIAL_STATE;   // 创建的瞬间就有确定值，不需要任何防御性判断
```

这就是 `initial` 的价值：**把"当前状态永远有效"这个不变量，从"靠约定/靠记住初始化"变成"语法层面强制要求"。**

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

// ❌ 如果省略 initial，TypeScript 会直接报编译错误：
// Property 'initial' is missing
const brokenMachine = setup({}).createMachine({
  id: 'broken',
  states: {
    idle: {},
    active: {},
  },
  // initial: 'idle',  <- 故意注释掉，演示编译期就会报错
} as any); // 这里用 as any 只是为了让示例代码本身能展示错误场景，实际项目中不要这样做

// ✅ 正确写法
const machine = setup({}).createMachine({
  id: 'toggle',
  initial: 'idle',
  states: {
    idle: {
      on: { ACTIVATE: { target: 'active' } },
    },
    active: {
      on: { DEACTIVATE: { target: 'idle' } },
    },
  },
});

const actor = createActor(machine).start();
console.log(actor.getSnapshot().value); // 'idle'，创建瞬间就有明确的当前状态
```

嵌套状态（下一课细讲）里，每一层子状态也都需要各自的 `initial`——起点这个约束是**递归成立**的，不管图有多深，每一层都不能没有入口。

## 6 TypeScript 语法拆解

```ts
initial: 'idle',
```
值必须是同级 `states` 对象里存在的某个 key。XState v5 配合 `setup()` 使用时，如果这个值不存在于 `states` 里，或者干脆省略了 `initial` 字段，TypeScript 会在编译期直接标红报错——这不是运行时才发现的问题，写代码的当下 IDE 就会提示你。这是 XState v5 相比 v4 的一个重要进步：更多"结构性错误"被类型系统在编译期拦下来了。

## 7 常见错误

**错误 1：以为不写 `initial` 会默认选第一个定义的状态。**

XState 不会做这种"猜测"，`initial` 是强制字段，必须显式写明。这是有意为之——依赖"对象属性定义顺序"来决定行为，是一种隐晦且容易出 bug 的写法（JS 对象的键顺序在某些场景下并不总是直觉的），XState 选择要求你显式声明，消除这类隐患。

**错误 2：`initial` 指向的状态名拼错，和 `states` 里的 key 对不上。**

```ts
// ❌ 'idl' 不是 states 里定义的任何一个状态
initial: 'idl',
states: { idle: {}, active: {} },
```
这类拼写错误在有类型检查的情况下会被直接拦下来，属于"用类型系统防御低级错误"的典型案例。

**错误 3：嵌套状态（下一课）里忘记给子层也设置 `initial`。**

进入一个有子状态的复合状态时，如果这个复合状态自己没有 `initial`，同样会导致"进到这一层却不知道具体在哪个子状态"的问题——这个约束在每一层都成立，不只是顶层。

## 8 为什么官方这样设计

`initial` 被设计成必填字段（而不是有默认值或者可以省略），是 XState 一贯风格的体现：**能在编译期堵住的错误，就不要留到运行时才暴露。** 一个"当前状态未定义"的状态机，比一个完全没有的状态机还危险——它看起来能用，但任何依赖"当前状态一定有值"的代码（几乎所有代码）都可能在某个边界情况下崩溃。强制要求 `initial`，等于是从源头上让这类 bug 根本没有产生的机会。

## 9 最佳实践

1. 创建任何一个新的 `states` 块（无论是顶层还是嵌套的子状态），第一件事就是想清楚 `initial` 该是哪个。
2. `initial` 通常应该指向"最保守、最安全"的状态（比如 `idle`、`closed`），而不是某个中间态。
3. 让 IDE 的类型检查帮你捕获拼写错误，不要手动反复核对字符串是否一致。
4. 画状态图时，习惯性地在起始状态旁边画一个明确的入口箭头（就像第 1 课的图那样），思考的时候和写代码的时候保持同一份心智模型。
5. 如果发现某个状态图很难确定"到底该从哪里开始"，往往说明这个状态图的职责边界没想清楚，值得退一步重新梳理需求。

## 10 一句话总结

**`initial` 是状态图的强制入口：没有它，"当前状态永远有确定值"这个核心承诺从创建的第一刻就不成立。**

## 11 设计思考（Design Thinking）

**需求**：电梯系统里，电梯创建后应该处于什么状态？

**方案 A（推荐）**：显式指定一个安全、明确的起点，比如"停在某一层、门关闭"。

```ts
states: {
  idleAtFloor: {},   // <- initial
  moving: {},
  doorsOpen: {},
}
initial: 'idleAtFloor',
```

**方案 B（错误）**：认为"电梯的初始状态很复杂，取决于运行时的很多因素（当前楼层、是否有人），所以先不写 initial，等运行时再动态决定"。

```ts
// ❌ 试图绕过 initial 的强制性，本质上是在逃避"到底从哪开始"这个必须现在就回答的问题
const elevatorMachine = setup({}).createMachine({
  states: { idleAtFloor: {}, moving: {}, doorsOpen: {} },
  // 没有 initial，指望运行时"自动判断"
});
```

方案 B 的问题在于：把"这台机器一开始该处于什么状态"这个建模问题，误当成了"运行时才能知道的动态信息"。哪怕电梯的具体楼层、朝向确实是运行时数据，"进入哪个**状态**（模式）"依然是可以、也必须在设计阶段就想清楚的——楼层这类具体数字，本来就该放进 Context（第 5 课），而不是让 `initial` 本身变得不确定。方案 A 把"起点是哪个模式"这个建模决策显式地做出来，具体楼层这种数据可以通过 `input` 或初始 `context` 传入，两件事分开处理，`initial` 依然保持简单明确。

## 12 设计原则（Design Rules）

本课新增：

- **`initial` 是每一层 `states` 的强制字段，不存在"默认选第一个"这种隐式行为。**
- **"从哪个模式开始"是设计阶段就该确定的建模问题，不要和"具体数据的初始值"（该放 Context）混为一谈。**
- **能被类型系统在编译期拦住的结构性错误（如 initial 指向不存在的状态），就不要留到运行时才发现。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[11 Nested State——为什么要拆层](11-nested-state.md)
