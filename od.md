- 示例
	- `od -Ax -t x1 -j 166 -N 4 main`
		- -Ax
			- 按照16进制格式输出
		- -t x1
			- 输出格式为十六进制单字节显示
		- -j 166
			- 从文件开始跳过166(0xa6)个字节
		- -N 4
			- 只dump 4字节数据
	- 判断系统大小端
	  collapsed:: true
		- ```bash
		  echo -n I | od -to2 | head -n1 | cut -f2 -d " " | cut -c6
		  0: Big-Endian
		  1: Little-Endian
		  
		  # 也可以通过lscpu命令查看系统的大小端
		  root@slot-18:~ [P2]# lscpu
		  Architecture:        ppc64
		  CPU op-mode(s):      32-bit, 64-bit
		  Byte Order:          Big Endian
		  CPU(s):              8
		  On-line CPU(s) list: 0-7
		  Thread(s) per core:  2
		  Core(s) per socket:  4
		  Socket(s):           1
		  Model:               2.0 (pvr 8040 0120)
		  Model name:          e6500, altivec supported
		  L1d cache:           128 KiB
		  L1i cache:           128 KiB
		  L2 cache:            2 MiB
		  L3 cache:            1 MiB
		  
		  # 也可以通过file命令查看系统大小端
		  root@slot-120:~ [P2]# file /opt/berwick/build/armstrong/powerpc/bin/cms
		  /opt/berwick/build/armstrong/powerpc/bin/cms: ELF 32-bit MSB executable, PowerPC or cisco 4500, version 1 (SYSV), dynamically linked, interpreter /lib/ld.so.1, BuildID[sha1]=8e66834f791f43124f9cf5fcec1fd0f232e31055, for GNU/Linux 3.2.0, with debug_info, not stripped
		  
		  # 也可以通过命令查看elf文件头信息来获取系统大小端信息
		  $ readelf -h ./build/armstrong/powerpc/bin/cms
		  ELF Header:
		    Magic:   7f 45 4c 46 01 02 01 00 00 00 00 00 00 00 00 00
		    Class:                             ELF32
		    Data:                              2's complement, big endian
		    Version:                           1 (current)
		    OS/ABI:                            UNIX - System V
		    ABI Version:                       0
		    Type:                              EXEC (Executable file)
		    Machine:                           PowerPC
		    Version:                           0x1
		    Entry point address:               0x10003ec0
		  
		  ```
- 参考文档
	- [man od](https://man7.org/linux/man-pages/man1/od.1.html)