# 24 type / interface：给类型起名字的两种方式

> 上一课：[23 Destructuring](23-destructuring.md) · 下一课：[25 Generic](25-generic.md)

## 1 为什么需要

第 18-21 课一直在 `setup({ types: { context: {} as {...} } })` 里"内联"写对象类型。但如果这个 Context 形状比较复杂，或者需要在好几个地方复用同一个类型（比如既要标注 Context，又要标注某个函数的参数），每次都完整重写一遍花括号里的内容既啰嗦又容易在多处修改时漏改。需要一种方式"给一个类型起个名字"，以后直接用这个名字引用，这就是 `type` 和 `interface` 要解决的问题。

## 2 核心思想

**`type` 和 `interface` 都是"给一个类型起名字，以后可以复用这个名字代替重复书写完整定义"的语法，在 XState 场景下，两者绝大多数时候可以互换，官方示例更常用 `type`。**

## 3 ASCII 图

```
不起名字，每次都重写:
  function a(x: { count: number }) {...}
  function b(x: { count: number }) {...}    <- 重复了同样的形状

起名字后，复用:
  type Counter = { count: number };
  function a(x: Counter) {...}
  function b(x: Counter) {...}              <- 改一处，两处都同步
```

## 4 JS 写法

纯 JS 没有类型，自然也没有"给类型起名字"这个概念——这一课的内容是 TypeScript 特有的，JS 部分只能展示"如果没有这个能力，会怎样"：

```js
// 纯 JS：靠注释或命名约定表达"这个参数应该长什么样"，没有工具能强制校验
// 参数 ctx 应该是 { count: number } 的形状
function increment(ctx) {
  return ctx.count + 1;
}
```

## 5 TS 写法

用 `type` 起名字：

```ts
type CounterContext = {
  count: number;
};

function increment(ctx: CounterContext) {
  return ctx.count + 1;
}
```

用 `interface` 起名字（对于对象形状，写法几乎一样）：

```ts
interface CounterContextI {
  count: number;
}

function increment2(ctx: CounterContextI) {
  return ctx.count + 1;
}
```

两者在这个场景下效果相同。区别主要在更进阶的用法上（比如 `interface` 支持"声明合并"，`type` 支持给 Union、元组这类非对象结构起名字），本课不深入这些差异——**在 XState 的日常使用中，记住"给对象形状起名字，官方示例习惯用 `type`"就足够了**。

## 6 XState 中如何使用

```ts
import { setup, assign, createActor } from 'xstate';

// 给 Context 和 Event 的形状分别起名字
type PlayerContext = {
  volume: number;
  currentSong: string;
};

type PlayerEvent =
  | { type: 'PLAY' }
  | { type: 'STOP' }
  | { type: 'SET_VOLUME'; level: number };

const playerMachine = setup({
  types: {
    context: {} as PlayerContext, // 直接引用命名类型，而不是重新写一遍花括号
    events: {} as PlayerEvent,
  },
  actions: {
    setVolume: assign({
      volume: ({ event }) => {
        if (event.type !== 'SET_VOLUME') return 0;
        return event.level;
      },
    }),
  },
}).createMachine({
  id: 'player',
  initial: 'idle',
  context: { volume: 50, currentSong: 'Track 1' },
  states: {
    idle: {
      on: {
        PLAY: { target: 'playing' },
        SET_VOLUME: { actions: 'setVolume' },
      },
    },
    playing: {
      on: { STOP: { target: 'idle' } },
    },
  },
});

const player = createActor(playerMachine).start();
```

给类型起名字之后，`PlayerContext`、`PlayerEvent` 还可以在别的地方复用——比如你想单独写一个"格式化当前播放信息"的工具函数，可以直接用 `PlayerContext` 标注参数类型，不需要重新写一遍 `{ volume: number; currentSong: string }`。

## 7 TypeScript 语法拆解

```ts
type PlayerContext = {
  volume: number;
  currentSong: string;
};
```
`type` 是关键字，`PlayerContext` 是你给这个类型起的名字（约定俗成用大写字母开头的驼峰命名，PascalCase），等号右边是这个名字所代表的具体类型定义（这里是第 18 课学的对象类型）。

```ts
type PlayerEvent =
  | { type: 'PLAY' }
  | { type: 'STOP' }
  | { type: 'SET_VOLUME'; level: number };
```
`type` 同样可以给 Union 类型起名字（这是 `interface` 做不到的——`interface` 只能描述对象形状，不能直接表示"或者"关系的 Union），这也是 XState 官方文档偏好用 `type` 而不是 `interface` 的原因之一：事件类型几乎总是 Union，用 `type` 更自然、统一。

```ts
context: {} as PlayerContext,
```
把第 21 课学的 `as` 断言目标，从"内联的完整类型定义"换成"一个命名类型的引用"——语法上完全等价，只是可读性更好、也更方便复用。

## 8 常见错误

**错误 1：给类型和给变量起了容易混淆的相同或相近名字。**

```ts
// ❌ 类型名和变量名撞在一起，容易读混
type context = { count: number }; // 类型名不该用小写开头，也不该和常见变量名撞车
const context = { count: 0 };
```
遵守"类型用大写开头的 PascalCase（`PlayerContext`），变量/字段用小写开头的 camelCase（`context`）"这个约定，能极大减少这类混淆。

**错误 2：以为命名类型和内联类型在 XState 里的行为有区别。**

```ts
// 这两种写法在 XState 眼里完全等价，只是可读性和复用性不同
context: {} as PlayerContext,
context: {} as { volume: number; currentSong: string },
```
命名类型只是"起了个名字方便复用"，不会改变任何运行时行为或类型检查的严格程度。

**错误 3：在需要 Union 的地方误用 `interface`，导致语法错误或不得不用更绕的写法。**

```ts
// ❌ interface 不能直接表示 Union
interface PlayerEvent {
  // 没有办法在这里写 "type: 'PLAY'" | "type: 'STOP'" 这种或的关系
}
```
需要 Union 类型时用 `type`，这是本课要记住的最实用的判断标准。

## 9 为什么官方这样设计

XState 官方文档和示例代码统一偏好 `type` 而不是 `interface`，核心原因是**事件类型几乎总是需要表达"多种可能形状之一"（Union）**，而这是 `type` 的拿手好戏、`interface` 做不到的能力。既然 Context 用 `type` 也完全够用（对象形状两者都能表达），为了保持代码风格统一，XState 生态干脆统一用 `type`，不需要你在"这里该用 type 还是 interface"这个问题上纠结——记住"XState 里统一用 `type`"就够了，除非你在和某个明确要求 `interface` 的第三方库集成。

## 10 一句话总结

**`type` 给类型起名字方便复用，XState 场景下统一使用 `type`（而不是 `interface`），因为事件类型经常需要用到 `type` 才能表达的 Union。**

## 11 设计思考（Design Thinking）

**需求**：给一台稍微复杂的状态机的 Context 和 Event 类型命名，方便复用和阅读。

**方案 A（推荐）**：在 `setup()` 调用之外，用 `type` 提前把 Context、Event 的形状命名清楚，再在 `types` 里引用这些名字。

```ts
type OrderContext = { amount: number; isVerified: boolean };
type OrderEvent = { type: 'SUBMIT' } | { type: 'APPROVE' };

setup({
  types: { context: {} as OrderContext, events: {} as OrderEvent },
})
```

**方案 B（错误，或者说不推荐）**：所有类型都直接内联写在 `setup({ types: {...} })` 内部，哪怕内容又长又复杂。

```ts
// ❌ 内容一多，setup 调用变得又臭又长，且这些类型无法在别处复用
setup({
  types: {
    context: {} as {
      amount: number;
      isVerified: boolean;
      customerInfo: { name: string; email: string; address: { city: string; zip: string } };
      // ... 继续堆砌
    },
  },
})
```

方案 B 在类型比较简单时完全没问题（前面几课就一直这样写），但一旦 Context/Event 的形状变得复杂（嵌套对象、字段很多），内联写法会让 `setup()` 调用本身变得难以阅读，而且如果项目里其他地方（比如一个独立的工具函数）也需要用到"订单 Context"这个形状，只能被迫重新抄一遍，未来任何一处修改都可能忘记同步另一处。方案 A 提前命名，`setup()` 内部只留一个简短的类型引用，可读性和可维护性都更好。**判断标准很简单：类型定义如果只有一两行，内联写也无妨；一旦变复杂或需要复用，就该提前用 `type` 命名。**

## 12 设计原则（Design Rules）

本课新增：

- **简单类型可以内联写在 `setup({ types: {...} })` 里；复杂或需要复用的类型应该提前用 `type` 命名。**
- **XState 场景统一使用 `type` 而不是 `interface`，因为事件类型经常需要 `type` 才支持的 Union 表达能力。**
- **类型名用 PascalCase（大写开头驼峰），变量/字段名用 camelCase（小写开头驼峰），避免混淆。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[25 Generic——`ActorRefFrom<T>` 这类尖括号是什么](25-generic.md)
