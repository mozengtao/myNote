# DPDK 深度解析 (1/9)：Hugepages — 大页内存

---

## 1. 痛点 (The "Why")

### 传统 4KB 页面的困境

Linux 默认使用 **4KB** 页面管理虚拟内存。对于高性能网络应用，需要分配大量连续的 DMA buffer（通常数百 MB 到数 GB）。这导致：

```
┌─────────────────────────────────────────────────────────┐
│              TLB Pressure with 4KB Pages                 │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Assuming DPDK needs 1GB of memory:                     │
│    4KB pages → 262,144 page table entries (PTE)         │
│    2MB pages →     512 page table entries               │
│    1GB pages →       1 page table entry                 │
│                                                         │
│  Typical CPU TLB capacity:                              │
│    L1 dTLB: 64 entries (4KB) / 32 entries (2MB)         │
│    L2 TLB:  1536 entries (4KB) / 1536 entries (2MB)     │
│                                                         │
│  4KB scheme: 262,144 >> 1,600 → frequent TLB miss       │
│  2MB scheme:    512 < 1,568  → almost no TLB miss       │
│                                                         │
│  Cost of a single TLB miss:                             │
│    4-level page table walk ≈ 10-100 ns                  │
│    If page tables not in cache → multiple memory hits   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> **中文说明：** 此图展示了 4KB 页面下的 TLB 压力分析。当 DPDK 需要 1GB 内存时，4KB 页面需要 262,144 个页表项，远超 TLB 容量（约 1,600 条目），导致频繁 TLB miss。而 2MB 大页仅需 512 个页表项，几乎消除了 TLB miss。

### 性能影响量化

| 场景 | 4KB 页面 | 2MB 大页 | 提升 |
|------|---------|---------|------|
| TLB miss rate (1GB 工作集) | ~15% | ~0.1% | **150x** |
| 单包处理延迟 | ~800ns | ~200ns | **4x** |
| 64B 小包吞吐 (单核) | ~5 Mpps | ~14 Mpps | **2.8x** |

> **核心矛盾：** 高速网卡的 DMA 引擎需要物理连续内存，而 Linux 的伙伴系统 (Buddy System) 在运行一段时间后很难分配到大块连续物理内存（内存碎片化）。Hugepages 在系统启动时就预留了物理连续的大块内存，完美解决这两个问题。

---

## 2. 核心概念与架构

### 2.1 Linux Hugepages 原理

```
         Standard 4KB pages                    2MB Hugepages
  ┌───┬───┬───┬───┬───┬───┐          ┌───────────────────────┐
  │4KB│4KB│4KB│4KB│4KB│...│ x262144  │        2MB            │ x512
  └───┴───┴───┴───┴───┴───┘          └───────────────────────┘
  Each needs its own PTE              Each needs only one PMD entry

  4-level page table walk (4KB):      3-level page table walk (2MB):
  PGD → PUD → PMD → PTE → Page       PGD → PUD → PMD → Page
  (4 memory accesses)                 (3 accesses, one level fewer!)

  For 1GB hugepages:
  PGD → PUD → Page                    (2 memory accesses, two levels fewer!)
```

> **中文说明：** 此图对比标准 4KB 页面与 2MB 大页的页表结构。4KB 页面每个需要独立的 PTE，1GB 内存需 262,144 个页表项；2MB 大页每块只需一个 PMD 条目，仅需 512 个。大页减少了页表遍历级数，从而降低 TLB miss 成本。

### 2.2 DPDK 如何使用 Hugepages

```
     At Boot Time                    During DPDK Init (rte_eal_init)
 ┌──────────────┐                  ┌────────────────────────────────┐
 │ Kernel       │                  │ 1. Scan /sys/kernel/mm/        │
 │ reserves     │                  │    hugepages/ for hugepage info│
 │ Hugepages    │                  │                                │
 │              │                  │ 2. Traverse hugetlbfs mount    │
 │ hugetlbfs    │ ──── mount ───→  │    points (/dev/hugepages)     │
 │ /dev/huge-   │                  │                                │
 │  pages       │                  │ 3. mmap() into process addr    │
 │              │                  │    space                       │
 │              │                  │                                │
 │              │                  │ 4. Build internal memseg       │
 │              │                  │    (phys addr ↔ virt addr)     │
 │              │                  │                                │
 │              │                  │ 5. Init malloc heap based on   │
 │              │                  │    memseg                      │
 └──────────────┘                  └────────────────────────────────┘
```

> **中文说明：** 此图描述 DPDK 使用 Hugepages 的完整流程。系统启动时内核预留大页并挂载 hugetlbfs；DPDK 在 rte_eal_init 时扫描大页信息、遍历挂载点、mmap 映射、构建 memseg 结构，最后初始化 malloc 堆。

### 2.3 DPDK 内存段 (Memory Segments)

```
 Physical Memory Layout (NUMA Node 0):

 ┌──────────────────────────────────────────────────────────┐
 │                  Hugepage file: rtemap_0                 │
 │  ┌─────────┬─────────┬─────────┬─────────┬─────────┐     │
 │  │ memseg  │ memseg  │ memseg  │ memseg  │ memseg  │     │
 │  │   0     │   1     │   2     │   3     │   4     │     │
 │  │ 2MB     │ 2MB     │ 2MB     │ 2MB     │ 2MB     │     │
 │  │         │         │         │         │         │     │
 │  │ virt:   │ virt:   │ virt:   │ virt:   │ virt:   │     │
 │  │ 0x7f... │ 0x7f... │ 0x7f... │ 0x7f... │ 0x7f... │     │
 │  │         │         │         │         │         │     │
 │  │ phys:   │ phys:   │ phys:   │ phys:   │ phys:   │     │
 │  │ 0x1000..│ 0x1200..│ 0x1400..│ 0x1600..│ 0x1800..│     │
 │  └─────────┴─────────┴─────────┴─────────┴─────────┘     │
 └──────────────────────────────────────────────────────────┘

 Memseg table maintained by EAL:
 ┌─────────┬──────────────┬──────────────┬──────┬──────────┐
 │  Index  │  Virt Addr   │  Phys Addr   │ Len  │ NUMA Node│
 ├─────────┼──────────────┼──────────────┼──────┼──────────┤
 │    0    │ 0x7f4000000  │ 0x100000000  │ 2MB  │    0     │
 │    1    │ 0x7f4200000  │ 0x100200000  │ 2MB  │    0     │
 │   ...   │    ...       │    ...       │ ...  │   ...    │
 └─────────┴──────────────┴──────────────┴──────┴──────────┘
```

> **中文说明：** 此图展示 DPDK 的物理内存布局。每个 hugepage 文件（如 rtemap_0）被划分为多个 2MB 的 memseg，每个 memseg 有独立的虚拟地址和物理地址。EAL 维护 memseg 表，记录索引、虚址、物址、长度和 NUMA 节点等信息。

---

## 3. 配置与使用

### 3.1 系统级配置

**方法一：启动时通过 GRUB 预留（推荐生产环境）**

```bash
# /etc/default/grub
GRUB_CMDLINE_LINUX="default_hugepagesz=1G hugepagesz=1G hugepages=4 \
                     hugepagesz=2M hugepages=1024 \
                     iommu=pt intel_iommu=on"
# 解释：
#   default_hugepagesz=1G  默认大页尺寸
#   hugepagesz=1G hugepages=4   预留 4 个 1GB 大页 (共 4GB)
#   hugepagesz=2M hugepages=1024 预留 1024 个 2MB 大页 (共 2GB)
#   iommu=pt                IOMMU passthrough (VFIO 需要)

sudo update-grub && sudo reboot
```

**方法二：运行时动态分配（仅 2MB 大页）**

```bash
# 分配 1024 个 2MB 大页 (NUMA node 0)
echo 1024 | sudo tee /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

# 分配 1024 个 2MB 大页 (NUMA node 1)
echo 1024 | sudo tee /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
```

> **注意：** 1GB 大页**只能**在启动时通过 GRUB 预留，无法运行时动态分配。

**方法三：挂载 hugetlbfs**

```bash
# 2MB 大页挂载点
sudo mkdir -p /dev/hugepages
sudo mount -t hugetlbfs nodev /dev/hugepages

# 1GB 大页挂载点 (如果使用)
sudo mkdir -p /dev/hugepages-1G
sudo mount -t hugetlbfs -o pagesize=1G nodev /dev/hugepages-1G

# 永久挂载 (/etc/fstab)
nodev /dev/hugepages hugetlbfs defaults 0 0
nodev /dev/hugepages-1G hugetlbfs pagesize=1G 0 0
```

### 3.2 验证大页状态

```bash
# 查看大页分配情况
cat /proc/meminfo | grep -i huge
# AnonHugePages:         0 kB
# ShmemHugePages:        0 kB
# HugePages_Total:    1024    ← 总数
# HugePages_Free:     1024    ← 空闲数
# HugePages_Rsvd:        0    ← 预留但未使用
# HugePages_Surp:        0    ← 超额数
# Hugepagesize:       2048 kB ← 默认大小

# 查看 NUMA 节点级别
cat /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
cat /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages

# 查看支持的大页尺寸
ls /sys/kernel/mm/hugepages/
# hugepages-1048576kB  hugepages-2048kB
```

### 3.3 DPDK 启动参数

```bash
# 指定大页目录
sudo ./dpdk-app -l 0-3 -n 4 --huge-dir /dev/hugepages

# 指定使用的大页内存总量
sudo ./dpdk-app -l 0-3 -n 4 -m 2048  # 使用 2048MB

# DPDK 20.11+ 动态内存模式 (默认)
sudo ./dpdk-app -l 0-3 -n 4 --in-memory  # 不创建 hugepage 文件

# 指定每个 NUMA node 的 socket 内存
sudo ./dpdk-app -l 0-3 -n 4 --socket-mem 1024,1024
```

---

## 4. 关键 API

### 内存分配相关

```c
/*
 * rte_malloc — 从 hugepage 上分配对齐内存
 * @type:  标识字符串 (调试用)
 * @size:  分配大小 (字节)
 * @align: 对齐要求 (0 = RTE_CACHE_LINE_SIZE)
 * 返回:   虚拟地址指针, 失败返回 NULL
 */
void *rte_malloc(const char *type, size_t size, unsigned align);

/*
 * rte_malloc_socket — 在指定 NUMA socket 上分配
 * @socket: NUMA socket ID (-1 = SOCKET_ID_ANY)
 */
void *rte_malloc_socket(const char *type, size_t size,
                        unsigned align, int socket);

/*
 * rte_free — 释放 rte_malloc 分配的内存
 */
void rte_free(void *ptr);

/*
 * rte_mem_virt2phy — 虚拟地址转物理地址
 * (DMA 编程必需, NIC 的 DMA 引擎使用物理地址)
 */
phys_addr_t rte_mem_virt2phy(const void *virtaddr);

/*
 * rte_mem_virt2iova — 虚拟地址转 IOVA
 * (VFIO 模式下使用 IOVA 而非物理地址)
 */
rte_iova_t rte_mem_virt2iova(const void *virtaddr);
```

---

## 5. 生产级代码示例

```c
#include <stdio.h>
#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_memory.h>

#define BUFFER_SIZE  (64 * 1024)  /* 64KB */

static int
hugepage_demo(void)
{
	void *buf;
	rte_iova_t iova;
	const struct rte_memseg *ms;

	/* 在 NUMA socket 0 上分配 cache-line 对齐的内存 */
	buf = rte_malloc_socket("demo_buf", BUFFER_SIZE,
	    RTE_CACHE_LINE_SIZE, 0);
	if(buf == NULL) {
		fprintf(stderr, "rte_malloc failed\n");
		return -1;
	}

	/* 获取 IOVA (用于 DMA 编程) */
	iova = rte_mem_virt2iova(buf);
	printf("virt: %p → iova: 0x%lx\n", buf, (unsigned long)iova);

	/* 遍历所有内存段，查看 hugepage 布局 */
	printf("\n--- Memory Segments ---\n");
	rte_memseg_walk(
	    (rte_memseg_walk_t)NULL,  /* 使用默认遍历 */
	    NULL);

	rte_free(buf);
	return 0;
}

int
main(int argc, char **argv)
{
	int ret;

	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("EAL init failed\n");

	hugepage_demo();

	rte_eal_cleanup();
	return 0;
}
```

---

## 6. 最佳实践与陷阱

### Cache Alignment（缓存行对齐）

```
 ❌ Wrong: Use malloc() for DPDK data structures
    → May cross cache line boundaries → false sharing
    → Not on hugepage → cannot get physical address

 ✅ Correct: Use rte_malloc() or rte_zmalloc()
    → Default 64B (RTE_CACHE_LINE_SIZE) alignment
    → Allocated on hugepage → IOVA via rte_mem_virt2iova

 ✅ Struct declaration:
    struct my_data {
        uint64_t counter;
        ...
    } __rte_cache_aligned;  /* Ensure start addr on cache line boundary */
```

> **中文说明：** 此图说明 DPDK 内存分配的缓存对齐要求。错误做法是使用 malloc()，可能造成伪共享且无法获取物理地址；正确做法是使用 rte_malloc()/rte_zmalloc() 在 hugepage 上分配并对齐，或对结构体使用 __rte_cache_aligned。

### 常见陷阱

| 陷阱 | 症状 | 解决方案 |
|------|------|---------|
| 未预留足够大页 | `rte_eal_init` 失败: "Cannot init memory" | 增加 `nr_hugepages` 或 GRUB 参数 |
| NUMA 不均衡 | 跨 socket 内存访问延迟高 | 使用 `--socket-mem` 分别指定 |
| 运行时才分配 1GB 大页 | 分配失败 (碎片化) | 必须在 GRUB 启动参数中预留 |
| 非 root 权限 | 无法 mmap hugepages | 配置 `hugetlb` group 或使用 `dpdk-hugepages.py` |
| 大页泄漏 | 重启后大页仍被占用 | 清理 `/dev/hugepages/rtemap_*` 文件 |
| 忘记 mount hugetlbfs | EAL 找不到大页 | `mount -t hugetlbfs nodev /dev/hugepages` |

### 2MB vs 1GB 选择指南

```
 ┌────────────────────────┬──────────────┬──────────────┐
 │                        │   2MB 大页    │   1GB 大页   │
 ├────────────────────────┼──────────────┼──────────────┤
 │ 灵活性                 │ 高 (可动态)   │ 低 (仅启动)  │
 │ TLB 效率               │ 好            │ 极佳         │
 │ 内存浪费               │ 较少          │ 可能较多     │
 │ 页表遍历级数           │ 3级           │ 2级          │
 │ 推荐场景               │ 通用          │ 大内存池/NFV │
 │ IOMMU 兼容性           │ 好            │ 需验证       │
 └────────────────────────┴──────────────┴──────────────┘
```

---

## 7. 知识检查 (Knowledge Check)

> **问题：** 假设你的 DPDK 应用需要在一台双 NUMA 节点服务器上处理 100Gbps 流量，每个 NUMA 节点连接一块 Mellanox ConnectX-6 网卡。你计划为每个网卡的收发队列分配 4096 个 Mbuf（每个 Mbuf 约 2KB + 元数据）。
>
> 1. 你应该选择 2MB 还是 1GB 大页？为什么？
> 2. 如果你使用 `--socket-mem 0,2048`（只在 NUMA node 1 上分配内存），但 CPU 0-3 在 NUMA node 0 上运行数据面线程去处理 NUMA node 1 上网卡的队列，会发生什么性能问题？
> 3. `rte_mem_virt2iova()` 返回的地址是给谁用的？为什么应用代码不直接用虚拟地址操作 NIC？

### 参考答案

**Q1：应该选择 1GB 大页。**

计算内存需求：每个 NUMA 节点上，4096 Mbuf × ~2.5KB ≈ 10MB（仅 Mbuf 数据区）。但实际上 mempool 管理结构、Ring 后端、描述符环、Hash 表等数据结构加在一起，典型部署很容易达到数百 MB 到 GB 级别。在 100Gbps 场景下：
- **1GB 大页**仅需 1 个页表项就能覆盖 1GB 内存 → 页表遍历只需 2 级（PGD → PUD → Page），TLB miss 几乎为零。
- **2MB 大页**覆盖同样 1GB 需要 512 个页表项，虽然也不错，但在 NIC DMA 高频读写时，1GB 大页的 TLB 优势更显著。
- 100Gbps 线速下每包只有 ~6.7ns 处理预算，任何一次 TLB miss（10-100ns 页表遍历）都会直接导致丢包。

**选型建议：** 生产环境处理 100Gbps 流量，应在 GRUB 中预留 1GB 大页。2MB 大页适合开发测试或内存需求较小的场景。

**Q2：会发生严重的跨 NUMA 内存访问性能问题。**

这里存在**双重 NUMA 不对齐**：
1. **CPU 与内存不对齐：** CPU 0-3 在 NUMA node 0 上运行，但所有内存（mempool、mbuf、Ring）都分配在 NUMA node 1 上。每次 CPU 访问 mbuf 数据都要通过 QPI/UPI 互联链路访问远端内存，延迟从 ~80ns 增加到 ~130ns（+60%），带宽减半。
2. **NIC DMA 与内存可能对齐但 CPU 不对齐：** 如果网卡在 NUMA node 1 上，NIC DMA 写入 node 1 的内存倒是本地的，但 CPU 在 node 0 读取 DMA 写入的数据仍然是远端访问。

**后果：** 每个包至少 3-5 次远端内存访问（读 Rx 描述符、读 mbuf 元数据、读包数据、写 Tx 描述符等），吞吐量可能下降 30-50%。**正确做法：** 使用 `--socket-mem 2048,2048` 两个节点都分配内存，并确保 CPU、mempool、NIC 三者在同一个 NUMA 节点上。

**Q3：`rte_mem_virt2iova()` 返回的 IOVA 地址是给 NIC 的 DMA 引擎用的。**

- **NIC 的 DMA 引擎是独立的硬件单元**，它不使用 CPU 的 MMU（虚拟地址页表），无法理解虚拟地址。DMA 引擎需要物理地址（或在 VFIO/IOMMU 模式下需要 IOVA — IO Virtual Address）来定位要读写的内存位置。
- 当 CPU 把 Rx 描述符填好交给 NIC 时，描述符中的 buffer 地址字段必须是**物理地址/IOVA**，这样 NIC 的 DMA 引擎才能把收到的包数据写到正确的 mbuf 位置。
- 如果填入虚拟地址，DMA 引擎会把数据写到错误的物理位置，轻则数据损坏，重则覆盖内核内存导致系统崩溃。
- 这就是为什么 DPDK 用 hugepages 的另一个原因：hugepage 保证物理连续，简化了虚拟地址到物理地址的映射。

---

*下一章：[EAL — 环境抽象层](./02-eal.md)*
