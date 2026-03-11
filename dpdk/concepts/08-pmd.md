# DPDK 深度解析 (8/9)：PMD — 轮询模式驱动

---

## 1. 痛点 (The "Why")

### 内核网络驱动的中断噩梦

```
┌─────────────────────────────────────────────────────────┐
│          传统中断驱动模型 vs 轮询模式                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ═══ 传统中断模型 (Interrupt-Driven) ═══                │
│                                                         │
│  NIC 收到包                                              │
│    │                                                     │
│    ▼                                                     │
│  触发硬中断 (IRQ)          ← ~5 μs                      │
│    │                                                     │
│  CPU 保存上下文             ← cache 污染                 │
│    │                                                     │
│  中断处理程序 (top half)    ← 关闭中断                   │
│    │                                                     │
│  触发软中断 (softirq)       ← NAPI poll                 │
│    │                                                     │
│  软中断处理 (bottom half)                                │
│    │                                                     │
│  协议栈处理 (netif_receive_skb)                          │
│    │                                                     │
│  送到 socket buffer                                      │
│    │                                                     │
│  唤醒用户态进程             ← 又一次上下文切换             │
│    │                                                     │
│  copy_to_user              ← 数据拷贝                    │
│                                                         │
│  总延迟: ~10-50 μs/包                                    │
│  100Gbps 64B 包: 148M 次中断/秒 → 中断风暴!             │
│                                                         │
│  ═══ DPDK 轮询模式 (Poll Mode) ═══                      │
│                                                         │
│  Worker Lcore (独占 CPU):                                │
│    while(1) {                                            │
│        nb_rx = rte_eth_rx_burst(port, q, bufs, 32);     │
│        if(nb_rx > 0)                                     │
│            process_and_forward(bufs, nb_rx);             │
│    }                                                     │
│                                                         │
│  ✅ 无中断, 无上下文切换                                  │
│  ✅ 无系统调用, 无内核协议栈                               │
│  ✅ NIC DMA 直接写入用户态 mbuf                           │
│  ✅ 批量处理 (burst), 摊薄开销                            │
│                                                         │
│  单包延迟: ~200ns                                        │
│  吞吐: 14-20 Mpps/core (64B 包)                         │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 核心概念与架构

### 2.1 PMD 架构全景

```
 ┌──────────────────────────────────────────────────────────┐
 │                    Userspace Application                  │
 │                                                          │
 │   rte_eth_rx_burst()          rte_eth_tx_burst()         │
 │        │                           │                     │
 │        ▼                           ▼                     │
 │   ┌──────────────────────────────────────────────┐       │
 │   │              ethdev API Layer                 │       │
 │   │   Unified device abstraction (rte_eth_dev)    │       │
 │   │   rx_pkt_burst / tx_pkt_burst function ptrs   │       │
 │   └──────────────┬───────────────────────────────┘       │
 │                  │                                        │
 │   ┌──────────────┴───────────────────────────────┐       │
 │   │              PMD Driver Layer                 │       │
 │   │                                              │       │
 │   │  ┌────────┐ ┌────────┐ ┌────────┐ ┌──────┐  │       │
 │   │  │ ixgbe  │ │  i40e  │ │  mlx5  │ │virtio│  │       │
 │   │  │ PMD    │ │  PMD   │ │  PMD   │ │ PMD  │  │       │
 │   │  └───┬────┘ └───┬────┘ └───┬────┘ └──┬───┘  │       │
 │   └──────┼──────────┼──────────┼─────────┼──────┘       │
 │          │          │          │         │               │
 └──────────┼──────────┼──────────┼─────────┼───────────────┘
            │          │          │         │
      ┌─────┴─────┐   │    ┌─────┴────┐    │
      │  UIO/VFIO │   │    │ bifurcated │   │
      │(userspace)│   │    │(part kernel)│  │
      └─────┬─────┘   │    └─────┬────┘    │
            │          │          │         │
 ═══════════╪══════════╪══════════╪═════════╪═══ Hardware
            │          │          │         │
 ┌──────────┴──────────┴──────────┴─────────┴────────────┐
 │                       NIC Hardware                     │
 │   ┌──────────┐                    ┌──────────┐        │
 │   │ Rx Queue │  DMA    Hugepage   │ Tx Queue │        │
 │   │ (HW Ring)│ ◄─────► (mbuf)    │ (HW Ring)│        │
 │   └──────────┘                    └──────────┘        │
 └───────────────────────────────────────────────────────┘
```

> PMD 架构分层：应用调用 ethdev API，ethdev 通过函数指针分发到各厂商 PMD（ixgbe、i40e、mlx5、virtio 等）；UIO/VFIO 实现完全用户态访问，bifurcated 模式则保留内核驱动；底层 NIC 通过 DMA 与 hugepage 上的 mbuf 交互。

### 2.2 Rx/Tx 描述符环 (Descriptor Ring)

```
 ═══ Rx Descriptor Ring ═══

 CPU view:                             NIC DMA engine view:
 ┌─────────────────────────────────────────────────────┐
 │                                                     │
 │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ │
 │  │Desc │ │Desc │ │Desc │ │Desc │ │Desc │ │Desc │ │
 │  │  0  │ │  1  │ │  2  │ │  3  │ │  4  │ │  5  │ │
 │  │     │ │     │ │     │ │     │ │     │ │     │ │
 │  │buf= │ │buf= │ │DD=1 │ │DD=1 │ │DD=0 │ │DD=0 │ │
 │  │NULL │ │NULL │ │data │ │data │ │wait │ │wait │ │
 │  │ used  │ used  │ready│ │ready│ │DMA  │ │DMA  │ │
 │  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ │
 │                    ▲ rx_tail           ▲            │
 │                    │ (CPU consumes     │            │
 │                    │  from here)      │            │
 │                    │                   │            │
 │                                   NIC DMA writes   │
 │                                   here             │
 │                                                     │
 └─────────────────────────────────────────────────────┘

 Rx descriptor structure (ixgbe example):
 struct ixgbe_adv_rx_desc {
     union {
         /* Read format (CPU → NIC): 告诉 NIC mbuf 物理地址 */
         struct {
             uint64_t pkt_addr;     /* mbuf 数据区物理地址 */
             uint64_t hdr_addr;     /* header split 地址 */
         } read;
         /* Write-back format (NIC → CPU): NIC 写回包信息 */
         struct {
             uint32_t status_error; /* DD=1 表示包已到达 */
             uint16_t length;       /* 包长度 */
             uint16_t vlan_tag;
             ...
         } wb;
     };
 };

 rx_burst() internal flow:
 1. Read desc[rx_tail].status → DD=1? (Descriptor Done)
 2. DD=1 → packet arrived, get mbuf from descriptor
 3. Write new empty mbuf into consumed descriptor (refill)
 4. Update rx_tail register to notify NIC
 5. Return received mbuf array to application
```

> Rx 描述符环：CPU 从 rx_tail 消费 DD=1 的已就绪描述符，NIC DMA 从另一侧写入新包；消费后需 refill 空 mbuf，并更新 rx_tail 通知 NIC。

```
 ═══ Tx Descriptor Ring ═══

 ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐
 │Desc │ │Desc │ │Desc │ │Desc │ │Desc │ │Desc │
 │  0  │ │  1  │ │  2  │ │  3  │ │  4  │ │  5  │
 │     │ │     │ │     │ │     │ │     │ │     │
 │DD=1 │ │DD=1 │ │DD=0 │ │DD=0 │ │free │ │free │
 │sent │ │sent │ │in   │ │in   │ │     │ │     │
 │     │ │     │ │trans│ │trans│ │     │ │     │
 └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘
  ▲ clean             ▲ tx_tail
  │                   │ (NIC sends up to here)
  │
  CPU reclaims mbufs for sent packets

 tx_burst() internal flow:
 1. Check free descriptor count (must be ≥ packets to send)
 2. Maybe clean first: check DD=1 sent descriptors, free their mbufs
 3. Write phys addr of mbufs to descriptors
 4. Set offload info (checksum, TSO, etc.)
 5. Update tx_tail register → NIC starts DMA read and transmit
```

> Tx 描述符环：CPU 将待发 mbuf 写入描述符并更新 tx_tail，NIC 从该位置开始 DMA 取数发送；发送完成后 NIC 将描述符 DD 置 1，CPU 可回收 mbuf；需注意 clean 策略（默认立即回收 vs tx_free_thresh 延迟回收）。

### 2.3 设备绑定与驱动

```
 NIC switching between kernel driver ↔ DPDK PMD:

 Initial state:
 ┌─────────────────┐
 │ NIC: 0000:03:00 │ ← kernel driver (ixgbe/i40e/mlx5_core)
 │ Driver: ixgbe   │    ethX exists, normal use
 └─────────────────┘

 Bound to DPDK:
 ┌─────────────────┐
 │ NIC: 0000:03:00 │ ← vfio-pci / uio_pci_generic
 │ Driver: vfio-pci│    ethX gone! DPDK-only access
 └─────────────────┘

 Driver comparison:
 ┌───────────────────┬────────────────┬────────────────────┐
 │                   │   UIO          │   VFIO             │
 ├───────────────────┼────────────────┼────────────────────┤
 │ IOMMU support     │ ✗ (no protect) │ ✅ (DMA isolate)   │
 │ Interrupt support │ basic          │ MSI-X              │
 │ Security          │ needs root     │ can run non-root   │
 │ Container-friendly│ ✗              │ ✅ (device pass)    │
 │ SR-IOV VF         │ ✗              │ ✅                  │
 │ Recommended use   │ old kernel/debug│ production (prefer)│
 └───────────────────┴────────────────┴────────────────────┘

 Special: Mellanox (mlx5) — no unbind needed!
 ┌──────────────────────────────────────────────────────┐
 │ mlx5 PMD uses "bifurcated driver" mode:               │
 │ - Kernel driver (mlx5_core) stays loaded              │
 │ - Data plane in userspace via Verbs API               │
 │ - Control plane still via kernel (link, stats)        │
 │ - Benefit: no dpdk-devbind, coexists with kernel      │
 └──────────────────────────────────────────────────────┘
```

> 设备绑定：NIC 可由内核驱动或 DPDK（UIO/VFIO）接管；绑定后 ethX 消失，仅 DPDK 可访问。UIO 无 IOMMU 需 root，VFIO 支持 DMA 隔离和容器；Mellanox mlx5 支持 bifurcated 模式，无需解绑即可与内核共存。

---

## 3. 使用与配置

### 3.1 设备绑定操作

```bash
# 查看网卡当前状态
dpdk-devbind.py --status

# 输出示例:
# Network devices using kernel driver
# ====================================
# 0000:03:00.0 'Ethernet Controller X710' if=eth0 drv=i40e active
# 0000:03:00.1 'Ethernet Controller X710' if=eth1 drv=i40e unused
#
# Network devices using DPDK-compatible driver
# =============================================
# <none>

# 加载 VFIO 模块
sudo modprobe vfio-pci

# 关闭接口 (解绑前必须)
sudo ip link set eth1 down

# 绑定到 VFIO
sudo dpdk-devbind.py -b vfio-pci 0000:03:00.1

# 验证
dpdk-devbind.py --status
# 0000:03:00.1 'Ethernet Controller X710' drv=vfio-pci unused=i40e

# 恢复到内核驱动
sudo dpdk-devbind.py -b i40e 0000:03:00.1
```

### 3.2 VFIO + IOMMU 配置

```bash
# GRUB 启用 IOMMU
GRUB_CMDLINE_LINUX="intel_iommu=on iommu=pt"
# AMD 系统: amd_iommu=on

sudo update-grub && sudo reboot

# 验证 IOMMU
dmesg | grep -i iommu
# DMAR: IOMMU enabled

# 不使用 IOMMU 的 VFIO (测试用)
echo 1 | sudo tee /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
```

### 3.3 端口配置参数

```c
struct rte_eth_conf port_conf = {
    .rxmode = {
        .mq_mode = RTE_ETH_MQ_RX_RSS,     /* RSS 多队列 */
        .max_lro_pkt_size = 0,
        .offloads = (
            RTE_ETH_RX_OFFLOAD_CHECKSUM |  /* 硬件校验 */
            RTE_ETH_RX_OFFLOAD_RSS_HASH    /* RSS hash */
        ),
    },
    .txmode = {
        .mq_mode = RTE_ETH_MQ_TX_NONE,
        .offloads = (
            RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | /* 硬件计算 */
            RTE_ETH_TX_OFFLOAD_TCP_CKSUM  |
            RTE_ETH_TX_OFFLOAD_MULTI_SEGS   /* 多段发送 */
        ),
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,  /* 使用默认 RSS key */
            .rss_hf = (
                RTE_ETH_RSS_IP |
                RTE_ETH_RSS_TCP |
                RTE_ETH_RSS_UDP
            ),
        },
    },
};
```

---

## 4. 关键 API

```c
/*
 * rte_eth_dev_configure — 配置端口
 * @port_id:       端口号
 * @nb_rx_queue:   Rx 队列数
 * @nb_tx_queue:   Tx 队列数
 * @eth_conf:      端口配置结构体
 */
int rte_eth_dev_configure(uint16_t port_id,
    uint16_t nb_rx_queue, uint16_t nb_tx_queue,
    const struct rte_eth_conf *eth_conf);

/*
 * rte_eth_rx_queue_setup — 配置 Rx 队列
 * @nb_rx_desc: 描述符数量 (ring 大小, 通常 512-4096)
 * @socket_id:  NUMA socket
 * @rx_conf:    队列配置 (NULL = 默认)
 * @mb_pool:    mbuf 内存池 (NUMA 对齐!)
 */
int rte_eth_rx_queue_setup(uint16_t port_id,
    uint16_t rx_queue_id, uint16_t nb_rx_desc,
    unsigned socket_id,
    const struct rte_eth_rxconf *rx_conf,
    struct rte_mempool *mb_pool);

/*
 * rte_eth_tx_queue_setup — 配置 Tx 队列
 */
int rte_eth_tx_queue_setup(uint16_t port_id,
    uint16_t tx_queue_id, uint16_t nb_tx_desc,
    unsigned socket_id,
    const struct rte_eth_txconf *tx_conf);

/*
 * rte_eth_dev_start / rte_eth_dev_stop
 */
int rte_eth_dev_start(uint16_t port_id);
int rte_eth_dev_stop(uint16_t port_id);

/*
 * rte_eth_rx_burst — 批量收包 (数据面热路径!)
 * @nb_pkts: 最大收包数 (burst size)
 * 返回:     实际收到的包数
 *
 * 性能关键: 这个函数被 inline, 直接调用 PMD 的函数指针
 * → 零间接调用开销
 */
static inline uint16_t
rte_eth_rx_burst(uint16_t port_id, uint16_t queue_id,
    struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

/*
 * rte_eth_tx_burst — 批量发包
 * 返回: 实际入队的包数 (未入队的需要应用释放!)
 */
static inline uint16_t
rte_eth_tx_burst(uint16_t port_id, uint16_t queue_id,
    struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
```

---

## 5. 生产级代码示例

```c
#include <stdio.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024
#define BURST_SIZE      32
#define NUM_MBUFS       8191
#define MBUF_CACHE      250

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP,
		},
	},
};

/*
 * 完整的端口初始化流程
 */
static int
port_init(uint16_t port_id, struct rte_mempool *pool)
{
	struct rte_eth_dev_info dev_info;
	uint16_t nb_rxd = RX_DESC_DEFAULT;
	uint16_t nb_txd = TX_DESC_DEFAULT;
	int socket_id;
	int ret;
	uint16_t q;

	/* 获取设备信息 */
	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if(ret != 0)
		return ret;

	printf("port %u: driver=%s, max_rx_queues=%u, "
	    "max_tx_queues=%u\n",
	    port_id, dev_info.driver_name,
	    dev_info.max_rx_queues, dev_info.max_tx_queues);

	socket_id = rte_eth_dev_socket_id(port_id);

	/* 调整描述符数量到设备支持的范围 */
	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id,
	    &nb_rxd, &nb_txd);
	if(ret != 0)
		return ret;

	/* 配置端口: 1 Rx + 1 Tx 队列 */
	ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
	if(ret != 0)
		return ret;

	/* 配置 Rx 队列 */
	ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
	    socket_id, NULL, pool);
	if(ret < 0)
		return ret;

	/* 配置 Tx 队列 */
	ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
	    socket_id, NULL);
	if(ret < 0)
		return ret;

	/* 启动端口 */
	ret = rte_eth_dev_start(port_id);
	if(ret < 0)
		return ret;

	/* 开启混杂模式 (接收所有包) */
	rte_eth_promiscuous_enable(port_id);

	/* 打印 MAC 地址 */
	struct rte_ether_addr addr;
	rte_eth_macaddr_get(port_id, &addr);
	printf("port %u MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
	    port_id,
	    addr.addr_bytes[0], addr.addr_bytes[1],
	    addr.addr_bytes[2], addr.addr_bytes[3],
	    addr.addr_bytes[4], addr.addr_bytes[5]);

	return 0;
}

/*
 * 数据面主循环: 两端口互转
 */
static void
forwarding_loop(void)
{
	struct rte_mbuf *bufs[BURST_SIZE];
	uint16_t nb_rx, nb_tx, i;

	printf("Starting forwarding: port 0 <-> port 1\n");

	for(;;) {
		/* Port 0 → Port 1 */
		nb_rx = rte_eth_rx_burst(0, 0, bufs, BURST_SIZE);
		if(nb_rx > 0) {
			nb_tx = rte_eth_tx_burst(1, 0, bufs, nb_rx);
			for(i = nb_tx; i < nb_rx; i++)
				rte_pktmbuf_free(bufs[i]);
		}

		/* Port 1 → Port 0 */
		nb_rx = rte_eth_rx_burst(1, 0, bufs, BURST_SIZE);
		if(nb_rx > 0) {
			nb_tx = rte_eth_tx_burst(0, 0, bufs, nb_rx);
			for(i = nb_tx; i < nb_rx; i++)
				rte_pktmbuf_free(bufs[i]);
		}
	}
}

int
main(int argc, char **argv)
{
	struct rte_mempool *pool;
	int ret;

	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("EAL init failed\n");

	if(rte_eth_dev_count_avail() < 2)
		rte_panic("Need at least 2 ports\n");

	pool = rte_pktmbuf_pool_create("MBUF_POOL",
	    NUM_MBUFS * 2, MBUF_CACHE, 0,
	    RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if(pool == NULL)
		rte_panic("Cannot create pool\n");

	port_init(0, pool);
	port_init(1, pool);

	forwarding_loop();

	return 0;
}
```

---

## 6. 最佳实践与陷阱

### Burst Size 优化

```
 ┌──────────────────────────────────────────────────────┐
 │  Burst Size choice directly affects performance:      │
 │                                                      │
 │  Too small (1-4):                                    │
 │    → Low PCIe efficiency (small TLP)                 │
 │    → High call overhead per packet                   │
 │                                                      │
 │  Optimal (32-64):                                    │
 │    → 32 is DPDK recommended                          │
 │    → Matches NIC prefetch depth                      │
 │    → Full PCIe bandwidth utilization                 │
 │                                                      │
 │  Too large (>64):                                    │
 │    → Higher latency (wait for batch to fill)         │
 │    → Large stack arrays → cache pressure             │
 │    → Diminishing returns                             │
 └──────────────────────────────────────────────────────┘
```

> Burst Size 过小会降低 PCIe 利用率并放大调用开销；32–64 为推荐区间，与 NIC prefetch 和 DPDK 建议一致；过大则增加延迟和 cache 压力，收益递减。

### Tx 回收策略

```
 tx_burst → NIC sends → descriptor DD=1 → CPU can reclaim mbuf

 Two reclaim modes:
 ① In tx_burst (default):
    - Each tx_burst first cleans completed descriptors
    - Pro: automatic, app doesn't need to care
    - Con: if burst gap long, descriptors may run out

 ② Deferred reclaim (tx_free_thresh):
    - Set tx_free_thresh = 32
    - Reclaim only when free descriptors < 32
    - Pro: fewer reclaims, batch free more efficient

 struct rte_eth_txconf tx_conf = {
     .tx_free_thresh = 32,    /* 空闲描述符低于此值时回收 */
     .tx_rs_thresh = 32,      /* 每 N 个描述符设一次 RS bit */
 };
```

> Tx 回收：默认在 tx_burst 内立即 clean 已发送描述符；若 burst 间隔长可配置 tx_free_thresh 延迟回收，只在空闲描述符不足时批量回收，减少 overhead。

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| tx_burst 返回值未检查 | mbuf 泄漏/mempool 耗尽 | 释放所有未发送的 mbuf |
| RSS key 不匹配 | 流量分布不均 | 使用 Toeplitz hash 或对称 key |
| 描述符太少 | 丢包 (no buffer) | 至少 1024, 高速场景 4096 |
| mbuf pool 太小 | rx_burst 返回 0 | 参考 mempool 章节计算 |
| offload 不匹配 | 发包无 checksum | 检查 `dev_info.tx_offload_capa` |
| 忘记 `dev_start()` | 一切正常但收不到包 | 启动顺序检查 |

---

## 7. 知识检查 (Knowledge Check)

> **问题：**
>
> 1. `rte_eth_rx_burst()` 是 `static inline` 函数。为什么 DPDK 要把数据面最热的函数做成 inline？这跟传统内核驱动通过 `struct net_device_ops` 虚函数表调用 `ndo_start_xmit()` 有什么本质区别？
> 2. Rx 描述符中的 DD (Descriptor Done) bit 是由谁设置的？CPU 还是 NIC？如果 CPU 在 busy-poll 循环中检查 DD bit，这个操作是读寄存器还是读内存？（提示：考虑 DMA write-back）
> 3. 为什么 DPDK 推荐 VFIO 而不是 UIO？在容器化 (Docker/K8s) 部署场景下，VFIO 的 IOMMU 保护有什么具体好处？

### 参考答案

**Q1：inline 消除了函数调用开销和间接跳转的代价，这在纳秒级预算下至关重要。**

**DPDK `static inline` 的效果：**
- 编译器将 `rte_eth_rx_burst()` 的代码直接嵌入调用处，消除了：
  - **函数调用开销：** CALL + RET 指令（~2-5ns），栈帧建立/销毁。
  - **间接跳转惩罚：** 虽然内部仍有一层 `dev->rx_pkt_burst` 函数指针，但 inline 后编译器有机会通过 PGO（Profile-Guided Optimization）或 devirtualization 进一步优化。
  - **指令缓存 miss：** inline 后代码与调用者在同一个代码区域，利于 instruction cache 命中。

**与内核 `ndo_start_xmit()` 的本质区别：**

| 方面 | DPDK `rte_eth_rx_burst()` | 内核 `ndo_start_xmit()` |
|------|--------------------------|------------------------|
| 调用方式 | static inline + 函数指针 | 虚函数表间接调用 |
| 间接跳转 | 1 次（可优化） | 至少 1 次（不可 inline） |
| Retpoline | 不需要（用户态） | 需要（Spectre 缓解），每次间接跳转 +10-20ns |
| 上下文 | 用户态，无系统调用 | 内核态，从 socket 层到驱动经过多层调用 |
| 分支预测 | busy-loop 热路径，分支预测准确率高 | 中断上下文，代码路径多变，预测困难 |

**关键点：** 内核驱动因为 Spectre/Meltdown 缓解措施（Retpoline），每次间接调用增加 ~10-20ns。148Mpps 场景下这就是 1.5-3 GHz 的 CPU 算力纯浪费。DPDK 在用户态完全避免了这个问题。

**Q2：DD bit 由 NIC 硬件通过 DMA write-back 设置；CPU 检查 DD bit 是读内存，不是读寄存器。**

**详细流程：**

1. CPU 初始化时，将 Rx 描述符数组分配在 hugepage 上，把数组的**物理地址**写入 NIC 的 Rx Ring Base Address 寄存器。
2. CPU 填好描述符的 "read format"：将空 mbuf 的物理地址写入描述符的 `pkt_addr` 字段，DD bit 初始为 0。
3. NIC 收到包后：
   - DMA 引擎将包数据写入描述符指定的 mbuf 物理地址。
   - DMA 引擎将描述符 write-back 为 "write-back format"：**设置 DD=1**，同时写入包长度、RSS hash、VLAN 等信息。
   - 这个 write-back 是**NIC 通过 PCIe DMA 写入主内存**（不是 CPU 的寄存器）。
4. CPU 在 busy-poll 中检查 DD bit：`if (desc[tail].wb.status & DD_BIT)` —— 这是一次**普通内存读取**。

**为什么是读内存而不是读寄存器：**
- 描述符环在主内存（hugepage）中，NIC 通过 DMA 更新它。
- 如果 CPU 读 NIC 的 MMIO 寄存器来检查状态，每次读寄存器需要 PCIe 往返（~500ns-1μs），太慢了。
- 读内存只需 ~80ns（本地 DRAM），且 CPU cache 可以缓存描述符 → 如果 DD 还没被 DMA 更新，cache hit 只需 ~1-4ns。
- NIC DMA write-back 时会通过 cache coherence 协议（DDIO/DCA）把更新直接注入 CPU 的 L3 cache → 检查 DD 变为 L3 hit（~12ns）。

**Q3：VFIO 通过 IOMMU 提供 DMA 内存隔离和安全保护，这在多租户/容器场景下至关重要。**

**UIO 的安全缺陷：**
- UIO 将 NIC 的**全部 PCIe BAR 空间和 DMA 能力**直接暴露给用户态进程。
- 用户态进程可以编程 NIC 的 DMA 引擎**访问任意物理内存地址** → 可以读/写内核内存、其他进程内存。
- 本质上，UIO 下拿到 NIC 等于拿到了系统的全部内存访问权限 → 必须 root 运行。

**VFIO + IOMMU 的保护：**
- IOMMU 为每个设备建立独立的**IO 页表**，限制 NIC DMA 只能访问明确映射的物理内存区域。
- NIC 尝试 DMA 到未映射的地址 → IOMMU 拒绝并上报错误（DMA Remapping Fault）。

**容器化（Docker/K8s）场景的具体好处：**

1. **容器隔离：** 容器 A 的 DPDK 应用使用 NIC-A（通过 SR-IOV VF），容器 B 使用 NIC-B。IOMMU 保证 NIC-A 的 DMA 不能访问容器 B 的内存。没有 IOMMU，一个容器中的恶意代码可以通过 DMA 读取所有容器的数据。

2. **非 root 运行：** VFIO 可以通过 device cgroup 和文件权限将设备访问权限授予非 root 用户/容器，不需要 `--privileged`。

3. **SR-IOV VF 直通：** K8s 中的 SR-IOV Device Plugin 依赖 VFIO 将 VF 安全地分配给 Pod。每个 Pod 的 VF 受 IOMMU 限制，即使 Pod 被攻击，也无法通过 DMA 逃逸。

4. **热迁移支持：** VFIO 的地址翻译能力使得虚拟机/容器的内存重映射成为可能，支持在线迁移。

---

*上一章：[Mbuf](./07-mbuf.md) | 下一章：[KNI](./09-kni.md)*
