# 01 Machine：状态机的"总图"

> 上一课：无（这是第一课） · 下一课：[02 State](02-state.md)

## 1 为什么需要

想象你要实现一个"门"。产品经理说需求很简单："门可以开，可以关。"

你可能会这样写：

```js
let isOpen = false;
let isLocked = false;
let isOpening = false; // 动画播放中？
```

写着写着你会发现：`isOpen = true` 同时 `isLocked = true` 是什么意思？`isOpening = true` 同时 `isOpen = true` 又是什么意思？这些布尔值的组合里，有些是合法状态，有些根本不该存在，但代码完全不会阻止你把它们组合出来。

现实问题是：**当一个东西的行为依赖"当下处于哪种模式"，而模式又是有限的、互斥的，一堆独立布尔值/变量就守不住这个约束。** 你需要一种方式，把"当前允许存在哪些模式"这件事一次性、显式地写下来。这就是 Machine 要解决的问题。

## 2 核心思想

**Machine 就是一张状态图：把一个东西"所有可能的模式"和"模式之间怎么切换"画在一张图里，一次性说清楚。**

## 3 ASCII 图

门（Door）：

```
+--------+   OPEN    +--------+
| Closed | --------> | Opened |
+--------+           +--------+
    ^                    |
    |       CLOSE        |
    +--------------------+
```

交通灯（Traffic Light）：

```
+-------+   TIMER   +--------+   TIMER   +-----+   TIMER
| Red   | --------> | Green  | --------> |Yellow| --------+
+-------+           +--------+           +-----+          |
    ^                                                      |
    +------------------------------------------------------+
```

播放器（Media Player）：

```
+------+    PLAY    +---------+    PAUSE    +--------+
| Idle | ---------> | Playing | <---------> | Paused |
+------+            +---------+             +--------+
                          |
                         STOP
                          |
                          v
                      +------+
                      | Idle |
                      +------+
```

三张图长得不一样，但结构完全一样：**有限个方框（状态） + 箭头（事件驱动的切换）**。这就是"状态图"这个概念的本质，Machine 就是它的代码表示。

## 4 JS 思维

先不用 XState，纯 JS 怎么表示"门"这张图？

```js
// 用一个对象把"图"写下来：状态 -> 事件 -> 下一个状态
const doorGraph = {
  closed: { OPEN: 'opened' },
  opened: { CLOSE: 'closed' },
};

let current = 'closed'; // 只有一个"当前所在的方框"

function send(event) {
  const nextState = doorGraph[current][event];
  if (nextState) {
    current = nextState;
  }
}

send('OPEN');
console.log(current); // "opened"
send('CLOSE');
console.log(current); // "closed"
```

注意这里的关键设计：**`current` 永远只是一个字符串，永远只指向图里的一个方框。** 不存在"既是 closed 又是 opened"的中间态。这不是 XState 的限制，是状态图这个模型本身的设定——所有可能的组合，图里已经画完了，不需要也不允许你再叠加别的布尔值来表达"模式"。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const doorMachine = setup({}).createMachine({
  id: 'door',
  initial: 'closed',
  states: {
    closed: {
      on: {
        OPEN: { target: 'opened' },
      },
    },
    opened: {
      on: {
        CLOSE: { target: 'closed' },
      },
    },
  },
});

const doorActor = createActor(doorMachine);

doorActor.subscribe((snapshot) => {
  console.log('当前状态:', snapshot.value);
});

doorActor.start();          // 当前状态: closed
doorActor.send({ type: 'OPEN' });  // 当前状态: opened
doorActor.send({ type: 'CLOSE' }); // 当前状态: closed
```

对照第 4 节的 JS 版本：`doorGraph` 对应 `states`，`doorGraph.closed.OPEN` 对应 `states.closed.on.OPEN`，`current` 对应 `snapshot.value`。**XState 没有发明新概念，只是把你脑子里那张图，翻译成了一份结构化的、可以被工具理解的数据。**

## 6 TypeScript 语法拆解

逐行看这段代码里出现的写法（这些点第六部分会展开讲，这里先混个脸熟）：

```ts
import { setup, createActor } from 'xstate';
```
从 `xstate` 这个包里，导入两个函数：`setup` 和 `createActor`。这是标准的 JS/TS 模块导入语法，跟状态机本身无关。

```ts
const doorMachine = setup({}).createMachine({ ... });
```
`setup({})` 返回一个"配置好的构造器"，`.createMachine({...})` 在它上面接着调用，传入这张图的具体内容。现在 `{}` 是空的，因为这一课还不需要 actions/guards/types——第 6 课之后你才会看到 `setup({})` 里真正填东西。**这是链式调用（先 setup 后 createMachine），不是两个不相关的函数。**

```ts
id: 'door',
```
给这台机器起个名字，字符串。目前只用来调试打印、以后拼接跳转路径（第 11 课会用到），先记住有这个字段。

```ts
initial: 'closed',
```
告诉机器"图里从哪个方框开始"，对应第 4 节 JS 版本里的 `let current = 'closed'`。第 10 课会专门讲为什么这个字段是必须的。

```ts
states: { closed: { ... }, opened: { ... } },
```
一个普通对象，key 是状态名，value 是这个状态的定义。对应 JS 版本的 `doorGraph`。

```ts
on: { OPEN: { target: 'opened' } },
```
`on` 表示"在这个状态下，能响应哪些事件"。`OPEN` 是事件名，`{ target: 'opened' }` 表示"收到 OPEN 事件后，跳到 opened 状态"。第 4 课会专门讲 `target`。

```ts
const doorActor = createActor(doorMachine);
```
`createActor` 不是"运行这张图"，是"围绕这张图，造一个可以被驱动的实例"。这一点很重要：`doorMachine` 本身只是一份**静态描述**（数据），`doorActor` 才是**活的、有状态的东西**。你可以用同一个 `doorMachine` 造出好几个互不影响的 `doorActor`（比如一栋楼里的每一扇门）。

```ts
doorActor.subscribe((snapshot) => { ... });
```
订阅"每次状态变化时"的回调。`snapshot` 是当前快照，`snapshot.value` 就是当前状态名。

```ts
doorActor.start();
```
真正启动这个实例，让它进入 `initial` 状态。启动之前，这台机器"存在"但"没在跑"。

```ts
doorActor.send({ type: 'OPEN' });
```
向这个实例发送一个事件对象。注意事件永远是 `{ type: '...' }` 这种对象形式，不是裸字符串 `'OPEN'`——第 3 课会解释为什么。

## 7 常见错误

**错误 1：把 Machine 和 Actor 搞混，直接对 `doorMachine` 调用 `.send()`。**

```ts
// ❌ 错误：doorMachine 是静态描述，没有 send 方法
doorMachine.send({ type: 'OPEN' });
```

Machine 是图纸，Actor 才是照着图纸造出来、能运行的东西。图纸不能"被驱动"，只有实例能。

**错误 2：以为需要给每个可能的状态组合单独建模，导致状态数量爆炸。**

```ts
// ❌ 错误思路：门 + 是否上锁 + 是否有人推 = 8 种组合手写 8 个状态
states: {
  closedLockedNoPush: {},
  closedUnlockedNoPush: {},
  closedLockedPushing: {},
  // ... 一直往下枚举
}
```

这是没理解"状态图只画**互斥的模式**"，可以并行独立变化的维度应该用 Context（第 5 课）或 Parallel State（第 12 课）表达，不是全排列。

**错误 3：在没有先画图之前就直接写代码。**

先写 `states: {...}`，一边写一边想"我到底有几个状态"，很容易漏掉转移或画出不该存在的状态。正确顺序永远是：先在纸上/白板上画出方框和箭头，确认图是对的，再翻译成 `createMachine`。

## 8 为什么官方这样设计

XState 的核心哲学是：**状态机的定义应该是一份纯数据（JSON 可序列化的配置），而不是一段命令式代码。**

好处是：

- 这份配置可以被可视化工具（Stately Studio）直接画出图来，也可以反过来从图生成代码——因为它本质上就是图的数据表示。
- 配置和"运行这份配置的引擎"是分离的：`createMachine` 只是描述，`createActor` 才引入运行时状态。这让同一份逻辑可以在服务端、客户端、测试环境里被反复实例化，互不干扰。
- 强制你在写代码前先想清楚"这张图长什么样"，而不是走一步看一步地堆 if/else。

## 9 最佳实践

1. 动手写代码前，先在纸上画出状态图（哪怕只是文字版的 ASCII 图）。
2. 给 Machine 取一个能一眼看出"这是管什么的"的 `id`（如 `'door'`、`'trafficLight'`），排查问题时会经常看到它。
3. 状态名用小写驼峰或全小写（`closed`、`opened`），保持整个项目统一。
4. 一个 Machine 只回答"这个东西现在处于哪种模式"，不要把跟模式无关的数据也硬塞进状态名里（比如不要有 `openedAt3pm` 这种状态）。
5. 先让图能跑起来（哪怕逻辑很粗糙），再逐步加 Guard、Action、Context——不要一上来就想把所有细节都建模进第一版。

## 10 一句话总结

**Machine 就是一张状态图：把"有几种模式"和"模式怎么切换"一次性、显式地写清楚。**

## 11 设计思考（Design Thinking）

**需求**：实现一个"门"，能开、能关。

**方案 A（推荐）**：用状态图建模，`closed` / `opened` 是两个互斥的状态，`OPEN` / `CLOSE` 是驱动切换的事件。

```
states: { closed: { on: { OPEN: {...} } }, opened: { on: { CLOSE: {...} } } }
```

**方案 B（错误）**：用一个布尔值 `isOpen` 加一堆 if 判断。

```js
let isOpen = false;
function toggle() {
  if (isOpen) {
    // 关门逻辑
  } else {
    // 开门逻辑
  }
}
```

方案 B 现在能跑，但你没法回答这几个问题：门开的时候能不能再收到"OPEN"事件？门坏了（`broken`）算第三种状态吗，`isOpen` 该是 `true` 还是 `false`？随着需求变多，`isOpen`、`isBroken`、`isLocked` 会越叠越多，组合出的"非法状态"（比如同时 broken 又 opened）代码完全无法阻止。

方案 A 的图从一开始就只画出"合法的模式"，非法组合根本不存在于图里，不需要额外代码去防御。这就是"用状态机建模"相对"用一堆布尔值"的根本优势：**约束是画在图上的，不是靠程序员记住的。**

## 12 设计原则（Design Rules）

本课新增：

- **Machine 是图纸（静态描述），Actor 是照图纸造出的实例（运行时）。** 图纸本身不能被驱动，不要对 Machine 调用 `.send()`。
- **状态图只画互斥的、有限的模式。** 能同时成立、独立变化的维度不属于状态图本身（后面会用 Context 或 Parallel State 处理）。
- **先画图，再写代码。** 图错了，代码怎么写都是错的。

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[02 State——为什么状态机只需要知道"当前处于哪个模式"](02-state.md)
