# 25 Generic：`ActorRefFrom<T>` 这类尖括号是什么

> 上一课：[24 type / interface](24-type-interface.md) · 下一课：[26 Optional Property](26-optional-property.md)

## 1 为什么需要

第 16 课出现过这样一行代码：

```ts
downloads: ActorRefFrom<typeof downloadTaskLogic>[];
```

`ActorRefFrom<...>` 后面跟着尖括号，里面又放了一个类型——这是什么写法？现实问题是：XState 提供了很多"工具类型"（比如"给定某个 Actor 逻辑，推导出它的引用类型"），这些工具需要能够"套用在任意具体类型上"，而不是只为某一个固定类型写死。需要一种方式表达"这个类型工具，可以接收另一个类型作为参数，加工后产出一个新类型"——这就是泛型（Generic）。

## 2 核心思想

**泛型是"类型层面的参数"：一个类型定义（或函数）可以先留一个"占位类型"，使用时再填入具体类型，让同一套逻辑能安全地套用在不同的具体类型上。**

## 3 ASCII 图

```
不用泛型（重复劳动）:
  type ActorRefForLogin = ActorRef<...针对登录 Actor 写死的具体类型...>
  type ActorRefForDownload = ActorRef<...针对下载 Actor 写死的具体类型...>
       （逻辑完全一样，只是套用的具体类型不同，要重复定义）

用泛型（一套逻辑，套用不同类型）:
  type ActorRefFrom<T> = ...基于 T 推导出的引用类型...
                    |
        传入不同的 T ------+------+
                    v             v
       ActorRefFrom<LoginLogic>  ActorRefFrom<DownloadLogic>
```

## 4 JS 写法

纯 JS 没有类型，自然没有"类型层面的参数"这个概念，但可以先用"函数参数"类比感受一下"参数化"这个思想本身：

```js
// 函数参数是"值层面的参数"：同一套逻辑，喂进不同的具体值
function double(x) {
  return x * 2;
}
double(3); // 6
double(10); // 20
```

泛型就是把这个"参数化"的思想，从"值"提升到了"类型"这个层面。

## 5 TS 写法

先看一个不用泛型、重复劳动的例子：

```ts
// ❌ 为每种具体类型都单独写一遍"包一层数组"的类型，逻辑重复
type LoginRefs = { ref: string }[];
type DownloadRefs = { ref: number }[];
```

用泛型抽象出"通用逻辑"：

```ts
// 定义一个泛型类型：Box<T> 表示"一个装着 T 类型值的盒子"
type Box<T> = { value: T };

type StringBox = Box<string>; // { value: string }
type NumberBox = Box<number>; // { value: number }

const a: StringBox = { value: 'hello' };
const b: NumberBox = { value: 42 };
```

`Box<T>` 里的 `T` 是一个"占位类型"（约定俗成用单个大写字母，最常见的是 `T`，代表 Type），使用时通过 `Box<string>`、`Box<number>` 这种尖括号语法"填入"具体类型，`T` 在定义体内所有出现的地方都会被替换成你填入的那个具体类型。

泛型也常用在函数上：

```ts
function identity<T>(value: T): T {
  return value;
}

const x = identity<string>('hello'); // T 被指定为 string
const y = identity(42); // 不写 <number> 也可以，TS 会根据传入的值自动推断 T 是 number
```

## 6 XState 中如何使用

```ts
import { setup, fromCallback, assign, createActor, type ActorRefFrom } from 'xstate';

const downloadTaskLogic = fromCallback<{ type: 'CANCEL' }, { url: string }>(
  ({ input, sendBack, receive }) => {
    // ... 具体实现省略（第 16 课完整给出过）
    return () => {};
  },
);

const downloaderMachine = setup({
  types: {
    context: {} as {
      // ActorRefFrom<T> 是 XState 提供的泛型工具类型：
      // 给定一个 Actor 逻辑的类型，推导出"指向它的引用"的类型
      downloads: ActorRefFrom<typeof downloadTaskLogic>[];
    },
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
            downloads: ({ context, spawn }) => [
              ...context.downloads,
              spawn('downloadTaskLogic', { input: { url: 'file.zip' } }),
            ],
          }),
        },
      },
    },
  },
});
```

`ActorRefFrom<typeof downloadTaskLogic>` 这行代码，把泛型的"占位类型"填成了"`downloadTaskLogic` 这个变量的类型"（`typeof` 是取变量类型的语法，这里配合泛型一起出现），最终推导出"指向 `downloadTaskLogic` 实例的引用"这个具体类型——如果项目里还有别的 Actor 逻辑（比如 `loginWork`），完全可以写 `ActorRefFrom<typeof loginWork>` 得到另一个对应的引用类型，`ActorRefFrom` 这一套推导逻辑是通用的，不需要为每种 Actor 逻辑单独定义一遍。

## 7 TypeScript 语法拆解

```ts
type Box<T> = { value: T };
```
`Box<T>` 里的 `<T>` 声明"这个类型接收一个类型参数，暂且叫它 T"，定义体 `{ value: T }` 里的 `T` 会在实际使用时被替换成具体类型。

```ts
ActorRefFrom<typeof downloadTaskLogic>
```
`ActorRefFrom` 是 XState 库导出的一个泛型类型（定义方式和 `Box<T>` 是同一种机制，只是内部实现复杂得多），尖括号里 `typeof downloadTaskLogic` 就是"填入的具体类型参数"。`typeof` 在这里的作用是"给我 `downloadTaskLogic` 这个变量的类型"（而不是它的值），这是 TypeScript 里"从值获取类型"的常见手法，经常和泛型搭配出现。

```ts
fromCallback<{ type: 'CANCEL' }, { url: string }>(...)
```
第 16 课出现过的这行代码，`fromCallback` 本身也是一个泛型函数，尖括号里传入了两个类型参数（分别对应"这个 Actor 能接收的事件类型"和"需要的 input 类型"），这是"一个泛型可以接收多个类型参数，用逗号分隔"的例子。

## 8 常见错误

**错误 1：以为泛型的尖括号是"函数调用"的括号，混淆类型参数和函数参数。**

```ts
Box<string>  // 这是"类型"，尖括号里是类型参数
Box(string)  // ❌ 这样写是错的，圆括号是函数调用，Box 是一个类型不是函数
```

**错误 2：手动写复杂的泛型类型参数，而不是让 TypeScript 自动推断。**

```ts
// 大多数场景下，TS 能根据上下文自动推断出泛型参数，不需要每次都手写尖括号
const y = identity(42); // TS 自动推断 T 是 number，不需要写成 identity<number>(42)
```
只有在 TypeScript 无法自动推断、或者你想显式覆盖推断结果时，才需要手写尖括号里的类型参数。

**错误 3：看到 XState 类型定义里大量的泛型（比如 `ActorRefFrom<T>`、`Actor<TLogic>`），试图逐字逐句读懂它们的内部实现。**

作为 XState 的**使用者**，你只需要知道"这个工具类型接收什么类型参数、用来干什么"（比如 `ActorRefFrom<T>` 用来"从 Actor 逻辑类型推导出引用类型"），不需要读懂 XState 源码里这些工具类型是怎么用更底层的泛型技巧实现的——这已经超出"读懂 XState 所需的 TypeScript"这门课的范围，属于"如果你想自己写类型库"才需要深入的领域。

## 9 为什么官方这样设计

XState 的类型系统大量使用泛型，是因为 XState 本身是一个"通用引擎"——它不知道你会用它来建模播放器、门、订单还是电梯，每个用户的 Context、Event、Actor 逻辑形状都不一样。泛型正是 TypeScript 里表达"这套逻辑对任意具体类型都成立，具体类型由使用者决定"的标准工具。`setup()` 函数本身也大量运用泛型，才能做到"你传入什么样的 `types.context`/`types.events`，后续所有 Action/Guard 回调函数里的 `context`/`event` 参数就自动获得对应的精确类型"——这条第 5 课就提到过的"类型推导链路"，背后的实现机制正是泛型。

## 10 一句话总结

**泛型是"类型层面的参数"，让同一套类型定义或函数能安全地套用在不同的具体类型上；作为 XState 使用者，你只需要知道常见工具类型（如 `ActorRefFrom<T>`）接收什么、产出什么，不必深究其内部实现。**

## 11 设计思考（Design Thinking）

**需求**：项目里有好几种不同的 Actor 逻辑（登录、下载、心跳），都需要把它们的引用类型存进各自 Machine 的 Context 里。

**方案 A（推荐）**：直接使用 XState 提供的泛型工具 `ActorRefFrom<T>`，针对不同的 Actor 逻辑填入不同的类型参数。

```ts
type LoginContext = { loginRef: ActorRefFrom<typeof loginWork> };
type DownloaderContext = { downloads: ActorRefFrom<typeof downloadTaskLogic>[] };
```

**方案 B（错误）**：为每种 Actor 逻辑手写一个专属的引用类型，而不是复用泛型工具。

```ts
// ❌ 每种 Actor 都要手动搞清楚"引用类型"该怎么定义，容易写错、写漏字段，且逻辑重复
type LoginRef = { send: (event: any) => void; getSnapshot: () => any /* ... 手动猜测还有哪些字段 */ };
```

方案 B 需要你自己搞清楚"一个 Actor 引用到底应该长什么样"（有哪些方法、哪些属性），这些细节在 XState 内部可能会随版本演进而调整，手写的类型很容易过时或本身就不准确。方案 A 直接复用官方维护的泛型工具，`ActorRefFrom<T>` 内部的推导逻辑由 XState 团队负责维护和更新，你只需要专注于"我的 Actor 逻辑的类型是什么"，剩下的交给泛型机制自动推导——这正是泛型"一套逻辑，套用在任意类型上"的价值所在。

## 12 设计原则（Design Rules）

本课新增：

- **泛型是类型层面的参数，让工具类型能套用在任意具体类型上；作为使用者，优先复用官方提供的泛型工具（如 `ActorRefFrom<T>`），而不是手写等价逻辑。**
- **大多数场景下让 TypeScript 自动推断泛型参数，只有推断失败或需要显式覆盖时才手写尖括号。**
- **不必读懂 XState 内部泛型工具的实现细节，只需要知道它们"接收什么、产出什么"。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课（也是本部分最后一课）：[26 Optional Property——什么时候一个字段可以不存在](26-optional-property.md)
