- [giving applications a chance to survive OS kernel crashes](https://www.usenix.org/legacy/event/hotdep08/tech/full_papers/depoutovitch/depoutovitch_html/index.html)
- [[The Linux Startup Process]]
- linux内核子系统
	- 进程调度
		- process scheduler，也称作进程管理，负责挂起和恢复进程
	- 内存管理
		- memory manager，负责管理内存资源，提供虚拟内存机制
	- 虚拟文件系统
		- virtual file system，隐藏硬件细节，为所有外设提供统一的操作接口
	- 网络
		- network，负责管理系统中网络设备
	- 进程间通信
		- inter-process communication，提供多个进程，多资源的互斥访问、进程间的同步和消息传递机制
	- [Linux内核的整体架构](http://www.wowotech.net/linux_kenrel/11.html)
	- [[Kconfig]]
	- [[Kbuild]]
- 参考文档
	- [Some explanation to some errors and warnings](https://helpful.knobs-dials.com/index.php/Some_explanation_to_some_errors_and_warnings)
	- [The Linux kernel](https://www.win.tue.nl/~aeb/linux/lk/lk.html#toc1)
	- [内核编译系统概述](https://github.com/PinoTsao/Makefile/blob/master/01.kbuild_summary.md)
	- [Linux Kernel Makefiles](https://docs.kernel.org/kbuild/makefiles.html)
	- [Linux Kernel Makefiles](https://www.kernel.org/doc/html/v5.6/kbuild/makefiles.html)
	- [kernel Index of /doc/Documentation/](https://www.kernel.org/doc/Documentation/)
	- [linux kernel config](https://www.kernelconfig.io/index.html) **search linux kernel module name or linux kernel config name**
	- [kthreads](https://www.kernel.org/doc/Documentation/kernel-per-CPU-kthreads.txt)
	- [Kernel threads](https://subscription.packtpub.com/book/application-development/9781785883057/1/ch01lvl1sec13/kernel-threads)
	- [The Linux Kernel documentation](https://www.kernel.org/doc/html/latest/)
	- [OS Dev](https://wiki.osdev.org/Main_Page)
	- [The Linux Kernel Archives](https://www.kernel.org/)
	- [Learning operating system development using Linux kernel and Raspberry Pi](https://github.com/s-matyukevich/raspberry-pi-os)
	- [perf-tools](https://github.com/brendangregg/perf-tools)
	- [The linux-mtd Archives](http://lists.infradead.org/pipermail/linux-mtd/)
	- [Linux内核启动流程-基于ARM64](https://mshrimp.github.io/2020/04/19/Linux%E5%86%85%E6%A0%B8%E5%90%AF%E5%8A%A8%E6%B5%81%E7%A8%8B-%E5%9F%BA%E4%BA%8EARM64/)
	- [Linux Kernel 启动全过程](http://119.23.219.145/posts/linux-kernel-linux-kernel-%E5%90%AF%E5%8A%A8%E5%85%A8%E8%BF%87%E7%A8%8B/)
	- [Linux启动过程综述](https://sites.google.com/site/frankindai/linux-start-up)
	- [linux内核相关](https://mshrimp.github.io/archives/)
	- [程序锅](http://119.23.219.145/)
	- [Linux等待队列](https://hughesxu.github.io/posts/Linux_Wait_Queue/)
	- [The Linux kernel user's and administrator's guide](https://docs.kernel.org/admin-guide/index.html)
	- [copy_from_user 分析](https://www.cnblogs.com/rongpmcu/p/7662749.html)
	  id:: 65129e48-147d-4897-82dc-88d6a179d592
	- [What does __init mean in the Linux kernel code?](https://stackoverflow.com/questions/8832114/what-does-init-mean-in-the-linux-kernel-code)  

	## linux source code
	- Recommended: Linux 2.6.32 or 3.2
	✔ Why 2.6.32?
		Longest-lived LTS kernel in history
		Stable, widely used in production for a decade
		Modern features included:
		cgroups v1
		early RCU implementation
		modern scheduler (CFS)
		netfilter/iptables in usable form
		ext4

		Codebase is much smaller and simpler than 4.x/5.x/6.x

	✔ Why 3.2?
		Not too big, not too old
		Very readable VM, scheduler, filesystems
		Cleaner than 2.6
		Contains modern APIs without the huge complexity introduced later
		💡 If I had to pick one version for learning, it's Linux 3.2.

	- Good first modules to read
		| Subsystem                    | Difficulty | Why it's good                   |
		| ---------------------------- | ---------- | ------------------------------- |
		| `kernel/sched` (CFS)         | Medium     | Modern scheduling principles    |
		| `kernel/rcu`                 | Hard       | Key to modern concurrency       |
		| `fs/ext2`                    | Easy       | Simple filesystem to understand |
		| `mm` (page allocator, buddy) | Medium     | Core memory management concepts |
		| `drivers/char/random.c`      | Easy       | Self-contained                  |
		| `net/ipv4`                   | Medium     | Classic TCP/IP implementation   |

	- clone only the Linux 3.2 tag (faster, smaller)
		git clone --depth 1 --branch v3.2 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git

## linux 架构
┌─────────────────────────────────────────────────────────────────┐
│                     用户空间 (User Space)                        │
├─────────────────────────────────────────────────────────────────┤
│                   系统调用接口 (System Call Interface)           │
├────────────┬────────────┬────────────┬────────────┬─────────────┤
│   进程管理 │   内存管理  │  文件系统   │ 网络协议栈  │    IPC      │
│  kernel/   │    mm/     │    fs/     │    net/    │   ipc/      │
├────────────┴────────────┴────────────┴────────────┴─────────────┤
│              虚拟文件系统 VFS / 通用块层 block/                   │
├─────────────────────────────────────────────────────────────────┤
│                    设备驱动层 drivers/                           │
├─────────────────────────────────────────────────────────────────┤
│                 体系结构抽象层 arch/                             │
├─────────────────────────────────────────────────────────────────┤
│                     硬件 (Hardware)                             │
└─────────────────────────────────────────────────────────────────┘