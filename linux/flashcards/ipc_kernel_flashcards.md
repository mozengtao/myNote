# IPC (Inter-Process Communication) Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel IPC mechanisms, data structures, and APIs
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. IPC Overview (IPC概述)

---

Q: What are the main IPC mechanisms in Linux?
A: Linux提供多种进程间通信机制：

```
+------------------------------------------------------------------+
|                    Linux IPC Mechanisms                           |
+------------------------------------------------------------------+
|                                                                  |
|  +-----------------+  +-----------------+  +-----------------+   |
|  |     Pipes       |  |     Signals     |  |  Shared Memory  |   |
|  | - Anonymous     |  | - kill/signal   |  | - mmap          |   |
|  | - Named (FIFO)  |  | - sigaction     |  | - shmget/shmat  |   |
|  +-----------------+  +-----------------+  +-----------------+   |
|                                                                  |
|  +-----------------+  +-----------------+  +-----------------+   |
|  | Message Queues  |  |   Semaphores    |  |  Unix Sockets   |   |
|  | - System V      |  | - System V      |  | - SOCK_STREAM   |   |
|  | - POSIX         |  | - POSIX         |  | - SOCK_DGRAM    |   |
|  +-----------------+  +-----------------+  +-----------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

| 机制 | 特点 | 适用场景 |
|------|------|----------|
| 管道 | 单向、亲缘进程 | 简单数据流 |
| 命名管道 | 单向、任意进程 | 独立进程通信 |
| 信号 | 异步通知 | 事件通知 |
| 共享内存 | 最快、需同步 | 大量数据共享 |
| 消息队列 | 有界缓冲 | 结构化消息 |
| 信号量 | 同步原语 | 资源同步 |
| Unix套接字 | 双向、灵活 | 本地网络式通信 |
[Basic]

---

Q: What is the difference between System V IPC and POSIX IPC?
A: 
| 特性 | System V IPC | POSIX IPC |
|------|-------------|-----------|
| **API风格** | xxxget/xxxctl/xxxop | xxx_open/xxx_close |
| **命名** | 数字key | 字符串名称 (/name) |
| **头文件** | sys/ipc.h, sys/shm.h等 | mqueue.h, semaphore.h等 |
| **持久性** | 内核持久化 | 文件系统持久化 |
| **引用计数** | 无自动清理 | 有引用计数 |
| **接口一致性** | 不一致 | 统一一致 |
| **内核支持** | 完整 | 部分需要用户空间库 |

```c
// System V 共享内存
int shmid = shmget(key, size, IPC_CREAT | 0666);
void *ptr = shmat(shmid, NULL, 0);

// POSIX 共享内存
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, size);
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
```
[Basic]

---

## 2. Pipes (管道)

---

Q: What is a pipe and how is it implemented in the kernel?
A: 管道是最简单的IPC机制，内核实现为环形缓冲区：

```
写进程                                    读进程
   |                                        ^
   v                                        |
+--+----------------------------------------+--+
|  |  Ring Buffer (typically 64KB)          |  |
|  | +----+----+----+----+----+----+----+   |  |
|  | |data|data|data|    |    |    |    |   |  |
|  | +----+----+----+----+----+----+----+   |  |
|  |      ^              ^                  |  |
|  |      |              |                  |  |
|  |    read_pos       write_pos            |  |
+--+----------------------------------------+--+
   pipe_write()                     pipe_read()
```

内核数据结构：
```c
// fs/pipe.c
struct pipe_inode_info {
    struct mutex mutex;              // 保护管道
    wait_queue_head_t rd_wait;       // 读等待队列
    wait_queue_head_t wr_wait;       // 写等待队列
    unsigned int head;               // 写位置
    unsigned int tail;               // 读位置
    unsigned int max_usage;          // 最大使用量
    unsigned int ring_size;          // 环大小
    unsigned int nr_accounted;
    unsigned int readers;            // 读者数
    unsigned int writers;            // 写者数
    unsigned int files;              // 打开的文件数
    unsigned int r_counter;
    unsigned int w_counter;
    struct page *bufs[PIPE_DEF_BUFFERS]; // 缓冲区页面
    struct pipe_buffer *bufs;        // 缓冲区数组
};

struct pipe_buffer {
    struct page *page;               // 数据页
    unsigned int offset;             // 页内偏移
    unsigned int len;                // 数据长度
    const struct pipe_buf_operations *ops;
    unsigned int flags;
};
```
[Intermediate]

---

Q: How does the pipe() system call work?
A: 
```c
// 用户空间
int pipefd[2];
pipe(pipefd);  // pipefd[0]=读端, pipefd[1]=写端

// 内核实现 (fs/pipe.c)
SYSCALL_DEFINE1(pipe, int __user *, fildes)
{
    return do_pipe2(fildes, 0);
}

static int do_pipe2(int __user *fildes, int flags)
{
    struct file *files[2];
    int fd[2];
    int error;
    
    // 1. 创建管道inode和pipe_inode_info
    error = __do_pipe_flags(fd, files, flags);
    if (error)
        return error;
    
    // 2. 复制fd到用户空间
    if (copy_to_user(fildes, fd, sizeof(fd))) {
        // 清理
        return -EFAULT;
    }
    
    // 3. 安装fd
    fd_install(fd[0], files[0]);
    fd_install(fd[1], files[1]);
    
    return 0;
}

// 管道file_operations
const struct file_operations pipefifo_fops = {
    .open           = fifo_open,
    .llseek         = no_llseek,
    .read_iter      = pipe_read,
    .write_iter     = pipe_write,
    .poll           = pipe_poll,
    .unlocked_ioctl = pipe_ioctl,
    .release        = pipe_release,
    .fasync         = pipe_fasync,
    .splice_write   = iter_file_splice_write,
    .splice_read    = pipe_splice_read,
};
```
[Intermediate]

---

Q: How does pipe read and write blocking work?
A: 
```c
// 读阻塞条件：管道为空且有写者
static ssize_t pipe_read(struct kiocb *iocb, struct iov_iter *to)
{
    struct pipe_inode_info *pipe = ...;
    
    __pipe_lock(pipe);
    
    for (;;) {
        // 检查是否有数据
        unsigned int head = pipe->head;
        unsigned int tail = pipe->tail;
        
        if (head != tail) {
            // 有数据，读取
            break;
        }
        
        // 没有数据
        if (!pipe->writers) {
            // 没有写者，返回EOF
            ret = 0;
            break;
        }
        
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            break;
        }
        
        // 阻塞等待数据
        __pipe_unlock(pipe);
        
        if (wait_event_interruptible(pipe->rd_wait, 
                                     pipe_readable(pipe)))
            return -ERESTARTSYS;
        
        __pipe_lock(pipe);
    }
    
    // 读取数据后唤醒等待的写者
    wake_up_interruptible(&pipe->wr_wait);
    
    __pipe_unlock(pipe);
    return ret;
}

// 写阻塞条件：管道满且有读者
// 类似逻辑，满时等待rd_wait，写后唤醒wr_wait
```
[Intermediate]

---

Q: What is a named pipe (FIFO) and how does it differ from anonymous pipe?
A: 
| 特性 | 匿名管道 | 命名管道(FIFO) |
|------|----------|----------------|
| 创建 | pipe() | mkfifo() 或 mknod() |
| 进程关系 | 需要亲缘关系 | 任意进程 |
| 文件系统 | 无对应文件 | 有对应文件节点 |
| 生命周期 | 随进程结束 | 持久化直到删除 |

```c
// 创建命名管道
mkfifo("/tmp/myfifo", 0666);

// 内核实现
// fs/pipe.c
static int fifo_open(struct inode *inode, struct file *filp)
{
    struct pipe_inode_info *pipe;
    bool is_pipe = inode->i_sb->s_magic == PIPEFS_MAGIC;
    int ret;
    
    // FIFO特殊处理：等待另一端打开
    if (filp->f_mode & FMODE_READ) {
        // 读端：等待写端打开（除非O_NONBLOCK）
        pipe->readers++;
        if (!pipe->writers) {
            if (!(filp->f_flags & O_NONBLOCK)) {
                // 阻塞等待写端
                wait_for_partner(pipe, &pipe->w_counter);
            }
        }
    }
    
    if (filp->f_mode & FMODE_WRITE) {
        // 写端：等待读端打开
        pipe->writers++;
        if (!pipe->readers) {
            if (filp->f_flags & O_NONBLOCK) {
                // 非阻塞模式，无读者返回错误
                return -ENXIO;
            }
            wait_for_partner(pipe, &pipe->r_counter);
        }
    }
    
    return 0;
}
```
[Intermediate]

---

## 3. Signals (信号)

---

Q: What are signals and how are they represented in the kernel?
A: 信号是异步通知机制：

```c
// 信号集表示 (include/linux/signal.h)
typedef struct {
    unsigned long sig[_NSIG_WORDS];  // 位图，每bit代表一个信号
} sigset_t;

// 进程的信号信息 (include/linux/sched/signal.h)
struct signal_struct {
    atomic_t sigcnt;
    atomic_t live;
    
    wait_queue_head_t wait_chldexit;  // 等待子进程
    
    struct task_struct *curr_target;   // 当前信号目标
    
    struct sigpending shared_pending;  // 共享挂起信号
    
    int group_exit_code;
    int group_stop_count;
    unsigned int flags;
    
    // ...
};

// 挂起的信号
struct sigpending {
    struct list_head list;    // sigqueue链表
    sigset_t signal;          // 挂起信号位图
};

// 信号队列项
struct sigqueue {
    struct list_head list;
    int flags;
    siginfo_t info;           // 信号详细信息
    struct user_struct *user;
};

// task_struct中的信号字段
struct task_struct {
    // ...
    struct signal_struct *signal;
    struct sighand_struct *sighand;  // 信号处理程序
    
    sigset_t blocked;                // 阻塞的信号
    sigset_t real_blocked;
    struct sigpending pending;       // 私有挂起信号
    
    unsigned long sas_ss_sp;         // 备用信号栈
    size_t sas_ss_size;
    // ...
};
```
[Intermediate]

---

Q: How does the kernel deliver a signal to a process?
A: 信号发送和递送流程：
```
发送信号:
kill(pid, sig) / raise(sig) / kill()
         |
         v
    sys_kill()
         |
         v
    kill_something_info()
         |
         v
    __send_signal()
         |
         +---> 分配sigqueue
         +---> 加入pending链表
         +---> 设置signal位图
         +---> signal_wake_up() 唤醒进程
         
递送信号:
    进程返回用户态前
         |
         v
    exit_to_user_mode_prepare()
         |
         v
    do_signal()
         |
         v
    get_signal() - 获取待处理信号
         |
         +---> 检查信号处理方式
         |
         +---> SIG_DFL: 执行默认动作
         |     (终止/停止/忽略/core dump)
         |
         +---> SIG_IGN: 忽略
         |
         +---> handler: 设置用户态栈帧
                  |
                  v
             setup_rt_frame()
                  |
                  v
             跳转到用户态handler执行
                  |
                  v
             sigreturn系统调用返回
```
[Intermediate]

---

Q: How to implement a signal handler in user space?
A: 
```c
#include <signal.h>
#include <stdio.h>

// 信号处理函数
void handler(int sig, siginfo_t *info, void *ucontext)
{
    printf("Received signal %d from pid %d\n", sig, info->si_pid);
}

int main()
{
    struct sigaction sa;
    
    // 设置处理函数
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    
    // 安装信号处理程序
    sigaction(SIGUSR1, &sa, NULL);
    
    // 等待信号
    pause();
    
    return 0;
}

// 内核中sigaction的实现
SYSCALL_DEFINE4(rt_sigaction, int, sig,
                const struct sigaction __user *, act,
                struct sigaction __user *, oact,
                size_t, sigsetsize)
{
    struct k_sigaction new_sa, old_sa;
    
    if (act) {
        if (copy_from_user(&new_sa.sa, act, sizeof(new_sa.sa)))
            return -EFAULT;
    }
    
    ret = do_sigaction(sig, act ? &new_sa : NULL, oact ? &old_sa : NULL);
    
    if (!ret && oact) {
        if (copy_to_user(oact, &old_sa.sa, sizeof(old_sa.sa)))
            return -EFAULT;
    }
    
    return ret;
}
```
[Basic]

---

Q: What are real-time signals and how do they differ from standard signals?
A: 
| 特性 | 标准信号 (1-31) | 实时信号 (SIGRTMIN-SIGRTMAX) |
|------|-----------------|------------------------------|
| 排队 | 不排队（合并） | 排队（每个都递送） |
| 顺序 | 不保证 | 按发送顺序递送 |
| 附加数据 | 无 | 可通过sigqueue发送数据 |
| 默认动作 | 各不相同 | 终止进程 |
| 优先级 | 无 | 编号小的优先级高 |

```c
// 发送实时信号带数据
union sigval value;
value.sival_int = 42;
sigqueue(pid, SIGRTMIN, value);

// 接收实时信号数据
void handler(int sig, siginfo_t *info, void *ctx)
{
    printf("Received value: %d\n", info->si_value.sival_int);
}

// 内核处理
static int __send_signal(int sig, struct siginfo *info,
                         struct task_struct *t, ...)
{
    struct sigqueue *q;
    
    // 标准信号：检查是否已挂起
    if (sig < SIGRTMIN) {
        if (sigismember(&pending->signal, sig))
            goto out_set;  // 已挂起，不重复添加
    }
    
    // 分配sigqueue
    q = __sigqueue_alloc(sig, t, GFP_ATOMIC, override_rlimit);
    if (q) {
        list_add_tail(&q->list, &pending->list);
        copy_siginfo(&q->info, info);
    }
    
out_set:
    sigaddset(&pending->signal, sig);
    return 0;
}
```
[Intermediate]

---

## 4. System V Shared Memory (System V共享内存)

---

Q: What is System V shared memory and its kernel implementation?
A: System V共享内存允许多个进程共享同一块物理内存：

```c
// 内核数据结构 (ipc/shm.c)
struct shmid_kernel {
    struct kern_ipc_perm shm_perm;    // 权限
    struct file *shm_file;            // 关联的文件（tmpfs）
    unsigned long shm_nattch;         // 连接数
    unsigned long shm_segsz;          // 段大小
    time64_t shm_atim;               // 最后attach时间
    time64_t shm_dtim;               // 最后detach时间
    time64_t shm_ctim;               // 最后修改时间
    struct pid *shm_cprid;           // 创建者pid
    struct pid *shm_lprid;           // 最后操作者pid
    struct user_struct *mlock_user;
    
    struct task_struct *shm_creator;
    struct list_head shm_clist;
};

// IPC命名空间
struct ipc_namespace {
    struct ipc_ids ids[3];  // 信号量、消息队列、共享内存
    // ...
    unsigned int shm_ctlmax;  // 单个段最大值
    unsigned int shm_ctlall;  // 系统总量限制
    unsigned int shm_ctlmni;  // 最大段数
    // ...
};
```

使用流程：
```c
// 1. 创建/获取共享内存
int shmid = shmget(key, size, IPC_CREAT | 0666);

// 2. 连接到进程地址空间
void *ptr = shmat(shmid, NULL, 0);

// 3. 使用共享内存
strcpy(ptr, "Hello, shared memory!");

// 4. 断开连接
shmdt(ptr);

// 5. 删除（当引用计数为0时实际删除）
shmctl(shmid, IPC_RMID, NULL);
```
[Intermediate]

---

Q: How does shmget() work in the kernel?
A: 
```c
SYSCALL_DEFINE3(shmget, key_t, key, size_t, size, int, shmflg)
{
    struct ipc_namespace *ns = current->nsproxy->ipc_ns;
    
    return ksys_shmget(ns, key, size, shmflg);
}

static long ksys_shmget(struct ipc_namespace *ns, key_t key,
                        size_t size, int shmflg)
{
    struct ipc_params shm_params;
    
    shm_params.key = key;
    shm_params.flg = shmflg;
    shm_params.u.size = size;
    
    return ipcget(ns, &shm_ids(ns), &shm_ops, &shm_params);
}

// 创建新的共享内存段
static int newseg(struct ipc_namespace *ns, struct ipc_params *params)
{
    struct shmid_kernel *shp;
    struct file *file;
    
    // 1. 分配shmid_kernel结构
    shp = kvmalloc(sizeof(*shp), GFP_KERNEL);
    
    // 2. 在tmpfs中创建文件作为后端
    file = shmem_kernel_file_setup("SYSV", size, ...);
    
    // 3. 初始化结构
    shp->shm_perm.key = key;
    shp->shm_segsz = size;
    shp->shm_file = file;
    shp->shm_nattch = 0;
    
    // 4. 添加到IPC ID管理
    shp->shm_perm.id = ipc_addid(&shm_ids(ns), &shp->shm_perm, ...);
    
    return shp->shm_perm.id;
}
```
[Advanced]

---

Q: How does shmat() map shared memory to process address space?
A: 
```c
SYSCALL_DEFINE3(shmat, int, shmid, char __user *, shmaddr, int, shmflg)
{
    unsigned long ret;
    long err;
    
    err = do_shmat(shmid, shmaddr, shmflg, &ret, SHMLBA);
    if (err)
        return err;
    
    // 返回映射地址
    force_successful_syscall_return();
    return (long)ret;
}

long do_shmat(int shmid, char __user *shmaddr, int shmflg,
              ulong *raddr, unsigned long shmlba)
{
    struct shmid_kernel *shp;
    struct file *file;
    unsigned long addr;
    unsigned long prot, flags;
    
    // 1. 查找共享内存段
    shp = shm_obtain_object_check(ns, shmid);
    
    // 2. 权限检查
    if (ipcperms(ns, &shp->shm_perm, acc_mode))
        return -EACCES;
    
    // 3. 获取文件引用
    file = shp->shm_file;
    get_file(file);
    shp->shm_nattch++;
    
    // 4. 确定映射地址
    if (shmaddr) {
        addr = (unsigned long)shmaddr;
        // 地址对齐检查
    } else {
        addr = 0;  // 让内核选择
    }
    
    // 5. 建立内存映射
    addr = do_mmap(file, addr, size, prot, flags, 0, &populate, NULL);
    
    *raddr = addr;
    return 0;
}
```

地址空间布局：
```
+------------------+ 高地址
|      Stack       |
+------------------+
|        |         |
|        v         |
+------------------+
|   Shared Memory  | <-- shmat()映射到这里
|   (SHM segment)  |
+------------------+
|        ^         |
|        |         |
+------------------+
|       Heap       |
+------------------+
|       BSS        |
+------------------+
|      Data        |
+------------------+
|      Text        |
+------------------+ 低地址
```
[Advanced]

---

## 5. System V Message Queues (System V消息队列)

---

Q: What is a message queue and its kernel implementation?
A: 消息队列允许进程发送和接收带类型的消息：

```c
// 内核数据结构 (ipc/msg.c)
struct msg_queue {
    struct kern_ipc_perm q_perm;     // 权限
    time64_t q_stime;               // 最后发送时间
    time64_t q_rtime;               // 最后接收时间
    time64_t q_ctime;               // 最后修改时间
    unsigned long q_cbytes;          // 队列当前字节数
    unsigned long q_qnum;            // 队列消息数
    unsigned long q_qbytes;          // 队列最大字节数
    struct pid *q_lspid;            // 最后发送者pid
    struct pid *q_lrpid;            // 最后接收者pid
    
    struct list_head q_messages;     // 消息链表
    struct list_head q_receivers;    // 等待接收的进程
    struct list_head q_senders;      // 等待发送的进程
};

// 消息结构
struct msg_msg {
    struct list_head m_list;
    long m_type;                     // 消息类型
    size_t m_ts;                     // 消息大小
    struct msg_msgseg *next;         // 分段消息
    void *security;
    /* 消息数据紧跟在结构体后面 */
};
```

使用示例：
```c
// 消息结构
struct msgbuf {
    long mtype;       // 消息类型（>0）
    char mtext[256];  // 消息内容
};

// 创建/获取消息队列
int msgid = msgget(key, IPC_CREAT | 0666);

// 发送消息
struct msgbuf msg = {.mtype = 1, .mtext = "Hello"};
msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0);

// 接收消息
struct msgbuf rcv;
msgrcv(msgid, &rcv, sizeof(rcv.mtext), 1, 0);
// msgtype=0: 接收任意类型
// msgtype>0: 接收指定类型
// msgtype<0: 接收类型<=|msgtype|的最小类型消息
```
[Intermediate]

---

Q: How does msgsnd() work in the kernel?
A: 
```c
SYSCALL_DEFINE4(msgsnd, int, msqid, struct msgbuf __user *, msgp,
                size_t, msgsz, int, msgflg)
{
    return do_msgsnd(msqid, msgp->mtype, msgp->mtext, msgsz, msgflg);
}

static int do_msgsnd(int msqid, long mtype, void __user *mtext,
                     size_t msgsz, int msgflg)
{
    struct msg_queue *msq;
    struct msg_msg *msg;
    
    // 1. 验证消息类型
    if (mtype < 1)
        return -EINVAL;
    
    // 2. 分配消息结构并复制数据
    msg = load_msg(mtext, msgsz);
    if (IS_ERR(msg))
        return PTR_ERR(msg);
    msg->m_type = mtype;
    msg->m_ts = msgsz;
    
    // 3. 查找消息队列
    msq = msq_obtain_object_check(ns, msqid);
    
    for (;;) {
        // 4. 检查队列是否有空间
        if (msq->q_cbytes + msgsz > msq->q_qbytes) {
            if (msgflg & IPC_NOWAIT) {
                free_msg(msg);
                return -EAGAIN;
            }
            
            // 阻塞等待
            prepare_to_wait(&msq->q_senders, ...);
            ipc_unlock_object(&msq->q_perm);
            schedule();
            continue;
        }
        
        // 5. 添加消息到队列
        list_add_tail(&msg->m_list, &msq->q_messages);
        msq->q_cbytes += msgsz;
        msq->q_qnum++;
        
        // 6. 唤醒等待的接收者
        ss_wakeup(&msq->q_receivers, ...);
        
        break;
    }
    
    return 0;
}
```
[Advanced]

---

## 6. System V Semaphores (System V信号量)

---

Q: What is a System V semaphore set?
A: System V信号量是一组信号量的集合，用于进程同步：

```c
// 内核数据结构 (ipc/sem.c)
struct sem_array {
    struct kern_ipc_perm sem_perm;   // 权限
    time64_t sem_ctime;             // 最后修改时间
    time64_t sem_otime;             // 最后操作时间
    struct list_head pending_alter;  // 等待修改的进程
    struct list_head pending_const;  // 等待不修改的进程
    struct list_head list_id;
    int sem_nsems;                   // 信号量数量
    int complex_count;               // 复杂操作计数
    unsigned int use_global_lock;
    struct sem sems[];               // 信号量数组（柔性数组）
};

// 单个信号量
struct sem {
    int semval;                      // 信号量值
    struct pid *sempid;              // 最后操作的pid
    spinlock_t lock;
    struct list_head pending_alter;
    struct list_head pending_const;
    time64_t sem_otime;
};

// 信号量操作
struct sembuf {
    unsigned short sem_num;          // 信号量索引
    short sem_op;                    // 操作值
    short sem_flg;                   // 标志 (IPC_NOWAIT, SEM_UNDO)
};
```

使用示例：
```c
// 创建信号量集（3个信号量）
int semid = semget(key, 3, IPC_CREAT | 0666);

// 初始化
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;
arg.val = 1;
semctl(semid, 0, SETVAL, arg);

// P操作（等待/减少）
struct sembuf op = {.sem_num = 0, .sem_op = -1, .sem_flg = 0};
semop(semid, &op, 1);

// V操作（释放/增加）
struct sembuf op = {.sem_num = 0, .sem_op = 1, .sem_flg = 0};
semop(semid, &op, 1);
```
[Intermediate]

---

Q: How does semop() implement atomic operations?
A: 
```c
SYSCALL_DEFINE3(semop, int, semid, struct sembuf __user *, tsops,
                unsigned, nsops)
{
    return do_semtimedop(semid, tsops, nsops, NULL);
}

static int do_semtimedop(int semid, struct sembuf __user *tsops,
                         unsigned nsops, const struct timespec64 *timeout)
{
    struct sem_array *sma;
    struct sembuf *sops;
    int error;
    
    // 1. 复制操作数组
    sops = kvmalloc_array(nsops, sizeof(*sops), GFP_KERNEL);
    if (copy_from_user(sops, tsops, nsops * sizeof(*sops)))
        return -EFAULT;
    
    // 2. 获取信号量数组
    sma = sem_obtain_object_check(ns, semid);
    
    // 3. 尝试执行操作
    error = perform_atomic_semop(sma, sops, nsops, ...);
    
    if (error == 0) {
        // 操作成功
        goto out_unlock;
    }
    
    if (error > 0) {
        // 需要阻塞等待
        struct sem_queue queue;
        
        queue.sops = sops;
        queue.nsops = nsops;
        queue.undo = un;
        queue.pid = task_tgid(current);
        queue.status = -EINTR;
        
        // 加入等待队列
        append_to_queue(sma, &queue);
        
        // 等待
        do {
            error = schedule_timeout(timeout);
            // 检查是否被唤醒
        } while (queue.status == -EINTR && !signal_pending(current));
        
        error = queue.status;
    }
    
out_unlock:
    // 唤醒其他可能可以执行的等待者
    update_queue(sma, ...);
    
    return error;
}

// 原子执行信号量操作
static int perform_atomic_semop(struct sem_array *sma,
                                struct sembuf *sops, int nsops, ...)
{
    int result = 0;
    
    for (int i = 0; i < nsops; i++) {
        struct sem *curr = &sma->sems[sops[i].sem_num];
        int sem_op = sops[i].sem_op;
        
        if (sem_op < 0) {
            // P操作：检查是否足够
            if (curr->semval < -sem_op) {
                result = 1;  // 需要等待
                break;
            }
        } else if (sem_op == 0) {
            // 等待变为0
            if (curr->semval != 0) {
                result = 1;
                break;
            }
        }
    }
    
    if (result == 0) {
        // 所有检查通过，执行操作
        for (int i = 0; i < nsops; i++) {
            struct sem *curr = &sma->sems[sops[i].sem_num];
            curr->semval += sops[i].sem_op;
        }
    }
    
    return result;
}
```
[Advanced]

---

## 7. POSIX Shared Memory (POSIX共享内存)

---

Q: How does POSIX shared memory work?
A: POSIX共享内存通过tmpfs文件实现：

```c
// 用户空间使用
#include <sys/mman.h>
#include <fcntl.h>

// 创建/打开
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);

// 设置大小
ftruncate(fd, 4096);

// 映射
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

// 使用
strcpy(ptr, "Hello, POSIX shm!");

// 解除映射
munmap(ptr, 4096);

// 关闭
close(fd);

// 删除
shm_unlink("/myshm");
```

内核实现（glibc + tmpfs）：
```c
// glibc中shm_open的实现
int shm_open(const char *name, int oflag, mode_t mode)
{
    // 将/name转换为/dev/shm/name
    char *path = "/dev/shm" + name;
    
    // 调用普通的open
    return open(path, oflag, mode);
}

// /dev/shm是tmpfs挂载点
// mount -t tmpfs tmpfs /dev/shm

// 所以POSIX共享内存实际上是：
// 1. 在tmpfs中创建文件
// 2. 使用mmap映射该文件
// 3. 文件内容存储在内存中
```
[Intermediate]

---

## 8. POSIX Message Queues (POSIX消息队列)

---

Q: How do POSIX message queues differ from System V?
A: 
```c
// POSIX消息队列通过特殊文件系统mqueue实现
// mount -t mqueue none /dev/mqueue

// 用户空间使用
#include <mqueue.h>

// 创建/打开
struct mq_attr attr = {
    .mq_flags = 0,
    .mq_maxmsg = 10,      // 最大消息数
    .mq_msgsize = 256,    // 最大消息大小
    .mq_curmsgs = 0
};
mqd_t mq = mq_open("/myqueue", O_CREAT | O_RDWR, 0666, &attr);

// 发送（带优先级）
mq_send(mq, "Hello", 6, 1);  // 优先级1

// 接收（返回优先级最高的）
char buf[256];
unsigned int prio;
mq_receive(mq, buf, sizeof(buf), &prio);

// 定时接收
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 5;  // 5秒超时
mq_timedreceive(mq, buf, sizeof(buf), &prio, &ts);

// 异步通知
struct sigevent sev = {
    .sigev_notify = SIGEV_SIGNAL,
    .sigev_signo = SIGUSR1
};
mq_notify(mq, &sev);

// 关闭和删除
mq_close(mq);
mq_unlink("/myqueue");
```

内核实现（ipc/mqueue.c）：
```c
// 消息队列inode信息
struct mqueue_inode_info {
    spinlock_t lock;
    struct inode vfs_inode;
    
    wait_queue_head_t wait_q;
    
    struct msg_msg **msg;           // 消息数组（按优先级组织）
    unsigned long qsize;            // 当前大小
    
    struct sigevent notify;         // 异步通知
    struct pid *notify_owner;
    struct user_namespace *notify_user_ns;
    
    unsigned long notify_cookie;
};
```
[Intermediate]

---

## 9. POSIX Semaphores (POSIX信号量)

---

Q: What are named and unnamed POSIX semaphores?
A: 
```c
#include <semaphore.h>

/*=== 命名信号量 ===*/
// 创建/打开（可跨进程）
sem_t *sem = sem_open("/mysem", O_CREAT, 0666, 1);

// P操作
sem_wait(sem);      // 阻塞
sem_trywait(sem);   // 非阻塞
sem_timedwait(sem, &ts);  // 超时

// V操作
sem_post(sem);

// 关闭和删除
sem_close(sem);
sem_unlink("/mysem");


/*=== 匿名信号量 ===*/
sem_t sem;

// 初始化
// pshared=0: 线程间
// pshared=1: 进程间（需在共享内存中）
sem_init(&sem, pshared, initial_value);

// 操作同上
sem_wait(&sem);
sem_post(&sem);

// 销毁
sem_destroy(&sem);


/*=== 进程间匿名信号量 ===*/
// 必须放在共享内存中
int fd = shm_open("/shm_sem", O_CREAT | O_RDWR, 0666);
ftruncate(fd, sizeof(sem_t));
sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);
sem_init(sem, 1, 1);  // pshared=1
```

内核实现（基于futex）：
```c
// POSIX信号量在用户空间通过futex实现
// glibc: nptl/sem_wait.c

int sem_wait(sem_t *sem)
{
    // 快速路径：尝试原子减1
    if (atomic_dec_if_positive(&sem->value) > 0)
        return 0;
    
    // 慢速路径：阻塞等待
    do {
        // 调用futex系统调用等待
        futex(&sem->value, FUTEX_WAIT, ...);
    } while (atomic_dec_if_positive(&sem->value) <= 0);
    
    return 0;
}

int sem_post(sem_t *sem)
{
    // 增加值
    atomic_inc(&sem->value);
    
    // 唤醒等待者
    futex(&sem->value, FUTEX_WAKE, 1, ...);
    
    return 0;
}
```
[Intermediate]

---

## 10. Unix Domain Sockets (Unix域套接字)

---

Q: What are Unix domain sockets and their kernel implementation?
A: Unix域套接字提供本地进程间的双向通信：

```c
// 内核数据结构 (net/unix/af_unix.c)
struct unix_sock {
    struct sock sk;                  // 通用socket
    struct unix_address *addr;       // 地址
    struct path path;                // 绑定路径
    struct mutex iolock;
    struct mutex bindlock;
    struct sock *peer;               // 对端socket
    struct list_head link;
    atomic_long_t inflight;
    spinlock_t lock;
    unsigned long gc_flags;
    wait_queue_head_t peer_wait;     // 等待连接
    struct unix_sk_receive_queue receive_queue; // 接收队列
};

// 地址结构
struct unix_address {
    refcount_t refcnt;
    int len;
    unsigned int hash;
    struct sockaddr_un name[0];      // 柔性数组
};
```

使用示例：
```c
// 服务端
int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
strcpy(addr.sun_path, "/tmp/mysock");
unlink(addr.sun_path);  // 删除已存在的

bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
listen(server_fd, 5);

int client_fd = accept(server_fd, NULL, NULL);

// 客户端
int fd = socket(AF_UNIX, SOCK_STREAM, 0);
connect(fd, (struct sockaddr *)&addr, sizeof(addr));

// 通信
write(fd, "Hello", 5);
read(fd, buf, sizeof(buf));
```
[Intermediate]

---

Q: How do Unix sockets pass file descriptors between processes?
A: Unix域套接字可以传递文件描述符（SCM_RIGHTS）：

```c
// 发送文件描述符
void send_fd(int unix_sock, int fd_to_send)
{
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    struct iovec iov = {.iov_base = "x", .iov_len = 1};
    
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;     // 传递文件描述符
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *((int *)CMSG_DATA(cmsg)) = fd_to_send;
    
    sendmsg(unix_sock, &msg, 0);
}

// 接收文件描述符
int receive_fd(int unix_sock)
{
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    char data[1];
    struct iovec iov = {.iov_base = data, .iov_len = 1};
    
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    
    recvmsg(unix_sock, &msg, 0);
    
    cmsg = CMSG_FIRSTHDR(&msg);
    return *((int *)CMSG_DATA(cmsg));  // 接收到的fd
}
```

内核实现：
```c
// net/unix/af_unix.c
static int unix_scm_to_skb(struct scm_cookie *scm, struct sk_buff *skb,
                           bool send_fds)
{
    if (send_fds && scm->fp) {
        // 将文件描述符附加到skb
        UNIXCB(skb).fp = scm_fp_dup(scm->fp);
    }
    return 0;
}

// 接收端获取fd
static void unix_peek_fds(struct scm_cookie *scm, struct sk_buff *skb)
{
    // 为接收进程分配新的fd
    scm->fp = scm_fp_dup(UNIXCB(skb).fp);
}
```
[Advanced]

---

Q: What is the difference between SOCK_STREAM and SOCK_DGRAM for Unix sockets?
A: 
| 特性 | SOCK_STREAM | SOCK_DGRAM |
|------|-------------|------------|
| 连接 | 需要connect/accept | 无连接 |
| 可靠性 | 可靠、有序 | 可靠（本地）、有序 |
| 边界 | 字节流 | 消息边界保留 |
| API | read/write/send/recv | sendto/recvfrom |

```c
// SOCK_DGRAM使用
int server = socket(AF_UNIX, SOCK_DGRAM, 0);
bind(server, &addr, len);

// 客户端发送
int client = socket(AF_UNIX, SOCK_DGRAM, 0);
// 可选：绑定客户端地址用于接收响应
bind(client, &client_addr, len);
sendto(client, msg, len, 0, &server_addr, sizeof(server_addr));

// 服务端接收
struct sockaddr_un from;
socklen_t fromlen = sizeof(from);
recvfrom(server, buf, size, 0, &from, &fromlen);
// from中包含发送者地址，可用于响应
```

内核处理：
```c
// SOCK_STREAM: 像TCP一样维护连接状态
// 使用unix_stream_recvmsg/unix_stream_sendmsg

// SOCK_DGRAM: 像UDP一样发送独立消息
// 使用unix_dgram_recvmsg/unix_dgram_sendmsg
```
[Intermediate]

---

## 11. Futex (Fast Userspace Mutex)

---

Q: What is a futex and how does it work?
A: Futex是用户空间互斥锁的内核支持：

```
无竞争情况（快速路径）：
+------------------+
|    User Space    |
|   atomic CAS     |  纯用户空间操作，无系统调用
+------------------+

有竞争情况（慢速路径）：
+------------------+
|    User Space    |
|   atomic CAS     |  失败
+--------+---------+
         |
         v
+--------+---------+
|  futex syscall   |  进入内核
+--------+---------+
         |
         v
+------------------+
|   Kernel wait    |  阻塞在futex队列
|   on hash table  |
+------------------+
```

内核数据结构：
```c
// kernel/futex.c
struct futex_hash_bucket {
    atomic_t waiters;
    spinlock_t lock;
    struct plist_head chain;
};

struct futex_q {
    struct plist_node list;
    struct task_struct *task;     // 等待的任务
    spinlock_t *lock_ptr;
    union futex_key key;          // futex地址标识
    struct futex_pi_state *pi_state;
    struct rt_mutex_waiter *rt_waiter;
    // ...
};

// futex key（标识futex地址）
union futex_key {
    struct {
        u64 i_seq;
        unsigned long pgoff;
        unsigned int offset;
    } shared;                     // 共享映射
    struct {
        union {
            struct mm_struct *mm;
            u64 __tmp;
        };
        unsigned long address;
        unsigned int offset;
    } private;                    // 私有映射
    struct {
        u64 ptr;
        unsigned long word;
        unsigned int offset;
    } both;
};
```
[Advanced]

---

Q: How are pthread mutexes implemented using futex?
A: 
```c
// glibc中pthread_mutex的简化实现
typedef struct {
    int __lock;           // 锁状态：0=未锁，1=已锁，2=已锁+有等待者
    int __count;          // 递归计数
    int __owner;          // 所有者线程ID
    // ...
} pthread_mutex_t;

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    // 快速路径：尝试从0变为1
    if (atomic_compare_exchange(&mutex->__lock, 0, 1) == 0)
        return 0;  // 获取成功
    
    // 慢速路径：设置为2并等待
    do {
        // 如果还没有等待者标记，设置它
        if (mutex->__lock != 2)
            atomic_exchange(&mutex->__lock, 2);
        
        // 等待futex
        futex(&mutex->__lock, FUTEX_WAIT, 2, NULL, NULL, 0);
        
    } while (atomic_compare_exchange(&mutex->__lock, 0, 2) != 0);
    
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    // 减少锁值
    if (atomic_fetch_sub(&mutex->__lock, 1) != 1) {
        // 有等待者（之前值为2）
        mutex->__lock = 0;
        
        // 唤醒一个等待者
        futex(&mutex->__lock, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
    
    return 0;
}
```

Futex系统调用：
```c
SYSCALL_DEFINE6(futex, u32 __user *, uaddr, int, op, u32, val,
                struct timespec __user *, utime, u32 __user *, uaddr2,
                u32, val3)
{
    switch (op & FUTEX_CMD_MASK) {
    case FUTEX_WAIT:
        // 如果*uaddr == val，阻塞
        return futex_wait(uaddr, val, timeout, ...);
        
    case FUTEX_WAKE:
        // 唤醒最多val个等待者
        return futex_wake(uaddr, val, ...);
        
    case FUTEX_REQUEUE:
        // 唤醒一些，将其余重新排队到uaddr2
        return futex_requeue(uaddr, uaddr2, ...);
        
    // ... 其他操作
    }
}
```
[Advanced]

---

## 12. eventfd (事件文件描述符)

---

Q: What is eventfd and how is it used?
A: eventfd提供轻量级进程/线程间通知机制：

```c
#include <sys/eventfd.h>

// 创建eventfd
int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
// 初始值为0，标志可选

// 写入（增加计数器）
uint64_t val = 1;
write(efd, &val, sizeof(val));  // 计数器 += val

// 读取（获取并重置计数器）
uint64_t val;
read(efd, &val, sizeof(val));   // val = 计数器, 计数器 = 0
// 如果EFD_SEMAPHORE标志，则val=1且计数器-=1

// 可用于poll/epoll
struct pollfd pfd = {.fd = efd, .events = POLLIN};
poll(&pfd, 1, -1);
```

内核实现：
```c
// fs/eventfd.c
struct eventfd_ctx {
    struct kref kref;
    wait_queue_head_t wqh;        // 等待队列
    __u64 count;                   // 计数器
    unsigned int flags;
};

static ssize_t eventfd_read(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct eventfd_ctx *ctx = file->private_data;
    ssize_t res;
    __u64 ucnt = 0;
    
    spin_lock_irq(&ctx->wqh.lock);
    res = -EAGAIN;
    
    if (ctx->count > 0) {
        // 有事件
        if (ctx->flags & EFD_SEMAPHORE) {
            ucnt = 1;
            ctx->count--;
        } else {
            ucnt = ctx->count;
            ctx->count = 0;
        }
        res = sizeof(ucnt);
    } else if (!(file->f_flags & O_NONBLOCK)) {
        // 阻塞等待
        prepare_to_wait(&ctx->wqh, &wait, TASK_INTERRUPTIBLE);
        spin_unlock_irq(&ctx->wqh.lock);
        schedule();
        // ...
    }
    
    spin_unlock_irq(&ctx->wqh.lock);
    
    if (res > 0)
        put_user(ucnt, (__u64 __user *)buf);
    
    return res;
}

static ssize_t eventfd_write(struct file *file, const char __user *buf,
                             size_t count, loff_t *ppos)
{
    struct eventfd_ctx *ctx = file->private_data;
    __u64 ucnt;
    
    get_user(ucnt, (__u64 __user *)buf);
    
    spin_lock_irq(&ctx->wqh.lock);
    
    if (ULLONG_MAX - ctx->count < ucnt)
        res = -EAGAIN;  // 会溢出
    else
        ctx->count += ucnt;
    
    // 唤醒等待者
    if (waitqueue_active(&ctx->wqh))
        wake_up_locked_poll(&ctx->wqh, EPOLLIN);
    
    spin_unlock_irq(&ctx->wqh.lock);
    
    return sizeof(ucnt);
}
```

典型应用场景：
- 线程间通知
- 与epoll配合使用
- 虚拟化中guest/host通信
[Intermediate]

---

## 13. timerfd (定时器文件描述符)

---

Q: How does timerfd work?
A: timerfd将定时器暴露为文件描述符：

```c
#include <sys/timerfd.h>

// 创建定时器
int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

// 设置定时
struct itimerspec its = {
    .it_value = {.tv_sec = 1, .tv_nsec = 0},     // 首次触发
    .it_interval = {.tv_sec = 1, .tv_nsec = 0}   // 间隔
};
timerfd_settime(tfd, 0, &its, NULL);

// 等待定时器（可配合epoll）
uint64_t expirations;
read(tfd, &expirations, sizeof(expirations));
// expirations = 自上次read以来的超时次数
```

内核实现：
```c
// fs/timerfd.c
struct timerfd_ctx {
    union {
        struct hrtimer tmr;           // 高精度定时器
        struct alarm alarm;
    } t;
    ktime_t tintv;                     // 间隔
    ktime_t moffs;
    wait_queue_head_t wqh;             // 等待队列
    u64 ticks;                         // 超时次数
    int clockid;
    short unsigned expired;
    short unsigned settime_flags;
    struct rcu_head rcu;
    struct list_head clist;
    spinlock_t cancel_lock;
    bool might_cancel;
};

// 定时器回调
static enum hrtimer_restart timerfd_tmrproc(struct hrtimer *htmr)
{
    struct timerfd_ctx *ctx = container_of(htmr, struct timerfd_ctx, t.tmr);
    
    spin_lock_irqsave(&ctx->wqh.lock, flags);
    ctx->expired = 1;
    ctx->ticks++;
    wake_up_locked_poll(&ctx->wqh, EPOLLIN);
    spin_unlock_irqrestore(&ctx->wqh.lock, flags);
    
    // 如果有间隔，重新启动
    if (ctx->tintv != 0)
        return HRTIMER_RESTART;
    
    return HRTIMER_NORESTART;
}
```
[Intermediate]

---

## 14. signalfd (信号文件描述符)

---

Q: How does signalfd work?
A: signalfd允许通过文件描述符接收信号：

```c
#include <sys/signalfd.h>

// 阻塞要接收的信号
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigaddset(&mask, SIGTERM);
sigprocmask(SIG_BLOCK, &mask, NULL);  // 必须先阻塞

// 创建signalfd
int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

// 读取信号信息
struct signalfd_siginfo info;
read(sfd, &info, sizeof(info));

printf("Received signal %d from pid %d\n",
       info.ssi_signo, info.ssi_pid);

// 可配合epoll使用
// 信号被同步处理，避免异步信号处理的复杂性
```

内核实现：
```c
// fs/signalfd.c
struct signalfd_ctx {
    sigset_t sigmask;             // 监听的信号集
};

static ssize_t signalfd_read(struct file *file, char __user *buf,
                             size_t count, loff_t *ppos)
{
    struct signalfd_ctx *ctx = file->private_data;
    struct signalfd_siginfo __user *siginfo = (void __user *)buf;
    siginfo_t info;
    int ret;
    
    ret = dequeue_signal(current, &ctx->sigmask, &info);
    
    if (ret == 0) {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;
        
        // 阻塞等待信号
        ret = wait_event_interruptible(current->sighand->signalfd_wqh,
                     has_pending_signals(&ctx->sigmask, 
                                        &current->pending, ...));
        if (ret)
            return ret;
        
        ret = dequeue_signal(current, &ctx->sigmask, &info);
    }
    
    // 复制信号信息到用户空间
    signalfd_copy_info(siginfo, &info);
    
    return sizeof(struct signalfd_siginfo);
}
```
[Intermediate]

---

## 15. IPC Namespaces (IPC命名空间)

---

Q: What is an IPC namespace and how does it provide isolation?
A: IPC命名空间隔离System V IPC和POSIX消息队列：

```c
// 每个命名空间有独立的IPC资源
struct ipc_namespace {
    struct kref kref;
    
    struct ipc_ids ids[3];           // 信号量、消息队列、共享内存
    
    // 资源限制
    unsigned int msg_ctlmax;
    unsigned int msg_ctlmnb;
    unsigned int msg_ctlmni;
    
    unsigned int shm_ctlmax;
    unsigned int shm_ctlall;
    unsigned int shm_ctlmni;
    
    unsigned int sem_ctlmni;
    unsigned int sem_ctlmsl;
    unsigned int sem_ctlmns;
    
    // POSIX消息队列
    struct vfsmount *mq_mnt;
    unsigned int mq_queues_count;
    unsigned int mq_queues_max;
    // ...
};

// 创建新IPC命名空间
struct ipc_namespace *create_ipc_ns(...)
{
    struct ipc_namespace *ns;
    
    ns = kmalloc(sizeof(*ns), GFP_KERNEL);
    
    // 初始化IPC ID数组
    sem_init_ns(ns);
    msg_init_ns(ns);
    shm_init_ns(ns);
    
    // 初始化消息队列文件系统
    mq_init_ns(ns);
    
    return ns;
}
```

使用示例：
```c
// 创建新的IPC命名空间
unshare(CLONE_NEWIPC);

// 或使用clone
clone(child_func, stack, CLONE_NEWIPC | SIGCHLD, arg);

// 效果：
// 父进程和子进程的System V IPC完全隔离
// 同一个key在不同命名空间中指向不同资源

// 容器典型用法
// Docker/LXC使用IPC命名空间隔离容器
```
[Intermediate]

---

## 16. Debugging IPC (调试IPC)

---

Q: How to debug and monitor IPC resources?
A: 
```bash
# System V IPC状态
ipcs           # 显示所有IPC资源
ipcs -m        # 共享内存
ipcs -q        # 消息队列
ipcs -s        # 信号量
ipcs -a        # 详细信息

# 删除IPC资源
ipcrm -m <shmid>  # 删除共享内存
ipcrm -q <msgid>  # 删除消息队列
ipcrm -s <semid>  # 删除信号量

# 系统限制
cat /proc/sys/kernel/msgmax    # 消息最大大小
cat /proc/sys/kernel/msgmnb    # 队列最大字节
cat /proc/sys/kernel/msgmni    # 最大队列数
cat /proc/sys/kernel/shmmax    # 最大共享内存段
cat /proc/sys/kernel/shmall    # 共享内存总页数
cat /proc/sys/kernel/shmmni    # 最大共享内存段数
cat /proc/sys/kernel/sem       # 信号量限制

# 进程打开的IPC
ls -l /proc/<pid>/fd           # 查看文件描述符
lsof -p <pid>                  # 详细列表

# POSIX消息队列
ls /dev/mqueue/

# 共享内存
ls /dev/shm/

# 跟踪IPC系统调用
strace -e trace=ipc ./program
strace -e trace=signal ./program
```

内核调试：
```c
// 启用调试输出
echo 1 > /sys/kernel/debug/tracing/events/signal/enable
echo 1 > /sys/kernel/debug/tracing/events/ipc/enable

// 查看跟踪
cat /sys/kernel/debug/tracing/trace_pipe
```
[Basic]

---

## 17. Common IPC Patterns (常见IPC模式)

---

Q: What are common IPC patterns and their implementations?
A: 
```c
/*=== 1. 生产者-消费者模式 ===*/
// 使用信号量和共享内存
struct shared_buffer {
    sem_t empty;    // 空槽位数
    sem_t full;     // 满槽位数
    sem_t mutex;    // 互斥
    int buffer[N];
    int in, out;
};

// 生产者
sem_wait(&sb->empty);
sem_wait(&sb->mutex);
sb->buffer[sb->in] = item;
sb->in = (sb->in + 1) % N;
sem_post(&sb->mutex);
sem_post(&sb->full);

// 消费者
sem_wait(&sb->full);
sem_wait(&sb->mutex);
item = sb->buffer[sb->out];
sb->out = (sb->out + 1) % N;
sem_post(&sb->mutex);
sem_post(&sb->empty);


/*=== 2. 请求-响应模式 ===*/
// 使用两个消息队列
int req_queue = msgget(REQ_KEY, ...);
int rsp_queue = msgget(RSP_KEY, ...);

// 客户端
msgsnd(req_queue, &request, ...);
msgrcv(rsp_queue, &response, sizeof(response), client_id, 0);

// 服务端
while (1) {
    msgrcv(req_queue, &request, ...);
    process(&request, &response);
    response.mtype = request.client_id;
    msgsnd(rsp_queue, &response, ...);
}


/*=== 3. 发布-订阅模式 ===*/
// 使用Unix域套接字多播
// 或信号+共享内存

// 发布者
for (sub in subscribers) {
    write(sub->fd, event, len);  // Unix域套接字
}

// 订阅者
struct pollfd fds[MAX_PUBS];
poll(fds, npubs, -1);


/*=== 4. 管道链模式 ===*/
// shell管道: cmd1 | cmd2 | cmd3
int fd1[2], fd2[2];
pipe(fd1);
pipe(fd2);

if (fork() == 0) {
    // cmd1: 写入fd1
    dup2(fd1[1], STDOUT_FILENO);
    exec(cmd1);
}

if (fork() == 0) {
    // cmd2: 从fd1读，写入fd2
    dup2(fd1[0], STDIN_FILENO);
    dup2(fd2[1], STDOUT_FILENO);
    exec(cmd2);
}

if (fork() == 0) {
    // cmd3: 从fd2读
    dup2(fd2[0], STDIN_FILENO);
    exec(cmd3);
}
```
[Intermediate]

---

Q: What are the common mistakes in IPC programming?
A: 
| 错误 | 后果 | 正确做法 |
|------|------|----------|
| 不清理IPC资源 | 资源泄漏 | 使用IPC_RMID或unlink |
| 信号处理函数中调用不安全函数 | 死锁/崩溃 | 只用async-signal-safe函数 |
| 共享内存未同步 | 数据竞争 | 使用信号量/互斥锁 |
| 管道读写端未正确关闭 | 死锁/僵尸 | fork后关闭不用的端 |
| 忽略SIGPIPE | 程序终止 | 处理或忽略SIGPIPE |
| 消息队列满时阻塞 | 死锁 | 使用IPC_NOWAIT或限制大小 |
| 信号量死锁 | 程序挂起 | 正确的加锁顺序、超时 |
| Unix套接字bind路径未unlink | bind失败 | 先unlink再bind |

```c
// 正确的管道使用
int pipefd[2];
pipe(pipefd);

if (fork() == 0) {
    close(pipefd[1]);  // 子进程关闭写端
    read(pipefd[0], ...);
    close(pipefd[0]);
    exit(0);
}

close(pipefd[0]);      // 父进程关闭读端
write(pipefd[1], ...);
close(pipefd[1]);      // 关闭写端，子进程读到EOF
wait(NULL);

// 处理SIGPIPE
signal(SIGPIPE, SIG_IGN);  // 或使用sigaction
// 写入关闭的管道时返回EPIPE而非终止
```
[Intermediate]

---

*Total: 100+ cards covering Linux kernel IPC implementation*

