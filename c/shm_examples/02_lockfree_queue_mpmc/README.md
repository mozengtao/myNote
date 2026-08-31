# 02. Lock-Free Queue（MPMC，多生产者多消费者）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 3 节。

## 心智模型

- **问题**：多个进程同时往同一个队列写，多个进程同时从同一个队列读，
  谁都可能在任意时刻被调度打断，如何不用 mutex 也不会互相踩到同一个槽位？
- **核心技巧**（Dmitry Vyukov 的经典 bounded MPMC 算法）：
  - 每个槽位除了数据外，多带一个 `sequence` 字段。
  - `enqueue`：用 CAS 争抢一个全局递增的 `enqueue_pos`；只有当
    `slot.sequence == pos` 时才说明"这个槽位空，且轮到你"，CAS 成功后才
    真正写数据，最后把 `slot.sequence` 置为 `pos + 1` 标记"数据就绪"。
  - `dequeue` 与之对称：只有 `slot.sequence == pos + 1` 才说明"数据已就绪"。
  - 全程只有一次 CAS + 少量原子读写，没有锁，也没有等待队列。
- **关键澄清（很容易搞错的一点）**：这种队列只保证
  **每个元素恰好被消费一次（不丢、不重）**，**不保证**多个并发消费者进程
  "完成业务处理"的先后顺序与生产顺序完全一致——槽位的"认领顺序"是严格
  递增的，但认领之后各消费者进程处理耗时不确定，可能被调度打乱。
  本示例最初尝试按"到达时间顺序"校验，会看到假阳性的 `OUT-OF-ORDER`；
  正确的校验方式是用 bitmap 检查"是否恰好被消费一次"，而不是检查顺序。
- **适用场景**：高频交易、DPDK、SPDK、多进程消息队列。
- **避免使用**：mutex、semaphore（那是下一代设计要解决的吞吐瓶颈）。

## 数据结构

```c
struct queue_cell {
    _Atomic size_t sequence;
    uint64_t data;
};

struct mpmc_queue {
    struct queue_cell buffer[QUEUE_CAPACITY];
    _Atomic size_t enqueue_pos;
    _Atomic size_t dequeue_pos;
    _Atomic uint32_t active_producers;   /* 用于生产者协调"最后一个退出的人发哨兵" */
    _Atomic uint8_t  seen_bitmap[...];   /* 仅用于本示例的正确性校验 */
};
```

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 会：

1. 用 `init` 创建并初始化共享队列（4 个生产者、2 个消费者）。
2. 启动 4 个生产者进程，每个各写 50000 个 `(producer_id, seq)`。
3. 启动 2 个消费者进程并发读取，直到收到哨兵值退出。
4. 最后一个完成的生产者负责推入 `num_consumers` 个哨兵，通知所有消费者停止。
5. 用 `verify` 校验：收发总数一致、无重复消费、无遗漏。

## 预期输出（节选）

```text
[init] queue created: capacity=4096 producers=4 consumers=2
[producer 0] pushed 50000 items
...
[producer 3] pushed 50000 items
[producer 3] last producer, pushing 2 sentinels
[consumer 0] done, received 102459 items
[consumer 1] done, received 97541 items
[verify] expected_total=200000 actual_total=200000 duplicates=0 missing=0 result=OK
```

两个消费者收到的数量之和恒等于 200000，`duplicates=0 missing=0` 说明每个元素
都恰好被消费一次——这正是无锁 MPMC 队列要保证的核心正确性，而不是"谁先来
后到"的顺序。
