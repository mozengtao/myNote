# 20 Literal Type：为什么 `type: 'PLAY'` 不是普通字符串

> 上一课：[19 Union](19-union.md) · 下一课：[21 as](21-as.md)

继续拆解：

```ts
events: {} as
  | { type: 'NEXT' }
  | { type: 'RESET' }
```

上一课讲了 `|` 把两个对象类型连成 Union。这一课深入每个对象类型内部，看 `type: 'NEXT'` 这个字段类型到底是什么——它不是"任意字符串"，而是"只能是这一个字符串"。

## 1 为什么需要

第 19 课能实现"根据 `event.type` 收窄到具体分支"，靠的就是每个分支的 `type` 字段类型都不一样、而且都很"精确"。如果 `type` 字段的类型只是笼统的 `string`，TypeScript 只知道"这是个字符串"，不知道"具体是哪个字符串"，也就没法据此区分 Union 的不同分支。需要一种比 `string` 更精确的类型：**只允许一个具体的值**。这就是字面量类型（Literal Type）。

## 2 核心思想

**字面量类型不是"某一类值的集合"（比如所有字符串），而是"恰好这一个具体值"，用来让类型系统精确到"值本身"，而不只是"值的种类"。**

## 3 ASCII 图

```
string 类型（宽泛）:
  "PLAY", "STOP", "hello", "anything", ... 全部合法

'PLAY' 字面量类型（精确）:
  只有 "PLAY" 这一个值合法，其他一律不允许
```

## 4 JS 写法

```js
// 纯 JS 没有"字面量类型"的概念，这里只能通过运行时判断实现类似效果
function send(event) {
  if (event.type === 'PLAY') {
    // 只有运行到这里、这个判断为真时，你才能"确信"event.type 就是 'PLAY'
    // 但这只是运行时的信息，写代码的当下 IDE 并不知道
  }
}
```

## 5 TS 写法

```ts
// 普通 string：任何字符串都合法
let a: string = 'PLAY';
a = 'anything'; // ✅ 合法，因为 string 类型没有限制具体值

// 字面量类型：只有这一个值合法
let b: 'PLAY' = 'PLAY';
// b = 'anything'; // ❌ 编译错误：Type '"anything"' is not assignable to type '"PLAY"'
```

单独使用字面量类型的场景不多，它真正的威力是**和 Union 组合**，表达"只能是这几个具体值之一"：

```ts
type PlayerAction = 'PLAY' | 'PAUSE' | 'STOP'; // 只能是这三个字符串之一

let action: PlayerAction = 'PLAY'; // ✅
// action = 'JUMP'; // ❌ 编译错误：不在允许的三个值范围内
```

## 6 XState 中如何使用

```ts
import { setup, createActor } from 'xstate';

const playerMachine = setup({
  types: {
    events: {} as
      | { type: 'PLAY' }   // 'PLAY' 是字面量类型，不是普通 string
      | { type: 'STOP' },  // 'STOP' 同理
  },
}).createMachine({
  id: 'player',
  initial: 'idle',
  states: {
    idle: {
      on: { PLAY: { target: 'playing' } },
    },
    playing: {
      on: { STOP: { target: 'idle' } },
    },
  },
});

const player = createActor(playerMachine).start();

player.send({ type: 'PLAY' });  // ✅ 'PLAY' 匹配字面量类型
// player.send({ type: 'play' }); // ❌ 编译错误：大小写不对，'play' 不是 'PLAY'
// player.send({ type: 'FLY' });  // ❌ 编译错误：根本不在 Union 里
```

如果 `types.events` 里 `type` 字段写的是宽泛的 `string`（而不是字面量），上面两个错误示例都不会在编译期被拦下来，只有运行时才会发现"这个事件没人处理"（回顾第 3 课"未监听事件被静默忽略"）。**字面量类型 + Union，正是 XState v5 能在编译期帮你堵住"事件名拼写错误"这类低级 bug 的根本原因。**

## 7 TypeScript 语法拆解

```ts
let b: 'PLAY' = 'PLAY';
```
冒号后面的 `'PLAY'` 是**类型**（出现在类型位置，即变量声明的 `:` 之后），等号后面的 `'PLAY'` 是**值**（普通的字符串字面量）。这是初学者最容易迷惑的地方：同样的字符 `'PLAY'`，出现在类型位置和值位置，含义完全不同——类型位置的 `'PLAY'` 表示"这个类型只包含这一个值"，值位置的 `'PLAY'` 就是普通的字符串数据。

```ts
type PlayerAction = 'PLAY' | 'PAUSE' | 'STOP';
```
多个字面量类型用 `|` 连接（复习第 19 课），构成一个"只能是这几个具体值之一"的类型，这种模式在 TypeScript 里非常常见，专门有个名字叫"字符串字面量联合类型"（string literal union）。

```ts
events: {} as { type: 'PLAY' } | { type: 'STOP' },
```
组合回第 18-19 课的写法：每个对象类型里的 `type` 字段都用字面量类型标注，多个对象类型之间用 `|` 连接——这正是"可辨识联合"（discriminated union）的标准写法：用一个字段（这里是 `type`）的字面量值来"辨识"当前具体是 Union 里的哪一个分支。

## 8 常见错误

**错误 1：把字面量类型和普通 `string` 类型混用，导致 TypeScript 把整个 Union 推断成宽泛的 `string`。**

```ts
// ❌ 如果不显式标注类型，TS 可能把 'PLAY' 推断成宽泛的 string 而不是字面量类型
const event = { type: 'PLAY' }; // 在某些上下文里，type 的推断类型可能是 string
```
这也是为什么 `types.events` 要显式写成 `{} as { type: 'PLAY' } | ...` 而不是让 TS 自己推断——显式标注能确保得到的是精确的字面量类型而不是宽泛的 `string`。

**错误 2：以为字面量类型只能是字符串。**

数字和布尔值同样可以是字面量类型：`type Zero = 0;`、`type True = true;`，只是 XState 的事件 `type` 字段约定俗成用字符串，所以本课例子都以字符串为主。

**错误 3：在 Union 里给不同分支使用相同的字面量值，导致无法区分。**

```ts
// ❌ 两个分支的 type 都是 'PLAY'，TS 没法根据 type 区分到底是哪个分支
type BadEvent = { type: 'PLAY'; a: number } | { type: 'PLAY'; b: string };
```
每个 Union 分支的判别字段（这里是 `type`）必须是**互不相同**的字面量值，否则"可辨识联合"就失去了辨识能力。

## 9 为什么官方这样设计

字面量类型存在的意义，是让 TypeScript 的类型系统能够精确到"具体的值"，而不只是"值的种类"。对 XState 来说，这个能力至关重要：状态名、事件名本质上都是**有限、具体的字符串集合**（回顾第 2 课"State 的取值范围必须有限"），字面量类型 + Union 恰好就是 TypeScript 里表达"有限、具体的集合"最自然的工具。没有字面量类型，`send({ type: string })` 这样的签名就只能接受"任意字符串"，完全无法在编译期校验事件名是否拼写正确、是否属于这台机器认识的事件——这会让 TypeScript 在 XState 这个场景下的价值大打折扣。

## 10 一句话总结

**字面量类型让一个类型精确到"具体的这一个值"，配合 Union 使用，正是 XState 能在编译期校验状态名、事件名拼写是否正确的根本机制。**

## 11 设计思考（Design Thinking）

**需求**：确保 `send()` 时传的事件名一定是这台机器认识的，杜绝拼写错误。

**方案 A（推荐）**：`types.events` 里每个分支的 `type` 用字面量类型精确标注。

```ts
events: {} as { type: 'PLAY' } | { type: 'STOP' },
```

**方案 B（错误）**：图方便，把 `type` 标注成宽泛的 `string`。

```ts
// ❌ 完全放弃了字面量类型带来的编译期校验能力
events: {} as { type: string },
```

方案 B 里，`send({ type: 'PLAAY' })`（拼错的事件名）在编译期完全不会报错，因为 `'PLAAY'` 也是一个合法的 `string`，只有运行时才会发现"这个事件没人处理"（回顾第 3 课的静默忽略行为）——而这类 bug 往往很隐蔽，因为程序不会崩溃，只是某个按钮点了没反应，排查起来比编译期报错麻烦得多。方案 A 用字面量类型把"能不能这样调用"这件事提前到写代码的当下就能发现，这正是本课想建立的核心认知：**字面量类型不是 TypeScript 的炫技语法，而是"用类型系统替你记住所有合法值"的实用工具。**

## 12 设计原则（Design Rules）

本课新增：

- **状态名、事件名这类"有限、具体的字符串集合"应该用字面量类型 + Union 标注，而不是宽泛的 `string`。**
- **同一 Union 里的判别字段（如 `type`）必须在各分支间互不相同，否则失去区分能力。**
- **显式标注字面量类型（而不依赖 TS 自动推断），能确保得到精确类型而不是被拓宽成 `string`。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[21 as——为什么 `{} as {...}` 这样写](21-as.md)
