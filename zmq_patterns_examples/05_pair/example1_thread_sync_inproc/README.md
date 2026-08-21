# 示例 1：inproc PAIR 线程间控制信号

主线程通过 `inproc://` + PAIR socket 控制一个工作线程：发 `START` 启动任务，工作线程一边"干活"一边汇报进度，主线程收到第 3 个进度后发 `STOP` 提前终止，工作线程收到后立刻停止并汇报 `DONE`。

## 场景

这是官方文档里 PAIR **最推荐**的用法：同一进程内，两个线程之间需要一个简单、双向、不需要考虑网络问题的控制通道，比 `pthread` 条件变量/互斥锁的写法更直观（消息就是消息，不用手动管理锁）。

## 心智模型

```
Main Thread (PAIR, bind inproc://control)
     │
     │──send "START"──────────────▶│
     │◀──recv "PROGRESS 0"─────────│  Worker Thread (PAIR, connect inproc://control)
     │◀──recv "PROGRESS 1"─────────│
     │◀──recv "PROGRESS 2"─────────│
     │◀──recv "PROGRESS 3"─────────│
     │──send "STOP"───────────────▶│  (工作线程检测到STOP，提前退出)
     │◀──recv "DONE (stopped)"─────│
```

**重要心智**：`inproc://` 要求**先 bind 后 connect**，且两端必须共享**同一个 Context**（`inproc` 是纯内存传递，不经过网络栈，天然要求双方在同一进程内）。

## 编译 & 运行（C）

```bash
make
./main
```

（不需要分开起两个进程——这个示例整个就是一个可执行文件，内部用 `pthread` 起了一个工作线程。）

## 运行（Python）

```bash
python3 main.py
```

（内部用 `threading.Thread` 起工作线程，用法与 C 版一一对应。）

## 预期输出

```
[Main] Sent START
[Worker] Received START, beginning work...
[Worker] Progress 0/10
[Main] Progress: 0/10
[Worker] Progress 1/10
[Main] Progress: 1/10
[Worker] Progress 2/10
[Main] Progress: 2/10
[Worker] Progress 3/10
[Main] Progress: 3/10
[Main] Got progress 3, sending STOP early
[Worker] Received STOP signal, exiting early
[Main] Worker done: DONE (stopped early at progress 3)
```

## 关键点

- PAIR 两端完全对等，**谁都可以先发**——本例里 Worker 主动汇报进度、Main 主动发 STOP，都不需要"等对方先问"。
- 如果需要 3 个以上的线程互相通信，PAIR 就不够用了（1对1限制），应该考虑用一个中心线程 + 多个 PAIR，或者直接换用 ROUTER/DEALER 做多路复用。
