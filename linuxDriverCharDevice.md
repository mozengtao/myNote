- 设备号
	- 主设备号表示某一类驱动
	- 次设备号表示某一类驱动下的某个设备
	- ```c
	  设备号：dev_t dev;
	  主设备号：MAJOR(dev)
	  次设备号：MINOR(dev)
	  MKDEV(major, minor)
	  ```
	- 设备号分配
		- 查看系统已分配的设备号
			- ```bsh
			  cat /proc/devices
			  ```
		- 静态分配
			- ```c
			  register_chrdev_region
			  unregister_chrdev_region
			  ```
		- 动态分配
			- ```c
			  alloc_chrdev_region
			  unregister_chrdev_region
			  ```
- 注册字符类设备
	- ```c
	  # 描述字符设备的结构体
	  struct cdev { 
	      struct kobject kobj; 
	      struct module *owner; 
	      const struct file_operations *ops; 
	      struct list_head list; 
	      dev_t dev; 
	      unsigned int count; 
	  };
	  
	  void cdev_init(struct cdev *cdev, const struct file_operations *fops)
	  int cdev_add(struct cdev *p, dev_t dev, unsigned count)
	  void cdev_del(struct cdev *p)
	    
	  struct file_operations {
	  	struct module *owner;
	  	loff_t (*llseek) (struct file *, loff_t, int);
	  	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	  	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	  	__poll_t (*poll) (struct file *, struct poll_table_struct *);
	  	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	  	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	  	int (*mmap) (struct file *, struct vm_area_struct *);
	  	int (*open) (struct inode *, struct file *);
	  	int (*flush) (struct file *, fl_owner_t id);
	  	int (*release) (struct inode *, struct file *);
	  	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	  	int (*fasync) (int, struct file *, int);
	  	int (*lock) (struct file *, int, struct file_lock *);
	  	int (*flock) (struct file *, int, struct file_lock *);
	  				unsigned int poll_flags);
	  	......
	  }
	  ```
- 创建设备节点
	- 手动创建
		- ```bash
		  mknod /dev/test c 236 0
		  mknod 设备路径 设备类型 主设备号 次设备号
		  
		  测试步骤：
		  1.加载字符设备驱动
		  2.根据步骤1加载的字符设备驱动的主设备号和次设备号创建设备节点
		  3.编写应用程序（打开设备节点，操作设备节点（open, read, write ...）则对应驱动的文件操作
		  函数得以执行）
		  ```
	- udev机制
		- udev是用户程序，可根据系统中设备的状态来创建或者删除设备节点，例如当驱动程序成功加载时自动创建设备节点，当驱动程序卸载时自动删除设备节点
	- 注册设备时自动创建
		- 通过mdev机制实现设备节点自动创建与删除(mdev是udev的简化版本,busybox构建根文件系统时会自动创建mdev)
		- ```c
		  struct class *class_create(const char *name);
		  struct device *device_create(const struct class *class, struct device *parent,
		  			     dev_t devt, void *drvdata, const char *fmt, ...);
		  
		  void device_destroy(const struct class *class, dev_t devt);
		  void class_destroy(const struct class *cls);
		  ```
- 用户空间和内核空间的数据交换
	- ```c
	  unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
	  unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
	  
	  # 可用于实现驱动的read, write, ioctl等操作
	  ```
- 文件私有数据
	- 文件私有数据就是在驱动代码中将私有数据private_data指向设备结构体，然后在read,write等函数中通过private_data访问设备结构体，体现了面向对象的思想
	- ```c
	  # 通过自定义设备结构体对设备用到的数据进行封装，在open函数中对private_data进行赋值，之后在
	  其他文件操作函数中进行使用，体现了面型对象的思想，使得驱动代码更加统一，代码更加整洁，便于代码
	  修改和维护
	  struct device_test {
	  	dev_t devnum;
	      int major;
	      int minor;
	      struct cdev cdev_test;
	      struct class *class_test;
	      struct device *device_test;
	  };
	  
	  struct device_test dev1;
	  
	  在open函数中
	  static int cdev_test_open(struct inode *inode, struct file *file)
	  {
	  	file->private_data = &dev1;
	      ......
	  }
	  在read函数中
	  static int cdev_test_read(struct file *file, char __user *buf, size_t size, loff_t *off)
	  {
	  	struct device_test *dev_test = (struct device_test *)file->private_data;
	      dev_test->devnum, dev_test->major, ......
	      ......
	  }
	  
	  # 私有数据的应用
	  一个驱动兼容多个设备的实现，等等应用场景
	  ```
- 杂项设备
	- 可以把无法归类的五花八门的设备归结为杂项设备，相对字符设备，^^杂项设备主设备号固定为10^^，并且杂项设备会自己调用class_create和device_create来自动创建设备节点，无需在驱动代码中显示进行创建
	- ```c
	  #杂项设备结构体
	  struct miscdevice  {
	  	int minor;
	  	const char *name;
	  	const struct file_operations *fops;
	  	struct list_head list;
	  	struct device *parent;
	  	struct device *this_device;
	  	const struct attribute_group **groups;
	  	const char *nodename;
	  	umode_t mode;
	  };
	  
	  extern int misc_register(struct miscdevice *misc);
	  extern void misc_deregister(struct miscdevice *misc);
	  
	  ```
- linux驱动代码中的错误处理
	- 使用goto语句，遵循“先进后出”的原则 统一对错误进行处理
		- ```c
		  if(err1)
		    goto err1_handle;
		  if(err2)
		    goto err2_handle;
		  
		  err2_handle:
		  	xxx
		  err1_handle:
		  	xxx
		  ```
	- 内核中保留了一段地址用来记录错误码，这段地址和错误码是一一对应的
		- 内核中的函数常常返回指针，如果内核函数返回指针，如何判断指针是有效地址还是错误码呢
			- 使用IS_ERR函数去检查函数的返回值
			- ```c
			  if(IS_ERR(XXX))
			    	ret = PTR_ERR(XXX)
			  ```