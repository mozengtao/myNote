# DPDK 核心组件深度解析

> **Data Plane Development Kit** — 一套用于高性能数据包处理的用户态框架与库集合。

---

## 为什么需要 DPDK？

传统 Linux 内核网络栈在设计之初追求的是 **通用性** 与 **安全性**，而非极致吞吐。当线速从 1 GbE 演进到 10/25/40/100 GbE 时，内核栈的多层抽象成为了性能瓶颈：

```
┌──────────────────────────────────────────────────────┐
│      Major Bottlenecks in Traditional Kernel Stack   │
├──────────────────────────────────────────────────────┤
│                                                      │
│  1. Interrupt Overhead                               │
│     - Each packet triggers hard IRQ -> softIRQ       │
│       -> context switch                              │
│     - 100Gbps ≈ 148.8Mpps (64B), interrupt storm     │
│       is unbearable                                  │
│                                                      │
│  2. Copy-to-User (Kernel/User Space Copy)            │
│     - sk_buff -> user buffer at least one memcpy     │
│     - Cache miss penalty is very high                │
│                                                      │
│  3. Too Many Protocol Stack Layers                   │
│     - L2 -> L3 -> L4 -> socket layer-by-layer parse  │
│     - Completely redundant for pure forwarding       │
│                                                      │
│  4. Locks and Shared Data Structures                 │
│     - netfilter/conntrack global locks               │
│     - Multi-core contention hurts scalability        │
│                                                      │
│  5. TLB Thrashing                                    │
│     - 4KB pages manage large DMA buffers             │
│     - Frequent TLB miss -> memory access latency     │
│       spikes                                         │
│                                                      │
└──────────────────────────────────────────────────────┘
```

> **中文说明：** 此图展示了传统 Linux 内核网络栈的 5 大性能瓶颈：中断开销、内核态/用户态拷贝、协议栈层次过多、锁与共享数据结构竞争、以及 TLB 抖动。这些问题在 100Gbps 线速下尤为突出。

## DPDK 的核心理念

```
┌────────────────────────────────────────────────────────┐
│                    DPDK Design Philosophy              │
├────────────────────────────────────────────────────────┤
│                                                        │
│  ① Kernel Bypass    — Bypass kernel, user-space NIC    │
│  ② Polling          — Eliminate interrupts, CPU pulls  │
│  ③ Hugepages        — Reduce TLB miss, memory perf     │
│  ④ CPU Affinity     — Pin threads to cores, no jitter  │
│  ⑤ Lockless Design  — Lock-free structures, max core   │
│  ⑥ NUMA Awareness   — Local mem first, avoid cross-node│
│  ⑦ Batch Processing — Batch rx/tx, amortize call cost  │
│                                                        │
└────────────────────────────────────────────────────────┘
```

> **中文说明：** 此图概括了 DPDK 的 7 大设计理念：内核旁路、轮询模式、大页内存、CPU 亲和性、无锁设计、NUMA 感知、以及批量处理。这些原则共同支撑了 DPDK 的高性能数据平面能力。

## DPDK 整体架构

```
 ┌──────────────────────────────────────────────────────────────────┐
 │                    User Application Layer                        │
 │         ┌───────┐  ┌───────┐  ┌────────┐  ┌──────┐               │
 │         │ l2fwd │  │ l3fwd │  │ OvS-DP │  │Custom│  ...          │
 │         └───┬───┘  └───┬───┘  └────┬───┘  └──┬───┘               │
 │             │          │           │          │                  │
 │  ┌──────────┴──────────┴───────────┴──────────┴───────────────┐  │
 │  │                     DPDK Libraries                         │  │
 │  │  ┌──────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌──────┐ ┌───────┐   │  │
 │  │  │ Ring │ │Mbuf  │ │Mempool│ │Timer │ │ Hash │ │ LPM   │   │  │
 │  │  └──────┘ └──────┘ └───────┘ └──────┘ └──────┘ └───────┘   │  │
 │  └──────────────────────────┬─────────────────────────────────┘  │
 │                             │                                    │
 │  ┌──────────────────────────┴─────────────────────────────────┐  │
 │  │                    EAL (Environment Abstraction Layer)     │  │
 │  │  Hugepage | Lcore | PCI/UIO | Log | Timer | ...            │  │
 │  └──────────────────────────┬─────────────────────────────────┘  │
 │                             │                                    │
 │  ┌──────────────────────────┴─────────────────────────────────┐  │
 │  │              PMD (Poll Mode Drivers)                       │  │
 │  │  ┌───────┐ ┌────────┐ ┌────────┐ ┌───────┐ ┌──────────┐    │  │
 │  │  │ixgbe  │ │  i40e  │ │ mlx5   │ │virtio │ │ af_xdp   │    │  │
 │  │  └───┬───┘ └───┬────┘ └───┬────┘ └───┬───┘ └────┬─────┘    │  │
 │  └──────┼─────────┼──────────┼─────────┼──────────┼───────────┘  │
 └─────────┼─────────┼──────────┼─────────┼──────────┼──────────────┘
           │         │          │         │          │
 ══════════╪═════════╪══════════╪═════════╪══════════╪══════ (UIO/VFIO)
           │         │          │         │          │
 ┌─────────┴─────────┴──────────┴─────────┴──────────┴────────────┐
 │                         Physical NIC                           │
 └────────────────────────────────────────────────────────────────┘
```

> **中文说明：** 此图展示了 DPDK 的分层架构：自顶而下依次为用户态应用、DPDK 库、EAL 环境抽象层、PMD 轮询驱动，最终通过 UIO/VFIO 访问物理网卡。EAL 负责大页、逻辑核、NUMA、PCI 等底层抽象。

## 组件文档索引

| # | 组件 | 文件 | 核心关注点 |
|---|------|------|-----------|
| 1 | [Hugepages](./01-hugepages.md) | `01-hugepages.md` | 大页内存、TLB 优化 |
| 2 | [EAL](./02-eal.md) | `02-eal.md` | 环境抽象层、初始化入口 |
| 3 | [Lcore](./03-lcore.md) | `03-lcore.md` | 逻辑核心、线程模型 |
| 4 | [NUMA](./04-numa.md) | `04-numa.md` | 非统一内存架构感知 |
| 5 | [Ring](./05-ring.md) | `05-ring.md` | 无锁环形队列 |
| 6 | [Mempool](./06-mempool.md) | `06-mempool.md` | 内存池、对象缓存 |
| 7 | [Mbuf](./07-mbuf.md) | `07-mbuf.md` | 报文缓冲区 |
| 8 | [PMD](./08-pmd.md) | `08-pmd.md` | 轮询模式驱动 |
| 9 | [KNI](./09-kni.md) | `09-kni.md` | 内核网络接口 |

## 组件依赖关系

```
                    ┌──────────┐
                    │ App Layer│
                    └────┬─────┘
                         │
              ┌──────────┼──────────┐
              │          │          │
         ┌────┴───┐ ┌───┴───┐ ┌───┴───┐
         │  KNI   │ │  PMD  │ │ Hash  │  ...
         └────┬───┘ └───┬───┘ └───┬───┘
              │         │         │
              └────┬────┘         │
                   │              │
              ┌────┴────┐   ┌────┴────┐
              │  Mbuf   │   │  Ring   │
              └────┬────┘   └────┬────┘
                   │             │
              ┌────┴─────────────┴────┐
              │       Mempool         │
              └───────────┬───────────┘
                          │
              ┌───────────┴───────────┐
              │         EAL           │
              │  (Hugepages + Lcore   │
              │   + NUMA + PCI/UIO)   │
              └───────────────────────┘
```

> **中文说明：** 此图展示了 DPDK 各组件的依赖关系：应用层（KNI、PMD、Hash 等）依赖 Mbuf 与 Ring，二者又共同依赖 Mempool，最终全部依托于 EAL 提供的大页、逻辑核、NUMA 与 PCI/UIO 抽象。

## 最小可运行示例：DPDK Hello World

```c
#include <stdio.h>
#include <rte_eal.h>
#include <rte_lcore.h>

static int
lcore_hello(__rte_unused void *arg)
{
	unsigned lcore_id;

	lcore_id = rte_lcore_id();
	printf("hello from core %u\n", lcore_id);
	return 0;
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	/* EAL 初始化：解析命令行参数、配置 hugepages、绑定设备等 */
	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("Cannot init EAL\n");

	/* 在每个 worker lcore 上启动函数 */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
	}

	/* master lcore 也执行 */
	lcore_hello(NULL);

	/* 等待所有 worker 完成 */
	rte_eal_mp_wait_lcore();

	/* 清理 EAL 资源 */
	rte_eal_cleanup();
	return 0;
}
```

**编译命令：**
```bash
gcc -o helloworld helloworld.c $(pkg-config --cflags --libs libdpdk)
```

**运行命令：**
```bash
sudo ./helloworld -l 0-3 -n 4 --huge-dir /dev/hugepages
```

---

> **阅读建议：** 按照索引顺序 1→9 阅读。Hugepages 和 EAL 是地基，理解它们后其余组件水到渠成。
