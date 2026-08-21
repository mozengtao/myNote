# 12 Parallel State：什么时候需要并行

> 上一课：[11 Nested State](11-nested-state.md) · 下一课：[13 Actor 思想](13-actor-model.md)

## 1 为什么需要

播放器除了"播放/暂停/停止"这个维度，往往还有另一个完全独立的维度："静音/非静音"。这两个维度可以任意组合：播放中可以静音，暂停中也可以静音。如果用第 11 课的嵌套状态硬套，你会发现自己在写：

```
playingMuted / playingUnmuted / pausedMuted / pausedUnmuted / stoppedMuted / stoppedUnmuted ...
```

这是"两个独立维度的笛卡尔积"，维度一多，状态数量会指数级爆炸——这正是本课要解决的问题：**当一个东西真的存在多个互相独立、可以同时成立的"模式维度"时，需要一种方式分别建模，而不是强行合并成一个大状态。**

## 2 核心思想

**Parallel State 是把几个互相独立、可以同时生效的状态维度分开建模，每个维度各自维护自己的当前状态，互不干扰。**

## 3 ASCII 图

```
+---------------------------------------------+
| player (parallel)                            |
|                                                |
|  +-----------------+   +-------------------+  |
|  | playback         |   | volume            |  |
|  |  idle/playing/    |   |  muted/unmuted    |  |
|  |  paused          |   |                    |  |
|  +-----------------+   +-------------------+  |
+---------------------------------------------+
```

两个方框（`playback` 和 `volume`）同时存在、同时"各自有一个当前状态"，互不影响彼此的转移。

## 4 JS 思维

```js
// 两个完全独立的字符串变量，各自维护自己的状态
let playback = 'idle'; // 'idle' | 'playing' | 'paused'
let volume = 'unmuted'; // 'muted' | 'unmuted'

function send(event) {
  // playback 维度自己的转移逻辑
  if (playback === 'idle' && event.type === 'PLAY') playback = 'playing';
  if (playback === 'playing' && event.type === 'PAUSE') playback = 'paused';

  // volume 维度自己的转移逻辑，完全独立，互不干扰
  if (volume === 'unmuted' && event.type === 'MUTE') volume = 'muted';
  if (volume === 'muted' && event.type === 'UNMUTE') volume = 'unmuted';
}

send({ type: 'PLAY' });
send({ type: 'MUTE' });
console.log(playback, volume); // 'playing' 'muted' —— 两个维度独立组合
```

对比第 11 课的嵌套写法：嵌套状态是"父子关系"（子状态从属于父状态，任意时刻只能在一个子状态），并行状态是"兄弟关系但各自独立"（两个区域同时存在，各自有当前状态）。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const playerMachine = setup({}).createMachine({
  id: 'player',
  type: 'parallel', // 声明这是一台并行状态机
  states: {
    playback: {
      initial: 'idle',
      states: {
        idle: { on: { PLAY: { target: 'playing' } } },
        playing: { on: { PAUSE: { target: 'paused' }, STOP: { target: 'idle' } } },
        paused: { on: { PLAY: { target: 'playing' } } },
      },
    },
    volume: {
      initial: 'unmuted',
      states: {
        unmuted: { on: { MUTE: { target: 'muted' } } },
        muted: { on: { UNMUTE: { target: 'unmuted' } } },
      },
    },
  },
});

const player = createActor(playerMachine).start();

player.send({ type: 'PLAY' });
player.send({ type: 'MUTE' });

console.log(player.getSnapshot().value);
// { playback: 'playing', volume: 'muted' }
// 两个维度的当前状态同时存在于同一份快照里
```

## 6 TypeScript 语法拆解

```ts
type: 'parallel',
```
这是状态节点（这里是顶层机器）上的一个特殊字段，值固定为字符串 `'parallel'`，表示"我的直接子状态不是互斥的候选项，而是各自独立、同时生效的区域（region）"。不写这个字段时，默认行为是第 11 课的嵌套状态（子状态互斥，只有一个是当前）。

```ts
states: {
  playback: { initial: 'idle', states: {...} },
  volume: { initial: 'unmuted', states: {...} },
},
```
在并行模式下，`states` 里的每个 key（`playback`、`volume`）不再是互斥的候选状态，而是"并行区域"，每个区域内部依然是一套完整的嵌套状态机（有自己的 `initial`、自己的 `states`）——第 11 课学到的所有嵌套规则，在每个并行区域内部原样适用。

```ts
{ playback: 'playing', volume: 'muted' }
```
并行状态机的 `snapshot.value` 是一个对象，每个 key 对应一个区域当前的状态值。这跟第 11 课嵌套状态的 `{ red: 'waiting' }` 长得像，但语义不同：嵌套状态的 `value` 只有一条"当前路径"，并行状态机的 `value` 同时包含**所有区域**各自的当前值。

## 7 常见错误

**错误 1：把本该合并成一个状态维度的东西，错误地拆成了并行区域。**

```ts
// ❌ loading/loaded/error 其实是同一件事的不同阶段，不是独立维度，不该并行
type: 'parallel',
states: {
  isLoading: { states: { yes: {}, no: {} } },
  isLoaded: { states: { yes: {}, no: {} } },
  isError: { states: { yes: {}, no: {} } },
}
```
这几个维度实际上是**互斥**的（不可能同时 `isLoading: yes` 又 `isError: yes` 又合理），说明它们本质是同一个维度的不同取值，应该合并成一个普通的嵌套/扁平状态（`loading` / `loaded` / `error`），而不是拆成假装独立的并行区域。

**错误 2：反过来，把真正独立的维度硬塞进一个状态里，导致组合爆炸（回到本课开头的问题）。**

```ts
// ❌ playback × volume 的笛卡尔积，回到了本课开头想避免的问题
states: { playingMuted: {}, playingUnmuted: {}, pausedMuted: {}, pausedUnmuted: {} }
```

**错误 3：忘记每个并行区域内部也需要自己的 `initial`。**

```ts
// ❌ volume 区域没有 initial，无法确定该维度的起始状态
volume: {
  states: { unmuted: {}, muted: {} }, // 缺少 initial
},
```

## 8 为什么官方这样设计

Parallel State 存在的意义，是承认**现实世界的很多东西天然具有多个正交（互不影响）的维度**，而 FSM 的"任意时刻只有一个当前状态"这个基本假设（第 2 课），只对**单一维度**成立。如果强行把多个独立维度塞进一个状态里，状态数量会随维度数指数增长，而且这些状态之间的"共同变化的部分"和"独立变化的部分"完全混在一起，很难维护。XState 用 `type: 'parallel'` 把"多个独立维度各自是一个小状态机，共同构成一份快照"这件事标准化，让你可以用第 1-11 课学的所有工具（Transition/Guard/Action/嵌套）分别对每个维度建模，互不干扰。

## 9 最佳实践

1. 判断两个维度是否该用并行建模的标准：它们能否**任意组合**、互不排斥？能 → 并行；不能（互斥）→ 应该合并成一个维度。
2. 每个并行区域内部依然要遵守嵌套状态的所有规则（`initial` 必填、共享行为放父层等）。
3. 不要仅仅因为"看起来有两个概念"就拆成并行——先问"这两个概念能同时以任意组合成立吗"。
4. 并行区域数量不宜过多，通常 2-3 个独立维度是合理上限；如果发现自己需要 5、6 个并行区域，可能说明这个 Machine 承担了太多职责，考虑拆成多个独立的 Actor（下一部分）协作。
5. 读取并行状态机的当前值时，同样优先用 `matches({ playback: 'playing' })` 这种语义化写法，而不是手动比较整个 `value` 对象。

## 10 一句话总结

**Parallel State 用于建模真正独立、可以任意组合的多个状态维度，避免用一个巨大状态硬凑组合爆炸。**

## 11 设计思考（Design Thinking）

**需求**：下载器需要同时追踪"下载进度"（未开始/下载中/已完成/失败）和"是否用户手动暂停"（暂停/未暂停）。

**方案 A（推荐）**：判断这两个维度能否任意组合——"下载中"可以被暂停，"失败"状态严格说不该有"暂停"的概念，仔细分析后发现这里其实"暂停"只在"下载中"这一个阶段有意义，不是真正独立于全部下载阶段的维度，所以正确做法可能是把"暂停"作为"下载中"这个状态下的一个子状态（嵌套，而不是并行）：

```ts
downloading: {
  initial: 'active',
  states: {
    active: { on: { PAUSE: { target: 'paused' } } },
    paused: { on: { RESUME: { target: 'active' } } },
  },
}
```

**方案 B（错误）**：不假思索地看到"两个形容词"（进度 + 是否暂停）就直接拆成并行区域。

```ts
// ❌ "暂停"和"失败"状态组合在业务上没有意义，但并行结构完全不会阻止这种非法组合
type: 'parallel',
states: {
  progress: { states: { notStarted: {}, downloading: {}, completed: {}, failed: {} } },
  pauseState: { states: { paused: {}, active: {} } },
}
// 这种建模下，{ progress: 'failed', pauseState: 'paused' } 是"合法"的组合，但业务上没有意义
```

方案 B 的问题在于：并行状态机的核心假设是"两个区域可以任意组合"，但"已失败"和"已暂停"同时成立在业务上根本不该发生，说明"暂停"其实是"下载中"这一个特定阶段才有意义的子概念，不是一个真正独立于全部下载阶段的维度。方案 A 先做了本课第 9 节强调的判断——"这两个概念能任意组合吗？"——发现答案是否定的，于是选择了更贴合实际语义的嵌套建模，避免了并行状态机制造出一堆本不该存在的组合。**这也说明"要不要用 Parallel"本身就是需要设计判断的，不是看到多个维度就无脑套用。**

## 12 设计原则（Design Rules）

本课新增：

- **只有真正独立、可以任意组合的多个维度才用 Parallel State；如果两个维度存在互斥或依赖关系，应该合并或改用嵌套建模。**
- **判断标准：两个状态能否同时任意成立而不产生业务上无意义的组合？能 → 并行；不能 → 不是并行。**
- **并行区域数量应保持精简（通常 2-3 个），过多独立维度往往意味着该拆成多个协作的 Actor（见第四部分）。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

第三部分到此结束。下一课进入第四部分：[13 Actor 思想——流程 vs 工作](13-actor-model.md)
