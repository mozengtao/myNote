# 05 Context：State 装不下的数据放哪里

> 上一课：[04 Transition](04-transition.md) · 下一课：[06 Action](06-action.md)

这是第一部分里最重要的一课，务必吃透。前四课建立的"State/Event/Transition"三件套只能回答"现在是什么模式、怎么切换模式"，但真实业务永远还有一堆**数据**——播放器的音量、当前播放到第几首歌、购物车里的商品——这些数据放哪里，是新手最容易建模错误的地方。

## 1 为什么需要

回到播放器的例子。状态图告诉你"现在是 `playing`"，但光知道"正在播放"还不够，你还需要知道：

- 音量是多少（0-100 之间任意值）
- 正在播放哪首歌（可能是几千首歌里的任意一首）
- 播放到列表的第几个位置

这些信息有一个共同特点：**取值范围很大甚至无限，而且它们的变化通常不影响"当前处于哪种模式"。** 音量从 50 调到 80，播放器依然是 `playing`。如果试图把这些数据也做成 State，会发生什么？

## 2 核心思想

**State 回答"现在是哪种模式"，Context 回答"现在这个模式下，具体的数据是什么"——State 是有限的分类，Context 是无限的数据。**

## 3 ASCII 图

```
+--------------------------------------+
| State: playing                       |
|                                       |
|  Context:                            |
|    volume: 72                        |
|    currentSong: "Track 5"            |
|    playlistIndex: 4                  |
+--------------------------------------+
```

State 是外层的"分类标签"，Context 是贴在标签下面、可以随时变化的具体数值。同一个 `playing` 状态，Context 可以有无数种不同的取值组合。

## 4 JS 思维

```js
// State：有限的模式
let currentState = 'playing'; // 'idle' | 'playing' | 'paused'

// Context：无限可能的数据，挂在状态机实例上
let context = {
  volume: 50,
  currentSong: 'Track 1',
  playlistIndex: 0,
};

function setVolume(newVolume) {
  context.volume = newVolume; // 状态还是 playing，只是数据变了
}

setVolume(80);
console.log(currentState); // 'playing'，没变
console.log(context.volume); // 80，变了
```

关键对比：`currentState` 只能取三个值之一，`context.volume` 可以是 0-100 之间任何数字。**如果你发现自己想给一个变量枚举"所有可能取值"却发现根本枚不完，这个变量大概率该放进 Context，而不是做成 State。**

## 5 XState v5 写法

```ts
import { setup, assign, createActor } from 'xstate';

const playerMachine = setup({
  types: {
    context: {} as {
      volume: number;
      currentSong: string;
      playlistIndex: number;
    },
  },
  actions: {
    setVolume: assign({
      volume: ({ event }) => {
        if (event.type !== 'SET_VOLUME') return 0;
        return event.level;
      },
    }),
  },
}).createMachine({
  id: 'player',
  initial: 'idle',
  context: {
    volume: 50,
    currentSong: 'Track 1',
    playlistIndex: 0,
  },
  states: {
    idle: {
      on: { PLAY: { target: 'playing' } },
    },
    playing: {
      on: {
        PAUSE: { target: 'paused' },
        SET_VOLUME: { actions: 'setVolume' },
      },
    },
    paused: {
      on: { PLAY: { target: 'playing' } },
    },
  },
});

const player = createActor(playerMachine).start();

console.log(player.getSnapshot().context.volume); // 50
player.send({ type: 'PLAY' });
player.send({ type: 'SET_VOLUME', level: 80 });

console.log(player.getSnapshot().value);          // 'playing'（State 没变）
console.log(player.getSnapshot().context.volume); // 80（Context 变了）
```

注意 `SET_VOLUME` 事件被处理后，`target` 没有出现——状态还是 `playing`，只有 `context.volume` 发生了变化。**这正是 Context 和 State 分工的直接体现：数据变化不一定伴随模式变化。**

## 6 TypeScript 语法拆解

```ts
types: {
  context: {} as {
    volume: number;
    currentSong: string;
    playlistIndex: number;
  },
},
```
`types.context` 是给 `setup()` 声明"这台机器的 Context 长什么形状"。`{} as { ... }` 这种写法（一个空对象后面跟 `as` 断言成某个类型）是 XState v5 里反复出现的固定套路，专门用来"只声明类型、不提供真实值"——第 21 课会详细拆这个语法。现在先记住：这里的花括号里写的是 TypeScript 的**类型**（`number`、`string`），不是真实数据。真实的初始数据在下面的 `context: { volume: 50, ... }` 里。

```ts
actions: {
  setVolume: assign({
    volume: ({ event }) => {
      if (event.type !== 'SET_VOLUME') return 0;
      return event.level;
    },
  }),
},
```
`assign(...)` 是 XState 提供的、专门用来"更新 Context"的动作创建函数。传给它一个对象，key 是要更新的 Context 字段名（`volume`），value 是一个函数，接收当前的 `{ event }`（还有 `context`，这里没用到），返回这个字段的新值。`assign` 不是直接赋值 `context.volume = ...`，而是"声明式"地描述"volume 这个字段该怎么算出新值"——第 22 课会拆解这里的箭头函数写法，第 23 课会拆解 `({ event }) =>` 里的解构写法。

```ts
if (event.type !== 'SET_VOLUME') return 0;
```
因为 `playing` 状态下可能收到不同类型的事件（`PAUSE`、`SET_VOLUME`），TypeScript 不知道走到这个函数时具体是哪种事件，所以要先判断 `event.type` 缩小范围，之后才能安全访问 `event.level`（这是"联合类型的收窄"，第 19 课会讲 Union）。

```ts
SET_VOLUME: { actions: 'setVolume' },
```
在 Transition 里加 `actions` 字段（而不是 `target`），表示"收到这个事件后执行某个动作，但不切换状态"。这是本课的关键写法：**Transition 可以只带 action 不带 target，这时候状态原地不动，只是数据变了。**

## 7 常见错误

**错误 1：把连续变化的数据强行做成 State（State 爆炸）。**

```ts
// ❌ 音量有 101 种可能取值，不可能也不应该枚举成状态
states: {
  playingVolume0: {},
  playingVolume1: {},
  // ... 一直写到 playingVolume100
}
```
这正是本课开头提到的陷阱。判断标准见第 2 节：State 是有限分类，Context 是无限数据。

**错误 2：忘记 `assign` 是"声明式描述"，直接在 Action 函数体里写 `context.volume = xxx`。**

```ts
// ❌ 不要直接修改 context，XState 的 context 应被视为不可变
actions: {
  badSetVolume: ({ context, event }) => {
    context.volume = event.level; // 绕过了 assign，容易导致状态不同步
  },
},
```
必须通过 `assign(...)` 来更新 Context，这样 XState 才能正确地生成新的快照并通知订阅者。

**错误 3：把所有东西一股脑塞进 Context，包括那些其实应该建模成 State 的"模式"信息。**

```ts
// ❌ isPlaying 其实就是 State（playing vs 非 playing），不该是 Context 里的布尔值
context: {
  isPlaying: true,
  isPaused: false,
  isLoading: false,
}
```
如果 Context 里出现一堆互斥的布尔值，通常说明这些信息其实是 State，应该拆回 `states` 里（这就是本课开头方案 B 的翻版，只是换了个地方犯同样的错）。

## 8 为什么官方这样设计

XState 把"用什么描述模式"和"用什么描述数据"拆成两个字段（`states` vs `context`），是因为这两类信息的**变化频率和变化方式完全不同**：State 变化是离散的、跳跃式的（从 A 跳到 B，中间没有过渡值），Context 变化往往是连续的、渐变的（音量从 50 慢慢调到 80）。把它们混在一起，会导致状态图的节点数量随着数据精度的提高而爆炸式增长（101 种音量 × 3 种播放状态 = 303 个节点），图完全失去了"一眼看清所有模式"的价值。`assign` 存在的意义，就是给"更新数据"这件事一个统一、可追踪、不破坏"事件驱动"原则的入口——数据的变化本身，也要通过 Action（第 6 课细讲）来完成，而不是被随意的代码直接改写。

## 9 最佳实践

1. 设计前先把需求里的每个"字段"分类：取值有限、互斥、影响可用动作 → State；取值连续或无限、不影响可用动作 → Context。
2. Context 的初始值一定要在 `context: {...}` 里写全，不要留 `undefined` 的坑（第 26 课会讲什么时候确实需要可选字段）。
3. 永远通过 `assign(...)` 更新 Context，不要在普通函数里直接改 `context.xxx`。
4. 如果发现 Context 里出现多个互斥的布尔值（`isA`/`isB`/`isC` 只能有一个是 true），几乎可以肯定这应该拆成 State。
5. 给 Context 加上 `types.context` 类型标注（本课示例已经这样做），这样后面所有引用 `context.xxx` 的地方都会有类型提示和拼写检查。

## 10 一句话总结

**State 是有限的模式分类，Context 是模式之下可以自由变化的数据，两者永远不要混着用。**

## 11 设计思考（Design Thinking）

**需求**：播放器需要保存"当前歌曲""音量""播放列表位置"这三项数据。

**方案 A（推荐）**：

```ts
// State：有限模式
states: { playing: {}, paused: {}, stopped: {} }

// Context：可变数据
context: {
  volume: 50,
  currentSong: 'Track 1',
  playlistIndex: 0,
}
```

**方案 B（错误）**：

```ts
// ❌ 把数据也做成 State，导致状态数量爆炸
states: {
  playingSong1: {},
  playingSong2: {},
  playingSong3: {},
  playingVolume50: {},
  playingVolume60: {},
  // 歌曲数 × 音量档位 × 播放/暂停 = 组合爆炸
}
```

方案 B 错在哪：`currentSong` 可能有几千首歌，`volume` 有 101 档，如果都做成 State，状态数会是这些维度的**笛卡尔积**，图会大到没有任何人能看懂，而且每加一首新歌都要往图里加新状态——这已经不是"建模"，是在用状态图模拟数组和数字。方案 A 里，`states` 永远只有 3 个，无论歌曲库有多大、音量精度多高，图的复杂度都不变，变化的只是 Context 里几个字段的值。**这就是"State 表示有限模式（Mode），Context 表示可变化的数据（Data）"这条原则最直观的体现。**

## 12 设计原则（Design Rules）

本课新增：

- **State 表示模式（Mode），Context 表示数据（Data）。这是整套课程里最重要的一条分工原则，后面所有课程都在巩固它。**
- **判断一个字段该放 State 还是 Context：取值是否有限、是否互斥、是否影响可用动作集合。是 → State；否 → Context。**
- **更新 Context 必须通过 `assign(...)`，不要在函数体里直接改 `context.xxx`。**
- **Context 里出现多个互斥布尔值，是"该拆成 State 却没拆"的信号。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

第一部分到此结束。下一课进入第二部分：[06 Action——副作用什么时候执行](06-action.md)
