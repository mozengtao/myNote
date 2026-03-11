# DPDK 深度解析 (9/9)：KNI — 内核网络接口

---

## 1. 痛点 (The "Why")

### DPDK 的 "孤岛困境"

```
┌─────────────────────────────────────────────────────────┐
│     Side Effects of DPDK Bypassing Kernel Entirely      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  After DPDK takes over NIC, kernel cannot see it:       │
│                                                         │
│  ❌ Cannot use ifconfig/ip to view or configure         │
│  ❌ Kernel stack cannot handle (ARP, ICMP, DHCP, BGP)   │
│  ❌ Cannot use tcpdump to capture packets               │
│  ❌ Cannot use iptables/nftables                         │
│  ❌ Cannot be used by kernel routing table (ip route)   │
│  ❌ System tools (ethtool, netstat) unusable            │
│                                                         │
│  Real-world problems:                                   │
│  ① VPN gateways need DPDK forward + kernel IPSec        │
│  ② Routers need DPDK fast path + kernel BGP/OSPF        │
│  ③ Firewalls need DPDK data plane + kernel conntrack    │
│  ④ Ops need ping/SSH to DPDK-bound IP addresses        │
│  ⑤ Testing needs tcpdump on DPDK port traffic           │
│                                                         │
│  Solution: KNI — build a bridge between DPDK & kernel    │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> 上图概括了 DPDK 完全绕过内核的副作用：接管网卡后内核无法管理该网卡，导致大量运维与协议栈能力不可用。实际场景如 VPN、路由器、防火墙等都需要 DPDK 数据面与内核协议栈协作。KNI 即在两者间架设桥梁，使控制面/管理流量走内核。

---

## 2. 核心概念与架构

### 2.1 KNI 整体架构

```
 ┌────────────────────────────────────────────────────────────┐
 │                     User Space                             │
 │                                                            │
 │  ┌────────────────────────────────────────────────────┐    │
 │  │              DPDK Application                      │    │
 │  │                                                    │    │
 │  │   rte_eth_rx_burst()                               │    │
 │  │        │                                           │    │
 │  │        ├─→ Fast path (most pkts) → rte_eth_tx_burst│    │
 │  │        │                                           │    │
 │  │        └─→ Slow path (control plane pkts)          │    │
 │  │             │                                      │    │
 │  │             ▼                                      │    │
 │  │        rte_kni_tx_burst()  ← send to kernel        │    │
 │  │                                                    │    │
 │  │        rte_kni_rx_burst()  ← receive from kernel   │    │
 │  │             │                                      │    │
 │  │             ▼                                      │    │
 │  │        rte_eth_tx_burst()  → send kernel pkts out  │    │
 │  │                                                    │    │
 │  └──────────────────────┬─────────────────────────────┘    │
 │                         │                                  │
 │                    ┌────┴────┐                             │
 │                    │  FIFO   │ User-space FIFO (mbuf ptr)  │
 │                    │(shared memory)                        │
 │                    └────┬────┘                             │
 │                         │                                  │
 └═════════════════════════╪══════════════════════════════════┘
                           │  /dev/kni (char device ioctl)
 ┌═════════════════════════╪════════════════════════════════┐
 │                         │                                │
 │  ┌──────────────────────┴───────────────────────────┐    │
 │  │            rte_kni.ko (kernel module)            │    │
 │  │                                                  │    │
 │  │  Receive mbuf → convert to sk_buff → netif_rx()  │    │
 │  │                                                  │    │
 │  │  Create virtual interfaces: vEth0, vEth1, ...    │    │
 │  │                                                  │    │
 │  │  ┌──────────┐  ┌──────────┐                      │    │
 │  │  │ vEth0    │  │ vEth1    │                      │    │
 │  │  │ 10.0.0.1 │  │ 10.0.1.1 │                      │    │
 │  │  └────┬─────┘  └────┬─────┘                      │    │
 │  └───────┼──────────────┼───────────────────────────┘    │
 │          │              │                                │
 │          ▼              ▼                                │
 │  ┌──────────────────────────────────────┐                │
 │  │       Linux Kernel Network Stack     │                │
 │  │  ARP │ ICMP │ TCP │ UDP │ routing    │                │
 │  └──────────────────────────────────────┘                │
 │                     Kernel Space                         │
 └══════════════════════════════════════════════════════════┘
```

> 上图展示 KNI 整体架构：上层为用户态 DPDK 应用，通过 FIFO 共享内存与内核模块通信；下层为 rte_kni.ko，将 mbuf 转为 sk_buff 注入内核协议栈，并创建 vEthX 虚拟网口，使内核能正常处理 ARP、ICMP、路由等。

### 2.2 KNI 数据流

```
 ═══ Uplink (DPDK → Kernel) ═══

 1. DPDK app receives packets (rx_burst)
 2. Classify as "needs kernel" (e.g. ARP, ICMP, BGP)
 3. rte_kni_tx_burst() writes mbuf ptrs to TX FIFO
 4. rte_kni.ko reads mbuf ptrs from FIFO
 5. Copy mbuf data to sk_buff
 6. netif_rx(skb) injects into kernel stack
 7. Kernel processes (e.g. ARP reply, ICMP echo)

 ═══ Downlink (Kernel → DPDK) ═══

 1. Kernel stack generates response (e.g. ARP reply)
 2. Sent via vEthX ndo_start_xmit
 3. rte_kni.ko copies sk_buff to mbuf
 4. Writes mbuf ptr to RX FIFO
 5. DPDK app rte_kni_rx_burst() reads from FIFO
 6. Sends via rte_eth_tx_burst()

 ═══ FIFO Structure (per-interface) ═══

 ┌─────────────────────────────────────────────┐
 │            KNI FIFO (per-interface)          │
 │                                             │
 │  TX FIFO (DPDK → Kernel):                   │
 │  ┌─────┬─────┬─────┬─────┬─────┬─────┐     │
 │  │mbuf*│mbuf*│mbuf*│     │     │     │     │
 │  └─────┴─────┴─────┴─────┴─────┴─────┘     │
 │                                             │
 │  RX FIFO (Kernel → DPDK):                   │
 │  ┌─────┬─────┬─────┬─────┬─────┬─────┐     │
 │  │mbuf*│mbuf*│     │     │     │     │     │
 │  └─────┴─────┴─────┴─────┴─────┴─────┘     │
 │                                             │
 │  ALLOC FIFO (DPDK pre-allocated empty mbuf):│
 │  ┌─────┬─────┬─────┬─────┬─────┬─────┐     │
 │  │mbuf*│mbuf*│mbuf*│mbuf*│mbuf*│     │     │
 │  └─────┴─────┴─────┴─────┴─────┴─────┘     │
 │  Kernel uses these mbufs for downlink data  │
 │                                             │
 │  FREE FIFO (used mbufs returned by kernel): │
 │  ┌─────┬─────┬─────┬─────┬─────┬─────┐     │
 │  │mbuf*│mbuf*│     │     │     │     │     │
 │  └─────┴─────┴─────┴─────┴─────┴─────┘     │
 │  DPDK retrieves and frees to mempool        │
 └─────────────────────────────────────────────┘
```

> 上图说明 KNI 上行与下行数据流：上行时 DPDK 将 mbuf 指针放入 TX FIFO，内核模块拷贝到 sk_buff 后注入协议栈；下行时内核通过 vEthX 发送，rte_kni.ko 拷贝到 mbuf 放入 RX FIFO。四类 FIFO（TX、RX、ALLOC、FREE）分别用于控制面传递和 mbuf 生命周期管理。

### 2.3 KNI vs 其他替代方案

```
 ┌────────────────┬──────────┬──────────┬──────────┬───────────┐
 │                │   KNI    │  TAP PMD │ virtio-  │ af_xdp    │
 │                │          │          │  user    │           │
 ├────────────────┼──────────┼──────────┼──────────┼───────────┤
 │ Kernel module  │ ✅ Yes   │ ✗ No     │ ✗ No    │ ✗ No      │
 │ Data copy      │ 1x       │ 1x       │ 0x      │ 0x        │
 │ Performance    │ Medium   │ Med-Low  │ High    │ High       │
 │ Deploy complexity│ High   │ Low      │ Medium  │ Medium     │
 │ Kernel stack   │ Full     │ Full     │ Limited │ Full       │
 │ DPDK version   │ Legacy   │ 17.02+   │ 17.11+  │ 19.05+     │
 │ Status         │ Maintained│ Active  │ Active  │ Preferred✅│
 └────────────────┴──────────┴──────────┴──────────┴───────────┘

 Note: KNI deprecated in DPDK 22.11+
       Prefer TAP PMD or af_xdp
```

> 上表对比 KNI 与 TAP PMD、virtio-user、af_xdp：KNI 需内核模块且有一次拷贝，部署复杂；af_xdp 性能最好且与内核栈交互完整，为当前推荐方案；DPDK 22.11 起 KNI 已被标记为 deprecated。

---

## 3. 使用与配置

### 3.1 编译与加载 KNI 内核模块

```bash
# 编译 DPDK 时启用 KNI (meson)
meson setup build -Denable_kmods=true
cd build && ninja

# 加载模块
sudo insmod ./kernel/linux/kni/rte_kni.ko

# 带参数加载 (指定内核线程模式)
sudo insmod rte_kni.ko kthread_mode=multiple
# kthread_mode=single   : 所有 KNI 接口共享一个内核线程
# kthread_mode=multiple : 每个 KNI 接口一个内核线程

# 验证加载
lsmod | grep kni
# rte_kni    32768  0

ls /dev/kni
# /dev/kni
```

### 3.2 KNI 接口配置

```bash
# DPDK 应用创建 KNI 接口后, 在另一个终端配置:

# 查看 KNI 虚拟接口
ip link show vEth0

# 配置 IP
sudo ip addr add 10.0.0.1/24 dev vEth0
sudo ip link set vEth0 up

# 配置路由
sudo ip route add 10.0.1.0/24 via 10.0.0.254 dev vEth0

# 现在可以:
ping 10.0.0.1                  # ✅ 内核回复 ICMP
tcpdump -i vEth0               # ✅ 抓 DPDK 转发来的包
ssh user@10.0.0.1              # ✅ 通过 KNI 访问系统
```

---

## 4. 关键 API

```c
/*
 * rte_kni_init — 初始化 KNI 子系统
 * @max_kni_ifaces: 最大 KNI 接口数
 * 必须在 rte_eal_init() 之后调用
 */
int rte_kni_init(unsigned int max_kni_ifaces);

/*
 * rte_kni_alloc — 创建一个 KNI 接口
 * @conf:     配置 (名称, mbuf pool, port_id 等)
 * @ops:      回调函数 (link up/down, MTU 变更等)
 * 成功后内核中出现对应的虚拟网口
 */
struct rte_kni *
rte_kni_alloc(struct rte_mempool *pktmbuf_pool,
    const struct rte_kni_conf *conf,
    struct rte_kni_ops *ops);

/*
 * rte_kni_tx_burst — 从 DPDK 发包到内核
 * (将 mbuf 指针写入 TX FIFO)
 */
unsigned rte_kni_tx_burst(struct rte_kni *kni,
    struct rte_mbuf **mbufs, unsigned num);

/*
 * rte_kni_rx_burst — 从内核收包到 DPDK
 * (从 RX FIFO 读取 mbuf 指针)
 */
unsigned rte_kni_rx_burst(struct rte_kni *kni,
    struct rte_mbuf **mbufs, unsigned num);

/*
 * rte_kni_handle_request — 处理内核的配置请求
 * 必须定期调用! (link change, MTU change 等)
 * 如果不调用, ip link set up 等命令会阻塞
 */
int rte_kni_handle_request(struct rte_kni *kni);

/*
 * rte_kni_release — 释放 KNI 接口
 */
int rte_kni_release(struct rte_kni *kni);
```

---

## 5. 生产级代码示例

```c
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_kni.h>
#include <rte_mbuf.h>

static struct rte_kni *kni_port;

/*
 * KNI 回调: 内核请求 link up
 */
static int
kni_change_mtu(uint16_t port_id, unsigned new_mtu)
{
	int ret;

	if(new_mtu > 9000 || new_mtu < 64)
		return -EINVAL;

	ret = rte_eth_dev_set_mtu(port_id, new_mtu);
	return ret;
}

static int
kni_config_network_if(uint16_t port_id, uint8_t if_up)
{
	if(if_up)
		rte_eth_dev_start(port_id);
	else
		rte_eth_dev_stop(port_id);
	return 0;
}

/*
 * 初始化 KNI 接口
 */
static struct rte_kni *
kni_init(uint16_t port_id, struct rte_mempool *pool)
{
	struct rte_kni_conf conf;
	struct rte_kni_ops ops;
	struct rte_kni *kni;

	memset(&conf, 0, sizeof(conf));
	snprintf(conf.name, RTE_KNI_NAMESIZE, "vEth%u", port_id);
	conf.group_id = port_id;
	conf.mbuf_size = RTE_MBUF_DEFAULT_BUF_SIZE;

	/* 获取端口信息用于内核侧配置 */
	rte_eth_macaddr_get(port_id,
	    (struct rte_ether_addr *)&conf.mac_addr);
	rte_eth_dev_get_mtu(port_id, &conf.mtu);

	memset(&ops, 0, sizeof(ops));
	ops.port_id = port_id;
	ops.change_mtu = kni_change_mtu;
	ops.config_network_if = kni_config_network_if;

	kni = rte_kni_alloc(pool, &conf, &ops);
	if(kni == NULL)
		rte_panic("Cannot create KNI for port %u\n",
		    port_id);

	return kni;
}

/*
 * 数据面: 快速路径 + 慢速路径分流
 */
static int
is_control_packet(struct rte_mbuf *m)
{
	struct rte_ether_hdr *eth;

	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/* ARP */
	if(eth->ether_type ==
	    rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
		return 1;

	/* ICMP (简化判断) */
	if(eth->ether_type ==
	    rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
		struct rte_ipv4_hdr *ip;

		ip = (struct rte_ipv4_hdr *)(eth + 1);
		if(ip->next_proto_id == IPPROTO_ICMP)
			return 1;
	}
	return 0;
}

static void
main_loop(uint16_t port_id, struct rte_kni *kni)
{
	struct rte_mbuf *rx_bufs[32];
	struct rte_mbuf *kni_bufs[32];
	struct rte_mbuf *fwd_bufs[32];
	uint16_t nb_rx, i;
	unsigned nb_kni, nb_fwd;

	for(;;) {
		/* 处理内核请求 (link up/down, MTU) */
		rte_kni_handle_request(kni);

		/* 收包 */
		nb_rx = rte_eth_rx_burst(port_id, 0,
		    rx_bufs, 32);
		if(nb_rx == 0)
			continue;

		/* 分流: 控制面 → KNI, 数据面 → 快速路径 */
		nb_kni = 0;
		nb_fwd = 0;
		for(i = 0; i < nb_rx; i++) {
			if(is_control_packet(rx_bufs[i]))
				kni_bufs[nb_kni++] = rx_bufs[i];
			else
				fwd_bufs[nb_fwd++] = rx_bufs[i];
		}

		/* 控制面包送到内核 */
		if(nb_kni > 0) {
			unsigned sent;

			sent = rte_kni_tx_burst(kni,
			    kni_bufs, nb_kni);
			for(i = sent; i < nb_kni; i++)
				rte_pktmbuf_free(kni_bufs[i]);
		}

		/* 数据面包走快速路径 (这里简化为直接转发) */
		if(nb_fwd > 0) {
			uint16_t sent;

			sent = rte_eth_tx_burst(port_id ^ 1, 0,
			    fwd_bufs, nb_fwd);
			for(i = sent; i < nb_fwd; i++)
				rte_pktmbuf_free(fwd_bufs[i]);
		}

		/* 从内核接收响应包, 发到网线上 */
		nb_rx = rte_kni_rx_burst(kni, rx_bufs, 32);
		if(nb_rx > 0) {
			uint16_t sent;

			sent = rte_eth_tx_burst(port_id, 0,
			    rx_bufs, nb_rx);
			for(i = sent; i < nb_rx; i++)
				rte_pktmbuf_free(rx_bufs[i]);
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

	rte_kni_init(1);

	pool = rte_pktmbuf_pool_create("KNI_POOL",
	    8191, 250, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
	    rte_socket_id());

	/* 端口初始化 (省略, 参考 PMD 章节) */

	kni_port = kni_init(0, pool);

	main_loop(0, kni_port);

	rte_kni_release(kni_port);
	rte_eal_cleanup();
	return 0;
}
```

---

## 6. 最佳实践与陷阱

### 性能考量

```
 KNI data path has one memory copy (mbuf ↔ sk_buff):
 → not suitable for high throughput!

 ┌──────────────────────────────────────────────────────┐
 │  KNI typical throughput: ~3-5 Mpps (vs PMD 14 Mpps)   │
 │                                                      │
 │  Suitable for:                                       │
 │    ✅ Control plane (ARP, ICMP, BGP, OSPF)           │
 │    ✅ Management traffic (SSH, SNMP, syslog)         │
 │    ✅ Debug capture (tcpdump)                        │
 │                                                      │
 │  Not suitable for:                                   │
 │    ❌ Data plane forwarding (use PMD + user stack)  │
 │    ❌ High bandwidth (use af_xdp or virtio-user)     │
 └──────────────────────────────────────────────────────┘
```

> 上框说明 KNI 性能限制：因 mbuf 与 sk_buff 之间有一次拷贝，吞吐仅约 3–5 Mpps，远低于 PMD。适合控制面、管理流量和 tcpdump，不适合数据面转发或大带宽场景。

### rte_kni_handle_request 必须调用

```
 ⚠ 关键陷阱:

 如果不定期调用 rte_kni_handle_request():
 → 内核侧 ip link set vEth0 up 会永久阻塞!
 → MTU 变更请求无法完成
 → 用户以为系统卡死

 解决:
 ① 在数据面循环中每次迭代调用 (推荐)
 ② 或在 service core 上定期调用
```

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| 未加载 rte_kni.ko | `rte_kni_alloc` 失败 | `insmod rte_kni.ko` |
| 未调用 `rte_kni_handle_request` | `ip link set up` 阻塞 | 数据面循环中调用 |
| KNI 接口未配置 IP | 内核不处理包 | `ip addr add ... dev vEthX` |
| ALLOC FIFO 空了 | 内核无法发包到 DPDK | 确保 DPDK 侧定期补充空 mbuf |
| KNI 处理大流量 | CPU 100% 但吞吐低 | 只发控制面包到 KNI |
| 内核模块版本不匹配 | insmod 失败 | 用匹配的内核头文件重新编译 |

### KNI 的未来

```
 ┌──────────────────────────────────────────────────────┐
 │  KNI 在 DPDK 22.11 被标记为 DEPRECATED              │
 │                                                      │
 │  推荐替代方案:                                        │
 │                                                      │
 │  1. TAP PMD (最简单):                                 │
 │     → 创建 Linux tap 设备                             │
 │     → 纯用户态, 无需内核模块                           │
 │     → rte_eth_dev_configure() 即可使用                │
 │                                                      │
 │  2. af_xdp PMD (最佳性能):                            │
 │     → 使用 Linux AF_XDP socket                        │
 │     → 零拷贝模式 (需要内核 5.4+)                      │
 │     → 兼顾内核栈和 DPDK 性能                          │
 │                                                      │
 │  3. virtio-user (虚拟化场景):                          │
 │     → DPDK ↔ 内核 vhost 通信                          │
 │     → 零拷贝                                          │
 │     → 适合容器和虚拟机                                │
 │                                                      │
 └──────────────────────────────────────────────────────┘
```

---

## 7. 知识检查 (Knowledge Check)

> **问题：**
>
> 1. KNI 中有 4 种 FIFO (TX, RX, ALLOC, FREE)。为什么需要 ALLOC 和 FREE FIFO？如果没有它们，内核模块如何获取 mbuf 来承载下行数据？
> 2. 你的 DPDK 路由器需要运行 BGP 守护进程 (FRRouting)。BGP 使用 TCP 连接。描述一个 BGP OPEN 消息从远端路由器到达本机 FRRouting 进程的完整数据路径（经过哪些组件，哪些地方发生拷贝）。
> 3. 如果你在 DPDK 24.x 上开始一个新项目，你会选择 KNI 还是其他方案（TAP PMD / af_xdp / virtio-user）？为什么？各自在什么场景下最适用？

### 参考答案

**Q1：ALLOC 和 FREE FIFO 解决了"内核模块无法调用 DPDK mempool API"的问题。**

**核心难题：** mbuf 是从 DPDK 用户态的 mempool 分配的，而 `rte_kni.ko` 运行在内核态。内核模块不能直接调用用户态的 `rte_pktmbuf_alloc()` —— 地址空间不同，mempool API 在用户态库中，内核中不存在。

**ALLOC FIFO 的作用（DPDK → 内核，传递空 mbuf）：**
- DPDK 用户态进程**预先分配一批空 mbuf**，将它们的指针放入 ALLOC FIFO。
- 当内核协议栈要通过 KNI 虚拟网口发包时（如 ARP reply），`rte_kni.ko` 从 ALLOC FIFO 中取出一个空 mbuf。
- 将 sk_buff 的数据拷贝到这个 mbuf 中。
- 将填好数据的 mbuf 指针放入 RX FIFO（内核 → DPDK 方向）。
- **如果没有 ALLOC FIFO：** 内核模块无法获得 mbuf，就无法将 sk_buff 数据传递给 DPDK 应用，下行路径完全不通。

**FREE FIFO 的作用（内核 → DPDK，归还用完的 mbuf）：**
- 上行路径中（DPDK → 内核），DPDK 将 mbuf 指针放入 TX FIFO。
- `rte_kni.ko` 取出 mbuf，将数据拷贝到新分配的 sk_buff 后，这个 mbuf 就用完了。
- 但内核模块不能调用 `rte_pktmbuf_free()`（用户态 API），所以将用完的 mbuf 指针放入 FREE FIFO。
- DPDK 用户态进程定期从 FREE FIFO 取回这些 mbuf 指针，调用 `rte_pktmbuf_free()` 归还到 mempool。
- **如果没有 FREE FIFO：** 每次上行传递的 mbuf 都会泄漏，mempool 很快耗尽。

**总结：** ALLOC/FREE FIFO 是一种"代理分配/释放"机制 —— 用户态负责 mbuf 生命周期管理，内核态通过 FIFO "借用" mbuf。

**Q2：BGP OPEN 消息的完整数据路径：**

```
远端路由器 → 网线 → 本机 NIC → DPDK → KNI → 内核 → FRRouting

详细步骤：

1. 远端路由器发出 BGP OPEN（TCP 包）
     ↓
2. NIC 收到帧，DMA 写入 Rx 描述符指定的 mbuf 数据区
   [零拷贝: NIC DMA → hugepage mbuf]
     ↓
3. DPDK worker lcore 的 rte_eth_rx_burst() 收到 mbuf
     ↓
4. 应用检查: dst_port == 179 (BGP) → 控制面包 → 走 KNI 慢速路径
     ↓
5. rte_kni_tx_burst() 将 mbuf 指针写入 TX FIFO
   [零拷贝: 只传递指针]
     ↓
6. rte_kni.ko 从 TX FIFO 取出 mbuf 指针
     ↓
7. rte_kni.ko 分配 sk_buff，将 mbuf 数据拷贝到 sk_buff
   [第 1 次拷贝: mbuf → sk_buff, ~1500B]
     ↓
8. rte_kni.ko 将 mbuf 指针放入 FREE FIFO（归还给用户态）
     ↓
9. netif_rx(skb) 将 sk_buff 注入内核协议栈
     ↓
10. 内核协议栈处理:
    L2 (Ethernet) → L3 (IP 路由查找, 确认目的地是本机)
    → L4 (TCP: 序号校验, ACK, 重组)
    → socket buffer (sock_queue_rcv_skb)
    [零拷贝: 只在 socket buffer 中排队]
     ↓
11. FRRouting 进程的 read()/recv() 系统调用
    内核将 socket buffer 中的数据拷贝到用户态
    [第 2 次拷贝: sk_buff → 用户态 buffer]
     ↓
12. FRRouting 解析 BGP OPEN 消息，进入 FSM 状态机处理
```

**总计 2 次数据拷贝：** mbuf→sk_buff（KNI 内部）+ sk_buff→用户态（read 系统调用）。这就是 KNI 不适合高带宽数据面的原因 —— 每个包都有 2 次拷贝。但对于 BGP 这种低频控制面协议（每秒几十到几百个包），完全可以接受。

**Q3：新项目应避免 KNI，推荐 TAP PMD 或 af_xdp，具体取决于场景。**

**不选 KNI 的原因：**
- DPDK 22.11 已标记 KNI 为 deprecated，24.x 中可能随时移除。
- 需要编译和维护内核模块（`rte_kni.ko`），内核版本升级时容易不兼容。
- 架构较重（4 个 FIFO + 内核线程 + 数据拷贝）。

**各方案最佳场景：**

| 方案 | 最适合场景 | 理由 |
|------|-----------|------|
| **TAP PMD** | 控制面交互（ARP/ICMP/BGP/OSPF）、开发测试 | 最简单，纯用户态，无需内核模块。创建 Linux TAP 设备后直接当作 DPDK ethdev 使用。`rte_eth_dev_configure()` 统一接口，代码侵入最小。性能：~1-3Mpps，足够控制面使用。 |
| **af_xdp PMD** | 需要高性能内核栈交互、不想完全绕过内核 | 基于 Linux AF_XDP socket，内核 5.4+ 支持零拷贝模式。性能可达 ~10Mpps+，接近纯 PMD。可以让内核栈和 DPDK 共存 —— 部分流量走内核栈（如管理流量），部分走 DPDK 快速路径。无需解绑内核驱动，运维更友好。 |
| **virtio-user** | 容器/虚拟机、与 OvS-DPDK/vhost 对接 | 通过 vhost 协议与内核或 hypervisor 通信，零拷贝。K8s Pod 中使用 DPDK 时，virtio-user + vhost-net 是最自然的选择。也可用于 DPDK 进程间通信。 |

**我的推荐（2024+ 新项目）：**
- **首选 TAP PMD** —— 如果只需控制面交互，它最简单、零依赖、代码改动最小。
- **需要高性能 → af_xdp** —— 如果需要与内核栈高速交换数据（如混合部署），af_xdp 是性能与兼容性的最佳平衡。
- **容器/虚拟化 → virtio-user** —— 如果运行在 K8s/VM 中，与 vSwitch 对接。

---

*上一章：[PMD](./08-pmd.md) | 回到 [总览](./00-overview.md)*
