# 01. Ring Buffer（SPSC，单生产者单消费者）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 2 节。

## 心智模型

- **问题**：一个进程持续产出数据，另一个进程持续消费，如何做到不加锁、不 malloc、cache friendly？
- **核心技巧**：
  - 用两个原子下标 `head`（写位置）/`tail`（读位置）代替锁；因为只有一个 writer 改 `head`、
    只有一个 reader 改 `tail`，所以只需要 `memory_order_acquire/release`，不需要 CAS。
  - `capacity` 取 2 的幂，用 `idx & (capacity - 1)` 代替 `% capacity`，避免除法。
  - 故意牺牲一个槽位来区分"空"和"满"：`head == tail` 为空，`(head+1) & mask == tail` 为满。
- **适用场景**：DPDK `rte_ring`（单核对单核）、音视频采集→处理、网络收包队列。
- **常见坑**：
  - `head`/`tail` 必须用 `_Atomic` + 正确的内存序，否则在弱内存序架构（ARM）上会看到"写了但还没写完"的数据。
  - 多消费者/多生产者场景不能直接套用这个实现，见 [../02_lockfree_queue_mpmc](../02_lockfree_queue_mpmc/)。

## 数据结构

```c
struct ring_buffer {
    _Atomic uint32_t head;   /* 生产者独占写 */
    _Atomic uint32_t tail;   /* 消费者独占写 */
    uint32_t capacity;
    struct ring_slot slots[RING_CAPACITY];
};
```

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 会在后台启动 producer、同步运行 consumer，等待两者都结束
后再退出。也可以手动分别运行两个终端观察：

```bash
make

# 终端 1（先跑生产者，后台运行）
./producer 200000 &

# 终端 2（消费者会自动等待共享内存出现）
./consumer
```

## 预期输出

```text
[producer] ring created, capacity=1024, count=200000
[producer] done, pushed 200000 items + 1 sentinel
[consumer] attached, capacity=1024
[consumer] got sentinel at seq=200000, stop
[consumer] received 200000 items, integrity=OK
```

`integrity=OK` 表示消费者收到的序列号严格连续、无丢失、无重复，验证了 SPSC 无锁环形缓冲区的正确性。
