# DPDK 深度解析 (3/9)：Lcore — 逻辑核心与线程模型

---

## 1. 痛点 (The "Why")

### 传统线程模型在包处理中的问题

```
┌─────────────────────────────────────────────────────────┐
│     Performance Killers in Traditional Multi-threaded   │
│                     Network Applications                 │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  1. Thread Scheduling Overhead                          │
│     - Linux CFS: context switch ~1-5 μs                  │
│     - 148Mpps (100G small pkts) → ~6.7ns budget per pkt  │
│     - One context switch = waste ~200-750 pkt cycles    │
│                                                         │
│  2. Cache Pollution                                     │
│     - Threads migrate across CPUs                        │
│     - L1/L2 cache content fully invalidated              │
│     - Cache warm-up: hundreds of μs                      │
│                                                         │
│  3. Lock Contention                                     │
│     - Multiple threads share same queue                  │
│     - spinlock/mutex causes core spin or block           │
│     - cache line bouncing                                │
│                                                         │
│  4. NUMA Unaware                                        │
│     - Threads scheduled to remote NUMA node              │
│     - Local mem ~80ns vs remote ~130ns (+60%)            │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> 上图概括了传统多线程网络应用的四大性能杀手：调度开销、缓存污染、锁竞争与 NUMA 无感知。

### DPDK 的解法：Run-to-Completion + 独占核心

```
 Traditional:                      DPDK Lcore Model:
 ┌──────────────┐                 ┌──────────────┐
 │   Thread A   │ ← may be        │   Lcore 0    │ ← bound to CPU 0
 │              │    scheduled    ├──────────────┤    never migrates
 │ process Q0   │    to any CPU   │  (Master)    │
 │ process Q1   │                 ├──────────────┤
 │ ...          │                 │   Lcore 1    │ ← bound to CPU 1
 ├──────────────┤                 │  (Worker)    │    exclusive core
 │   Thread B   │ ← competes      │  process     │    100% utilization
 │              │    with A for   │  Port0 RxQ0  │
 │ process Q0   │    same lock    ├──────────────┤
 │ ...          │                 │   Lcore 2    │ ← bound to CPU 2
 └──────────────┘                 │  (Worker)    │    exclusive core
                                  │  process     │
                                  │  Port0 RxQ1  │
                                  ├──────────────┤
                                  │   Lcore 3    │ ← bound to CPU 3
                                  │  (Worker)    │    exclusive core
                                  │  process     │
                                  │  Port1 RxQ0  │
                                  └──────────────┘
```

> 左图：传统模型中线程可被调度到任意 CPU，多线程争抢同一队列和锁。右图：DPDK 将每个 Lcore 绑定到固定 CPU，独占核心，无调度迁移。

---

## 2. 核心概念与架构

### 2.1 Master Lcore vs Worker Lcore

```
 ┌───────────────────────────────────────────────────────┐
 │                  Lcore Role Model                     │
 │                                                       │
 │   ┌─────────────────────────────────────────────┐     │
 │   │          Master Lcore (main lcore)          │     │
 │   │                                             │     │
 │   │  ★ Thread executing main()                   │     │
 │   │  ★ Handles init, config, management          │     │
 │   │  ★ Dispatches tasks via rte_eal_remote_     │     │
 │   │    launch()                                  │     │
 │   │  ★ May do dataplane (not recommended)        │     │
 │   │  ★ Default: first core in -l param          │     │
 │   │                                             │     │
 │   └──────────────────┬──────────────────────────┘     │
 │                      │                                │
 │          ┌───────────┼───────────┐                    │
 │          │           │           │                    │
 │   ┌──────┴──┐  ┌─────┴───┐  ┌───┴──────┐             │
 │   │Worker 1 │  │Worker 2 │  │Worker 3  │  ...         │
 │   │         │  │         │  │          │              │
 │   │dataplane│  │dataplane│  │dataplane │              │
 │   │ rx      │  │ rx      │  │ rx       │              │
 │   │ process │  │ process │  │ process  │              │
 │   │ tx      │  │ tx      │  │ tx       │              │
 │   │         │  │         │  │          │              │
 │   │exclusive│  │exclusive│  │exclusive │              │
 │   │ CPU 1   │  │ CPU 2   │  │ CPU 3    │              │
 │   └─────────┘  └──────────┘  └──────────┘             │
 │                                                       │
 └───────────────────────────────────────────────────────┘
```

> Master Lcore 运行 main()、做初始化并派发任务；Worker Lcore 独占各自 CPU，完成数据面的收包、处理、发包。

### 2.2 Lcore 线程状态机

```
                    rte_eal_init()
                         │
                         ▼
                  ┌──────────────┐
                  │   WAIT       │ ← Initial state after worker created
                  │ (idle wait)  │     pthread created & bound, no task
                  └──────┬───────┘
                         │
          rte_eal_remote_launch(f, arg, id)
                         │
                         ▼
                  ┌──────────────┐
                  │   RUNNING    │ ← Executing user function f(arg)
                  │ (executing)  │
                  └──────┬───────┘
                         │
                    f(arg) returns
                         │
                         ▼
                  ┌──────────────┐
                  │   FINISHED   │ ← Function done, waiting for master
                  │  (finished)  │
                  └──────┬───────┘
                         │
          rte_eal_wait_lcore(id) or
          rte_eal_mp_wait_lcore()
                         │
                         ▼
                  ┌──────────────┐
                  │   WAIT       │ ← Can remote_launch again
                  │ (idle wait)  │
                  └──────────────┘
```

> Worker 创建后先处于 WAIT；remote_launch 后进入 RUNNING 执行 f(arg)；返回后进入 FINISHED，主线程 wait 后回到 WAIT 可再次派发。

### 2.3 Run-to-Completion vs Pipeline 模型

```
 ═══════════════════════════════════════════════════════════
  Model A: Run-to-Completion (RTC) — Most Common
 ═══════════════════════════════════════════════════════════

   Each Lcore handles full Rx → Process → Tx independently:

   Lcore 1 (Port0 RxQ0):          Lcore 2 (Port0 RxQ1):
   ┌─────────────────┐            ┌─────────────────┐
   │  while(1) {     │            │  while(1) {     │
   │    rx_burst()   │            │    rx_burst()   │
   │    process()    │            │    process()    │
   │    tx_burst()   │            │    tx_burst()   │
   │  }              │            │  }              │
   └─────────────────┘            └─────────────────┘

   Pros: no cross-core comm, no locks, cache-friendly
   Cons: single core can bottleneck if logic is complex

 ═══════════════════════════════════════════════════════════
  Model B: Pipeline — Stage-Based Processing
 ═══════════════════════════════════════════════════════════

   Different Lcores for different stages:

   Lcore 1        Lcore 2         Lcore 3
   (Rx Stage)     (Process)       (Tx Stage)
   ┌─────────┐   ┌─────────┐    ┌──────────┐
   │rx_burst │──→│ process │──→ │ tx_burst │
   │         │   │ (DPI/   │    │          │
   │         │Ring│  ACL/  │Ring│          │
   │         │   │  NAT)   │    │          │
   └─────────┘   └─────────┘    └──────────┘

   Pros: complex logic (good per-stage cache locality)
   Cons: Ring transfer latency, stages need load balance

 ═══════════════════════════════════════════════════════════
```

> RTC 模型：每核独立完成 Rx→Process→Tx，无跨核通信。Pipeline 模型：不同 Lcore 负责 Rx、Process、Tx 不同阶段，通过 Ring 传递包。

### 2.4 CPU 隔离：确保独占

```bash
# Linux 内核启动参数 - 隔离 CPU 核心
# /etc/default/grub
GRUB_CMDLINE_LINUX="isolcpus=1-7 nohz_full=1-7 rcu_nocbs=1-7"

# 解释:
# isolcpus=1-7     : 内核调度器不会将普通进程调度到这些核心
# nohz_full=1-7    : 关闭这些核心上的定时器中断 (NO_HZ_FULL)
# rcu_nocbs=1-7    : RCU 回调不在这些核心上执行

# 结果: CPU 0 运行内核任务 + DPDK master lcore
#       CPU 1-7 完全被 DPDK worker lcores 独占
```

---

## 3. 使用与配置

### 3.1 Lcore 映射方式

```bash
# 方式 1: 简单列表 (lcore ID = CPU ID)
-l 0-3          # lcore 0→CPU0, lcore 1→CPU1, ...
-l 0,2,4,6      # lcore 0→CPU0, lcore 2→CPU2, ...

# 方式 2: 高级映射 (lcore ID ≠ CPU ID)
--lcores '0@0,1@2,2@4,3@6'
# lcore 0 → CPU 0
# lcore 1 → CPU 2
# lcore 2 → CPU 4
# lcore 3 → CPU 6

# 方式 3: 多 lcore 共享 CPU (超额分配，性能测试用)
--lcores '(0-3)@(0-1)'
# lcore 0-3 都运行在 CPU 0-1 上 (不推荐生产环境)

# 方式 4: 指定 main lcore
--main-lcore 4 -l 0-7
# lcore 4 成为 main lcore，而不是默认的第一个
```

### 3.2 服务核心 (Service Cores)

```
 DPDK 17.08+ introduces Service Cores:

 ┌──────────────────────────────────────────────┐
 │  Some PMDs need background periodic tasks    │
 │  (link status check, interrupt handling,     │
 │  etc.), but workers in busy loop cannot      │
 │  respond.                                    │
 │                                              │
 │  Service Core = lcore dedicated to these     │
 │  background services                         │
 └──────────────────────────────────────────────┘

 -l 0-5 -s 0x1   # lcore 0 as service core
                  # lcore 1-5 as workers
```

> Service Core 专门运行 PMD 所需的后台任务（如链路状态检查、中断处理），避免与 worker 的 busy loop 争用。

---

## 4. 关键 API

```c
/*
 * rte_lcore_id — 获取当前线程的 lcore ID
 * 只能在 EAL 线程中调用 (即通过 rte_eal_init 创建的线程)
 * 非 EAL 线程返回 LCORE_ID_ANY
 */
unsigned rte_lcore_id(void);

/*
 * rte_lcore_count — 获取可用 lcore 总数
 * 包括 master lcore
 */
unsigned rte_lcore_count(void);

/*
 * rte_socket_id — 获取当前 lcore 所在的 NUMA socket
 */
unsigned rte_socket_id(void);

/*
 * rte_lcore_index — 获取 lcore 在 lcore 列表中的索引
 * @lcore_id: lcore ID, -1 表示当前 lcore
 */
int rte_lcore_index(int lcore_id);

/*
 * RTE_LCORE_FOREACH_WORKER — 遍历所有 worker lcore
 * (不包括 master lcore)
 */
#define RTE_LCORE_FOREACH_WORKER(i)

/*
 * rte_eal_remote_launch — 在 worker 上启动函数
 * @f:         int (*)(void *) 类型函数
 * @arg:       传递给 f 的参数
 * @worker_id: 目标 lcore ID
 * 返回: 0 成功, -EBUSY 该 lcore 还在执行
 */
int rte_eal_remote_launch(lcore_function_t *f, void *arg,
                          unsigned worker_id);

/*
 * rte_get_main_lcore — 获取 main lcore ID
 */
unsigned rte_get_main_lcore(void);
```

---

## 5. 生产级代码示例：Run-to-Completion 转发

```c
#include <stdio.h>
#include <signal.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define RX_RING_SIZE  1024
#define TX_RING_SIZE  1024
#define BURST_SIZE    32
#define MBUF_POOL_SIZE 8191
#define MBUF_CACHE_SIZE 250

static volatile int quit_signal = 0;

struct lcore_conf {
	uint16_t port_id;
	uint16_t rx_queue_id;
	uint16_t tx_queue_id;
	uint16_t tx_port_id;
} __rte_cache_aligned;

static struct lcore_conf lcore_conf[RTE_MAX_LCORE];

/*
 * worker lcore: run-to-completion 转发循环
 */
static int
lcore_worker(void *arg)
{
	struct lcore_conf *conf;
	struct rte_mbuf *bufs[BURST_SIZE];
	uint16_t nb_rx, nb_tx;
	unsigned lcore_id;

	(void)arg;
	lcore_id = rte_lcore_id();
	conf = &lcore_conf[lcore_id];

	printf("lcore %u: forwarding port %u RxQ%u → port %u TxQ%u\n",
	    lcore_id, conf->port_id, conf->rx_queue_id,
	    conf->tx_port_id, conf->tx_queue_id);

	while(!quit_signal) {
		/* 收包 */
		nb_rx = rte_eth_rx_burst(conf->port_id,
		    conf->rx_queue_id, bufs, BURST_SIZE);
		if(nb_rx == 0)
			continue;

		/* 发包 */
		nb_tx = rte_eth_tx_burst(conf->tx_port_id,
		    conf->tx_queue_id, bufs, nb_rx);

		/* 释放未成功发送的 mbuf */
		if(nb_tx < nb_rx)
			rte_pktmbuf_free_bulk(&bufs[nb_tx],
			    nb_rx - nb_tx);
	}
	return 0;
}

static void
sighandler(int sig)
{
	(void)sig;
	quit_signal = 1;
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("EAL init failed\n");

	signal(SIGINT, sighandler);

	/*
	 * 配置 lcore ↔ port/queue 映射 (简化示例)
	 * 生产中应根据 NUMA 拓扑自动分配
	 */
	lcore_conf[1] = (struct lcore_conf){0, 0, 0, 1};
	lcore_conf[2] = (struct lcore_conf){0, 1, 1, 1};
	lcore_conf[3] = (struct lcore_conf){1, 0, 0, 0};

	/* 省略: 端口配置、队列初始化、内存池创建 ... */

	/* 启动所有 worker */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(lcore_worker, NULL, lcore_id);
	}

	/* master 等待退出信号 */
	rte_eal_mp_wait_lcore();
	rte_eal_cleanup();
	return 0;
}
```

---

## 6. 最佳实践与陷阱

### CPU Affinity 和 Cache 策略

```
 ✅ Best Practices:

 1. One lcore per physical core (avoid HT interference)
    - Hyperthreading shares L1/L2 cache → contention
    - Production: disable HT or use only one HT set

 2. Worker and NIC on same NUMA node
    - lcore 2 handles Port 0 → ensure CPU 2 & Port 0 on NUMA 0

 3. Use isolcpus to isolate dataplane cores
    - Avoid kernel thread interference (kworker, ksoftirqd, etc.)

 4. Cache-line align structs
    struct lcore_conf {
        ...
    } __rte_cache_aligned;  /* avoid false sharing */

 5. Master lcore does not handle dataplane
    - Reserve for management, stats, config (control plane)
```

> 推荐的 Lcore 使用方式：一核一 worker、同 NUMA 部署、隔离数据面核心、结构体按 cache line 对齐、Master 仅做控制面。

### False Sharing (伪共享) 详解

```
 ❌ False Sharing Example:

 struct stats {        /* sizeof = 16 bytes */
     uint64_t rx_pkts; /* lcore 1 writes frequently */
     uint64_t tx_pkts; /* lcore 2 writes frequently */
 };
 struct stats global_stats;  /* both fields on same cache line! */

 CPU 1 writes rx_pkts → entire cache line invalidated
 CPU 2 writes tx_pkts → must fetch from CPU 1's cache
 → frequent cache line bouncing → severe perf drop

 ✅ Fix:

 struct stats {
     uint64_t rx_pkts;
     uint64_t padding[7];  /* pad to 64 bytes */
     uint64_t tx_pkts;
 } __rte_cache_aligned;

 Or use per-lcore stats (recommended):

 static uint64_t rx_pkts[RTE_MAX_LCORE] __rte_cache_aligned;
 static uint64_t tx_pkts[RTE_MAX_LCORE] __rte_cache_aligned;
```

> 伪共享：多个核心频繁修改同一 cache line 内的不同字段，导致 cache line 在 CPU 间来回弹跳、性能骤降。通过对齐或 per-lcore 变量可避免。

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| Worker 函数不是死循环 | Lcore 立即退出 | Worker 中使用 `while(!quit)` 循环 |
| 未检查 lcore NUMA | 跨 socket 访问延迟 | `rte_lcore_to_socket_id()` 验证 |
| 超线程两个核都用 | 吞吐只增加 ~10-20% | 只使用一组 HT |
| 所有统计放全局变量 | 随核心数性能下降 | 使用 per-lcore 统计 |
| Master 参与数据面 | 管理命令延迟高 | Master 仅做控制面 |

---

## 7. 知识检查 (Knowledge Check)

> **问题：** 一台服务器有 2 个 NUMA 节点，每个节点 8 个物理核心（共 16 核，HT 关闭）。NUMA 0 上连接了一块 25Gbps 网卡，NUMA 1 上连接了另一块 25Gbps 网卡。
>
> 1. 你会如何分配 Lcore？Master lcore 放在哪个 NUMA 节点？
> 2. 如果使用 Run-to-Completion 模型，每个 lcore 处理一个 Rx 队列。你需要多少个 Rx 队列才能达到线速（25Gbps 64B 包 ≈ 37Mpps）？假设单核处理能力约 14Mpps。
> 3. 为什么 DPDK 选择 busy-polling 而不是 epoll + blocking？这种选择的代价是什么？

### 参考答案

**Q1：分配方案如下：**

```
NUMA 0 (8 核: CPU 0-7, 连接 25G NIC-A):
  - CPU 0: Master lcore (控制面, 管理, 统计)
  - CPU 1-7: 7 个 Worker lcore → 处理 NIC-A 的 Rx/Tx 队列

NUMA 1 (8 核: CPU 8-15, 连接 25G NIC-B):
  - CPU 8-15: 8 个 Worker lcore → 处理 NIC-B 的 Rx/Tx 队列
```

**Master lcore 放在 NUMA 0**（或任一节点都可以），理由：
- Master lcore 主要做控制面操作（配置变更、统计收集、CLI 响应），不处理数据面高频包，对延迟不敏感。
- 关键原则是**每块网卡的 Worker lcore 必须与网卡在同一 NUMA 节点**，Master 放哪里影响不大。
- 如果 Master 还兼做管理口网卡的处理，那就跟随管理口网卡所在节点。

**Q2：至少需要 3 个 Rx 队列（3 个 Worker lcore）。**

计算过程：
- 25Gbps 64B 小包线速 ≈ 37Mpps
- 单核处理能力 ≈ 14Mpps
- 37 / 14 ≈ 2.64 → 向上取整 = **3 个核心/队列**
- 3 × 14 = 42Mpps > 37Mpps ✅ 有余量

**但实际需考虑：**
- 14Mpps 是理想值，真实处理逻辑（查表、ACL、NAT）会降低单核吞吐。
- 建议预留 20-30% 余量 → **4 个队列**更稳妥。
- 流量不一定均匀分布到各队列（RSS 哈希可能不完美），某些队列可能过载。

**Q3：DPDK 选择 busy-polling 的核心原因是消除延迟。**

**为什么不用 epoll + blocking：**
- `epoll_wait()` 是系统调用，每次调用 ~200ns 上下文切换开销。
- 阻塞时线程被挂起，被唤醒时需要调度延迟 ~1-5μs。
- 100Gbps 64B 包，每包处理预算仅 ~6.7ns，一次系统调用就损失 30-750 个包的时间。
- 唤醒后 L1/L2 cache 已冷，需要重新加热 → 额外数百 ns。

**busy-polling 的优势：**
- 零系统调用、零上下文切换、零调度延迟。
- CPU cache 始终保持温热状态（循环访问相同的描述符/mbuf 数据结构）。
- 可预测的延迟，无抖动 (jitter)。

**代价：**
- **CPU 100% 占用**：即使没有流量，CPU 也在全速空转轮询，功耗 ~100-150W/核。
- **核心独占**：绑定给 DPDK 的核心不能被其他进程使用，需要"牺牲"物理核心。
- **运维成本**：需要 `isolcpus`、`nohz_full` 等内核参数配合隔离核心。
- **低流量场景浪费**：如果流量只有 1Gbps，用 8 个核心轮询是严重浪费。

**折中方案：** DPDK 也支持中断+轮询混合模式（`rte_eth_dev_rx_intr_enable()`）—— 无流量时休眠等中断唤醒，有流量后切换到 busy-poll，适合流量波动大的场景。

---

*上一章：[EAL](./02-eal.md) | 下一章：[NUMA](./04-numa.md)*
