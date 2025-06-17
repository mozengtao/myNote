[**Programmer’s Guide**](https://doc.dpdk.org/guides/prog_guide/index.html)  
[Quality of Service (QoS) Framework](https://doc.dpdk.org/guides-24.07/prog_guide/qos_framework.html)  

[dpdk](https://github.com/DPDK/dpdk) #github  
[dpdk 编程指南](https://dpdk-docs.readthedocs.io/en/latest/prog_guide/index.html)  
[**dpdk source code**](https://elixir.bootlin.com/dpdk/latest/source) #online  
[DPDK API](https://doc.dpdk.org/api/)  



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
**An mbuf with One Segment**  
![../_images/mbuf1.svg](https://doc.dpdk.org/guides/_images/mbuf1.svg)  
**An mbuf with Three Segments**  
![../_images/mbuf2.svg](https://doc.dpdk.org/guides/_images/mbuf2.svg)  
[mbuf和mempool](https://zhuanlan.zhihu.com/p/543676558)  
**[libmoon](https://github.com/libmoon/libmoon)**  
[rte_ring.h File Reference](https://doc.dpdk.org/api/rte__ring_8h.html)  
[rte_mbuf_dyn.h File Reference](https://doc.dpdk.org/api/rte__mbuf__dyn_8h.html#ac5f25ac463dea2d1b3f452fa8f430650)  
[librte_net Directory Reference](https://doc.dpdk.org/api-19.11/dir_0c4a44b1891135ef8e3f51c114dfc40e.html)  
