# 13 Actor 思想：流程 vs 工作

> 上一课：[12 Parallel State](12-parallel-state.md) · 下一课：[14 invoke](14-invoke.md)

## 1 为什么需要

登录这个场景：用户点击"登录"按钮后，需要发一个网络请求，等服务器响应之后才知道是成功还是失败。这里有两件性质完全不同的事：

1. **"当前处于哪个登录阶段"**——空闲、请求中、成功、失败。这是第 1-12 课一直在讲的 FSM 范畴：有限模式 + 事件驱动的转移。
2. **"真正发起并等待这个网络请求"**——这是一个异步的、有始有终的"工作"，本身不是一个状态图，而是一个会执行、会返回结果的任务。

如果把第二件事也硬塞进状态机的 Action 里直接写 `fetch(...).then(...)`，你会发现："请求成功后要 send 一个事件"这件事变得难以追踪、难以取消、难以和状态机的生命周期对齐（比如用户在请求还没返回时就离开了登录页，这个请求该怎么办？）。需要一个概念，把"独立运行、能产生结果、可以被启动和停止"的工作，和"负责决策流程走向"的状态机区分开。

## 2 核心思想

**Machine 负责"流程"（现在该做什么、下一步去哪），Actor 负责"工作"（具体去执行一件事，完成后把结果汇报回来）——两者是分工关系，不是包含关系。**

## 3 ASCII 图

```
+----------------------+          +------------------------+
| Machine (登录流程)     |          | Actor (真正发请求的工作) |
|                        |  invoke  |                          |
|  idle -> loading  -----+--------->|  fetch('/api/login')    |
|         |               |         |  等待网络返回...          |
|         |<--- onDone ---+---------|  返回 { token }          |
|         v                         +------------------------+
|  success / failure                                          |
+----------------------+
```

Machine 决定"什么时候该发请求、请求返回后该去哪个状态"，Actor 负责"真正把请求发出去、等结果回来"。

## 4 JS 思维

先看一个**没有分工**、把工作直接塞进流程判断里的写法：

```js
let current = 'idle';

function send(event) {
  if (current === 'idle' && event.type === 'SUBMIT') {
    current = 'loading';
    // 工作和流程判断混在一起：这里直接发请求
    fetch('/api/login', { method: 'POST' })
      .then((res) => res.json())
      .then((data) => {
        current = 'success'; // 请求成功后，直接从"工作代码"里改流程状态
      })
      .catch(() => {
        current = 'failure';
      });
  }
}
```

这段代码能跑，但它把"决定该不该发请求"和"真正怎么发这个请求、怎么处理结果"糅在了一起——如果以后想把"登录方式"从 `fetch` 换成 `XMLHttpRequest`，或者想在请求进行中支持"取消"，都得直接改这个判断逻辑内部。

分工之后的版本（先用最朴素的方式模拟"工作"是一个独立单元）：

```js
// "工作"独立封装成一个函数，不关心自己是被谁调用的
function loginWork(credentials, onSuccess, onFailure) {
  fetch('/api/login', { method: 'POST', body: JSON.stringify(credentials) })
    .then((res) => res.json())
    .then(onSuccess)
    .catch(onFailure);
}

// "流程"只负责决定何时启动工作、工作完成后去哪
let current = 'idle';
function send(event) {
  if (current === 'idle' && event.type === 'SUBMIT') {
    current = 'loading';
    loginWork(
      event.credentials,
      () => { current = 'success'; },
      () => { current = 'failure'; },
    );
  }
}
```

这就是 Actor 思想的雏形：**"工作"被抽成独立单元，流程只负责"什么时候启动它、它完成后走哪条路"，不关心工作内部具体怎么做。**

## 5 XState v5 写法（先看形状，细节下一课展开）

```ts
import { setup, fromPromise, createActor } from 'xstate';

const loginMachine = setup({
  actors: {
    loginWork: fromPromise(async ({ input }: { input: { username: string; password: string } }) => {
      const res = await fetch('/api/login', {
        method: 'POST',
        body: JSON.stringify(input),
      });
      if (!res.ok) throw new Error('登录失败');
      return res.json(); // 这个返回值会成为 onDone.output
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
        input: ({ event }) => {
          if (event.type !== 'SUBMIT') throw new Error('unexpected event');
          return { username: event.username, password: event.password };
        },
        onDone: { target: 'success' },
        onError: { target: 'failure' },
      },
    },
    success: {},
    failure: {
      on: { SUBMIT: { target: 'loading' } },
    },
  },
});

const login = createActor(loginMachine).start();
login.send({ type: 'SUBMIT', username: 'alice', password: '123456' });
```

本课先建立"Machine 管流程、Actor 管工作"这个心智模型；`invoke`/`fromPromise` 的具体语法细节，第 14、15 课会逐个拆解。

## 6 TypeScript 语法拆解

本课重点在思想而非语法，这里只点出和"分工"直接相关的两处：

```ts
actors: {
  loginWork: fromPromise(async ({ input }) => { ... }),
},
```
`setup({ actors: {...} })` 是"工作"的注册表，和 `actions`/`guards` 并列——这在结构上就体现了"工作"是和"副作用""条件判断"平级的、独立于流程判断的另一类概念。

```ts
loading: {
  invoke: { src: 'loginWork', ... },
},
```
`invoke` 写在状态节点上，意思是"进入这个状态后，启动对应的工作"；工作本身（`loginWork` 具体怎么实现）完全不在这个 Machine 的配置里出现，只出现在 `actors` 注册表里——这就是"流程配置"和"工作实现"在代码层面的物理分离。

## 7 常见错误

**错误 1：把异步请求直接写进 Action 函数体，而不是用 Actor 建模。**

```ts
// ❌ Action 应该是同步、简单的副作用（第 6 课），不该是异步工作的载体
actions: {
  doLogin: ({ event }) => {
    fetch('/api/login', { ... }).then(...); // 状态机无法感知这个异步操作的生命周期
  },
},
```
这样写的问题是：状态机完全不知道这个 `fetch` 什么时候完成、有没有出错，也没法在合适的时机自动取消它（比如用户中途导航离开）。

**错误 2：认为"只要是异步的就该用 Actor"，把所有异步操作不分场景地都塞进 invoke，即便它们和"流程"毫无关系。**

如果一个异步操作的结果完全不影响状态机该往哪走（比如"上报一次埋点，成不成功都无所谓"），用普通的 fire-and-forget 方式处理即可，不必强行建模成 Actor——Actor 存在的意义是"流程需要感知这件工作的进度或结果"，不是"给所有异步代码找个归宿"。

**错误 3：认为 Actor 只能用来处理网络请求。**

Actor 可以是任何"独立运行、产生结果或持续产生事件"的单元：一个 WebSocket 连接、一个定时轮询、甚至是另一个完整的状态机（子 Machine）。第 15、16 课会看到不同形态的 Actor。

## 8 为什么官方这样设计

这套分工是 XState v5 相比 v4 更强调的核心设计思想：**FSM 擅长的是"有限、离散的模式决策"，不擅长（也不该尝试）表达"一段持续运行、产生连续副作用的工作过程"。** 把这两件事分开，各自用最合适的抽象去表达——决策用状态图，工作用 Actor——比试图用一个模型（要么全用状态图硬建模所有异步细节，要么完全不用状态图、所有东西写成回调地狱）都更清晰。这也让 Machine 本身保持"纯粹描述流程决策"的特性：读一个 Machine 的配置，你能看懂"什么时候该做什么决定"，而不需要同时读懂某个具体工作的实现细节。

## 9 最佳实践

1. 先问："这个异步操作的结果，是否会影响状态机该往哪个方向走？" 会 → 该用 Actor；不会 → 普通的 fire-and-forget 处理即可。
2. 保持 Machine 配置只描述"流程决策"，具体的工作实现（怎么发请求、怎么解析响应）放进独立注册的 Actor 逻辑里。
3. 不要因为"用了 Actor 看起来更专业"就滥用；简单的同步副作用继续用 Action（第 6 课）就够了。
4. 把"工作"设计成可以脱离某个具体 Machine 单独测试的单元——这也是"流程"和"工作"分离带来的额外好处：工作逻辑可以独立编写单元测试。
5. 想清楚"工作"失败时，流程该往哪个方向走（对应第 5 节例子里的 `onError`），不要只考虑成功路径。

## 10 一句话总结

**Machine 决定"现在该做什么决定"，Actor 负责"真正把某件独立的工作做完并汇报结果"，两者分工协作，不要混为一谈。**

## 11 设计思考（Design Thinking）

**需求**：下载器需要发起一个耗时的文件下载任务，下载过程中显示进度，完成后转到"已完成"，失败后转到"失败"并允许重试。

**方案 A（推荐）**：Machine 只负责 `idle` → `downloading` → `completed`/`failed` 之间的流程决策；真正的下载逻辑（打开连接、追踪字节数、处理网络错误）封装成一个独立的 Actor，通过 `invoke` 挂到 `downloading` 状态上。

```ts
downloading: {
  invoke: { src: 'downloadWork', onDone: { target: 'completed' }, onError: { target: 'failed' } },
}
```

**方案 B（错误）**：把整个下载过程的所有细节（网络请求、进度追踪、错误处理、重试计数）都试图用纯状态图的 State 和 Transition 表达，比如给"每 10% 的下载进度"都建一个状态。

```ts
// ❌ 试图用状态图表达连续变化的进度和复杂的异步细节
states: {
  downloading0to10: {},
  downloading10to20: {},
  // ... 继续列举
  downloading90to100: {},
}
```

方案 B 是在用错误的工具做错误的事：下载进度是连续变化的数据（应该在 Actor 内部通过事件汇报，或者放进 Context），不是有限的模式；把它硬塞进状态图会导致状态数量爆炸，而且状态图完全无法表达"网络中断了该怎么重试"这种真正需要异步工作模型（Promise、Callback 等）才能自然表达的逻辑。方案 A 让状态图只回答"现在是不是在下载中"这个粗粒度的模式问题，下载的所有细节交给专门设计来处理异步工作的 Actor 去做，各司其职。

## 12 设计原则（Design Rules）

本课新增：

- **Machine 负责流程决策，Actor 负责具体工作，两者是分工协作关系，不要用一个模型硬做另一个模型该做的事。**
- **判断该不该用 Actor 的标准：这个操作的结果是否会影响流程走向？会 → Actor；不会 → 普通副作用即可。**
- **连续变化的进度类数据不应该体现为状态图节点，应放进 Context 或由 Actor 内部管理（呼应第 5 课 State vs Context 的分工）。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[14 invoke——把工作委托出去](14-invoke.md)
