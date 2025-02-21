- ^^并发^^可能会造成多个进程访问同一个资源，此时由于多个进程访问同一个资源而产生的问题就是^^竞争^^，在驱动程序的编写时可能会操作共享资源，因此需要考虑并发与竞争
- linux内核可能存在的并发场景
	- 中断程序并发访问
	- 抢占式并发访问
	- 多处理器并发访问
- 多个进程并发访问共享资源是不安全的，因此发生并发时^^保护的目标是共享资源^^
- 处理并发与竞争的方法
	- 原子操作
		- ```c
		  # 原子变量
		  typedef struct {
		  	int counter;
		  } atomic_t;
		  
		  #ifdef CONFIG_64BIT
		  typedef struct {
		  	long counter;
		  } atomic64_t;
		  #endif
		  
		  extern void atomic_set(atomic_t *, int);
		  
		  #define atomic_read(v)          (*(volatile int *)&(v)->counter)
		  
		  #define atomic_add(i, v)	((void)__atomic_add_return( (int)(i), (v)))
		  #define atomic_sub(i, v)	((void)__atomic_add_return(-(int)(i), (v)))
		  #define atomic_inc(v)		((void)__atomic_add_return(        1, (v)))
		  #define atomic_dec(v)		((void)__atomic_add_return(       -1, (v)))
		  
		  # 示例
		  static atomic64_t v = ATOMIC_INIT(1);
		  
		  static init cdev_test_open(struct inode *inode, struct file *file)
		  {
		  	if(!atomic64_dec_and_test(&v)) {
		        atomic64_inc(&v);
		        return -EBUSY;
		      }
		    	......
		  }
		  
		  static init cdev_test_release(struct inode *inode, struct file *file)
		  {
		      atomic64_inc(&v);
		    	......
		  }
		  ```
	- 自旋锁
		- "原地等待"，不断尝试获取自旋锁，直到成功才退出循环
		- ```c
		  typedef struct spinlock {
		  	union {
		  		struct raw_spinlock rlock;
		        	......
		  	};
		  } spinlock_t;
		  
		  spin_lock_init(_lock)
		  void spin_lock(spinlock_t *lock)
		  void spin_unlock(spinlock_t *lock)
		  ```
		- 锁的时间不能太长，即临界区代码必须执行的足够快
		- 使用自旋锁会禁止抢占，自旋锁保护的临界区里不能调用可能会导致线程休眠的函数，否则可能会发生死锁
		- 自旋锁一般用在多核CPU上
	- 信号量
		- 信号量会引起调用者睡眠，在持有锁的时间比较长的情况下考虑使用信号量
		- ```c
		  /* Please don't access any members of this structure directly */
		  struct semaphore {
		  	raw_spinlock_t		lock;
		  	unsigned int		count;
		  	struct list_head	wait_list;
		  };
		  
		  void sema_init(struct semaphore *sem, int val);
		  extern void down(struct semaphore *sem);
		  extern int __must_check down_interruptible(struct semaphore *sem);
		  extern int __must_check down_killable(struct semaphore *sem);
		  extern int __must_check down_trylock(struct semaphore *sem);
		  extern int __must_check down_timeout(struct semaphore *sem, long jiffies);
		  extern void up(struct semaphore *sem);
		  ```
	- 互斥锁
		- 互斥锁会导致休眠，所以在中断中不能使用互斥锁
		- 同一时刻只能有一个线程持有互斥锁，并且只要持有者才可以解锁
		- 不允许递归上锁和解锁
		- ```c
		  struct mutex {
		  	/* 1: unlocked, 0: locked, negative: locked, possible waiters */
		  	atomic_t		count;
		  	spinlock_t		wait_lock;
		  	struct list_head	wait_list;
		    	......
		  };
		  
		  mutex_init(mutex);
		  mutex_lock(lock);
		  void mutex_unlock(struct mutex *lock);
		  int mutex_is_locked(struct mutex *lock);
		  ```
	-