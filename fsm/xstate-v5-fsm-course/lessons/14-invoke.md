# 14 invoke：把工作委托出去

> 上一课：[13 Actor 思想](13-actor-model.md) · 下一课：[15 fromPromise / fromCallback](15-frompromise-fromcallback.md)

## 1 为什么需要

上一课确立了"Machine 管流程、Actor 管工作"的分工，但具体怎么把一个 Actor 和状态机的某个状态"绑定"起来？需要回答几个具体问题：工作什么时候启动？工作正常完成后流程该去哪？工作失败了又该去哪？工作还没完成时，状态机提前离开了这个状态，工作该怎么办？`invoke` 就是用来回答这些问题的机制。

## 2 核心思想

**`invoke` 是把一个 Actor 的生命周期绑定到某个状态节点上：进入状态时自动启动，状态被离开时自动停止，工作完成或失败会自动转化成状态机能响应的事件。**

## 3 ASCII 图

```
+---------------------------------------------------+
| loading (状态)                                      |
|                                                      |
|   invoke: loginWork  ---启动---> [Actor 开始执行]      |
|                                                      |
|   onDone -----------------------> success           |
|   onError ----------------------> failure           |
+---------------------------------------------------+
      |
      | 如果在工作完成前，因为别的事件离开了 loading 状态
      v
   [Actor 被自动停止，不会在离开后还悄悄触发 onDone/onError]
```

## 4 JS 思维

```js
let current = 'idle';
let cancelled = false; // 模拟"取消"的标记

function send(event) {
  if (current === 'idle' && event.type === 'SUBMIT') {
    current = 'loading';
    cancelled = false;

    fetch('/api/login')
      .then((res) => res.json())
      .then((data) => {
        if (cancelled) return; // 手动检查：如果已经离开了 loading，不要再改状态
        current = 'success';
      })
      .catch(() => {
        if (cancelled) return;
        current = 'failure';
      });
  }

  if (current === 'loading' && event.type === 'CANCEL') {
    cancelled = true; // 必须手动记住"要忽略稍后才到达的结果"
    current = 'idle';
  }
}
```

注意 `cancelled` 这个标记——如果没有它，即使用户已经取消、离开了 `loading` 状态，稍后姗姗来迟的网络响应仍然会把 `current` 强行改回 `success`/`failure`，这是异步代码里非常经典也非常隐蔽的 bug。`invoke` 的价值之一，就是把这种"手动记录要不要忽略过期结果"的心智负担，变成状态机自动处理的能力。

## 5 XState v5 写法

```ts
import { setup, fromPromise, createActor } from 'xstate';

const loginMachine = setup({
  actors: {
    loginWork: fromPromise(async () => {
      const res = await fetch('/api/login');
      if (!res.ok) throw new Error('登录失败');
      return res.json();
    }),
  },
}).createMachine({
  id: 'login',
  initial: 'idle',
  states: {
    idle: {
      on: { SUBMIT: { target: 'loading' } },
    },
    loading: {
      invoke: {
        src: 'loginWork',
        onDone: {
          target: 'success',
          actions: ({ event }) => console.log('登录结果:', event.output),
        },
        onError: {
          target: 'failure',
          actions: ({ event }) => console.log('登录出错:', event.error),
        },
      },
      on: {
        CANCEL: { target: 'idle' }, // 一旦离开 loading，XState 自动停止上面的 Actor
      },
    },
    success: {},
    failure: {
      on: { SUBMIT: { target: 'loading' } },
    },
  },
});

const login = createActor(loginMachine).start();
login.send({ type: 'SUBMIT' });
// 如果在请求返回之前发送 CANCEL，XState 会自动停止这次 invoke，
// 就算网络请求本身还在后台跑，其结果也不会再触发 onDone/onError。
```

## 6 TypeScript 语法拆解

```ts
invoke: {
  src: 'loginWork',
  onDone: { target: 'success', actions: ... },
  onError: { target: 'failure', actions: ... },
},
```
`src` 指定要启动的 Actor 逻辑（对应 `setup({ actors: {...} })` 里注册的名字）。`onDone`/`onError` 是两个特殊的转移配置，分别对应"这个 Actor 正常完成"和"这个 Actor 出错"两种情况，写法和普通 `on` 里的转移一样（可以有 `target`、`actions`、`guard`）。

```ts
actions: ({ event }) => console.log('登录结果:', event.output),
```
在 `onDone` 的 `actions` 里，`event.output` 就是这个 Actor 完成时返回的结果（对应第 15 课会细讲的 `fromPromise` 里 `return` 的值）。在 `onError` 里，对应字段是 `event.error`。

```ts
on: { CANCEL: { target: 'idle' } },
```
这条转移和 `invoke` 没有直接的语法关联，但**只要它触发、导致状态机离开 `loading`，XState 就会自动停止这个状态上正在运行的 Actor**——这是 `invoke` 语义里内置的行为，不需要你写任何额外代码去"取消"它。

## 7 常见错误

**错误 1：以为停止 invoke 会连带取消底层真正的网络请求。**

`invoke` 停止后，XState 层面不会再理会这个 Actor 稍后产生的结果（不会触发 `onDone`/`onError`），但如果底层实现（比如 `fetch`）本身没有实现真正的网络层取消（`AbortController`），请求可能仍在网络层继续进行，只是结果被状态机忽略了。如果需要真正中断网络请求，需要在 Actor 内部自己接入取消机制（第 15 课会看到 `fromCallback` 如何配合清理函数处理这类场景）。

**错误 2：只写了 `onDone`，忘记写 `onError`。**

```ts
// ❌ 忘记处理错误路径，请求失败时流程会卡住，没有任何转移
loading: {
  invoke: { src: 'loginWork', onDone: { target: 'success' } },
},
```
异步工作几乎总是"可能失败"的，`onError` 不是可选项，是默认应该考虑的路径。

**错误 3：把不该由流程决定的重试逻辑，全部塞进 `onError` 的 `actions` 里做隐式重试，而不是转移到一个明确的 `failure`/`retrying` 状态。**

```ts
// ❌ 在 action 里悄悄重新发起工作，状态图完全看不出"这里其实会自动重试"
onError: {
  actions: () => { /* 在这里悄悄重新触发 loginWork，没有对应的状态变化 */ },
},
```
重试应该体现为一次明确的状态转移（比如回到 `loading` 或进入专门的 `retrying` 状态），保持"所有行为在图上可见"的一贯原则（呼应第 4 课）。

## 8 为什么官方这样设计

把 Actor 的启动和停止直接绑定到状态节点的生命周期上（进入即启动、离开即停止），是为了从根本上消除"异步结果延迟到达、但当时的上下文已经不适用了"这一整类 bug（第 4 节 JS 思维里手写的 `cancelled` 标记，就是在手动模拟 XState 自动帮你做的事）。这背后的哲学和第 8 课的 `after` 定时器自动清理是同一个思路：**任何绑定在状态生命周期上的资源（无论是定时器还是 Actor），都应该随着状态的离开被自动、可靠地清理，而不是依赖开发者手动记账。**

## 9 最佳实践

1. 几乎每个 `invoke` 都应该同时写 `onDone` 和 `onError`，除非你非常确定这个工作不可能失败。
2. 不要假设"停止 invoke"等于"底层操作被真正中断"——如果需要真正的取消语义（比如中止网络请求），要在 Actor 实现内部处理。
3. 重试逻辑应该体现为明确的状态转移，而不是藏在 `onError` 的 action 里悄悄发生。
4. `onDone`/`onError` 的 `actions` 里如果需要更新 Context，记得用 `assign`（第 5 课），不要直接赋值。
5. 一个状态上通常只 `invoke` 一个核心的 Actor；如果发现需要同时启动好几个独立的工作，考虑它们是否该拆分到不同的状态，或者是否该用 `spawn`（第 16 课）动态管理多个并存的 Actor。

## 10 一句话总结

**`invoke` 把 Actor 的启动/停止绑定到状态的生命周期上，进入自动启动、离开自动停止，完成或出错自动转化成状态机能响应的事件。**

## 11 设计思考（Design Thinking）

**需求**：用户提交登录表单后，请求进行中如果用户点击"取消"，应该立刻回到表单页，且不希望姗姗来迟的网络响应再影响界面。

**方案 A（推荐）**：用 `invoke` 把登录请求挂在 `loading` 状态上，`CANCEL` 事件转移离开 `loading`，依赖 XState 自动停止已挂起的 Actor。

```ts
loading: {
  invoke: { src: 'loginWork', onDone: {...}, onError: {...} },
  on: { CANCEL: { target: 'idle' } },
}
```

**方案 B（错误）**：手动维护一个"当前请求 id"或"是否已取消"的标记变量，在 Promise 的 `.then` 里手动检查，决定要不要更新状态（就是第 4 节 JS 思维里那种写法，直接照搬到 XState 的 Action 里）。

```ts
// ❌ 在 XState 里重新发明 invoke 已经内置解决的问题
let requestId = 0;
actions: {
  doLogin: () => {
    const myId = ++requestId;
    fetch('/api/login').then((res) => {
      if (myId !== requestId) return; // 手动判断是否过期
      // ...
    });
  },
},
```

方案 B 完全绕开了 `invoke` 提供的自动生命周期管理，自己重新实现了一遍"判断结果是否已经过期"的逻辑，代码量更多、更容易出错（比如忘记在某个分支更新 `requestId`），而且这部分逻辑散落在 Action 函数体内部，状态图本身完全看不出"这里有一个正在进行、可能被取消的异步工作"。方案 A 让这层生命周期管理完全由 XState 负责，状态图上 `invoke` 字段本身就清楚地表达了"这个状态对应一个进行中的工作"。

## 12 设计原则（Design Rules）

本课新增：

- **`invoke` 绑定 Actor 生命周期到状态节点：进入启动、离开停止，不需要手动记录"是否已过期"之类的标记。**
- **几乎所有 `invoke` 都应该同时处理 `onDone` 和 `onError`，异步工作默认是"可能失败"的。**
- **`invoke` 停止不等于底层操作被真正中断；真正的取消需要在 Actor 实现内部处理。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[15 fromPromise / fromCallback——两种常见的工作类型](15-frompromise-fromcallback.md)
