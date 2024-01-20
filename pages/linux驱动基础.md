- 驱动
	- “让硬件动起来”
- linux驱动开发
	- 基于linux系统的各种驱动框架进行编程，提供统一的文件操作接口(例如:/dev)给应用程序进行调用
- linux驱动分类
	- 字符设备
		- 以串行的字节流的顺序进行访问的设备
	- 块设备
		- 按照任意顺序进行访问
	- 网络设备
		- 面向网络数据包的发送和接收
- linux内核源码
	- https://www.kernel.org/
	- 给内核开发者或者半导体厂家(NXP, TI, RK)使用的源码仓库
	- 半导体厂家从linux官网获取linux源码，半导体厂家根据自己的cpu基于下载的源码进行适配，适配好的源码会给到下游的开发板或者方案厂商，开发板厂商根据具体开发板进行相关外设的驱动开发，最终形成针对具体开发板的linux源码并提供给用户
- linux驱动程序基本结构
  collapsed:: true
	- ```c
	  #include <linux/module.h>
	  #include <linux/init.h>
	  
	  static int
	  hello_init(void)
	  {
	  	printk("hello world\n");
	  	return 0;
	  }
	  
	  static void
	  hello_exit(void)
	  {
	  	printk("byebye world\n");
	  }
	  
	  module_init(hello_init);
	  module_exit(hello_exit);
	  
	  MODULE_LICENSE("GPL");
	  MODULE_AUTHOR("Morris");
	  MODULE_VERSION("V1.0");
	  ```
- 如何编译linux驱动
  collapsed:: true
	- 将驱动程序放入linux内核源码，统一进行编译
		- ```bash
		  # 图形化配置界面
		  make menuconfig
		  
		  # 相关文件
		  Makefile
		  	定义了编译linux内核源码的具体规则 
		  Kconfig
		  	图形化配置界面的源文件，图形化配置界面中的选项由Kconfig文件决定，当执行make menuconfig时
		      内核配置工具会读取内核源码目录下的arch/$(ARCH)/Kconfig，其中export ARCH=arm64
		  config
		  	config文件位于linux内核源码的arch/$(ARCH)/configs目录下，时linux系统的默认配置文件
		  .config
		  	位于linux内核源码的顶层目录，编译linux内核时会使用.config文件里的配置来编译linux内核镜像
		      如果.config存在，则make menuconfig界面的默认配置即当前.config文件的配置
		      如果.config不存在，则make menuconfig界面的默认配置为Kconfig文件中的默认配置
		      
		  make xxx_defconfig会根据arch/$(ARCH)/configs目录下的默认文件生成.config文件
		  ```
		- 编译过程
			- ```bash
			  mkdir drivers/char/helloworld
			  
			  touch drivers/char/helloworld/Kconfig
			  #文件内容：
			  config helloworld
			  	bool "helloworld support"
			      default y
			      help
			      	helloworld
			  
			  # 用上一级的Kconfig文件包含当前Kconfig文件
			  # drivers/char/Kconfig 添加如下内容
			  source "drivers/char/helloworld/Kconfig"
			  
			  至此，make menuconfig中可以显示helloworld的选项
			  
			  
			  # 添加helloworld.c源文件到drivers/char/helloworld目录
			  cp helloworld.c drivers/char/helloworld
			  touch drivers/char/helloworld/Makefile
			  # Makefile内容如下
			  obj-$(CONFIG_helloworld)	:= helloworld.o
			  
			  # 用上一级的Makefile文件包含当前Makefile文件
			  obj-y	+= helloworld/
			  ```
	- 将驱动编译成内核模块，单独进行编译，独立于linux内核之外
		- linux模块可以动态地进行加载和卸载
		- linux内核模块的编译
			- ```bash
			  # Makefile
			  obj-m += helloworld.o
			  KDIR := /xx/linux-kernel
			  PWD ?= $(shell pwd)
			  all:
			  	make -C $(KDIR) M=$(PWD) modules
			  clean:
			  	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.order
			      
			  # 设置编译环境
			  export ARCH=arm65
			  export CROSS_COMPILE=/xxx/aarch64-linux-gnu-
			  
			  # linux源码的顶层目录下的Makefile控制着整个linux的编译流程，如果其中固定了ARCH和CROSS_COMPILE
			  的值，则在编译内核模块时无须再设置
			  
			  ```
		- linux内核模块的相关命令
			- ```bash
			  insmod
			  modprobe
			  
			  rmmod
			  
			  lsmod
			  
			  modinfo
			  ```
- 驱动传参
	- [Passing Command Line Arguments to a Module](https://tldp.org/LDP/lkmpg/2.6/html/x323.html)
- 内核模块符号导出
	- 目的是解决模块间的访问关系，例如A模块依赖B模块(A模块需要调用B模块中的函数)
	- 内核符号表的导出
		- ```bash
		  EXPORT_SYMBOL(符号名);
		  EXPORT_SYMBOL(符号名); // 只适用于包含GPL许可的模块
		  
		  # 导出的符号可以被其他模块使用，使用前需要进行声明
		  例如：
		  在A模块中导出符号add
		  extern int add(int a, int b);
		  EXPORT_SYMBOL(add);
		  先编译A模块，会生成符号表Module.symvers
		  
		  将A模块生成的符号表拷贝到B模块源文件目录下
		  在B模块的源码中声明A模块中导出的符号原型：
		  extern int add(int a, int b);
		  
		  卸载时需要先卸载B.ko，之后卸载A.ko
		  ```
- [[module_init]]
- 驱动代码如何使用Makefile中的宏定义
  collapsed:: true
	- Makefile中的宏定义
		- 如果把驱动编译进内核则使用KBUILD_CFLAGS_KERNEL中定义的宏
		- 如果把驱动编译成模块则使用KBUILD_CFLAGS_MODULE中定义的宏
	- 可根据需要在模块的Makefile中定义中来开关宏
		- ```bash
		  # 代码中用到的宏定义
		  drivers/md/md.c:#ifndef MODULE
		  
		  # 模块的Makefile中定义宏开关的示例（程序编译为模块）
		  KBUILD_CFLAGS_MODULE += -DDEBUG
		  也可以使用如下定义来控制宏开关
		  EXTRA_CFLAGS += -DDEBUG  (Deprecated)
		  
		  Makefile中实现对应类似c语言中的宏定义 #define DEBUG 3
		  KBUILD_CFLAGS_MODULE += -DDEBUG=3
		  ```
		- 启发：如果某些宏在内核的源码中找不到了，可以从Makefile或者链接脚本中查找宏定义
- 内核是如何执行ko文件的
	- 在编译内核模块时会生成xxx.mod.c，struct module结构体会被链接进ko文件里，在加载ko文件时内核会解析结构体并根据结构体的`mod->init`加载驱动
	- insmod是busybox提供的一个内核模块加载命令，通过执行系统调用init_module实现内核模块的加载工作
	- 可以在内核模块的源码中添加函数查看内核模块加载时的函数调用关系
		- ```c
		  static int
		  hello_init(void)
		  {
		    dump_stack();		// 打印函数调用关系
		    printk("hello world\n");
		  	return 0;
		  }
		  ```
	- [[Internals of insmod and rmmod]]
- 如何添加自定义系统调用
  collapsed:: true
	- ```c
	  # 在内核源码的任意位置添加系统调用服务
	  #include <linux/kernel.h>
	  #include <linux/syscalls.h>
	  
	  SYSDEFINE0(helloworld) {
	    printk("helloworld system call\n");
	    return 0;
	  }
	  
	  # 在unistd.h中添加对应的系统调用号，同时增加最大系统调用号
	  # 编译内核
	  
	  #用户态应用程序通过syscall系统调用进行自定义系统调用的访问
	  ```
- 参考文档
	- [Kconfig Language](https://www.kernel.org/doc/html/latest/kbuild/kconfig-language.html)