# 02 State：为什么需要"当前模式"

> 上一课：[01 Machine](01-machine.md) · 下一课：[03 Event](03-event.md)

## 1 为什么需要

第 1 课画出了整张图，但一台正在运行的机器，任何时刻都要能回答一个问题："你现在在哪个方框里？" 这就是 State 要解决的问题：**给"当前所处的模式"一个明确、唯一的名字。**

如果没有这个概念，你只能靠若干个变量组合去猜"现在是什么状态"，猜错的成本会随着变量变多而指数级上升（回忆第 1 课门的例子：`isOpen`、`isLocked`、`isOpening` 组合出的非法状态）。

## 2 核心思想

**State 是"当前允许生效的一种模式"，任意时刻有且只有一个 State 是当前的。**

## 3 ASCII 图

播放器：

```
+------+   +---------+   +--------+
| idle |   | playing |   | paused |
+------+   +---------+   +--------+
   ^ 当前只有一个方框会被标记为"我是现在这个"
```

登录：

```
+---------------+   +------------+   +---------------+
| loggedOut     |   | loggingIn  |   | loggedIn      |
+---------------+   +------------+   +---------------+
```

TCP 连接（简化版，忽略 half-close 等细节）：

```
+--------+   +------------+   +------------+   +--------+
| closed |   | syn_sent   |   | established|   | closed |
+--------+   +------------+   +------------+   +--------+
   (起点)                                          (终点)
```

三个例子的共性：任意时刻，只有一个方框是"亮着的"，其余都是暗的。

## 4 JS 思维

```js
// 一个普通字符串变量，就是"当前 State"最朴素的表示
let currentState = 'idle';

function isPlaying() {
  return currentState === 'playing';
}
```

关键点：`currentState` 永远只装**一个**值，而且这个值必须是图里预先定义好的合法状态名之一，不能是任意字符串。这跟一般的业务变量不一样——一般变量（比如 `username`）取值范围是开放的，State 的取值范围是**封闭、有限、提前列举好的**。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const playerMachine = setup({}).createMachine({
  id: 'player',
  initial: 'idle',
  states: {
    idle: {},
    playing: {},
    paused: {},
  },
});

const player = createActor(playerMachine);
player.subscribe((snapshot) => {
  console.log(snapshot.value); // 'idle' -> 之后可能是 'playing' / 'paused'
});
player.start();

console.log(player.getSnapshot().value); // 'idle'
console.log(player.getSnapshot().matches('idle')); // true
```

## 6 TypeScript 语法拆解

```ts
states: {
  idle: {},
  playing: {},
  paused: {},
},
```
这里每个状态先写成空对象 `{}`——因为还没有 `on`（第 4 课）。这是合法的：一个状态可以什么事件都不响应，只是"存在于图里"。

```ts
player.getSnapshot().value
```
`getSnapshot()` 返回当前快照对象，`.value` 是当前 State 的名字（字符串，或者第 11 课会看到的嵌套对象形式）。这是"读取当前是哪个方框"的标准方式，等价于第 4 节 JS 版本里的 `currentState`。

```ts
player.getSnapshot().matches('idle')
```
`matches()` 是"判断当前状态是不是某个值"的语义化写法，比自己写 `snapshot.value === 'idle'` 更推荐，因为嵌套状态（第 11 课）下字符串比较会不够用。

## 7 常见错误

**错误 1：把 State 当成普通变量，塞进跟"模式"无关的信息。**

```ts
// ❌ 状态名里编码了数据
states: {
  playingAtVolume50: {},
  playingAtVolume80: {},
}
```
音量是数据，不是模式。State 爆炸的根源几乎都是这种"把 Context 该做的事情塞进 State 名字"。

**错误 2：让多个状态同时为真。**

XState 的普通（非并行）状态机在任意时刻只有一个"当前叶子状态"，如果你发现自己想问"能不能同时是 playing 又是 loading"，要么这其实是两个独立维度（考虑第 12 课 Parallel State），要么其中一个根本不该是 State。

**错误 3：状态定义完全空着就以为"没用"，其实空对象 `{}` 本身就是合法且常见的写法（尤其是终止状态，比如 `closed`）。** 不要因为"这个状态没有配置"就怀疑自己写错了。

## 8 为什么官方这样设计

State 之所以被设计成"任意时刻唯一"，是因为**这正是有限状态机（FSM）这个数学模型的定义本身**——FSM 理论上就是"有限个状态 + 状态间的转移函数"，当前状态永远是单一确定的。XState 忠实地实现了这个模型，而不是发明一个新概念。这个约束不是限制，而是它能帮你消灭"不确定当前处于哪种模式"这类 bug 的根本原因。

## 9 最佳实践

1. State 名字用名词或形容词性质的"模式描述"（`idle`、`playing`、`loading`），不要用动词短语描述一次性动作。
2. 一台机器的 State 数量最好能一句话说清楚"分别是什么意思"；如果解释起来很绕，通常说明建模有问题。
3. 优先用 `matches()` 而不是直接比较 `snapshot.value === 'xxx'`，为以后可能出现的嵌套状态留余地。
4. 不要为了"看起来完整"而添加用不到的状态；State 应该对应真实存在的业务模式。
5. 设计时问自己："这个信息如果变了，业务的**可能动作集合**会变吗？" 会变 → 可能是 State；不会变 → 大概率是 Context（下一课细讲）。

## 10 一句话总结

**State 是任意时刻唯一、且从有限集合中取值的"当前模式"。**

## 11 设计思考（Design Thinking）

**需求**：播放器要能区分"没在播"、"正在播"、"暂停中"。

**方案 A（推荐）**：三个 State。

```ts
states: { idle: {}, playing: {}, paused: {} }
```

**方案 B（错误）**：用一个字符串变量硬编码但不作为状态图节点，散落在业务代码各处判断。

```js
let status = 'idle';
function play() {
  if (status === 'idle' || status === 'paused') status = 'playing';
  // 各种调用点自己判断，容易漏判
}
```

方案 B 表面上"也能用"，但所有合法性判断（能不能从 idle 直接跳到 paused？）都散落在业务函数里，没有一个地方能一眼看全"到底有哪些模式、哪些跳转是合法的"。方案 A 把这些信息集中在 `states` 里，天然就是文档，也天然就是校验规则的来源（第 4 课会看到非法事件会被直接忽略）。

## 12 设计原则（Design Rules）

本课新增：

- **State 表示模式（Mode），不表示数据。** 判断标准：这个信息变化时，"当前允许做的动作集合"是否也跟着变？是 → State；否 → Context。
- **任意时刻只有一个当前 State（并行状态机例外，见第 12 课）。**
- **State 的取值范围必须是有限且提前枚举好的，不能是开放字符串。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[03 Event——驱动状态变化的唯一入口](03-event.md)
