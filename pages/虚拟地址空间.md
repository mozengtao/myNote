- x86-32位虚拟地址空间
	- 用户进程上下文切换用户空间地址会发生改变，但是内核空间地址总是保持不变
	- ![](https://fanlv.fun/2021/07/25/linux-mem/Linux-Memory-X86-32.jpg){:height 506, :width 716}
	- 用户地址空间
		- `0 ~ 3G`
			- 保留区
				- 0x08048000
				- 虚拟地址空间的最低部分，未赋予物理地址，任何对它的引用都是非法的，用于捕获空指针和小整型值指针引用内存的异常情况。它并不是一个单一的内存区域，而是对地址空间中受到操作系统保护而禁止用户进程访问的地址区域的总称。
			- 代码段 text
				- 用于存放程序执行代码(即`CPU`执行的机器指令)，代码段通常属于只读，以防止其他程序意外地修改其指令(对该段的写操作将导致段错误)。某些架构也允许代码段为可写，即允许修改程序。
			- 数据段 data
				- 数据段通常用于存放程序中已初始化且初值不为`0`的全局变量和静态局部变量。数据段属于静态内存分配(静态存储区)，可读可写。数据段保存在目标文件中(在嵌入式系统里一般固化在镜像文件中)，其内容由程序初始化。
					- ```c
					  对于全局变量int gVar = 10
					  必须在目标文件数据段中保存10这个数据，然后在程序加载时复制到相应的内存。
					  ```
				- 数据段与`BSS段`的区别
					- 数据段占用物理文件，也占用内存空间
					- `BSS段`不占用物理文件尺寸，但占用内存空间
					- 当程序读取数据段的数据时，系统会出发缺页故障，从而分配相应的物理内存；当程序读取`BSS段`的数据时，内核会将其转到一个全零页面，不会发生缺页故障，也不会为其分配相应的物理内存
						- ```c
						  对于大型数组如int ar0[10000] = {1, 2, 3, ...}和int ar1[10000]
						  ar1放在BSS段，只记录共有10000*4个字节需要初始化为0
						  ar0则记录每个数据1、2、3...，此时BSS为目标文件所节省的磁盘空间相当可观
						  ```
					- 运行时数据段和`BSS段`的整个区段通常称为数据区。某些资料中“数据段”指代数据段 + `BSS段` + 堆
			- BSS段
				- `BSS(Block Started by Symbol)`段中通常存放程序中以下符号：
					- 未初始化的全局变量和静态局部变量
					- 初始值为`0`的全局变量和静态局部变量(依赖于编译器实现)
					- 未定义且初值不为`0`的符号(该初值即`common block`的大小)
				- 注意，尽管均放置于`BSS段`，但初值为`0`的全局变量是强符号，而未初始化的全局变量是弱符号。
			- 堆 heap
				- 堆用于存放进程运行时动态分配的内存段，可动态扩张或缩减。堆中内容是匿名的，不能按名字直接访问，只能通过指针间接访问
			- 内存映射段 mmap
				- 内核将硬盘文件的内容直接映射到内存, 任何应用程序都可通过`Linux`的`mmap()`系统调用请求这种映射。内存映射是一种方便高效的文件`I/O`方式， 因而被用于装载动态共享库。用户也可创建匿名内存映射，该映射没有对应的文件, 可用于存放程序数据。在`Linux`中，若通过`malloc()`请求一大块内存，`C运行库`将创建一个匿名内存映射，而不使用堆内存。”大块” 意味着比阈值 `MMAP_THRESHOLD`还大，缺省为`128KB`，可通过`mallopt()`调整。
				- malloc是如何分配内存
					- malloc获得虚拟空间的实现与glibc的版本有关
						- 分配内存小于128K，调用sbrk将堆顶指针向高地址移动，获得新的虚拟地址空间
						- 分配内存大于128K，调用mmap，在文件映射区中分配匿名虚拟地址空间
						- 128K是glibc的默认设置，可以通过mallopt进行设置
							- ```c
							  #include <stdlib.h>
							  #include <stdio.h>
							  #include <string.h>
							  #include <unistd.h>
							  #include <sys/mman.h>
							  #include <malloc.h>
							  void print_info(
							          char*      var_name,
							          char*      var_ptr,
							          size_t     size_in_kb
							  )
							  
							  {
							     printf("Address of %s(%luk) 0x%lx,  now heap top is 0x%lx\n",
							      var_name, size_in_kb, var_ptr, sbrk(0));
							  
							  }
							  int main(int argc, char** argv)
							  {
							          char *heap_var1, *heap_var2, *heap_var3 ;
							          char *mmap_var1, *mmap_var2, *mmap_var3 ;
							          char *maybe_mmap_var;
							          printf("Orginal heap top is 0x%lx\n", sbrk(0));
							          heap_var1 = malloc(32*1024);
							          print_info("heap_var1", heap_var1, 32);
							          heap_var2 = malloc(64*1024);
							          print_info("heap_var2", heap_var2, 64);
							          heap_var3 = malloc(127*1024);
							          print_info("heap_var3", heap_var3, 127);
							          printf("\n");
							          maybe_mmap_var = malloc(128*1024);
							          print_info("maybe_mmap_var", maybe_mmap_var, 128);
							          //mmap
							          mmap_var1 = malloc(128*1024);
							          print_info("mmap_var1", mmap_var1, 128);
							          // set M_MMAP_THRESHOLD to 64k
							          mallopt(M_MMAP_THRESHOLD, 64*1024);
							          printf("set M_MMAP_THRESHOLD to 64k\n");
							          mmap_var2 = malloc(64*1024);
							          print_info("mmap_var2", mmap_var2, 64);
							          mmap_var3 = malloc(127*1024);
							          print_info("mmap_var3", mmap_var3, 127);
							          return 1;
							  }
							  ```
					- mallloc实际分配的物理空间大小
						- ```c
						  #include <stdlib.h>
						  #include <stdio.h>
						  #include <string.h>
						  #include <unistd.h>
						  #include <sys/mman.h>
						  #include <malloc.h>
						  char ps_cmd[1024];
						  void print_info(
						          char*      var_name,
						          char*      var_ptr,
						          size_t     size_in_kb
						  )
						  
						  {
						          printf("Address of %s(%luk) 0x%lx,  now heap top is 0x%lx\n",
						                   var_name, size_in_kb, var_ptr, sbrk(0));
						          system(ps_cmd);
						  }
						  
						  int main(int argc, char** argv)
						  {
						          char *non_set_var, *set_1k_var, *set_5k_var, *set_7k_var;
						          pid_t pid;
						          pid = getpid();
						          sprintf(ps_cmd, "ps aux | grep %lu | grep -v grep", pid);
						          non_set_var = malloc(32*1024);
						          print_info("non_set_var", non_set_var, 32);
						          set_1k_var = malloc(64*1024);
						          memset(set_1k_var, 0, 1024);
						          print_info("set_1k_var", set_1k_var, 64);
						          set_5k_var = malloc(127*1024);
						          memset(set_5k_var, 0, 5*1024);
						          print_info("set_5k_var", set_5k_var, 127);
						          set_7k_var = malloc(64*1024);
						          memset(set_1k_var, 0, 7*1024);
						          print_info("set_7k_var", set_7k_var, 64);
						          return 1;
						  }
						  ```
			- 栈 stack
				- 由编译器自动分配释放，行为类似数据结构中的栈(先进后出)
					- 为函数内部声明的非静态局部变量(`C语言`中称“自动变量”)提供存储空间
					- 记录函数调用过程相关的维护性信息，称为栈帧(`Stack Frame`)或过程活动记录(`Procedure Activation Record`)
					- 持续地重用栈空间有助于使活跃的栈内存保持在CPU缓存中，从而加速访问
			- 进程的地址空间分布
				- ```c
				  #include <stdlib.h>
				  #include <stdio.h>
				  #include <string.h>
				  #include <unistd.h>
				  int   global_num = 0;
				  char  global_str_arr [65536] = {'a'};
				  int main(int argc, char** argv)
				  {
				          char* heap_var = NULL;
				          int local_var = 0;
				          printf("Address of function main 0x%lx\n", main);
				          printf("Address of global_num 0x%lx\n", &global_num);
				          printf("Address of global_str_arr 0x%lx ~ 0x%lx\n", &global_str_arr[0], &global_str_arr[65535]);
				          printf("Top of stack is 0x%lx\n", &local_var);
				          printf("Top of heap is 0x%lx\n", sbrk(0));
				          heap_var = malloc(sizeof(char) * 127 * 1024);
				          printf("Address of heap_var is 0x%lx\n", heap_var);
				          printf("Top of heap after malloc is 0x%lx\n", sbrk(0));
				          free(heap_var);
				          heap_var = NULL;
				          printf("Top of heap after free is 0x%lx\n", sbrk(0));
				          return 1;
				  }
				  ```
	- 内核地址空间
		- `3 ~ 4G`
			- 直接映射区 896M
				- 这一块空间是连续的，和物理内存是非常简单的映射关系，其实就是虚拟内存地址减去 `3G`，就得到物理内存的位置
			- 高端内存 HIGH_MEMORY
				- `x86-32`下特有的（`x64下没有这个东西`），因为内核虚拟空间只有`1G`无法管理全部的内存空间
				- 当内核想访问高于`896MB`物理地址内存时，从`0xF8000000 ~ 0xFFFFFFFF`地址空间范围内找一段相应大小空闲的逻辑地址空间，借一段地址空间，建立临时地址映射，用完后释放，达到这段地址空间可以循环使用，访问所有物理内存。
				- VMALLOC_OFFSET
					- 系统会在`low memory`和`VMALLOC`区域留8M，防止访问越界。因此假如理论上`vmalloc size`有`300M`，实际可用的也是只有`292M`
						- ```c
						  include/asm-x86/pgtable_32.h 
						  #define VMALLOC_OFFSET (8*1024*1024)
						  ```
					- 这个缺口可用作针对任何内核故障的保护措施。如果访问越界地址（即无意地访问物理上不存在的内存区），则访问失败并生成一个异常，报告该错误。
				- VMALLOC
					- 虚拟内存中连续、但物理内存中不连续的内存区，可以在`vmalloc`区域分配
					- 该机制通常用于用户过程，内核自身会试图尽力避免非连续的物理地址。
				- 持久映射
					- 内核专门为此留出一块线性空间，从 `PKMAP_BASE` 到 `FIXADDR_START` ，用于映射高端内存
				- 固定映射(临时映射)
					- `FIXADDR_START` 到 `FIXADDR_TOP`(`0xFFFF F000`) 的空间，称为固定映射区域，主要用于满足特殊需求
					- 这块空间具有如下特点：
						- 每个 `CPU` 占用一块空间
						- 在每个 `CPU` 占用的那块空间中，又分为多个小空间，每个小空间大小是 `1` 个 `page`，每个小空间用于一个目的，这些目的定义在 `kmap_types.h` 中的 `km_type` 中
- x86-64位虚拟地址空间
	- ![](https://fanlv.fun/2021/07/25/linux-mem/Linux-Memory-X86-64.jpg)
	- 查看进程虚拟地址空间
		- [[pmap]]
- 参考文档
	- [Linux内核虚拟地址空间](https://fanlv.fun/2021/07/25/linux-mem/)
	- [C内存操作API的实现原理](http://monchickey.com/post/2022/04/07/c-memory-api/)
	- [Linux 虚拟内存管理](https://cloud.tencent.com/developer/article/1004428)
	- [Linux的内存分页管理](https://www.cnblogs.com/vamei/p/9329278.html)
-