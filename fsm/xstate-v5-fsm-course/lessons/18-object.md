# 18 Object：给数据形状起个名字的起点

> 上一课：[17 生命周期](17-lifecycle.md) · 下一课：[19 Union](19-union.md)

第六部分只讲"读懂 XState 所需"的 TypeScript，不系统教 TS。目标是让你看懂官方文档和 XState 类型定义，而不是成为 TS 专家。这一课我们会开始拆解贯穿本部分的经典例子：

```ts
events: {} as
  | { type: 'NEXT' }
  | { type: 'RESET' }
```

拆解顺序固定：**Object → Union → Literal → as**，一步一步来，这一课先讲 Object。

## 1 为什么需要

第 3 课就说过，XState 的事件永远是对象形式：`{ type: 'PLAY' }`，不是裸字符串。为什么要多包一层？因为事件经常需要携带"不止一个字段"的信息——`type` 说明"发生了什么"，其他字段（比如 `level`）说明"具体的数据"。JavaScript 里，"一组相关的字段打包在一起"这个需求，天然的解决方案就是 Object（对象）。TypeScript 需要一种方式描述"这个对象应该长什么形状"，这就是本课要讲的内容。

## 2 核心思想

**Object 类型描述"一个对象应该有哪些字段、每个字段是什么类型"，是 TypeScript 里最基础的"形状描述"工具。**

## 3 ASCII 图

```
+---------------------------+
| { type: 'SET_VOLUME',     |
|   level: 80 }             |
+---------------------------+
        |
        | 这个对象的"形状"用 TS 描述出来：
        v
+---------------------------+
| { type: string;           |
|   level: number }         |
+---------------------------+
```

## 4 JS 写法

```js
// 纯 JS：没有类型标注，靠开发者自己记住这个对象"应该"长什么样
const event = { type: 'SET_VOLUME', level: 80 };

function handleEvent(e) {
  console.log(e.level); // 如果 e 其实没有 level 字段，运行时才会发现（undefined，不会报错）
}
```

## 5 TS 写法

```ts
// 用一个"对象类型"描述这个事件的形状
type SetVolumeEvent = {
  type: 'SET_VOLUME';
  level: number;
};

const event: SetVolumeEvent = { type: 'SET_VOLUME', level: 80 };

function handleEvent(e: SetVolumeEvent) {
  console.log(e.level); // 编译期就知道 e 一定有 level，且是 number
}

// 如果字段缺失或类型不对，编译期直接报错：
// const bad: SetVolumeEvent = { type: 'SET_VOLUME' }; // ❌ 缺少 level
// const bad2: SetVolumeEvent = { type: 'SET_VOLUME', level: '80' }; // ❌ level 应该是 number
```

## 6 XState 中如何使用

```ts
import { setup, createActor } from 'xstate';

const machine = setup({
  types: {
    // 这里的 {} as {...} 就是一个"对象类型"标注（as 部分留到第 21 课细讲）
    context: {} as { volume: number },
  },
}).createMachine({
  id: 'player',
  initial: 'active',
  context: { volume: 50 },
  states: {
    active: {},
  },
});

const player = createActor(machine).start();
console.log(player.getSnapshot().context.volume); // TS 知道这是 number
```

`types.context` 里花括号 `{ volume: number }` 就是一个对象类型，它告诉 TypeScript："这台机器的 Context 永远长这个形状"，之后所有 `context.volume` 的引用都会自动获得类型检查和自动补全。

## 7 TypeScript 语法拆解

```ts
type SetVolumeEvent = {
  type: 'SET_VOLUME';
  level: number;
};
```
`type SetVolumeEvent = ` 这部分是"给一个类型起名字"的语法（`type` 关键字，第 24 课会细讲），等号右边花括号里的内容才是真正的对象类型。

```ts
{
  type: 'SET_VOLUME';
  level: number;
}
```
花括号里，每一行是一个"字段名: 字段类型"的声明，字段之间用分号（也可以用逗号，两者在对象类型里等价）分隔。`type: 'SET_VOLUME'` 这里的类型不是泛泛的 `string`，而是"只能是这一个具体字符串"——这是第 20 课要讲的字面量类型，这里先不展开，只需要知道对象类型里的每个字段可以是任意类型，包括这种"更精确"的类型。

```ts
context: {} as { volume: number },
```
`{}` 是一个空对象**值**（JS 层面的空对象），`as { volume: number }` 是把这个空对象"断言"成某个类型（第 21 课细讲）。这里之所以先写一个空对象再断言类型，是因为 `types` 字段的作用**只是声明类型**，不提供真实的初始数据（真实的初始 Context 在下面的 `context: { volume: 50 }` 里）。本课你只需要认识"对象类型" `{ volume: number }` 这部分。

## 8 常见错误

**错误 1：把"对象类型"和"对象值"搞混。**

```ts
// ❌ 这是一个值（数据），不是类型
const shape = { volume: 50 };

// ✅ 这才是类型（描述形状，不是具体数据）
type Shape = { volume: number };
```
类型描述"长什么样"，值是"具体是什么"。刚接触 TS 时很容易在读代码时把两者弄混，一个简单的判断技巧：出现在 `type X = ...` 右边、或者变量声明的 `:` 后面的，是类型；出现在 `=` 右边（且不在 `type` 语句里）的，通常是值。

**错误 2：字段类型写成了具体的值，而不是类型。**

```ts
// ❌ 50 是一个值，不是类型；这样写会被理解成"字面量类型"（第 20 课），
// 意味着 volume 永远只能是 50 这一个数字，通常不是你想要的
type Shape = { volume: 50 };

// ✅ 大多数情况下你想要的是"任意 number"
type Shape = { volume: number };
```

**错误 3：忘记对象类型里字段的顺序不重要，误以为要和实际赋值顺序完全一致。**

```ts
type Shape = { volume: number; muted: boolean };
// 赋值时顺序可以不同，TS 只关心字段是否齐全、类型是否匹配
const value: Shape = { muted: false, volume: 50 }; // ✅ 完全合法
```

## 9 为什么官方这样设计

TypeScript 的对象类型系统是"结构化类型"（structural typing）：只要一个值的形状（有哪些字段、字段是什么类型）匹配，就认为它属于这个类型，不需要显式声明"这个对象是 Shape 类型的实例"（不像某些语言需要 `implements` 某个接口）。这种设计非常适合 XState 的使用场景——你写的 Context 初始值、事件对象，本身就是普通的 JS 对象字面量，TypeScript 只需要在幕后检查"形状对不对"，不需要你额外做任何"声明这是哪个类型"的动作，写起来几乎和纯 JS 一样自然，只是多了编译期的形状校验。

## 10 一句话总结

**对象类型用花括号描述"这个对象应该有哪些字段、每个字段是什么类型"，是描述 Context、Event 等数据形状的基本工具。**

## 11 设计思考（Design Thinking）

**需求**：给播放器的 Context 声明类型，包含音量和当前歌曲。

**方案 A（推荐）**：用一个清晰、扁平的对象类型，字段名和业务含义一一对应。

```ts
type PlayerContext = {
  volume: number;
  currentSong: string;
};
```

**方案 B（错误）**：为了"看起来更灵活"，把具体字段都塞进一个大而化之的通用对象类型里。

```ts
// ❌ 完全放弃了类型检查的意义，等于没有类型
type PlayerContext = Record<string, any>;
```

方案 B 虽然语法上也能"跑起来"，但它放弃了 TypeScript 能提供的最大价值——编译期发现拼写错误、字段类型错误。用 `Record<string, any>` 意味着 `context.vloume`（拼错的字段名）和 `context.volume` 在类型层面被同等对待，TypeScript 完全帮不上忙。方案 A 用具体、明确的字段声明，才能真正享受到"写错字段名会立刻在编辑器里标红"这种本课要追求的价值。

## 12 设计原则（Design Rules）

本课新增：

- **对象类型用来描述数据形状（Context、Event 等），是 XState 类型标注的基础构件。**
- **类型描述"长什么样"，值描述"具体是什么"，不要混淆两者出现的位置。**
- **不要为了图省事用 `Record<string, any>` 之类放弃类型检查的写法，那会让 TypeScript 的价值归零。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[19 Union——一个类型可以是好几种可能之一](19-union.md)
