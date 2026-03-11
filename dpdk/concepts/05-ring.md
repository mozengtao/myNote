# DPDK 深度解析 (5/9)：Ring — 无锁环形队列

---

## 1. 痛点 (The "Why")

### 传统队列的多核困境

```
┌─────────────────────────────────────────────────────────┐
│         Performance Bottlenecks of Locked Queues         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Scenario: Multiple CPU cores exchange data via queue    │
│                                                         │
│  Option A: mutex/spinlock-protected linked-list queue   │
│  ┌───────┐  ┌───────┐  ┌───────┐                       │
│  │Core 0 │  │Core 1 │  │Core 2 │                       │
│  │ SPIN  │  │ SPIN  │  │ LOCK  │ ← Only one core can   │
│  └───┬───┘  └───┬───┘  └───┬───┘   operate at a time  │
│      │          │          │                             │
│      └──────────┴──────────┘                             │
│                 │                                        │
│           ┌─────▼─────┐                                  │
│           │  Queue    │                                  │
│           │ (locked)  │                                  │
│           └───────────┘                                  │
│                                                         │
│  Problems:                                               │
│  ① Spinlock spinning wastes CPU cycles                   │
│  ② Lock holder preempted → all others stall (priority    │
│     inversion)                                           │
│  ③ Cache line bouncing: lock var invalidated across     │
│     cores repeatedly                                     │
│  ④ Linked nodes scattered in memory → cache-unfriendly  │
│  ⑤ Every enqueue needs malloc, dequeue needs free        │
│                                                         │
│  Option B: Linux kernel kfifo                            │
│  - Fixed-size ring buffer, but still needs spinlock      │
│  - Only supports single-producer/single-consumer lockless│
│                                                         │
│  DPDK Ring breakthrough:                                 │
│  ✅ MP/MC (multi-producer/multi-consumer) lockless ops   │
│  ✅ Cache-line aligned contiguous array → highly cache-  │
│     friendly                                             │
│  ✅ Fixed size → no dynamic allocation                   │
│  ✅ Burst operations → amortize memory barrier cost      │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> 图解说明：传统加锁队列在多核场景下的性能瓶颈。方案 A 为 spinlock 保护的链表，多核争用导致自旋、优先级反转和 cache line 抖动；方案 B 的 kfifo 虽有环形缓冲但仍需锁；DPDK Ring 通过无锁 MP/MC、cache 对齐和批量操作实现突破。

---

## 2. 核心概念与架构

### 2.1 Ring 内存布局

```
 struct rte_ring memory layout:

 ┌─────────────────────────────────────────────────────────┐
 │ Cache Line 0 (64 bytes)         — Ring metadata          │
 │ ┌────────────────────────────────────────────────────┐  │
 │ │ name[32]  │ flags │ size │ mask │ capacity │ ...   │  │
 │ └────────────────────────────────────────────────────┘  │
 ├─────────────────────────────────────────────────────────┤
 │ Cache Line 1 (64 bytes)         — Producer               │
 │ ┌────────────────────────────────────────────────────┐  │
 │ │       prod.head     │      prod.tail               │  │
 │ │   (producer head)   │   (producer tail)            │  │
 │ │                     │                              │  │
 │ │  padding to 64B ... (avoid false sharing w/ cons)   │  │
 │ └────────────────────────────────────────────────────┘  │
 ├─────────────────────────────────────────────────────────┤
 │ Cache Line 2 (64 bytes)         — Consumer               │
 │ ┌────────────────────────────────────────────────────┐  │
 │ │       cons.head     │      cons.tail               │  │
 │ │   (consumer head)   │   (consumer tail)             │  │
 │ │                     │                              │  │
 │ │  padding to 64B ... (avoid false sharing w/ prod)   │  │
 │ └────────────────────────────────────────────────────┘  │
 ├─────────────────────────────────────────────────────────┤
 │ Cache Line 3+  —  Data slots (void *ring[size])          │
 │ ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┐     │
 │ │  [0] │  [1] │  [2] │  [3] │  [4] │  ... │[N-1] │     │
 │ │ ptr  │ ptr  │ ptr  │ ptr  │ ptr  │      │ ptr  │     │
 │ └──────┴──────┴──────┴──────┴──────┴──────┴──────┘     │
 │                                                         │
 │  size must be power of 2 (bitwise mod: idx & mask)       │
 └─────────────────────────────────────────────────────────┘

 Key design:
 - prod and cons on separate cache lines → no prod/cons false sharing
 - Contiguous data region → efficient CPU prefetch
 - Stores pointers only (void *) → lightweight
```

> 图解说明：struct rte_ring 的内存布局。Cache Line 0 为元数据；Cache Line 1/2 分别为生产者与消费者指针，分离放置以消除伪共享；Cache Line 3 起为数据槽位数组，size 需为 2 的幂以便用 idx & mask 做模运算。

### 2.2 多生产者入队 (MP Enqueue) 算法

```
 CAS (Compare-And-Swap) lockless algorithm:

 Initial state: prod.head = prod.tail = 3

 Step 1: Read prod.head (3), compute new head (4)
 Step 2: CAS(&prod.head, 3, 4) — atomically change head 3→4
         If another core already changed it → CAS fails → retry Step 1
 Step 3: Write data to ring[3]
 Step 4: Wait prod.tail == 3 (ensure prior enqueues completed)
 Step 5: Update prod.tail = 4

 Timeline of two producers enqueuing concurrently:

 Core A                          Core B
 ─────                          ─────
 read prod.head = 3              read prod.head = 3
 CAS(head, 3→4) ✅ success      CAS(head, 3→4) ❌ fail → retry
 write ring[3] = obj_A          read prod.head = 4
                                 CAS(head, 4→5) ✅ success
                                 write ring[4] = obj_B
 wait tail==3 ✅                 wait tail==3...
 tail = 4                        wait tail==4 ✅
                                 tail = 5

 ┌───┬───┬───┬─────┬─────┬───┬───┐
 │   │   │   │obj_A│obj_B│   │   │
 └───┴───┴───┴─────┴─────┴───┴───┘
  0   1   2    3     4    5   6

                    ▲           ▲
                cons.tail     prod.tail
                  (3)           (5)
```

> 图解说明：多生产者入队 (MP Enqueue) 的 CAS 无锁算法。通过 CAS 竞争 head 获取槽位，写入数据后等待 tail 追赶，再更新 tail；图中展示 Core A 与 Core B 并发入队时的成功/失败与重试时序。

### 2.3 单生产者入队 (SP Enqueue) — 更快

```
 No CAS needed, just move pointers:

 prod.head = prod.tail = 3

 Step 1: new_head = prod.head + n    (n = enqueue count)
 Step 2: Write data to ring[3..3+n]
 Step 3: prod.head = prod.tail = 3 + n

 → No atomic ops, no memory barriers (writes)
 → ~30% faster than MP mode

 ⚠ But: must ensure only one thread does enqueue!
```

> 图解说明：单生产者入队 (SP Enqueue) 流程。无需 CAS，直接更新 head/tail 指针，无原子操作和内存屏障，比 MP 模式约快 30%；前提是必须保证只有一个线程执行 enqueue。

### 2.4 批量操作 (Burst)

```
 Single vs burst operations:

 Single (enqueue 32 objects one by one):
   32 × CAS + 32 × memory barriers = huge overhead

 Burst (enqueue 32 objects at once):
   1 × CAS + 1 × memory barrier + 32 × writes = efficient!

 rte_ring_enqueue_burst(ring, objs, 32, NULL);
 → Internally: CAS reserves 32 slots in one shot
 → Batch write, then update tail once
```

> 图解说明：单次操作与批量操作的对比。批量入队将 CAS 和内存屏障分摊到多个对象，一次预留多槽、批量写入再更新 tail，可显著减少开销。

---

## 3. 使用与配置

### 3.1 Ring 模式选择

```
 ┌──────────────────────────┬─────────────────────────────┐
 │        Mode              │        Use Case             │
 ├──────────────────────────┼─────────────────────────────┤
 │ SP/SC (single prod/cons) │ One lcore writes, one reads │
 │ RING_F_SP_ENQ |          │ Fastest, no CAS             │
 │ RING_F_SC_DEQ            │                             │
 ├──────────────────────────┼─────────────────────────────┤
 │ MP/MC (multi prod/cons)  │ Multiple lcores may r/w     │
 │ 0 (default flags)        │ Has CAS overhead            │
 ├──────────────────────────┼─────────────────────────────┤
 │ MP/SC                    │ Multiple enq, one deq       │
 │ RING_F_SC_DEQ            │ e.g. N Rx lcores → 1 Tx     │
 ├──────────────────────────┼─────────────────────────────┤
 │ SP/MC                    │ One enq, multiple deq        │
 │ RING_F_SP_ENQ            │ e.g. 1 dispatcher → N Worker│
 └──────────────────────────┴─────────────────────────────┘

 Performance order: SP/SC > SP/MC ≈ MP/SC > MP/MC
```

> 图解说明：Ring 模式选择表。SP/SC 单生产者单消费者最快；MP/MC 支持多核并发但有 CAS 开销；MP/SC、SP/MC 为混合模式，适用于 Rx→Tx 或分发器→Worker 等场景。

### 3.2 Ring vs 标准 FIFO 对比

```
 ┌───────────────────┬─────────────┬─────────────────────┐
 │                   │ DPDK Ring   │ Locked list queue   │
 ├───────────────────┼─────────────┼─────────────────────┤
 │ Max concurrency   │ N cores     │ 1 core (lock wait)  │
 │ Memory alloc      │ Zero (pre)  │ malloc/free per op │
 │ Cache friendly    │ Excellent   │ Poor (scattered)    │
 │ Fixed/variable    │ Fixed size  │ Variable length     │
 │ Burst ops         │ Native      │ Not supported       │
 │ Metadata overhead │ ~192B       │ 16-32B per node     │
 │ Watermark notify  │ Supported   │ Not supported       │
 │ NUMA aware        │ Supported   │ Manual impl needed  │
 └───────────────────┴─────────────┴─────────────────────┘
```

> 图解说明：DPDK Ring 与加锁链表队列的对比。Ring 在并发度、缓存、批量操作、NUMA 等方面均优于传统加锁队列。

---

## 4. 关键 API

```c
/*
 * rte_ring_create — 创建命名 ring
 * @name:      唯一名称 (用于多进程查找)
 * @count:     槽位数 (必须是 2 的幂, 实际可用 = count-1)
 * @socket_id: NUMA socket ID
 * @flags:     RING_F_SP_ENQ, RING_F_SC_DEQ, 或 0 (MP/MC)
 * 返回:       ring 指针, 失败 NULL
 */
struct rte_ring *
rte_ring_create(const char *name, unsigned count,
    int socket_id, unsigned flags);

/*
 * rte_ring_enqueue_burst — 批量入队 (尽力而为)
 * @r:           ring
 * @obj_table:   对象指针数组
 * @n:           入队数量
 * @free_space:  [出参] 剩余空间 (可选, NULL 忽略)
 * 返回:         实际入队数量 (可能 < n)
 */
unsigned
rte_ring_enqueue_burst(struct rte_ring *r,
    void * const *obj_table, unsigned n,
    unsigned *free_space);

/*
 * rte_ring_dequeue_burst — 批量出队 (尽力而为)
 * @r:          ring
 * @obj_table:  接收指针的数组
 * @n:          最大出队数量
 * @available:  [出参] 剩余可读数量 (可选)
 * 返回:        实际出队数量
 */
unsigned
rte_ring_dequeue_burst(struct rte_ring *r,
    void **obj_table, unsigned n,
    unsigned *available);

/*
 * rte_ring_enqueue / rte_ring_dequeue — 单个对象版本
 * 返回: 0 成功, -ENOBUFS/-ENOENT 满/空
 */
int rte_ring_enqueue(struct rte_ring *r, void *obj);
int rte_ring_dequeue(struct rte_ring *r, void **obj_p);

/*
 * rte_ring_count / rte_ring_free_count — 查询状态
 */
unsigned rte_ring_count(const struct rte_ring *r);
unsigned rte_ring_free_count(const struct rte_ring *r);

/*
 * rte_ring_lookup — 按名称查找 ring (多进程场景)
 */
struct rte_ring *rte_ring_lookup(const char *name);
```

---

## 5. 生产级代码示例

```c
#include <stdio.h>
#include <rte_eal.h>
#include <rte_ring.h>
#include <rte_lcore.h>
#include <rte_malloc.h>

#define RING_SIZE     1024
#define BURST_SIZE    32
#define NUM_ITEMS     10000

struct work_item {
	uint32_t id;
	uint64_t data;
} __rte_cache_aligned;

static struct rte_ring *work_ring;
static volatile int done = 0;

/*
 * 生产者: 批量入队工作项
 */
static int
producer(__rte_unused void *arg)
{
	struct work_item *items[BURST_SIZE];
	unsigned i, sent;
	uint32_t id = 0;
	int socket_id = rte_socket_id();

	while(id < NUM_ITEMS) {
		unsigned n = RTE_MIN(BURST_SIZE, NUM_ITEMS - id);

		for(i = 0; i < n; i++) {
			items[i] = rte_malloc_socket("item",
			    sizeof(struct work_item),
			    RTE_CACHE_LINE_SIZE, socket_id);
			items[i]->id = id + i;
			items[i]->data = (uint64_t)(id + i) * 42;
		}

		sent = rte_ring_enqueue_burst(work_ring,
		    (void **)items, n, NULL);

		/* 释放未成功入队的 */
		for(i = sent; i < n; i++)
			rte_free(items[i]);

		id += sent;
	}

	done = 1;
	return 0;
}

/*
 * 消费者: 批量出队并处理
 */
static int
consumer(__rte_unused void *arg)
{
	struct work_item *items[BURST_SIZE];
	unsigned nb_rx, i;
	uint64_t total = 0;
	unsigned processed = 0;

	while(!done || rte_ring_count(work_ring) > 0) {
		nb_rx = rte_ring_dequeue_burst(work_ring,
		    (void **)items, BURST_SIZE, NULL);

		for(i = 0; i < nb_rx; i++) {
			total += items[i]->data;
			rte_free(items[i]);
			processed++;
		}
	}

	printf("consumer: processed %u items, total = %lu\n",
	    processed, total);
	return 0;
}

int
main(int argc, char **argv)
{
	int ret;

	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("EAL init failed\n");

	if(rte_lcore_count() < 3)
		rte_panic("Need at least 3 lcores\n");

	/* SP/SC ring — 一个生产者, 一个消费者 */
	work_ring = rte_ring_create("work_ring", RING_SIZE,
	    rte_socket_id(),
	    RING_F_SP_ENQ | RING_F_SC_DEQ);
	if(work_ring == NULL)
		rte_panic("Cannot create ring\n");

	/* lcore 1 = producer, lcore 2 = consumer */
	unsigned lcore_id;
	int launched = 0;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if(launched == 0)
			rte_eal_remote_launch(producer, NULL,
			    lcore_id);
		else if(launched == 1)
			rte_eal_remote_launch(consumer, NULL,
			    lcore_id);
		launched++;
		if(launched >= 2)
			break;
	}

	rte_eal_mp_wait_lcore();

	printf("ring stats: used=%u free=%u\n",
	    rte_ring_count(work_ring),
	    rte_ring_free_count(work_ring));

	rte_ring_free(work_ring);
	rte_eal_cleanup();
	return 0;
}
```

---

## 6. 最佳实践与陷阱

### Cache Line 与 Memory Barrier

```
 1. Producer/consumer pointer separation (built into Ring)

    prod.head + prod.tail → exclusive Cache Line A
    cons.head + cons.tail → exclusive Cache Line B

    → Producer only writes A, consumer only writes B
    → No false sharing!

 2. Memory Barrier

    CAS in MP enqueue implies full memory barrier
    SP enqueue uses rte_smp_wmb() (write barrier)

    Write barrier ensures:
    "Write data to ring[idx] first, then update tail"
    Otherwise consumer may see updated tail but read stale data!

 3. Burst ops reduce barrier count

    ❌ Enqueue one by one: 32 × barrier
    ✅ Burst enqueue: 1 × barrier + 32 × write

    Performance: burst can be 5-10x faster
```

> 图解说明：Cache Line 与内存屏障要点。生产者/消费者指针分属不同 cache line 避免伪共享；MP 的 CAS 隐含全屏障，SP 使用写屏障保证数据先写入再更新 tail；批量操作可显著减少屏障次数。

### Ring Size 选择

```
 ⚠ Ring usable capacity = size - 1 (one slot used to distinguish full/empty)

 size must be power of 2:
   ✅ 64, 128, 256, 512, 1024, 2048, 4096, ...
   ❌ 100, 1000, 1500 → will be rejected

 Selection guide:
 ┌──────────────────┬────────────┐
 │ Scenario         │ Rec. size  │
 ├──────────────────┼────────────┤
 │ lcore-to-lcore   │ 512-1024   │
 │ Rx ring          │ 1024-4096  │
 │ Tx ring          │ 1024-4096  │
 │ mempool backend  │ Match pool │
 └──────────────────┴────────────┘

 Too small: frequent full → poor burst, packet loss
 Too large: waste memory, higher latency (deep queue)
```

> 图解说明：Ring 容量与 size 选择。实际可用为 size-1；size 必须为 2 的幂；不同场景有推荐值，过小易满丢包，过大浪费内存且延迟增加。

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| size 不是 2 的幂 | `rte_ring_create` 失败 | 使用 `rte_align32pow2()` |
| MP ring 用在 SP 场景 | 不必要的 CAS 开销 | 明确指定 `RING_F_SP_ENQ` |
| 忽略 burst 返回值 | 内存泄漏 (mbuf 未释放) | 始终检查并释放未入队对象 |
| Ring 跨 NUMA | 性能下降 | 创建时指定正确 socket_id |
| 依赖 `rte_ring_full()` 后入队 | TOCTOU 竞态 | 直接 enqueue 检查返回值 |

---

## 7. 知识检查 (Knowledge Check)

> **问题：**
>
> 1. DPDK Ring 的 `prod.head` 和 `cons.head` 为什么要放在不同的 cache line 中？如果放在同一个 cache line，在 100Gbps 场景下会发生什么？
> 2. 为什么 Ring 的 size 必须是 2 的幂？这和 `idx & mask` 有什么关系？如果改成 `idx % size` 会有什么性能影响？
> 3. 在 SP/SC 模式下，DPDK Ring 是否仍然需要内存屏障？为什么？（提示：考虑编译器优化和 CPU 乱序执行）

### 参考答案

**Q1：为了消除 false sharing（伪共享），这是 Ring 高性能的关键设计。**

**原理：**
- 现代 CPU 的缓存一致性协议（如 MESI）以 **cache line（64B）** 为粒度工作。
- 生产者频繁写 `prod.head/prod.tail`，消费者频繁写 `cons.head/cons.tail`。
- 如果它们在**同一个 cache line** 中：
  - 生产者（Core A）写 `prod.head` → 整条 cache line 在 Core A 变为 Modified 状态
  - 消费者（Core B）写 `cons.head` → 需要先从 Core A 获取该 cache line（Invalidate + Transfer），代价 ~40-70ns
  - Core A 下次写 `prod.tail` → 又要从 Core B 夺回 cache line
  - 如此反复 → **cache line bouncing**

**100Gbps 场景影响量化：**
- 148Mpps → 每 ~6.7ns 一个包 → 每个包至少一次 enqueue + 一次 dequeue
- 每次 cache line bouncing ~40ns → 远超单包处理预算
- 结果：Ring 的吞吐从理论 ~100Mpps 暴跌到 ~10-20Mpps，成为整个系统瓶颈

**DPDK 的做法：** `prod` 和 `cons` 各自独占一整个 64B cache line（中间用 padding 填充），生产者和消费者的写操作完全不会触发对方的 cache invalidation。

**Q2：size 是 2 的幂使得 `idx & mask` 等价于 `idx % size`，但快得多。**

**数学等价性：**
- 当 `size = 2^n` 时，`mask = size - 1 = 2^n - 1`（低 n 位全为 1）
- `idx & mask` 提取 idx 的低 n 位 = `idx mod 2^n`
- 例：`size=8, mask=7(0b111)`, `idx=11(0b1011) & 0b0111 = 0b0011 = 3 = 11 % 8` ✅

**性能差异：**
- **`idx & mask`：** 单条 AND 指令，1 个时钟周期，完全可流水线化。
- **`idx % size`：** 整数除法指令（DIV/IDIV），在 x86 上需要 **20-40 个时钟周期**，不可流水线化，且会阻塞 ALU。
- 在 Ring 的热路径中，每次 enqueue/dequeue 都要计算索引。如果用取模运算：
  - 148Mpps × 2（入+出）× 30 cycles ≈ 消耗 ~8.9 GHz 的 CPU 算力，仅用于索引计算！
  - 用位与运算则几乎零开销。

**这也是 size 不能是任意值的根本原因** —— 不是技术限制，而是性能选择。

**Q3：是的，SP/SC 模式仍然需要内存屏障（memory barrier），但比 MP/MC 更轻量。**

**需要屏障的原因：**

即使只有一个生产者和一个消费者（分别在不同 CPU 核心上），仍存在两个问题：

1. **编译器重排序：** 编译器可能将"更新 tail 指针"的指令排到"写入数据到 ring[idx]"之前，以优化指令调度。结果：消费者看到更新后的 tail，去读 ring[idx]，但数据还没写进去 → 读到脏数据。

2. **CPU 乱序执行（Out-of-Order Execution）：** x86 的 Store-Store 顺序通常是保证的（TSO 内存模型），但在 ARM/POWER 等弱内存模型 CPU 上，写操作可能被重排序。一条 `rte_smp_wmb()`（写屏障）确保"先写数据，再更新 tail"的顺序。

**DPDK SP enqueue 中的屏障：**

```c
ring[idx] = obj;          /* 先写数据 */
rte_smp_wmb();            /* 写屏障: 确保上面的写对其他核心可见 */
prod.tail = new_tail;     /* 再更新 tail */
```

**与 MP 的区别：**
- MP 需要 CAS（`__atomic_compare_exchange`），隐含 full barrier → 最重。
- SP 只需一条 `rte_smp_wmb()` → 在 x86 上编译为 compiler barrier（因为 x86 TSO 已保证 store 顺序），在 ARM 上编译为 `dmb st` 指令。
- 所以 SP/SC 比 MP/MC 快约 30%，但并非"零屏障"。

---

*上一章：[NUMA](./04-numa.md) | 下一章：[Mempool](./06-mempool.md)*
