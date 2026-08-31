# 03. Shared Object Pool（共享对象池 / Free List）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 4 节。

## 心智模型

- **问题**：共享内存里没有堆，`malloc()`/`free()` 分配的地址在别的进程里毫无意义。
  那高频分配/释放"对象"（比如网络包描述符、数据库缓冲页）怎么做到 O(1) 且无锁？
- **核心技巧 1**：预先在共享内存里分配好一大块固定大小的对象数组，
  分配/释放变成"从一个 Free List 上摘一个下标 / 挂回一个下标"，
  彻底不走 `malloc`/`free`。
- **核心技巧 2（最容易被忽略的一点）**：Free List 绝不能用裸指针
  （`next` 字段存指针）——因为共享内存在不同进程里的映射基址不同，
  A 进程里的合法指针在 B 进程里可能越界或指向别的东西。
  必须用**数组下标（等价于 offset）**代替指针。
- **核心技巧 3**：单链表 + CAS 的 Free List（Treiber Stack）在高并发下有
  经典的 **ABA 问题**：线程 A 读到 `head=X, next=Y` 后被换出；期间 X 被
  别的线程 pop 出去又 push 回来（此时 X 的 `next` 可能已经指向别的节点）；
  A 醒来后的 CAS 却"看起来"成功，实际上链表已经损坏。
  解决办法是给 `head` 附加一个只增不减的 **generation 计数器**，
  和下标一起打包进同一个 64 位原子变量做 CAS（tagged index）。
- **适用场景**：DPDK `rte_mempool`、SPDK Buffer Pool、数据库 Buffer Pool。

## 数据结构

```c
struct object_slot {
    uint32_t next;              /* 空闲时：free list 链接（下标，不是指针） */
    _Atomic uint32_t in_use;    /* 调试用：检测重复分配 */
    uint64_t payload;
};

struct object_pool {
    _Atomic uint64_t head_tagged;  /* 高32位 generation + 低32位 index，一起 CAS */
    struct object_slot slots[POOL_CAPACITY];
};
```

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 会：

1. `init` 创建一个容量仅为 4 的对象池（故意设小，方便触发"池已耗尽 -> 重试"路径）。
2. 启动 8 个 worker 进程并发执行 20000 次 `alloc -> 使用 -> free`。
3. 每次 alloc 后用 `atomic_exchange` 检测 `in_use` 是否已经是 1
   （如果是，说明两个进程同时拿到了同一个对象——Free List 有 bug）。
4. `verify` 校验：`total_alloc == total_free == 8*20000`，且 `double_alloc_bugs == 0`。

## 预期输出（节选）

```text
[init] object pool created, capacity=4
[worker 0] done, iterations=20000 pool_exhausted_hits=69145
...
[verify] expected_ops=160000 total_alloc=160000 total_free=160000 double_alloc_bugs=0 pool_exhausted_events=591193 result=OK
```

`pool_exhausted_events` 数值很大是预期的：8 个 worker 同时抢 4 个对象，
大部分时间池是空的，worker 会自旋重试——这正好演示了对象池在"并发需求
超过容量"时的正常退化行为（而不是 bug）。真正需要关注的是
`double_alloc_bugs=0`：证明无锁 Free List 在高并发下依然保证
"同一时刻一个对象只会被一个 worker 持有"。
