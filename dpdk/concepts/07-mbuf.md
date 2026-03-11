# DPDK 深度解析 (7/9)：Mbuf — 报文缓冲区

---

## 1. 痛点 (The "Why")

### 内核 sk_buff 的沉重代价

```
┌─────────────────────────────────────────────────────────┐
│       Linux sk_buff Bottlenecks in High-Speed Forwarding │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  1. sk_buff struct is huge (~256 bytes)                  │
│     - Contains state for all protocol stack layers       │
│     - Many fields unused in pure forwarding scenario     │
│     - Wastes precious cache space                        │
│                                                         │
│  2. Dynamic allocation                                  │
│     - Per packet RX: alloc_skb() → kmalloc()             │
│     - Per packet TX: kfree_skb()                         │
│     - SLAB/SLUB allocator becomes bottleneck at high freq│
│                                                         │
│  3. Data copying                                        │
│     - DMA writes to sk_buff data area                    │
│     - Multiple skb_copy possible during stack processing │
│     - Send to userspace: copy_to_user()                  │
│     - 100Gbps = 12.5GB/s pure data copy → CPU overwhelmed│
│                                                         │
│  4. Reference counting                                  │
│     - skb->users atomic operations                       │
│     - Atomic ops become bottleneck at high frequency     │
│                                                         │
│  DPDK rte_mbuf:                                         │
│  ✅ Only 128B metadata (half of sk_buff)                 │
│  ✅ Allocated from mempool → zero malloc/free            │
│  ✅ Zero-copy: NIC DMA writes directly to mbuf data area│
│  ✅ Refcnt: used only when needed, supports batch free   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> **中文说明：** Linux 的 sk_buff 在高速转发中存在四大瓶颈：结构体庞大、动态分配开销、数据拷贝以及引用计数的原子操作。DPDK 的 rte_mbuf 通过更小的元数据、mempool 预分配、零拷贝以及按需引用计数，有效规避了这些问题。

---

## 2. 核心概念与架构

### 2.1 rte_mbuf 内存布局

```
 Complete mbuf layout in mempool:

 Low addr
 ┌───────────────────────────────────────────────┐ ◄── m (rte_mbuf *)
 │              struct rte_mbuf (128B)            │
 │                                                │
 │  ┌────────────────────────────────────────┐   │
 │  │ buf_addr ──────────────────────────┐   │   │  Points to data buffer
 │  │ buf_iova          (DMA phys addr)  │   │   │  start
 │  │ data_off = 128    (headroom)       │   │   │
 │  │ refcnt   = 1                       │   │   │
 │  │ nb_segs  = 1                       │   │   │
 │  │ port     = port number             │   │   │
 │  │ ol_flags = offload flags           │   │   │
 │  │ pkt_len  = total packet length     │   │   │
 │  │ data_len = this segment data len   │   │   │
 │  │ vlan_tci                            │   │   │
 │  │ hash     (RSS hash / flow director) │   │   │
 │  │ tx_offload (TSO/checksum offload)   │   │   │
 │  │ pool     = owning mempool ptr      │   │   │
 │  │ next     = next segment (chained)  │   │   │
 │  │ ...                                 │   │   │
 │  └────────────────────────────────────┘   │   │
 ├───────────────────────────────────────────┤   │
 │        Private Area (optional, priv_size)  │   │
 │        Application-defined metadata        │   │
 ├───────────────────────────────────────────┤◄──┘ buf_addr
 │        Headroom (RTE_PKTMBUF_HEADROOM)     │
 │        Default 128 bytes                   │
 │        For prepending new protocol headers  │
 ├───────────────────────────────────────────┤◄── rte_pktmbuf_mtod(m)
 │                                           │     = buf_addr + data_off
 │        Packet Data                        │
 │        Actual packet content               │
 │        (Ethernet + IP + TCP/UDP + ...)    │
 │                                           │
 │        Length = data_len                   │
 │                                           │
 ├───────────────────────────────────────────┤
 │        Tailroom                           │
 │        Remaining space (append-able)       │
 │                                           │
 └───────────────────────────────────────────┘◄── buf_addr + buf_len
 High addr

 Default sizes:
   mbuf metadata: 128B
   headroom:      128B (RTE_PKTMBUF_HEADROOM)
   data+tail:     2048B (RTE_MBUF_DEFAULT_DATAROOM)
   buf_len:       2048 + 128 = 2176B
```

> **中文说明：** 一个 mbuf 在 mempool 中的完整内存布局：低地址是 128B 的 rte_mbuf 结构体（含 buf_addr、data_off、refcnt 等字段），后接可选的私有区、headroom、实际包数据区和 tailroom。buf_addr 指向 headroom 起始，data_off 决定包数据起始，默认 headroom 128B，数据区 2048B。

### 2.2 关键指针关系

```
 m->buf_addr                           m->buf_addr + m->buf_len
 │                                                │
 ▼                                                ▼
 ┌──────────┬────────────────────┬───────────────┐
 │ headroom │    packet data     │   tailroom    │
 └──────────┴────────────────────┴───────────────┘
 │          ▲                    ▲               │
 │          │                    │               │
 │    m->data_off          data_off +            │
 │     (128)               data_len              │
 │                                               │
 │◄───────────── m->buf_len (2176) ────────────►│

 Get data pointer:
   rte_pktmbuf_mtod(m, type) = (type)(m->buf_addr + m->data_off)

 Available space:
   headroom = m->data_off                        (for prepend)
   tailroom = m->buf_len - m->data_off - m->data_len (for append)
```

> **中文说明：** buf_addr 指向整个 buffer 起始，data_off 指向包数据起始（即 headroom 结束处），data_off + data_len 为包数据结束。headroom 可用于 prepend，tailroom 可用于 append。

### 2.3 多段 Mbuf (Chained/Scattered)

```
 Jumbo Frame (>1518B) or TSO scenario:
 Single mbuf data area insufficient → chain multiple mbufs via linked list

 ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
 │ mbuf segment 0   │    │ mbuf segment 1   │    │ mbuf segment 2   │
 │                  │    │                  │    │                  │
 │ nb_segs = 3      │    │ nb_segs = 0     │    │ nb_segs = 0     │
 │ pkt_len = 4500   │    │ data_len = 1500  │    │ data_len = 1500  │
 │ data_len = 1500  │    │                  │    │                  │
 │                  │    │                  │    │                  │
 │ next ────────────┼───►│ next ────────────┼───►│ next = NULL      │
 │                  │    │                  │    │                  │
 │ [1500B data]     │    │ [1500B data]     │    │ [1500B data]     │
 └──────────────────┘    └──────────────────┘    └──────────────────┘

 Head segment:
   pkt_len  = total packet length (4500)
   nb_segs  = total segment count (3)
   data_len = this segment data length (1500)

 Subsequent segments:
   data_len = this segment data length
   next     = ptr to next segment (NULL for last)
```

> **中文说明：** 巨型帧或 TSO 场景下单 mbuf 数据区不足，通过 next 指针将多个 mbuf 链成链表。头 segment 记录 pkt_len 和 nb_segs，每个 segment 存本段 data_len 和下一段 next 指针。

### 2.4 间接 Mbuf (Indirect Mbuf) — 零拷贝克隆

```
 Scenario: multicast/mirror — same packet sent to multiple ports

 ❌ Naive approach: memcpy entire packet
 ✅ Zero-copy: indirect mbuf shares data area

 ┌──────────────────┐     ┌──────────────────┐
 │ Direct Mbuf      │     │ Indirect Mbuf    │
 │ (original)       │     │ (clone)          │
 │                  │     │                  │
 │ refcnt = 2       │◄────│ Points to same   │
 │                  │     │ buf_addr         │
 │ ┌──────────────┐ │     │                  │
 │ │  packet data │ │     │ ol_flags |=      │
 │ │  (shared!)   │ │     │  IND_ATTACHED    │
 │ └──────────────┘ │     │                  │
 └──────────────────┘     └──────────────────┘

 rte_pktmbuf_clone(m, pool)
   → Alloc new mbuf (indirect) from pool
   → indirect->buf_addr = direct->buf_addr
   → direct->refcnt++
   → No data copy!

 On free:
   rte_pktmbuf_free(indirect)
   → refcnt--
   → refcnt > 0 → do not free data area
   → refcnt == 0 → return to mempool
```

> **中文说明：** 组播/镜像场景下，indirect mbuf 通过共享 direct mbuf 的 buf_addr 实现零拷贝克隆。克隆时分配新 mbuf 元数据并递增 refcnt，释放时递减 refcnt，仅当为 0 时才归还数据区到 mempool。

---

## 3. 使用与配置

### 3.1 Mbuf 大小计算

```
 Default values:
   RTE_PKTMBUF_HEADROOM      = 128  (tunable via build options)
   RTE_MBUF_DEFAULT_DATAROOM = 2048
   RTE_MBUF_DEFAULT_BUF_SIZE = RTE_MBUF_DEFAULT_DATAROOM
                              + RTE_PKTMBUF_HEADROOM
                              = 2176

 Jumbo frame:
   9000B MTU + 14B Eth + 4B CRC + 4B VLAN = 9022B
   buf_size >= 9022 + 128(headroom) = 9150B required
   or use scattered rx (multi-segment mbuf)

 Minimal mode (small packets only):
   64B min frame + headroom = 192B
   → smaller mbufs → more in mempool → less memory
```

> **中文说明：** 默认 headroom 128B、数据区 2048B；巨型帧需更大 buf_size 或使用多段 mbuf；小包场景可用更小 buf_size 以节省内存。

### 3.2 Offload Flags

```
 ol_flags field: describes NIC hardware offload info

 Rx direction (NIC → mbuf):
   RTE_MBUF_F_RX_IP_CKSUM_GOOD   IP checksum verified OK
   RTE_MBUF_F_RX_L4_CKSUM_GOOD   TCP/UDP checksum verified OK
   RTE_MBUF_F_RX_RSS_HASH        RSS hash computed
   RTE_MBUF_F_RX_VLAN            VLAN tag stripped

 Tx direction (mbuf → NIC):
   RTE_MBUF_F_TX_IP_CKSUM        Request NIC to compute IP checksum
   RTE_MBUF_F_TX_TCP_CKSUM       Request NIC to compute TCP checksum
   RTE_MBUF_F_TX_TCP_SEG         Request TSO (TCP Segmentation)
   RTE_MBUF_F_TX_VLAN            Request NIC to insert VLAN tag
```

> **中文说明：** ol_flags 描述 NIC 硬件卸载信息：Rx 方向表示校验和/ RSS/VLAN 等是否已由硬件完成，Tx 方向表示是否请求 NIC 计算校验和、做 TSO 或插入 VLAN。

---

## 4. 关键 API

```c
/*
 * rte_pktmbuf_alloc — 从 pool 分配一个 mbuf
 * 返回: mbuf 指针, 失败 NULL
 */
struct rte_mbuf *rte_pktmbuf_alloc(struct rte_mempool *pool);

/*
 * rte_pktmbuf_alloc_bulk — 批量分配 (推荐)
 * 返回: 0 成功, -ENOENT 不足
 */
int rte_pktmbuf_alloc_bulk(struct rte_mempool *pool,
    struct rte_mbuf **mbufs, unsigned count);

/*
 * rte_pktmbuf_free — 释放 mbuf (含链式段)
 * 递减 refcnt, 为 0 时归还 mempool
 */
void rte_pktmbuf_free(struct rte_mbuf *m);

/*
 * rte_pktmbuf_free_bulk — 批量释放 (推荐, 更高效)
 */
void rte_pktmbuf_free_bulk(struct rte_mbuf **mbufs,
    unsigned count);

/*
 * rte_pktmbuf_mtod — 获取数据区指针 (带类型转换)
 * 展开: (type)(m->buf_addr + m->data_off)
 */
#define rte_pktmbuf_mtod(m, type)

/*
 * rte_pktmbuf_prepend — 在头部插入空间 (减小 data_off)
 * 返回: 新数据起始指针, 空间不够返回 NULL
 */
char *rte_pktmbuf_prepend(struct rte_mbuf *m, uint16_t len);

/*
 * rte_pktmbuf_append — 在尾部追加空间 (增大 data_len)
 * 返回: 追加区域起始指针
 */
char *rte_pktmbuf_append(struct rte_mbuf *m, uint16_t len);

/*
 * rte_pktmbuf_clone — 零拷贝克隆 (indirect mbuf)
 */
struct rte_mbuf *
rte_pktmbuf_clone(struct rte_mbuf *md,
    struct rte_mempool *mp);
```

---

## 5. 生产级代码示例

```c
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

/*
 * 构造一个 UDP 包并发送
 */
static int
send_udp_packet(uint16_t port_id, struct rte_mempool *pool)
{
	struct rte_mbuf *m;
	struct rte_ether_hdr *eth;
	struct rte_ipv4_hdr *ip;
	struct rte_udp_hdr *udp;
	char *payload;
	uint16_t pkt_len;

	m = rte_pktmbuf_alloc(pool);
	if(m == NULL)
		return -1;

	/* 预留 headroom 后开始填充 */
	/* Ethernet header */
	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	memset(&eth->dst_addr, 0xFF, RTE_ETHER_ADDR_LEN);
	rte_eth_macaddr_get(port_id, &eth->src_addr);
	eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

	/* IP header */
	ip = (struct rte_ipv4_hdr *)(eth + 1);
	memset(ip, 0, sizeof(*ip));
	ip->version_ihl = 0x45;
	ip->time_to_live = 64;
	ip->next_proto_id = IPPROTO_UDP;
	ip->src_addr = rte_cpu_to_be_32(0x0A000001);
	ip->dst_addr = rte_cpu_to_be_32(0x0A000002);

	/* UDP header */
	udp = (struct rte_udp_hdr *)(ip + 1);
	udp->src_port = rte_cpu_to_be_16(12345);
	udp->dst_port = rte_cpu_to_be_16(54321);

	/* Payload */
	payload = (char *)(udp + 1);
	memcpy(payload, "Hello DPDK!", 11);

	/* 设置包长度 */
	pkt_len = sizeof(*eth) + sizeof(*ip) + sizeof(*udp) + 11;
	udp->dgram_len = rte_cpu_to_be_16(sizeof(*udp) + 11);
	ip->total_length = rte_cpu_to_be_16(sizeof(*ip) +
	    sizeof(*udp) + 11);
	m->data_len = pkt_len;
	m->pkt_len = pkt_len;

	/* 请求 NIC 硬件计算 checksum */
	m->ol_flags |= (RTE_MBUF_F_TX_IP_CKSUM |
	    RTE_MBUF_F_TX_UDP_CKSUM);
	m->l2_len = sizeof(*eth);
	m->l3_len = sizeof(*ip);

	/* 发送 */
	if(rte_eth_tx_burst(port_id, 0, &m, 1) != 1) {
		rte_pktmbuf_free(m);
		return -1;
	}
	return 0;
}

/*
 * Rx → 解析 → 修改 → Tx 转发示例
 */
static void
forward_loop(uint16_t rx_port, uint16_t tx_port)
{
	struct rte_mbuf *bufs[32];
	uint16_t nb_rx, nb_tx, i;

	nb_rx = rte_eth_rx_burst(rx_port, 0, bufs, 32);
	if(nb_rx == 0)
		return;

	for(i = 0; i < nb_rx; i++) {
		struct rte_ether_hdr *eth;
		struct rte_ether_addr tmp;

		eth = rte_pktmbuf_mtod(bufs[i],
		    struct rte_ether_hdr *);

		/* 交换 MAC 地址 */
		rte_ether_addr_copy(&eth->dst_addr, &tmp);
		rte_ether_addr_copy(&eth->src_addr, &eth->dst_addr);
		rte_ether_addr_copy(&tmp, &eth->src_addr);

		/* prefetch 下一个包的数据 (减少 cache miss) */
		if(i + 1 < nb_rx)
			rte_prefetch0(rte_pktmbuf_mtod(bufs[i + 1],
			    void *));
	}

	nb_tx = rte_eth_tx_burst(tx_port, 0, bufs, nb_rx);

	/* 释放未发送的 */
	for(i = nb_tx; i < nb_rx; i++)
		rte_pktmbuf_free(bufs[i]);
}
```

---

## 6. 最佳实践与陷阱

### Prefetch 优化

```
 In dataplane hot path, each mbuf data access may cause cache miss.
 Solution: while processing current packet, prefetch next packet.

 for(i = 0; i < nb_rx; i++) {
     /* prefetch next packet's mbuf metadata */
     if(i + 1 < nb_rx)
         rte_prefetch0(bufs[i + 1]);

     /* prefetch next packet's data area */
     if(i + 1 < nb_rx)
         rte_prefetch0(rte_pktmbuf_mtod(bufs[i + 1], void *));

     /* process current packet (cache warmed up) */
     process(bufs[i]);
 }

 More aggressive prefetch (pipeline style):

 /* prefetch first 4 */
 for(i = 0; i < 4 && i < nb_rx; i++)
     rte_prefetch0(rte_pktmbuf_mtod(bufs[i], void *));

 for(i = 0; i < nb_rx; i++) {
     if(i + 4 < nb_rx)
         rte_prefetch0(rte_pktmbuf_mtod(bufs[i + 4], void *));
     process(bufs[i]);
 }
```

> **中文说明：** 数据面热路径中，mbuf 数据访问易产生 cache miss。在处理当前包时 prefetch 下一个包的元数据和数据区，可提前加载到 cache；更激进的做法是流水线式预取后续多个包。

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| `rte_pktmbuf_free` 后继续访问 | 数据损坏/崩溃 | 释放后立即置 NULL |
| 忘记释放 `tx_burst` 失败的包 | mempool 耗尽 | 检查返回值并释放 |
| headroom 不够 prepend | `rte_pktmbuf_prepend` 返回 NULL | 增大 headroom 或先 adj |
| 未设置 `data_len`/`pkt_len` | NIC 发送空包或垃圾 | 构造包后必须设置长度 |
| 修改 indirect mbuf 的共享数据 | 影响所有引用者 | clone 后若需修改, 先 linearize |
| `rte_pktmbuf_mtod` 跨段访问 | 读到下一段的头部 | 检查 `data_len` 或 linearize |

### Mbuf 与 sk_buff 对比

```
 ┌──────────────────┬───────────────┬───────────────────┐
 │                  │  rte_mbuf     │  sk_buff           │
 ├──────────────────┼───────────────┼───────────────────┤
 │ Metadata size    │ 128 bytes     │ ~256 bytes         │
 │ Allocation       │ mempool O(1) │ slab (may sleep)   │
 │ Data copy        │ zero-copy     │ at least 1-2x      │
 │ Zero-copy clone  │ indirect mbuf│ skb_clone          │
 │ HW offload       │ ol_flags      │ features           │
 │ Ref count        │ 16-bit refcnt │ atomic_t users     │
 │ Batch free       │ free_bulk     │ not supported      │
 │ NUMA-aware       │ pool per-node │ no                 │
 └──────────────────┴───────────────┴───────────────────┘
```

> **中文说明：** rte_mbuf 相比 sk_buff 在元数据大小、分配方式、数据拷贝、克隆、硬件卸载、引用计数、批量释放和 NUMA 感知等方面均有优势，更适配高速数据面。

---

## 7. 知识检查 (Knowledge Check)

> **问题：**
>
> 1. `rte_pktmbuf_mtod(m, struct rte_ether_hdr *)` 实际展开后是什么表达式？为什么它能直接得到以太网头指针而不需要跳过任何其他头部？
> 2. 在一个多播转发场景中，你收到一个包需要从 4 个端口发出。使用 `rte_pktmbuf_clone()` vs `memcpy` 各自的优缺点是什么？在什么情况下 clone 反而更慢？
> 3. 为什么 DPDK 默认的 headroom 是 128 字节？在什么场景下你会需要增大 headroom？

### 参考答案

**Q1：展开为 `(struct rte_ether_hdr *)((char *)m->buf_addr + m->data_off)`。**

宏定义链：
```c
#define rte_pktmbuf_mtod(m, t) ((t)((char *)(m)->buf_addr + (m)->data_off))
```

**为什么直接就是以太网头：**
- NIC 收到的原始帧，第一个字节就是以太网帧的目的 MAC 地址。NIC 的 DMA 引擎把整个帧数据原封不动地写入 mbuf 的数据区。
- `data_off` 初始值 = `RTE_PKTMBUF_HEADROOM`（128），即 headroom 的尾部。NIC DMA 从 `buf_addr + data_off` 开始写入，所以数据起始就是以太网帧的第一个字节。
- 没有任何额外头部需要跳过 —— 不像内核的 sk_buff 那样有多层 `skb->data` 指针偏移历史。DPDK 的 mbuf 拿到的就是"裸"的二层帧。

**注意：** 如果之前调用了 `rte_pktmbuf_prepend()` 或 `rte_pktmbuf_adj()`，`data_off` 会变化，`mtod` 返回的指针也随之移动。所以 `mtod` 始终指向"当前数据起始位置"，不一定总是以太网头。

**Q2：clone 和 memcpy 各有优劣，取决于包的后续处理方式。**

**`rte_pktmbuf_clone()`（间接 mbuf）：**
- **优点：**
  - 零拷贝 —— 4 个克隆 mbuf 共享同一份包数据，仅分配 4 个 mbuf 元数据（各 128B），无 2KB+ 数据拷贝。
  - 速度快：~50ns（分配 mbuf + 设置指针 + refcnt++），远快于 memcpy 2KB（~100-200ns）。
  - 节省内存带宽。
- **缺点：**
  - **不能修改包内容！** 4 个 clone 共享数据区，修改一个会影响所有。如果需要为不同端口修改源 MAC 地址 → 必须先 `rte_pktmbuf_linearize()` 或拷贝。
  - 引用计数开销：`refcnt` 使用原子操作，4 个 clone 释放时 4 次 `atomic_sub`。
  - indirect mbuf 的后续处理可能更复杂（某些 PMD 不支持发送 indirect mbuf，需要 linearize）。

**`memcpy`（完整拷贝）：**
- **优点：**
  - 每个副本独立，可自由修改（换 MAC、改 VLAN 等）。
  - 代码简单直接，无引用计数问题。
  - 所有 PMD 都支持发送。
- **缺点：**
  - 拷贝开销：4 × 1500B ≈ 6KB 数据拷贝 → ~400ns + 内存带宽消耗。
  - 需要从 mempool 分配 4 个完整 mbuf（含数据区），内存消耗更大。

**clone 反而更慢的场景：**
- **小包（64B）：** clone 的 mbuf 分配 + refcnt 原子操作开销（~50ns）与 memcpy 64B（~10-20ns）相比并不占优势。
- **后续需要修改每份拷贝：** clone 后如果要 linearize 再修改，等于 clone 白做了，还多了一次间接寻址的开销。
- **PMD 不支持 indirect mbuf 发送：** 需要在 tx_burst 前隐式 linearize，增加延迟。

**Q3：128 字节的 headroom 是为了在包头前插入新的协议头。**

**128B 能覆盖的常见封装：**
- VLAN tag：4B（但通常由硬件插入）
- MPLS label：4-16B（1-4 层标签）
- GRE header：4-16B
- VXLAN：50B（outer Eth 14B + outer IP 20B + outer UDP 8B + VXLAN 8B）
- GTP-U：36B（outer IP 20B + outer UDP 8B + GTP 8B）
- IPsec ESP：~20-30B
- 上述组合：VXLAN + VLAN = ~54B

128B 足以容纳绝大多数单层或双层隧道封装头，同时又不会太大浪费内存。

**需要增大 headroom 的场景：**
- **多层隧道嵌套：** 如 GRE-over-VXLAN-over-IPsec → 可能需要 >128B。
- **自定义封装协议：** 带有大 metadata 的私有隧道头。
- **加密后扩展：** IPsec 加密后数据膨胀 + IV + trailer 可能需要更多空间。

**修改方法：** 编译 DPDK 时 `-Dc_args='-DRTE_PKTMBUF_HEADROOM=256'`，或在创建 mempool 时指定更大的 `data_room_size`。注意增大 headroom 会增大每个 mbuf 的总大小，减少 mempool 能容纳的对象数量。

---

*上一章：[Mempool](./06-mempool.md) | 下一章：[PMD](./08-pmd.md)*
