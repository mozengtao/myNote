# 高性能 Shared Memory 的常用典型设计

在 Linux、DPDK、SPDK、数据库、高频交易（HFT）、Redis 等高性能系统中，Shared Memory 的设计模式已经比较成熟。大多数系统不会简单地将结构体直接放入共享内存，而是采用一些经过长期实践验证的数据结构和架构模式。

---

# 1. Shared Memory 总体架构

典型的共享内存一般划分为三个区域：

```text
+-------------------------------------------------------+
| Shared Memory                                          |
|                                                       |
| +------------------+                                  |
| | Metadata/Header  |                                  |
| +------------------+                                  |
|                                                       |
| +------------------+                                  |
| | Control Area     |                                  |
| | Read Index       |                                  |
| | Write Index      |                                  |
| | Version          |                                  |
| | State            |                                  |
| +------------------+                                  |
|                                                       |
| +------------------+                                  |
| | Data Area        |                                  |
| | Ring Buffer      |                                  |
| | Queue            |                                  |
| | Hash Table       |                                  |
| +------------------+                                  |
+-------------------------------------------------------+
```

通常包括：

- **Header（元数据）**
- **Control（控制信息）**
- **Data（业务数据）**

---

# 2. Ring Buffer（最经典）

适用于：

- 单生产者单消费者（SPSC）
- 多生产者多消费者（MPSC/MPMC）

结构如下：

```text
Producer

      │

      ▼

 Write Index

+--------------------------------+
| Ring Buffer                    |
|                                |
| 0 1 2 3 4 5 6 7 ...            |
|                                |
+--------------------------------+

 Read Index

      ▲

Consumer
```

控制结构通常只有：

```c
struct ring_ctrl {
    atomic_uint head;
    atomic_uint tail;
};
```

特点：

- O(1)
- 顺序访问
- Cache Friendly
- 无 malloc/free
- 非常适合流水线处理

典型应用：

- DPDK rte_ring
- io_uring SQ/CQ
- Linux perf Ring Buffer
- 视频流
- 网络报文队列

---

# 3. Lock-Free Queue

适用于：

- 多 Producer
- 多 Consumer

结构：

```text
Producer1

Producer2

Producer3

      │

      ▼

 Lock-free Queue

      ▲

Consumer1

Consumer2
```

通常采用：

- Atomic
- CAS
- Memory Barrier

避免：

- mutex
- semaphore

典型：

- HFT
- DPDK
- SPDK
- 消息队列

---

# 4. Shared Object Pool（共享对象池）

共享的不是数据，而是对象。

```text
+--------------------------------+

Object Pool

--------------------------------

Object1

Object2

Object3

...

ObjectN

--------------------------------

Free List

+--------------------------------+
```

对象分配：

```
pop freelist
```

对象释放：

```
push freelist
```

避免：

- malloc()
- free()

典型：

- DPDK rte_mempool
- SPDK Buffer Pool
- 数据库 Buffer Pool

---

# 5. Shared Hash Table

多个进程共享一张 Hash 表。

```text
+----------------+

Bucket0

Bucket1

Bucket2

...

+----------------+
```

适用于：

- Session
- Cache
- NAT Table
- Flow Table
- DNS Cache

为了减少锁竞争：

通常采用：

- Bucket Lock
- RCU
- Lock-Free Hash

而不是整张 Hash 加一把大锁。

---

# 6. Producer-Consumer Pipeline（流水线）

例如视频处理：

```text
Capture

↓

Shared Memory

↓

Decoder

↓

Shared Memory

↓

Encoder

↓

Shared Memory

↓

Display
```

每一级：

都是一个 Ring Buffer。

特点：

- 零拷贝
- 高吞吐
- 模块解耦

典型：

- 视频
- AI 推理
- 网络包处理

---

# 7. Shared Configuration（共享配置）

一个配置，多进程共享。

```text
Config Process

↓

Shared Memory

↓

Worker1

Worker2

Worker3
```

特点：

- Write Once
- Read Many

通常：

```c
struct Config {

    uint32_t version;

    ...

};
```

Worker：

```
Version变化？

↓

Reload
```

---

# 8. Snapshot（快照）

读写完全分离。

Writer：

```
Current Data

↓

New Snapshot
```

完成以后：

```
Atomic Swap Pointer
```

Reader：

始终：

```
Read Snapshot
```

特点：

- Reader 无锁
- Writer 不影响 Reader

数据库大量采用。

---

# 9. Double Buffer（双缓冲）

结构：

```text
Producer

↓

Buffer A

Buffer B

↑

Consumer
```

Producer：

一直写 Buffer A。

完成：

```
Atomic Swap
```

Consumer：

一直读 Buffer B。

适用于：

- GUI
- 视频
- GPU
- 行情系统

---

# 10. Shared Memory + Event Notification

Shared Memory 只负责共享数据。

通知机制独立。

例如：

```
Shared Memory

+

eventfd
```

或者：

```
Shared Memory

+

futex
```

或者：

```
Shared Memory

+

Semaphore
```

流程：

```text
Producer

↓

Write Data

↓

eventfd_write()

↓

Consumer Wakeup
```

典型：

- io_uring
- DPDK
- Linux IPC

---

# 11. Shared Memory + HugePage

适合：

大内存。

例如：

```
2MB HugePage

↓

Shared Memory

↓

多个Process
```

优点：

减少：

- TLB Miss
- Page Walk

典型：

- DPDK
- SPDK
- Oracle
- ClickHouse

---

# 12. NUMA-Aware Shared Memory

服务器：

```text
CPU0

Node0 Memory

CPU1

Node1 Memory
```

优化方式：

```
Node0

↓

Shared Memory0

Node1

↓

Shared Memory1
```

原则：

CPU 尽量访问本地 NUMA 内存。

减少：

Remote Memory Access。

---

# 13. Header + Version + Magic（工业界必备）

共享内存通常都会有 Header。

例如：

```c
struct shm_header {

    uint32_t magic;

    uint32_t version;

    uint32_t length;

    uint32_t crc;

};
```

作用：

| 字段 | 作用 |
|------|------|
| Magic | 判断共享内存是否合法 |
| Version | 版本兼容 |
| Length | 数据长度 |
| CRC | 数据完整性 |

大型系统还会增加：

```c
struct shm_header {

    uint32_t magic;

    uint16_t major_version;

    uint16_t minor_version;

    uint64_t sequence;

    uint64_t timestamp;

    uint32_t flags;

};
```

---

# 工业界最常见的组合

真正的高性能系统一般采用如下组合：

```text
                Shared Memory
                      │
        ┌─────────────┴─────────────┐
        │                           │
     Header                     Data Area
        │                           │
   Magic / Version           Ring Buffer
   Sequence                  Object Pool
   CRC                       Hash Table
        │                           │
        └─────────────┬─────────────┘
                      │
               Atomic / Memory Barrier
                      │
          eventfd / futex / semaphore
```

---

# 总结

| 设计原则 | 核心思想 | 常见技术 |
|-----------|----------|----------|
| 数据布局 | Header + Control + Data 分层 | Header、Control Block、Data Area |
| 顺序访问 | 提高 Cache 命中率 | Ring Buffer |
| 无锁或少锁 | 避免内核同步开销 | Atomic、CAS、Lock-Free Queue |
| 对象复用 | 避免 malloc/free | Object Pool、Slab |
| 读写解耦 | Reader 与 Writer 分离 | Double Buffer、Snapshot、RCU |
| 不共享裸指针 | 使用 Offset 替代 Pointer | Offset Table |
| 数据与通知解耦 | 共享内存负责数据，事件负责唤醒 | eventfd、futex、Semaphore |
| Cache 优化 | 减少 False Sharing | Cache Line 对齐、Padding |
| 可靠性 | 支持恢复和版本兼容 | Magic、Version、Sequence、CRC |
| 硬件优化 | 利用 CPU 特性 | HugePage、NUMA、Prefetch |

---

# 一句话总结

高性能 Shared Memory 的核心思想可以概括为：

> **共享内存负责数据共享，Atomic 保证一致性，Ring Buffer 负责流式传输，Object Pool 管理对象生命周期，Event Notification 完成跨进程唤醒，Header 提供可靠性与版本管理，最终构建出零拷贝、低延迟、高吞吐的多进程通信架构。**