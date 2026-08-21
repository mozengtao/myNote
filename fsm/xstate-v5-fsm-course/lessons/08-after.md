# 08 after：定时器也是 Event

> 上一课：[07 Guard](07-guard.md) · 下一课：[09 always](09-always.md)

## 1 为什么需要

交通灯的红灯不需要任何"用户操作"就会在几十秒后自动变绿——这是一种**由时间驱动而不是由外部输入驱动**的转移。如果没有专门的支持，你只能自己写 `setTimeout`，在回调里手动 `send` 一个事件，还要自己操心"如果状态提前变了要不要清掉这个定时器"（忘记清理是异步代码里最常见的 bug 之一）。

## 2 核心思想

**`after` 本质上是"到时间自动触发的事件"，只是这个事件不需要外部输入，而是状态机自己内置的定时器帮你发送。**

## 3 ASCII 图

交通灯：

```
+-------+  after 3000ms  +--------+  after 2000ms  +--------+
|  red  | --------------> | green | --------------> | yellow |
+-------+                 +--------+                 +--------+
    ^                                                      |
    |                    after 1000ms                      |
    +------------------------------------------------------+
```

每个状态一进入，就自动"排了个队"：过了指定时间还没被打断（没有转移到别的状态），就自动跳到下一个。

## 4 JS 思维

```js
let currentState = 'red';
let timer = null;

function enterState(state) {
  currentState = state;
  clearTimeout(timer); // 先清掉上一个状态遗留的定时器，避免重复触发

  if (state === 'red') {
    timer = setTimeout(() => enterState('green'), 3000);
  } else if (state === 'green') {
    timer = setTimeout(() => enterState('yellow'), 2000);
  } else if (state === 'yellow') {
    timer = setTimeout(() => enterState('red'), 1000);
  }
}

enterState('red'); // 自动开始整个循环
```

注意 `clearTimeout(timer)` 这行——如果状态机因为别的事件提前离开了当前状态，必须记得清掉还没触发的定时器，否则旧定时器触发时会把状态改到一个"过时的"目标，产生很难排查的 bug。这正是 `after` 存在的意义之一：**这件容易忘记、容易出错的清理工作，XState 帮你自动做了。**

## 5 XState v5 写法

```ts
import { setup, createActor } from 'xstate';

const trafficLightMachine = setup({}).createMachine({
  id: 'trafficLight',
  initial: 'red',
  states: {
    red: {
      after: {
        3000: { target: 'green' },
      },
    },
    green: {
      after: {
        2000: { target: 'yellow' },
      },
    },
    yellow: {
      after: {
        1000: { target: 'red' },
      },
    },
  },
});

const light = createActor(trafficLightMachine).start();

light.subscribe((snapshot) => {
  console.log('当前灯色:', snapshot.value);
});
// 3 秒后自动打印 'green'，再过 2 秒自动打印 'yellow'，如此循环
```

如果红灯期间被外部事件打断（比如加一条 `EMERGENCY` 事件直接跳到某个特殊状态），XState 会自动帮你取消掉那个还没触发的 3000ms 定时器，不需要你手写 `clearTimeout`。

## 6 TypeScript 语法拆解

```ts
after: {
  3000: { target: 'green' },
},
```
`after` 是状态节点上的一个字段，形状类似 `on`，只是 key 从"事件名字符串"换成了"毫秒数"。`3000` 表示"进入这个状态 3000 毫秒后，如果还没有因为别的原因离开，就自动转移到 `target`"。

可以把 `after` 理解成"XState 内部自动生成了一个定时事件，进入状态时自动发送定时器，时间到了自动 `send` 这个内置事件，触发对应转移"——概念上跟 `on` 完全一样，只是触发源从"外部调用 `send`"变成了"内部计时器"。这也是本课标题"`after` 本质也是 Event"的含义。

## 7 常见错误

**错误 1：以为 `after` 的计时器不会被清除，会一直计时直到触发。**

实际上，只要状态机因为任何原因离开了这个状态（哪怕只是短暂经过），这个 `after` 定时器就会被自动取消。如果你依赖"进入过这个状态就一定会在 N 毫秒后触发"，需要确认期间没有发生别的转移把它提前带走。

**错误 2：把需要根据业务数据动态计算的延迟时间写死成字面量数字。**

```ts
// 需要动态延迟时，不能直接写死数字，要用函数返回毫秒数
after: {
  someDelay: { target: 'green' }, // 具体动态延迟的写法请查阅官方文档的 delays 配置
}
```
固定延迟直接写数字没问题；如果延迟时间依赖 Context（比如"根据交通流量动态调整红灯时长"），需要用 `setup({ delays: {...} })` 注册一个返回毫秒数的函数，而不是写死常量——这部分属于进阶用法，本课先建立"到时间自动转移"这个核心概念。

**错误 3：滥用 `after` 去实现"轮询"或"长时间等待外部结果"的逻辑。**

```ts
// ❌ 用 after 不断重试来"等待"一个异步结果，是在滥用定时器语义
checking: {
  after: {
    1000: { target: 'checking' }, // 每秒钟"转移"到自己，其实是在假装轮询
  },
},
```
如果本质是"等待一个异步操作完成"，应该用第四部分的 Actor（`invoke` + `fromPromise`），而不是用 `after` 硬凑一个轮询循环。`after` 更适合"确定的、单纯的超时/定时"场景。

## 8 为什么官方这样设计

把"定时触发的转移"设计成和 `on` 结构一致的 `after`，而不是让你自己写 `setTimeout`，本质上是在贯彻"所有转移都应该在配置里显式可见"这条从第 4 课就开始强调的原则。定时器的自动创建和自动清理，是 XState 在幕后帮你处理的"生命周期管理"——你不需要自己记得在 `exit` 里手写 `clearTimeout`，因为状态机知道"我离开这个状态了，跟这个状态绑定的定时器自然要失效"。这也再次体现了 Action/Transition 都是绑定在状态节点生命周期上的思想（呼应第 6 课）。

## 9 最佳实践

1. `after` 适合"确定时长的超时/定时"场景（红绿灯切换、请求超时、消息自动消失），不适合模拟轮询或长时间异步等待。
2. 延迟时间如果和业务无关、是固定常量，直接写数字；如果依赖 Context 动态计算，查阅官方文档的 `delays` 配置项。
3. 不需要手动管理定时器的创建和清除——这是 `after` 的核心价值，不要在 Action 里自己又叠加一层 `setTimeout`。
4. 给交通灯这类"自动循环"的状态机画图时，记得把每个状态的 `after` 时长标在箭头旁边，和事件驱动的转移一起标注，图才完整。
5. 测试涉及 `after` 的逻辑时，善用测试框架提供的"快进时钟"能力（比如 fake timers），不要在测试里真的等待几秒钟。

## 10 一句话总结

**`after` 是内置的定时事件：到时间自动触发转移，且状态一旦被提前离开，定时器会被自动清理。**

## 11 设计思考（Design Thinking）

**需求**：红灯持续 3 秒后自动变绿，但如果期间有"紧急车辆通过"事件，要立刻跳过红灯变绿，并且不能让原来的 3 秒定时器再误触发一次。

**方案 A（推荐）**：紧急事件作为普通 `on` 转移，`after` 交给 XState 自动管理清理。

```ts
red: {
  after: { 3000: { target: 'green' } },
  on: { EMERGENCY: { target: 'green' } },
}
```

**方案 B（错误）**：自己手写 `setTimeout` + 全局变量记录定时器 id，在紧急事件处理函数里手动清除。

```js
// ❌ 手动管理定时器，容易漏清理导致"迟到"的定时器触发时把状态改错
let redTimer = setTimeout(() => enterState('green'), 3000);
function onEmergency() {
  clearTimeout(redTimer); // 必须记得写这一行，容易忘
  enterState('green');
}
```

方案 B 把"清理定时器"这件事的责任完全交给了程序员的记忆力——项目稍微复杂一点、转移路径一多，几乎必然会漏掉某个该清理的地方，产生"状态已经变了，但几秒后突然又被旧定时器改回去"这种诡异 bug。方案 A 让 `EMERGENCY` 转移和 `after` 定时器都由 XState 统一管理生命周期：一旦 `red` 状态因为任何原因被离开（无论是 `after` 触发还是 `EMERGENCY` 触发），另一条路径涉及的定时器/监听都会被自动清理，不需要你写任何额外的清理代码。

## 12 设计原则（Design Rules）

本课新增：

- **`after` 本质是内置的定时 Event，和 `on` 结构一致，只是触发源是计时器而不是外部输入。**
- **状态一旦被离开，其 `after` 定时器会被自动清理，不需要手动管理生命周期。**
- **`after` 适合确定时长的超时/定时场景，不适合模拟轮询或替代真正的异步等待（那是 Actor 的职责，见第四部分）。**

完整清单见 [DESIGN-RULES.md](../DESIGN-RULES.md)。

---

下一课：[09 always——什么时候自动跳转](09-always.md)
