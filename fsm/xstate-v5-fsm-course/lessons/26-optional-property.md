# 26 Optional Property：什么时候一个字段可以不存在

> 上一课：[25 Generic](25-generic.md) · 这是全课程最后一课

## 1 为什么需要

第 19 课强调过："事件的不同分支该有不同的字段，不要用一个'万能对象+可选字段'糊弄。" 但这不代表"可选字段"这个语法本身没有用武之地——有些场景，一个字段确实"可能有、也可能没有"，比如"用户提交表单时，备注是可选的"。需要一种方式精确表达"这个字段允许缺席"，同时又不滥用它去掩盖本该用 Union 精确表达的场景。这就是本课要讲清楚的边界。

## 2 核心思想

**可选属性（`字段名?: 类型`）表示"这个字段可能存在、也可能是 `undefined`"，适合真正意义上"有没有都合理"的数据，而不是用来敷衍"其实应该拆成 Union 的不同事件形状"。**

## 3 ASCII 图

```
必需字段:  { type: 'SUBMIT'; note: string }
              -> note 必须存在，缺了就是类型错误

可选字段:  { type: 'SUBMIT'; note?: string }
              -> note 可能存在（string），也可能不存在（undefined）
              两种情况都合法
```

## 4 JS 写法

```js
// 纯 JS：字段本来就可以随意缺省，没有强制约束
function submit(form) {
  console.log(form.note); // 如果没传 note，这里就是 undefined，运行时才知道
}

submit({ note: '备注内容' });
submit({}); // 完全合法，note 缺省
```

## 5 TS 写法

```ts
type SubmitForm = {
  content: string;   // 必需字段
  note?: string;     // 可选字段：加一个问号
};

function submit(form: SubmitForm) {
  if (form.note) {
    console.log('备注:', form.note); // 这里 form.note 被收窄成 string（排除了 undefined）
  } else {
    console.log('没有备注');
  }
}

submit({ content: 'hello' }); // ✅ 合法，note 缺省
submit({ content: 'hello', note: '重要' }); // ✅ 合法，note 提供了
// submit({ note: 'x' }); // ❌ 编译错误：缺少必需字段 content
```

`form.note?: string` 和"`note: string | undefined`"在大多数场景下效果相似，但有一个关键区别：`note?: string` 允许你**完全不写这个字段**（对象字面量里可以没有 `note` 这个 key），而 `note: string | undefined` 要求这个字段**必须存在**，只是值可以是 `undefined`——这个细节差异在严格模式下会体现出来，日常使用时，需要"可以整个不提供"的场景用 `?`。

## 6 XState 中如何使用

```ts
import { setup, createActor } from 'xstate';

const orderMachine = setup({
  types: {
    context: {} as {
      amount: number;
      note?: string; // 备注是可选信息，不影响任何状态判断
    },
    events: {} as
      | { type: 'SUBMIT'; note?: string } // 提交时备注可填可不填
      | { type: 'CANCEL' },
  },
}).createMachine({
  id: 'order',
  initial: 'draft',
  context: { amount: 0 },
  states: {
    draft: {
      on: {
        SUBMIT: {
          target: 'submitted',
          actions: ({ event }) => {
            console.log('订单备注:', event.note ?? '（无备注）');
          },
        },
      },
    },
    submitted: {},
  },
});

const order = createActor(orderMachine).start();
order.send({ type: 'SUBMIT' }); // ✅ 不带 note 也合法
order.send({ type: 'SUBMIT', note: '尽快发货' }); // ✅ 带 note 也合法
```

回顾第 5 课埋下的伏笔——早期课程为了让例子聚焦在核心概念上，Context 都是"字段齐全、没有可选属性"的形式；到这里终于可以完整解释：**只有在字段本身确实"有没有都合理、不影响状态判断"时，才应该用可选属性；如果这个字段的有无实际上意味着"这其实是两种不同的事件/状态"，应该拆成 Union（第 19 课），而不是简单标个问号了事。**

## 7 TypeScript 语法拆解

```ts
note?: string;
```
字段名后面紧跟一个问号 `?`，再跟冒号和类型。这个问号只能紧跟在字段名后面，不能写在类型前面（`note: ?string` 是错误语法）。

```ts
event.note ?? '（无备注）'
```
`??` 是"空值合并运算符"（nullish coalescing），表示"如果左边是 `null` 或 `undefined`，就用右边的值代替"。这个语法经常和可选属性搭配出现，用来"给缺省的可选字段提供一个默认展示值"。

```ts
if (form.note) { console.log(form.note); }
```
在 `if` 判断为真的分支里，TypeScript 会把 `form.note` 的类型从 `string | undefined` 收窄为 `string`（因为 `if` 条件排除了 `undefined`/空字符串等假值）——这和第 19 课"根据 `event.type` 收窄 Union"是同一种"类型收窄"能力，只是这里收窄的对象是"可选字段是否存在"，而不是"Union 的哪个分支"。

## 8 常见错误

**错误 1：滥用可选属性去表达"其实是不同事件形状"的场景（回顾第 19 课强调过的反例）。**

```ts
// ❌ SET_VOLUME 和 PLAY 应该拆成 Union 的不同分支，而不是共用一个"万能事件+可选字段"
events: {} as { type: 'PLAY' | 'SET_VOLUME'; level?: number },
```
这样写的问题是：TypeScript 无法保证"只有 `SET_VOLUME` 才会带 `level`"，`{ type: 'PLAY', level: 999 }` 在这种写法下是合法的，但业务上毫无意义——应该用 Union 精确区分（第 19 课）。

**错误 2：忘记处理"字段确实缺省"的情况，直接当作一定存在来使用。**

```ts
// ❌ 没有先判断 note 是否存在，就直接把它当 string 使用（TS 会在这里报错提醒你）
function badSubmit(form: SubmitForm) {
  console.log(form.note.length); // 编译错误：note 可能是 undefined，不能直接调用 .length
}
```
可选字段必须先判断（或用 `??` 提供默认值），才能安全使用——这正是 TypeScript 在这里发挥价值的地方：强迫你考虑"没有这个字段"的情况，而不是等运行时崩溃才发现。

**错误 3：把状态机 Context 里"其实应该有初始默认值"的字段标成可选，而不是干脆给一个默认值。**

```ts
// 不太推荐：如果 volume 总是应该有初始值，标成可选反而让所有用到它的地方都要多写一次判断
context: {} as { volume?: number },
context: { }, // 初始值里也没给 volume

// 更好的做法：直接给一个默认值，字段变成必需
context: {} as { volume: number },
context: { volume: 50 },
```
只有"这个信息本来就可能长期不存在"（比如用户还没填写的备注）才适合用可选属性；如果只是"暂时忘了给初始值"，应该直接补上默认值，让字段保持必需，减少后续所有引用处都要判断"存不存在"的负担。

## 9 为什么官方这样设计

TypeScript 的可选属性设计成"字段名后面加问号"这种轻量语法，而不是要求你每次都显式写 `string | undefined`，是为了让"允许缺省"这个很常见的需求表达起来足够简洁。但它的存在不应该被滥用成"偷懒地把所有不确定的字段都标成可选，绕开真正应该做的类型区分工作"——第 19 课已经详细论证过，"多种确实不同的形状"应该用 Union 精确表达；可选属性只该留给"同一种形状下，某个字段确实可能缺省"这种更窄、更真实的场景。这也是本课程"FSM 思维 > API > TypeScript 语法"这条主线在类型系统内部的延伸：**先想清楚这个字段的业务语义到底是什么，再选择用哪种 TypeScript 语法表达它，而不是反过来，看哪个语法写着方便就用哪个。**

## 10 一句话总结

**可选属性（`字段?: 类型`）用于表达"同一种形状下，某个字段确实可能缺省"，不要用它来敷衍本该用 Union 精确区分的不同事件/状态形状。**

## 11 设计思考（Design Thinking）

**需求**：订单提交事件，用户可以选填一段备注。

**方案 A（推荐）**：`note` 是同一个 `SUBMIT` 事件里真正意义上"可有可无"的附加信息，用可选属性表达。

```ts
events: {} as { type: 'SUBMIT'; note?: string } | { type: 'CANCEL' },
```

**方案 B（错误）**：把"是否有备注"错误地建模成两种不同的事件类型。

```ts
// ❌ "有没有备注"不改变事件的核心语义（都是"提交"这个动作），
// 拆成两种事件类型反而让调用方需要多做一次没有意义的判断
events: {} as
  | { type: 'SUBMIT_WITH_NOTE'; note: string }
  | { type: 'SUBMIT_WITHOUT_NOTE' }
  | { type: 'CANCEL' },
```

方案 B 混淆了"这个字段该不该用 Union 区分"的判断标准：判断标准是"这个信息的有无，是否代表着完全不同的业务动作或流程走向"。备注有没有，并不改变"用户在提交订单"这个核心事实，也不需要状态机根据"有没有备注"走不同的转移路径——这正是可选属性该登场的场景。反过来，如果确实存在"提交时是否需要审批"这种"有无会导致完全不同后续流程"的字段，那就应该像第 19 课强调的那样拆成不同的事件/状态，而不是标成可选完事。方案 A 准确抓住了"备注"这个字段的真实业务语义（附加信息，不影响流程分支），用最贴切的语法表达它。

## 12 设计原则（Design Rules）

本课新增（也是全课程 TypeScript 部分的收官原则）：

- **可选属性用于"同一形状下确实可能缺省"的字段；如果字段的有无代表不同的业务动作或流程分支，应该拆成 Union，而不是标可选敷衍了事。**
- **使用可选字段前必须处理"缺省"的情况（判断或 `??` 默认值），不能假设它一定存在。**
- **选择 TypeScript 语法之前，先想清楚字段的真实业务语义——这条原则不只适用于本课，是贯穿第六部分所有 TS 知识点的共同底线：语法服务于建模，不是反过来。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

## 恭喜，全部 26 课到此结束

现在请回到 [DESIGN-RULES.md](../DESIGN-RULES.md)，把整个课程积累下来的 FSM 设计原则完整读一遍——那才是你真正应该带走的东西。API 会变，这些原则不会。

如果想复习某个具体知识点，回到 [README.md](../README.md) 的课程目录随时跳转。
