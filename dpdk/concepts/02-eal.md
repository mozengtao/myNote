# DPDK 深度解析 (2/9)：EAL — 环境抽象层

---

## 1. 痛点 (The "Why")

### 高性能网络应用的环境初始化困境

要绕过内核直接操作硬件，应用需要自行处理大量底层细节：

```
┌─────────────────────────────────────────────────────────┐
│           Without EAL: What You'd Need to Do             │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  1. Parse /proc/cpuinfo to determine CPU topology        │
│  2. Bind threads to specific cores via sched_setaffinity │
│  3. Scan PCI devices via sysfs, unbind kernel drivers    │
│  4. mmap() hugepages, manage virt/physical addr mapping  │
│  5. Detect NUMA topology, ensure memory locality         │
│  6. Init log, timer, interrupt subsystems                │
│  7. Handle multi-process shared memory (primary/secondary)│
│  8. Port to different OS (Linux/FreeBSD/Windows)         │
│                                                         │
│  Each step has many edge cases and platform differences  │
│  → Reinventing the wheel, and error-prone                │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> 若没有 EAL，应用需自行完成 CPU 拓扑解析、线程绑定、PCI 扫描解绑、大页映射、NUMA 检测、日志/定时器/中断初始化、多进程共享内存与跨平台适配。每一步都有大量边界情况和平台差异，重复造轮子且易出错。

**EAL 的价值：** 将所有这些底层环境初始化封装为一次 `rte_eal_init()` 调用，对上层提供统一抽象。

---

## 2. 核心概念与架构

### 2.1 EAL 在 DPDK 中的位置

```
 ┌──────────────────────────────────────────────────────┐
 │                  User Application                    │
 │                                                      │
 │  main() {                                            │
 │      rte_eal_init(argc, argv);  ← Everything starts  │
 │      ...                                             │
 │  }                                                   │
 └───────────────────────┬──────────────────────────────┘
                         │
 ┌───────────────────────┴──────────────────────────────┐
 │                     EAL Layer                       │
 │                                                      │
 │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐  │
 │  │ CPU/Thread│ │ Memory   │ │ PCI/Bus  │ │ Log    │  │
 │  │ Mgmt     │ │ (hugepage)│ │ Scan/Bind│ │ System │  │
 │  └──────────┘ └──────────┘ └──────────┘ └────────┘  │
 │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐  │
 │  │ Timer    │ │ Interrupt │ │ Multi-   │ │ Device │  │
 │  │          │ │ (UIO/VFIO)│ │ Process  │ │ Probe  │  │
 │  └──────────┘ └──────────┘ └──────────┘ └────────┘  │
 │                                                      │
 └──────────────────────────────────────────────────────┘
```

> EAL 位于用户应用之下，将 CPU/线程管理、大页内存、PCI 扫描绑定、日志、定时器、中断、多进程和设备探测等子系统封装为统一抽象，应用只需调用 rte_eal_init() 即可完成所有环境初始化。

### 2.2 `rte_eal_init()` 内部流程

```
 rte_eal_init(argc, argv)
 │
 ├─ 1. Arg parsing (eal_parse_args)
 │     -l 0-3          → logical cores to use
 │     -n 4            → memory channels
 │     --huge-dir      → hugepage mount dir
 │     --socket-mem    → memory per NUMA node
 │     -a / --allow    → whitelist PCI device
 │     --file-prefix   → prefix for multi-instance
 │     --proc-type     → primary / secondary / auto
 │
 ├─ 2. Log init (rte_eal_log_init)
 │     → set log level & output target
 │
 ├─ 3. CPU detection (rte_cpu_get_features)
 │     → detect SSE4.2, AVX2, AVX-512 etc.
 │     → used to pick optimal data path impl
 │
 ├─ 4. Hugepage memory init (rte_eal_memory_init)
 │     ├─ scan sysfs for hugepage info
 │     ├─ mmap hugepages into process addr space
 │     ├─ build memseg (virt ↔ phys mapping)
 │     └─ init malloc heap (per-NUMA)
 │
 ├─ 5. NUMA topology detection
 │     → /sys/devices/system/node/
 │     → build lcore ↔ NUMA socket mapping
 │
 ├─ 6. Lcore init (eal_thread_init_master + workers)
 │     ├─ main thread registered as master lcore
 │     ├─ create pthread for each worker lcore
 │     ├─ bind each pthread to CPU core
 │     └─ workers enter WAIT state
 │
 ├─ 7. PCI/bus scan (rte_bus_scan)
 │     ├─ traverse /sys/bus/pci/devices/
 │     ├─ read vendor/device ID
 │     └─ build device list
 │
 ├─ 8. Device probe (rte_bus_probe)
 │     ├─ match registered PMD drivers
 │     ├─ call PMD .probe() callback
 │     └─ init ethdev ports
 │
 └─ 9. Service core init (rte_service_init) [optional]
       → register background services (timer, intr etc.)
```

> rte_eal_init() 依次执行：参数解析、日志初始化、CPU 特性检测、大页内存初始化、NUMA 拓扑检测、Lcore 初始化、PCI 总线扫描、设备探测，以及可选的服务核心初始化。每一步都依赖前序步骤的结果。

### 2.3 多进程模型

```
 ┌─────────────────────────────────────────────────────────┐
 │              DPDK Multi-Process Architecture            │
 │                                                         │
 │  ┌──────────────────┐    ┌──────────────────┐           │
 │  │ Primary Process  │    │ Secondary Process │           │
 │  │                  │    │                   │           │
 │  │ ★ Init all res   │    │ ★ Attach to exist │           │
 │  │ ★ Create memzone │    │ ★ Lookup memzone  │           │
 │  │ ★ Alloc ring/pool│    │ ★ Lookup ring/pool│           │
 │  │ ★ Config ethdev  │    │ ★ Use ethdev      │           │
 │  │                  │    │                   │           │
 │  └────────┬─────────┘    └────────┬──────────┘           │
 │           │                       │                      │
 │           │  shared hugepage mem  │                      │
 │           │  ┌───────────────────┐ │                      │
 │           └──┤   rte_config      ├─┘                      │
 │              │   (shared config)   │                        │
 │              ├───────────────────┤                        │
 │              │   memzones[]      │  All procs map to same │
 │              │   memsegs[]       │  virt addr → ptr share │
 │              │   malloc heaps[]  │                        │
 │              └───────────────────┘                        │
 │                                                         │
 └─────────────────────────────────────────────────────────┘

 Launch:
   Primary:   ./app --proc-type=primary
   Secondary: ./app --proc-type=secondary
```

> DPDK 多进程模型中，Primary 负责初始化资源、创建 memzone、分配 ring/pool、配置网口；Secondary 负责连接已有资源、查找 memzone/ring/pool 并使用网口。二者通过共享大页内存中的 rte_config 通信，所有进程映射到相同虚拟地址，指针可直接共享。

---

## 3. 使用与配置

### 3.1 常用 EAL 命令行参数

```bash
# 基本启动
sudo ./app -l 0-3 -n 4

# 完整参数示例
sudo ./app \
  -l 0,2,4,6            \  # 使用 CPU 0,2,4,6
  -n 4                   \  # 4个内存通道
  --socket-mem 1024,1024 \  # NUMA0: 1G, NUMA1: 1G
  --huge-dir /dev/hugepages \
  -a 0000:03:00.0        \  # 只绑定这个 PCI 设备
  --file-prefix dpdk1    \  # 多实例时避免冲突
  --log-level 7          \  # DEBUG 级别日志
  --                     \  # EAL 参数与应用参数分隔符
  -p 0x3 -q 2               # 应用自身参数
```

### 3.2 EAL 参数速查

| 参数 | 含义 | 示例 |
|------|------|------|
| `-l <cores>` | 使用的逻辑核心列表 | `-l 0-3` 或 `-l 0,2,4` |
| `-n <channels>` | 内存通道数 | `-n 4` |
| `-a <PCI>` | 白名单 PCI 设备 | `-a 0000:03:00.0` |
| `-b <PCI>` | 黑名单 PCI 设备 | `-b 0000:07:00.0` |
| `--socket-mem` | 每 NUMA 节点内存(MB) | `--socket-mem 1024,1024` |
| `--huge-dir` | hugepage 目录 | `--huge-dir /dev/hugepages` |
| `--file-prefix` | 多实例前缀 | `--file-prefix app1` |
| `--proc-type` | 进程类型 | `--proc-type secondary` |
| `--in-memory` | 不创建 hugepage 文件 | `--in-memory` |
| `--log-level` | 日志级别 (1-8) | `--log-level 7` |
| `--main-lcore` | 指定 main lcore ID | `--main-lcore 0` |
| `--lcores` | 高级核心映射 | `--lcores '(0-1)@0,(2-3)@1'` |

---

## 4. 关键 API

```c
/*
 * rte_eal_init — DPDK 入口，初始化所有子系统
 * 返回: 消耗的 argc 数量 (>0 成功), -1 失败
 * 注意: 只调用一次, 且必须在其他 rte_* 之前调用
 */
int rte_eal_init(int argc, char **argv);

/*
 * rte_eal_cleanup — 释放 EAL 资源
 * 注意: 与 rte_eal_init 配对使用
 */
int rte_eal_cleanup(void);

/*
 * rte_eal_remote_launch — 在指定 worker lcore 上启动函数
 * @f:        要执行的函数
 * @arg:      传递给函数的参数
 * @worker_id: 目标 worker lcore ID
 *
 * worker 线程从 WAIT 状态被唤醒，执行 f(arg)
 */
int rte_eal_remote_launch(lcore_function_t *f, void *arg,
                          unsigned worker_id);

/*
 * rte_eal_mp_wait_lcore — 等待所有 worker 完成
 * 阻塞直到所有 remote_launch 的函数返回
 */
void rte_eal_mp_wait_lcore(void);

/*
 * rte_eal_process_type — 查询当前进程类型
 * 返回: RTE_PROC_PRIMARY 或 RTE_PROC_SECONDARY
 */
enum rte_proc_type_t rte_eal_process_type(void);
```

---

## 5. 生产级代码示例

```c
#include <stdio.h>
#include <signal.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>

static volatile int force_quit = 0;

static void
signal_handler(int signum)
{
	if(signum == SIGINT || signum == SIGTERM)
		force_quit = 1;
}

/*
 * worker lcore 主循环
 */
static int
worker_main(__rte_unused void *arg)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned socket_id = rte_socket_id();

	printf("lcore %u on NUMA socket %u: running\n",
	    lcore_id, socket_id);

	while(!force_quit) {
		/* 数据面处理逻辑 */
	}

	printf("lcore %u: exiting\n", lcore_id);
	return 0;
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;
	uint16_t nb_ports;

	/* 1. EAL 初始化 */
	ret = rte_eal_init(argc, argv);
	if(ret < 0)
		rte_panic("EAL init failed\n");

	/* 调整 argc/argv 跳过 EAL 参数 */
	argc -= ret;
	argv += ret;

	/* 2. 信号处理 */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* 3. 检查可用端口 */
	nb_ports = rte_eth_dev_count_avail();
	if(nb_ports == 0)
		rte_panic("No Ethernet ports found\n");

	printf("Process type: %s\n",
	    rte_eal_process_type() == RTE_PROC_PRIMARY ?
	    "PRIMARY" : "SECONDARY");
	printf("Available ports: %u\n", nb_ports);
	printf("Available lcores: %u\n", rte_lcore_count());

	/* 4. 在每个 worker lcore 上启动工作函数 */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(worker_main, NULL, lcore_id);
	}

	/* 5. master lcore 也参与工作 */
	worker_main(NULL);

	/* 6. 等待所有 worker 退出 */
	rte_eal_mp_wait_lcore();

	/* 7. 清理 */
	rte_eal_cleanup();
	return 0;
}
```

---

## 6. 最佳实践与陷阱

### 初始化顺序

```
 ✅ Correct initialization order:
    rte_eal_init()
    → rte_pktmbuf_pool_create()   (create mempool)
    → rte_eth_dev_configure()     (configure port)
    → rte_eth_rx_queue_setup()    (setup RX queue)
    → rte_eth_tx_queue_setup()    (setup TX queue)
    → rte_eth_dev_start()         (start port)
    → rte_eal_remote_launch()     (launch workers)

 ❌ Common mistakes:
    - Call other rte_* before rte_eal_init()
    - Forget argc -= ret; argv += ret; to skip EAL args
    - Try to create existing resources in secondary process
```

> 必须先调用 rte_eal_init()，再按顺序创建 mempool、配置网口、设置队列、启动端口、启动 worker。不要在 EAL 初始化前调用任何 rte_*，记得调整 argc/argv，且在 secondary 进程中只能查找已有资源，不能重复创建。

### 多实例运行

```bash
# 实例 1: 使用 CPU 0-3, 端口 0
sudo ./app --file-prefix app1 -l 0-3 -a 0000:03:00.0 -- -p 0x1

# 实例 2: 使用 CPU 4-7, 端口 1
sudo ./app --file-prefix app2 -l 4-7 -a 0000:05:00.0 -- -p 0x1

# 注意: --file-prefix 确保 hugepage 文件不冲突
```

### 常见陷阱

| 陷阱 | 症状 | 解决 |
|------|------|------|
| argc/argv 未调整 | 应用参数解析错误 | `argc -= ret; argv += ret;` |
| 多实例无 file-prefix | "Cannot create lock" 错误 | 每个实例不同的 `--file-prefix` |
| Secondary 在 Primary 之前启动 | 找不到共享内存 | 确保 Primary 先启动 |
| EAL 参数与应用参数混淆 | 初始化失败 | 用 `--` 分隔 |
| 忘记 `rte_eal_cleanup()` | hugepage 泄漏 | 始终在退出前调用 |

---

## 7. 知识检查 (Knowledge Check)

> **问题：** 你有一个 DPDK 应用需要运行两个实例来处理不同的网卡端口。
>
> 1. 如果不使用 `--file-prefix`，两个实例能否同时运行？为什么？
> 2. `rte_eal_init()` 的返回值为什么是"消耗的 argc 数量"而不是简单的 0/-1？这个设计有什么好处？
> 3. 在 secondary 进程中调用 `rte_pktmbuf_pool_create()` 创建一个与 primary 中同名的 mempool 会发生什么？应该用什么 API 替代？

### 参考答案

**Q1：不能同时运行。**

DPDK EAL 初始化时会在 `/var/run/dpdk/` 目录下创建运行时文件（配置文件、hugepage 映射文件、锁文件等）。默认的 `file-prefix` 是 `rte`，两个实例会尝试创建/锁定相同的文件：
- `/var/run/dpdk/rte/config` — 共享配置文件
- `/var/run/dpdk/rte/.rte_config` — 文件锁
- `/dev/hugepages/rtemap_*` — hugepage 映射文件

第二个实例启动时尝试获取文件锁会失败，报错 `"Cannot create lock on '/var/run/dpdk/rte/.rte_config'"`。使用不同的 `--file-prefix`（如 `--file-prefix app1` 和 `--file-prefix app2`）后，各自使用独立的运行时文件，互不冲突。

**Q2：返回"消耗的 argc 数量"是为了让应用能正确解析自己的命令行参数。**

DPDK 的 EAL 参数（如 `-l 0-3 -n 4 --huge-dir /dev/hugepages`）和应用自身参数（如 `-p 0x3 -q 2`）混在同一个 `argc/argv` 中，通过 `--` 分隔：

```bash
./app -l 0-3 -n 4 -- -p 0x3 -q 2
```

`rte_eal_init()` 返回它消耗的参数数量（比如 5），应用代码做 `argc -= ret; argv += ret;` 后，`argv` 就跳过了 EAL 参数，指向 `-- -p 0x3 -q 2`。这样应用可以用标准的 `getopt()` 解析自己的参数，不会被 EAL 的 `-l`、`-n` 等参数干扰。如果只返回 0/-1，应用就无法知道应该跳过几个参数，要么自行解析 EAL 参数（重复工作），要么完全放弃命令行参数（不灵活）。

**Q3：创建会失败，返回 NULL，`rte_errno` 设为 `EEXIST`。**

在 DPDK 多进程模型中，Primary 进程负责创建所有共享资源（mempool、ring、memzone 等），Secondary 进程只能**查找**已有资源。原因：
- 所有命名资源都注册在共享的 `memzone` 表中，同名资源不允许重复创建。
- `rte_pktmbuf_pool_create()` 内部会调用 `rte_memzone_reserve()`，如果名称已存在，就会失败。

**正确做法：** Secondary 进程应使用 `rte_mempool_lookup("pool_name")` 查找已有的 mempool。这个函数在共享内存中查找 Primary 创建的 mempool 结构并返回指针。由于两个进程 mmap 了相同的 hugepage 到相同的虚拟地址，指针可以直接跨进程使用。

---

*上一章：[Hugepages](./01-hugepages.md) | 下一章：[Lcore](./03-lcore.md)*
