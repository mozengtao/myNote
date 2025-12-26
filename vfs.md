- vfs的作用
	- 向上给用户提供统一的"文件"操作接口
	- 向下给底层驱动的实现制定统一的接口框架
	- ![](https://img-blog.csdnimg.cn/20200615094618108.jpg?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMyNzQwMTA3,size_16,color_FFFFFF,t_70)
- vfs的4个主要对象
	- super_block
		- 描述一个已经挂载的文件系统，包含设备名，分区大小，blocksize等基础数据，superblock方法集合，所包含的inode链表等
		- superblock包含的信息众多，与磁盘上的实际数据对应，常作为入参被内外部接口引用，对它的更新会适时同步到磁盘上
		- ```c
		  struct super_block {
		    
		  }
		  ```
	- [[inode]]
		- 描述一个文件(普通文件，目录，FIFO等等)，包含文件类型，大小，更新时间等数据，inode方法集合，其包含的信息同样与磁盘上的数据对应，需要适时同步
		- ```c
		  struct inode {
		    
		  }
		  ```
	- dentry
		- 描述系统目录树中的一个节点，通常会指向一个inode，其主要信息有该dentry的父子dentry，文件名，指向的inode，dentry方法集合等，dentry最主要的作用是记录一个inode在目录树中的位置。dentry结构保存在内存dcache中，在磁盘中往往没有直接关联的数据。
		- dentry直译为目录项，需要与目录进行区分，例如一个路径“/var/log/txt.log”,其中包含4个目录项“/”,"var","log","txt.log"，前3个目录项指向目录inode，最后一个目录项指向普通文件inode
		- dentry实现的方法主要是对dentry结构的初始化，比较，校验，删除等操作。由于dentry通常只维护在内存中，底层文件系统驱动往往不需要实现全部方法，系统有默认处理流程
		- ```c
		  struct dentry {
		    
		  }
		  ```
	- file
		- 描述进程打开的一个文件，在open时被创建，主要包含信息是file方法集合，以及记录当前文件的偏移。对于一个文件来讲，其inode对象是唯一的，而一个文件可以被多个进程打开，其file对象可以有多个
		- ```c
		  struct file {
		    
		  }
		  ```
- vfs对象之间的相互作用
	- 文件系统需要将系统的目录树与磁盘上实际的文件对应起来，并提供访问操作的方法
	- 实例1
		- 假设系统的根文件系统为ext4格式，将一个fat格式的磁盘挂载在`/media/sda`目录，现在需要读写该fat磁盘根目录下的`1.log`文件，即读写`/media/sda/1.log`文件
			- 如何根据路径`/media/sda/1.log`找到该文件在fat磁盘中的inode
				- ![](https://img-blog.csdnimg.cn/20200614083315789.jpg?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMyNzQwMTA3,size_16,color_FFFFFF,t_70)
				- 系统启动后，各文件系统驱动加载完，file_system_type结构被创建，并加入全局链表管理
				- 文件系统挂载完毕后，super_block，mount，根目录dentry结构被创建，并设置好相互关系
				- 执行`open("/media/sda/1.log")`，从系统根目录`/`开始逐项遍历查找dentry
					- 先从cache里查找，若找不到就建立新的dentry并与文件系统上对应文件的inode关联
				- 当遍历到根文件系统的"sda" dentry，发现该dentry是一个挂载点（`dentry->d_flags`标记有DCACHE_MOUNTED）
				- 根据当前的path从mount结构hash list中找到fat磁盘的mount，将下一次查找的dentry改为fat的root dentry，完成在不同文件系统间的跳转，最终找到"1.log" dentry,建立与file的联系，open结束返回
			- 找到目标文件的dentry和inode后，再来看看是如何进行读写的
				- ![](https://img-blog.csdnimg.cn/20200612101927263.jpg?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMyNzQwMTA3,size_16,color_FFFFFF,t_70)
				- 系统调用通过file结构对文件进行读写操作，file中保存了相应文件的inode，以及其在pagecache的映射address_space
				- 读写流程主要通过file_operations，及address_space_operations两套接口完成
					- file_operations是一套较上层的接口，提供给系统调用以及内部模块使用，用于实现各种IO模型以及其他需要进行IO操作的内核功能
					- address_space_operations负责磁盘与内存之间的数据读写，将文件操作转换为磁盘IO下发给block层。内存侧通常是pagecache，若指定了O_DIRECT方式，也可以是应用提供的内存地址
					- 例如对于ext4上一次普通的buffer IO read，首先会将磁盘上的数据读到内存pagecache中，再将pagecache中的数据拷贝到用户buf ，大致路径是：系统调用`read() -> file_operations.read_iter() -> generic_file_buffered_read() -> address_space_operations.readpage() -> copy_page_to_iter()`

[Linux 文件系统解析](https://blog.csdn.net/qq_32740107/article/details/106566454)  
[Linux 文件系统解析2](https://blog.csdn.net/qq_32740107/article/details/106722811?spm=1001.2014.3001.5501)  
[Linux 文件系统解析3](https://blog.csdn.net/qq_32740107/article/details/106867342?spm=1001.2014.3001.5501)  
[Linux 文件系统解析4](https://blog.csdn.net/qq_32740107/article/details/107057066?spm=1001.2014.3001.5501)  
[The Linux Kernel's VFS](http://haifux.org/lectures/119/linux-2.4-vfs/linux-2.4-vfs.html)  
[The "Virtual File System" in Linux](https://www.linux.it/~rubini/docs/vfs/vfs.html)  