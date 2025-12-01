[**dpdk source code**](https://elixir.bootlin.com/dpdk/latest/source) #online  
[**dpdk**](https://github.com/DPDK/dpdk) #github  

[**Programmer’s Guide**](https://doc.dpdk.org/guides/prog_guide/index.html)  
[Programmer’s Guide](https://doc.dpdk.org/guides-24.07/prog_guide/index.html)  
[Core Components](https://doc.dpdk.org/guides/prog_guide/overview.html)  

[ReadmeX DPDK](https://readmex.com/en-US/DPDK/dpdk/page-18fde8de6-6adc-4a25-8704-31c9f1196d7f)  

[Quality of Service (QoS) Framework](https://doc.dpdk.org/guides-24.07/prog_guide/qos_framework.html)  

[dpdk 编程指南](https://dpdk-docs.readthedocs.io/en/latest/prog_guide/index.html)  
[DPDK API](https://doc.dpdk.org/api/)  

[Link Bonding Poll Mode Driver Library](https://doc.dpdk.org/guides/prog_guide/link_bonding_poll_mode_drv_lib.html)  
[]()  
[]()  
[]()  

![dpdk howo](./dpdk/dpdk_howto.md)
![dpdk初始化及包处理流程](./dpdk/DPDK_initialization_and_packet_processing.md)  
![DPDK 网卡直接访问 Huge Page 的核心机制](./dpdk/DPDK_UIO_and_Hugepage_Mechanism.md)  
![DPDK 网卡驱动重新绑定机制详解](./dpdk/DPDK_Driver_Binding_Mechanism.md)  
![]()  
![]()  

## EAL 初始化
EAL 初始化（rte_eal_init()）就是把进程从“普通用户态程序”变成“适合做高性能报文处理的运行环境”
1. 解析 EAL 参数、初始化日志与配置
作用：统一处理 DPAA、PCI、内存、lcore 等的全局参数，让后续模块有一致的配置基础。
2. Hugepage 内存初始化（DPDK 内存子系统）
从内核预留的 HugePages（例如 2MB / 1GB）中：
发现各 NUMA node 上的巨大页内存。
通过 mmap() 把 hugepage 映射进进程地址空间。
基于 hugepage 内存构建：
memseg / memzone：大块连续物理内存描述。
mempool 基础：后面你创建 rte_mempool（mbuf pool 等）时就依赖这套物理连续内存。
处理多进程场景：
使用 /var/run/.rte_config + hugepage 文件来在 primary/secondary 进程间共享同一块 DPDK 内存。
作用：给 DPDK 提供“高带宽、物理连续、可共享”的基础内存池，使得 DMA、零拷贝、高速 cache 友好都成为可能。
3. Lcore（逻辑核）和线程运行环境
探测系统 CPU 拓扑（NUMA 节点、core、socket）。
按参数选择一组 lcore：
标记 RTE_MAX_LCORE 中哪些是启用的。
指定哪个是 master lcore，哪些是 slave lcore。
为每个 lcore 建立 per-lcore 数据区：
rte_lcore_id()、RTE_PER_LCORE() 这样的宏都需要这些数据。
后续当你调用：
rte_eal_mp_remote_launch() / rte_eal_remote_launch() 时：
EAL 会在各个 slave lcore 上创建 POSIX 线程，把它们绑到指定 CPU 上执行你的循环（如 lcore_main()）。
作用：为“每核一个轮询循环”的 DPDK 编程模型打好多核、绑核和 per-lcore 数据基础。
4. 总线与设备发现 / 绑定（NIC 等）
构建 DPDK 的 总线框架（rte_bus）：
默认包含 PCI bus、VDEV bus 等。
扫描系统上的设备：
PCI 扫描：枚举所有 NIC、加速卡等。
虚拟总线设备：根据 --vdev 等 EAL 参数创建软件设备（如 TAP、PCAP、ring 等）。
将发现的设备绑定到 DPDK 驱动：
使用 vfio-pci / igb_uio 等驱动（要提前绑定好）。
为每个驱动创建 rte_device / rte_eth_dev 对象。
初始化基本的驱动层：
之后你调用 rte_eth_dev_configure()、rte_eth_rx_queue_setup() 等才能工作。
作用：把系统 NIC/加速卡“接入”到 DPDK 世界，使其可以绕过内核协议栈进行用户态高速收发包。
5. 计时器、闹钟、信号等运行时服务
初始化 定时器子系统：
基于 TSC 或 HPET 等高精度时钟，提供 rte_timer。
初始化闹钟/报警机制：
供超时、周期任务使用。
设置部分信号处理（如 SIGINT/SIGTERM）：
提供统一退出/清理路径（根据应用需要使用）。
作用：为应用提供通用的时间轮询、超时控制和有序退出能力。
6. 多进程/多实例支持的基础
通过 --proc-type=primary/secondary、--file-prefix：
primary 创建共享内存区域、设备全局状态。
secondary 进程附着到已有的 hugepage 区域和设备元数据。
这让多个 DPDK 进程可以共享同一块 NIC 队列/内存，分别负责不同功能（如收包进程、处理进程、统计进程等）。
作用：为更复杂的多进程架构提供统一的共享环境，避免重复分配设备和内存。

总结
EAL 初始化做的就是：
解析配置 → 准备 hugepage 内存 → 建立 lcore 运行环境 → 发现并绑定设备 → 初始化计时/日志等基础设施，
让你的主业务代码可以专注在 while (1) { rte_eth_rx_burst(...); ... } 这样的高速报文循环上，而不用关心底层 OS/硬件细节。

当把 NIC 从内核驱动解绑，绑定到 DPDK 兼容驱动（如 vfio-pci / igb_uio），并用 DPDK PMD 配好 RX 队列后，内核就不再为这个网卡创建正常的 netdev/队列，也就无法把包送进协议栈；此时只有用户态 DPDK 程序在轮询这些队列并收包

## 基本概念
EAL(Environment Abstraction Layer)，负责为应用间接访问底层的资源，比如内存空间、线程、设备、定时器等

传统linux内核接收数据包处理流程  
```
	  1.网卡收到报文后，通过DMA将报文从网卡拷贝到内存
	  2.网卡触发中断通知系统有报文到达（一个报文，一个中断）
	  3.系统分配sk_buff,将报文拷贝至sk_buff
	  4.协议栈对sk_buff中的报文进行处理，报文处理完成后从内核空间拷贝至用户将空间buff供用户空间处理
	  
	  整个过程涉及多次用户空间和内核空间之间的内存拷贝和系统调用，占用大量的系统资源
```
DPDK依赖的主要技术  
```
	UIO: 运行用户态的驱动，减少用户空间和内存空间的数据拷贝，通过Baypass Kernel直接在用户态通过轮询机制对数据进行操作        
	HUGEPAGE: 利用大内存页机制提高内存的访问效率（提高Cache的命中率） 
	CPU Affinity: 减少进程或线程在不同CPU core之间切换锁产生的开销，从而提高CPU的利用率  
```

[EAL parameters](https://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html)  
[DPDK Tools User Guides](https://doc.dpdk.org/guides/tools/index.html)  
[DPDK系列](https://blog.csdn.net/fpcc/article/details/135179524)  
[Configuration file syntax](https://dpdk-docs.readthedocs.io/en/latest/sample_app_ug/ip_pipeline.  html#configuration-file-syntax)
[dpdk application configuration file format](https://doc.dpdk.org/guides-18.02/sample_app_ug/ip_pipeline.  html#ip-pipeline-configuration-file)
[What is DPDK?](https://www.packetcoders.io/what-is-dpdk/)  
[DPDK Overview](https://doc.dpdk.org/guides/prog_guide/overview.html)  
[DPDK file list](https://doc.dpdk.org/api/files.html) #online  
	  id:: 6583e4b3-c03a-445a-bde5-40af958c49bb
[Tun|Tap Poll Mode Driver](https://doc.dpdk.org/guides/nics/tap.html)  
[深入浅出DPDK](https://zzqcn.github.io/opensource/dpdk/hf-dpdk/index.html) #online  
[深入浅出DPDK](https://github.com/0voice/expert_readed_books/blob/master/%E8%AE%A1%E7%AE%97%E6%9C%BA%E7%A7%91%E5%AD%A6/%E6%B7%B1%E5%85%A5%E6%B5%85%E5%87%BADPDK.pdf) #pdf #online #github  
[DPDK 实现的不完全笔记](https://switch-router.gitee.io/blog/dpdk-note/)  
[DPDK documentation](https://doc.dpdk.org/guides/index.html)  
[Memory in Data Plane Development Kit](https://www.intel.com/content/www/us/en/developer/articles/technical/  memory-in-dpdk-part-1-general-concepts.html)
[从零开始学习DPDK：掌握这些常用库函数就够了](https://zhuanlan.zhihu.com/p/644115268)  
[Sample Applications User Guides](https://doc.dpdk.org/guides/sample_app_ug/)  
[Mbuf Library](https://doc.dpdk.org/guides/prog_guide/mbuf_lib.html)  
[DPDK Coding Style](https://doc.dpdk.org/guides/contributing/coding_style.html)  
[Get Started Using the Data Plane Development Kit (DPDK) Software Network Interface Card (Soft NIC)](https://www.intel.com/content/www/us/en/developer/articles/guide/get-started-using-the-dpdk-soft-network-interface-card-soft-nic.html)  
[Tun|Tap Poll Mode Driver](https://doc.dpdk.org/guides/nics/tap.html)  
[CONTRAIL DPDK vROUTER](https://www.juniper.net/documentation/en_US/day-one-books/contrail-DPDK.pdf)  
[Managing Processors Availability](https://www.baeldung.com/linux/managing-processors-availability)  
[rte_mbuf.h File Reference](https://doc.dpdk.org/api/rte__mbuf_8h.html)  
[Mbuf Library](https://doc.dpdk.org/guides/prog_guide/mbuf_lib.html)  
[mbuf和mempool](https://zhuanlan.zhihu.com/p/543676558)  
[libmoon](https://github.com/libmoon/libmoon)  
[rte_ring.h File Reference](https://doc.dpdk.org/api/rte__ring_8h.html)  
[rte_mbuf_dyn.h File Reference](https://doc.dpdk.org/api/rte__mbuf__dyn_8h.html#ac5f25ac463dea2d1b3f452fa8f430650)  
[librte_net Directory Reference](https://doc.dpdk.org/api-19.11/dir_0c4a44b1891135ef8e3f51c114dfc40e.html)  
