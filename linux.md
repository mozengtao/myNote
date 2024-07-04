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

- [The kernel’s command-line parameters](https://www.kernel.org/doc/html/latest/admin-guide/kernel-parameters.html)

- [How to uncompress and list an initramfs content on Linux](https://linuxconfig.org/how-to-uncompress-and-list-an-initramfs-content-on-linux)
- [Linux Kernel Development - Extending Drivers for new Devices](https://javiercarrascocruz.github.io/)

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

	[vcapuser@WEIR_STRAWBERRY-1 ~]$ cd /dev/
	[vcapuser@WEIR_STRAWBERRY-1 dev]$ ls -l null
	crw-rw-rw-. 1 root root 1, 3 Jun 15 19:47 null
	[vcapuser@WEIR_STRAWBERRY-1 dev]$ ls -l |grep "^c" | grep " 1,"
	crw-rw-rw-. 1 root root      1,   7 Jun 15 19:47 full
	crw-r--r--. 1 root root      1,  11 Jun 15 19:47 kmsg
	crw-r-----. 1 root kmem      1,   1 Jun 15 19:47 mem
	crw-rw-rw-. 1 root root      1,   3 Jun 15 19:47 null
	crw-r-----. 1 root kmem      1,   4 Jun 15 19:47 port
	crw-rw-rw-. 1 root root      1,   8 Jun 15 19:47 random
	crw-rw-rw-. 1 root root      1,   9 Jun 15 19:47 urandom
	crw-rw-rw-. 1 root root      1,   5 Jun 15 19:47 zero
	[vcapuser@WEIR_STRAWBERRY-1 dev]$ cd /tmp/
	[vcapuser@WEIR_STRAWBERRY-1 tmp]$ sudo mknod pickle c 1 2
	[vcapuser@WEIR_STRAWBERRY-1 tmp]$ ls -l pickle
	crw-r--r--. 1 root root 1, 2 Jun 16 22:26 pickle
	[vcapuser@WEIR_STRAWBERRY-1 tmp]$ cat pickle
	cat: pickle: No such device or address

	mknod 创建的是device file 而不是 device, 所以设备文件存在并不意味着设备存在。

	GRUB
	GRUB comes after POST, and the BIOS
	GRUB is installed in a special place on disk
	GRUB loads the kernel, initial root filesystem, sets up the kernel command line, and then transfers control to the kernel.
	GRUB can be interrupted, and you can interact with it.

	[root@STB-2 ~]# man -k printf
	asprintf (3)         - print to allocated string
	dprintf (3)          - print to a file descriptor
	format (n)           - Format a string in the style of sprintf
	fprintf (3)          - formatted output conversion
	fprintf (3p)         - print formatted output
	fwprintf (3)         - formatted wide-character output conversion
	fwprintf (3p)        - print formatted wide-character output
	ldns_buffer_printf (3) - (unknown subject)
	printf (1)           - format and print data
	printf (1p)          - write formatted output
	printf (3)           - formatted output conversion
	printf (3p)          - print formatted output
	snprintf (3)         - formatted output conversion
	snprintf (3p)        - print formatted output
	......

	[root@STB-2 ~]# file /etc/default/grub
	/etc/default/grub: ASCII text
	[root@STB-2 ~]# ls -l /etc/grub.d/
	total 72
	-rwxr-xr-x. 1 root root  8702 Jul 28  2020 00_header
	-rwxr-xr-x. 1 root root  1043 Mar 21  2019 00_tuned
	-rwxr-xr-x. 1 root root   232 Jul 28  2020 01_users
	-rwxr-xr-x. 1 root root 10781 Jul 28  2020 10_linux
	-rwxr-xr-x. 1 root root 10275 Jul 28  2020 20_linux_xen
	-rwxr-xr-x. 1 root root  2559 Jul 28  2020 20_ppc_terminfo
	-rwxr-xr-x. 1 root root 11169 Jul 28  2020 30_os-prober
	-rwxr-xr-x. 1 root root   214 Jul 28  2020 40_custom
	-rwxr-xr-x. 1 root root   216 Jul 28  2020 41_custom
	-rw-r--r--. 1 root root   483 Jul 28  2020 README

	morrism@localhost ~/repos/fsl-linux (develop) $ sudo ls -l /boot/grub2/grub.cfg
	-rw-r--r--. 1 root root 5115 Nov 17  2020 /boot/grub2/grub.cfg


	grub2-mkconfig command is used to generate a new config file

	Root file system: the filesystem that contains "/".
	Initial RAM disk or RAM filesystem(initrd) is used to provide drivers and support for mounting the system's real root file system. initrd has an init that the kernel runs first. When the init from the initrd terminates, the Linux kernel starts init again; this time from the real filesystem, which is commonly on disk.

	morrism@localhost ~/repos/fsl-linux (develop) $ sudo ls -l /proc/1/exe
	lrwxrwxrwx. 1 root root 0 Jun 17 08:47 /proc/1/exe -> /usr/lib/systemd/systemd
	morrism@localhost ~/repos/fsl-linux (develop) $ ls -l /lib
	lrwxrwxrwx. 1 root root 7 Nov  3  2020 /lib -> usr/lib
	morrism@localhost ~/repos/fsl-linux (develop) $ which init
	/usr/sbin/init
	morrism@localhost ~/repos/fsl-linux (develop) $ ls -l /usr/sbin/init
	lrwxrwxrwx. 1 root root 22 Aug 10  2021 /usr/sbin/init -> ../lib/systemd/systemd

	morrism@localhost ~/repos/fsl-linux (develop) $ sudo file /boot/initramfs-*
	/boot/initramfs-0-rescue-459bdef659c147f88006c050437993e0.img: ASCII cpio archive (SVR4 with no CRC)
	/boot/initramfs-4.18.0-240.15.1.el8_3.x86_64.img:              ASCII cpio archive (SVR4 with no CRC)
	/boot/initramfs-4.18.0-305.25.1.el8_4.x86_64.img:              gzip compressed data, max compression, from Unix, original size 100695552
	/boot/initramfs-4.18.0-305.3.1.el8.x86_64.img:                 gzip compressed data, max compression, from Unix, original size 100692992
	initrd 每个 kernel release 都有一个对应的initrd

	LKM

	morrism@localhost ~/repos/fsl-linux (develop) $ uname -r
	4.18.0-305.25.1.el8_4.x86_64
	morrism@localhost ~/repos/fsl-linux (develop) $ ls -l /lib/modules/4.18.0-305.25.1.el8_4.x86_64
	total 17952
	-rw-r--r--.  1 root root      323 Nov  3  2021 bls.conf
	lrwxrwxrwx.  1 root root       45 Nov  3  2021 build -> /usr/src/kernels/4.18.0-305.25.1.el8_4.x86_64
	-rw-r--r--.  1 root root   192173 Nov  3  2021 config
	drwxr-xr-x.  2 root root        6 Nov  3  2021 extra
	drwxr-xr-x. 13 root root      141 Feb 24  2023 kernel
	-rw-r--r--.  1 root root   888946 Feb 24  2023 modules.alias
	-rw-r--r--.  1 root root   851641 Feb 24  2023 modules.alias.bin
	-rw-r--r--.  1 root root      468 Nov  3  2021 modules.block
	-rw-r--r--.  1 root root     7687 Nov  3  2021 modules.builtin
	-rw-r--r--.  1 root root     9869 Feb 24  2023 modules.builtin.bin
	-rw-r--r--.  1 root root   313488 Feb 24  2023 modules.dep
	-rw-r--r--.  1 root root   427426 Feb 24  2023 modules.dep.bin
	-rw-r--r--.  1 root root      383 Feb 24  2023 modules.devname
	-rw-r--r--.  1 root root      153 Nov  3  2021 modules.drm
	-rw-r--r--.  1 root root       34 Nov  3  2021 modules.modesetting
	-rw-r--r--.  1 root root     1627 Nov  3  2021 modules.networking
	-rw-r--r--.  1 root root   104032 Nov  3  2021 modules.order
	-rw-r--r--.  1 root root      638 Feb 24  2023 modules.softdep
	-rw-r--r--.  1 root root   437190 Feb 24  2023 modules.symbols
	-rw-r--r--.  1 root root   531055 Feb 24  2023 modules.symbols.bin
	lrwxrwxrwx.  1 root root        5 Nov  3  2021 source -> build
	-rw-r--r--.  1 root root   367479 Nov  3  2021 symvers.gz
	-rw-------.  1 root root  4166558 Nov  3  2021 System.map
	drwxr-xr-x.  2 root root        6 Nov  3  2021 updates
	drwxr-xr-x.  2 root root       40 Feb 24  2023 vdso
	-rwxr-xr-x.  1 root root 10034312 Nov  3  2021 vmlinuz
	drwxr-xr-x.  3 root root       23 Feb 24  2023 weak-updates

	morrism@localhost ~/repos/fsl-linux (develop) $ uname -r
	4.18.0-305.25.1.el8_4.x86_64

	morrism@localhost ~/repos/fsl-linux (develop) $ ls /lib/modules/4.18.0-305.25.1.el8_4.x86_64/build/
	arch   certs   drivers   fs       init  Kconfig  lib       Makefile.rhelver  Module.symvers  samples  security  System.map  usr   vmlinux.id
	block  crypto  firmware  include  ipc   kernel   Makefile  mm                net             scripts  sound     tools       virt

	如果编译linux module
	make -C /lib/modules/$(uname -r)/build M=$PWD modules

	在当前工作目录创建：
	simplemodule.c
	Makefile:
		obj-m := simplemodule.o

	```