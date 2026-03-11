# DPDK 深度解析 (4/9)：NUMA Awareness — 非统一内存架构感知

---

## 1. 痛点 (The "Why")

### 现代多路服务器的内存延迟陷阱

```
┌─────────────────────────────────────────────────────────┐
│         Memory Access Latency under NUMA                 │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Typical dual-socket Intel Xeon topology:                │
│                                                         │
│  ┌─────────────────┐         ┌─────────────────┐       │
│  │   NUMA Node 0   │   QPI   │   NUMA Node 1   │       │
│  │                 │◄───────►│                 │       │
│  │  CPU 0-7        │  ~40ns  │  CPU 8-15       │       │
│  │  DDR4 ch 0-2    │ extra   │  DDR4 ch 3-5    │       │
│  │  NIC Port 0     │ latency │  NIC Port 1     │       │
│  │                 │         │                 │       │
│  │  Local: ~80ns   │         │  Local: ~80ns   │       │
│  └─────────────────┘         └─────────────────┘       │
│                                                         │
│  Memory access latency comparison:                      │
│  ┌──────────────────┬──────────┬──────────┬─────────┐  │
│  │ Access Type      │ Latency  │ BW       │ Impact  │  │
│  ├──────────────────┼──────────┼──────────┼─────────┤  │
│  │ L1 Cache         │  ~1 ns   │ ~2 TB/s  │ Baseline│  │
│  │ L2 Cache         │  ~4 ns   │ ~1 TB/s  │         │  │
│  │ L3 Cache         │  ~12 ns  │ ~400GB/s │         │  │
│  │ Local DRAM       │  ~80 ns  │ ~60GB/s  │ 1.0x   │  │
│  │ Remote DRAM(QPI) │ ~130 ns  │ ~30GB/s  │ 1.6x!  │  │
│  └──────────────────┴──────────┴──────────┴─────────┘  │
│                                                         │
│  At 148Mpps (100G) scenario:                            │
│  Each remote access adds ~50ns                          │
│  → 3 remote accesses/pkg → ~30% throughput loss         │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> 双路 Intel Xeon 服务器中，NUMA Node 0 与 Node 1 通过 QPI 互联。本地 DRAM 访问约 80ns，跨 QPI 的远端 DRAM 访问约 130ns（约 1.6 倍）。在 100G 线速场景下，若每包有 3 次远端访问，吞吐可能下降约 30%。

### 典型的 NUMA 失误场景

```
 ❌ CPU on Node 0, but mbuf pool on Node 1:

    CPU 0 (Node 0)              Memory (Node 1)
    ┌──────────┐                ┌──────────────┐
    │ rx_burst │ ──── remote ──→│ mbuf pool    │
    │ process  │ ──── remote ──→│              │
    │ tx_burst │ ──── remote ──→│              │
    └──────────┘                └──────────────┘
    +50ns × 3 accesses/pkg × 14Mpps = huge loss!

 ❌ NIC DMA writes to remote memory:

    NIC (Node 0 PCIe)          Memory (Node 1)
    ┌──────────┐                ┌──────────────┐
    │ DMA wr   │ ──── remote ──→│ Rx Ring      │
    └──────────┘                └──────────────┘
    DMA BW limited by QPI → packet loss
```

> 典型错误：CPU 在 Node 0 而 mbuf pool 在 Node 1，每次 rx/tx 都要跨 NUMA 访问，延迟叠加；NIC DMA 写入远端内存时，带宽受 QPI 限制易丢包。

---

## 2. 核心概念与架构

### 2.1 NUMA 拓扑发现

```
 DPDK EAL discovers NUMA topology during rte_eal_init():

 /sys/devices/system/node/
 ├── node0/
 │   ├── cpulist          → "0-7"     (CPUs on node 0)
 │   ├── meminfo          → total/free memory
 │   └── hugepages/       → hugepage allocation
 └── node1/
     ├── cpulist          → "8-15"
     ├── meminfo
     └── hugepages/

 /sys/bus/pci/devices/0000:03:00.0/
 └── numa_node            → "0"       (PCI device's NUMA node)
```

> DPDK EAL 在初始化时通过 sysfs 读取 NUMA 拓扑：node0/node1 下的 cpulist、meminfo、hugepages；PCI 设备的 numa_node 表示网卡所属 NUMA 节点。

### 2.2 DPDK 的 NUMA 感知设计

```
 NUMA awareness runs through all DPDK critical paths:

 ┌──────────────────────────────────────────────────────┐
 │                   NUMA Node 0                        │
 │                                                      │
 │  ┌────────────┐  ┌─────────────┐  ┌──────────────┐  │
 │  │ Lcore 0-3  │  │ Mempool 0   │  │ NIC Port 0   │  │
 │  │ (Worker)   │  │ (mbuf pool) │  │ (PCIe slot)  │  │
 │  │            │  │             │  │              │  │
 │  │ socket_id  │  │ socket_id   │  │ socket_id    │  │
 │  │   = 0      │  │   = 0       │  │   = 0        │  │
 │  └──────┬─────┘  └──────┬──────┘  └──────┬───────┘  │
 │         │               │                │          │
 │         └───────────────┼────────────────┘          │
 │                         │                            │
 │              All local on Node 0!                    │
 │              Zero cross-NUMA access!                 │
 └──────────────────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────┐
 │                   NUMA Node 1                        │
 │                                                      │
 │  ┌────────────┐  ┌─────────────┐  ┌──────────────┐  │
 │  │ Lcore 4-7  │  │ Mempool 1   │  │ NIC Port 1   │  │
 │  │ (Worker)   │  │ (mbuf pool) │  │ (PCIe slot)  │  │
 │  │ socket_id  │  │ socket_id   │  │ socket_id    │  │
 │  │   = 1      │  │   = 1       │  │   = 1        │  │
 │  └──────┬─────┘  └──────┬──────┘  └──────┬───────┘  │
 │         └───────────────┼────────────────┘          │
 │              All local on Node 1!                    │
 └──────────────────────────────────────────────────────┘
```

> DPDK 的 NUMA 感知设计：Lcore、Mempool、NIC Port 的 socket_id 一致，全部在同一 NUMA 本地，避免跨 NUMA 访问。

### 2.3 Per-Socket 内存分配

```
 Memory layout at rte_eal_init():

 --socket-mem 2048,2048

 Hugepage physical memory:
 ┌──────────────────────────┬──────────────────────────┐
 │     NUMA Node 0          │     NUMA Node 1          │
 │     (2048 MB)            │     (2048 MB)            │
 │                          │                          │
 │  ┌────────────────────┐  │  ┌────────────────────┐  │
 │  │  malloc heap 0     │  │  │  malloc heap 1     │  │
 │  │                    │  │  │                    │  │
 │  │  ┌──────────────┐  │  │  ┌──────────────┐  │  │
 │  │  │ Mempool 0    │  │  │  │ Mempool 1    │  │  │
 │  │  │ (Port0 mbufs)│  │  │  │ (Port1 mbufs)│  │  │
 │  │  └──────────────┘  │  │  └──────────────┘  │  │
 │  │  ┌──────────────┐  │  │  ┌──────────────┐  │  │
 │  │  │ Ring 0       │  │  │  │ Ring 1       │  │  │
 │  │  └──────────────┘  │  │  └──────────────┘  │  │
 │  │  ┌──────────────┐  │  │  ┌──────────────┐  │  │
 │  │  │ Hash Table 0 │  │  │  │ Hash Table 1 │  │  │
 │  │  └──────────────┘  │  │  └──────────────┘  │  │
 │  └────────────────────┘  │  └────────────────────┘  │
 └──────────────────────────┴──────────────────────────┘
```

> Per-Socket 内存分配：使用 `--socket-mem 2048,2048` 时，每个 NUMA 节点各 2048MB 大页，malloc heap、Mempool、Ring、Hash Table 分别分配在对应节点本地。

---

## 3. 使用与配置

### 3.1 检查系统 NUMA 拓扑

```bash
# 查看 NUMA 节点数和 CPU 分布
numactl --hardware
# available: 2 nodes (0-1)
# node 0 cpus: 0 1 2 3 4 5 6 7
# node 0 size: 32768 MB
# node distances:
# node   0   1
#   0:  10  21
#   1:  21  10

# 查看 PCI 设备的 NUMA 归属
cat /sys/bus/pci/devices/0000:03:00.0/numa_node
# 0

# 使用 lspci 结合 NUMA 信息
lspci -vvs 03:00.0 | grep NUMA
# NUMA node: 0

# 查看每个 NUMA 节点的大页
cat /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
cat /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
```

### 3.2 DPDK NUMA 配置

```bash
# 为每个 NUMA 节点分别分配大页
echo 1024 | sudo tee /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
echo 1024 | sudo tee /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages

# DPDK 启动时指定每节点内存
sudo ./app -l 0-15 -n 4 --socket-mem 2048,2048

# 确保 lcore 与端口 NUMA 对齐
# Port 0 在 NUMA 0 → 用 lcore 0-7 处理
# Port 1 在 NUMA 1 → 用 lcore 8-15 处理
```

---

## 4. 关键 API

```c
/*
 * rte_socket_id — 获取当前 lcore 的 NUMA socket ID
 * 在 worker 函数中调用以确保本地内存分配
 */
unsigned rte_socket_id(void);

/*
 * rte_lcore_to_socket_id — 查询任意 lcore 的 NUMA socket
 */
unsigned rte_lcore_to_socket_id(unsigned lcore_id);

/*
 * rte_eth_dev_socket_id — 查询网卡端口的 NUMA socket
 * 关键: 创建 mempool 时应匹配此 socket
 */
int rte_eth_dev_socket_id(uint16_t port_id);

/*
 * rte_pktmbuf_pool_create — 创建 mbuf 池 (NUMA 感知)
 * @socket_id: 指定在哪个 NUMA 节点分配
 *             使用 rte_eth_dev_socket_id(port) 对齐
 */
struct rte_mempool *
rte_pktmbuf_pool_create(const char *name, unsigned n,
    unsigned cache_size, uint16_t priv_size,
    uint16_t data_room_size, int socket_id);

/*
 * rte_malloc_socket — 在指定 NUMA 节点分配内存
 * @socket: NUMA socket ID, SOCKET_ID_ANY = 自动选择
 */
void *rte_malloc_socket(const char *type, size_t size,
    unsigned align, int socket);

/*
 * rte_ring_create — 创建 ring (NUMA 感知)
 * @socket_id: NUMA socket ID
 */
struct rte_ring *
rte_ring_create(const char *name, unsigned count,
    int socket_id, unsigned flags);
```

---

## 5. 生产级代码示例

```c
#include <stdio.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>

#define NUM_MBUFS      8191
#define MBUF_CACHE     250
#define RX_DESC        1024
#define TX_DESC        1024

struct port_conf {
	uint16_t port_id;
	int socket_id;
	struct rte_mempool *pool;
} __rte_cache_aligned;

/*
 * NUMA 感知的端口初始化
 * 所有资源都分配在网卡所在的 NUMA 节点上
 */
static int
init_port_numa_aware(uint16_t port_id)
{
	struct rte_eth_conf port_conf = {0};
	struct rte_mempool *pool;
	int socket_id;
	char name[32];
	int ret;

	/* 获取端口所在 NUMA 节点 */
	socket_id = rte_eth_dev_socket_id(port_id);
	if(socket_id < 0)
		socket_id = 0;

	printf("port %u: NUMA socket %d\n", port_id, socket_id);

	/* 在同一 NUMA 节点创建 mbuf pool */
	snprintf(name, sizeof(name), "pool_%u", port_id);
	pool = rte_pktmbuf_pool_create(name, NUM_MBUFS,
	    MBUF_CACHE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, socket_id);
	if(pool == NULL)
		rte_panic("Cannot create mbuf pool on socket %d\n",
		    socket_id);

	/* 配置端口 */
	ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
	if(ret < 0)
		return ret;

	/* Rx 队列 — 使用同 NUMA 节点的 pool */
	ret = rte_eth_rx_queue_setup(port_id, 0, RX_DESC,
	    socket_id, NULL, pool);
	if(ret < 0)
		return ret;

	/* Tx 队列 — 同 NUMA 节点 */
	ret = rte_eth_tx_queue_setup(port_id, 0, TX_DESC,
	    socket_id, NULL);
	if(ret < 0)
		return ret;

	ret = rte_eth_dev_start(port_id);
	return ret;
}

/*
 * 验证 lcore 与 port 的 NUMA 对齐
 */
static void
check_numa_alignment(void)
{
	unsigned lcore_id;
	uint16_t port_id;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		int lcore_socket = rte_lcore_to_socket_id(lcore_id);

		RTE_ETH_FOREACH_DEV(port_id) {
			int port_socket = rte_eth_dev_socket_id(port_id);

			if(lcore_socket != port_socket) {
				printf("WARNING: lcore %u (socket %d) "
				    "misaligned with port %u (socket %d)\n",
				    lcore_id, lcore_socket,
				    port_id, port_socket);
			}
		}
	}
}

int
main(int argc, char **argv)
{
	int ret;
	uint16_t port_id;

	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("EAL init failed\n");

	RTE_ETH_FOREACH_DEV(port_id) {
		ret = init_port_numa_aware(port_id);
		if(ret < 0)
			rte_panic("port %u init failed\n", port_id);
	}

	check_numa_alignment();

	/* ... worker 启动逻辑 ... */

	rte_eal_cleanup();
	return 0;
}
```

---

## 6. 最佳实践与陷阱

### NUMA 对齐黄金法则

```
 ┌───────────────────────────────────────────────────────┐
 │              NUMA Alignment Checklist                 │
 │                                                       │
 │  ☑ NIC on NUMA Node X                                 │
 │  ☑ Rx/Tx queue socket_id = X                          │
 │  ☑ mbuf pool socket_id = X                            │
 │  ☑ Worker lcore pinned to CPUs on NUMA Node X         │
 │  ☑ Hash Table / LPM etc. allocated on Node X          │
 │  ☑ Use --socket-mem so Node X has enough hugepages    │
 │                                                       │
 │  Any item fails → cross-NUMA access → 30-60% perf drop│
 └───────────────────────────────────────────────────────┘
```

> NUMA 对齐黄金法则：NIC、队列、mbuf pool、Worker lcore、数据结构、大页分配需全部落在同一 NUMA 节点；任一项不满足都会引发跨 NUMA 访问，性能下降 30–60%。

### 跨 NUMA 通信

```
 When data must cross NUMA (e.g. Port0→Port1 forward):

 Approach 1: Cross-NUMA Ring (simple but adds latency)
 ┌──────────┐     ┌──────────┐     ┌──────────┐
 │ Lcore 0  │     │ Ring     │     │ Lcore 8  │
 │ (Node 0) │────→│(any NUMA)│────→│ (Node 1) │
 │ rx_burst │     │          │     │ tx_burst │
 └──────────┘     └──────────┘     └──────────┘

 Approach 2: Batch + prefetch (recommended)
 - Accumulate BURST_SIZE mbufs before enqueue
 - Receiver prefetches mbuf data right after dequeue
 - Reduces cross-NUMA access count

 Approach 3: Re-allocate mbuf (extreme perf needs)
 - Node 0 rx → copy to Node 1 mbuf → free original
 - Copy cost vs all-local access afterward
 - Only worth it when post-processing >> copy cost
```

> 跨 NUMA 通信的三种思路：① 跨 NUMA Ring，实现简单但增加延迟；② 批量 enqueue + 预取，减少跨 NUMA 访问次数（推荐）；③ 在目标节点 re-allocate mbuf 并拷贝，仅在后处理远超拷贝开销时划算。

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| mempool 创建时 socket_id=0 | Node 1 的 lcore 性能差 | 用 `rte_eth_dev_socket_id()` |
| 大页只在 Node 0 分配 | Node 1 内存不足 | 两个节点都分配大页 |
| `rte_malloc()` 不指定 socket | 可能分配到远端 | 用 `rte_malloc_socket()` |
| 单 mempool 供所有端口 | 混合 NUMA 访问 | 每个 NUMA 节点一个 pool |
| 忽略 `rte_eth_dev_socket_id()` 返回 -1 | 默认 socket 0 | 检查返回值并兜底 |

---

## 7. 知识检查 (Knowledge Check)

> **问题：** 你的应用从 Port 0 (NUMA 0) 收包，经过查表处理后从 Port 1 (NUMA 1) 发出。
>
> 1. `rte_pktmbuf_pool_create()` 应该用哪个 `socket_id`？Node 0 还是 Node 1？为什么？（提示：思考 DMA 写入发生在哪里）
> 2. 如果查表用的 Hash Table 只有一份且分配在 Node 0，Node 1 上的 lcore 去查表会有什么后果？你会怎么优化？
> 3. `numactl --hardware` 输出的 `node distances` 矩阵中，值 "10" 和 "21" 分别代表什么？在 4-socket 系统中，最远 socket 间的距离可能是多少？

### 参考答案

**Q1：应该用 Node 0（socket_id = 0），即 Rx 端网卡所在的 NUMA 节点。**

关键推理链：
1. 收包时，**NIC 的 DMA 引擎将包数据写入 mbuf** → 这是第一次内存写入。
2. DMA 写入的目标地址就是 mbuf 的数据区（由 Rx 描述符中的物理地址指定）。
3. 如果 mbuf pool 在 Node 0（与 Port 0 的 NIC 同 NUMA），DMA 写入是**本地内存写**，速度最快、带宽最大。
4. 如果 mbuf pool 在 Node 1，NIC DMA 必须跨 QPI/UPI 写入远端内存 → 带宽减半，且可能成为 100Gbps 线速下的瓶颈，导致 Rx 描述符耗尽丢包。
5. 虽然最终发包时 Tx DMA 需要从 Node 0 读取数据跨 NUMA 发往 Port 1（Node 1），但**Rx DMA 写入频率 ≥ Tx DMA 读取频率**，优先保障 Rx 侧。

**更优方案：** 如果有充足内存，可以创建两个 pool —— Node 0 一个用于 Port 0 Rx，发往 Port 1 时在 Node 1 re-allocate mbuf 并拷贝数据。但通常这种双份内存+拷贝仅在后续处理非常复杂时才划算。

**Q2：Node 1 上的 lcore 每次查表都会产生跨 NUMA 内存访问。**

**后果：**
- 每次 Hash Table lookup 涉及 2-5 次内存访问（hash 计算、桶定位、key 比较、value 读取）。
- 如果 Hash Table 在 Node 0，Node 1 的 lcore 每次访问延迟增加 ~50ns × 2-5 = 100-250ns/包。
- 以 14Mpps 单核吞吐计算，这会将吞吐直接腰斩到 5-7Mpps。

**优化方案（按推荐程度排序）：**
1. **每个 NUMA 节点一份 Hash Table 副本**（推荐）—— `rte_hash_create()` 时分别指定 socket 0 和 socket 1，两份内容完全相同。更新时双写。代价是内存翻倍，但内存便宜、延迟无价。
2. **只用 Node 0 的 lcore 做查表**—— 把数据面处理集中在 Node 0，Node 1 的 lcore 只做简单转发。限制了架构灵活性。
3. **prefetch 优化**—— 跨 NUMA 访问前先 `rte_prefetch0()`，流水线化处理多个包。减轻但不能消除延迟影响。

**Q3：值 "10" 表示本地访问延迟（基准），"21" 表示跨一跳节点的相对延迟。**

- Linux NUMA distance 是一个相对值，**10 是基准值**，代表本节点内存访问（最快）。
- **21 表示跨节点访问延迟是本地的 2.1 倍**。实际物理延迟：本地 ~80ns，跨节点 ~80 × 2.1 ≈ 168ns（具体因硬件而异，这里的比例关系是准确的）。
- 在 **4-socket 系统**中，CPU 间的互联拓扑通常不是全连接的（成本太高），而是环形或网格：

```
  Node 0 ——— Node 1
    |    ╲ ╱    |
    |     ╳     |
    |    ╱ ╲    |
  Node 3 ——— Node 2
```

- 最远的两个节点（如 Node 0 和 Node 2 对角线）需要跨两跳，distance 可能达到 **31**（3.1 倍本地延迟），实际延迟可达 ~250ns。
- 某些 4-socket 平台（如 Intel 8-socket Xeon）的最远距离可达 40+。

---

*上一章：[Lcore](./03-lcore.md) | 下一章：[Ring](./05-ring.md)*
