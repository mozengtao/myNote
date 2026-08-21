# 11 Nested State：为什么要拆层

> 上一课：[10 Initial](10-initial.md) · 下一课：[12 Parallel State](12-parallel-state.md)

这是第三部分的重点课，务必吃透。

## 1 为什么需要

交通灯的红灯，实际业务里往往不是一个简单的"红灯亮着"，而是包含好几个子阶段：红灯常亮等待（`waiting`）、红灯期间允许行人通过（`pedestrian`）、红灯快结束前的闪烁提醒（`flash`）。如果把这些子阶段都摊平成和 `red`/`green`/`yellow` 同一层级的状态，会出现两个问题：

1. 图会变得又宽又平，看不出"`waiting`/`pedestrian`/`flash` 都从属于红灯这个大阶段"这层语义关系。
2. 一些逻辑（比如"不管红灯内部在哪个子阶段，收到 `EMERGENCY` 都要立刻跳到黄灯"）需要在三个子状态里各写一遍，非常啰嗦。

现实世界的很多状态天然就是"有层次"的：先分大类，大类里再分小类。状态图需要一种方式表达这种层次关系，这就是 Nested State（嵌套状态）。

## 2 核心思想

**Nested State 是把一组有共同归属、共享部分行为的子状态，收纳进一个父状态里，形成"大状态套小状态"的层次结构。**

## 3 ASCII 图

```
+--------------------------------------------+
| red (父状态)                                |
|                                              |
|   +----------+   after 2s   +------------+  |
|   | waiting  | -----------> | pedestrian |  |
|   +----------+              +------------+  |
|                                    |         |
|                              after 1s        |
|                                    v         |
|                              +--------+      |
|                              | flash  |      |
|                              +--------+      |
+--------------------------------------------+
        |
        | after 总时长结束，或收到 EMERGENCY（红灯内部任何子状态都能响应）
        v
   +--------+
   | green  |
   +--------+
```

`waiting` → `pedestrian` → `flash` 是 `red` 内部的子状态转移；`red` 整体到 `green` 是父状态之间的转移。两层转移互不干扰，但父状态上定义的行为（比如 `EMERGENCY` 事件）对所有子状态自动生效。

## 4 JS 思维

```js
// 用两层字符串表示"父状态.子状态"，模拟嵌套关系
let current = { parent: 'red', child: 'waiting' };

function send(event) {
  if (current.parent === 'red') {
    // 父状态统一响应的事件，不管子状态是什么
    if (event.type === 'EMERGENCY') {
      current = { parent: 'green', child: null };
      return;
    }
    // 子状态各自的转移
    if (current.child === 'waiting' && event.type === 'PEDESTRIAN_TIMER') {
      current.child = 'pedestrian';
    } else if (current.child === 'pedestrian' && event.type === 'FLASH_TIMER') {
      current.child = 'flash';
    }
  }
}
```

关键点：`EMERGENCY` 的判断只写了一次，挂在"父状态"这一层，任何子状态下都能生效——这正是拆层带来的好处：**共享行为只需要声明一次，而不用在每个子状态里重复。**

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const trafficLightMachine = setup({}).createMachine({
  id: 'trafficLight',
  initial: 'red',
  states: {
    red: {
      initial: 'waiting', // 子层也需要自己的 initial（第 10 课的规则递归成立）
      on: {
        EMERGENCY: { target: 'green' }, // 挂在父状态上，waiting/pedestrian/flash 都能响应
      },
      states: {
        waiting: {
          after: { 2000: { target: 'pedestrian' } },
        },
        pedestrian: {
          after: { 1000: { target: 'flash' } },
        },
        flash: {
          after: { 1000: { target: '#trafficLight.green' } }, // 用绝对路径跳出父状态
        },
      },
    },
    green: {
      after: { 3000: { target: 'yellow' } },
    },
    yellow: {
      after: { 1000: { target: 'red' } },
    },
  },
});

const light = createActor(trafficLightMachine).start();
light.subscribe((snapshot) => {
  console.log(snapshot.value);
  // 可能打印: { red: 'waiting' } -> { red: 'pedestrian' } -> { red: 'flash' } -> 'green' -> ...
});
```

## 6 TypeScript 语法拆解

```ts
red: {
  initial: 'waiting',
  states: { waiting: {...}, pedestrian: {...}, flash: {...} },
},
```
一个状态节点里如果自己又有 `states` 字段，它就变成了"复合状态"（父状态），必须像顶层一样再指定一个 `initial`，表示"进入 `red` 之后，默认先进入哪个子状态"。

```ts
on: {
  EMERGENCY: { target: 'green' },
},
```
写在父状态 `red` 这一层的 `on`，会对**所有子状态**生效——无论当前是 `waiting`、`pedestrian` 还是 `flash`，收到 `EMERGENCY` 都会响应。这是嵌套状态最重要的能力：**行为可以在合适的层级声明一次，自动覆盖所有下级。**

```ts
after: { 1000: { target: '#trafficLight.green' } },
```
这里的 `target` 用的是**绝对路径**：`#trafficLight` 是整台机器的 `id`（第 1 课提到过 `id` 字段以后会用来拼路径，这里就是那个用途），`.green` 表示"这台机器顶层的 `green` 状态"。绝对路径用 `#机器id.状态路径` 的形式，可以从图里任何一个深度跳到任何一个已知位置。

对比同一段代码如果写 `target: 'pedestrian'`（相对路径）：相对路径是相对于**当前所在的父状态**解析的，`flash` 和 `pedestrian` 同属 `red` 的子状态，所以在 `flash` 内部写 `target: 'pedestrian'` 会被理解为跳到 `red.pedestrian`。但本课例子要跳到 `green`，`green` 和 `flash` 不在同一个父状态下，必须用带 `#id` 的绝对路径，或者写成 `target: 'green'` 并确保 XState 能正确解析"跳出当前层级"——实践中，**跨父状态跳转优先使用绝对路径 `#id.state`，避免路径解析的歧义**，这是本课要建立的关键区分。

```ts
{ red: 'waiting' }
```
这是嵌套状态下 `snapshot.value` 的形态——不再是一个简单字符串，而是一个"父状态: 子状态"的对象，因为当前状态既要说清楚"处于 red 大类"，又要说清楚"red 内部具体在 waiting 阶段"。这也是第 2 课提到"`matches()` 比直接比较字符串更稳妥"的原因所在。

## 7 常见错误

**错误 1：复合状态忘记写自己的 `initial`。**

```ts
// ❌ red 有子状态却没指定 initial，无法确定进入 red 后该停在哪个子状态
red: {
  states: { waiting: {}, pedestrian: {}, flash: {} },
  // 缺少 initial: 'waiting'
},
```

**错误 2：混淆相对路径和绝对路径，导致跳转到意料之外的状态。**

```ts
// ❌ 在 flash（属于 red 的子状态）里写 target: 'green'，
// 如果 red 内部恰好也有一个叫 green 的子状态，会产生歧义或跳错位置
flash: {
  after: { 1000: { target: 'green' } }, // 应该用 '#trafficLight.green' 更明确
},
```
跨父状态跳转时，优先使用带 `#id` 的绝对路径，消除"这个名字到底指哪一层"的歧义。

**错误 3：为了偷懒，把本该是子状态共享的行为，在每个子状态里重复写一遍，而不是提到父状态层级。**

```ts
// ❌ EMERGENCY 在三个子状态里各写一遍，冗余且容易遗漏
waiting: { on: { EMERGENCY: { target: '#trafficLight.green' } } },
pedestrian: { on: { EMERGENCY: { target: '#trafficLight.green' } } },
flash: { on: { EMERGENCY: { target: '#trafficLight.green' } } },
```
应该把 `EMERGENCY` 提到 `red` 这一层的 `on` 上，只写一次，三个子状态自动继承——这正是拆层的意义所在（呼应第 6 课"绑定在合适的层级，而不是每条路径重复"的思想）。

## 8 为什么官方这样设计

嵌套状态的设计动机，是让状态图能够**匹配现实世界天然的层次结构**，而不是强迫所有状态都摊平成同一个扁平列表。红灯的三个子阶段确实"属于"红灯这个大类，把这种从属关系显式表达出来，有两个直接好处：一是共享行为（比如 `EMERGENCY`）只需要在父层声明一次；二是阅读代码/图的人可以"分层理解"——先理解顶层三个大状态（红黄绿），需要时再展开某个状态看细节，而不必一次性面对一个几十个状态摊平在一起的巨大列表。这是"分而治之"思想在状态建模上的直接体现。

## 9 最佳实践

1. 当你发现好几个状态"共享同一组行为"或者"总是一起进出"时，考虑把它们收进一个父状态。
2. 复合状态必须写 `initial`，这个规则每一层都适用，没有例外。
3. 跨父状态跳转优先用绝对路径 `#机器id.目标状态`，只有确定在同一父状态内部跳转时才用简单的相对路径。
4. 不要为了"看起来有层次"而过度嵌套——只有真正存在共享行为或天然从属关系时才拆层，拆层本身不是目的。
5. 读取嵌套状态机的当前状态时，用 `matches('red.waiting')` 或 `matches({ red: 'waiting' })` 这类语义化方法，而不是手动解构 `snapshot.value` 对象。

## 10 一句话总结

**Nested State 把有共同归属、共享行为的子状态收纳进父状态，让共享逻辑只声明一次，也让复杂状态图能分层阅读。**

## 11 设计思考（Design Thinking）

**需求**：红灯内部有 `waiting`/`pedestrian`/`flash` 三个子阶段，且无论在哪个子阶段，遇到"紧急车辆"都要立刻变绿。

**方案 A（推荐）**：用嵌套状态，`EMERGENCY` 挂在父状态 `red` 上。

```ts
red: {
  initial: 'waiting',
  on: { EMERGENCY: { target: 'green' } },
  states: { waiting: {}, pedestrian: {}, flash: {} },
}
```

**方案 B（错误）**：不用嵌套，把 `waiting`/`pedestrian`/`flash` 和 `green`/`yellow` 全部摊平成同一层的 6 个状态，靠状态名前缀（比如 `redWaiting`、`redPedestrian`）表达"归属关系"。

```ts
// ❌ 状态名靠字符串前缀表达层次，机器完全不知道这些状态"共享归属"
states: {
  redWaiting: { on: { EMERGENCY: { target: 'green' }, PEDESTRIAN_TIMER: { target: 'redPedestrian' } } },
  redPedestrian: { on: { EMERGENCY: { target: 'green' }, FLASH_TIMER: { target: 'redFlash' } } },
  redFlash: { on: { EMERGENCY: { target: 'green' } } },
  green: {},
  yellow: {},
}
```

方案 B 表面上也能工作，但 `EMERGENCY` 的处理逻辑被复制了三份，如果以后要给"红灯大类"新增一条共享规则（比如"红灯期间收到 `MAINTENANCE` 都要跳到维护模式"），需要同时改三处，容易漏改导致不一致。而且状态名的"归属关系"完全依赖命名约定（`red` 前缀），机器和类型系统都无法验证这种关系是否成立。方案 A 用真正的嵌套结构表达归属，共享行为写一次自动生效，而且 TypeScript 和可视化工具都能理解这层真实的父子关系。

## 12 设计原则（Design Rules）

本课新增：

- **发现多个状态共享行为或天然从属于同一大类时，用 Nested State 表达，而不是靠命名前缀模拟层次。**
- **复合状态必须有自己的 `initial`，这条规则递归成立，每一层都不例外。**
- **跨父状态跳转优先用绝对路径 `#机器id.状态路径`，避免相对路径在深层嵌套下产生歧义。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[12 Parallel State——什么时候需要并行](12-parallel-state.md)
