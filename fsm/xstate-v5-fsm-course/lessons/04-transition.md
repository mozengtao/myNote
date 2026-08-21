# 04 Transition：状态怎么变

> 上一课：[03 Event](03-event.md) · 下一课：[05 Context](05-context.md)

## 1 为什么需要

第 3 课说"Event 驱动状态变化"，但"驱动"具体是怎么发生的？光有事件还不够——同一个事件，在不同状态下可能对应不同的下一个状态（`PAUSE` 在 `playing` 下有意义，在 `idle` 下没意义）。需要一种结构，把"在 A 状态下收到 X 事件 → 变成 B 状态"这条规则明确写下来。这条规则就是 Transition（转移）。

## 2 核心思想

**Transition 是"某个状态 + 某个事件 → 目标状态"这条规则本身，状态机里所有的状态变化，都必须经过一条明确写出来的 Transition，不存在"隐式跳转"。**

## 3 ASCII 图

```
+------+   PLAY    +---------+
| idle | --------> | playing |
+------+           +---------+
```

对应一条 Transition：**在 `idle` 状态下，收到 `PLAY` 事件，跳转到 `playing` 状态。**

## 4 JS 思维

```js
// 转移规则集中写在一个查找表里
const transitions = {
  idle: { PLAY: 'playing' },
  playing: { PAUSE: 'paused', STOP: 'idle' },
  paused: { PLAY: 'playing', STOP: 'idle' },
};

let current = 'idle';

function send(event) {
  const nextState = transitions[current]?.[event.type];
  if (nextState) {
    current = nextState;
  }
  // 找不到对应规则：什么都不做（回顾第 3 课）
}
```

`transitions[current]?.[event.type]` 这一行就是"转移"的本质：**给定当前状态和事件，查表得到目标状态。** 状态机的所有行为，理论上都可以归约成反复执行这一次查表。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const playerMachine = setup({}).createMachine({
  id: 'player',
  initial: 'idle',
  states: {
    idle: {
      on: {
        PLAY: { target: 'playing' },
      },
    },
    playing: {
      on: {
        PAUSE: { target: 'paused' },
        STOP: { target: 'idle' },
      },
    },
    paused: {
      on: {
        PLAY: { target: 'playing' },
        STOP: { target: 'idle' },
      },
    },
  },
});

const player = createActor(playerMachine).start();
console.log(player.getSnapshot().value); // 'idle'

player.send({ type: 'PLAY' });
console.log(player.getSnapshot().value); // 'playing'
```

`on: { PLAY: { target: 'playing' } }` 这个结构，key 是事件名，value 里的 `target` 是目标状态名——这就是一条完整的 Transition。

## 6 TypeScript 语法拆解

```ts
PLAY: { target: 'playing' },
```
`PLAY` 是这条规则响应的事件名（对应 `on` 对象的 key）。`{ target: 'playing' }` 是这条规则的"配置对象"，`target` 字段指定跳到哪个状态。

```ts
target: 'playing'
```
`target` 的值是**同级 `states` 对象里某个 key 的名字**，字符串必须和状态名完全一致，写错了 XState 在开发时会报警告（拼错的目标状态是新手最常见的错误之一）。

还可以写成简写形式（两种写法完全等价）：

```ts
on: {
  PLAY: 'playing', // 简写：直接写目标状态名字符串
}
// 等价于
on: {
  PLAY: { target: 'playing' },
}
```

简写适合"只跳转、没有 guard/action"的最简单情况；一旦要加条件（第 7 课）或副作用（第 6 课），就必须用完整的对象形式。

## 7 常见错误

**错误 1：`target` 写错状态名（拼写错误或大小写不一致）。**

```ts
states: {
  idle: { on: { PLAY: { target: 'Playing' } } }, // ❌ 状态名其实是 playing（小写)
  playing: {},
}
```
这是最常见的低级错误，建议开发时打开 XState 的开发者工具或严格类型（第 6 部分会讲的 `types.states` 之类的类型标注能在编译期堵住这个问题）。

**错误 2：以为"没写 Transition 也能跳转"。**

```ts
// ❌ idle 状态完全没有 on，期待它能自动响应 PLAY
states: {
  idle: {}, // 没有任何 on
  playing: {},
}
```
没有显式写出来的 Transition，就是不存在的路。发 `PLAY` 事件不会有任何效果。

**错误 3：把业务逻辑判断写在 Transition 外面，而不是用 Transition 本身表达。**

```ts
// ❌ 在业务代码里手动判断该不该 send
if (currentState === 'idle') {
  player.send({ type: 'PLAY' });
}
```
这完全违背了状态机的意义——判断"能不能变化"应该交给状态机自己（图里有没有这条 Transition），业务代码只管"发生了什么事件"，不应该越俎代庖去预判状态机的反应。

## 8 为什么官方这样设计

"所有状态变化都必须经过显式 Transition"这个约束，本质上是把"状态图"变成了一份**可以被静态分析的完整地图**。如果允许"隐式跳转"（比如某个回调里直接改 `context.state`），这份地图就不完整了，工具（可视化、类型检查、测试覆盖率分析）都没法准确知道这台机器到底能走到哪些状态。XState 的设计哲学始终是：**能写进配置的，就不要藏进代码逻辑里。**

## 9 最佳实践

1. 每加一个新状态，先问"这个状态需要响应哪些事件、分别跳到哪"，再动手写 `on`。
2. 只有单纯跳转、不需要条件或副作用时，用简写形式 `PLAY: 'playing'`，代码更紧凑。
3. 保持状态名和事件名的拼写在整个项目中完全一致，可以考虑用 TypeScript 的字面量联合类型（第 19、20 课）在编译期堵住拼写错误。
4. 不要在业务代码里"预判"状态机会不会响应某个事件——直接 `send`，让状态机自己决定要不要理会。
5. 画图的时候，箭头旁边写清楚"事件名"，这样图和代码（`on` 的 key）能一一对应，减少翻译损耗。

## 10 一句话总结

**Transition 是"当前状态 + 事件 → 目标状态"这条规则本身，没写出来的转移就是不存在的转移。**

## 11 设计思考（Design Thinking）

**需求**：播放器暂停时按"播放"，应该恢复播放。

**方案 A（推荐）**：在 `paused` 状态下也写一条 `PLAY` 的 Transition，目标同样是 `playing`。

```ts
paused: {
  on: { PLAY: { target: 'playing' } },
}
```

**方案 B（错误）**：只在 `idle` 写 `PLAY` 的 Transition，`paused` 状态下想恢复播放时，先手动把状态"改回" `idle`，再发一次 `PLAY`。

```ts
// ❌ 业务代码里做了状态机该做的转移决策
if (currentState === 'paused') {
  forceSetState('idle');
}
player.send({ type: 'PLAY' });
```

方案 B 的问题在于：它在业务代码里悄悄绕过了状态图，凭空造出一条"图上根本没画"的路径（`paused` 直接变 `idle` 又变 `playing`），破坏了"所有变化都经过显式 Transition"的约束，而且这个隐藏的中间态可能会意外触发 `idle` 的 entry/exit 行为（第 6 课）。方案 A 老老实实在图上加一条边，逻辑清晰、可追踪、也不会有意外副作用。

## 12 设计原则（Design Rules）

本课新增：

- **Transition 表示状态变化（Flow）：没有显式写出来的转移，就当作不存在。**
- **同一个事件在不同状态下可以对应不同（甚至没有）的 Transition，这是正常且常见的设计。**
- **业务代码只管发送事件，不要预判或代替状态机做"能不能跳转"的判断。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[05 Context——State 装不下的数据放哪里](05-context.md)
