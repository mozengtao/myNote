- linux源码编译过程
  collapsed:: true
	- 下载指定版本的内核源码
	- 下载指定版本的编译工具链
	- 安装必要的工具
		- ```bash
		  sudo apt install make gcc flex bison
		  sudo apt install libncurses5-dev libpixman-1-dev
		  sudo apt install zlib1g-dev
		  sudo apt install libglib2.0-0
		  sudo apt install libglib2.0-dev
		  sudo apt install build-essential libgmp3-dev libmpc-dev bc
		  ```
	- 编译源码
		- ```bash
		  export PATH=$PATH:/opt/gcc-arm-11.2-2022.02-x86_64-arm-none-eabi/bin
		  # 查看gcc版本
		  arm-none-eabi-gcc -v
		  
		  export CROSS_COMPILE=arm-none-eabi-
		  export ARCH=arm
		  make O=out vexpress_defconfig -j4
		  make O=out -j4
		  ```
- ARM 架构的不同交叉编译工具链的差异
  collapsed:: true
	- ```bash
	  arm-none-eabi
	  用于嵌入式系统开发的工具链,产生的可执行文件（二进制文件）没有对应的操作系统依赖，
	  适用于裸机开发或者基于实时操作系统（RTOS）的应用程序开发
	  
	  arm-none-linux-gnueabihf
	  用于 Linux 系统开发的硬浮点（hard-float）工具链,硬浮点指令是直接使用处理器的硬件浮点寄存器进行计算，
	  性能更高。这个工具链生成的可执行文件需要与 Linux 操作系统配合使用
	  
	  arm-none-linux-gnueabi
	  用于 Linux 系统开发的工具链，但使用软浮点（soft-float）计算，即通过函数调用来模拟浮点运算，性能较硬浮
	  点略低。这个工具链生成的可执行文件同样需要与 Linux 操作系统配合使用
	  ```
- 不同内核镜像的差异
	- ```bash
	  Image是最早的 Linux 内核镜像格式，它是一个非压缩的镜像文件。该镜像适用于一些较旧的硬件平台或
	  嵌入式设备，这些设备可能没有足够的内存来加载压缩的内核镜像
	  
	  zImage 是对内核进行了轻量级压缩的镜像文件。这个压缩的格式使得内核文件更小，但仍然需要解压缩才
	  能加载到内存中。大多数基于 x86 架构的计算机和一些 ARM 平台都支持加载 zImage
	  
	  bzImage（Big zImage）是对内核进行了更强大的压缩的镜像文件。这个压缩格式是使用 GNU 压缩工具 
	  gzip（或其变种 bzip2）进行的，可以进一步减小内核文件的大小。与 zImage 相比，bzImage 的压缩
	  更为高效。大多数现代系统都支持加载 bzImage
	  ```
- 参考文档
	- [编译kernel](https://five2hill.gitee.io/ebook/doc/linux-build.html)
	- [linux内核源码阿里云下载](https://mirrors.aliyun.com/linux-kernel/) #sourcecode
	- [编译器等工具链下载](https://mirrors.aliyun.com/armbian-releases/_toolchain/)
	- [Building a Custom Linux Kernel & Debugging via QEMU + GDB](https://www.josehu.com/memo/2021/01/02/linux-kernel-build-debug.html)
	- [Kernel debug with GDB and QEMU](https://mhcerri.github.io/posts/kernel-debug-with-gdb-and-qemu/)
	- [Booting a Custom Linux Kernel in QEMU and Debugging It With GDB](http://nickdesaulniers.github.io/blog/2018/10/24/booting-a-custom-linux-kernel-in-qemu-and-debugging-it-with-gdb/)
	- [Debugging Linux kernels with Qemu and GDB](https://www.sobyte.net/post/2022-02/debug-linux-kernel-with-qemu-and-gdb/)