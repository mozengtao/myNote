# 22 Arrow Function：assign 和 guard 里反复出现的箭头函数

> 上一课：[21 as](21-as.md) · 下一课：[23 Destructuring](23-destructuring.md)

## 1 为什么需要

回顾第 5 课的 `assign`：

```ts
assign({
  volume: ({ event }) => event.level,
})
```

`({ event }) => event.level` 这种写法在 XState 代码里几乎无处不在——Action、Guard、`assign`、`invoke.input` 全都用它。如果你对箭头函数不熟悉，读 XState 代码会有一种"到处都是奇怪符号"的感觉。这一课把这个语法点单独拎出来讲清楚。

## 2 核心思想

**箭头函数是"定义一个函数"的简洁写法，在 XState 里几乎所有需要"给一个动作/条件/取值方式"的地方，都用箭头函数来表达"这里的逻辑该怎么算"。**

## 3 ASCII 图

```
输入                     箭头函数                    输出
{ context, event }  --->  ({ context, event }) => context.volume + 1  ---> 一个新的 volume 值
```

## 4 JS 写法

先看 JS 里"定义函数"的几种等价写法：

```js
// 传统函数声明
function add(a, b) {
  return a + b;
}

// 函数表达式
const add2 = function (a, b) {
  return a + b;
};

// 箭头函数（推荐、也是 XState 里几乎唯一使用的形式）
const add3 = (a, b) => {
  return a + b;
};

// 箭头函数：函数体只有一行 return 时，可以省略花括号和 return 关键字
const add4 = (a, b) => a + b;
```

三种写法本质上都是"定义一个接收 a、b，返回 a+b 的函数"，箭头函数只是语法更紧凑，尤其适合"作为参数传给另一个函数"的场景（这正是 `assign({ volume: (箭头函数) })` 里发生的事——箭头函数被当作参数，传给了 `assign`）。

## 5 TS 写法

```ts
// 箭头函数的参数和返回值都可以加类型标注
const add: (a: number, b: number) => number = (a, b) => a + b;

// 更常见的写法：直接在参数括号里标注类型，让 TS 自动推断返回值类型
const add2 = (a: number, b: number) => a + b; // 返回值类型自动推断为 number
```

## 6 XState 中如何使用

```ts
import { setup, assign, createActor } from 'xstate';

const counterMachine = setup({
  types: {
    context: {} as { count: number },
    events: {} as { type: 'INCREMENT'; amount: number },
  },
  actions: {
    // 箭头函数作为 assign 的字段值：接收 { context, event }，返回新的 count
    increment: assign({
      count: ({ context, event }) => context.count + event.amount,
    }),
  },
  guards: {
    // 箭头函数作为 guard：接收 { context }，返回布尔值
    isUnderLimit: ({ context }) => context.count < 100,
  },
}).createMachine({
  id: 'counter',
  initial: 'active',
  context: { count: 0 },
  states: {
    active: {
      on: {
        INCREMENT: {
          actions: 'increment',
          guard: 'isUnderLimit',
        },
      },
    },
  },
});

const counter = createActor(counterMachine).start();
counter.send({ type: 'INCREMENT', amount: 5 });
console.log(counter.getSnapshot().context.count); // 5
```

`({ context, event }) => context.count + event.amount` 这个箭头函数，本质上跟第 4 节的 `(a, b) => a + b` 是同一种语法，只是参数从 `a, b` 换成了一个被解构的对象 `{ context, event }`（解构本身留到下一课细讲），逻辑更复杂一点（访问对象字段而不是直接相加），但函数定义的语法骨架完全一样。

## 7 TypeScript 语法拆解

```ts
count: ({ context, event }) => context.count + event.amount,
```
`({ context, event })` 是这个箭头函数的**参数**（一个对象，字段名是 `context` 和 `event`——这是解构写法，下一课细讲）。`=>` 后面 `context.count + event.amount` 是函数体（只有一行表达式，自动作为返回值，不需要写 `return`）。因为整个 `assign` 是在 `setup()` 内部定义的，且前面已经通过 `types` 声明了 `context`/`events` 的类型，所以这里的 `context`、`event` 会被 TypeScript 自动推断出正确的类型，不需要你手写类型标注——这是 `setup()` 提供"类型推导链路"的具体体现。

```ts
isUnderLimit: ({ context }) => context.count < 100,
```
Guard 的箭头函数同样是"接收一个对象参数，返回一个值"，只是这里返回的是布尔值（第 7 课讲过 Guard 必须返回布尔值）。

## 8 常见错误

**错误 1：忘记箭头函数单行表达式会自动 return，多此一举地又加了花括号却忘了写 return。**

```ts
// ❌ 加了花括号（意味着这是"函数体"而不是"单行表达式"），但忘记写 return
count: ({ context, event }) => {
  context.count + event.amount; // 这行代码的结果被计算了，但没有 return，函数返回 undefined
},
```
一旦用花括号 `{}` 包裹箭头函数体，就必须显式写 `return`；只有省略花括号、直接写表达式时，才会自动返回这个表达式的值。

**错误 2：箭头函数里直接修改传入的 `context`，而不是返回新值。**

```ts
// ❌ assign 期望的是"返回新值"，不是"就地修改"
count: ({ context, event }) => {
  context.count += event.amount; // 直接修改了参数对象，这不是 assign 期望的用法
  return context.count;
},
```
虽然这段代码可能"看起来能用"，但违背了 `assign` 的设计意图（第 5 课强调的"声明式描述新值该怎么算"），应该只读取 `context`，返回一个全新计算出来的值，不要修改传入的对象本身。

**错误 3：把箭头函数和普通函数在 `this` 的行为上混淆（这个问题在 XState 场景里较少见，但值得知道）。**

箭头函数不会创建自己的 `this`，它会"继承"外层作用域的 `this`；普通函数会有自己独立的 `this`。XState 的 Action/Guard 几乎都推荐用箭头函数，也和这个特性有关——你不需要担心 `this` 指向谁，只需要关注传进来的参数（`context`、`event` 等）。

## 9 为什么官方这样设计

XState v5 的 Action、Guard、`assign` 字段函数几乎清一色推荐箭头函数写法，是因为这些地方的函数逻辑通常很简短（"从 context/event 里算出一个值"），箭头函数的简洁语法（尤其是省略花括号和 return 的单行写法）能让配置代码保持紧凑、易读，不会被大量的 `function` 关键字和 `return` 语句稀释掉真正重要的逻辑。同时，箭头函数不绑定自己的 `this`，避免了传统函数在被当作回调传递时可能出现的 `this` 指向问题——虽然这些函数本来也不太需要用到 `this`，但选择箭头函数从一开始就规避了这类潜在的困惑。

## 10 一句话总结

**箭头函数是定义函数的简洁写法，在 XState 里几乎所有 Action/Guard/assign 字段都用它来表达"给定输入，该怎么算出结果"。**

## 11 设计思考（Design Thinking）

**需求**：写一个 Guard，判断当前计数是否超过上限。

**方案 A（推荐）**：用箭头函数的单行表达式写法，逻辑一目了然。

```ts
guards: {
  isUnderLimit: ({ context }) => context.count < 100,
},
```

**方案 B（不推荐，但语法上也成立）**：用传统 `function` 关键字，逻辑相同但更啰嗦。

```ts
guards: {
  isUnderLimit: function (params) {
    return params.context.count < 100;
  },
},
```

两种写法功能完全等价，方案 B 并不是"错误"，但在 XState 的代码风格里明显更冗长——多了 `function`/`return` 关键字，参数也不方便直接解构（如果用传统函数写法，通常还要多一行 `const { context } = params;`）。整个 XState 生态（官方文档、示例、社区代码）几乎统一使用箭头函数 + 解构参数的写法，保持这个约定能让你写出的代码和官方文档"长得一样"，也更容易读懂别人写的 XState 代码。这里更多是"社区惯例、代码风格"层面的取舍，而不是"对错"问题。

## 12 设计原则（Design Rules）

本课新增：

- **XState 的 Action/Guard/assign 字段统一用箭头函数表达"给定输入怎么算结果"，保持配置代码简洁。**
- **单行表达式自动 return，一旦加花括号包裹函数体，必须显式写 return。**
- **函数内部只读取传入的 context，不要直接修改它——始终返回全新计算出的值。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[23 Destructuring——`{ context, event }` 这种写法是什么](23-destructuring.md)
