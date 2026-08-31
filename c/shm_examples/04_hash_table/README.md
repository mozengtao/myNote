# 04. Shared Hash Table（共享哈希表，Bucket Lock）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 5 节。

## 心智模型

- **问题**：多个进程要共享一张 Session/Cache/Flow Table，既要保证并发安全，
  又不想让"全表一把大锁"变成瓶颈。
- **核心技巧 1**：链地址法哈希表，但每个 bucket 拥有**独立**的
  `pthread_mutex_t`。只有 hash 到同一个 bucket 的操作才会互斥，
  不同 bucket 完全并行——这是"减少锁竞争"的关键。
- **核心技巧 2**：跨进程互斥锁必须显式设置
  `pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)`，
  否则跨进程加锁是未定义行为（glibc 默认属性只保证同进程线程间可用）。
- **核心技巧 3**：链表节点复用 [../03_object_pool](../03_object_pool/) 里
  "无锁 tagged-index Free List" 的技巧来分配/回收——同一个心智模型
  在不同设计里反复出现：**共享内存里凡是"链表"，节点分配都不能用 malloc，
  链接都不能用裸指针**。
- **适用场景**：Session 表、Cache、NAT Table、Flow Table、DNS Cache。
- **常见坑**：如果不用 bucket lock 而是"整表一把锁"，在 8 核以上机器上
  哈希表会退化成串行访问，完全失去并发优势。

## 数据结构

```c
struct hash_node {
    uint32_t next;   /* 同一个 bucket 内的下一个节点下标 */
    uint64_t key;
    uint64_t value;
};

struct shared_hash_table {
    pthread_mutex_t bucket_lock[NUM_BUCKETS];   /* PTHREAD_PROCESS_SHARED */
    uint32_t bucket_head[NUM_BUCKETS];
    _Atomic uint64_t node_pool_head_tagged;     /* 复用 03 的无锁 Free List */
    struct hash_node nodes[MAX_NODES];
};
```

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 依次完成：

1. `init` 创建 64 个 bucket 的哈希表。
2. 8 个 writer 进程并发插入互不重叠的 key 区间（共 8000 个 key，
   value = key*2），大概率会有多个 writer 落到同一个 bucket，
   真实触发 bucket 锁竞争。
3. 4 个 reader 进程并发查询**同一段**完整 key 区间，验证并发读的正确性。
4. 1 个 deleter 进程删除所有 `key % 3 == 0` 的条目。
5. `verify` 扫描整个区间，确认被删除的 key 查不到、其余 key 值仍然正确。

## 预期输出（节选）

```text
[init] hash table created, buckets=64 max_nodes=16384
--- writers ---
[writer 0] inserted 1000 keys
...
--- concurrent readers (full range, overlapping) ---
[reader 0] range=[0,8000) hits=8000 misses=0 mismatches=0
...
--- deleter (removes key % 3 == 0) ---
[deleter] deleted 2667 keys (key % 3 == 0) in [0,8000)
--- final verify ---
[verify] range=[0,8000) errors=0 insert=8000 delete=2667 lookup_hit=37333 lookup_miss=2667 value_mismatch=0 result=OK
```

`errors=0` 说明：8 个进程并发写入、4 个进程并发读取、1 个进程并发删除，
整个过程中 bucket 锁始终保证了链表结构不被破坏，且节点 Free List 的
分配/回收也没有产生野指针或重复使用。
