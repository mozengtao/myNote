- 打印文件的共享库依赖
	- > ldd - print shared object dependencies
- 原理
	- ldd是一个shell脚本，ldd显示可执行文件的依赖的原理是：通过设置一系列环境变量如`LD_TRACE_LOADED_OBJECTS、LD_WARN、LD_BIND_NOW、LD_LIBRARY_VERSION、LD_VERBOSE`等，当`LD_TRACE_LOADED_OBJECTS`环境变量不为空时，任何可执行程序在运行时，它都会只显示模块的dependency，而程序并不真正执行
		- ```bash
		  # 当设置如下变量时，在shell终端执行任何程序都只会显示模块的dependency而不真正的执行程序
		  export LD_TRACE_LOADED_OBJECTS=1
		  ```
	- ldd显示可执行程序的[[动态库]]依赖，实质是通过**ld-linux.so**（elf动态库的装载器）来实现的，ld-linux.so模块会先于executable模块程序工作，并获得控制权，因此当上述的那些环境变量被设置时，ld-linux.so选择了显示可执行模块的依赖。也可以通过执行ld-linux.so模块直接显示可执行程序的动态库依赖，如
		- 通过file命令可以查看文件的解释器(也叫作运行时链接器)
			- ```bash
			  # 查看文件的解释器
			  root@slot-120:~ [P2]# file /opt/berwick/build/armstrong/powerpc/bin/cms
			  /opt/berwick/build/armstrong/powerpc/bin/cms: ELF 32-bit MSB executable, PowerPC or cisco 4500, version 1 (SYSV), dynamically linked, interpreter /lib/ld.so.1, BuildID[sha1]=8e66834f791f43124f9cf5fcec1fd0f232e31055, for GNU/Linux 3.2.0, with debug_info, not stripped
			  
			  # 利用解释器查看文件的动态库依赖
			  root@slot-120:~ [P1]# strings /usr/sbin/rsyslogd | grep ld.so
			  /lib/ld.so.1
			  
			  root@slot-120:~ [P2]# /lib/ld.so.1 --list /usr/bin/gs_cli
			          linux-vdso32.so.1 (0x00100000)
			          libm.so.6 => /lib/libm.so.6 (0x0ff0f000)
			          librt.so.1 => /lib/librt.so.1 (0x0fede000)
			          libxml2.so.2 => /usr/lib/libxml2.so.2 (0x0fd1c000)
			          libz.so.1 => /lib/libz.so.1 (0x0fcdb000)
			          libssl.so.1.1 => /usr/lib/libssl.so.1.1 (0x0fc18000)
			          libcrypto.so.1.1 => /usr/lib/libcrypto.so.1.1 (0x0f943000)
			          libpcap.so.1 => /usr/lib/libpcap.so.1 (0x0f8d2000)
			          libutil.so.1 => /lib/libutil.so.1 (0x0f8a1000)
			          libconfd.so => /opt/berwick/tailf-confd-pkg/powerpc/lib/libconfd.so (0x0f7f5000)
			          libauthentication.so.10 => /usr/lib/libauthentication.so.10 (0x0f7c4000)
			          libpthread.so.0 => /lib/libpthread.so.0 (0x0f771000)
			          libc.so.6 => /lib/libc.so.6 (0x0f57d000)
			          /lib/ld.so.1 (0x20198000)
			          libdl.so.2 => /lib/libdl.so.2 (0x0f54c000)
			  ```
			- ```bash
			  morrism@localhost $ /lib64/ld-linux-x86-64.so.2 --list /usr/bin/ls
			          linux-vdso.so.1 (0x00007fff6d5f8000)
			          libselinux.so.1 => /lib64/libselinux.so.1 (0x00007fc67d799000)
			          libcap.so.2 => /lib64/libcap.so.2 (0x00007fc67d593000)
			          libc.so.6 => /lib64/libc.so.6 (0x00007fc67d1ce000)
			          libpcre2-8.so.0 => /lib64/libpcre2-8.so.0 (0x00007fc67cf4a000)
			          libdl.so.2 => /lib64/libdl.so.2 (0x00007fc67cd46000)
			          /lib64/ld-linux-x86-64.so.2 (0x00007fc67dbe6000)
			          libpthread.so.0 => /lib64/libpthread.so.0 (0x00007fc67cb26000)
			  
			  ```
- 示例
	- ```bash
	  orrism@localhost ~/repos/berwick-setup (develop) $ ldd /usr/bin/ls
	          linux-vdso.so.1 (0x00007ffd7d0ac000)
	          libselinux.so.1 => /lib64/libselinux.so.1 (0x00007fe371920000)
	          libcap.so.2 => /lib64/libcap.so.2 (0x00007fe37171a000)
	          libc.so.6 => /lib64/libc.so.6 (0x00007fe371355000)
	          libpcre2-8.so.0 => /lib64/libpcre2-8.so.0 (0x00007fe3710d1000)
	          libdl.so.2 => /lib64/libdl.so.2 (0x00007fe370ecd000)
	          /lib64/ld-linux-x86-64.so.2 (0x00007fe371d6d000)
	          libpthread.so.0 => /lib64/libpthread.so.0 (0x00007fe370cad000)
	  ```
	- 通过[[strings]]命令查看文件的动态库依赖
		- ```bash
		  root@slot-120:~ [P1]# strings /usr/bin/gs_cli | grep "lib.*so"
		  /lib/ld.so.1
		  libm.so.6
		  librt.so.1
		  libxml2.so.2
		  libz.so.1
		  libssl.so.1.1
		  libcrypto.so.1.1
		  libpcap.so.1
		  libutil.so.1
		  libpthread.so.0
		  libc.so.6
		  ```
- 参考文档
	- [man ldd](https://man7.org/linux/man-pages/man1/ldd.1.html)
	- [command ld.so.1](https://docs.oracle.com/cd/E86824_01/html/E54763/ld.so.1-1.html)
	- [man ld-linux.so](https://linux.die.net/man/8/ld-linux.so)