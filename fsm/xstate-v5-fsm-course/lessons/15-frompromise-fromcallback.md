# 15 fromPromise / fromCallback：两种常见的工作类型

> 上一课：[14 invoke](14-invoke.md) · 下一课：[16 spawn](16-spawn.md)

## 1 为什么需要

上一课的 `invoke.src` 指向一个"Actor 逻辑"，但这个逻辑具体长什么样？现实中的"工作"大致分两类：**有明确终点、只会产生一个最终结果**的（比如一次 HTTP 请求，要么成功要么失败，结束后就完事了），和**没有固定终点、会持续产生一连串事件**的（比如一个 WebSocket 连接、一个持续的心跳检测）。这两类工作的"形状"不一样，需要两种不同的构造方式，这就是 `fromPromise` 和 `fromCallback` 存在的原因。

## 2 核心思想

**`fromPromise` 用来包装"一次性、有明确成功/失败结果"的工作；`fromCallback` 用来包装"持续运行、随时可能产生多个事件、需要显式清理"的工作。**

## 3 ASCII 图

```
fromPromise（一次性工作）：
  启动 ---> 执行中 ---> 成功（一个结果）
                   \\--> 失败（一个错误）

fromCallback（持续性工作）：
  启动 ---> 持续运行，随时 sendBack(event) ---> ... ---> ... 
                   \\
                    --> 状态机决定停止时，自动调用清理函数
```

## 4 JS 思维

一次性工作（对应 `fromPromise`）：

```js
function loginWork() {
  return fetch('/api/login').then((res) => res.json()); // 一个 Promise，一定会 resolve 或 reject 一次
}
```

持续性工作（对应 `fromCallback`）：

```js
function heartbeatWork(sendEventBack) {
  const timer = setInterval(() => {
    sendEventBack({ type: 'HEARTBEAT_TICK' }); // 会反复触发很多次，不是"一次性结果"
  }, 5000);

  return function cleanup() {
    clearInterval(timer); // 必须提供一个"怎么停止"的方式
  };
}
```

两者的核心区别：一次性工作用返回值（或抛出的错误）表达结果，天然适合用 Promise 表达；持续性工作没有"单一返回值"这个概念，它需要一种方式"随时往外发事件"，并且必须明确"怎么停止自己"（否则会像第 8 课强调的那样发生资源泄漏）。

## 5 XState v5 写法

**`fromPromise`**——包装一次性工作：

```ts
import { setup, fromPromise, createActor } from 'xstate';

const loginMachine = setup({
  actors: {
    loginWork: fromPromise(async ({ input }: { input: { username: string } }) => {
      const res = await fetch(`/api/login?user=${input.username}`);
      if (!res.ok) throw new Error('登录失败');
      return res.json(); // 这个值会成为 onDone 的 event.output
    }),
  },
}).createMachine({
  id: 'login',
  initial: 'idle',
  states: {
    idle: { on: { SUBMIT: { target: 'loading' } } },
    loading: {
      invoke: {
        src: 'loginWork',
        input: ({ event }) => {
          if (event.type !== 'SUBMIT') throw new Error('unexpected');
          return { username: event.username };
        },
        onDone: { target: 'success' },
        onError: { target: 'failure' },
      },
    },
    success: {},
    failure: {},
  },
});
```

**`fromCallback`**——包装持续性工作（下载器的进度上报）：

```ts
import { setup, fromCallback, createActor, assign } from 'xstate';

const downloaderMachine = setup({
  types: { context: {} as { progress: number } },
  actors: {
    downloadWork: fromCallback(({ sendBack, receive }) => {
      const timer = setInterval(() => {
        sendBack({ type: 'PROGRESS_TICK' }); // 持续、多次地往状态机发事件
      }, 500);

      receive((event) => {
        if (event.type === 'PAUSE_REQUESTED') {
          clearInterval(timer); // 也可以监听外部发来的事件
        }
      });

      return () => clearInterval(timer); // 清理函数：状态机离开时自动调用
    }),
  },
  actions: {
    incrementProgress: assign({
      progress: ({ context }) => Math.min(context.progress + 10, 100),
    }),
  },
}).createMachine({
  id: 'downloader',
  initial: 'idle',
  context: { progress: 0 },
  states: {
    idle: { on: { START: { target: 'downloading' } } },
    downloading: {
      invoke: { src: 'downloadWork' },
      on: {
        PROGRESS_TICK: { actions: 'incrementProgress' },
        CANCEL: { target: 'idle' }, // 离开时，XState 自动调用上面 fromCallback 返回的清理函数
      },
    },
  },
});

const downloader = createActor(downloaderMachine).start();
downloader.send({ type: 'START' });
```

## 6 TypeScript 语法拆解

```ts
fromPromise(async ({ input }: { input: { username: string } }) => { ... })
```
`fromPromise` 接收一个函数，这个函数会拿到 `{ input }`（`invoke.input` 传进来的值），必须返回一个 Promise（用 `async` 修饰的函数天然返回 Promise）。这个 Promise `resolve` 的值会成为 `onDone` 事件的 `event.output`；如果 `throw` 或 `reject`，会成为 `onError` 事件的 `event.error`。

```ts
fromCallback(({ sendBack, receive }) => { ... return () => {...}; })
```
`fromCallback` 接收一个函数，拿到 `{ sendBack, receive }`（以及 `input`，本例没用到）：
- `sendBack(event)`：主动把一个事件发送回状态机，可以调用任意多次（这正是它适合"持续性工作"的原因）。
- `receive(callback)`：反过来，监听状态机（或其他 Actor）发给这个工作的事件。
- **函数的返回值应该是一个"清理函数"**（本例是 `() => clearInterval(timer)`），状态机离开对应状态、停止这个 Actor 时，XState 会自动调用它——这是 `fromCallback` 对"我怎么知道该怎么停止自己"这个问题的标准回答。

```ts
onDone: { target: 'success' } // 只适用于 fromPromise 这类"有终点"的 Actor
```
`fromCallback` 包装的工作**没有"完成"的概念**（它是持续运行的，直到被外部停止），所以不会触发 `onDone`；`fromPromise` 包装的工作**一定会**在某个时刻 resolve 或 reject 一次，`onDone`/`onError` 只对这类 Actor 有意义。

## 7 常见错误

**错误 1：给 `fromCallback` 忘记返回清理函数，导致状态机离开后定时器/连接仍在后台运行（资源泄漏）。**

```ts
// ❌ 没有 return 清理函数，setInterval 永远不会被清除
fromCallback(({ sendBack }) => {
  setInterval(() => sendBack({ type: 'TICK' }), 1000);
  // 缺少 return () => clearInterval(...)
});
```

**错误 2：用 `fromPromise` 包装一个本质上是持续性的工作，试图靠"反复 resolve"来模拟持续事件。**

```ts
// ❌ fromPromise 的语义是"一次性结果"，不适合表达持续的心跳
fromPromise(async () => {
  while (true) {
    await new Promise((r) => setTimeout(r, 5000));
    // 这里没有办法把每一次 tick 都单独汇报给状态机，只能等最终结果
  }
});
```
持续性、会多次产生事件的工作，应该用 `fromCallback`（或更进阶的 `fromObservable`，超出本课范围），不要用 `fromPromise` 硬凑。

**错误 3：在 `fromPromise` 里吞掉错误，导致失败被误判为成功。**

```ts
// ❌ catch 住错误却返回了一个"看起来正常"的值，onError 永远不会触发
fromPromise(async () => {
  try {
    const res = await fetch('/api/login');
    return res.json();
  } catch {
    return null; // 应该重新 throw，而不是悄悄返回 null 假装成功
  }
});
```

## 8 为什么官方这样设计

`fromPromise` 和 `fromCallback` 分别对应 JavaScript 里两种最常见、最基础的异步原语（Promise 和"手动控制的回调 + 清理函数"），XState 没有发明新的异步范式，而是为这两种你已经熟悉的模式各自提供了一层"如何和状态机对接"的标准适配器。这样设计的好处是：你不需要为了使用 XState 而学习一套全新的异步编程方式，只需要知道"我的工作是一次性的还是持续性的"，然后选择对应的 `from*` 函数包装即可——这也是为什么官方文档里管这些叫"actor logic creators"（Actor 逻辑构造器）：它们负责把"普通的 JS 异步模式"转换成"XState 能理解、能管理生命周期的 Actor"。

## 9 最佳实践

1. 先判断工作性质：一次性、有明确成功/失败结果 → `fromPromise`；持续运行、可能多次产生事件、需要显式停止 → `fromCallback`。
2. `fromCallback` 几乎总是需要返回一个清理函数，写完第一行代码就应该想清楚"怎么停止"，而不是写完主逻辑才补。
3. `fromPromise` 里如果 catch 住了错误，要么重新 `throw`，要么明确设计成"这种情况就是要走成功路径"，不要含糊地吞掉错误又假装成功。
4. `fromCallback` 的 `sendBack` 发出的事件类型要在 Machine 里对应处理（第 5 节例子中的 `PROGRESS_TICK`），确保状态机知道该怎么响应。
5. 复杂的、有多个阶段的持续性工作，考虑内部再拆一层状态判断，或者干脆用一个子 Machine 通过 `invoke` 挂载（`invoke.src` 也可以直接是另一个 Machine，超出本课范围，先知道有这个可能性）。

## 10 一句话总结

**`fromPromise` 包装一次性、有明确结果的工作；`fromCallback` 包装持续运行、需要显式清理的工作——按工作的"形状"选择对应的构造器。**

## 11 设计思考（Design Thinking）

**需求**：下载器需要持续汇报下载进度，同时登录需要发一次性的请求。

**方案 A（推荐）**：登录用 `fromPromise`（一次性结果），下载进度汇报用 `fromCallback`（持续事件），各自选择匹配自己"形状"的构造器。

```ts
loginWork: fromPromise(async ({ input }) => { ... return result; }),
downloadWork: fromCallback(({ sendBack }) => { const t = setInterval(...); return () => clearInterval(t); }),
```

**方案 B（错误）**：图省事，统一都用 `fromPromise`，下载进度试图靠"每 10% 就单独 invoke 一次新的 Promise"来拼凑出"持续汇报"的效果。

```ts
// ❌ 用一次性工具拼凑持续性场景，代码别扭且难以表达"暂停/恢复"
downloading10Percent: { invoke: { src: 'downloadChunk', onDone: { target: 'downloading20Percent' } } },
downloading20Percent: { invoke: { src: 'downloadChunk', onDone: { target: 'downloading30Percent' } } },
```

方案 B 不仅需要为每个进度区间单独建一个状态（回到第 12 课讨论过的"连续数据不该建模成状态"的老问题），而且"暂停下载"这种需要"随时响应外部指令、随时可以停止"的能力，`fromPromise` 完全没有对应的机制去表达（Promise 一旦开始执行，中途没有标准方式"暂停"它）。方案 A 用 `fromCallback` 的 `receive` 天然支持"随时接收暂停指令"，`sendBack` 天然支持"随时汇报进度"，工具和场景的形状严丝合缝，不需要额外的变通。

## 12 设计原则（Design Rules）

本课新增：

- **按工作的"形状"选构造器：一次性结果用 `fromPromise`，持续事件流用 `fromCallback`，不要用一个硬凑另一个的场景。**
- **`fromCallback` 必须返回清理函数，这是它对"如何停止自己"这个问题的强制回答，缺失会导致资源泄漏。**
- **`fromPromise` 里捕获错误后要么重新抛出、要么明确视为成功路径，不要模糊地吞掉。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[16 spawn——动态创建的 Actor](16-spawn.md)
