# Node.js 深度剖析 — Part 1: 架构概览与事件循环

> 承接 [Part 1 (Core Concepts)](nodejs_tutorial_part1.md) 和 [Part 2 (Building Things)](nodejs_tutorial_part2.md)，本系列深入 Node.js 运行时内部原理。
>
> **适用读者**：已具备 Node.js 基础使用能力，希望理解"为什么"而非仅"怎么用"的开发者。

---

## 目录

- [模块一：架构概览与核心理念](#模块一架构概览与核心理念)
  - [1.1 核心模型：单线程、事件驱动、非阻塞 I/O](#11-核心模型单线程事件驱动非阻塞-io)
  - [1.2 关键组件的协作流程](#12-关键组件的协作流程)
  - [1.3 线程模型的真相](#13-线程模型的真相)
  - [1.4 请求处理全流程追踪](#14-请求处理全流程追踪)
- [模块二：事件循环深度解析](#模块二事件循环深度解析)
  - [2.1 Libuv 事件循环的六个阶段](#21-libuv-事件循环的六个阶段)
  - [2.2 微任务与宏任务](#22-微任务与宏任务)
  - [2.3 `process.nextTick` vs `setImmediate`](#23-processnexttick-vs-setimmediate)
  - [2.4 实践观察：预测输出顺序](#24-实践观察预测输出顺序)
  - [2.5 事件循环的阻塞与饥饿](#25-事件循环的阻塞与饥饿)

---

## 模块一：架构概览与核心理念

### 1.1 核心模型：单线程、事件驱动、非阻塞 I/O

传统的多线程服务器（如 Apache 的 prefork 模型）为每个连接分配一个线程或进程。这意味着：

```
传统模型（每连接一线程）:

  Client 1 ──→ [Thread 1] ──→ 阻塞等待数据库查询 ──→ 响应
  Client 2 ──→ [Thread 2] ──→ 阻塞等待文件读取   ──→ 响应
  Client 3 ──→ [Thread 3] ──→ 阻塞等待网络请求   ──→ 响应
  ...
  Client N ──→ [Thread N] ──→ 内存耗尽，线程切换开销暴增
```

每个线程约消耗 1-8 MB 栈内存，加上上下文切换成本，线程数到达数千时性能急剧下降。这就是所谓的 **C10K 问题**。

Node.js 采用了完全不同的策略：

```
Node.js 模型（单线程 + 事件循环）:

  Client 1 ──┐
  Client 2 ──┤
  Client 3 ──┼──→ [单个 JS 线程] ──→ 事件循环调度
  ...        │         │
  Client N ──┘         ├── I/O 操作 → 委托给 OS 或线程池（非阻塞）
                       ├── 回调注册到事件队列
                       └── 当 I/O 完成 → 回调被执行
```

**关键洞察**：绝大多数 Web 应用的瓶颈是 I/O（数据库查询、文件读写、网络请求），而非 CPU 计算。Node.js 不在 I/O 等待上浪费线程——它注册回调，然后去处理下一个请求。

用 C 程序员的视角理解：Node.js 就是一个精心封装的 `epoll`/`kqueue` 事件循环，只不过用 JavaScript 而非 C 来写回调函数。

### 1.2 关键组件的协作流程

```
┌──────────────────────────────────────────────────────────────────┐
│                     你的 JavaScript 代码                         │
│            (app.js, route handlers, business logic)              │
├──────────────────────────────────────────────────────────────────┤
│                  Node.js Core Library (JS)                       │
│         (fs, http, net, stream, crypto 等模块的 JS 层)           │
├──────────────────────────────────────────────────────────────────┤
│                  Node.js Bindings (C/C++)                        │
│          (连接 JS 层与底层 C/C++ 库的桥梁)                        │
│          使用 V8 API 将 C++ 对象暴露为 JS 对象                    │
├───────────────┬───────────────┬──────────────────────────────────┤
│   V8 Engine   │    libuv      │       其他依赖库                  │
│  (C++, ~1M    │  (C, ~25K     │                                  │
│   行代码)      │   行代码)     │  ┌─ c-ares: 异步 DNS 解析        │
│               │               │  ├─ OpenSSL/BoringSSL: TLS/加密  │
│  ● JS 解析    │  ● 事件循环    │  ├─ zlib: 压缩/解压              │
│  ● JIT 编译   │  ● 异步 I/O   │  ├─ llhttp: HTTP 解析            │
│  ● 内存管理   │  ● 线程池      │  ├─ nghttp2: HTTP/2 协议         │
│  ● GC 垃圾回收│  ● 信号处理    │  └─ ICU: 国际化支持              │
│               │  ● 子进程管理  │                                  │
├───────────────┴───────────────┴──────────────────────────────────┤
│                      Operating System                            │
│           (epoll/kqueue/IOCP, syscalls, TCP/IP stack)            │
└──────────────────────────────────────────────────────────────────┘
```

**各层职责详解**：

| 层级 | 职责 | 类比（C 开发者视角） |
|---|---|---|
| JavaScript 代码 | 业务逻辑、路由、回调 | `main()` 函数及应用逻辑 |
| Node.js Core (JS) | 封装底层 API 为友好的 JS 接口 | `libc` 的头文件和封装函数 |
| Node.js Bindings (C++) | 将 JS 调用翻译为 C/C++ 调用 | FFI / JNI 绑定层 |
| V8 | 编译执行 JavaScript | CPU + 编译器 |
| libuv | 事件循环、异步 I/O 抽象 | `select()`/`epoll()` + 线程池 |
| 其他库 | 专门领域（加密、DNS、HTTP 解析等） | 链接的 `.so` 共享库 |

**一次 HTTP 请求的完整生命周期**：

```
1. 客户端发送 TCP 连接请求
   │
2. OS 内核接受连接 (accept syscall)
   │
3. libuv 的 poll 阶段通过 epoll/kqueue 感知到新连接
   │
4. libuv 触发 connection 回调 → Node.js bindings 层
   │
5. Bindings 层通过 V8 创建 JS 层的 Socket 对象
   │
6. 触发 http.Server 的 'request' 事件
   │
7. 你的回调函数执行：
   │   app.get('/users', async (req, res) => {
   │       const users = await db.query('SELECT * FROM users');
   │       res.json(users);
   │   });
   │
8. db.query() → Bindings → libuv → 线程池发起数据库 I/O
   │
9. JS 线程继续处理其他请求（不阻塞）
   │
10. 数据库响应 → libuv 通知 → 回调进入事件队列
    │
11. 事件循环下一轮 → 执行回调 → res.json(users) → 响应客户端
```

### 1.3 线程模型的真相

"Node.js 是单线程的"这个说法既对又不对。更准确的描述是：

> **JavaScript 的执行是单线程的。Node.js 进程本身是多线程的。**

```
Node.js 进程的线程全景：

┌──────────────────────────────────────────────────┐
│                 Node.js 进程                      │
│                                                  │
│  [Main Thread]                                   │
│   ├── V8 JavaScript 执行                         │
│   ├── libuv 事件循环                              │
│   └── 微任务处理（nextTick, Promise）              │
│                                                  │
│  [V8 Helper Threads]                             │
│   ├── GC 并发标记线程                             │
│   ├── JIT 编译后台线程                            │
│   └── 优化/去优化线程                             │
│                                                  │
│  [libuv Thread Pool] (默认 4 个, 最多 1024)       │
│   ├── Thread 1: fs.readFile('/big.csv')          │
│   ├── Thread 2: crypto.pbkdf2(password)          │
│   ├── Thread 3: dns.lookup('example.com')        │
│   └── Thread 4: zlib.gzip(data)                  │
│                                                  │
│  [OS Async I/O]  (不占用线程池)                    │
│   ├── TCP/UDP 网络 I/O (epoll/kqueue/IOCP)       │
│   ├── 管道/Unix domain socket                     │
│   └── 信号处理                                    │
│                                                  │
│  [Worker Threads]  (如果你显式创建)                │
│   ├── worker_thread 1                            │
│   └── worker_thread 2                            │
└──────────────────────────────────────────────────┘
```

**哪些操作使用线程池？哪些不使用？**

| 操作类型 | 使用线程池？ | 底层机制 |
|---|---|---|
| TCP/UDP 网络 I/O | 否 | OS 异步原语 (epoll/kqueue/IOCP) |
| 文件系统操作 (fs.*) | **是** | POSIX 没有真正的异步文件 I/O |
| DNS 查询 (dns.lookup) | **是** | 调用 getaddrinfo() — 阻塞调用 |
| DNS 查询 (dns.resolve) | 否 | 使用 c-ares 库（非阻塞） |
| crypto 操作 (pbkdf2, scrypt) | **是** | CPU 密集型，卸载到线程 |
| zlib 压缩/解压 | **是** | CPU 密集型 |
| 子进程 (child_process) | 否 | OS fork/exec |
| HTTP 请求处理 | 否 | 基于 TCP 的 OS 异步 I/O |

**为什么文件 I/O 用线程池？**

Linux 的 `epoll` 和 macOS 的 `kqueue` 对常规文件不提供真正的异步 I/O 通知（它们只对 socket、pipe 等网络 fd 有效）。Linux 有 `io_uring`（5.1+）和 AIO，但 libuv 出于兼容性和可靠性考虑，仍然使用线程池来模拟文件系统的异步操作。

```c
/* 简化的 libuv 内部实现思路 (伪代码) */

/* 网络 I/O：直接注册到 epoll */
int
handle_tcp_read(uv_stream_t *stream)
{
    /* 直接添加到 epoll 监控，无需线程 */
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, stream->fd, &ev);
    return 0;
}

/* 文件 I/O：提交到线程池 */
int
handle_fs_read(uv_fs_t *req)
{
    /* 将读取操作提交给线程池中的一个工作线程 */
    uv__work_submit(&req->work, uv__fs_work, uv__fs_done);
    return 0;
}
```

### 1.4 请求处理全流程追踪

以一个读取文件并返回的 HTTP 请求为例，追踪每一层的执行：

```javascript
const http = require('http');
const fs = require('fs').promises;

const server = http.createServer(async (req, res) => {
    const data = await fs.readFile('/tmp/data.json', 'utf8');
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(data);
});

server.listen(3000);
```

**追踪路径**：

```
┌─ 步骤 1: server.listen(3000)
│   JS 层: net.Server.listen()
│   Bindings: TCPWrap::Listen()
│   libuv: uv_listen() → uv_tcp_listen()
│   OS: bind() + listen() + 注册到 epoll
│
├─ 步骤 2: 客户端连接到达
│   OS: epoll_wait() 返回事件
│   libuv: uv__server_io() → accept()
│   Bindings: TCPWrap::OnConnection()
│   JS 层: 触发 'connection' 事件 → http 解析请求 → 触发 'request' 事件
│   你的回调: async (req, res) => { ... }
│
├─ 步骤 3: fs.readFile() 调用
│   JS 层: fs.promises.readFile()
│   Bindings: FSReqCallback::New() → 创建 uv_fs_t 请求
│   libuv: uv_fs_read() → uv__work_submit()
│   Thread Pool: 工作线程执行 pread() 系统调用（阻塞在工作线程中）
│   *** 主线程此时空闲，可以处理其他请求 ***
│
├─ 步骤 4: 文件读取完成
│   Thread Pool: pread() 返回 → 将结果放入完成队列
│   libuv: uv__work_done() → uv__io_poll() 通过 eventfd 唤醒
│   事件循环下一次迭代的 pending callbacks 阶段处理
│
└─ 步骤 5: 响应发送
    JS 层: res.end(data)
    Bindings: StreamBase::WriteString()
    libuv: uv_write() → 直接写入 TCP socket
    OS: write() / sendmsg() → TCP 协议栈发送数据
```

---

## 模块二：事件循环深度解析

### 2.1 Libuv 事件循环的六个阶段

事件循环是 Node.js 的心脏。它不是一个简单的 `while(1) { poll_events(); handle_events(); }` 循环，而是有着精心设计的多阶段处理流水线。

```
         ┌───────────────────────────────────────────┐
     ┌──→│           1. timers (定时器)                │
     │   │   执行到期的 setTimeout/setInterval 回调     │
     │   └──────────────────┬────────────────────────┘
     │                      │ ↓ 清空微任务队列
     │   ┌──────────────────┴────────────────────────┐
     │   │        2. pending callbacks               │
     │   │   执行上一轮推迟的系统级回调                  │
     │   │   (如 TCP 连接错误的 ECONNREFUSED 回调)      │
     │   └──────────────────┬────────────────────────┘
     │                      │ ↓ 清空微任务队列
     │   ┌──────────────────┴────────────────────────┐
     │   │        3. idle, prepare                   │
     │   │   libuv 内部使用（Node.js 内部维护）         │
     │   │   对应用开发者透明                           │
     │   └──────────────────┬────────────────────────┘
     │                      │ ↓ 清空微任务队列
     │   ┌──────────────────┴────────────────────────┐
     │   │           4. poll (轮询)                   │
     │   │   ● 计算应阻塞等待 I/O 的时间               │
     │   │   ● 执行 I/O 相关回调 (数据到达/连接到来等)   │
     │   │   ● 这是事件循环花费最多时间的阶段            │
     │   └──────────────────┬────────────────────────┘
     │                      │ ↓ 清空微任务队列
     │   ┌──────────────────┴────────────────────────┐
     │   │        5. check (检查)                     │
     │   │   执行 setImmediate() 回调                  │
     │   │   专为"I/O 之后立即执行"设计                  │
     │   └──────────────────┬────────────────────────┘
     │                      │ ↓ 清空微任务队列
     │   ┌──────────────────┴────────────────────────┐
     │   │        6. close callbacks                 │
     │   │   执行关闭回调 (如 socket.on('close'))       │
     │   └──────────────────┬────────────────────────┘
     │                      │ ↓ 清空微任务队列
     │                      │
     │   ┌──────────────────┴────────────────────────┐
     │   │   是否还有活跃的 handles/requests？          │
     │   │   是 → 继续下一轮迭代                        │
     └───│   否 → 退出事件循环，进程结束                 │
         └───────────────────────────────────────────┘
```

**各阶段详解**：

#### 阶段 1: timers

检查是否有到期的定时器回调。`setTimeout` 和 `setInterval` 的回调在这里执行。

```javascript
// setTimeout 在 timers 阶段执行
setTimeout(() => {
    console.log('timer expired');
}, 100);
```

注意：定时器的延迟是**最小延迟**，不是精确延迟。如果 poll 阶段处理 I/O 花了 200ms，那么 `setTimeout(fn, 100)` 实际上至少要等 200ms 后才能执行。

**底层实现**：libuv 使用**最小堆**（min-heap）来管理定时器，这样每次只需检查堆顶元素是否到期，时间复杂度 O(log n)。

#### 阶段 2: pending callbacks

处理上一轮循环中被推迟的回调。通常是一些操作系统级别的错误回调，如 TCP 连接时收到 `ECONNREFUSED`。大多数 I/O 回调在 poll 阶段执行，这里处理的是例外情况。

#### 阶段 3: idle, prepare

内部使用阶段。`idle` 并不意味着"空闲时执行"——它在每次循环迭代时都会运行。`prepare` 在 poll 之前执行。Node.js 内部用于统计和维护工作。对应用开发者来说基本透明。

#### 阶段 4: poll

这是事件循环最核心的阶段，执行以下工作：

1. **计算阻塞时间**：根据最近的定时器到期时间，决定 `epoll_wait()` / `kevent()` 应等待多久
2. **执行 I/O 回调**：处理已就绪的 I/O 事件（数据接收、连接就绪等）

```
Poll 阶段的行为决策：

                 进入 poll 阶段
                       │
              ┌────────┴────────┐
              │ poll 队列非空？   │
              └────┬───────┬────┘
                  是       否
                   │       │
          执行所有回调 ┌──────┴──────┐
          (同步执行)  │ 有 setImmediate │
                    │  注册过？       │
                    └──┬────────┬───┘
                      是        否
                       │        │
               立即离开,    ┌─────┴─────┐
               进入 check  │ 有定时器？  │
                          └──┬────┬───┘
                            是    否
                             │    │
                    等待 I/O, 最多 │
                    等到最近的     │
                    timer 到期    阻塞等待 I/O
                                 (直到有事件到达)
```

#### 阶段 5: check

执行所有 `setImmediate()` 回调。`setImmediate` 从语义上设计为"在 I/O 完成后立即执行"，所以它紧跟在 poll 阶段之后。

#### 阶段 6: close callbacks

处理关闭事件的回调，比如 `socket.on('close', ...)` 或 `server.close(callback)`。如果一个 handle 通过 `uv_close()` 被关闭，其关闭回调在这里触发。

### 2.2 微任务与宏任务

Node.js 中的异步回调分为两大类：

```
┌─────────────────────────────────────────────────────┐
│                    微任务 (Microtasks)               │
│   在每个事件循环阶段结束后、进入下一阶段前执行           │
│                                                     │
│   优先级从高到低：                                    │
│   1. process.nextTick() 回调                        │
│   2. Promise 回调 (.then/.catch/.finally)            │
│   3. queueMicrotask() 回调                          │
│                                                     │
│   特点：会递归清空——微任务中产生的新微任务               │
│         也会在当前阶段结束前被执行完毕                  │
├─────────────────────────────────────────────────────┤
│                    宏任务 (Macrotasks)               │
│   在事件循环的特定阶段执行                             │
│                                                     │
│   ● setTimeout / setInterval  → timers 阶段         │
│   ● setImmediate              → check 阶段          │
│   ● I/O 回调                  → poll 阶段           │
│   ● close callbacks          → close 阶段           │
└─────────────────────────────────────────────────────┘
```

**微任务在每个阶段切换时都会被清空**，这是 Node.js v11+ 的行为（与浏览器对齐）：

```
事件循环的实际执行流：

  ┌─ timers 阶段
  │    执行 timer 回调 A
  │    → 清空微任务队列 (nextTick → Promise)
  │    执行 timer 回调 B
  │    → 清空微任务队列
  │
  ├─ pending callbacks 阶段
  │    执行 pending 回调
  │    → 清空微任务队列
  │
  ├─ idle/prepare
  │    → 清空微任务队列
  │
  ├─ poll 阶段
  │    执行 I/O 回调 X
  │    → 清空微任务队列
  │    执行 I/O 回调 Y
  │    → 清空微任务队列
  │
  ├─ check 阶段
  │    执行 setImmediate 回调
  │    → 清空微任务队列
  │
  └─ close callbacks
       执行 close 回调
       → 清空微任务队列
```

> **Node.js v11 之前**：微任务在整个阶段结束后才清空（即所有 timer 回调执行完才处理微任务）。v11+ 改为每个回调执行完就清空微任务，与浏览器行为一致。

**代码验证**：

```javascript
// 微任务优先级演示
setTimeout(() => console.log('1: setTimeout'), 0);

setImmediate(() => console.log('2: setImmediate'));

Promise.resolve().then(() => console.log('3: Promise'));

process.nextTick(() => console.log('4: nextTick'));

queueMicrotask(() => console.log('5: queueMicrotask'));

console.log('6: 同步代码');

/*
 * 输出：
 * 6: 同步代码          ← 同步代码最先执行
 * 4: nextTick          ← nextTick 微任务优先级最高
 * 3: Promise           ← Promise 微任务次之
 * 5: queueMicrotask    ← queueMicrotask 与 Promise 同级
 * 1: setTimeout        ← timers 阶段（可能在 setImmediate 之前或之后）
 * 2: setImmediate      ← check 阶段
 *
 * 注意：setTimeout(fn, 0) 和 setImmediate 在主模块中的顺序
 * 取决于事件循环启动时 timer 是否已经到期，因此不确定。
 */
```

### 2.3 `process.nextTick` vs `setImmediate`

这两个 API 的命名容易产生误导。从名字上看 `nextTick` 好像应该在"下一次循环"执行，而 `setImmediate` 应该"立即"执行。但实际恰恰相反：

| 特性 | `process.nextTick()` | `setImmediate()` |
|---|---|---|
| 执行时机 | 当前操作完成后**立即**执行，在进入下一个事件循环阶段之前 | 在当前循环的 **check 阶段**执行 |
| 本质 | 微任务 | 宏任务 |
| 队列位置 | 微任务队列（最高优先级） | check 阶段队列 |
| 递归风险 | **有**——递归调用会阻塞事件循环 | **无**——递归调用在下一轮循环执行 |
| 适用场景 | 确保回调在当前同步代码执行完后立刻运行 | 确保回调在 I/O 处理之后执行 |

**递归风险的具体示例**：

```javascript
// 危险！会饿死事件循环
function recursiveNextTick() {
    process.nextTick(() => {
        console.log('nextTick');
        recursiveNextTick(); // 永远不会离开微任务阶段
    });
}
recursiveNextTick();
setTimeout(() => console.log('这永远不会执行'), 0);

// 安全——每次递归都在新的循环迭代中
function recursiveImmediate() {
    setImmediate(() => {
        console.log('immediate');
        recursiveImmediate(); // 允许其他阶段（如 timers, poll）有机会执行
    });
}
recursiveImmediate();
setTimeout(() => console.log('这会正常执行'), 1000);
```

**何时使用 `process.nextTick`？**

1. 在构造函数中发出事件（确保用户有机会注册监听器之后才触发）：

```javascript
const EventEmitter = require('events');

class MyStream extends EventEmitter {
    constructor() {
        super();
        // 如果直接 emit，调用者还没来得及注册 listener
        // this.emit('data', 'hello'); // 错误！

        // 正确做法：defer 到当前同步代码之后
        process.nextTick(() => {
            this.emit('data', 'hello');
        });
    }
}

const stream = new MyStream();
stream.on('data', (chunk) => {
    console.log('Received:', chunk); // 正常接收到
});
```

2. 确保回调始终异步执行（API 一致性原则）：

```javascript
function loadData(path, callback) {
    const cached = cache.get(path);
    if (cached) {
        // 即使有缓存，也异步调用 callback
        // 避免调用者的代码执行顺序取决于是否命中缓存
        process.nextTick(() => callback(null, cached));
        return;
    }
    fs.readFile(path, callback);
}
```

### 2.4 实践观察：预测输出顺序

#### 实验 1: 基础排序

```javascript
console.log('start');

setTimeout(() => console.log('timeout1'), 0);
setTimeout(() => console.log('timeout2'), 0);

setImmediate(() => console.log('immediate1'));
setImmediate(() => console.log('immediate2'));

Promise.resolve()
    .then(() => {
        console.log('promise1');
        process.nextTick(() => console.log('nextTick-inside-promise'));
    })
    .then(() => console.log('promise2'));

process.nextTick(() => console.log('nextTick1'));
process.nextTick(() => {
    console.log('nextTick2');
    Promise.resolve().then(() => console.log('promise-inside-nextTick'));
});

console.log('end');
```

**逐步分析**：

```
阶段 0: 同步执行阶段
  → start
  → end
  （注册了: 2个 setTimeout, 2个 setImmediate, 1个 Promise 链, 2个 nextTick）

阶段 0.5: 清空微任务（在进入事件循环之前）
  先清空 nextTick 队列:
    → nextTick1
    → nextTick2
      （nextTick2 中产生了新的 Promise 微任务）
  再清空 Promise 队列:
    → promise1
      （promise1 中产生了新的 nextTick 微任务，新的 .then 也排入 Promise 队列）
    → promise-inside-nextTick
      （这是 nextTick2 中产生的 Promise）
  nextTick 队列中又有新任务:
    → nextTick-inside-promise
  继续清空 Promise 队列:
    → promise2

进入事件循环:
  timers 阶段:
    → timeout1
    → timeout2
  check 阶段:
    → immediate1
    → immediate2
```

**最终输出**：
```
start
end
nextTick1
nextTick2
promise1
promise-inside-nextTick
nextTick-inside-promise
promise2
timeout1
timeout2
immediate1
immediate2
```

#### 实验 2: I/O 回调中的 setTimeout vs setImmediate

```javascript
const fs = require('fs');

fs.readFile(__filename, () => {
    console.log('I/O callback');

    setTimeout(() => console.log('timeout'), 0);
    setImmediate(() => console.log('immediate'));

    process.nextTick(() => console.log('nextTick'));
    Promise.resolve().then(() => console.log('promise'));
});
```

**分析**：`readFile` 的回调在 poll 阶段执行。执行完后：

1. 先清空微任务队列 → `nextTick`, `promise`
2. 然后进入 check 阶段 → `immediate`
3. 下一轮循环 timers 阶段 → `timeout`

```
输出（确定性的）：
I/O callback
nextTick
promise
immediate
timeout
```

在 I/O 回调中，`setImmediate` **总是**在 `setTimeout(fn, 0)` 之前执行，因为从 poll 阶段出来先经过 check 阶段。

#### 实验 3: 主模块中 setTimeout vs setImmediate 的不确定性

```javascript
// 在主模块（非 I/O 回调内）
setTimeout(() => console.log('timeout'), 0);
setImmediate(() => console.log('immediate'));
```

```
可能的输出 1:          可能的输出 2:
timeout                immediate
immediate              timeout
```

**原因**：`setTimeout(fn, 0)` 在 Node.js 内部实际上是 `setTimeout(fn, 1)`（最小延迟 1ms）。如果事件循环启动进入 timers 阶段时已经过了 1ms，则 timeout 先执行。否则 timer 还未到期，循环继续到 poll → check，setImmediate 先执行。这取决于系统时钟精度和进程启动速度。

#### 实验 4: 嵌套定时器

```javascript
setImmediate(() => {
    console.log('immediate1');
    setImmediate(() => console.log('immediate2'));
    setTimeout(() => console.log('timeout-from-immediate'), 0);
    process.nextTick(() => console.log('nextTick-from-immediate'));
});

setTimeout(() => {
    console.log('timeout1');
    setImmediate(() => console.log('immediate-from-timeout'));
    setTimeout(() => console.log('timeout2'), 0);
    process.nextTick(() => console.log('nextTick-from-timeout'));
}, 0);
```

**分析**：

假设 `setTimeout` 先执行（进入 timers 阶段时 1ms 已过）：

```
第一轮循环:
  timers 阶段:
    → timeout1
    微任务: → nextTick-from-timeout
  check 阶段:
    → immediate1
    微任务: → nextTick-from-immediate

第二轮循环:
  timers 阶段:
    → timeout-from-immediate
    → timeout2
  check 阶段:
    → immediate-from-timeout
    → immediate2
```

### 2.5 事件循环的阻塞与饥饿

**阻塞问题**：由于 JavaScript 在单线程上执行，任何长时间运行的同步操作都会阻塞整个事件循环。

```javascript
const http = require('http');

const server = http.createServer((req, res) => {
    if (req.url === '/compute') {
        // 模拟一个 CPU 密集型计算
        // 这会阻塞事件循环 5 秒！
        const start = Date.now();
        while (Date.now() - start < 5000) {
            // busy loop
        }
        res.end('Computed!');
    } else {
        res.end('Hello!');
    }
});

server.listen(3000);

/*
 * 问题：当一个请求访问 /compute 时，
 * 所有其他请求（包括简单的 / ）都被阻塞 5 秒。
 * 因为事件循环卡在这个同步计算中，无法处理任何 I/O 回调。
 */
```

**可视化阻塞效果**：

```
时间线:
t=0s    请求 A 访问 /compute
        │
        ├── JS 线程开始执行同步计算 ──────────────────────┐
        │                                                │
t=1s    请求 B 访问 / (到达 OS TCP 缓冲区，但无法被处理)    │
t=2s    请求 C 访问 / (同上)                               │
t=3s    (什么都不会发生)                                   │
t=4s    (事件循环完全冻结)                                 │
        │                                                │
t=5s    计算完成，响应 A ←──────────────────────────────────┘
        │
        ├── 事件循环恢复
        ├── 立刻处理请求 B → 响应
        └── 立刻处理请求 C → 响应
```

**解决方案**：

```javascript
const { Worker } = require('worker_threads');

const server = http.createServer((req, res) => {
    if (req.url === '/compute') {
        // 将 CPU 密集型工作卸载到独立线程
        const worker = new Worker('./heavy-computation.js');
        worker.on('message', (result) => {
            res.end(`Computed: ${result}`);
        });
        worker.on('error', (err) => {
            res.writeHead(500);
            res.end(err.message);
        });
    } else {
        res.end('Hello!');
    }
});
```

**微任务饥饿**：

```javascript
// 这个 nextTick 递归会让事件循环永远停留在微任务清空阶段
function infiniteNextTick() {
    process.nextTick(infiniteNextTick);
}
infiniteNextTick();

// 任何 I/O、timer、setImmediate 都永远不会被执行
// 这就是"微任务饥饿"
```

**监控事件循环延迟的实用代码**：

```javascript
let lastCheck = process.hrtime.bigint();

setInterval(() => {
    const now = process.hrtime.bigint();
    const delta = Number(now - lastCheck) / 1e6; // 转为毫秒
    lastCheck = now;

    // interval 设为 1000ms，如果 delta 远大于 1000，说明事件循环被阻塞
    if (delta > 1500) {
        console.warn(`Event loop lag: ${(delta - 1000).toFixed(0)}ms`);
    }
}, 1000);

// 或使用 monitorEventLoopDelay (Node.js 11.10+)
const { monitorEventLoopDelay } = require('perf_hooks');
const h = monitorEventLoopDelay({ resolution: 20 });
h.enable();

setInterval(() => {
    console.log({
        min: h.min / 1e6,       // ms
        max: h.max / 1e6,
        mean: h.mean / 1e6,
        p99: h.percentile(99) / 1e6,
    });
    h.reset();
}, 5000);
```

---

## 小结

| 概念 | 关键要点 |
|---|---|
| 单线程模型 | JS 执行单线程，但 Node.js 进程是多线程的（V8 线程 + libuv 线程池） |
| 事件循环 | 6 个阶段按固定顺序执行：timers → pending → idle/prepare → poll → check → close |
| 微任务 | `nextTick` > `Promise` > `queueMicrotask`，在每个回调执行完后清空 |
| 宏任务 | `setTimeout` (timers)、I/O 回调 (poll)、`setImmediate` (check) |
| 确定性 | I/O 回调中 `setImmediate` 总在 `setTimeout(fn,0)` 之前；主模块中顺序不确定 |
| 阻塞 | 同步计算和 nextTick 递归都会阻塞事件循环，可用 Worker Threads 解决 |

> 下一篇：[Part 2 — 异步编程范式与 V8 引擎](nodejs_deep_dive_part2.md)
