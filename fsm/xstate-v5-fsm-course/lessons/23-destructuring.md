# 23 Destructuring：`{ context, event }` 这种写法是什么

> 上一课：[22 Arrow Function](22-arrow-function.md) · 下一课：[24 type / interface](24-type-interface.md)

## 1 为什么需要

上一课的箭头函数参数写成 `({ context, event }) => ...`，而不是 `(params) => params.context...`。为什么 XState 几乎所有回调函数都用花括号包裹参数名？因为 XState 传给这些回调的，永远是**一个包含好几个字段的对象**（`{ context, event, self, spawn, ... }`），如果每次都要先接一个变量名再逐个 `.` 出来用，代码会很啰嗦。JavaScript/TypeScript 提供了一种"直接从对象里按字段名取值"的语法，这就是解构（Destructuring）。

## 2 核心思想

**解构是"从一个对象（或数组）里，按字段名直接提取出你需要的部分，作为独立变量使用"的语法糖，能省掉"先存一个中间变量再逐个访问属性"的步骤。**

## 3 ASCII 图

```
传入的对象:  { context: {...}, event: {...}, self: ..., spawn: ... }
                     |                |
                     | 解构提取        | 解构提取
                     v                v
              变量 context       变量 event
              （其他字段如果不需要，直接忽略，不用管）
```

## 4 JS 写法

不用解构的写法：

```js
function handle(params) {
  const context = params.context; // 手动取出 context
  const event = params.event;     // 手动取出 event
  console.log(context.count, event.type);
}
```

用解构的写法：

```js
function handle({ context, event }) {
  // 直接在参数位置"解开"传入的对象，context 和 event 变成了两个独立变量
  console.log(context.count, event.type);
}

handle({ context: { count: 1 }, event: { type: 'PLAY' } });
```

两段代码效果完全一样，解构版本省掉了两行"手动取值"的代码。解构也可以用在普通变量赋值上，不只是函数参数：

```js
const state = { value: 'idle', context: { count: 0 } };
const { value, context } = state; // 一次性从 state 里取出两个字段
console.log(value, context); // 'idle' { count: 0 }
```

## 5 TS 写法

```ts
// 解构 + 类型标注：类型写在解构花括号后面
function handle({ context, event }: { context: { count: number }; event: { type: string } }) {
  console.log(context.count, event.type);
}
```

注意类型标注的位置：`{ context, event }` 是解构（取哪些字段），冒号后面的 `{ context: {...}; event: {...} }` 才是这个参数**整体**的类型（哪些字段，各自什么类型）——这两处花括号长得像，含义完全不同：前一个是"解构模式"，后一个是"类型声明"。

## 6 XState 中如何使用

```ts
import { setup, assign, createActor } from 'xstate';

const machine = setup({
  types: {
    context: {} as { count: number },
    events: {} as { type: 'INCREMENT'; amount: number },
  },
  actions: {
    increment: assign({
      // { context, event } 解构出这次调用需要的两个字段，忽略其他不需要的（比如 self）
      count: ({ context, event }) => context.count + event.amount,
    }),
    logOnly: (
      // 只需要 event，就只解构 event，不必把 context 也列出来
      { event },
    ) => console.log('收到事件:', event.type),
  },
}).createMachine({
  id: 'counter',
  initial: 'active',
  context: { count: 0 },
  states: {
    active: {
      on: {
        INCREMENT: {
          actions: ['increment', 'logOnly'],
        },
      },
    },
  },
});

const counter = createActor(machine).start();
counter.send({ type: 'INCREMENT', amount: 3 });
```

XState 传给 Action/Guard/assign 回调的对象，实际上包含比 `context`、`event` 更多的字段（比如 `self`、第 16 课用到的 `spawn`），解构的好处正体现在这里：**你只需要在花括号里写出你真正用得到的字段名，其余的字段完全不用理会，代码不会因为对象上"还有别的字段"而变得啰嗦。**

## 7 TypeScript 语法拆解

```ts
({ context, event }) => context.count + event.amount
```
函数参数位置的 `{ context, event }` 是对象解构模式：告诉 JS/TS"这个参数是一个对象，我要把它的 `context` 字段和 `event` 字段分别取出来，作为同名的局部变量使用"。

```ts
({ event }) => console.log('收到事件:', event.type)
```
只解构需要的字段——即使传进来的对象上还有 `context`、`self` 等其他字段，只写 `{ event }` 就只提取 `event` 这一个变量，其他字段不会出现在函数体的作用域里（想用也用不到，除非你把它也加进解构花括号）。

```ts
const { value, context } = state;
```
这是"变量赋值"场景下的解构（跟函数参数解构语法一样，只是不在参数位置），第 5 课、第 17 课里读取快照时经常写的 `snapshot.value`、`snapshot.context`，本质上也可以用这种解构写法一次性取出多个字段，只是本课程之前为了更直观地展示"从哪个对象上取值"，多用了 `.` 访问的写法。

## 8 常见错误

**错误 1：把解构和"重命名"搞混，误以为解构出来的变量名可以随便叫。**

```ts
// 解构时可以重命名，但要用冒号显式指定，不能直接改名字
const { context: ctx } = params; // ✅ 合法：把 context 字段取出来，命名为 ctx
console.log(ctx.count);

// ❌ 如果没写冒号重命名，变量名必须和字段名完全一致
const { renamedContext } = params; // 这其实是在找一个叫 renamedContext 的字段，不是重命名 context
```

**错误 2：解构一个可能不存在的字段，却没有提供默认值，导致拿到 `undefined` 却当作正常值使用。**

```ts
// 如果 event 上不一定有 amount 字段（比如 Union 里有的分支没有），
// 直接解构后使用容易在某些事件类型下访问到 undefined
const { amount } = event; // amount 可能是 undefined，取决于具体是哪种事件
```
这类问题本质上是第 19 课"先判断 event.type 收窄类型"没有做到位，解构本身只是"取值语法"，不会替你处理"这个字段到底存不存在"的逻辑判断。

**错误 3：过度解构，导致函数签名里参数列表过长，可读性反而下降。**

```ts
// 如果一个函数需要用到对象上七八个字段，全部摊开解构可能让参数列表过长
({ context, event, self, system, spawn, emit, meta }) => { ... }
```
只解构真正用得到的字段，不需要为了"展示自己知道这些字段存在"而全部列出来。

## 9 为什么官方这样设计

XState 的所有回调（Action、Guard、`assign` 字段、`invoke.input` 等）统一设计成"接收一个对象参数"（而不是多个独立参数，比如 `(context, event) => ...`），是为了**未来可以给这个对象添加新字段，而不破坏已有的调用方代码**——如果签名是 `(context, event)` 这种位置参数，以后想加第三个信息（比如 `self`）就必须要求所有调用方在第二个参数后面再加一个，容易搞乱参数顺序；而"接收一个对象，你需要什么就解构什么"的设计，加新字段完全不影响任何已有代码，因为没有解构那个新字段的老代码根本不会注意到它的存在。解构语法则是配合这种"对象参数"设计的最佳搭档：既保留了"对象可以随时扩展新字段"的灵活性，又不需要每次都写"先存中间变量再取属性"的样板代码。

## 10 一句话总结

**解构是从对象里按字段名直接提取值的语法糖，XState 用"对象参数 + 解构"的组合，既方便未来扩展字段，又能让回调函数只关心自己真正需要的部分。**

## 11 设计思考（Design Thinking）

**需求**：一个 Guard 只需要用到 `context`，完全不需要 `event`。

**方案 A（推荐）**：只解构需要的字段。

```ts
guards: {
  isUnderLimit: ({ context }) => context.count < 100,
},
```

**方案 B（不推荐）**：接收整个参数对象，再手动访问需要的属性。

```ts
guards: {
  isUnderLimit: (params) => params.context.count < 100,
},
```

两种写法效果完全一样，方案 B 并没有"错"，但每次访问都要多写一次 `params.`，如果函数体里要多次用到 `context`，这种冗余会被放大好几倍。方案 A 用解构把 `context` 直接变成一个局部变量，函数体内直接使用，可读性更好——而且这也是整个 XState 社区约定俗成的风格，保持一致有助于你更快读懂别人写的代码，也更容易被别人读懂。

## 12 设计原则（Design Rules）

本课新增：

- **解构只是"取值"的语法糖，不会自动处理"字段可能不存在"的情况——那依然需要第 19 课学的类型收窄来保证安全。**
- **只解构函数体真正用得到的字段，不需要的字段不必列出，保持参数列表简洁。**
- **XState 用"对象参数 + 解构"而非位置参数，是为了未来能安全地扩展新字段而不破坏已有代码。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[24 type / interface——给类型起名字的两种方式](24-type-interface.md)
