# 16 spawn：动态创建的 Actor

> 上一课：[15 fromPromise / fromCallback](15-frompromise-fromcallback.md) · 下一课：[17 生命周期](17-lifecycle.md)

## 1 为什么需要

`invoke`（第 14 课）绑定的是"某个状态节点对应固定一个 Actor"，进入这个状态就启动，离开就停止——数量和时机都是静态、写死在配置里的。但有些场景需要"运行时动态决定要开几个、什么时候开"：下载器可能同时下载好几个文件，每个文件对应一个独立运行的下载任务，数量取决于用户点了几次"添加下载"，不是配置阶段就能确定的固定数字。这就需要一种"在运行时按需创建 Actor"的能力，这就是 `spawn`。

## 2 核心思想

**`spawn` 是在运行时按需创建一个新的 Actor 实例，通常还需要把它的引用存进 Context，以便后续单独跟它通信；跟 `invoke` 的固定绑定不同，`spawn` 的数量和时机由业务逻辑动态决定。**

## 3 ASCII 图

```
+---------------------------------------------------+
| downloader (Machine)                                |
|                                                       |
|  context.downloads: [ ]                              |
|                                                       |
|  ADD_DOWNLOAD ---> spawn(downloadTaskLogic) ---> 存入 context.downloads
|  ADD_DOWNLOAD ---> spawn(downloadTaskLogic) ---> 存入 context.downloads
|  ADD_DOWNLOAD ---> spawn(downloadTaskLogic) ---> 存入 context.downloads
|                                                       |
|  每个 spawn 出来的 Actor 独立运行，互不干扰                |
+---------------------------------------------------+
```

## 4 JS 思维

```js
let downloads = []; // 保存所有"正在跑的下载任务"的引用

function addDownload(url) {
  const task = startDownloadTask(url); // 每调用一次，动态创建一个新的独立任务
  downloads.push(task); // 存起来，方便以后单独控制某一个（比如取消第 2 个）
}

addDownload('file1.zip');
addDownload('file2.zip');
addDownload('file3.zip');
console.log(downloads.length); // 3 —— 数量完全取决于调用了多少次，不是写死的
```

对比 `invoke`：`invoke` 更像是"函数定义时就写死要调用哪个函数"，`spawn` 更像是"运行时按需 `new` 出一个新实例"，两者的关系类似"静态绑定"和"动态创建"。

## 5 XState v5 写法

```ts
import { setup, fromCallback, assign, createActor, type ActorRefFrom } from 'xstate';

const downloadTaskLogic = fromCallback<{ type: 'CANCEL' }, { url: string }>(
  ({ input, sendBack, receive }) => {
    console.log(`开始下载: ${input.url}`);
    const timer = setTimeout(() => {
      sendBack({ type: 'DOWNLOAD_DONE', url: input.url } as any);
    }, 2000);

    receive((event) => {
      if (event.type === 'CANCEL') clearTimeout(timer);
    });

    return () => clearTimeout(timer);
  },
);

const downloaderMachine = setup({
  types: {
    context: {} as {
      downloads: ActorRefFrom<typeof downloadTaskLogic>[];
    },
    events: {} as { type: 'ADD_DOWNLOAD'; url: string },
  },
  actors: { downloadTaskLogic },
}).createMachine({
  id: 'downloader',
  initial: 'active',
  context: { downloads: [] },
  states: {
    active: {
      on: {
        ADD_DOWNLOAD: {
          actions: assign({
            downloads: ({ context, spawn, event }) => [
              ...context.downloads,
              spawn('downloadTaskLogic', { input: { url: event.url } }),
            ],
          }),
        },
      },
    },
  },
});

const downloader = createActor(downloaderMachine).start();
downloader.send({ type: 'ADD_DOWNLOAD', url: 'file1.zip' });
downloader.send({ type: 'ADD_DOWNLOAD', url: 'file2.zip' });
console.log(downloader.getSnapshot().context.downloads.length); // 2
```

## 6 TypeScript 语法拆解

```ts
downloads: ActorRefFrom<typeof downloadTaskLogic>[]
```
`ActorRefFrom<T>` 是 XState 提供的工具类型，给定某个 Actor 逻辑的类型，推导出"指向这个逻辑创建出的实例"的引用类型。这里的意思是"`downloads` 是一个数组，每一项都是指向 `downloadTaskLogic` 实例的引用"。`typeof downloadTaskLogic` 是取变量 `downloadTaskLogic` 的类型（第 25 课会更系统讲泛型，这里先知道这行代码在描述"数组里装的是什么类型的引用"）。

```ts
assign({
  downloads: ({ context, spawn, event }) => [
    ...context.downloads,
    spawn('downloadTaskLogic', { input: { url: event.url } }),
  ],
}),
```
这是本课最关键的一行。`assign` 的回调函数除了 `context`/`event`，在 XState v5 里还能拿到 `spawn` 这个函数（专门在 `assign` 内部使用，用来创建新 Actor 并同时把引用放进返回的 Context 里）。`spawn('downloadTaskLogic', { input: {...} })` 立即创建一个新的、独立运行的 Actor 实例，返回值是指向它的引用，随后通过展开运算符 `...context.downloads` 拼接进一个新数组，赋回 `downloads` 字段——这保证了每次 `ADD_DOWNLOAD` 都会新增一个独立的下载任务，而不是覆盖之前的。

```ts
fromCallback<{ type: 'CANCEL' }, { url: string }>(...)
```
这是给 `fromCallback` 显式标注两个类型参数：第一个是"这个 Actor 能接收的事件类型"，第二个是"这个 Actor 需要的 input 类型"。第 25 课会专门讲这种"尖括号传类型"的泛型写法，这里先知道它的作用是让 `input`、`receive` 里的 `event` 都有正确的类型提示。

## 7 常见错误

**错误 1：`spawn` 出来的 Actor 忘记存进 Context，导致之后无法单独引用它（比如想取消某一个下载任务）。**

```ts
// ❌ spawn 出来的引用没有保存，创建完就"丢了"，没法再单独控制它
actions: ({ spawn }) => {
  spawn('downloadTaskLogic', { input: { url: 'file.zip' } }); // 引用没有被存起来
},
```
应该像第 5 节示例那样，把 `spawn(...)` 的返回值放进 `assign` 更新后的 Context 数组里。

**错误 2：把本该用 `invoke` 的"数量固定、生命周期跟状态绑定"的场景，错误地用 `spawn` 实现，导致清理逻辑要自己手写。**

```ts
// ❌ 只有一个固定的登录请求，数量和生命周期都很确定，用 spawn 反而增加了复杂度
actions: assign({
  loginRef: ({ spawn }) => spawn('loginWork'),
}),
```
这种"进入某状态就该有且只有一个、离开状态就该自动销毁"的场景，`invoke` 天然就能做到自动生命周期管理（第 14 课），用 `spawn` 反而需要自己在合适的时机手动停止它。

**错误 3：忘记提供"停止某个 spawn 出来的 Actor"的方式，导致创建了很多任务后无法单独取消。**

真实项目里，既然可以动态创建多个 Actor，通常也需要能单独停止某一个（比如根据 `url` 找到对应的引用，调用相应的停止方法或发送取消事件）。这部分属于进阶用法，本课只建立"动态创建、需要显式管理"这个核心概念，具体的停止 API 请查阅官方文档。

## 8 为什么官方这样设计

`spawn` 的存在，是为了覆盖 `invoke` 无法覆盖的场景：**数量和生命周期无法在配置阶段静态确定，必须由运行时的业务逻辑动态决定**。XState 选择把 `spawn` 设计成"只能在 `assign` 内部使用、并且推荐把引用存进 Context"，是因为这样能保持一致性——Context 依然是"这台机器当前所有数据"的唯一容器，即使这份数据是"指向其他 Actor 的引用"而不是普通的数字或字符串，也遵循同一套"通过 assign 更新、可以在快照里读取"的规则，不需要为 `spawn` 出来的东西发明一套平行的、独立于 Context 的管理机制。

## 9 最佳实践

1. 判断该用 `invoke` 还是 `spawn`：数量固定（通常是 1 个）、生命周期完全跟随某个状态 → `invoke`；数量运行时动态决定、需要在业务代码里单独引用和控制某一个 → `spawn`。
2. `spawn` 出来的引用几乎总是要存进 Context，否则创建完就无法再单独管理它。
3. 提前设计好"如何找到某个特定的 spawn 出来的 Actor"（比如给每个任务一个唯一 id，存成 `{ id, ref }` 的形式），而不是只存一个裸引用数组。
4. 明确设计"何时、如何停止"每一个动态创建的 Actor，不要只考虑创建、不考虑清理。
5. 不要滥用 `spawn`——如果所有场景其实数量都是固定的 1 个，`invoke` 的自动生命周期管理几乎总是更简单、更不容易出错的选择。

## 10 一句话总结

**`spawn` 用于运行时动态创建数量不固定的 Actor，通常需要把引用存进 Context 以便后续单独管理，跟 `invoke` 的静态绑定形成互补。**

## 11 设计思考（Design Thinking）

**需求**：下载器允许用户同时添加任意多个下载任务，每个任务独立运行、互不干扰，且能单独取消某一个。

**方案 A（推荐）**：每次"添加下载"都 `spawn` 一个新的独立 Actor，连同一个唯一标识存进 Context 里的数组，取消时根据标识找到对应引用单独处理。

```ts
context: { downloads: [] as { id: string; ref: ActorRefFrom<typeof downloadTaskLogic> }[] }
```

**方案 B（错误）**：试图用固定数量的几个 `invoke`（比如预先声明 `download1`、`download2`、`download3` 三个状态各自 `invoke` 一个任务）来应付"任意多个"的需求。

```ts
// ❌ 用静态、数量写死的 invoke 应付本质上数量不固定的需求
states: {
  slot1: { invoke: { src: 'downloadTaskLogic' } },
  slot2: { invoke: { src: 'downloadTaskLogic' } },
  slot3: { invoke: { src: 'downloadTaskLogic' } },
  // 用户点第 4 次"添加下载"时，没有 slot4 可用
}
```

方案 B 的问题很直观：`invoke` 的数量在写配置的时候就已经固定了，无法应对"用户可以添加任意多个下载任务"这种运行时才能确定数量的真实需求。方案 A 用 `spawn` 把"创建一个新的独立工作单元"变成一个可以在运行时被反复调用的动作，数量完全由业务逻辑（用户点了几次）决定，这正是 `spawn` 存在的核心价值——**为"数量不固定、需要动态管理"的场景提供 `invoke` 无法提供的灵活性。**

## 12 设计原则（Design Rules）

本课新增：

- **`invoke` 用于数量固定、生命周期跟随状态的场景；`spawn` 用于数量运行时动态决定、需要单独管理的场景，两者互补而非互相替代。**
- **`spawn` 出来的引用要存进 Context 并配合唯一标识，方便后续单独查找和控制。**
- **动态创建的 Actor 要显式设计清理/停止机制，不能只管创建不管销毁。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

第四部分到此结束。下一课进入第五部分：[17 生命周期——从 setup() 到 Snapshot](17-lifecycle.md)
