# 03 Event：驱动状态变化的唯一入口

> 上一课：[02 State](02-state.md) · 下一课：[04 Transition](04-transition.md)

## 1 为什么需要

State 是"当前在哪个方框"，但方框之间是怎么切换的？现实世界里，状态从不无缘无故变化——门不会自己打开，播放器不会自己开始播放，一定是"发生了什么"：用户点击了播放按钮、收到了服务器的响应、定时器到期了。

如果没有一个统一的"发生了什么"的概念，状态切换逻辑就会散落在代码各处，谁都能在任何时候直接把状态改掉，State 在第 2 课里辛苦建立起来的"任意时刻唯一、有限"的保证就会被绕过。

## 2 核心思想

**Event 是外部世界对状态机说的"发生了这件事"，状态机只能通过 Event 被驱动，不能被直接修改。**

## 3 ASCII 图

播放器收到的事件：

```
  PLAY          STOP          PAUSE
   |              |              |
   v              v              v
+------+     +------+       +------+
| idle |     | idle |       |paused|
+------+     +------+       +------+
```

（这里先只画"事件长什么样"，事件具体怎么改变状态是下一课 Transition 的内容。）

## 4 JS 思维

先看一个**不用 Event** 的错误写法，感受一下问题在哪：

```js
// ❌ 直接改状态，谁都能改，改的理由完全不可追踪
let currentState = 'idle';
currentState = 'playing'; // 这行代码可以出现在项目的任何地方
```

再看用 Event 的写法：

```js
let currentState = 'idle';

// 所有状态变化都必须"报备"发生了什么事件
function send(event) {
  if (currentState === 'idle' && event.type === 'PLAY') {
    currentState = 'playing';
  } else if (currentState === 'playing' && event.type === 'STOP') {
    currentState = 'idle';
  }
  // 其余组合：忽略，不合法
}

send({ type: 'PLAY' });
console.log(currentState); // 'playing'
```

注意 `event` 是一个**对象**，长得像 `{ type: 'PLAY' }`，不是裸字符串 `'PLAY'`。为什么要多包一层对象？因为事件除了"发生了什么"，往往还要带"额外信息"，比如 `{ type: 'SEEK', position: 30 }`。如果事件只能是字符串，就没地方放这个 `position`。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const playerMachine = setup({}).createMachine({
  id: 'player',
  initial: 'idle',
  states: {
    idle: {
      on: { PLAY: { target: 'playing' } },
    },
    playing: {
      on: {
        STOP: { target: 'idle' },
        PAUSE: { target: 'paused' },
      },
    },
    paused: {
      on: { PLAY: { target: 'playing' } },
    },
  },
});

const player = createActor(playerMachine).start();

player.send({ type: 'PLAY' });   // idle -> playing
player.send({ type: 'PAUSE' });  // playing -> paused
player.send({ type: 'STOP' });   // paused 状态下没有 STOP，事件被忽略，仍是 paused
```

最后一行很关键：`paused` 状态下没有定义 `STOP` 事件的处理，发送它**不会报错，也不会改变状态**，只是被静默忽略。这正是"图里没画的路，就是不存在的路"——第 1 课强调的约束在这里生效了。

## 6 TypeScript 语法拆解

```ts
on: { PLAY: { target: 'playing' } },
```
`on` 对象的 key 是事件名（字符串），必须跟你 `send` 时用的 `type` 值完全对应。

```ts
player.send({ type: 'PLAY' });
```
`send` 接收一个对象，而不是字符串。这个对象至少要有 `type` 字段。目前这个字段的类型是"任意字符串"（因为还没用 `setup({ types })` 约束，第 5 课起会看到），所以 TypeScript 现在不会帮你检查拼错的事件名——这也是为什么第 20 课会讲字面量类型如何堵住这个漏洞。

```ts
type: 'PLAY'
```
这是一个**字符串字面量**（literal），不是普通的 `string` 类型。普通 `string` 类型允许任意字符串，`'PLAY'` 这个字面量类型只允许恰好是 `"PLAY"` 这一个值。现在你只需要知道：**事件的 `type` 字段用字面量类型来标注，是为了让 TypeScript 能在编译期帮你检查"这个事件名是不是拼对了、机器认不认识"。** 第 20 课会专门拆解字面量类型的语法。

## 7 常见错误

**错误 1：直接发送字符串而不是对象。**

```ts
// ❌ send 需要对象
player.send('PLAY');
```
XState v5 的类型定义会直接在编译期报错拒绝这行代码，这是刻意为之（见第 8 节）。

**错误 2：以为发送了未定义的事件会报错。**

```ts
player.send({ type: 'FLY_TO_MOON' }); // 不报错，什么都不会发生
```
状态机对"图里没画的事件"的默认反应是**忽略**，不是抛异常。如果你需要"收到非法事件要报错"，需要自己在 Action 里显式处理，这不是状态机的默认行为。

**错误 3：把事件名和状态名混着用，或者事件名用小写、状态名也用小写，导致读代码时分不清谁是谁。**

约定俗成的做法是：事件名全大写 + 下划线（`PLAY`、`FETCH_SUCCESS`），状态名小写驼峰（`idle`、`fetchingData`）。一眼就能分辨。

## 8 为什么官方这样设计

这背后是"单向数据流"思想（和 Redux 的 action 概念几乎是同一个哲学）：**任何状态变化都必须先有一个显式、可记录、可回放的"发生了什么"，而不是允许代码在任意地方直接赋值改状态。**

好处是可追溯性：出问题时，你可以把整个事件序列打印出来（`PLAY` → `PAUSE` → `STOP`），重新推演状态机是怎么走到当前这个状态的。如果允许直接改 State，这种回放能力就没有了——你不知道"谁"、"什么时候"、"为什么"把状态改了。

## 9 最佳实践

1. 事件名全大写 + 下划线，一眼区分于状态名。
2. 事件名用"发生了什么"而不是"要做什么"来命名：`PLAY`（用户按下播放）比 `SET_STATE_PLAYING`（命令式地指定目标状态）更符合事件驱动的思想。
3. 需要携带数据的事件，把数据放进事件对象本身，比如 `{ type: 'SEEK', position: 30 }`，不要用全局变量传递。
4. 不要假设"发送了事件就一定会触发状态变化"——目标状态没监听这个事件时，事件会被静默忽略，这是正常行为。
5. 第 5 课之前不需要担心事件类型的完整 TypeScript 标注；先把事件驱动的思维方式建立起来，类型是锦上添花，不是必需品。

## 10 一句话总结

**Event 是"发生了什么"的显式声明，状态机只能被 Event 驱动，不能被直接赋值。**

## 11 设计思考（Design Thinking）

**需求**：播放器需要响应"用户点击播放"。

**方案 A（推荐）**：定义 `PLAY` 事件，由状态机决定收到后做什么。

```ts
player.send({ type: 'PLAY' });
```

**方案 B（错误）**：业务代码直接调用一个"设置状态"的函数。

```ts
// ❌ 命令式地指定目标状态，跳过了状态机的判断
function onPlayButtonClick() {
  setPlayerState('playing'); // 不管当前是什么状态，强行设置
}
```

方案 B 的问题是：调用方需要自己知道"现在能不能变成 playing"，状态机变成了被动接受指令的容器，而不是主动做决策的主体。如果播放器正处于"加载中"（`loading`）状态，方案 B 依然会强行把它改成 `playing`，产生不一致的状态；方案 A 里，`loading` 状态如果没监听 `PLAY` 事件，这次点击会被自然、安全地忽略，完全不需要业务代码自己判断"现在能不能播放"。

## 12 设计原则（Design Rules）

本课新增：

- **Event 表示外部输入（Input），状态机永远是"被事件驱动"而不是"被直接赋值"。**
- **未被当前状态监听的事件会被静默忽略，这是特性不是 bug。**
- **事件命名用"发生了什么"（PLAY），不要用"要做什么的指令"（SET_PLAYING）。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[04 Transition——事件如何真正改变状态](04-transition.md)
