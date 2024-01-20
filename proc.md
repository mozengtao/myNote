- [[procfs]]
- proc文件系统是一个==伪文件系统==，它提供了访问linux内核数据结构的接口。
  > The proc filesystem is a ==pseudo-filesystem== which provides an ==interface to kernel data structures==.  It is commonly ==mounted at /proc==.  Typically, it is mounted automatically by the system, but it can also be mounted manually using a command such as:
    `mount -t proc proc /proc`
    Most of the files in the proc filesystem are read-only, but some files are writable, allowing kernel variables to be changed.
- /proc目录结构
	- ```sh
	  /proc/
	  ├── 1
	  |......(pid)
	  ├── buddyinfo
	  ├── cmdline
	  ├── consoles
	  ├── cpuinfo
	  ├── crypto
	  ├── devices
	  ├── diskstats
	  ├── dma
	  ├── driver
	  ├── filesystems
	  ├── fs
	  ├── interrupts
	  ......
	  ```
- `/proc/cmdline`
	- > This file shows the parameters passed to the kernel at the time it is started
	- [Kernel arguments](https://linux-sunxi.org/Kernel_arguments)
- `/proc/config.gz`
  collapsed:: true
	- > Linux can store a gzip’ed copy of the kernel configuration file used to build the kernel in the kernel itself, and make it available to users via */proc/config.gz*
	- ```bash
	  root@slot-120:/etc/profile.d# zcat /proc/config.gz | head -n 3
	  #
	  # Automatically generated file; DO NOT EDIT.
	  # Linux/powerpc 4.1.8 Kernel Configuration
	  root@slot-120:/etc/profile.d# zcat /proc/config.gz | head -n 5
	  #
	  # Automatically generated file; DO NOT EDIT.
	  # Linux/powerpc 4.1.8 Kernel Configuration
	  #
	  CONFIG_PPC64=y
	  ```
- `/proc/devices`
- `/proc/meminfo`
- `/proc/PID/maps`
- `/proc/PID/smaps`
- 参考文档
	- [man 5 proc](https://man7.org/linux/man-pages/man5/proc.5.html)
	- [The /proc Filesystem](https://www.kernel.org/doc/html/latest/filesystems/proc.html#)
	- [proc filesystem hierarchy](https://tldp.org/LDP/Linux-Filesystem-Hierarchy/html/proc.html)
	- [THE /proc FILESYSTEM](https://www.kernel.org/doc/Documentation/filesystems/proc.txt)
	- [The /proc/meminfo File in Linux](https://www.baeldung.com/linux/proc-meminfo)
	- [Understanding the Linux /proc/id/maps File](https://www.baeldung.com/linux/proc-id-maps)