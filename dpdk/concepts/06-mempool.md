# DPDK 深度解析 (6/9)：Mempool — 内存池

---

## 1. 痛点 (The "Why")

### 动态内存分配的代价

```
┌─────────────────────────────────────────────────────────┐
│         Disaster of malloc/free in Datapath             │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  100Gbps 64B pkts ≈ 148Mpps                             │
│  Each pkt needs buffer: 148M malloc + 148M free/sec     │
│                                                         │
│  glibc malloc problems:                                 │
│  ┌─────────────────────────────────────────────────┐    │
│  │ 1. Syscall overhead                               │    │
│  │    - Large allocs trigger mmap/brk syscalls       │    │
│  │    - Syscall ~200ns, per-pkt budget only ~6.7ns   │    │
│  │                                                   │    │
│  │ 2. Lock contention                                │    │
│  │    - ptmalloc arena locks                         │    │
│  │    - Multi-core contention → perf drops w/ cores  │    │
│  │                                                   │    │
│  │ 3. Memory fragmentation                           │    │
│  │    - Repeated alloc/free of varying sizes → frag  │    │
│  │    - Performance degrades over long runs          │    │
│  │                                                   │    │
│  │ 4. Cache-unfriendly                               │    │
│  │    - Allocated objects at non-contiguous addrs    │    │
│  │    - Cannot leverage CPU prefetch                 │    │
│  │                                                   │    │
│  │ 5. Not on Hugepage                                │    │
│  │    - TLB miss                                     │    │
│  │    - Cannot get physical addr (required for DMA)  │    │
│  └─────────────────────────────────────────────────┘    │
│                                                         │
│  DPDK Mempool solution:                                  │
│  ✅ Pre-alloc fixed-size object pool → zero runtime alloc│
│  ✅ Per-lcore cache → lock-free fast path               │
│  ✅ On Hugepage → physically contiguous, TLB-friendly   │
│  ✅ Cache-line aligned → no false sharing               │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> 上图说明：在数据面路径上调用 malloc/free 的灾难。100Gbps 64 字节线速包需要每秒约 1.48 亿次分配/释放；glibc malloc 存在系统调用开销、锁竞争、碎片化、缓存不友好、非大页等问题；DPDK Mempool 通过预分配、每核缓存、大页和 cache-line 对齐逐一解决。

---

## 2. 核心概念与架构

### 2.1 Mempool 三层架构

```
                  ┌───────────────────────────────────────┐
                  │            应用层调用                   │
                  │   rte_mempool_get / rte_mempool_put    │
                  └──────────────────┬────────────────────┘
                                     │
                  ┌──────────────────┴────────────────────┐
 Layer 1:        │        Per-lcore Cache                  │
 (最快)          │                                         │
                  │  每个 lcore 有独立的本地缓存             │
                  │  get/put 完全无锁, 无原子操作            │
                  │  大小: 通常 256-512 个对象               │
                  │                                         │
                  │  ┌────────┐ ┌────────┐ ┌────────┐      │
                  │  │Cache 0 │ │Cache 1 │ │Cache 2 │ ...  │
                  │  │(lcore0)│ │(lcore1)│ │(lcore2)│      │
                  │  │ 250个  │ │ 250个  │ │ 250个  │      │
                  │  └───┬────┘ └───┬────┘ └───┬────┘      │
                  └──────┼──────────┼──────────┼───────────┘
                         │          │          │
           cache miss → │          │          │ ← cache miss
                         │          │          │
                  ┌──────┴──────────┴──────────┴───────────┐
 Layer 2:        │         Ring (共享后备池)                 │
 (较慢)          │                                         │
                  │  当 per-lcore cache 空了 → 从 ring 批   │
                  │  量取一批到 cache                        │
                  │  当 per-lcore cache 满了 → 归还一批到   │
                  │  ring                                   │
                  │                                         │
                  │  ┌─────────────────────────────┐        │
                  │  │  rte_ring (MP/MC 或 SP/SC)  │        │
                  │  │  存储所有 mempool 对象指针    │        │
                  │  └─────────────────────────────┘        │
                  └────────────────────────────────────────┘
                                     │
                  ┌──────────────────┴────────────────────┐
 Layer 3:        │      Hugepage 内存 (对象存储)           │
 (实际数据)      │                                         │
                  │  ┌─────┬─────┬─────┬─────┬─────┬───┐  │
                  │  │Obj 0│Obj 1│Obj 2│Obj 3│Obj 4│...│  │
                  │  │     │     │     │     │     │   │  │
                  │  │2304B│2304B│2304B│2304B│2304B│   │  │
                  │  └─────┴─────┴─────┴─────┴─────┴───┘  │
                  │                                         │
                  │  每个对象: header + private + data      │
                  │  所有对象 cache-line 对齐                │
                  │  物理地址连续 (在同一 hugepage 内)       │
                  └────────────────────────────────────────┘
```

### 2.2 Per-lcore Cache 工作原理

```
 假设 cache_size = 4, cache_flushthresh = 8 (2 × cache_size)

 === get 操作 (从 cache 取对象) ===

 Cache 有对象:                    Cache 为空:
 ┌───┬───┬───┬───┐              ┌───┬───┬───┬───┐
 │ A │ B │ C │ D │              │   │   │   │   │ len=0
 └───┴───┴───┴───┘ len=4        └───┴───┴───┴───┘
       │                               │
       ▼ (直接从 cache 取)              ▼ (从 ring 批量取)
   return D; len=3;              ring_dequeue_burst(4);
   无锁, 无原子操作!             ┌───┬───┬───┬───┐
                                 │ E │ F │ G │ H │ len=4
                                 └───┴───┴───┴───┘
                                 return H; len=3;

 === put 操作 (归还对象到 cache) ===

 Cache 未满:                     Cache 溢出 (len > flushthresh):
 ┌───┬───┬───┬───┐              ┌───┬───┬───┬───┬───┬───┬───┬───┬───┐
 │ A │ B │ C │   │ len=3        │ A │ B │ C │ D │ E │ F │ G │ H │ X │
 └───┴───┴───┴───┘              └───┴───┴───┴───┴───┴───┴───┴───┴───┘
       │                         len=9 > flushthresh(8)
       ▼                               │
 ┌───┬───┬───┬───┐                     ▼ (归还 cache_size 个到 ring)
 │ A │ B │ C │ X │ len=4        ring_enqueue_burst(A,B,C,D); // 4个
 └───┴───┴───┴───┘              ┌───┬───┬───┬───┬───┐
   无锁, 无原子操作!             │ E │ F │ G │ H │ X │ len=5
                                 └───┴───┴───┴───┴───┘
```

### 2.3 Mempool 对象内存布局

```
 Single mempool object (for pktmbuf):

 ┌──────────────────────────────────────────────────────┐
 │                    rte_mempool_objhdr                 │ obj header
 │  ┌───────────────────────────────────────────────┐   │ (mgmt)
 │  │ mp (mempool ptr) │ iova │ list entry │ ...   │   │
 │  └───────────────────────────────────────────────┘   │
 ├──────────────────────────────────────────────────────┤
 │                    Object data area                   │
 │  ┌───────────────────────────────────────────────┐   │
 │  │              rte_mbuf (128B)                   │   │ metadata
 │  │  buf_addr │ data_off │ pkt_len │ ...          │   │
 │  ├───────────────────────────────────────────────┤   │
 │  │           headroom (128B)                     │   │ reserved
 │  ├───────────────────────────────────────────────┤   │
 │  │                                               │   │
 │  │           Data area (typically 2048B)          │   │ actual pkt
 │  │                                               │   │ data
 │  ├───────────────────────────────────────────────┤   │
 │  │           tailroom (optional)                  │   │
 │  └───────────────────────────────────────────────┘   │
 ├──────────────────────────────────────────────────────┤
 │                    rte_mempool_objtlr                 │ obj trailer
 │  ┌───────────────────────────────────────────────┐   │ (debug only)
 │  │ cookie (detect overflow in debug mode)         │   │
 │  └───────────────────────────────────────────────┘   │
 └──────────────────────────────────────────────────────┘

 Total size ≈ objhdr(64B) + mbuf(128B) + headroom(128B)
             + data(2048B) + tailroom + objtlr
             Align to cache line → ≈ 2432B per object
```

> 上图说明：Mempool 对象内存布局（以 pktmbuf 为例）。由对象头（mempool 指针、iova 等）、对象数据区（mbuf 元数据 + headroom + 数据区 + tailroom）和对象尾（调试用 cookie）组成，总大小约 2432B 并做 cache-line 对齐。

---

## 3. 使用与配置

### 3.1 Mempool 创建参数

```c
/*
 * 创建 pktmbuf 专用 mempool (最常用)
 */
struct rte_mempool *pool = rte_pktmbuf_pool_create(
    "MBUF_POOL",            /* 名称 (全局唯一) */
    8191,                   /* 对象数量 (建议: 2^n - 1) */
    250,                    /* per-lcore cache 大小 */
    0,                      /* private area 大小 */
    RTE_MBUF_DEFAULT_BUF_SIZE, /* 数据区大小 (2048+128) */
    rte_socket_id()         /* NUMA socket */
);
```

### 3.2 对象数量计算

```
 How many mbufs needed?

 Formula: n = (nb_rx_desc + nb_tx_desc + burst_size + cache_size)
            × nb_queues × nb_ports + spare

 Example:
   2 ports, 4 Rx + 4 Tx queues per port
   1024 descriptors per queue, burst = 32, cache = 250

   n = (1024 + 1024 + 32 + 250) × 4 × 2 + 1024
     = 2330 × 8 + 1024
     = 19,664

   Round up to 2^n - 1: 32767 (or 16383 if memory tight)

 Why 2^n - 1?
   → mempool ring size must be 2^n
   → ring actual capacity = 2^n - 1
   → so pool obj count 2^n - 1 fully uses ring
```

> 上图说明：对象数量计算公式。n 由各端口、队列的描述符数、burst、cache 及 spare 共同决定。示例中 2 端口 × 4 队列，得出约 19664 个对象，取 32767 (2^15-1) 以充分利用 Ring 容量。

### 3.3 Cache Size 选择

```
 ┌──────────────────────────────────────────────┐
 │           cache_size Selection Guide         │
 ├──────────────────────────────────────────────┤
 │                                              │
 │  cache_size = 0:                              │
 │    Disable per-lcore cache                    │
 │    Every get/put goes through ring (MP/MC)    │
 │    → Poor perf, rarely used                   │
 │                                              │
 │  cache_size = 250 (recommended):              │
 │    DPDK example default                       │
 │    Fits most use cases                        │
 │    flushthresh = 500                          │
 │                                              │
 │  cache_size = 512:                            │
 │    Large burst size or batch processing       │
 │    Refill/flush 512 objects each time         │
 │                                              │
 │  Constraints:                                 │
 │    cache_size ≤ RTE_MEMPOOL_CACHE_MAX_SIZE    │
 │                  (default 512)                │
 │    cache_size should be multiple of burst_size│
 │    (else low cache utilization)               │
 │                                              │
 └──────────────────────────────────────────────┘
```

> 上图说明：cache_size 选择指南。0 表示禁用 per-lcore cache；250 为推荐默认值；512 适用于大 burst 场景。需满足不超过最大值，且为 burst_size 的整数倍以保证缓存利用率。

---

## 4. 关键 API

```c
/*
 * rte_pktmbuf_pool_create — 创建 pktmbuf 内存池
 * (封装了 rte_mempool_create + mbuf 初始化回调)
 * @n:              对象数量
 * @cache_size:     per-lcore cache 大小 (0=禁用)
 * @priv_size:      每个 mbuf 的 private area 大小
 * @data_room_size: 数据区大小 (含 headroom)
 * @socket_id:      NUMA socket
 */
struct rte_mempool *
rte_pktmbuf_pool_create(const char *name, unsigned n,
    unsigned cache_size, uint16_t priv_size,
    uint16_t data_room_size, int socket_id);

/*
 * rte_mempool_create — 通用 mempool (非 mbuf 用)
 * 可自定义初始化回调, 对象大小等
 */
struct rte_mempool *
rte_mempool_create(const char *name, unsigned n,
    unsigned elt_size, unsigned cache_size,
    unsigned private_data_size,
    rte_mempool_ctor_t *mp_init, void *mp_init_arg,
    rte_mempool_obj_cb_t *obj_init, void *obj_init_arg,
    int socket_id, unsigned flags);

/*
 * rte_mempool_get_bulk — 批量获取对象
 * 返回: 0 成功, -ENOENT 不足
 * 注意: 如果不足, 一个都不会分配 (all or nothing)
 */
int rte_mempool_get_bulk(struct rte_mempool *mp,
    void **obj_table, unsigned n);

/*
 * rte_mempool_put_bulk — 批量归还对象
 */
void rte_mempool_put_bulk(struct rte_mempool *mp,
    void * const *obj_table, unsigned n);

/*
 * rte_mempool_avail_count — 查询可用对象数
 * (包括所有 per-lcore cache 中的对象)
 */
unsigned rte_mempool_avail_count(const struct rte_mempool *mp);

/*
 * rte_mempool_lookup — 按名称查找 (多进程)
 */
struct rte_mempool *rte_mempool_lookup(const char *name);
```

---

## 5. 生产级代码示例

```c
#include <stdio.h>
#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#define NUM_MBUFS     8191
#define CACHE_SIZE    250
#define BURST         32

/*
 * 演示 mempool 基本操作和 per-lcore cache 效果
 */
static void
mempool_demo(int socket_id)
{
	struct rte_mempool *pool;
	struct rte_mbuf *mbufs[BURST];
	unsigned i;
	int ret;

	/* 创建 pool, 指定 NUMA 节点 */
	pool = rte_pktmbuf_pool_create("demo_pool",
	    NUM_MBUFS, CACHE_SIZE, 0,
	    RTE_MBUF_DEFAULT_BUF_SIZE, socket_id);
	if(pool == NULL)
		rte_panic("pool create failed: %s\n",
		    rte_strerror(rte_errno));

	printf("pool '%s' created on socket %d\n",
	    pool->name, socket_id);
	printf("  total objects: %u\n",
	    rte_mempool_avail_count(pool));
	printf("  object size: %u\n", pool->elt_size);
	printf("  cache size: %u\n", pool->cache_size);

	/* 批量分配 mbuf */
	ret = rte_pktmbuf_alloc_bulk(pool, mbufs, BURST);
	if(ret < 0)
		rte_panic("alloc failed\n");

	printf("  after alloc %u mbufs: avail=%u\n",
	    BURST, rte_mempool_avail_count(pool));

	/* 使用 mbuf ... */
	for(i = 0; i < BURST; i++) {
		char *data = rte_pktmbuf_mtod(mbufs[i], char *);
		rte_pktmbuf_append(mbufs[i], 64);
		memset(data, 0x42, 64);
	}

	/* 批量释放 */
	rte_pktmbuf_free_bulk(mbufs, BURST);
	printf("  after free: avail=%u\n",
	    rte_mempool_avail_count(pool));

	rte_mempool_free(pool);
}

int
main(int argc, char **argv)
{
	int ret;

	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("EAL init failed\n");

	mempool_demo(rte_socket_id());

	rte_eal_cleanup();
	return 0;
}
```

---

## 6. 最佳实践与陷阱

### Cache Alignment

```
 Mempool internally ensures:
 ✅ Each object start addr cache-line aligned
 ✅ per-lcore cache struct cache-line aligned
 ✅ ring prod/cons ptrs on separate cache lines

 You must ensure:
 ✅ Custom private data also cache-line aligned
 ✅ Multi-mempool stats not on same cache line
```

> 上图说明：Cache 对齐。Mempool 已保证对象、per-lcore cache、ring 指针的 cache-line 对齐；应用需保证自定义 private 数据和统计结构也做好对齐，避免伪共享。

### Mempool 调试

```c
/* 打印 mempool 详细统计 */
rte_mempool_dump(stdout, pool);

/* 检查 mempool 完整性 (debug 模式) */
rte_mempool_audit(pool);

/* 检查所有对象的 cookie (检测 buffer overflow) */
/* 编译 DPDK 时开启: meson configure -Dc_args='-DRTE_LIBRTE_MEMPOOL_DEBUG' */
```

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| Pool 太小 | `rte_eth_rx_burst` 返回 0, 丢包 | 参考公式计算, 预留余量 |
| cache_size 不匹配 burst | Cache 效率低 | `cache_size = n × burst_size` |
| 忘记 NUMA 对齐 | 跨 socket 性能差 | 匹配 `rte_eth_dev_socket_id()` |
| 多端口共享一个 pool | 跨 NUMA + 争用 | 每 NUMA 节点一个 pool |
| get 后忘记 put | Pool 耗尽, 无法收包 | 确保每条路径都释放 mbuf |
| 在非 EAL 线程使用 | 无 per-lcore cache | 用 `rte_mempool_generic_get()` |

### 外部 Mempool 驱动

```
 DPDK 18.05+ supports pluggable mempool backends:

 Default: rte_ring (software impl)
 Optional:
   - rte_mempool_stack    : Stack-based LIFO (better temporal locality)
   - rte_mempool_bucket   : Bucket alloc (for many small objects)
   - Hardware mempool     : Some NICs support hw mempool
                            (e.g. Cavium/Marvell OCTEONTX)

 Config:
   rte_mempool_set_ops_byname(pool, "stack", NULL);
```

> 上图说明：外部 Mempool 驱动。DPDK 18.05+ 支持可插拔后端，默认 rte_ring；可选 stack（更好时间局部性）、bucket（大量小对象）、或 NIC 硬件 mempool；通过 `rte_mempool_set_ops_byname` 配置。

---

## 7. 知识检查 (Knowledge Check)

> **问题：**
>
> 1. 如果 `cache_size = 250`，一次 burst 收 32 个包，cache 何时会触发 "从 Ring 补货" 操作？何时会触发 "向 Ring 退货"？
> 2. 为什么 `rte_mempool_get_bulk()` 是 "all or nothing" 语义（要么全部分配，要么一个不分配）？这种设计在数据面有什么好处？
> 3. 在多进程模型中，Secondary 进程通过 `rte_mempool_lookup("pool_name")` 找到了 Primary 创建的 mempool。此时 Secondary 进程的 per-lcore cache 是空的还是共享 Primary 的？Secondary 分配的 mbuf 和 Primary 分配的 mbuf 是否共享同一个 Ring 后端？

### 参考答案

**Q1：补货和退货的触发条件如下。**

`cache_size = 250`，`flushthresh = 2 × cache_size = 500`。

**从 Ring 补货（cache 为空时）：**
- per-lcore cache 是一个栈结构，`get` 操作从栈顶取。
- 当 cache 中的对象数量（`len`）为 0 时，下次 `rte_mempool_get()` 会**一次性从 Ring dequeue `cache_size`（250）个对象到 cache**，然后再从 cache 取。
- 在 burst 收包场景中：刚启动时 cache 为空，第一次 `rte_pktmbuf_alloc_bulk(pool, mbufs, 32)` 触发补货 250 个到 cache，然后取 32 个，剩 218 个。后续连续收包：218 → 186 → 154 → ... → 26 → 取 32 个不够（26 < 32），再次从 Ring 补 250 个到 cache。
- **触发条件总结：** `cache->len < 请求数量 n` 时触发补货。

**向 Ring 退货（cache 溢出时）：**
- `put` 操作将对象压入 cache 栈顶。
- 当 `cache->len + n > flushthresh（500）` 时，先将 cache 底部的 `cache_size`（250）个对象**批量 enqueue 回 Ring**，然后再把新对象放入 cache。
- 场景：worker 持续释放 mbuf，cache 从 400 → 432 → 464 → 496 → 再放 32 个时 496 + 32 = 528 > 500 → 触发退货：先把底部 250 个还给 Ring（cache 剩 246），再放入 32 个（cache = 278）。
- **触发条件总结：** `cache->len + n > flushthresh` 时触发退货。

**Q2："all or nothing" 语义简化了数据面的错误处理，提高了性能。**

**好处：**

1. **无需部分回滚：** 如果是"尽力分配"语义（返回实际分配的数量），调用者需要处理"分配了一半怎么办"的逻辑 —— 可能需要释放已分配的、重试、降级处理。这些分支在热路径上增加了 CPU 周期和代码复杂度。

2. **与 `rx_burst` 配合：** `rte_eth_rx_burst()` 需要提前填好 Rx 描述符中的 mbuf 地址。如果 `get_bulk(32)` 只返回了 20 个，应用需要额外逻辑处理不足的 12 个描述符。all-or-nothing 保证要么 32 个全到位，要么知道 pool 见底了可以采取全局策略（如暂停收包）。

3. **内部优化：** 实现上，all-or-nothing 可以先检查 `Ring 中可用数 >= n`，不够就直接返回失败，避免了 dequeue 一半再回滚的开销。

4. **与之对比：** `rte_ring_enqueue_burst()` 是"尽力而为"语义（返回实际入队数），因为发包场景下部分成功是可接受的（未发成功的释放即可）。两种语义分别用在适合的场景。

**Q3：per-lcore cache 是空的；Ring 后端是共享的。**

**详细解释：**

1. **per-lcore cache 是进程私有的：**
   - `struct rte_mempool` 中的 `local_cache` 数组是按 lcore ID 索引的，每个 lcore 有独立的 cache。
   - Secondary 进程通过 `rte_mempool_lookup()` 拿到的是共享内存中 mempool 结构体的指针，可以看到 Primary 的 cache 结构。
   - **但是：** Secondary 进程的 lcore ID 和 Primary 不同（或相同 ID 但不同进程），实际使用时各自维护独立的 cache 内容。
   - Secondary 刚 lookup 到 mempool 时，其 lcore 对应的 cache `len = 0`（空的），第一次 `get` 会触发从共享 Ring 补货。

2. **Ring 后端是共享的：**
   - mempool 底层的 `rte_ring` 存储在共享的 hugepage 内存中。
   - Primary 和 Secondary 的 `get/put` 操作最终都会操作**同一个 Ring**（通过 per-lcore cache 缓冲后）。
   - 这意味着两个进程可以互相"借用"对方的 mbuf：Primary 分配的 mbuf 可以被 Secondary 释放回 Ring，反之亦然。
   - **注意：** 既然共享 Ring，就必须使用 MP/MC 模式（多生产者/多消费者），因为两个进程的多个 lcore 可能同时操作 Ring。

---

*上一章：[Ring](./05-ring.md) | 下一章：[Mbuf](./07-mbuf.md)*
