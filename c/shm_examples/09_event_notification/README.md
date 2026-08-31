# 09. Event Notification（共享内存 + 事件通知）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 10 节。

## 心智模型

前面 01~08 的所有示例都靠**忙等（busy-loop / spin）**发现"数据变了"——
简单、延迟低，但会持续占用一个 CPU 核心空转。当消费者大部分时间都在
"等"而不是"干活"时，这种浪费在真实系统里是不可接受的。

Event Notification 要解决的问题是：**共享内存本身没有"叫醒对方"的能力**
（它只是一段内存，没有阻塞/唤醒语义），必须借助内核提供的另一种 IPC
原语，让等待方真正睡眠、不占 CPU，等有数据时再被唤醒。本节用三种不同
的"叫醒"机制演示同一个思路，并刻意选择了三种不同的进程关系来突出
每种机制的适用边界：

| 子示例 | 唤醒机制 | 进程关系 | 关键 API |
|---|---|---|---|
| `eventfd/` | 内核计数器 + fd | 父子进程（fork 继承 fd） | `eventfd()`, `read()`, `write()` |
| `sem/` | 命名信号量 | 两个完全独立的进程 | `sem_open()`, `sem_post()`, `sem_wait()` |
| `futex/` | 用户态字 + 内核睡眠队列 | 两个完全独立的进程 | `futex(FUTEX_WAIT/WAKE)`（原始系统调用）|

## 三种机制怎么选

- **eventfd**：只有在“有父子关系（或能互相传递 fd，比如通过 Unix
  Domain Socket）”时才好用，因为它靠文件描述符表达身份，不像命名对象
  一样自带一个全局可见的名字。换来的好处是可以直接塞进 `epoll`，
  和网络 I/O 事件统一处理——常见于事件驱动服务器把"内部任务完成"
  和"socket 可读"用同一个 `epoll_wait` 一起等。
- **sem_open 命名信号量**：不需要任何父子关系，两个进程各自
  `sem_open` 同一个名字即可握手，天生就是"计数型"信号（`sem_post`
  可以连续调用多次，对应多个待处理项）。缺点是不能塞进 `epoll`（在
  Linux 上信号量本身不是一个可 poll 的 fd）。
- **futex**：最底层、最轻量——glibc 的 mutex/条件变量、甚至
  `sem_open` 的实现内部最终都会落到 futex。它的杀手级特性是
  "快速路径完全在用户态用原子 CAS 完成，没有竞争时不进内核系统调用"，
  只有真正需要等待时才陷入内核睡眠。代价是接口最原始（本示例手写了
  `futex_wait_until_eq()` 封装 CAS + `syscall(SYS_futex, ...)`），
  实践中大多数场景应该直接用 `pthread_mutex_t`
  (`PTHREAD_PROCESS_SHARED`，参见 04_hash_table) 或 `sem_open`，
  只有在追求极致性能、需要自定义等待协议时才会直接手写 futex。

## eventfd/：父子进程，fork 天然共享 fd

`fork()` 会复制文件描述符表，父子进程持有同一个内核 eventfd 对象的
两份引用，不需要任何额外传递手段。本示例用 `EFD_SEMAPHORE` 模式，
让每次 `read()` 只消耗计数器里的 1（行为等价于信号量），子进程借此
知道"又有一个 item 可以消费了"。

```bash
cd eventfd
make
./run_demo.sh
```

预期输出：

```text
[eventfd child] consumed=20000 mismatches=0
[eventfd parent] produced=20000 items
```

## sem/：两个独立进程，命名信号量握手

`writer` 与 `reader` 是两个完全独立启动的二进制（没有 fork
关系），各自通过同一个名字 `/shm_examples_sem_ready`
`sem_open()`，即可跨进程同步——这是"跨越没有亲缘关系的进程"场景下
最简单的选择。

```bash
cd sem
make
./run_demo.sh
```

预期输出：

```text
[sem writer] produced=20000 items
[sem reader] consumed=20000 mismatches=0
```

## futex/：两个独立进程，共享内存里的裸 futex word

共享内存里放两个 `_Atomic uint32_t`：`slot_free`（槽位空闲）与
`data_ready`（数据待取），用 CAS 实现单槽位握手协议：

```text
writer: 等 slot_free 1->0（CAS，失败则 futex_wait 睡眠）
        写入数据
        data_ready 0->1，futex_wake 唤醒 reader

reader: 等 data_ready 1->0（CAS，失败则 futex_wait 睡眠）
        读取并校验数据
        slot_free 0->1，futex_wake 唤醒 writer
```

`futex_wait(addr, expected)` 会原子地检查 `*addr == expected` 是否
仍成立——不成立立即返回（说明对端已经又更新了，绝不会错过一次唤醒），
成立才真正陷入内核睡眠，这正是 futex 避免"检查和睡眠之间发生竞争"
（经典的 lost-wakeup 问题）的关键设计。

```bash
cd futex
make
./run_demo.sh
```

预期输出：

```text
[futex writer] shared memory ready, producing 20000 items
[futex writer] done, produced=20000 items
[futex reader] consumed=20000 mismatches=0
```

## 一次性运行全部三个子示例

```bash
make
./run_demo.sh
```

三个子示例都会打印 `mismatches=0`，证明无论用哪种唤醒机制，数据在
"写完 - 通知 - 唤醒 - 读取"这条链路上都没有丢失或损坏。
