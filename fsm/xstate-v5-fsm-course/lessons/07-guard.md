# 07 Guard：为什么不能用 if

> 上一课：[06 Action](06-action.md) · 下一课：[08 after](08-after.md)

## 1 为什么需要

现实里很多转移是"有条件的"：余额不够不能提现，权限不够不能进入管理页，未成年不能购买某些商品。你当然可以在业务代码里写一堆 `if` 判断该不该 `send` 事件，但这样一来，"能不能转移"这条规则又从状态机的配置里逃逸到了业务代码里——回到了第 4 课强调过的问题：判断该不该转移，应该是状态机自己的职责，不该由调用方代劳。

## 2 核心思想

**Guard 是"这条转移能不能发生"的条件判断，写在状态机配置里，而不是写在发送事件之前的业务代码里。**

## 3 ASCII 图

```
                 guard: hasEnoughBalance
                          |
                          v
+---------+   WITHDRAW   +-----------+
| account | -----------> | withdrawn |
+---------+              +-----------+
     |
     | WITHDRAW（余额不足，guard 为 false）
     v
   （原地不动，转移不发生）
```

## 4 JS 思维

```js
let balance = 50;
let currentState = 'account';

function send(event) {
  if (currentState === 'account' && event.type === 'WITHDRAW') {
    const hasEnoughBalance = balance >= event.amount; // 这就是 guard
    if (hasEnoughBalance) {
      balance -= event.amount;
      currentState = 'withdrawn';
    }
    // guard 为 false：什么都不做，状态原地不动
  }
}
```

关键点：`hasEnoughBalance` 是一个**返回布尔值的纯判断**，它只回答"能不能"，不执行任何副作用（不改数据、不打日志）。这跟上一课的 Action 有本质区别——Action 是"做事"，Guard 是"判断能不能做"。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const accountMachine = setup({
  types: {
    context: {} as { balance: number },
    events: {} as { type: 'WITHDRAW'; amount: number },
  },
  guards: {
    hasEnoughBalance: ({ context, event }) => context.balance >= event.amount,
  },
}).createMachine({
  id: 'account',
  initial: 'active',
  context: { balance: 50 },
  states: {
    active: {
      on: {
        WITHDRAW: {
          target: 'withdrawn',
          guard: 'hasEnoughBalance',
        },
      },
    },
    withdrawn: {},
  },
});

const account = createActor(accountMachine).start();

account.send({ type: 'WITHDRAW', amount: 1000 }); // guard 为 false，转移不发生
console.log(account.getSnapshot().value); // 仍是 'active'

account.send({ type: 'WITHDRAW', amount: 30 }); // guard 为 true，转移发生
console.log(account.getSnapshot().value); // 'withdrawn'
```

## 6 TypeScript 语法拆解

```ts
guards: {
  hasEnoughBalance: ({ context, event }) => context.balance >= event.amount,
},
```
在 `setup({ guards: {...} })` 里注册所有会用到的 Guard 函数，跟 Action 的注册方式一样。Guard 函数接收 `{ context, event }`，**必须返回一个布尔值**，这是它和 Action 最本质的区别：Action 的返回值没有意义（做完就完了），Guard 的返回值直接决定转移发不发生。

```ts
WITHDRAW: {
  target: 'withdrawn',
  guard: 'hasEnoughBalance',
},
```
Transition 上加一个 `guard` 字段，值是 Guard 的名字。**只有 `guard` 返回 `true`，`target` 指定的跳转才会真正发生**；返回 `false` 时，这条 Transition 就像不存在一样，事件被忽略（和第 3 课"未监听的事件被忽略"效果一致，只是这里是"监听了但条件不满足"）。

```ts
events: {} as { type: 'WITHDRAW'; amount: number },
```
这里声明事件类型自带 `amount: number` 字段，这样 Guard 函数里的 `event.amount` 才有类型提示，不会是 `unknown`。

## 7 常见错误

**错误 1：在业务代码里先判断一遍，再决定要不要 `send`，state 机内部又重复写一遍 guard。**

```ts
// ❌ 双重判断，逻辑分散在两个地方，容易不一致
if (balance >= amount) {
  account.send({ type: 'WITHDRAW', amount });
}
```
既然状态机已经有 guard 了，业务代码应该**无条件发送事件**，让状态机自己决定要不要响应。双重判断的风险是：两处判断逻辑以后可能会不同步（比如只改了 guard，忘了改业务代码里的判断，或者反过来）。

**错误 2：把有副作用的代码写进 Guard 函数。**

```ts
// ❌ guard 里不应该有副作用，也不应该修改任何东西
guards: {
  badGuard: ({ context }) => {
    console.log('checking...'); // 副作用
    context.checkedAt = Date.now(); // 更是错上加错，直接改了 context
    return context.balance > 0;
  },
},
```
Guard 必须是**纯函数**：给定同样的 `context`/`event`，永远返回同样的结果，不产生任何副作用。这样才能保证它可以被安全地反复调用（比如多个候选转移各自有 guard，XState 需要按顺序试探）。

**错误 3：该拆 Guard 的场景，用一个巨大的布尔表达式糊在一起，不给它起名字。**

```ts
// ❌ 内联表达式，看不出这个判断的业务含义
WITHDRAW: {
  target: 'withdrawn',
  guard: ({ context, event }) => context.balance >= event.amount && context.isVerified && !context.isFrozen,
},
```
虽然内联写法在语法上是允许的，但当条件涉及多个业务概念时，拆成命名清晰的具名 Guard（甚至拆成多个 Guard 组合）可读性会好得多，见第 9 节最佳实践。

## 8 为什么官方这样设计

Guard 被设计成"转移配置的一部分"而不是"业务代码里的 if"，核心原因和第 4 课的 Transition 一致：**要让状态图成为一份完整、可信的地图**。如果"能不能转移"这件事一部分写在图里、一部分写在业务代码的 if 里，那么光看状态机配置，你永远无法确定这条转移真正的触发条件是什么，图就不再是"唯一可信来源"了。把 Guard 收进配置，意味着你可以把整张图导出、可视化、甚至自动生成测试用例覆盖所有"guard 为真/为假"的分支——这些能力都建立在"所有决策逻辑都在配置里"这个前提上。

## 9 最佳实践

1. Guard 必须是纯函数：不改 Context、不打日志、不做异步操作，只返回布尔值。
2. 给每个业务含义独立的判断条件起一个具名 Guard（`hasEnoughBalance`、`hasPermission`、`isAdult`），不要把好几个业务概念揉进一个内联表达式。
3. 判断"是否要拆成 Guard"的标准：如果这个条件涉及业务规则（余额、权限、年龄），几乎肯定要拆；如果只是简单的技术性判断且只用一次，内联写法也可以接受。
4. 不要在业务代码里对同一个条件做二次判断，无条件 `send`，把决策权彻底交给状态机。
5. 同一条转移可以有多个候选（按顺序尝试，命中第一个 guard 为真的），复杂的多分支决策优先考虑用多条"同事件不同 target+guard"的转移表达，而不是在一个 Guard 里塞 if-else。

## 10 一句话总结

**Guard 是写在配置里的"能不能转移"的纯判断，业务代码永远无条件发送事件，把决策权交给状态机。**

## 11 设计思考（Design Thinking）

**需求**：用户提现时，要求余额充足、且账户已实名认证。

**方案 A（推荐）**：拆成两个独立、具名的 Guard，转移里同时要求两者都满足（用组合 guard 或者数组形式，具体语法请查阅官方文档的 `and`/`or` 组合器；这里先示意思路）。

```ts
guards: {
  hasEnoughBalance: ({ context, event }) => context.balance >= event.amount,
  isVerified: ({ context }) => context.isVerified,
},
// 转移里组合使用（概念示意）
WITHDRAW: {
  target: 'withdrawn',
  guard: ({ context, event }, params) =>
    context.balance >= event.amount && context.isVerified,
}
```

**方案 B（错误）**：业务代码里先判断，通过了才 `send`；状态机本身完全不设防。

```ts
// ❌ 状态机对"提现"这件事没有任何自我保护能力
if (balance >= amount && isVerified) {
  account.send({ type: 'WITHDRAW', amount });
} else {
  alert('不满足提现条件');
}
```

方案 B 的根本问题：状态机本身"来者不拒"，任何人只要拿到这个 actor 的引用，绕过这段业务代码直接 `send({ type: 'WITHDRAW', amount: 99999 })`，账户余额校验就完全失效了——业务规则的强制力依赖调用方"自觉遵守"，这在稍微复杂一点的系统里是不可靠的。方案 A 把规则刻进状态机本身，不管谁在什么时候发送这个事件，状态机自己会拒绝不满足条件的转移，规则只有一处、且无法被绕过。

## 12 设计原则（Design Rules）

本课新增：

- **Guard 决定能否变化（Decision）：是转移配置的一部分，不是业务代码里的 if。**
- **Guard 必须是纯函数：不改数据、无副作用、只返回布尔值。**
- **业务代码永远无条件发送事件，判断权完全交给状态机自己的 Guard，避免规则散落两处导致不一致或被绕过。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[08 after——定时器也是 Event](08-after.md)
