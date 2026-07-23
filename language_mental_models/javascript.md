# JavaScript —— 事件（Event）

> **核心驱动力：程序由事件驱动，而不是顺序执行。**
> JavaScript 程序员不问"这行代码之后立刻执行什么"，而问"什么时候会收到通知，通知回来之后该做什么"。

---

## 心智模型图解

```
Event（发生了什么？点击/定时器/网络返回...）
     │
     ▼
Callback（事件发生后该执行的代码）
     │
     ▼
Event Loop（谁负责在合适的时机取出回调并执行）
```

JavaScript 代码"写在那里"和"什么时候执行"是两件分开的事。
调用 `setTimeout`、`fetch`、`addEventListener` 时，你并没有立刻执行某段逻辑，
而是"注册了一个未来某个事件发生时才会被触发的回调"，真正的执行时机由事件循环决定。

---

## 核心驱动力详解

- **同步代码立刻执行，异步回调等事件发生后才执行**：这是所有 JS 异步行为的地基。
- **事件循环（Event Loop）是调度中心**：它反复检查"当前调用栈是否为空"，为空就从任务队列取出下一个回调执行。
- **微任务（microtask，如 Promise 回调）优先于宏任务（macrotask，如 setTimeout）**：决定了同样是"异步"，执行顺序也可能天差地别。
- **`async`/`await` 不是新的执行模型，只是 Promise 链的语法糖**：让"事件驱动"的代码读起来更像同步代码。

---

## 典型代码片段

### 1. `addEventListener` —— 最直观的"事件→回调"绑定

```javascript
button.addEventListener("click", (event) => {
    console.log("button clicked at", event.clientX, event.clientY);
});

console.log("listener registered"); // 这一行会先打印，点击事件可能几秒后才发生
```

**心智模型解读**：`addEventListener` 不会阻塞代码继续执行，它只是"登记"了一个未来事件的处理器；
`console.log("listener registered")` 会立刻执行，而回调函数要等真实的点击事件发生才会被调用。

### 2. `setTimeout` —— 把代码"投递"到未来某个时间点执行

```javascript
console.log("A");
setTimeout(() => console.log("B"), 0);
console.log("C");
// 输出顺序：A C B，即使延迟是 0ms
```

**心智模型解读**：`setTimeout(fn, 0)` 并不代表"立刻执行"，而是"至少延迟 0ms 之后，
把 `fn` 放进任务队列"。当前这一批同步代码（`console.log("C")`）一定会先跑完，事件循环才会去处理队列里的回调。

### 3. Promise 的 `then`/`catch` 链 —— 把"未来的值"和"处理它的动作"绑定

```javascript
fetch("/api/user/1")
    .then((res) => res.json())
    .then((user) => console.log("got user:", user.name))
    .catch((err) => console.error("request failed:", err));

console.log("fetch initiated");
```

**心智模型解读**：`fetch` 立刻返回一个 Promise（代表"未来会有一个结果"），
`.then` 注册的是"这个结果到达之后该做什么"。`"fetch initiated"` 一定比任何 `.then` 回调先打印。

### 4. `async`/`await` —— 用同步的"外观"写异步事件流

```javascript
async function loadUser(id) {
    console.log("start loading");
    const res = await fetch(`/api/user/${id}`); // 等待事件（网络响应）到达
    const user = await res.json();
    console.log("loaded:", user.name);
    return user;
}

loadUser(1);
console.log("this runs before loadUser finishes");
```

**心智模型解读**：`await` 会暂停当前 `async` 函数的执行，把控制权交还给事件循环，
但不会阻塞整个程序——所以 `"this runs before loadUser finishes"` 通常会比 `"loaded: ..."` 先打印。

### 5. 微任务 vs 宏任务 —— 同样是"异步"，顺序也不同

```javascript
console.log("1: sync");

setTimeout(() => console.log("2: macrotask (setTimeout)"), 0);

Promise.resolve().then(() => console.log("3: microtask (Promise)"));

console.log("4: sync");

// 实际输出顺序：1, 4, 3, 2
```

**心智模型解读**：事件循环在每一轮"宏任务"执行完毕后，会先清空所有微任务队列，
再去取下一个宏任务。Promise 回调属于微任务，比 `setTimeout` 的宏任务回调更早被执行。

### 6. Node.js `EventEmitter` —— 把"发布/订阅"模式做成显式 API

```javascript
const { EventEmitter } = require("events");

const bus = new EventEmitter();

bus.on("order:placed", (order) => {
    console.log("send confirmation email for order", order.id);
});

bus.on("order:placed", (order) => {
    console.log("update inventory for order", order.id);
});

bus.emit("order:placed", { id: 42 }); // 触发事件，两个监听器依次同步执行
```

**心智模型解读**：`EventEmitter` 把"一个事件可以有多个订阅者"这种模式标准化了，
`emit` 时会按注册顺序依次调用所有监听器——这是"事件驱动"思维在服务端代码里的直接体现。

### 7. `Promise.all` —— 等待多个事件都发生后再继续

```javascript
async function loadDashboard() {
    const [user, orders, notifications] = await Promise.all([
        fetch("/api/user").then((r) => r.json()),
        fetch("/api/orders").then((r) => r.json()),
        fetch("/api/notifications").then((r) => r.json()),
    ]);
    console.log(user, orders, notifications);
}
```

**心智模型解读**：三个请求几乎同时发出（并发），`Promise.all` 注册的回调要等"全部完成"这个
复合事件发生后才会触发，而不是像顺序 `await` 一样一个个排队等待。

### 8. Callback Hell vs Promise 链 —— 同一件事，两种"事件串联"写法

```javascript
// 回调地狱：一层套一层，事件的先后关系全靠缩进表达
getUser(1, (user) => {
    getOrders(user.id, (orders) => {
        getOrderDetails(orders[0].id, (detail) => {
            console.log(detail);
        });
    });
});
```

```javascript
// Promise 链：把"下一个事件发生后做什么"平铺成一条链
getUserAsync(1)
    .then((user) => getOrdersAsync(user.id))
    .then((orders) => getOrderDetailsAsync(orders[0].id))
    .then((detail) => console.log(detail))
    .catch((err) => console.error(err));
```

**心智模型解读**：两者本质上都是"一个事件的回调里触发下一个事件"，
但 Promise 把嵌套关系"拍平"成链式调用，并统一了错误处理（一个 `.catch` 能捕获链上任何一环的失败）。

### 9. `fetch` 的取消 —— 用 `AbortController` 提前终止一个"未完成的事件"

```javascript
const controller = new AbortController();

fetch("/api/slow-endpoint", { signal: controller.signal })
    .then((res) => res.json())
    .catch((err) => {
        if (err.name === "AbortError") console.log("request was cancelled");
    });

setTimeout(() => controller.abort(), 3000); // 3 秒后触发"取消"事件
```

**心智模型解读**：请求本身是一个"尚未发生完的事件"，`AbortController` 提供了一种
"从外部触发一个信号，打断这个事件的等待过程"的机制——这也是事件模型的一种延伸应用。

### 10. 自定义发布订阅（Pub/Sub）—— 手写一个最小事件总线

```javascript
class EventBus {
    constructor() {
        this.listeners = new Map();
    }

    on(event, handler) {
        if (!this.listeners.has(event)) this.listeners.set(event, []);
        this.listeners.get(event).push(handler);
    }

    emit(event, payload) {
        (this.listeners.get(event) || []).forEach((handler) => handler(payload));
    }
}

const bus = new EventBus();
bus.on("theme:changed", (theme) => document.body.className = theme);
bus.emit("theme:changed", "dark-mode");
```

**心智模型解读**：这段代码本质上重新实现了一个迷你版的 `EventEmitter`，
说明"事件驱动"不是浏览器/Node.js 特有的黑魔法，而是一种可以用最基础的语言特性（Map + 数组）实现的通用模式。

### 11. 微任务队列的另一个陷阱 —— `async` 函数体的同步部分也会立即执行

```javascript
async function run() {
    console.log("A"); // 同步部分，立即执行
    await null;       // 从这里开始，剩余代码被安排到微任务队列
    console.log("B");
}

console.log("start");
run();
console.log("end");
// 输出顺序：start, A, end, B
```

**心智模型解读**：`async` 函数在遇到第一个 `await` 之前的代码，是完全同步执行的；
只有 `await` 之后的部分，才会变成"等事件（这里是 Promise resolve）发生后执行的回调"。

---

## 黄金法则

> **不要想着顺序执行，而要想着事件驱动。**

看到一段包含 `setTimeout`/`fetch`/`addEventListener`/`Promise`/`async` 的代码，
先画出"哪些是立即执行的同步代码，哪些是注册的事件回调"，再去推理真正的执行顺序。

---

## 常见误区对比

### 误区一：以为异步函数会"阻塞"后面的代码

```javascript
// 错误心智模型：以为 fetchData() 执行完才会往下走
let data;
fetchData().then((res) => { data = res; });
console.log(data); // undefined！这一行在 then 回调之前就执行了
```

```javascript
// JavaScript 习惯写法：把"之后要用到数据"的逻辑也放进事件回调里，或者用 await
async function main() {
    const data = await fetchData(); // 显式等待这个事件发生
    console.log(data); // 正确拿到数据
}
main();
```

**为什么后者更好**：`data` 的赋值发生在"未来某个事件（网络响应）到达"之后，
在事件真正发生之前就去读取 `data`，只能读到初始值——必须把"用数据"的代码也纳入事件驱动的流程里。

### 误区二：在循环里直接用回调，误以为回调会按"直觉顺序"执行

```javascript
// 错误心智模型：以为循环体里的异步操作会按顺序一个个"等完"再继续下一轮
for (let i = 0; i < 3; i++) {
    setTimeout(() => console.log(i), 100); // 常见坑：var 会导致全打印 3，let 才按预期打印 0 1 2
}
```

```javascript
// JavaScript 习惯写法：明确用 async/await 或 Promise 串联，控制真正的顺序
async function runSequentially() {
    for (let i = 0; i < 3; i++) {
        await new Promise((resolve) => setTimeout(resolve, 100));
        console.log(i); // 严格按顺序、间隔 100ms 打印 0 1 2
    }
}
runSequentially();
```

**为什么后者更好**：`setTimeout` 只是"注册未来的回调"，循环体本身会立刻跑完三次注册，
不会等回调执行完再进入下一轮——如果需要"事件之间的严格先后关系"，必须显式用 `await` 表达出来。

---

## 快速上手 Checklist

- [ ] 看到 `setTimeout`/`fetch`/`addEventListener`，能立刻分清"这是同步代码"还是"注册了一个未来的回调"吗？
- [ ] 能说清楚微任务（Promise）和宏任务（setTimeout）谁先执行吗？
- [ ] 写 `async`/`await` 代码时，是否清楚 `await` 之前的代码是同步执行的？
- [ ] 遇到"数据还没到就被使用"的 bug，第一反应是检查"这份数据是不是要等一个事件"吗？
- [ ] 需要"多个异步操作全部完成才继续"时，会想到用 `Promise.all` 而不是嵌套回调吗？

---

上一篇：[Java —— 职责](java.md) ・ 下一篇：[Haskell —— 变换](haskell.md)
