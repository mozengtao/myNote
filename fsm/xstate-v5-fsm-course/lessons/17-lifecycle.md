# 17 生命周期：从 setup() 到 Snapshot

> 上一课：[16 spawn](16-spawn.md) · 下一课：[18 Object（TypeScript 部分开始）](18-object.md)

前 16 课分别学了各个零件，这一课把它们串起来，看清楚"一台 XState v5 机器从代码到运行"完整经历了哪些阶段。

## 1 为什么需要

到目前为止，每一课都在局部使用 `setup`、`createMachine`、`createActor`、`start`、`send`，但没有一课完整解释过它们之间的先后顺序和各自的职责边界。如果心里没有一张"全局流程图"，遇到"为什么这个类型推导不出来""为什么状态没有更新"这类问题时，很难判断到底是哪个环节出了问题。

## 2 核心思想

**XState v5 的完整生命周期是一条单向流水线：先用 `setup()` 声明类型和可复用逻辑，再用 `createMachine()` 描述图，然后用 `createActor()` 造出运行时实例，`start()` 激活它，之后靠 `send()` 不断喂入事件触发 `Transition`，每次转移都会产生一份新的 `State Snapshot`。**

## 3 ASCII 图

```
setup()
   |
   v
createMachine()      <- 到这里为止，都只是"描述"，还没有任何运行时状态
   |
   v
   Machine (静态图)
   |
   v
createActor(machine)  <- 造一个"可运行的实例"，但还没激活
   |
   v
   Actor (未启动)
   |
   v
start()               <- 激活，进入 initial 状态，产生第一份 Snapshot
   |
   v
   Actor (运行中) <----------------------+
   |                                      |
   v                                      |
send(event) ---> Transition ---> 新的 State Snapshot
                                          |
                                          +---(继续 send，循环往复)
```

## 4 JS 思维

用普通函数模拟这条流水线的各个阶段：

```js
// 1. "setup" 阶段：准备好可复用的逻辑（这里简化成一个配置对象）
const config = {
  actions: { log: () => console.log('执行了 log') },
};

// 2. "createMachine" 阶段：只是把图的结构描述出来，一份纯数据
const machineDescription = {
  initial: 'idle',
  states: {
    idle: { on: { GO: { target: 'active', actions: 'log' } } },
    active: {},
  },
};

// 3. "createActor" 阶段：围绕这份描述，造一个"运行时容器"，此时还没有当前状态
function createActor(description) {
  return { description, current: undefined, started: false };
}
const actor = createActor(machineDescription);

// 4. "start" 阶段：激活，产生第一份"快照"
function start(actor) {
  actor.current = actor.description.initial; // 'idle'
  actor.started = true;
}
start(actor);

// 5. "send" 阶段：驱动一次 Transition，产生新的快照
function send(actor, event) {
  const transition = actor.description.states[actor.current]?.on?.[event.type];
  if (transition) {
    if (transition.actions === 'log') config.actions.log();
    actor.current = transition.target; // 新的快照
  }
}
send(actor, { type: 'GO' });
console.log(actor.current); // 'active'
```

每一步在真实 XState 里都对应一个具体的函数调用，只是内部实现复杂得多（要处理嵌套状态、并行状态、Guard、invoke 的生命周期等等），但**整体流程的骨架和这段简化代码完全一致**。

## 5 XState v5 写法

把完整的生命周期在一段代码里走一遍：

```ts
import { setup, createActor, assign } from 'xstate';

// ① setup：声明类型 + 注册可复用的 actions/guards/actors
const doorMachine = setup({
  types: {
    context: {} as { openCount: number },
    events: {} as { type: 'OPEN' } | { type: 'CLOSE' },
  },
  actions: {
    logOpen: assign({
      openCount: ({ context }) => context.openCount + 1,
    }),
  },
}).createMachine({
  // ② createMachine：描述图本身（纯数据，此时还没有任何运行时状态）
  id: 'door',
  initial: 'closed',
  context: { openCount: 0 },
  states: {
    closed: {
      on: { OPEN: { target: 'opened', actions: 'logOpen' } },
    },
    opened: {
      on: { CLOSE: { target: 'closed' } },
    },
  },
});

// ③ createActor：造一个运行时实例（此时还未启动，没有当前状态）
const doorActor = createActor(doorMachine);

// 提前订阅，才能收到 start() 之后的第一份快照
doorActor.subscribe((snapshot) => {
  console.log('快照更新:', snapshot.value, snapshot.context);
});

// ④ start：激活，进入 initial 状态，产生第一份 Snapshot
doorActor.start();
// 打印: 快照更新: closed { openCount: 0 }

// ⑤ send -> Transition -> 新的 Snapshot，循环往复
doorActor.send({ type: 'OPEN' });
// 打印: 快照更新: opened { openCount: 1 }

doorActor.send({ type: 'CLOSE' });
// 打印: 快照更新: closed { openCount: 1 }
```

## 6 TypeScript 语法拆解

```ts
setup({ types: {...}, actions: {...} }).createMachine({...})
```
`setup` 返回一个"已经知道类型和可用逻辑名字"的构造器，`.createMachine` 在它基础上接收具体的图结构。TypeScript 能在你写 `actions: 'logOpen'` 这样的字符串时，检查这个名字是否真的在 `setup` 里注册过——这种"跨两次函数调用的类型联动"，是 `setup()` 存在的核心价值之一（第 8 部分会在具体 TS 语法里再细讲）。

```ts
const doorActor = createActor(doorMachine);
```
这一步的返回类型是"一个尚未启动、但已经知道具体 Context/Event 类型的 Actor 实例"。注意此时还不能安全地读取"当前状态"这个概念——严格来说，`start()` 之前调用 `getSnapshot()` 依然能拿到基于 `initial` 计算出的快照，但订阅者只有在 `start()` 之后才会收到通知（本例特意把 `subscribe` 放在 `start` 之前，正是为了不错过第一份快照）。

```ts
doorActor.subscribe((snapshot) => { ... snapshot.value ... snapshot.context ... });
```
`snapshot` 是一个包含 `value`（当前 State）、`context`（当前 Context）等信息的对象，每次 Transition 之后都会产生一份新的快照。你可以把整个状态机的运行过程，理解成"不断产生新快照的过程"，`send` 是触发下一次快照产生的唯一方式。

## 7 常见错误

**错误 1：在 `start()` 之前调用 `send()`。**

```ts
// ❌ 还没启动就发送事件，行为未定义/不会按预期工作
const actor = createActor(machine);
actor.send({ type: 'OPEN' }); // 应该先 start()
actor.start();
```
必须先 `start()`，激活之后才能开始 `send`。

**错误 2：在 `start()` 之后才订阅，错过了第一份（`initial`）快照。**

```ts
const actor = createActor(machine);
actor.start();
actor.subscribe((snapshot) => { ... }); // 错过了 start() 那一刻产生的第一份快照
```
如果需要"从一开始"就能看到所有快照变化，应该在 `start()` **之前**完成订阅（如本课第 5 节示例）。

**错误 3：把 `createMachine` 返回的东西当成"正在运行的实例"直接使用。**

```ts
// ❌ machine 只是静态描述，没有 send/subscribe 这些运行时方法
doorMachine.send({ type: 'OPEN' });
```
这其实是第 1 课就强调过的"Machine 是图纸，Actor 才是实例"，放在这里再次确认：整条生命周期流水线里，只有 `createActor` 之后的产物才具备"运行时行为"。

## 8 为什么官方这样设计

把生命周期设计成"先描述（setup + createMachine），再实例化（createActor），再激活（start），再驱动（send）"这样清晰分离的阶段，本质上是"数据与运行时分离"哲学（第 1 课就提到过）在整个架构层面的体现：**图的描述本身是不可变的、可以被多次复用的静态数据；每一次 `createActor` 都是基于这份不变的描述，独立地"生一个新的运行时实例"；`start`/`send` 则是这个实例自己的生命周期动作，不会影响到 Machine 描述本身，也不会影响其他从同一个 Machine 创建出来的 Actor。** 这种分离让"测试一个 Machine 的行为"变得简单——每个测试用例都可以 `createActor(machine)` 造一个全新、干净的实例，互不污染。

## 9 最佳实践

1. 记住这条单向流水线的顺序：`setup → createMachine → createActor → start → send → Transition → Snapshot`，遇到问题时先定位自己在哪个阶段。
2. 需要"从一开始就能看到所有快照"时，`subscribe` 一定要写在 `start()` 之前。
3. 永远不要对 `createMachine` 的返回值直接调用 `send`/`start`/`subscribe`——这些是 Actor（`createActor` 的产物）的方法。
4. 同一个 Machine 描述可以安全地被多次 `createActor`，产生互不干扰的独立实例，这是编写单元测试时的常用手法。
5. 把"快照"当作只读的、每次 Transition 后全新产生的数据来对待，不要试图手动修改某次拿到的 `snapshot` 对象。

## 10 一句话总结

**XState v5 的生命周期是一条单向流水线：先描述图（setup + createMachine），再造实例（createActor），再激活（start），然后靠 send 驱动 Transition，不断产生新的 Snapshot。**

## 11 设计思考（Design Thinking）

**需求**：在自动化测试中，要验证"门被打开两次后，`openCount` 应该是 2"，且每个测试用例之间不能互相影响。

**方案 A（推荐）**：每个测试用例都独立调用 `createActor(doorMachine)` 创建一个全新实例，测试之间天然隔离，因为 Machine 描述本身不可变、不携带任何运行时状态。

```ts
test('opens twice increments count', () => {
  const actor = createActor(doorMachine).start();
  actor.send({ type: 'OPEN' });
  actor.send({ type: 'CLOSE' });
  actor.send({ type: 'OPEN' });
  expect(actor.getSnapshot().context.openCount).toBe(2);
});
```

**方案 B（错误）**：为了"节省资源"，在所有测试用例之间共享同一个 Actor 实例。

```ts
// ❌ 所有测试共用一个 actor，前一个测试的状态会污染后一个测试
const sharedActor = createActor(doorMachine).start();

test('test A', () => { sharedActor.send({ type: 'OPEN' }); /* ... */ });
test('test B', () => { /* 这里 sharedActor 已经不是 initial 状态了 */ });
```

方案 B 违背了"生命周期分离"带来的核心好处——`createMachine` 产生的描述本该是可以安全复用、多次实例化的，如果图省事共享同一个 Actor 实例，测试之间就会通过这个共享的运行时状态互相污染，导致测试顺序影响测试结果（这是测试代码里最难排查的问题之一）。方案 A 每次都从"纯粹的静态描述"重新造一个全新实例，充分利用了 Machine 和 Actor 分离这个设计带来的隔离性。

## 12 设计原则（Design Rules）

本课新增：

- **Machine 描述（setup + createMachine 的产物）是不可变的静态数据，可以安全地被多次 `createActor` 复用，互不干扰。**
- **生命周期是单向流水线：先描述、再实例化、再激活、再驱动，顺序不能颠倒（比如不能在 start 前 send）。**
- **需要完整观察一个 Actor 生命周期的所有快照时，`subscribe` 必须写在 `start()` 之前。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

第五部分到此结束。第六部分开始讲阅读 XState 所需的 TypeScript 语法：[18 Object](18-object.md)
