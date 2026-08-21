# 06 Action：副作用什么时候执行

> 上一课：[05 Context](05-context.md) · 下一课：[07 Guard](07-guard.md)

## 1 为什么需要

前五课讲的都是"状态机内部的事"：State 是什么、怎么变、数据放哪。但真实系统永远需要"对外面的世界做点什么"——开门时点亮一盏灯、播放器切歌时打一条日志、连接建立时通知服务器。这些"对外部世界产生影响"的动作，就是 Action（副作用）。

如果没有一个专门的地方放这些副作用，代码会自然而然地把它们和状态判断逻辑搅在一起，变成"改一次状态、顺手做十件事"的意大利面条。

## 2 核心思想

**Action 是状态机在"进入某状态""离开某状态""响应某事件"这三个时机点上执行的副作用，本身不影响状态机往哪个方向变化。**

## 3 ASCII 图

```
        entry: turnOnLight        exit: turnOffLight
              |                         |
              v                         v
+--------+  OPEN   +--------+  CLOSE  +--------+
| closed | ------> | opened | ------> | closed |
+--------+         +--------+         +--------+
                        ^
                        |
              transition action: logOpenEvent
                 (写在 OPEN 这条 Transition 上)
```

## 4 JS 思维

```js
let currentState = 'closed';

function send(event) {
  if (currentState === 'closed' && event.type === 'OPEN') {
    console.log('log: door is opening'); // transition action：转移过程中执行
    currentState = 'opened';
    turnOnLight(); // entry：进入 opened 之后执行
  } else if (currentState === 'opened' && event.type === 'CLOSE') {
    turnOffLight(); // exit：离开 opened 之前执行
    currentState = 'closed';
  }
}
```

三种时机在 JS 版本里对应三个不同的插入点：**转移开始时**（transition action）、**进入新状态后**（entry）、**离开旧状态前**（exit）。XState 只是把这三个插入点变成了配置里的固定字段，不用你手写 if/else 去插入。

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const doorMachine = setup({
  actions: {
    turnOnLight: () => console.log('💡 灯亮了'),
    turnOffLight: () => console.log('💡 灯灭了'),
    logOpenEvent: () => console.log('📝 记录：有人开门'),
  },
}).createMachine({
  id: 'door',
  initial: 'closed',
  states: {
    closed: {
      on: {
        OPEN: {
          target: 'opened',
          actions: 'logOpenEvent', // transition action：转移时执行一次
        },
      },
    },
    opened: {
      entry: 'turnOnLight',  // 进入 opened 时执行一次
      exit: 'turnOffLight',  // 离开 opened 时执行一次
      on: {
        CLOSE: { target: 'closed' },
      },
    },
  },
});

const door = createActor(doorMachine).start();
door.send({ type: 'OPEN' });
// 打印顺序：
// 📝 记录：有人开门   <- transition action，转移过程中
// 💡 灯亮了           <- entry，进入 opened 之后
door.send({ type: 'CLOSE' });
// 💡 灯灭了           <- exit，离开 opened 之前
```

## 6 TypeScript 语法拆解

```ts
actions: {
  turnOnLight: () => console.log('💡 灯亮了'),
  ...
},
```
在 `setup({ actions: {...} })` 里预先注册所有会用到的 Action 函数，key 是这个 Action 的名字（字符串），value 是真正执行的函数。这一步做完，后面在 `entry`/`exit`/`actions` 字段里就可以直接用**字符串**引用它们（`'turnOnLight'`），而不用每次都写一遍函数体。

```ts
entry: 'turnOnLight',
```
`entry` 是状态节点上的一个字段，值是 Action 的名字（或名字数组）。**进入这个状态时**执行。

```ts
exit: 'turnOffLight',
```
和 `entry` 对称，**离开这个状态时**执行。

```ts
OPEN: {
  target: 'opened',
  actions: 'logOpenEvent',
},
```
这是"挂在 Transition 上的 Action"，和 `entry`/`exit` 的区别是：它绑定的是**这条具体的转移**，而不是某个状态。如果同一个目标状态可以通过多条不同的转移到达，各自可以执行不同的 transition action，但 `entry` 无论从哪条路径进来都会执行。

## 7 常见错误

**错误 1：分不清 entry/exit 和 transition action 的触发条件，导致重复执行或遗漏。**

如果你想要"不管从哪条路径进入这个状态，都执行某个动作"，应该用 `entry`；如果你想要"只有走这一条特定转移才执行"，才用 transition action。把该用 `entry` 的逻辑写成了每条转移各写一份 `actions`，很容易漏掉某条路径。

**错误 2：把应该用 Guard（下一课）判断的逻辑，写进 Action 里用 if 分流。**

```ts
// ❌ Action 里做业务判断，掺杂了"要不要做"和"怎么做"
actions: {
  maybeOpen: ({ context }) => {
    if (context.balance >= 100) {
      // 强行在 action 里决定分支，但 action 本身不该决定"要不要转移"
    }
  },
},
```
Action 只负责"做事"，不负责"决定要不要变状态"——那是 Guard 的职责，混在一起会让转移逻辑很难通过看配置就看懂。

**错误 3：在 Action 里做了不该做的事——比如直接修改 Context。**

```ts
// ❌ 更新 Context 必须用 assign（第 5 课），不要在普通 action 里手改
actions: {
  badAction: ({ context }) => {
    context.volume = 100; // 错误：绕过了 assign
  },
},
```

## 8 为什么官方这样设计

把副作用拆成 entry/exit/transition action 三种，是为了让"进入某个模式时该做什么"这件事**和这个模式本身绑定在一起，而不是散落在所有能到达它的转移里**。举个例子：如果"进入 opened 状态就要开灯"这条规则只写在 `entry` 上，无论未来新增多少条能到达 `opened` 的路径（比如遥控器开门、指纹开门），灯都会自动亮起，不需要在每条新转移里都记得加一遍"开灯"的代码。这是**把副作用和状态绑定，而不是和转移路径绑定**的设计哲学，减少了"新增一条路径却忘记加副作用"这类 bug。

## 9 最佳实践

1. "不管怎么进来都要做的事"用 `entry`，"只有这条特定转移才要做的事"用 transition action，先想清楚归属。
2. Action 函数应该是"纯粹的副作用执行者"，不要在里面做状态是否该转移的判断——那是 Guard 的事。
3. 更新 Context 永远用 `assign`（第 5 课），普通 Action 只处理"真正对外部世界产生影响"的事（打日志、发通知、操作 DOM）。
4. 给 Action 起名字用动词短语（`turnOnLight`、`logOpenEvent`），一看名字就知道做了什么。
5. 复杂的副作用逻辑（比如异步请求）不要直接塞进 Action——那属于第四部分 Actor 的范畴，Action 应该保持简单、同步、可预测。

## 10 一句话总结

**Action 是"进入/离开某状态"或"响应某转移"时执行的副作用，只负责做事，不负责判断该不该做。**

## 11 设计思考（Design Thinking）

**需求**：门打开时点亮灯，关闭时熄灯；同时不管走哪条门被打开的路径，都要记一条日志。

**方案 A（推荐）**：`entry`/`exit` 绑定在状态上处理灯光，`entry` 里顺带处理日志（因为"记日志"这件事其实也是"不管怎么进来都要做"，属于 entry 语义）。

```ts
opened: {
  entry: ['turnOnLight', 'logOpenEvent'],
  exit: 'turnOffLight',
}
```

**方案 B（错误）**：在每一条能到达 `opened` 的 Transition 上都重复写一遍"开灯 + 记日志"。

```ts
// ❌ 如果以后新增"遥控器开门"这条转移，容易忘记也加上 turnOnLight
states: {
  closed: { on: { OPEN: { target: 'opened', actions: ['turnOnLight', 'logOpenEvent'] } } },
  locked: { on: { UNLOCK_AND_OPEN: { target: 'opened', actions: ['turnOnLight', 'logOpenEvent'] } } },
}
```

方案 B 把本该属于"状态"的副作用，错误地绑定到了"转移"上，导致同一段逻辑要在每条路径上重复一遍，未来新增路径时极易遗漏。方案 A 把"进入 opened 就要做的事"一次性写在 `entry` 里，无论未来加多少条新路径通向 `opened`，这条规则自动生效，不需要每次都记得同步修改。

## 12 设计原则（Design Rules）

本课新增：

- **Action 表示副作用（Effect）：只负责"做事"，不负责"判断该不该变化"（那是 Guard，见下一课）。**
- **绑定在状态上用 `entry`/`exit`（不管走哪条路径都生效），绑定在具体转移上用 transition action（只有这条路径才生效）。优先考虑用 `entry`/`exit`，除非确实需要区分路径。**
- **更新 Context 永远走 `assign`，普通 Action 不直接改数据。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[07 Guard——为什么不能用 if](07-guard.md)
