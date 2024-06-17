[GNU Binutils](https://www.gnu.org/software/binutils/)
[LIB BFD, the Binary File Descriptor Library](https://ftp.gnu.org/old-gnu/Manuals/bfd-2.9.1/html_chapter/bfd_toc.html)


- [How programs get run](https://lwn.net/Articles/630727/)
- [How programs get run: ELF binaries](https://lwn.net/Articles/631631/)
- [Understanding mmap](https://unix.stackexchange.com/questions/389124/understanding-mmap)
- [Memory mapping](https://linux-kernel-labs.github.io/refs/heads/master/labs/memory_mapping.html)
- [More about   `mmap()`   file access](https://biriukov.dev/docs/page-cache/5-more-about-mmap-file-access/)
- [The mmap Device Operation](http://www.makelinux.net/ldd3/chp-15-sect-2.shtml)
- [In-depth understanding of mmap - kernel code analysis and driver demo examples](https://www.sobyte.net/post/2022-03/mmap/)
- [Understanding the Memory Layout of Linux Executables](https://gist.github.com/CMCDragonkai/10ab53654b2aa6ce55c11cfc5b2432a4)
- [**Operating Systems: Three Easy Pieces**](https://pages.cs.wisc.edu/~remzi/OSTEP/)
	- [**Operating Systems: Three Easy Pieces中文版**](https://pages.cs.wisc.edu/~remzi/OSTEP/Chinese/)
- [**Operating System Concepts**](https://pc.woozooo.com/mydisk.php)
	- [**操作系统导论**](https://github.com/gsZhiZunBao/e-books/blob/main/%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F%E5%AF%BC%E8%AE%BA.pdf)
-
- [嵌入式Linux学习目录](https://draapho.github.io/2017/11/23/1734-linux-content/)
- [[linux内核设计与实现]]
- [Memory mapping](https://linux-kernel-labs.github.io/refs/heads/master/labs/memory_mapping.html)
- [[Linux Input Subsystem]]
- [[dev]]
- [Unix Programming Frequently Asked Questions](http://web.archive.org/web/20120418113033/http://www.steve.org.uk/Reference/Unix/faq_toc.html)
- [**APUE**](https://notes.shichao.io/apue/)
- [jz2440学习笔记](https://yifengyou.gitbooks.io/jz2440/content/docs/Linux%E9%A9%B1%E5%8A%A8/) 知识点目录
- [**I.MX6U嵌入式Linux驱动开发指南**](https://github.com/alientek-openedv/imx6ull-document/blob/master/%E3%80%90%E6%AD%A3%E7%82%B9%E5%8E%9F%E5%AD%90%E3%80%91I.MX6U%E5%B5%8C%E5%85%A5%E5%BC%8FLinux%E9%A9%B1%E5%8A%A8%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97V1.5.2.pdf) #pdf #github
- [**Linux Storage**](http://linux-training.be/storage/index.html) #online
- [Linux From Scratch HOWTO](https://www.linuxfromscratch.org/museum/lfs-museum/2.0-beta1/LFS-HOWTO-2.0-beta1-HTML/LFS-HOWTO-2.0-beta1.html#toc10) #online
- [Linux System Administration](http://linux-training.be/sysadmin/index.html) #online
- [**Linux Internals**](https://www.learnlinux.org.za/courses/build/internals/) #online
- [linux online books](https://www.linuxtopia.org/online_books/) #online
- [Huge Page Settings and Disabling Huge Pages in Linux](https://www.baeldung.com/linux/huge-pages-management)

- linux kernel
	```bash
	morrism@localhost ~ $ uname -r
	4.18.0-305.25.1.el8_4.x86_64

	morrism@localhost ~ $ ls -l /boot/vmlinuz-4.18.0-305.25.1.el8_4.x86_64
	-rwxr-xr-x. 1 root root 10034312 Nov  3  2021 /boot/vmlinuz-4.18.0-305.25.1.el8_4.x86_64

	morrism@localhost ~ $ file /boot/vmlinuz-4.18.0-305.25.1.el8_4.x86_64
	/boot/vmlinuz-4.18.0-305.25.1.el8_4.x86_64: Linux kernel x86 boot executable bzImage, version 4.18.0-305.25.1.el8_4.x86_64 (mockbuild@kbuilder.bsys.centos.org) #1 SMP Wed Nov 3 10:29:07 UTC, RO-rootFS, swap_dev 0x9, Normal VGA

	列举硬件信息：
	lshw
	lspci

	lsusb
	lsblk

	lscpu
	lsdev

	hdparm - get/set SATA/IDE device parameters
	setpci - configure PCI devices

	System calls are functions implemented by the kernel and meant to be called from user space.

	man 2 read

	morrism@localhost ~ $ grep "NR_read" /usr/src/kernels/4.18.0-305.3.1.el8.x86_64/include/uapi/asm-generic/unistd.h
	#define __NR_read 63
	__SYSCALL(__NR_read, sys_read)
	#define __NR_readv 65
	__SC_COMP(__NR_readv, sys_readv, compat_sys_readv)
	#define __NR_readlinkat 78
	__SYSCALL(__NR_readlinkat, sys_readlinkat)
	#define __NR_readahead 213
	__SC_COMP(__NR_readahead, sys_readahead, compat_sys_readahead)

	printk() is the kernel's function for code to print messages, it is sent to RAM buffer and the system console, dmesg command can be used to show RAM buffer messages from kernel.

	morrism@localhost ~ $ dmesg | grep command
	[    0.000000] Kernel command line: BOOT_IMAGE=(hd0,msdos1)/vmlinuz-4.18.0-305.25.1.el8_4.x86_64 root=/dev/mapper/cl-root ro resume=/dev/mapper/cl-swap rd.lvm.lv=cl/root rd.lvm.lv=cl/swap rhgb quiet

	morrism@localhost ~ $ sudo journalctl -t kernel | grep command
	Jun 17 08:47:25 localhost.localdomain kernel: Kernel command line: BOOT_IMAGE=(hd0,msdos1)/vmlinuz-4.18.0-305.25.1.el8_4.x86_64 root=/dev/mapper/cl-root ro resume=/dev/mapper/cl-swap rd.lvm.lv=cl/root rd.lvm.lv=cl/swap rhgb quiet
	...

	morrism@localhost /tmp/x $ strace date |& grep read
	read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\2405\2\0\0\0\0\0"..., 832) = 832
	read(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32) = 32
	read(3, "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\3\0\0\0\3\0\0\0\0"..., 4096) = 582
	read(3, "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\3\0\0\0\3\0\0\0\0"..., 4096) = 357

	morrism@localhost /tmp/x $ strace -c date
	Mon Jun 17 09:56:41 CST 2024
	% time     seconds  usecs/call     calls    errors syscall
	------ ----------- ----------- --------- --------- ----------------
	66.07    0.000294         294         1           execve
	9.44    0.000042           7         6           mmap
	8.31    0.000037           9         4           openat
	2.47    0.000011           2         4           read
	2.47    0.000011           1         6           close
	2.47    0.000011          11         1         1 access
	2.25    0.000010           1         6           fstat
	2.25    0.000010           2         4           mprotect
	2.25    0.000010           5         2         1 arch_prctl
	1.12    0.000005           2         2           lseek
	0.90    0.000004           1         4           brk
	0.00    0.000000           0         1           write
	0.00    0.000000           0         1           munmap
	------ ----------- ----------- --------- --------- ----------------
	100.00    0.000445          10        42         2 total


	morrism@PC24036:~/ubuntu$ strace cd /tmp
	strace: Can't stat 'cd': No such file or directory
	morrism@PC24036:~/ubuntu$ cat cd.sh
	#!/bin/bash

	cd "$@"
	morrism@PC24036:~/ubuntu$ strace ./cd.sh /tmp/ |& grep chdir
	chdir("/tmp")                           = 0

	morrism@PC24036:~/ubuntu$ find /proc/sys | grep ip_forward
	/proc/sys/net/ipv4/ip_forward
	/proc/sys/net/ipv4/ip_forward_update_priority
	/proc/sys/net/ipv4/ip_forward_use_pmtu

	morrism@PC24036:~/ubuntu$ sysctl net.ipv4.ip_forward
	net.ipv4.ip_forward = 0



	```