# 19 Union：一个类型可以是好几种可能之一

> 上一课：[18 Object](18-object.md) · 下一课：[20 Literal Type](20-literal-type.md)

继续拆解：

```ts
events: {} as
  | { type: 'NEXT' }
  | { type: 'RESET' }
```

上一课讲了单个对象类型（`{ type: 'NEXT' }` 本身就是一个对象类型）。这一课讲这几个对象类型之间的关系——为什么中间要用竖线 `|` 连起来。

## 1 为什么需要

一个播放器的事件可不止一种：`PLAY`、`PAUSE`、`SET_VOLUME`……每种事件的形状还不一样（`SET_VOLUME` 带 `level` 字段，`PLAY` 什么都不带）。第 18 课的对象类型只能描述"一种固定形状"，但 `send()` 方法需要接受"这些形状里的任意一种"。需要一种方式表达"这个值可能是形状 A，也可能是形状 B，也可能是形状 C"——这就是 Union（联合类型）。

## 2 核心思想

**Union 类型用 `|` 把几个类型连起来，表示"这个值是其中任意一种，但具体是哪种要看运行时的值"。**

## 3 ASCII 图

```
PlayerEvent =
   +----------------------+
   | { type: 'PLAY' }     |
   +----------------------+
              |
              | 或者（|）
              v
   +---------------------------------------+
   | { type: 'SET_VOLUME', level: number } |
   +---------------------------------------+
              |
              | 或者（|）
              v
   +----------------------+
   | { type: 'STOP' }     |
   +----------------------+
```

## 4 JS 写法

```js
// 纯 JS：send 函数接受"任意形状"的事件对象，没有约束
function send(event) {
  if (event.type === 'PLAY') {
    // ...
  } else if (event.type === 'SET_VOLUME') {
    console.log(event.level); // 如果传进来的事件根本没有 level，运行时才发现是 undefined
  }
}
```

## 5 TS 写法

```ts
type PlayEvent = { type: 'PLAY' };
type StopEvent = { type: 'STOP' };
type SetVolumeEvent = { type: 'SET_VOLUME'; level: number };

// Union：这三种类型的任意一种
type PlayerEvent = PlayEvent | StopEvent | SetVolumeEvent;

function send(event: PlayerEvent) {
  if (event.type === 'SET_VOLUME') {
    console.log(event.level); // ✅ TS 知道，走到这个分支，event 一定是 SetVolumeEvent，一定有 level
  }
  // if (event.type === 'PLAY') { console.log(event.level); } // ❌ 编译错误：PlayEvent 上没有 level
}
```

`event.type === 'SET_VOLUME'` 这一行判断非常关键：TypeScript 会根据这个判断，**自动把 `event` 的类型"收窄"**成 `SetVolumeEvent`，所以在这个 if 分支里访问 `event.level` 是安全的，编译器全程知道。这个能力叫"类型收窄"（narrowing），下一课讲字面量类型时会再深入一层。

## 6 XState 中如何使用

```ts
import { setup, createActor, assign } from 'xstate';

const playerMachine = setup({
  types: {
    context: {} as { volume: number },
    events: {} as
      | { type: 'PLAY' }
      | { type: 'STOP' }
      | { type: 'SET_VOLUME'; level: number },
  },
  actions: {
    setVolume: assign({
      volume: ({ event }) => {
        if (event.type !== 'SET_VOLUME') return 0;
        return event.level; // 这里能安全访问 .level，正是因为上面判断先收窄了类型
      },
    }),
  },
}).createMachine({
  id: 'player',
  initial: 'idle',
  context: { volume: 50 },
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
player.send({ type: 'SET_VOLUME', level: 80 }); // ✅ 合法
// player.send({ type: 'SET_VOLUME' }); // ❌ 编译错误：缺少 level
// player.send({ type: 'FLY' }); // ❌ 编译错误：不在 Union 里的事件类型
```

这就是第 3 课埋下的伏笔"第 5 课起会看到完整的事件类型标注"的落地：`types.events` 用 Union 把这台机器"所有可能收到的事件形状"一次性列全，之后 `send()` 就会在编译期检查你传的事件对象合不合法。

## 7 TypeScript 语法拆解

```ts
type PlayerEvent = PlayEvent | StopEvent | SetVolumeEvent;
```
`|` 是 Union 类型的语法符号，读作"或者"。`PlayerEvent` 这个类型的值，运行时可能是 `PlayEvent`、`StopEvent`、`SetVolumeEvent` 里的任意一个具体形状，但**任意时刻，它一定是某一个具体的形状，不是"三者的合体"**——这一点很容易望文生义搞错，Union 不是"同时具备三者所有字段"，而是"三选一"。

```ts
events: {} as
  | { type: 'PLAY' }
  | { type: 'STOP' }
  | { type: 'SET_VOLUME'; level: number },
```
这里没有先用 `type` 给每种事件单独起名字，而是直接把三个对象类型内联写在一起，用 `|` 连接——这在 XState 的 `types.events` 里是最常见的写法（当事件种类不多、不需要在别处复用某个具体事件类型时）。每一行前面的 `|` 只是格式化风格（把第一个 `|` 也对齐写出来，纯粹为了美观和方便增删行），语法上等价于 `{ type: 'PLAY' } | { type: 'STOP' } | { type: 'SET_VOLUME'; level: number }`。

```ts
if (event.type !== 'SET_VOLUME') return 0;
```
这是"收窄" Union 类型最常见的手法：通过判断 `type` 字段（这个字段在 Union 里的每个分支都恰好是不同的字面量值，第 20 课细讲），TypeScript 就能推断出"排除了 SET_VOLUME 之后，剩下的分支要么没有 level，要么这行代码之后 event 一定是 SET_VOLUME"。

## 8 常见错误

**错误 1：以为 Union 类型的值"同时具备"所有分支的字段。**

```ts
type Event = { type: 'PLAY' } | { type: 'SET_VOLUME'; level: number };

function handle(event: Event) {
  console.log(event.level); // ❌ 编译错误：PLAY 分支没有 level，TS 不允许在没收窄前访问
}
```
必须先判断 `event.type`，收窄之后才能访问某个分支特有的字段。

**错误 2：忘记给 Union 里的每个分支的 `type` 用不同的字面量值，导致 TypeScript 无法区分到底走的是哪个分支。**

```ts
// ❌ 两个分支的 type 字段类型都是宽泛的 string，无法用来区分
type BadEvent = { type: string; level: number } | { type: string };
```
Union 要配合下一课的字面量类型（每个分支的 `type` 是具体不同的字符串），才能真正发挥"根据 type 自动收窄"的威力。

**错误 3：Union 分支列得不全，遗漏了实际会用到的事件类型。**

```ts
// ❌ 忘记把 SET_VOLUME 加进 Union，导致 send({ type: 'SET_VOLUME', level: 80 }) 编译报错
events: {} as { type: 'PLAY' } | { type: 'STOP' },
```
`types.events` 里的 Union 必须覆盖这台机器实际会用到的**所有**事件类型，少一个都会导致对应的 `send` 调用在编译期报错。

## 9 为什么官方这样设计

XState 的事件天然是"多种不同形状、但共享一个 `type` 判别字段"的数据——这正是 TypeScript Union 类型（尤其是"可辨识联合"，discriminated union）最擅长表达的场景。用 Union 而不是"一个包含所有字段、大部分可选"的单一对象类型来描述事件，好处是**类型系统能精确知道"某种具体事件下，哪些字段一定存在、哪些字段根本不该出现"**，而不是所有字段都变成"可能有可能没有"的可选属性（第 26 课会讲可选属性，以及为什么那不是这里的正确选择）。这让 Action/Guard 函数里访问 `event.xxx` 时，既有类型安全，又不需要写一堆可选判断。

## 10 一句话总结

**Union 用 `|` 表达"这个值是几种可能形状之一"，配合对 `type` 字段的判断，可以让 TypeScript 精确收窄到具体是哪一种。**

## 11 设计思考（Design Thinking）

**需求**：给播放器声明所有可能的事件类型。

**方案 A（推荐）**：用 Union 把每种事件的具体形状分别列出来。

```ts
events: {} as
  | { type: 'PLAY' }
  | { type: 'STOP' }
  | { type: 'SET_VOLUME'; level: number },
```

**方案 B（错误）**：用一个"万能事件类型"，把所有可能用到的字段都塞进同一个对象类型，标成可选。

```ts
// ❌ 所有事件长得一样宽泛，完全没有区分度
events: {} as {
  type: string;
  level?: number;
},
```

方案 B 表面上"能覆盖所有情况"，但代价是类型系统完全无法帮你检查"这次是不是传错了事件"——`{ type: 'PLAY', level: 999 }` 在方案 B 下是合法的（`level` 是可选字段），但这在业务上毫无意义（`PLAY` 事件本不该有 `level`）；而且 `type: string` 意味着 `send({ type: 'TYPO_EVENT' })` 这种拼写错误也完全不会被拦截。方案 A 用 Union 让每种事件的形状都精确、独立，任何不匹配某个具体分支的写法都会在编译期报错，这正是 Union 存在的核心价值。

## 12 设计原则（Design Rules）

本课新增：

- **多种可能形状的数据（尤其是事件）用 Union 类型表达，而不是用一个"万能对象+可选字段"敷衍了事。**
- **Union 类型的值任意时刻是"其中一种"，不是"所有分支字段的合体"，访问某分支特有字段前必须先判断收窄。**
- **`types.events` 的 Union 必须覆盖机器实际会用到的所有事件类型，遗漏会导致对应 `send` 调用编译报错。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[20 Literal Type——为什么 `type: 'PLAY'` 不是普通字符串](20-literal-type.md)
