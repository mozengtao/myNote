# Node.js 深度剖析 — Part 3: Libuv 详解与综合应用

> 承接 [Part 1 — 架构概览与事件循环](nodejs_deep_dive_part1.md) 和 [Part 2 — 异步编程范式与 V8 引擎](nodejs_deep_dive_part2.md)

---

## 目录

- [模块五：Libuv 库详解](#模块五libuv-库详解)
  - [5.1 核心职责与跨平台抽象](#51-核心职责与跨平台抽象)
  - [5.2 Handles 和 Requests](#52-handles-和-requests)
  - [5.3 线程池机制](#53-线程池机制)
  - [5.4 网络 I/O vs 文件 I/O：底层实现差异](#54-网络-io-vs-文件-io底层实现差异)
  - [5.5 libuv 事件循环的 C 语言视角](#55-libuv-事件循环的-c-语言视角)
- [模块六：综合应用与知识巩固](#模块六综合应用与知识巩固)
  - [6.1 构建一个迷你事件循环](#61-构建一个迷你事件循环)
  - [6.2 async_hooks：追踪异步资源生命周期](#62-async_hooks追踪异步资源生命周期)
  - [6.3 设计模式实现：连接池](#63-设计模式实现连接池)
  - [6.4 设计模式实现：背压控制流处理](#64-设计模式实现背压控制流处理)
  - [6.5 综合问答](#65-综合问答)

---

## 模块五：Libuv 库详解

### 5.1 核心职责与跨平台抽象

Libuv 是 Node.js 的"操作系统抽象层"。它最初是为 Node.js 开发的，现在也被其他项目（如 Julia、Luvit）使用。

```
Libuv 抽象的操作系统差异:

              Node.js / 应用代码
                     │
                     ▼
            ┌─── libuv API ─────┐
            │   统一的接口       │
            └───────┬───────────┘
                    │
       ┌────────────┼────────────────┐
       │            │                │
    Linux        macOS           Windows
   ┌─────┐     ┌──────┐         ┌──────┐
   │epoll│     │kqueue│         │ IOCP │
   └─────┘     └──────┘         └──────┘
   eventfd     kevent()        GetQueuedCompletion
   timerfd                      StatusEx()
   signalfd                     CreateIoCompletion
   inotify     FSEvents         Port()
   io_uring    kqueue(EVFILT     ReadDirectoryChanges
               _VNODE)
```

**Libuv 提供的完整功能集**：

| 功能领域 | API 示例 | 对应的 Node.js 模块 |
|---|---|---|
| 事件循环 | `uv_run()`, `uv_stop()` | 隐式 — Node.js 主循环 |
| TCP/UDP 网络 | `uv_tcp_t`, `uv_udp_t` | `net`, `dgram` |
| DNS 解析 | `uv_getaddrinfo()` | `dns.lookup()` |
| 文件系统 | `uv_fs_open()`, `uv_fs_read()` | `fs` |
| 子进程 | `uv_spawn()`, `uv_process_t` | `child_process` |
| 管道/IPC | `uv_pipe_t` | `net` (IPC), `child_process` |
| TTY 控制 | `uv_tty_t` | `tty`, `readline` |
| 定时器 | `uv_timer_t` | `setTimeout`, `setInterval` |
| 信号处理 | `uv_signal_t` | `process.on('SIGINT')` |
| 文件监控 | `uv_fs_event_t`, `uv_fs_poll_t` | `fs.watch()`, `fs.watchFile()` |
| 线程池 | `uv_queue_work()` | 内部使用 (fs, crypto, dns.lookup) |
| 共享库加载 | `uv_dlopen()` | `process.dlopen()` |
| 线程/互斥锁 | `uv_thread_t`, `uv_mutex_t` | `worker_threads` 底层 |

### 5.2 Handles 和 Requests

Libuv 使用两种核心数据结构来管理异步操作：

```
┌────────────────────────────────────────────────────────────────┐
│                        Handles (句柄)                          │
│                                                                │
│  代表长生命周期对象，可以在活跃时持续触发回调                       │
│                                                                │
│  生命周期: 创建 → 初始化 → 启动 → [持续回调] → 关闭 → 释放       │
│                                                                │
│  示例:                                                         │
│   ┌─────────────┬──────────────────────────────────┐           │
│   │ uv_tcp_t    │ TCP 服务器/连接 (持续监听/收发)     │           │
│   │ uv_timer_t  │ 定时器 (持续/重复触发)              │           │
│   │ uv_idle_t   │ 每次循环迭代触发                    │           │
│   │ uv_signal_t │ 信号处理 (SIGINT 等)               │           │
│   │ uv_pipe_t   │ 管道/Unix domain socket            │           │
│   │ uv_process_t│ 子进程                             │           │
│   └─────────────┴──────────────────────────────────┘           │
│                                                                │
│  关键特性: 只要 handle 是活跃的，事件循环就不会退出               │
│           必须显式 uv_close() 才能释放                          │
├────────────────────────────────────────────────────────────────┤
│                       Requests (请求)                          │
│                                                                │
│  代表短生命周期操作，完成后调用一次回调就结束                      │
│                                                                │
│  生命周期: 创建 → 提交 → [等待完成] → 回调 → 释放               │
│                                                                │
│  示例:                                                         │
│   ┌─────────────┬──────────────────────────────────┐           │
│   │ uv_fs_t     │ 文件系统操作 (open/read/write)     │           │
│   │ uv_write_t  │ 写入操作                           │           │
│   │ uv_connect_t│ 连接请求                           │           │
│   │ uv_work_t   │ 线程池工作项                        │           │
│   │ uv_getaddrinfo_t│ DNS 查询                      │           │
│   └─────────────┴──────────────────────────────────┘           │
│                                                                │
│  关键特性: 一次性的，完成后自动结束                               │
│           不影响事件循环的存活判断                                │
└────────────────────────────────────────────────────────────────┘
```

**用 C 代码展示差异**：

```c
#include <uv.h>
#include <stdio.h>

/* Handle 示例: TCP 服务器 — 长生命周期 */

static void
on_connection(uv_stream_t *server, int status)
{
    /* 每次有新连接时触发，server handle 持续存活 */
    printf("New connection! (status=%d)\n", status);
}

void
setup_tcp_server(uv_loop_t *loop)
{
    uv_tcp_t *server = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, server);                /* 初始化 handle */

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", 8080, &addr);
    uv_tcp_bind(server, (struct sockaddr *)&addr, 0);

    uv_listen((uv_stream_t *)server, 128, on_connection);  /* 启动 */
    /* server handle 现在是活跃的，事件循环会持续运行 */
}

/* Request 示例: 文件读取 — 短生命周期 */

static void
on_read_complete(uv_fs_t *req)
{
    if(req->result < 0) {
        fprintf(stderr, "Read error: %s\n", uv_strerror(req->result));
    } else {
        printf("Read %ld bytes\n", req->result);
    }
    uv_fs_req_cleanup(req);  /* 清理请求 */
    free(req);               /* 释放内存 */
    /* request 的生命周期到此结束 */
}

void
read_file(uv_loop_t *loop, uv_file fd, char *buf, size_t len)
{
    uv_fs_t *req = malloc(sizeof(uv_fs_t));
    uv_buf_t iov = uv_buf_init(buf, len);
    uv_fs_read(loop, req, fd, &iov, 1, 0, on_read_complete);
    /* req 被提交到线程池，完成后回调被调用一次 */
}
```

**Handle 与 Request 的关系**：

```
一个 TCP 服务器的完整交互:

  uv_tcp_t (handle)          ← 服务器本身，长生命周期
       │
       ├── uv_listen()      ← 开始监听
       │
       ├── on_connection()  ← 新连接到达 (多次触发)
       │       │
       │       ├── uv_accept()
       │       │       │
       │       │       └── uv_tcp_t (handle)   ← 新的客户端连接，也是长生命周期
       │       │               │
       │       │               ├── uv_read_start()  ← 开始读取
       │       │               │
       │       │               ├── uv_write_t (request) ← 写入操作，短生命周期
       │       │               │
       │       │               └── uv_close()  ← 关闭连接
       │       │
       │       └── (下一个连接...)
       │
       └── uv_close()       ← 关闭服务器
```

### 5.3 线程池机制

Libuv 的线程池是一个固定大小的工作线程池，用于处理那些无法通过 OS 异步 I/O 完成的操作。

```
线程池架构:

                  主线程 (JS 执行 + 事件循环)
                          │
                   uv_queue_work()
                          │
                          ▼
             ┌─── 任务队列 (FIFO) ───┐
             │  [task1][task2][task3] │
             └──────────┬────────────┘
                        │
          ┌─────────────┼─────────────┐
          │             │             │
     ┌────▼────┐   ┌────▼────┐   ┌────▼────┐   ┌─────────┐
     │ Worker  │   │ Worker  │   │ Worker  │   │ Worker  │
     │ Thread 1│   │ Thread 2│   │ Thread 3│   │ Thread 4│
     └────┬────┘   └────┬────┘   └────┬────┘   └────┬────┘
          │             │             │             │
          └─────────────┼─────────────┘             │
                        │                           │
                   完成通知 (通过 eventfd/pipe)
                        │
                        ▼
                  主线程执行完成回调
```

**默认大小与调整**：

```bash
# 默认 4 个工作线程
UV_THREADPOOL_SIZE=4  # 默认值

# 调整线程池大小 (必须在程序启动前设置)
UV_THREADPOOL_SIZE=16 node app.js

# 最大值 1024
UV_THREADPOOL_SIZE=1024 node app.js
```

```javascript
// 在代码中设置 (必须在任何异步操作之前)
process.env.UV_THREADPOOL_SIZE = '16';

// 验证
const { execSync } = require('child_process');
// 注意：环境变量在 libuv 初始化时读取，之后修改无效
```

**线程池饱和问题**：

```javascript
const crypto = require('crypto');
const fs = require('fs');

// 场景：4 个线程池 + 密集 crypto 操作 + 文件读取

// 4 个 crypto 操作占满线程池
for (let i = 0; i < 4; i++) {
    crypto.pbkdf2('password', 'salt', 100000, 64, 'sha512', () => {
        console.log(`crypto ${i} done at ${Date.now() - start}ms`);
    });
}

// 文件读取必须等待线程池有空闲线程
fs.readFile('/tmp/small.txt', () => {
    console.log(`file read done at ${Date.now() - start}ms`);
    // 即使文件很小，也要等 crypto 操作释放一个线程才能开始
});

const start = Date.now();

/*
 * 线程池大小为 4 时的输出：
 *
 * crypto 0 done at 85ms
 * file read done at 87ms    ← 等了 85ms 才开始读取！
 * crypto 1 done at 88ms
 * crypto 2 done at 89ms
 * crypto 3 done at 90ms
 *
 * 如果设置 UV_THREADPOOL_SIZE=8:
 * file read done at 2ms     ← 立刻执行
 * crypto 0 done at 86ms
 * ...
 */
```

**使用线程池的操作完整列表**：

| 操作 | 对应 Node.js API |
|---|---|
| 所有 `fs.*` 操作 | `fs.readFile`, `fs.writeFile`, `fs.stat`, ... |
| `dns.lookup()` | 使用系统 `getaddrinfo()` |
| `crypto.pbkdf2()` | 密码哈希 |
| `crypto.scrypt()` | 密码哈希 |
| `crypto.randomBytes()` | 随机数生成 |
| `crypto.randomFill()` | 随机数填充 |
| `zlib` 操作 | `zlib.gzip()`, `zlib.deflate()`, ... |

### 5.4 网络 I/O vs 文件 I/O：底层实现差异

这是理解 Node.js 性能特征的关键区分点。

#### 网络 I/O: `net.createServer()`

```
net.createServer() 的底层路径:

  JavaScript:  net.createServer(callback)
       │
  Bindings:    TCPWrap::New() + TCPWrap::Listen()
       │
  libuv:       uv_tcp_init() + uv_listen()
       │
  OS 层面:
       │
  Linux:       socket() → bind() → listen()
               然后将 fd 注册到 epoll:
               epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)
       │
  macOS:       socket() → bind() → listen()
               然后注册到 kqueue:
               kevent(kq, &changelist, 1, NULL, 0, NULL)
       │
  Windows:     CreateIoCompletionPort() + AcceptEx()
       │
  事件到达时:
       │
  Linux:       epoll_wait() 返回 → 无需线程池参与
  macOS:       kevent() 返回     → 无需线程池参与
  Windows:     GetQueuedCompletionStatus() → 无需线程池
```

**关键**：网络 I/O 完全不使用线程池，由操作系统的 I/O 多路复用机制直接通知。这就是为什么 Node.js 能处理数以万计的并发网络连接而不需要那么多线程。

#### 文件 I/O: `fs.readFile()`

```
fs.readFile() 的底层路径:

  JavaScript:  fs.readFile('/tmp/data.txt', callback)
       │
  Bindings:    FSReqCallback → 创建 uv_fs_t request
       │
  libuv:       uv_fs_read()
       │
       ├── 将请求提交到线程池队列
       │
       ▼
  线程池中的工作线程:
       │
       ├── 执行阻塞系统调用: pread(fd, buf, len, offset)
       │   (这个调用在工作线程中阻塞，不影响主线程)
       │
       ├── 完成后，将结果放入完成队列
       │
       └── 通过 eventfd (Linux) / pipe (其他) 通知主线程
              │
              ▼
  主线程的事件循环:
       │
       ├── poll 阶段的 epoll_wait() 感知到 eventfd 可读
       │
       └── 执行完成回调 → callback(null, data)
```

**为什么文件 I/O 不能像网络一样用 epoll？**

```
原因：POSIX 的设计限制

epoll/kqueue 对文件 fd 的行为:
  ● 常规文件的 fd 在 epoll 中 **总是** 报告为"可读/可写"
  ● 因为内核不会为磁盘 I/O 做延迟通知
  ● epoll 是为 socket/pipe 等"可能阻塞"的 fd 设计的

对比:
  socket fd → epoll: "数据到了才通知你"  ← 有意义
  file fd   → epoll: "总是说有数据"      ← 无意义，不如直接 read()

  因此 libuv 选择:
    网络 → epoll/kqueue (真正异步)
    文件 → 线程池 + 阻塞 read() (模拟异步)

Linux 5.1+ 的 io_uring:
  ● 提供了真正的异步文件 I/O
  ● libuv 有实验性支持 (uv_loop_configure UV_LOOP_USE_IO_URING)
  ● 未来可能成为默认选项
```

### 5.5 libuv 事件循环的 C 语言视角

以下是 libuv 事件循环核心函数 `uv_run()` 的简化版本：

```c
/*
 * 简化的 uv_run() 实现 — 展示事件循环的骨架
 * 基于 libuv 源码 src/unix/core.c
 */
int
uv_run(uv_loop_t *loop, uv_run_mode mode)
{
    int timeout;
    int ran_pending;

    while(uv__loop_alive(loop)) {

        uv__update_time(loop);           /* 更新循环时钟 */

        /* 阶段 1: timers */
        uv__run_timers(loop);            /* 检查并执行到期的定时器 */

        /* 阶段 2: pending callbacks */
        ran_pending = uv__run_pending(loop); /* 执行推迟的 I/O 回调 */

        /* 阶段 3: idle, prepare */
        uv__run_idle(loop);
        uv__run_prepare(loop);

        /* 计算 poll 阶段应阻塞多久 */
        timeout = uv__backend_timeout(loop);
        /*
         * timeout 的计算逻辑:
         *   - 如果 loop 要停止 → 0 (不等待)
         *   - 如果有 idle handles → 0
         *   - 如果有 pending callbacks → 0
         *   - 如果有 close handles → 0
         *   - 如果有定时器 → 到最近定时器的剩余时间
         *   - 否则 → -1 (无限等待)
         */

        /* 阶段 4: poll — 这是花费时间最多的地方 */
        if(timeout != 0)
            uv__io_poll(loop, timeout);
        /*
         * uv__io_poll 的内部 (Linux):
         *   epoll_wait(loop->backend_fd, events, ARRAY_SIZE(events), timeout);
         *   for each ready event:
         *       watcher->cb(loop, watcher, event);  // 执行 I/O 回调
         */

        /* 阶段 5: check */
        uv__run_check(loop);             /* 执行 setImmediate 回调 */

        /* 阶段 6: close callbacks */
        uv__run_closing_handles(loop);   /* 执行 uv_close() 的回调 */

        /* UV_RUN_ONCE 模式下只跑一轮 */
        if(mode == UV_RUN_ONCE) {
            uv__update_time(loop);
            uv__run_timers(loop);        /* 再检查一次 timers */
            break;
        }

        /* UV_RUN_NOWAIT 模式下也只跑一轮 */
        if(mode == UV_RUN_NOWAIT)
            break;
    }

    return uv__loop_alive(loop);
}

/*
 * uv__loop_alive: 检查是否还有活跃的 handle 或待处理的 request
 */
static int
uv__loop_alive(const uv_loop_t *loop)
{
    return uv__has_active_handles(loop)
        || uv__has_active_reqs(loop)
        || loop->closing_handles != NULL;
}

/*
 * uv__run_timers: 从最小堆中弹出所有到期的定时器
 */
static void
uv__run_timers(uv_loop_t *loop)
{
    struct heap_node *heap_node;
    uv_timer_t *handle;

    for(;;) {
        heap_node = heap_min(timer_heap(loop));
        if(heap_node == NULL)
            break;

        handle = container_of(heap_node, uv_timer_t, heap_node);
        if(handle->timeout > loop->time)
            break;  /* 最近的定时器还没到期 */

        uv_timer_stop(handle);   /* 从堆中移除 */
        uv_timer_again(handle);  /* 如果是 interval，重新插入 */
        handle->timer_cb(handle); /* 执行回调 */
    }
}
```

---

## 模块六：综合应用与知识巩固

### 6.1 构建一个迷你事件循环

以下是一个极度简化的事件循环实现，展示核心概念：

```javascript
class MiniEventLoop {
    constructor() {
        this.timers = [];              // 定时器堆（简化为数组）
        this.immediates = [];          // setImmediate 队列
        this.nextTickQueue = [];       // process.nextTick 队列
        this.promiseQueue = [];        // Promise 微任务队列
        this.pendingIO = [];           // 模拟的 I/O 回调
        this.time = 0;                 // 逻辑时钟
    }

    // --- 注册 API ---

    setTimeout(fn, delay) {
        const timer = {
            callback: fn,
            expiry: this.time + delay,
            id: Symbol('timer'),
        };
        this.timers.push(timer);
        this.timers.sort((a, b) => a.expiry - b.expiry);
        return timer.id;
    }

    setImmediate(fn) {
        this.immediates.push(fn);
    }

    nextTick(fn) {
        this.nextTickQueue.push(fn);
    }

    enqueuePromise(fn) {
        this.promiseQueue.push(fn);
    }

    simulateIO(fn, latency) {
        const io = { callback: fn, readyAt: this.time + latency };
        this.pendingIO.push(io);
    }

    // --- 微任务清空 ---

    drainMicrotasks() {
        while (this.nextTickQueue.length > 0 || this.promiseQueue.length > 0) {
            // nextTick 优先
            while (this.nextTickQueue.length > 0) {
                const fn = this.nextTickQueue.shift();
                fn();
            }
            // 然后 Promise
            while (this.promiseQueue.length > 0) {
                const fn = this.promiseQueue.shift();
                fn();
            }
        }
    }

    // --- 事件循环 ---

    run() {
        console.log('=== Event loop started ===');

        while (this.isAlive()) {
            this.time++;

            // 阶段 1: Timers
            const expiredTimers = this.timers.filter(t => t.expiry <= this.time);
            this.timers = this.timers.filter(t => t.expiry > this.time);
            for (const timer of expiredTimers) {
                console.log(`  [timers] executing timer (was set for t=${timer.expiry})`);
                timer.callback();
                this.drainMicrotasks();
            }

            // 阶段 2-3: pending + idle/prepare (跳过)

            // 阶段 4: Poll — 处理就绪的 I/O
            const readyIO = this.pendingIO.filter(io => io.readyAt <= this.time);
            this.pendingIO = this.pendingIO.filter(io => io.readyAt > this.time);
            for (const io of readyIO) {
                console.log(`  [poll] executing I/O callback (ready at t=${io.readyAt})`);
                io.callback();
                this.drainMicrotasks();
            }

            // 阶段 5: Check — setImmediate
            const imms = [...this.immediates];
            this.immediates = [];
            for (const fn of imms) {
                console.log('  [check] executing setImmediate');
                fn();
                this.drainMicrotasks();
            }

            // 阶段 6: Close callbacks (跳过)
        }

        console.log('=== Event loop finished ===');
    }

    isAlive() {
        return this.timers.length > 0
            || this.immediates.length > 0
            || this.pendingIO.length > 0
            || this.nextTickQueue.length > 0
            || this.promiseQueue.length > 0;
    }
}

// 测试
const loop = new MiniEventLoop();

loop.setTimeout(() => {
    console.log('    timeout callback');
    loop.nextTick(() => console.log('    nextTick from timeout'));
    loop.enqueuePromise(() => console.log('    promise from timeout'));
}, 2);

loop.simulateIO(() => {
    console.log('    I/O callback');
    loop.setImmediate(() => console.log('    immediate from I/O'));
}, 1);

loop.setImmediate(() => console.log('    first immediate'));

loop.run();

/*
 * 输出:
 * === Event loop started ===
 *   [poll] executing I/O callback (ready at t=1)
 *     I/O callback
 *   [check] executing setImmediate
 *     first immediate
 *   [check] executing setImmediate
 *     immediate from I/O
 *   [timers] executing timer (was set for t=2)
 *     timeout callback
 *     nextTick from timeout
 *     promise from timeout
 * === Event loop finished ===
 */
```

### 6.2 async_hooks：追踪异步资源生命周期

`async_hooks` 模块可以追踪 Node.js 中每个异步资源的创建、触发和销毁。

```javascript
const async_hooks = require('async_hooks');
const fs = require('fs');

// 同步写入（因为在 async hook 回调中不能用异步 I/O）
function log(...args) {
    fs.writeFileSync(1, args.join(' ') + '\n');
}

const indent = new Map();
let indentLevel = 0;

const hook = async_hooks.createHook({
    init(asyncId, type, triggerAsyncId) {
        const spaces = ' '.repeat(indentLevel * 2);
        log(`${spaces}INIT  [${type}] asyncId=${asyncId} trigger=${triggerAsyncId}`);
        indent.set(asyncId, indentLevel);
    },

    before(asyncId) {
        indentLevel = (indent.get(asyncId) || 0) + 1;
        const spaces = ' '.repeat(indentLevel * 2);
        log(`${spaces}BEFORE asyncId=${asyncId}`);
    },

    after(asyncId) {
        const spaces = ' '.repeat(indentLevel * 2);
        log(`${spaces}AFTER  asyncId=${asyncId}`);
        indentLevel = Math.max(0, indentLevel - 1);
    },

    destroy(asyncId) {
        const level = indent.get(asyncId) || 0;
        const spaces = ' '.repeat(level * 2);
        log(`${spaces}DESTROY asyncId=${asyncId}`);
        indent.delete(asyncId);
    },
});

hook.enable();

// 触发一些异步操作
setTimeout(() => {
    log('--- timeout callback ---');

    fs.readFile(__filename, () => {
        log('--- readFile callback ---');
    });
}, 100);

/*
 * 输出 (简化):
 *
 * INIT  [Timeout] asyncId=2 trigger=1
 *   BEFORE asyncId=2
 *   --- timeout callback ---
 *   INIT  [FSREQCALLBACK] asyncId=3 trigger=2
 *     INIT  [FSREQCALLBACK] asyncId=4 trigger=3
 *   AFTER  asyncId=2
 *       BEFORE asyncId=4
 *       --- readFile callback ---
 *       AFTER  asyncId=4
 * DESTROY asyncId=2
 *     DESTROY asyncId=3
 *       DESTROY asyncId=4
 */
```

**AsyncLocalStorage（生产环境推荐）**：

`async_hooks` 的性能开销较大，不推荐直接在生产环境使用。`AsyncLocalStorage` 是基于 `async_hooks` 的高级抽象，性能更好：

```javascript
const { AsyncLocalStorage } = require('async_hooks');
const http = require('http');
const crypto = require('crypto');

const requestContext = new AsyncLocalStorage();

const server = http.createServer((req, res) => {
    const requestId = crypto.randomUUID();

    requestContext.run({ requestId, startTime: Date.now() }, () => {
        handleRequest(req, res);
    });
});

async function handleRequest(req, res) {
    log('Processing request');
    await queryDatabase();
    log('Database query complete');
    res.end('ok');
}

async function queryDatabase() {
    log('Executing query');
    await new Promise(r => setTimeout(r, 50));
    log('Query finished');
}

function log(message) {
    const ctx = requestContext.getStore();
    if (ctx) {
        const elapsed = Date.now() - ctx.startTime;
        console.log(`[${ctx.requestId.slice(0, 8)}] +${elapsed}ms ${message}`);
    } else {
        console.log(message);
    }
}

/*
 * 输出:
 * [a1b2c3d4] +0ms Processing request
 * [a1b2c3d4] +1ms Executing query
 * [f5e6d7c8] +0ms Processing request      ← 并发请求，不同 ID
 * [f5e6d7c8] +0ms Executing query
 * [a1b2c3d4] +52ms Query finished
 * [a1b2c3d4] +52ms Database query complete
 * [f5e6d7c8] +51ms Query finished
 * [f5e6d7c8] +51ms Database query complete
 */
```

### 6.3 设计模式实现：连接池

基于事件循环和 Promise 的高效连接池实现：

```javascript
const EventEmitter = require('events');

class ConnectionPool extends EventEmitter {
    #connections = [];   // 空闲连接
    #waiting = [];       // 等待获取连接的请求
    #active = 0;         // 正在使用的连接数
    #size;
    #factory;
    #destroyFn;

    constructor({ size = 10, create, destroy, validate }) {
        super();
        this.#size = size;
        this.#factory = { create, destroy, validate };
        this.#destroyFn = destroy || (conn => conn.end());
    }

    async acquire(timeoutMs = 5000) {
        // 尝试从空闲池获取
        while (this.#connections.length > 0) {
            const conn = this.#connections.pop();

            if (this.#factory.validate && !await this.#factory.validate(conn)) {
                await this.#destroyFn(conn);
                continue;
            }

            this.#active++;
            return conn;
        }

        // 如果还有容量，创建新连接
        if (this.#active < this.#size) {
            this.#active++;
            try {
                const conn = await this.#factory.create();
                return conn;
            } catch (err) {
                this.#active--;
                throw err;
            }
        }

        // 池满，等待释放
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                const idx = this.#waiting.findIndex(w => w.resolve === resolve);
                if (idx !== -1) this.#waiting.splice(idx, 1);
                reject(new Error(`Connection pool timeout after ${timeoutMs}ms`));
            }, timeoutMs);

            this.#waiting.push({
                resolve: (conn) => {
                    clearTimeout(timer);
                    resolve(conn);
                },
                reject,
            });
        });
    }

    release(conn) {
        // 如果有等待者，直接交接
        if (this.#waiting.length > 0) {
            const waiter = this.#waiting.shift();
            waiter.resolve(conn);
            return;
        }

        // 否则放回空闲池
        this.#active--;
        this.#connections.push(conn);
        this.emit('release');
    }

    async withConnection(fn) {
        const conn = await this.acquire();
        try {
            return await fn(conn);
        } finally {
            this.release(conn);
        }
    }

    async drain() {
        for (const conn of this.#connections) {
            await this.#destroyFn(conn);
        }
        this.#connections = [];

        for (const waiter of this.#waiting) {
            waiter.reject(new Error('Pool is draining'));
        }
        this.#waiting = [];
    }

    get stats() {
        return {
            total: this.#active + this.#connections.length,
            active: this.#active,
            idle: this.#connections.length,
            waiting: this.#waiting.length,
        };
    }
}

// 使用示例
const pool = new ConnectionPool({
    size: 5,
    create: async () => {
        console.log('Creating new connection');
        return { id: Math.random().toString(36).slice(2, 8), query: async (sql) => sql };
    },
    validate: async (conn) => {
        return conn.id !== undefined;
    },
    destroy: async (conn) => {
        console.log(`Destroying connection ${conn.id}`);
    },
});

async function main() {
    const results = await Promise.all(
        Array.from({ length: 10 }, (_, i) =>
            pool.withConnection(async (conn) => {
                console.log(`Task ${i} using connection ${conn.id}`);
                await new Promise(r => setTimeout(r, 100));
                return `result-${i}`;
            })
        )
    );

    console.log('Stats:', pool.stats);
    console.log('Results:', results.length);
    await pool.drain();
}

main().catch(console.error);
```

### 6.4 设计模式实现：背压控制流处理

背压（Backpressure）是流处理中最重要的概念之一。当数据生产速度超过消费速度时，必须有机制让生产者放慢脚步。

```
背压的本质:

  生产者(快) ───→ ████████████ ───→ 消费者(慢)
                   ↑ 缓冲区满了！

  如果不处理背压：
  ● 内存不断增长 → OOM (Out of Memory) 崩溃
  ● 数据丢失

  正确的背压处理：
  生产者(暂停) ─×→ ████████ ───→ 消费者(处理中)
                              │
                   缓冲区排空 ←─┘
                              │
  生产者(恢复) ──→ ████ ────→ 消费者(跟上了)
```

**Node.js Stream 中的背压机制**：

```javascript
const { Readable, Writable, Transform, pipeline } = require('stream');
const { promisify } = require('util');
const pipelineAsync = promisify(pipeline);

// 快速生产者
class DataProducer extends Readable {
    #count = 0;
    #max;

    constructor(max = 1000000) {
        super({ highWaterMark: 16384 }); // 内部缓冲 16KB
        this.#max = max;
    }

    _read(size) {
        // 持续推送数据直到内部缓冲满
        let pushMore = true;
        while (pushMore && this.#count < this.#max) {
            const chunk = `line ${this.#count}: ${'x'.repeat(100)}\n`;
            this.#count++;
            pushMore = this.push(chunk);
            // push() 返回 false 时表示内部缓冲已满
            // 此时 _read 应停止推送，等待 drain 后框架会再次调用 _read
        }
        if (this.#count >= this.#max) {
            this.push(null); // 结束信号
        }
    }
}

// 慢速消费者
class SlowConsumer extends Writable {
    #bytesWritten = 0;

    constructor() {
        super({ highWaterMark: 16384 });
    }

    _write(chunk, encoding, callback) {
        this.#bytesWritten += chunk.length;
        // 模拟慢速处理
        setTimeout(() => {
            callback(); // 告知框架可以接收下一块
        }, 1);
    }

    _final(callback) {
        console.log(`Total bytes processed: ${this.#bytesWritten}`);
        callback();
    }
}

// 转换流（带背压）
class UpperCaseTransform extends Transform {
    _transform(chunk, encoding, callback) {
        const upper = chunk.toString().toUpperCase();
        callback(null, upper);
    }
}

// 正确使用 pipeline（自动处理背压和错误）
async function run() {
    const start = Date.now();

    await pipelineAsync(
        new DataProducer(100000),
        new UpperCaseTransform(),
        new SlowConsumer()
    );

    console.log(`Pipeline complete in ${Date.now() - start}ms`);
}

run().catch(console.error);
```

**手动处理背压（pipe 的底层原理）**：

```javascript
const fs = require('fs');

// 手动版本 — 展示 pipe() 内部做了什么
function manualPipe(readable, writable) {
    return new Promise((resolve, reject) => {
        readable.on('data', (chunk) => {
            const canContinue = writable.write(chunk);
            if (!canContinue) {
                // 写入端缓冲满了 → 暂停读取（背压！）
                readable.pause();
                writable.once('drain', () => {
                    // 写入端缓冲排空 → 恢复读取
                    readable.resume();
                });
            }
        });

        readable.on('end', () => {
            writable.end();
        });

        writable.on('finish', resolve);
        readable.on('error', reject);
        writable.on('error', reject);
    });
}

// 使用
async function copyFile(src, dst) {
    const readable = fs.createReadStream(src);
    const writable = fs.createWriteStream(dst);
    await manualPipe(readable, writable);
}
```

```
背压的事件流:

  Readable                              Writable
     │                                     │
     ├── emit('data', chunk1) ──────────→ write(chunk1)
     │                                     │ 返回 true (缓冲未满)
     ├── emit('data', chunk2) ──────────→ write(chunk2)
     │                                     │ 返回 true
     ├── emit('data', chunk3) ──────────→ write(chunk3)
     │                                     │ 返回 false！(缓冲满了)
     ├── pause()  ← ── ── ── ── ── ──     │
     │   (停止读取)                         │ 慢慢消费...
     │   (等待...)                          │ 慢慢消费...
     │                                     │ 缓冲排空
     ├── ── ── ── ── ── ── ── ── ──→ emit('drain')
     ├── resume()                          │
     │   (恢复读取)                         │
     ├── emit('data', chunk4) ──────────→ write(chunk4)
     ...
```

### 6.5 综合问答

#### Q1: 为什么说 Node.js 适合 I/O 密集型而非 CPU 密集型应用？

```
I/O 密集型应用 (Node.js 擅长):
  Web 服务器、API 网关、实时聊天、文件代理...

  请求 A: [接收] [等待DB 50ms] [响应]
  请求 B:   [接收] [等待API 30ms] [响应]
  请求 C:     [接收] [等待FS 10ms] [响应]

  在等待 I/O 的 50ms 中，事件循环可以处理成百上千个其他请求。
  单线程 + 事件循环在这里比多线程更高效（无线程创建/切换开销）。

CPU 密集型应用 (Node.js 不擅长):
  图像处理、视频转码、科学计算、压缩...

  请求 A: [接收] [==== CPU 计算 500ms ====] [响应]
  请求 B:                                    [接收] [==== CPU 500ms ====] [响应]
  请求 C:                                                                 [等待...]

  JS 线程被计算占满，无法处理其他请求。
  对比多线程服务器：多核可以并行计算，不互相阻塞。

解决方案:
  ● worker_threads: 将 CPU 密集型任务卸载到独立线程
  ● child_process: 使用独立进程处理
  ● cluster 模块: 多进程利用多核
  ● 将计算密集型部分用 C/C++ 写成原生插件
```

#### Q2: `setImmediate` 和 `setTimeout(fn, 0)` 在事件循环的哪个阶段执行？

```
setTimeout(fn, 0):
  ● 实际延迟 ≥ 1ms (Node.js 内部最小延迟)
  ● 在 timers 阶段执行 (事件循环的第一个阶段)
  ● 底层实现: libuv 的最小堆定时器

setImmediate(fn):
  ● 在 check 阶段执行 (poll 阶段之后)
  ● 专为 "I/O 之后立即执行" 设计
  ● 底层实现: libuv 的 check handles

在 I/O 回调中:
  readFile → callback (poll) → check (setImmediate 先) → timers (setTimeout 后)
  顺序确定: setImmediate 总是先于 setTimeout

在主模块中:
  顺序不确定: 取决于事件循环启动时 1ms 是否已过
```

#### Q3: 如何诊断和修复一个持续增长的内存堆？

```
诊断步骤:

1. 确认泄漏存在
   $ node --max-old-space-size=512 app.js  # 限制堆大小，加速暴露问题
   $ node --trace-gc app.js                # 观察 GC 后内存是否持续增长

2. 获取堆快照
   a. 启动时: node --inspect app.js
   b. 运行时: kill -USR1 <pid>  (启用 inspector)
   c. Chrome DevTools → Memory → Take heap snapshot
   d. 间隔数分钟取两次快照

3. 分析快照
   比较两次快照 (Comparison 视图):
   ● 查看 "# New" 列 → 哪些对象在增长？
   ● 查看 "Size Delta" → 哪些对象占用内存增长最多？
   ● 点击对象 → "Retainers" → 谁在引用它？

4. 常见修复
   ┌─────────────────────────┬──────────────────────────────┐
   │ 泄漏模式                │ 修复方法                      │
   ├─────────────────────────┼──────────────────────────────┤
   │ 全局数组/Map 不断增长    │ 使用 LRU 缓存或 WeakMap      │
   │ EventEmitter 监听器累积  │ socket.on('close') 时 remove │
   │ 闭包持有大对象           │ 在不需要时置 null             │
   │ setInterval 未清理       │ 确保 clearInterval           │
   │ Promise 链过长           │ 控制并发数，避免无限队列      │
   └─────────────────────────┴──────────────────────────────┘

5. 持续监控 (生产环境)
   ● process.memoryUsage() 定期上报
   ● --max-old-space-size 设置合理上限
   ● 使用 clinic.js (npm) 自动诊断
```

```javascript
// 生产环境内存监控示例
const v8 = require('v8');

setInterval(() => {
    const mem = process.memoryUsage();
    const heap = v8.getHeapStatistics();
    const metrics = {
        rss:           (mem.rss / 1024 / 1024).toFixed(1) + ' MB',
        heapUsed:      (mem.heapUsed / 1024 / 1024).toFixed(1) + ' MB',
        heapTotal:     (mem.heapTotal / 1024 / 1024).toFixed(1) + ' MB',
        external:      (mem.external / 1024 / 1024).toFixed(1) + ' MB',
        heapUsagePct:  ((heap.used_heap_size / heap.heap_size_limit) * 100).toFixed(1) + '%',
    };

    if (parseFloat(metrics.heapUsagePct) > 80) {
        console.warn('MEMORY WARNING:', metrics);
        // 可以在这里触发堆快照或报警
    }
}, 30000);
```

---

## 全系列知识图谱

```
                        Node.js 运行时全景
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
     V8 Engine           libuv              Node.js Core
     (Part 2, 模块四)    (Part 3, 模块五)    (Part 1-3)
          │                   │                   │
     ┌────┴────┐        ┌────┴────┐        ┌─────┴─────┐
     │         │        │         │        │           │
   JIT      GC/堆    事件循环   线程池   异步范式    EventEmitter
   编译     管理       │         │      (Part 2,    (Part 2,
   (4.4)   (4.1-4.3)  │         │       模块三)     3.5)
                      │         │
              ┌───────┴───┐    │
              │           │    │
           6个阶段     微任务   │
           (Part 1,   (Part 1, │
            2.1)       2.2)    │
                              │
                    ┌─────────┴─────────┐
                    │                   │
                 网络 I/O            文件 I/O
                 (epoll/kqueue)     (线程池)
                 (5.4)              (5.4)
```

---

## 推荐深入阅读

| 资源 | 说明 |
|---|---|
| [Node.js 官方文档](https://nodejs.org/docs/latest/api/) | API 参考 |
| [libuv 设计概览](http://docs.libuv.org/en/v1.x/design.html) | libuv 架构文档 |
| [V8 博客](https://v8.dev/blog) | V8 引擎技术文章 |
| [Node.js 源码 `src/` 目录](https://github.com/nodejs/node/tree/main/src) | C++ 绑定层 |
| [libuv 源码](https://github.com/libuv/libuv/tree/v1.x/src) | 事件循环核心实现 |
| [clinic.js](https://clinicjs.org/) | Node.js 性能诊断工具 |
| [0x](https://github.com/davidmarkclements/0x) | 火焰图分析工具 |
