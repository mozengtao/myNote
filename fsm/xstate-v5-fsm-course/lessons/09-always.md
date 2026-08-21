# 09 always：什么时候自动跳转

> 上一课：[08 after](08-after.md) · 下一课：[10 Initial](10-initial.md)

## 1 为什么需要

有些转移既不依赖外部事件（`on`），也不依赖时间（`after`），而是纯粹依赖**当前数据的状态**：订单金额一旦超过某个阈值，应该立刻标记为"需要审批"，不需要等任何人点按钮。这种"数据一旦满足条件就该立刻反应"的场景，需要一种"不需要事件触发、只要条件成立就自动转移"的机制——这就是 `always`。

## 2 核心思想

**`always` 是"不需要任何事件，只要进入这个状态就立刻检查条件，条件满足就自动转移"的特殊转移。**

## 3 ASCII 图

```
+-----------+   always: amount > 1000   +-------------------+
| submitted | -------------------------> | needsApproval     |
+-----------+                            +-------------------+
      |
      | always 条件不满足时，原地留在 submitted，
      | 等待普通事件（比如 APPROVE）驱动
```

## 4 JS 思维

```js
function enterState(state, context) {
  let current = state;

  // 模拟 always：进入状态后立刻检查一次条件
  if (current === 'submitted' && context.amount > 1000) {
    current = 'needsApproval'; // 不需要任何外部事件，直接跳转
  }

  return current;
}

console.log(enterState('submitted', { amount: 1500 })); // 'needsApproval'
console.log(enterState('submitted', { amount: 500 }));  // 'submitted'
```

和 `after` 的区别很关键：`after` 是"等一段时间后"触发，`always` 是"立刻"检查，不等待任何东西——本质是一次性的条件分流，通常和 Guard（第 7 课）搭配使用。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const orderMachine = setup({
  types: {
    context: {} as { amount: number },
  },
  guards: {
    isLargeAmount: ({ context }) => context.amount > 1000,
  },
}).createMachine({
  id: 'order',
  initial: 'submitted',
  context: { amount: 0 },
  states: {
    submitted: {
      always: {
        target: 'needsApproval',
        guard: 'isLargeAmount',
      },
      on: {
        APPROVE: { target: 'approved' },
      },
    },
    needsApproval: {
      on: {
        APPROVE: { target: 'approved' },
      },
    },
    approved: {},
  },
});

const bigOrder = createActor(orderMachine, {
  input: undefined,
}).start();
// 注意：这里直接用初始 context 举例；真实项目里金额通常通过 input 或事件传入。
```

更贴近真实用法的写法（金额在创建时就确定）：

```ts
const machine2 = setup({
  types: { context: {} as { amount: number } },
  guards: { isLargeAmount: ({ context }) => context.amount > 1000 },
}).createMachine({
  id: 'order',
  initial: 'submitted',
  context: ({ input }: { input: { amount: number } }) => ({ amount: input.amount }),
  states: {
    submitted: {
      always: { target: 'needsApproval', guard: 'isLargeAmount' },
      on: { APPROVE: { target: 'approved' } },
    },
    needsApproval: { on: { APPROVE: { target: 'approved' } } },
    approved: {},
  },
});

const order = createActor(machine2, { input: { amount: 1500 } }).start();
console.log(order.getSnapshot().value); // 'needsApproval'，一进 submitted 就被自动带走了
```

## 6 TypeScript 语法拆解

```ts
always: {
  target: 'needsApproval',
  guard: 'isLargeAmount',
},
```
`always` 的结构和一条普通 Transition 几乎一样（`target` + `guard`），唯一的区别是它没有对应的事件名——它不是写在 `on` 里的，而是状态节点上独立的字段，意味着"每次进入这个状态（或者这个状态内部的 Context 发生变化后重新求值），立刻检查一次这个 guard"。

```ts
context: ({ input }: { input: { amount: number } }) => ({ amount: input.amount }),
```
`context` 字段除了可以直接写一个固定对象（前几课的写法），也可以写成一个函数，接收 `{ input }`（创建 actor 时传入的初始参数），返回真正的初始 Context。这里用到了第 26 课会讲的写法思路，先知道"context 可以是函数"即可，不用深究。

## 7 常见错误

**错误 1：在 `always` 的 guard 里写了会产生副作用或耗时的逻辑。**

```ts
// ❌ always 可能在一次求值周期里被反复检查，不能放重逻辑或副作用
guards: {
  badGuard: () => {
    fetch('/api/check'); // 绝对不要在这里发请求
    return true;
  },
},
```
`always` 的条件检查应该是极其轻量的同步判断，异步操作属于 Actor 的职责（第四部分）。

**错误 2：给 `always` 配置一个恒为 `true` 的 guard（或者不加 guard），导致状态机死循环。**

```ts
// ❌ 没有 guard，意味着"无条件立刻转移"，如果目标状态又无条件转回来，会死循环
stateA: { always: { target: 'stateB' } },
stateB: { always: { target: 'stateA' } }, // 无限来回跳转
```
`always` 必须最终能"稳定"在某个不再满足自动转移条件的状态，否则会陷入死循环，XState 在开发模式下通常会检测并报错提示。

**错误 3：把本该是"用户主动触发"的转移也写成了 `always`。**

```ts
// ❌ "审批通过"应该是外部事件（人工操作），不应该是自动条件判断
needsApproval: {
  always: { target: 'approved', guard: 'someAutoCondition' }, // 审批不该自动发生
},
```
如果转移在语义上是"外部世界主动做了什么"（人点了按钮、收到了审批结果），应该用 `on` 建模成 Event；只有"纯粹取决于当前数据、不需要外部输入"的判断，才适合 `always`。

## 8 为什么官方这样设计

`always` 存在的意义是把"进入状态后立刻做一次条件判断并可能分流"这件常见的需求标准化，而不是让开发者用"进入状态后手动调用一个函数、函数里再手动 `send` 一个事件"这种绕远路的方式实现。它让"这个状态其实是一个临时的、会自动被条件带走的中转点"这件事，直接体现在配置里——你一眼就能看出 `submitted` 不是一个稳定终点，而是会根据 `amount` 自动分流的判断节点。这与 Guard（第 7 课）是同一哲学的延伸：**决策逻辑应该在配置里可见，而不是藏在某个被调用的函数深处。**

## 9 最佳实践

1. `always` 的 guard 必须是纯函数、极轻量，不做异步操作、不产生副作用。
2. 确保 `always` 链条最终会"稳定"下来，不会因为互相满足对方的条件而死循环。
3. 只有"完全取决于当前数据、不需要任何外部输入"的转移才用 `always`；需要用户操作或外部系统响应的转移用 `on`。
4. 把用 `always` 的状态当作"临时中转站"来设计和命名（比如 `submitted` 明确暗示"提交后马上会被分流"），阅读代码的人能更快理解这个状态不会久留。
5. 优先考虑：能不能在进入这个状态之前就用 Guard 判断好，直接走不同的 Transition 到不同目标？如果可以，往往比用 `always` 多绕一层更直接、更清晰——`always` 更适合"目标状态不方便在源头判断"或"条件依赖状态内部才产生的 Context 变化"的场景。

## 10 一句话总结

**`always` 是不需要事件、进入状态后立刻检查条件的自动转移，适合"纯数据驱动"的分流，不适合替代真正的外部事件。**

## 11 设计思考（Design Thinking）

**需求**：订单提交后，如果金额超过 1000，需要自动进入"待审批"，否则等待人工点击"通过"。

**方案 A（推荐）**：`submitted` 状态用 `always` + Guard 自动分流大额订单，小额订单留在原地等待普通事件。

```ts
submitted: {
  always: { target: 'needsApproval', guard: 'isLargeAmount' },
  on: { APPROVE: { target: 'approved' } },
}
```

**方案 B（错误）**：在业务代码里，订单一创建就立刻检查金额，直接决定把 Machine 的初始状态设成 `needsApproval` 或 `submitted` 中的一个，状态机本身不参与这个判断。

```ts
// ❌ 判断逻辑在状态机外部，状态机图上完全看不出"大额订单会被特殊处理"这件事
const initialState = amount > 1000 ? 'needsApproval' : 'submitted';
const order = createActor(orderMachine.provide({ /* 硬改 initial，实际上很别扭 */ }));
```

方案 B 把本该属于状态机的决策逻辑（"金额超标就要审批"）挪到了外部业务代码里，而且 XState 的 `initial` 本来就不是设计给"外部动态决定"用的，这种写法既别扭又让状态图失去了"自解释"的价值——任何人光看 `orderMachine` 的配置，都无法得知这条业务规则的存在，必须去翻业务代码才知道。方案 A 把这条规则直接写在图里，图和规则永远同步。

## 12 设计原则（Design Rules）

本课新增：

- **`always` 用于"纯数据驱动、不需要外部输入"的自动分流，条件判断必须轻量、无副作用。**
- **需要外部输入（用户操作、外部系统响应）的转移用 `on`，不要用 `always` 冒充。**
- **`always` 链条必须最终收敛，避免因为互相满足条件导致死循环。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

第二部分到此结束。下一课进入第三部分：[10 Initial——为什么必须有起点](10-initial.md)
