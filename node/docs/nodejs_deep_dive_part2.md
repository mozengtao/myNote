# Node.js 深度剖析 — Part 2: 异步编程范式与 V8 引擎

> 承接 [Part 1 — 架构概览与事件循环](nodejs_deep_dive_part1.md)

---

## 目录

- [模块三：异步编程范式深度探索](#模块三异步编程范式深度探索)
  - [3.1 演进历程：从回调地狱到 async/await](#31-演进历程从回调地狱到-asyncawait)
  - [3.2 错误处理对比](#32-错误处理对比)
  - [3.3 性能模式与控制流](#33-性能模式与控制流)
  - [3.4 取消与超时：AbortController](#34-取消与超时abortcontroller)
  - [3.5 EventEmitter：异步事件的基石](#35-eventemitter异步事件的基石)
- [模块四：V8 引擎关键技术细节](#模块四v8-引擎关键技术细节)
  - [4.1 内存管理与堆结构](#41-内存管理与堆结构)
  - [4.2 垃圾回收机制](#42-垃圾回收机制)
  - [4.3 常见内存泄漏场景及诊断](#43-常见内存泄漏场景及诊断)
  - [4.4 JIT 编译与优化策略](#44-jit-编译与优化策略)
  - [4.5 V8 与 Node.js 的交互接口](#45-v8-与-nodejs-的交互接口)

---

## 模块三：异步编程范式深度探索

### 3.1 演进历程：从回调地狱到 async/await

Node.js 异步编程经历了三个时代：

```
时间线:
2009 ──── Callback 时代 (Node.js 诞生)
          │
2015 ──── Promise 时代 (ES6 / Node 4.0)
          │
2017 ──── Async/Await 时代 (ES2017 / Node 7.6)
```

#### Callback 时代

Node.js 最初使用 **Error-first Callback** 约定：回调函数第一个参数永远是 error 对象（或 null）。

```javascript
const fs = require('fs');

// 单个操作——还算清晰
fs.readFile('/etc/hostname', 'utf8', (err, hostname) => {
    if (err) {
        console.error('Failed to read hostname:', err.message);
        return;
    }
    console.log('Hostname:', hostname.trim());
});
```

问题在嵌套操作中暴露——读取文件 → 处理内容 → 写入结果 → 确认写入：

```javascript
// "回调地狱" (Callback Hell / Pyramid of Doom)
fs.readFile('input.txt', 'utf8', (err, data) => {
    if (err) return handleError(err);

    processData(data, (err, result) => {
        if (err) return handleError(err);

        fs.writeFile('output.txt', result, (err) => {
            if (err) return handleError(err);

            fs.stat('output.txt', (err, stats) => {
                if (err) return handleError(err);

                console.log('Written:', stats.size, 'bytes');
                // 更多嵌套...
            });
        });
    });
});
```

**缺陷分析**：
- 深层嵌套降低可读性
- 错误处理必须在每一层重复
- 无法使用 `try/catch`（异步错误不在同步调用栈上）
- 流程控制（并行、竞争）需要手动计数器

#### Promise 时代

Promise 是对"未来值"的封装。一个 Promise 有三种状态：

```
          ┌─── fulfilled (resolve) ──→ .then(onFulfilled)
          │
pending ──┤
          │
          └─── rejected (reject)   ──→ .catch(onRejected)

一旦状态确定就不可变 (settled / immutable)
```

```javascript
const fsp = require('fs').promises;

// 链式写法——消除嵌套
fsp.readFile('input.txt', 'utf8')
    .then(data => processDataAsync(data))
    .then(result => fsp.writeFile('output.txt', result))
    .then(() => fsp.stat('output.txt'))
    .then(stats => console.log('Written:', stats.size, 'bytes'))
    .catch(err => console.error('Pipeline failed:', err.message));
```

**Promise 的内部机制（简化）**：

```javascript
// Promise 内部状态机的概念模型
class SimplePromise {
    #state = 'pending';
    #value = undefined;
    #callbacks = [];

    constructor(executor) {
        const resolve = (value) => {
            if (this.#state !== 'pending') return;
            this.#state = 'fulfilled';
            this.#value = value;
            this.#callbacks.forEach(cb => cb.onFulfilled(value));
        };

        const reject = (reason) => {
            if (this.#state !== 'pending') return;
            this.#state = 'rejected';
            this.#value = reason;
            this.#callbacks.forEach(cb => cb.onRejected(reason));
        };

        try {
            executor(resolve, reject);
        } catch (err) {
            reject(err);
        }
    }

    then(onFulfilled, onRejected) {
        return new SimplePromise((resolve, reject) => {
            const handle = { onFulfilled, onRejected, resolve, reject };
            if (this.#state === 'pending') {
                this.#callbacks.push(handle);
            } else {
                queueMicrotask(() => {
                    const fn = this.#state === 'fulfilled'
                        ? onFulfilled : onRejected;
                    try {
                        resolve(fn(this.#value));
                    } catch (err) {
                        reject(err);
                    }
                });
            }
        });
    }
}
```

#### Async/Await 时代

`async/await` 是 Promise 的语法糖，让异步代码看起来像同步代码：

```javascript
const fsp = require('fs').promises;

async function processFile() {
    const data = await fsp.readFile('input.txt', 'utf8');
    const result = await processDataAsync(data);
    await fsp.writeFile('output.txt', result);
    const stats = await fsp.stat('output.txt');
    console.log('Written:', stats.size, 'bytes');
}

processFile().catch(err => console.error('Pipeline failed:', err.message));
```

**本质**：编译器将 async 函数转换为状态机。每个 `await` 是一个挂起点，函数在此暂停并让出控制权，等 Promise resolve 后从该点恢复。

```
async function example() {
    console.log('A');       // 状态 0: 同步执行
    await promise1;         // 挂起点 → 让出控制权
    console.log('B');       // 状态 1: promise1 resolve 后恢复
    await promise2;         // 挂起点 → 让出控制权
    console.log('C');       // 状态 2: promise2 resolve 后恢复
}

转换为状态机 (概念上):

function example() {
    return new Promise((resolve) => {
        // 状态 0
        console.log('A');
        promise1.then(() => {
            // 状态 1
            console.log('B');
            promise2.then(() => {
                // 状态 2
                console.log('C');
                resolve();
            });
        });
    });
}
```

#### 三种范式对比总结

| 维度 | Callback | Promise | Async/Await |
|---|---|---|---|
| 可读性 | 嵌套深 | 链式较好 | 接近同步，最佳 |
| 错误处理 | 手动检查每层 | `.catch()` 集中 | `try/catch` 自然 |
| 调试 | 调用栈丢失 | 部分保留 | 完整调用栈 |
| 流程控制 | 手动 | `Promise.all` 等 | `for await` + 组合 |
| 性能 | 最低开销 | 微小开销（微任务调度） | 与 Promise 等同 |
| 适用场景 | 旧代码、极简单回调 | 库 API 返回值 | 应用层业务逻辑 |

### 3.2 错误处理对比

#### Callback: Error-first 模式

```javascript
function readAndParse(path, callback) {
    fs.readFile(path, 'utf8', (err, data) => {
        if (err) {
            callback(err); // 传递错误
            return;
        }
        try {
            const parsed = JSON.parse(data);
            callback(null, parsed);
        } catch (parseErr) {
            callback(parseErr); // 同步异常也必须通过 callback 传递
        }
    });
}

// 调用
readAndParse('config.json', (err, config) => {
    if (err) {
        console.error('Failed:', err.message);
        return;
    }
    console.log(config);
});
```

**陷阱**：

```javascript
// 常见错误：忘记 return
fs.readFile('file.txt', 'utf8', (err, data) => {
    if (err) {
        callback(err);
        // 忘了 return! 下面的代码继续执行了
    }
    // data 可能是 undefined，导致 crash
    const result = data.toUpperCase();
    callback(null, result);
});

// 常见错误：在 callback 中 throw
fs.readFile('file.txt', 'utf8', (err, data) => {
    if (err) throw err; // 这会变成 uncaught exception！
    // 应该用 callback(err) 传递
});
```

#### Promise: .catch() 链

```javascript
function readAndParse(path) {
    return fs.promises.readFile(path, 'utf8')
        .then(data => JSON.parse(data)); // JSON.parse 抛出的异常自动变成 rejection
}

// 调用——统一的错误处理
readAndParse('config.json')
    .then(config => console.log(config))
    .catch(err => console.error('Failed:', err.message));
```

**catch 的位置影响行为**：

```javascript
// 模式 A: 末尾 catch——捕获链上所有错误
fetchUser()
    .then(user => fetchOrders(user.id))
    .then(orders => processOrders(orders))
    .catch(err => {
        // fetchUser、fetchOrders、processOrders 任一失败都走这里
        console.error(err);
    });

// 模式 B: 中间 catch——恢复后继续
fetchUser()
    .catch(err => {
        console.warn('fetchUser failed, using default');
        return defaultUser; // 返回恢复值
    })
    .then(user => fetchOrders(user.id)) // 即使上面失败也会执行
    .then(orders => processOrders(orders))
    .catch(err => console.error('Pipeline failed:', err));
```

**未处理的 rejection**：

```javascript
// Node.js 默认会在 unhandledRejection 时打印警告
// 从 Node.js 15 开始，未处理的 rejection 会导致进程退出

process.on('unhandledRejection', (reason, promise) => {
    console.error('Unhandled Rejection at:', promise, 'reason:', reason);
    // 在生产环境应记录日志并优雅退出
    process.exit(1);
});
```

#### Async/Await: try/catch

```javascript
async function readAndParse(path) {
    const data = await fs.promises.readFile(path, 'utf8');
    return JSON.parse(data); // 同步异常自动变成 rejected Promise
}

// 调用
async function main() {
    try {
        const config = await readAndParse('config.json');
        console.log(config);
    } catch (err) {
        console.error('Failed:', err.message);
    }
}
```

**细粒度错误处理**：

```javascript
async function complexOperation() {
    let user;
    try {
        user = await fetchUser();
    } catch (err) {
        console.warn('User fetch failed, using default');
        user = defaultUser;
    }

    let orders;
    try {
        orders = await fetchOrders(user.id);
    } catch (err) {
        // 区分错误类型
        if (err.code === 'ECONNREFUSED') {
            throw new Error('Order service is down');
        }
        orders = [];
    }

    return processOrders(orders);
}
```

### 3.3 性能模式与控制流

#### 串行 vs 并行

```javascript
// 串行：一个接一个，总耗时 = sum(各操作耗时)
async function serial() {
    const user   = await fetchUser();      // 100ms
    const orders = await fetchOrders();    // 200ms
    const items  = await fetchItems();     // 150ms
    // 总耗时 ≈ 450ms
    return { user, orders, items };
}

// 并行：同时启动，总耗时 = max(各操作耗时)
async function parallel() {
    const [user, orders, items] = await Promise.all([
        fetchUser(),      // 100ms ─┐
        fetchOrders(),    // 200ms ─┤─ 并行
        fetchItems(),     // 150ms ─┘
    ]);
    // 总耗时 ≈ 200ms
    return { user, orders, items };
}
```

```
时间线对比:

串行:
  fetchUser   |████|
  fetchOrders         |████████|
  fetchItems                      |██████|
              0     100    200    300    450ms

并行:
  fetchUser   |████|
  fetchOrders |████████|
  fetchItems  |██████|
              0     100    200ms
```

#### Promise.all vs Promise.allSettled vs Promise.race vs Promise.any

```javascript
const tasks = [
    fetch('https://api-a.com/data'),   // 成功, 150ms
    fetch('https://api-b.com/data'),   // 失败, 100ms
    fetch('https://api-c.com/data'),   // 成功, 200ms
];

// Promise.all: 全部成功才成功，任一失败则立即 reject
try {
    const results = await Promise.all(tasks);
    // 不会到这里——api-b 失败了
} catch (err) {
    // err 是 api-b 的错误
}

// Promise.allSettled: 等所有完成，无论成功失败
const results = await Promise.allSettled(tasks);
// [
//   { status: 'fulfilled', value: Response },
//   { status: 'rejected',  reason: Error },
//   { status: 'fulfilled', value: Response }
// ]

// Promise.race: 最先完成的结果（无论成功或失败）
const fastest = await Promise.race(tasks);
// api-b 最快(100ms)，但它失败了 → 抛出错误

// Promise.any: 最先成功的结果（忽略失败）
const firstSuccess = await Promise.any(tasks);
// api-a 在 150ms 成功 → 返回 api-a 的结果
// 只有全部失败才 reject (AggregateError)
```

```
对比矩阵:

             全成功           有失败
all          ✓ 返回全部结果    ✗ 立即 reject（快速失败）
allSettled   ✓ 返回全部结果    ✓ 返回全部结果（含失败详情）
race         ✓ 返回最快的      依赖最快的结果
any          ✓ 返回最快的      ✓ 返回最快成功的
```

#### 控制并发数

在实际场景中，你可能有 10000 个 URL 需要请求，但不想同时发 10000 个请求（会耗尽 fd、触发限流）。

```javascript
async function parallelLimit(tasks, concurrency) {
    const results = new Array(tasks.length);
    let index = 0;

    async function worker() {
        while (index < tasks.length) {
            const i = index++;
            results[i] = await tasks[i]();
        }
    }

    const workers = Array.from({ length: concurrency }, () => worker());
    await Promise.all(workers);
    return results;
}

// 使用：同时最多 5 个并发请求
const urls = Array.from({ length: 100 }, (_, i) => `https://api.example.com/item/${i}`);
const tasks = urls.map(url => () => fetch(url).then(r => r.json()));

const results = await parallelLimit(tasks, 5);
```

### 3.4 取消与超时：AbortController

`AbortController` 是 Node.js 15+ 原生支持的异步操作取消机制。

```javascript
// 基本使用
const controller = new AbortController();
const { signal } = controller;

// 超时取消
setTimeout(() => controller.abort(), 5000);

try {
    const response = await fetch('https://slow-api.com/data', { signal });
    const data = await response.json();
} catch (err) {
    if (err.name === 'AbortError') {
        console.log('Request was cancelled (timeout)');
    } else {
        throw err;
    }
}
```

**超时工具函数**：

```javascript
function withTimeout(asyncFn, ms) {
    return async (...args) => {
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), ms);

        try {
            const result = await asyncFn(...args, { signal: controller.signal });
            return result;
        } finally {
            clearTimeout(timeout);
        }
    };
}

// Node.js 内置的 AbortSignal.timeout() (Node 17.3+)
const signal = AbortSignal.timeout(5000);

await fs.promises.readFile('big-file.txt', { signal });
```

**手动支持取消的异步函数**：

```javascript
async function pollForResult(taskId, { signal } = {}) {
    while (true) {
        // 每次循环检查是否被取消
        signal?.throwIfAborted();

        const result = await checkStatus(taskId);
        if (result.done) return result.data;

        await new Promise((resolve, reject) => {
            const timer = setTimeout(resolve, 1000);

            // 监听 abort 事件以在等待期间也能取消
            signal?.addEventListener('abort', () => {
                clearTimeout(timer);
                reject(signal.reason);
            }, { once: true });
        });
    }
}

const controller = new AbortController();
const resultPromise = pollForResult('task-123', { signal: controller.signal });

// 用户点击取消
cancelButton.onclick = () => controller.abort();
```

### 3.5 EventEmitter：异步事件的基石

`EventEmitter` 是 Node.js 中最基础的异步模式，许多核心模块都继承自它：

```
EventEmitter 的继承关系:

                  EventEmitter
                       │
        ┌──────────────┼──────────────┐
        │              │              │
    Stream         net.Server    http.Server
        │              │
   ┌────┼────┐     net.Socket
   │    │    │
Readable  Writable  Duplex/Transform
   │                   │
fs.ReadStream    zlib.Gzip
```

**基本使用**：

```javascript
const EventEmitter = require('events');

class TaskRunner extends EventEmitter {
    async run(tasks) {
        this.emit('start', { total: tasks.length });

        for (let i = 0; i < tasks.length; i++) {
            try {
                const result = await tasks[i]();
                this.emit('progress', { index: i, result });
            } catch (err) {
                this.emit('error', err);
            }
        }

        this.emit('end');
    }
}

const runner = new TaskRunner();

runner.on('start', ({ total }) => console.log(`Starting ${total} tasks`));
runner.on('progress', ({ index }) => console.log(`Task ${index} done`));
runner.on('error', (err) => console.error('Task failed:', err.message));
runner.on('end', () => console.log('All tasks complete'));
```

**关键 API**：

```javascript
const ee = new EventEmitter();

// 注册监听器
ee.on('data', handler);          // 持续监听
ee.once('ready', handler);       // 只触发一次后自动移除
ee.prependListener('data', h);   // 插入到监听器列表头部

// 移除监听器
ee.removeListener('data', handler);  // 等价于 ee.off('data', handler)
ee.removeAllListeners('data');       // 移除 'data' 的所有监听器
ee.removeAllListeners();             // 移除所有事件的所有监听器

// 触发事件
ee.emit('data', payload);  // 同步调用所有监听器，返回 boolean (是否有监听器)

// 实用方法
ee.listenerCount('data');    // 监听器数量
ee.eventNames();             // 所有已注册事件名
ee.getMaxListeners();        // 默认 10，超过会警告潜在内存泄漏
ee.setMaxListeners(50);      // 调整上限
```

**重要特性：emit 是同步的**：

```javascript
const ee = new EventEmitter();

ee.on('data', (val) => {
    console.log('listener 1:', val);
});

ee.on('data', (val) => {
    console.log('listener 2:', val);
});

console.log('before emit');
ee.emit('data', 42);
console.log('after emit');

/*
 * 输出:
 * before emit
 * listener 1: 42
 * listener 2: 42
 * after emit
 *
 * emit 同步执行所有监听器，然后才返回。
 */
```

**将 EventEmitter 事件转为 Promise / async iterator**：

```javascript
const { once, on } = require('events');

// 等待单次事件
async function waitForReady(server) {
    await once(server, 'listening');
    console.log('Server is ready');
}

// 使用 for-await-of 遍历事件流
async function processRequests(server) {
    for await (const [req, res] of on(server, 'request')) {
        res.end('handled');
    }
}
```

**error 事件的特殊行为**：

```javascript
const ee = new EventEmitter();

// 如果没有 'error' 监听器，emit('error') 会 throw
// 这会导致 uncaught exception -> 进程崩溃
ee.emit('error', new Error('boom'));
// => throws Error: boom

// 因此任何 EventEmitter 实例都应注册 error 处理
ee.on('error', (err) => {
    console.error('Caught:', err.message);
});
```

---

## 模块四：V8 引擎关键技术细节

### 4.1 内存管理与堆结构

V8 将内存分为**栈内存**和**堆内存**。栈存放基本类型值和函数调用帧，堆存放对象（引用类型）。

```
V8 堆内存布局:

┌───────────────────────────────────────────────────────────┐
│                        V8 Heap                            │
│                                                           │
│  ┌─────────────────────────────────────────────────────┐  │
│  │              New Space (新生代)                       │  │
│  │              ~~~~~~~~~~~~~~                          │  │
│  │  大小: 1-8 MB (可通过 --max-semi-space-size 调整)     │  │
│  │                                                     │  │
│  │  ┌─────────────────┬─────────────────┐              │  │
│  │  │  Semi-space A   │  Semi-space B   │              │  │
│  │  │  (from/to)      │  (from/to)      │              │  │
│  │  │  活跃的半区      │  空闲的半区       │              │  │
│  │  └─────────────────┴─────────────────┘              │  │
│  │  用途: 新分配的短生命周期对象                          │  │
│  │  GC 算法: Scavenge (复制算法, 非常快)                  │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                           │
│  ┌─────────────────────────────────────────────────────┐  │
│  │              Old Space (老生代)                       │  │
│  │              ~~~~~~~~~~~~~~                          │  │
│  │  大小: 默认 ~1.4 GB (64位), 可通过                    │  │
│  │        --max-old-space-size 调整                      │  │
│  │                                                     │  │
│  │  ┌──────────────┐  ┌──────────────┐                 │  │
│  │  │ Old Pointer  │  │ Old Data     │                 │  │
│  │  │ Space        │  │ Space        │                 │  │
│  │  │ (含指针的    │  │ (纯数据,    │                  │  │
│  │  │  对象)       │  │  无指针)     │                  │  │
│  │  └──────────────┘  └──────────────┘                 │  │
│  │  用途: 经过两次 Scavenge 存活的长生命周期对象           │  │
│  │  GC 算法: Mark-Sweep + Mark-Compact                  │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                           │
│  ┌──────────────────┐  ┌──────────────────┐              │
│  │  Large Object    │  │  Code Space      │              │
│  │  Space           │  │                  │              │
│  │  大于阈值的      │  │  JIT 编译后的    │              │
│  │  大对象          │  │  机器代码        │              │
│  └──────────────────┘  └──────────────────┘              │
│                                                           │
│  ┌──────────────────┐                                    │
│  │  Map Space        │                                    │
│  │  (Hidden Classes/ │                                    │
│  │   对象形状描述)    │                                    │
│  └──────────────────┘                                    │
└───────────────────────────────────────────────────────────┘
```

**对象在堆中的生命周期**：

```
对象分配在 New Space
       │
       ├── 第一次 Scavenge GC → 存活 → 复制到另一半区
       │
       ├── 第二次 Scavenge GC → 仍存活 → "晋升" 到 Old Space
       │
       └── 在 Old Space 中，等待 Mark-Sweep/Mark-Compact 回收
```

### 4.2 垃圾回收机制

#### Scavenge (新生代 GC)

```
工作原理 (Cheney 算法):

Before GC:
  From-space: [A][B][C][D][E]
  To-space:   [  空  ]

1. 从根对象出发，遍历所有可达对象
2. 将可达对象复制到 To-space
3. 更新所有指向旧地址的引用

After GC (假设 B, D 不可达):
  From-space: [  可以释放  ]
  To-space:   [A][C][E]

4. 交换 From-space 和 To-space 的角色

特点:
  ✓ 速度极快 (只复制存活对象，大部分新生对象都短命)
  ✓ 自动整理内存碎片
  ✗ 内存利用率 50% (始终有一半空间空闲)
  ✗ 不适合大量长生命周期对象
```

#### Mark-Sweep-Compact (老生代 GC)

```
阶段 1: Mark (标记)
  从根对象 (全局对象、执行栈、handles) 出发
  遍历所有可达对象，标记为"活跃"

  根 ──→ [A]✓ ──→ [B]✓ ──→ [C]✓
              ──→ [D]✓
         [E]✗ (不可达)
         [F]✗ (不可达)

阶段 2: Sweep (清除)
  遍历整个 Old Space，回收未标记对象的内存

  [A][  ][B][  ][C][D]   (E, F 的内存被释放)

阶段 3: Compact (整理) — 可选
  将存活对象移动到一端，消除碎片

  [A][B][C][D][       自由空间       ]
```

**GC 对性能的影响与优化**：

```
V8 GC 的优化策略:

┌─ 增量标记 (Incremental Marking)
│   不一次性完成标记，分多次小步骤进行
│   每次标记几毫秒，然后让出执行权给 JS
│
│   JS 执行 → [标记 5ms] → JS 执行 → [标记 5ms] → ...
│
├─ 并发标记 (Concurrent Marking)
│   标记工作在后台线程执行，不停顿 JS 线程
│
│   JS 线程:    [执行] ──────────────── [执行]
│   GC 线程:         [标记──────────]
│
├─ 并行清除 (Parallel Sweeping)
│   多个线程同时清除不同内存页
│
├─ 惰性清除 (Lazy Sweeping)
│   不立刻清除所有垃圾，在需要分配新内存时按需清除
│
└─ Orinoco (V8 的现代 GC 架构)
    组合以上所有策略，将 GC 停顿降到最低
```

### 4.3 常见内存泄漏场景及诊断

#### 场景 1: 全局变量积累

```javascript
// 泄漏：data 数组不断增长，永远不会被 GC
const data = [];

function processRequest(req) {
    data.push(req.body); // 每次请求都追加，从不清理
}
```

#### 场景 2: 闭包引用

```javascript
function createHandler() {
    const hugeBuffer = Buffer.alloc(10 * 1024 * 1024); // 10MB

    return function handler(req, res) {
        // handler 闭包持有对 hugeBuffer 的引用
        // 即使 handler 不使用 hugeBuffer，只要 handler 存活，
        // hugeBuffer 就不会被 GC（V8 优化有时能避免这个问题，但不保证）
        res.end('ok');
    };
}

const handlers = [];
for (let i = 0; i < 100; i++) {
    handlers.push(createHandler()); // 积累 1GB 内存
}
```

#### 场景 3: 未清理的事件监听器

```javascript
const ee = new EventEmitter();

function setupHandler(socket) {
    // 每次连接都注册新的监听器
    ee.on('broadcast', (msg) => {
        socket.write(msg);
    });

    // 当 socket 断开时，监听器未移除！
    // 结果：已断开的 socket 对象因闭包引用而无法被 GC
}

// 修复：
function setupHandler(socket) {
    const handler = (msg) => socket.write(msg);
    ee.on('broadcast', handler);

    socket.on('close', () => {
        ee.removeListener('broadcast', handler); // 清理！
    });
}
```

#### 场景 4: Timer 泄漏

```javascript
function pollServer(url) {
    const intervalId = setInterval(async () => {
        const data = await fetch(url);
        // 处理数据...
    }, 5000);

    // 如果没有 clearInterval(intervalId)，
    // 这个 timer 和它的闭包会永远存活
}

// 每次调用 pollServer 都创建新的 interval，旧的永远不清理
```

#### 诊断方法：堆快照分析

```bash
# 启动 Node.js 并开启 inspector
node --inspect app.js

# 或者动态发送信号开启 (Linux)
kill -USR1 <pid>
```

```javascript
// 程序内获取堆统计
const v8 = require('v8');

setInterval(() => {
    const stats = v8.getHeapStatistics();
    console.log({
        totalHeapSize:      (stats.total_heap_size / 1024 / 1024).toFixed(1) + ' MB',
        usedHeapSize:       (stats.used_heap_size / 1024 / 1024).toFixed(1) + ' MB',
        heapSizeLimit:      (stats.heap_size_limit / 1024 / 1024).toFixed(1) + ' MB',
        externalMemory:     (stats.external_memory / 1024 / 1024).toFixed(1) + ' MB',
    });
}, 10000);
```

```javascript
// 手动触发堆快照 (写入文件，用 Chrome DevTools 分析)
const v8 = require('v8');
const fs = require('fs');

function takeHeapSnapshot() {
    const snapshotStream = v8.writeHeapSnapshot();
    console.log(`Heap snapshot written to: ${snapshotStream}`);
}

// 或者通过 inspector 协议
const inspector = require('inspector');
const session = new inspector.Session();
session.connect();

session.post('HeapProfiler.takeHeapSnapshot', null, (err, r) => {
    console.log('Heap snapshot taken');
    session.disconnect();
});
```

```
Chrome DevTools 堆快照分析流程:

1. 打开 chrome://inspect → 连接到 Node.js 进程
2. Memory → Take heap snapshot
3. 关注 "Retained Size" 列 → 真正占用的内存
4. 比较两次快照 → 查看 "Objects allocated between Snapshot 1 and Snapshot 2"
5. 搜索增长最快的对象类型

常用视图:
  Summary:     按构造函数分组，查看对象数量和大小
  Comparison:  对比两次快照，找到新增对象
  Containment: 查看对象的引用链（谁引用了这个对象？）
  Statistics:  堆使用率饼图
```

### 4.4 JIT 编译与优化策略

V8 使用多层编译流水线：

```
JavaScript 源码
       │
       ▼
┌──────────────────┐
│    Parser        │  解析为 AST (抽象语法树)
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│   Ignition       │  V8 的字节码解释器
│   (Interpreter)  │  快速启动，低内存占用
│                  │  同时收集类型反馈信息 (Type Feedback)
└──────┬───────────┘
       │
       │  函数被频繁调用 (变成 "热点函数")
       │  Type Feedback 显示类型稳定
       │
       ▼
┌──────────────────┐
│   TurboFan       │  优化编译器
│   (JIT Compiler) │  生成高度优化的机器代码
│                  │  使用 Type Feedback 做类型特化
└──────┬───────────┘
       │
       │  如果类型假设被违反 → Deoptimization
       │  回退到 Ignition 解释执行
       ▼
```

#### 内联缓存 (Inline Caches)

```javascript
function getX(obj) {
    return obj.x; // 这个属性访问会被内联缓存加速
}

/*
 * 第一次调用 getX({ x: 1 })：
 *   - V8 发现 obj 的 "shape" (Hidden Class) 是 {x: offset 0}
 *   - 记录: "如果 shape 匹配，直接从 offset 0 读取"
 *
 * 后续调用 getX({ x: 2 })（相同 shape）：
 *   - shape 匹配 → 直接用 offset 读取，跳过属性查找
 *   - 这就是 "单态内联缓存" (Monomorphic IC)
 *
 * 如果调用 getX({ x: 1, y: 2 })（不同 shape）：
 *   - IC 变为 "多态" (Polymorphic)，需要检查多个 shape
 *   - 性能下降
 *
 * 如果超过 4 种不同 shape：
 *   - IC 变为 "超态" (Megamorphic)
 *   - 退化为通用属性查找，性能最差
 */
```

**写出 V8 友好的代码**：

```javascript
// 好：所有对象保持相同 shape
function createPoint(x, y) {
    return { x, y }; // 总是相同的属性顺序和类型
}
const p1 = createPoint(1, 2);
const p2 = createPoint(3, 4);
// p1 和 p2 共享相同的 Hidden Class

// 差：动态添加属性导致不同 shape
function createPoint(x, y) {
    const p = {};
    p.x = x;
    if (y !== undefined) p.y = y; // 条件属性
    if (x > 100) p.big = true;     // 条件属性
    return p;
}
// 每种条件组合产生不同的 Hidden Class → IC 退化
```

```javascript
// 好：保持类型一致
function sum(arr) {
    let total = 0;
    for (let i = 0; i < arr.length; i++) {
        total += arr[i]; // V8 假设 arr[i] 总是 number
    }
    return total;
}
sum([1, 2, 3]);    // number 数组 → TurboFan 优化
sum([1, '2', 3]);  // 混合类型！→ Deoptimization → 性能悬崖

// 好：使用 TypedArray 做数值计算
const arr = new Float64Array(1000);
// V8 确定知道每个元素是 float64，无需类型检查
```

#### Deoptimization (去优化)

```javascript
function add(a, b) {
    return a + b;
}

// 前 10000 次调用：add(整数, 整数) → TurboFan 编译为整数加法机器指令
for (let i = 0; i < 10000; i++) {
    add(i, i + 1);
}

// 突然传入字符串：
add('hello', 'world');
// TurboFan 的机器代码假设 a, b 是整数 → 假设违反
// → Deoptimization: 丢弃优化代码，回到 Ignition 解释执行
// → 重新收集类型信息，可能生成新的（处理多种类型的）优化代码
```

```
监控去优化事件:

$ node --trace-deopt app.js
$ node --trace-opt app.js
$ node --trace-opt --trace-deopt app.js 2>&1 | grep "not stable"
```

### 4.5 V8 与 Node.js 的交互接口

#### v8 模块

```javascript
const v8 = require('v8');

// 堆统计
const stats = v8.getHeapStatistics();
console.log(stats);
/*
{
  total_heap_size: 6537216,
  total_heap_size_executable: 524288,
  total_physical_size: 5765120,
  total_available_size: 2191287608,
  used_heap_size: 4883744,
  heap_size_limit: 2197815296,
  malloced_memory: 8192,
  peak_malloced_memory: 585728,
  does_zap_garbage: 0,
  number_of_native_contexts: 1,
  number_of_detached_contexts: 0,
  total_global_handles_size: 8192,
  used_global_handles_size: 2112,
  external_memory: 1335527
}
*/

// 堆空间详细统计
const spaces = v8.getHeapSpaceStatistics();
spaces.forEach(space => {
    console.log(`${space.space_name}: ${(space.space_used_size / 1024).toFixed(0)} KB`);
});
/*
  read_only_space: 0 KB
  new_space: 1024 KB
  old_space: 2316 KB
  code_space: 416 KB
  map_space: 264 KB
  large_object_space: 0 KB
  ...
*/

// 结构化序列化/反序列化 (比 JSON 快，支持更多类型)
const buf = v8.serialize({ date: new Date(), regex: /hello/g, map: new Map([['a', 1]]) });
const obj = v8.deserialize(buf);
console.log(obj.date instanceof Date); // true (JSON 做不到)
console.log(obj.regex instanceof RegExp); // true
console.log(obj.map instanceof Map); // true
```

#### 设置 V8 引擎参数

```bash
# 调整堆大小
node --max-old-space-size=4096 app.js    # 老生代 4GB
node --max-semi-space-size=64 app.js     # 新生代半区 64MB

# 暴露 GC 接口 (用于测试/调试)
node --expose-gc app.js
# 然后在代码中: global.gc() 手动触发 GC

# 打印 GC 事件
node --trace-gc app.js
# [45023:0x...]    15 ms: Scavenge 2.1 (3.0) -> 1.8 (4.0) MB, 0.6 / 0.0 ms ...
# [45023:0x...]  2015 ms: Mark-sweep 42.0 (44.0) -> 39.3 (44.0) MB, 15.3 / 0.0 ms ...

# 详细 GC 信息
node --trace-gc --trace-gc-verbose app.js
```

#### 原生插件 (N-API / Node-API)

当需要从 Node.js 调用 C/C++ 代码时，使用 Node-API（稳定的 ABI 接口）：

```c
/* addon.c — 一个简单的原生插件 */
#include <node_api.h>

static napi_value
add(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);

    double a, b;
    napi_get_value_double(env, args[0], &a);
    napi_get_value_double(env, args[1], &b);

    napi_value result;
    napi_create_double(env, a + b, &result);
    return result;
}

static napi_value
init(napi_env env, napi_value exports)
{
    napi_value fn;
    napi_create_function(env, NULL, 0, add, NULL, &fn);
    napi_set_named_property(env, exports, "add", fn);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
```

```javascript
// 使用原生插件
const addon = require('./build/Release/addon');
console.log(addon.add(3, 4)); // 7
```

---

## 小结

| 概念 | 关键要点 |
|---|---|
| 异步演进 | Callback → Promise → Async/Await，各有适用场景 |
| 错误处理 | Error-first → .catch() → try/catch，async/await 最自然 |
| 并行控制 | `Promise.all` (全部)、`allSettled` (容错)、`race` (竞速)、`any` (首个成功) |
| 取消机制 | `AbortController` / `AbortSignal.timeout()` |
| EventEmitter | Node.js 异步事件基础，emit 是同步的，注意清理监听器 |
| V8 堆结构 | New Space (Scavenge) → Old Space (Mark-Sweep-Compact) |
| 内存泄漏 | 全局变量、闭包、未清理监听器、Timer 忘记清理 |
| JIT 优化 | Ignition (解释) → TurboFan (编译)；保持类型稳定，避免 Deoptimization |
| V8 交互 | `v8` 模块、`--trace-gc`、堆快照、Node-API 原生插件 |

> 下一篇：[Part 3 — Libuv 详解与综合应用](nodejs_deep_dive_part3.md)
