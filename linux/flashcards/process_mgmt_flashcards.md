# Process Management Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel process management, scheduling, and task structures
> **Language**: English terms with Chinese explanations
> **Total Cards**: 120+

---

## 1. Process Fundamentals (进程基础)

---

Q: What is a process in Linux?
A: 进程是程序的运行实例：

```
+------------------------------------------------------------------+
|                         Process                                   |
+------------------------------------------------------------------+
|                                                                  |
|  +----------------+  +----------------+  +----------------+      |
|  |   Code (Text)  |  |     Data       |  |     Stack      |      |
|  |   (只读)       |  |   (全局变量)    |  |   (局部变量)    |      |
|  +----------------+  +----------------+  +----------------+      |
|                                                                  |
|  +----------------+  +----------------+  +----------------+      |
|  |      Heap      |  |  Memory Maps   |  |   File Desc.   |      |
|  |   (动态分配)   |  |   (共享库等)    |  |   (打开文件)    |      |
|  +----------------+  +----------------+  +----------------+      |
|                                                                  |
|  +----------------------------------------------------------+   |
|  |                   Kernel Resources                        |   |
|  |  - task_struct (进程描述符)                               |   |
|  |  - mm_struct (内存描述符)                                 |   |
|  |  - files_struct (文件表)                                  |   |
|  |  - signal_struct (信号信息)                               |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

进程特点：
- 独立的地址空间
- 拥有系统资源（文件、内存等）
- 有唯一的PID
- 可包含多个线程
[Basic]

---

Q: What is the difference between process and thread?
A: 
| 特性 | 进程 (Process) | 线程 (Thread) |
|------|---------------|---------------|
| 地址空间 | 独立 | 共享 |
| 创建开销 | 大 | 小 |
| 通信 | IPC（管道、共享内存等） | 直接共享内存 |
| 上下文切换 | 慢（需切换页表） | 快（共享页表） |
| 隔离性 | 强 | 弱 |
| 崩溃影响 | 不影响其他进程 | 可能影响同进程其他线程 |

```
进程模型：                        线程模型：
+----------+ +----------+         +------------------------+
| Process1 | | Process2 |         |       Process          |
|          | |          |         |  +------+ +------+     |
| [Code]   | | [Code]   |         |  |Thread| |Thread|     |
| [Data]   | | [Data]   |         |  +------+ +------+     |
| [Stack]  | | [Stack]  |         |  [Code] [Data]         |
|          | |          |         |  [Stack1] [Stack2]     |
+----------+ +----------+         +------------------------+
独立地址空间                       共享地址空间，独立栈
```

Linux实现：
```c
// Linux中线程也是task_struct
// 区别在于clone()时的标志
fork()  -> clone(SIGCHLD)                    // 新进程
pthread_create() -> clone(CLONE_VM | CLONE_FS | 
                          CLONE_FILES | CLONE_SIGHAND | 
                          CLONE_THREAD)       // 新线程
```
[Basic]

---

Q: What is task_struct and what does it contain?
A: `task_struct`是进程描述符，包含进程所有信息：

```c
// include/linux/sched.h
struct task_struct {
    /*=== 状态信息 ===*/
    volatile long state;              // 进程状态
    unsigned int flags;               // 进程标志 (PF_*)
    int on_rq;                        // 是否在运行队列
    int prio, static_prio, normal_prio;  // 优先级
    unsigned int rt_priority;         // 实时优先级
    
    /*=== 调度信息 ===*/
    const struct sched_class *sched_class;  // 调度类
    struct sched_entity se;           // CFS调度实体
    struct sched_rt_entity rt;        // RT调度实体
    unsigned int policy;              // 调度策略
    int nr_cpus_allowed;
    cpumask_t cpus_mask;              // CPU亲和性
    
    /*=== 进程关系 ===*/
    struct task_struct __rcu *real_parent;  // 真实父进程
    struct task_struct __rcu *parent;       // 父进程（可能是调试器）
    struct list_head children;        // 子进程链表
    struct list_head sibling;         // 兄弟进程链表
    struct task_struct *group_leader; // 线程组leader
    
    /*=== 标识信息 ===*/
    pid_t pid;                        // 进程ID
    pid_t tgid;                       // 线程组ID
    struct pid *thread_pid;           // PID结构指针
    
    /*=== 内存管理 ===*/
    struct mm_struct *mm;             // 内存描述符
    struct mm_struct *active_mm;      // 活动内存描述符
    
    /*=== 文件系统 ===*/
    struct fs_struct *fs;             // 文件系统信息
    struct files_struct *files;       // 打开的文件
    struct nsproxy *nsproxy;          // 命名空间
    
    /*=== 信号 ===*/
    struct signal_struct *signal;     // 信号处理
    struct sighand_struct *sighand;   // 信号处理程序
    sigset_t blocked;                 // 阻塞的信号
    struct sigpending pending;        // 挂起的信号
    
    /*=== 时间统计 ===*/
    u64 utime, stime;                 // 用户态/内核态时间
    u64 start_time;                   // 启动时间
    
    /*=== 其他 ===*/
    char comm[TASK_COMM_LEN];         // 进程名
    struct thread_struct thread;       // CPU相关上下文
    // ... 还有更多字段
};
```

大小：约 6-8 KB（根据配置不同）
[Intermediate]

---

## 2. Process States (进程状态)

---

Q: What are the process states in Linux?
A: Linux进程状态机：

```
                    +------------------+
                    |                  |
         fork()     |  TASK_RUNNING    |<----+
            +------>|  (就绪/运行)      |     |
            |       |                  |     |
            |       +--------+---------+     |
            |                |               |
            |    被调度执行   |               | 被唤醒
            |                v               |
+--------+  |       +--------+---------+     |
|  NEW   |--+       |    Running       |     |
|  (新建) |          |    (CPU执行中)    |     |
+--------+          +--------+---------+     |
                             |               |
        +--------------------+--------------------+
        |                    |                    |
        | 等待I/O            | 等待信号/事件       | 等待不可中断事件
        v                    v                    v
+-------+--------+  +--------+-------+  +--------+-------+
| TASK_INTER-    |  | TASK_STOPPED   |  | TASK_UNINTER-  |
| RUPTIBLE       |  | (停止)          |  | RUPTIBLE       |
| (可中断睡眠)    |  | SIGSTOP/ptrace |  | (不可中断睡眠)  |
+-------+--------+  +--------+-------+  +--------+-------+
        |                    |                    |
        +--------------------+--------------------+
                             |
                             v
                    +--------+-------+
                    |  TASK_ZOMBIE   |  <-- exit()但未被wait()
                    |  (僵尸)         |
                    +--------+-------+
                             |
                             | wait()
                             v
                    +--------+-------+
                    |     DEAD       |
                    |   (已销毁)      |
                    +----------------+
```

状态值定义：
```c
// include/linux/sched.h
#define TASK_RUNNING            0x0000  // 就绪或运行
#define TASK_INTERRUPTIBLE      0x0001  // 可中断睡眠
#define TASK_UNINTERRUPTIBLE    0x0002  // 不可中断睡眠
#define __TASK_STOPPED          0x0004  // 停止
#define __TASK_TRACED           0x0008  // 被跟踪
#define EXIT_DEAD               0x0010  // 最终状态
#define EXIT_ZOMBIE             0x0020  // 僵尸
#define TASK_PARKED             0x0040  // 停放（kthread）
#define TASK_DEAD               0x0080  // 死亡
#define TASK_WAKEKILL           0x0100  // 可被致命信号唤醒
#define TASK_WAKING             0x0200  // 正在唤醒
#define TASK_NOLOAD             0x0400  // 不计入负载
#define TASK_NEW                0x0800  // 新创建

// 组合状态
#define TASK_KILLABLE           (TASK_WAKEKILL | TASK_UNINTERRUPTIBLE)
```
[Intermediate]

---

Q: What is the difference between TASK_INTERRUPTIBLE and TASK_UNINTERRUPTIBLE?
A: 
| 特性 | TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE |
|------|-------------------|---------------------|
| 可被信号唤醒 | ✓ 是 | ✗ 否 |
| ps显示 | S (sleeping) | D (disk sleep) |
| 用途 | 等待用户输入、网络I/O | 等待磁盘I/O、关键操作 |
| 可kill | ✓ 可以 | ✗ 不能 |

```c
// 可中断睡眠示例
// 等待数据到达，可被信号中断
wait_event_interruptible(wq, condition);

// 不可中断睡眠示例
// 等待磁盘I/O完成，不能被中断
wait_event(wq, condition);

// TASK_KILLABLE - 折中方案
// 只能被致命信号(SIGKILL)唤醒
wait_event_killable(wq, condition);
```

为什么需要TASK_UNINTERRUPTIBLE：
```c
// 场景：读取磁盘数据
void read_block(struct buffer_head *bh)
{
    submit_bh(READ, bh);
    
    // 如果使用INTERRUPTIBLE：
    // 1. 进程发送读请求
    // 2. 进程睡眠等待
    // 3. 收到信号被唤醒
    // 4. 进程返回-EINTR
    // 5. 磁盘数据到达，但进程已不在等待
    // 6. 数据丢失或状态不一致！
    
    // 所以必须用UNINTERRUPTIBLE
    wait_on_buffer(bh);  // 不可中断等待
}
```
[Intermediate]

---

## 3. Process Creation (进程创建)

---

Q: How does fork() work in the kernel?
A: fork()通过clone()实现：

```
fork() 系统调用流程：

User Space:
    fork()
       |
       v
Kernel Space:
    sys_fork()
       |
       v
    kernel_clone()
       |
       +---> copy_process()
       |        |
       |        +---> dup_task_struct()      // 复制task_struct
       |        |
       |        +---> copy_creds()           // 复制凭证
       |        |
       |        +---> copy_semundo()         // 复制信号量
       |        |
       |        +---> copy_files()           // 复制文件表
       |        |
       |        +---> copy_fs()              // 复制fs_struct
       |        |
       |        +---> copy_sighand()         // 复制信号处理
       |        |
       |        +---> copy_signal()          // 复制signal_struct
       |        |
       |        +---> copy_mm()              // 复制内存描述符
       |        |                            // (COW页表)
       |        +---> copy_namespaces()      // 复制命名空间
       |        |
       |        +---> copy_thread()          // 复制线程信息
       |        |
       |        +---> pid_alloc()            // 分配PID
       |
       +---> wake_up_new_task()              // 唤醒新进程
       |
       v
    返回子进程PID（父进程）
    返回0（子进程）
```

Copy-on-Write (COW)：
```c
// 内存不立即复制，而是共享+标记只读
// 当任一进程写入时才真正复制
fork() 后的内存状态：
+------------------+     +------------------+
|  Parent Process  |     |  Child Process   |
+------------------+     +------------------+
|  Page Table      |     |  Page Table      |
|  [PTE1]--+       |     |  [PTE1]--+       |
|  [PTE2]--+--+    |     |  [PTE2]--+--+    |
+----------+--+----+     +----------+--+----+
           |  |                     |  |
           |  +---> [Physical Page] <--+  共享，标记只读
           |                        |
           +------> [Physical Page] <-----+ 共享，标记只读

写入时：
Parent写入Page1
           |
           v
触发Page Fault --> 复制页面 --> 更新父进程页表
```
[Intermediate]

---

Q: What is the implementation of clone()?
A: clone()是创建进程/线程的通用接口：

```c
// kernel/fork.c
SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
                int __user *, parent_tidptr, int __user *, child_tidptr,
                unsigned long, tls)
{
    struct kernel_clone_args args = {
        .flags          = (lower_32_bits(clone_flags) & ~CSIGNAL),
        .pidfd          = parent_tidptr,
        .child_tid      = child_tidptr,
        .parent_tid     = parent_tidptr,
        .exit_signal    = (lower_32_bits(clone_flags) & CSIGNAL),
        .stack          = newsp,
        .tls            = tls,
    };
    
    return kernel_clone(&args);
}

// 核心克隆函数
pid_t kernel_clone(struct kernel_clone_args *args)
{
    struct task_struct *p;
    pid_t nr;
    
    // 1. 复制进程
    p = copy_process(NULL, trace, NUMA_NO_NODE, args);
    if (IS_ERR(p))
        return PTR_ERR(p);
    
    // 2. 获取PID
    nr = get_task_pid(p, PIDTYPE_PID)->nr;
    
    // 3. 唤醒新任务
    wake_up_new_task(p);
    
    return nr;
}
```

clone标志：
```c
// include/uapi/linux/sched.h
#define CLONE_VM        0x00000100  // 共享内存空间
#define CLONE_FS        0x00000200  // 共享文件系统信息
#define CLONE_FILES     0x00000400  // 共享文件描述符表
#define CLONE_SIGHAND   0x00000800  // 共享信号处理
#define CLONE_PIDFD     0x00001000  // 创建pidfd
#define CLONE_PTRACE    0x00002000  // 允许ptrace
#define CLONE_VFORK     0x00004000  // 父进程阻塞直到子进程exit/exec
#define CLONE_PARENT    0x00008000  // 与父进程同级
#define CLONE_THREAD    0x00010000  // 同一线程组
#define CLONE_NEWNS     0x00020000  // 新mount命名空间
#define CLONE_SYSVSEM   0x00040000  // 共享System V信号量
#define CLONE_SETTLS    0x00080000  // 设置TLS
#define CLONE_PARENT_SETTID  0x00100000  // 设置父进程tid
#define CLONE_CHILD_CLEARTID 0x00200000  // 清除子进程tid
#define CLONE_DETACHED  0x00400000  // 未使用
#define CLONE_UNTRACED  0x00800000  // 不可ptrace
#define CLONE_CHILD_SETTID   0x01000000  // 设置子进程tid
#define CLONE_NEWCGROUP 0x02000000  // 新cgroup命名空间
#define CLONE_NEWUTS    0x04000000  // 新UTS命名空间
#define CLONE_NEWIPC    0x08000000  // 新IPC命名空间
#define CLONE_NEWUSER   0x10000000  // 新user命名空间
#define CLONE_NEWPID    0x20000000  // 新PID命名空间
#define CLONE_NEWNET    0x40000000  // 新network命名空间
```
[Advanced]

---

Q: What is the difference between fork(), vfork(), and clone()?
A: 
| 特性 | fork() | vfork() | clone() |
|------|--------|---------|---------|
| 地址空间 | 复制(COW) | 共享 | 可选 |
| 父进程行为 | 继续执行 | 阻塞直到子进程exec/exit | 可选 |
| 使用场景 | 创建进程 | 快速exec | 创建进程/线程 |
| 灵活性 | 低 | 低 | 高 |

```c
// fork() 实现
SYSCALL_DEFINE0(fork)
{
    struct kernel_clone_args args = {
        .exit_signal = SIGCHLD,
    };
    return kernel_clone(&args);
}

// vfork() 实现
SYSCALL_DEFINE0(vfork)
{
    struct kernel_clone_args args = {
        .flags       = CLONE_VFORK | CLONE_VM,
        .exit_signal = SIGCHLD,
    };
    return kernel_clone(&args);
}

// pthread_create() 使用的clone标志
clone(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
      CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
      CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID,
      stack, &parent_tid, tls, &child_tid);
```

vfork的危险性：
```c
// vfork共享地址空间，子进程修改会影响父进程
pid_t pid = vfork();
if (pid == 0) {
    // 危险！不能修改局部变量
    // 不能调用除exec/exit外的函数
    // 不能return
    execl("/bin/ls", "ls", NULL);
    _exit(1);  // 使用_exit而非exit
}
```
[Intermediate]

---

## 4. Process Termination (进程终止)

---

Q: How does exit() work in the kernel?
A: 进程退出流程：

```
exit() / exit_group() 流程：

    do_exit(code)
         |
         v
    +----+-----------------------------+
    |    设置 PF_EXITING 标志          |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    exit_signals()                |
    |    - 发送挂起信号给父进程         |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    exit_mm()                     |
    |    - 释放内存描述符              |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    exit_files()                  |
    |    - 关闭所有文件                |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    exit_fs()                     |
    |    - 释放文件系统信息            |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    exit_task_namespaces()        |
    |    - 释放命名空间                |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    exit_notify()                 |
    |    - 通知父进程 (SIGCHLD)        |
    |    - 为子进程找新父进程          |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    设置 state = TASK_DEAD        |
    +----+-----------------------------+
         |
         v
    +----+-----------------------------+
    |    schedule()                    |
    |    - 永不返回                    |
    +----+-----------------------------+
```

代码实现：
```c
// kernel/exit.c
void __noreturn do_exit(long code)
{
    struct task_struct *tsk = current;
    
    // 设置退出标志
    tsk->flags |= PF_EXITING;
    
    // 处理信号
    exit_signals(tsk);
    
    // 释放各种资源
    exit_mm();
    exit_sem(tsk);
    exit_shm(tsk);
    exit_files(tsk);
    exit_fs(tsk);
    exit_task_namespaces(tsk);
    
    // 保存退出码
    tsk->exit_code = code;
    
    // 通知父进程
    exit_notify(tsk, group_dead);
    
    // 进入TASK_DEAD状态
    tsk->state = TASK_DEAD;
    
    // 调度其他进程（永不返回）
    schedule();
    BUG();  // 不应该到达这里
}
```
[Intermediate]

---

Q: What is a zombie process and how is it created?
A: 僵尸进程是已退出但未被父进程回收的进程：

```
僵尸进程产生：

    Parent Process              Child Process
         |                           |
         | fork()                    |
         +-------------------------->|
         |                           |
         |                           | 执行任务
         |                           |
         |                           | exit()
         |                           |
         |                     +-----+-----+
         |                     |  ZOMBIE   |
         |                     | (僵尸状态) |
         |                     +-----+-----+
         |                           |
         | wait() 回收               |
         +-------------------------->|
         |                           |
         |                     +-----+-----+
         |                     |   DEAD    |
         |                     | (已销毁)   |
         |                     +-----------+
```

僵尸进程特点：
```c
// 僵尸进程保留的信息
struct task_struct {
    // 保留这些用于wait()
    int exit_code;           // 退出码
    int exit_signal;         // 退出信号
    u64 utime, stime;        // CPU时间统计
    
    // 已释放的资源
    // mm_struct *mm = NULL
    // files_struct *files = NULL
    // 等
};

// 查看僵尸进程
ps aux | grep Z
// USER  PID %CPU %MEM STAT COMMAND
// root  123  0.0  0.0  Z    [defunct]
```

避免僵尸进程：
```c
// 方法1：wait()回收
wait(NULL);

// 方法2：忽略SIGCHLD
signal(SIGCHLD, SIG_IGN);

// 方法3：双fork
pid_t pid = fork();
if (pid == 0) {
    pid_t pid2 = fork();
    if (pid2 == 0) {
        // 孙子进程执行任务
        // 父进程(子进程)立即退出
        // 孙子进程被init收养
    } else {
        exit(0);
    }
} else {
    wait(NULL);  // 回收子进程
}

// 方法4：使用sigaction的SA_NOCLDWAIT
struct sigaction sa;
sa.sa_handler = SIG_DFL;
sa.sa_flags = SA_NOCLDWAIT;
sigaction(SIGCHLD, &sa, NULL);
```
[Intermediate]

---

Q: How does wait() work?
A: wait()系统调用回收子进程：

```c
// kernel/exit.c
SYSCALL_DEFINE4(wait4, pid_t, upid, int __user *, stat_addr,
                int, options, struct rusage __user *, ru)
{
    struct wait_opts wo;
    struct pid *pid = NULL;
    
    // 设置等待选项
    wo.wo_type = (upid < -1) ? PIDTYPE_PGID :
                 (upid == -1) ? PIDTYPE_MAX :
                 (upid == 0) ? PIDTYPE_PGID : PIDTYPE_PID;
    
    wo.wo_flags = options | WEXITED;
    wo.wo_stat = 0;
    
    // 执行等待
    ret = do_wait(&wo);
    
    // 返回状态
    if (stat_addr)
        put_user(wo.wo_stat, stat_addr);
    
    return ret;
}

static long do_wait(struct wait_opts *wo)
{
    for (;;) {
        // 遍历子进程
        do_wait_thread(wo, tsk);
        
        if (wo->wo_flags & WNOHANG)
            break;  // 非阻塞
        
        // 阻塞等待
        schedule();
    }
}
```

wait选项：
```c
// include/uapi/linux/wait.h
#define WNOHANG     0x00000001  // 非阻塞
#define WUNTRACED   0x00000002  // 报告停止的子进程
#define WSTOPPED    WUNTRACED
#define WEXITED     0x00000004  // 报告已退出的子进程
#define WCONTINUED  0x00000008  // 报告继续运行的子进程
#define WNOWAIT     0x01000000  // 不回收（peek）

// 状态宏
WIFEXITED(status)    // 正常退出？
WEXITSTATUS(status)  // 退出码
WIFSIGNALED(status)  // 被信号杀死？
WTERMSIG(status)     // 终止信号
WIFSTOPPED(status)   // 停止？
WSTOPSIG(status)     // 停止信号
```
[Intermediate]

---

## 5. Process Scheduling (进程调度)

---

Q: What are the scheduler classes in Linux?
A: Linux使用调度类实现不同调度策略：

```
+------------------------------------------------------------------+
|                    Linux Scheduler Classes                        |
+------------------------------------------------------------------+
|                                                                  |
|  优先级高                                                         |
|     ^                                                            |
|     |  +------------------+                                      |
|     |  |   stop_sched     |  停止调度类（最高优先级）              |
|     |  |   (migration)    |  用于CPU迁移和停止任务                |
|     |  +--------+---------+                                      |
|     |           |                                                |
|     |  +--------v---------+                                      |
|     |  |   dl_sched       |  Deadline调度类                       |
|     |  | SCHED_DEADLINE   |  截止时间调度                         |
|     |  +--------+---------+                                      |
|     |           |                                                |
|     |  +--------v---------+                                      |
|     |  |   rt_sched       |  实时调度类                           |
|     |  | SCHED_FIFO       |  先进先出                             |
|     |  | SCHED_RR         |  时间片轮转                           |
|     |  +--------+---------+                                      |
|     |           |                                                |
|     |  +--------v---------+                                      |
|     |  |   fair_sched     |  公平调度类（CFS）                    |
|     |  | SCHED_NORMAL     |  普通进程                             |
|     |  | SCHED_BATCH      |  批处理                               |
|     |  +--------+---------+                                      |
|     |           |                                                |
|     |  +--------v---------+                                      |
|     |  |   idle_sched     |  空闲调度类                           |
|     |  | SCHED_IDLE       |  最低优先级                           |
|     +--+------------------+                                      |
|  优先级低                                                         |
+------------------------------------------------------------------+
```

调度类链表：
```c
// kernel/sched/core.c
// 按优先级顺序链接
#define sched_class_highest (&stop_sched_class)

// 调度类定义
struct sched_class {
    const struct sched_class *next;
    
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)(struct rq *rq);
    
    struct task_struct *(*pick_next_task)(struct rq *rq);
    
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    void (*set_next_task)(struct rq *rq, struct task_struct *p, bool first);
    
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)(struct task_struct *p);
    
    // ...
};

// 遍历调度类
#define for_each_class(class) \
    for (class = sched_class_highest; class; class = class->next)
```
[Intermediate]

---

Q: How does CFS (Completely Fair Scheduler) work?
A: CFS基于虚拟运行时间实现公平调度：

```
CFS核心概念：

1. 虚拟运行时间 (vruntime)
   - 进程实际运行时间 × (NICE_0_LOAD / 进程权重)
   - 权重高的进程vruntime增长慢
   - 调度器选择vruntime最小的进程

2. 红黑树组织
        +------------------+
        |    vruntime=10   |  <- 最小vruntime（下一个运行）
        +--------+---------+
                 |
        +--------+--------+
        |                 |
   +----v----+      +----v----+
   | vr=15   |      | vr=20   |
   +---------+      +---------+

3. 时间片计算
   sched_period = 24ms (默认，随运行队列增长)
   进程时间片 = sched_period × (进程权重 / 队列总权重)
```

核心数据结构：
```c
// include/linux/sched.h
struct sched_entity {
    struct load_weight      load;       // 权重
    struct rb_node          run_node;   // 红黑树节点
    struct list_head        group_node;
    unsigned int            on_rq;      // 是否在运行队列
    
    u64                     exec_start; // 开始执行时间
    u64                     sum_exec_runtime; // 总执行时间
    u64                     vruntime;   // 虚拟运行时间
    u64                     prev_sum_exec_runtime;
    
    // 组调度
    struct sched_entity     *parent;
    struct cfs_rq           *cfs_rq;    // 所属CFS运行队列
    struct cfs_rq           *my_q;      // 拥有的CFS运行队列
    
    // 统计
    struct sched_statistics statistics;
};

// CFS运行队列
struct cfs_rq {
    struct load_weight      load;       // 总权重
    unsigned int            nr_running; // 进程数
    
    u64                     min_vruntime; // 最小vruntime
    
    struct rb_root_cached   tasks_timeline; // 红黑树
    struct sched_entity     *curr;      // 当前运行
    struct sched_entity     *next;      // 下一个
    struct sched_entity     *last;      // 上一个
    struct sched_entity     *skip;      // 跳过
    
    // ...
};
```

调度时机：
```c
// 选择下一个任务
static struct task_struct *pick_next_task_fair(struct rq *rq)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se;
    
    // 获取红黑树最左节点（最小vruntime）
    se = __pick_first_entity(cfs_rq);
    
    return task_of(se);
}

// 更新vruntime
static void update_curr(struct cfs_rq *cfs_rq)
{
    struct sched_entity *curr = cfs_rq->curr;
    u64 now = rq_clock_task(rq);
    u64 delta_exec;
    
    delta_exec = now - curr->exec_start;
    curr->exec_start = now;
    curr->sum_exec_runtime += delta_exec;
    
    // vruntime = 实际时间 × NICE_0_LOAD / 权重
    curr->vruntime += calc_delta_fair(delta_exec, curr);
    
    // 更新最小vruntime
    update_min_vruntime(cfs_rq);
}
```
[Advanced]

---

Q: What is the nice value and how does it affect scheduling?
A: nice值影响进程优先级和CPU时间分配：

```
Nice值范围：-20（最高优先级）到 +19（最低优先级）
默认值：0

+-----------+--------+--------+---------------------------+
| Nice 值   | 优先级  | 权重   | 相对CPU时间               |
+-----------+--------+--------+---------------------------+
|   -20     |   0    | 88761  | ~10倍于nice 0            |
|   -10     |  10    |  9548  | ~3倍于nice 0             |
|     0     |  20    |  1024  | 基准                      |
|    10     |  30    |   110  | ~1/10于nice 0            |
|    19     |  39    |    15  | 最低                      |
+-----------+--------+--------+---------------------------+
```

权重表：
```c
// kernel/sched/core.c
const int sched_prio_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15,
};
```

设置nice值：
```c
// 用户空间
nice(5);                    // 增加nice值
setpriority(PRIO_PROCESS, 0, 5);  // 设置nice值

// 内核实现
SYSCALL_DEFINE1(nice, int, increment)
{
    int nice = task_nice(current);
    nice += increment;
    
    // 范围检查
    if (nice < MIN_NICE)
        nice = MIN_NICE;
    if (nice > MAX_NICE)
        nice = MAX_NICE;
    
    // 权限检查（降低nice需要特权）
    if (increment < 0 && !capable(CAP_SYS_NICE))
        return -EPERM;
    
    set_user_nice(current, nice);
    return 0;
}
```
[Intermediate]

---

## 6. Context Switch (上下文切换)

---

Q: What happens during a context switch?
A: 上下文切换是保存当前进程状态并恢复另一进程的过程：

```
Context Switch流程：

当前进程A                        下一进程B
    |                               |
    v                               |
+---+---------------------------+   |
| 1. 保存A的用户态寄存器         |   |
|    (已在进入内核时保存到栈)    |   |
+---+---------------------------+   |
    |                               |
    v                               |
+---+---------------------------+   |
| 2. 切换内核栈                  |   |
|    current = B                 |   |
|    switch_to(A, B)             |   |
+---+---------------------------+   |
    |                               |
    v                               |
+---+---------------------------+   |
| 3. 切换地址空间（如果需要）     |   |
|    switch_mm(A->mm, B->mm)     |   |
|    - 切换CR3（页表基址）       |   |
|    - 刷新TLB                   |   |
+---+---------------------------+   |
    |                               |
    v                               |
+---+---------------------------+   |
| 4. 恢复B的内核寄存器           |<--+
|    (从B的内核栈恢复)           |
+---+---------------------------+
    |
    v
+---+---------------------------+
| 5. 返回B的用户态               |
|    (恢复用户态寄存器)          |
+---+---------------------------+
```

核心代码：
```c
// kernel/sched/core.c
static __always_inline struct rq *
context_switch(struct rq *rq, struct task_struct *prev,
               struct task_struct *next)
{
    // 准备切换
    prepare_task_switch(rq, prev, next);
    
    // 切换地址空间
    if (!next->mm) {
        // 内核线程：借用上一个进程的mm
        next->active_mm = prev->active_mm;
        mmgrab(prev->active_mm);
    } else {
        // 普通进程：切换mm
        switch_mm_irqs_off(prev->active_mm, next->mm, next);
    }
    
    // 切换寄存器上下文
    switch_to(prev, next, prev);
    
    // 返回后prev可能已经改变
    return finish_task_switch(prev);
}

// arch/x86/entry/entry_64.S
// switch_to宏展开
#define switch_to(prev, next, last) do {
    // 保存callee-save寄存器
    // 切换栈指针
    // 恢复callee-save寄存器
} while (0)
```
[Advanced]

---

Q: What triggers a context switch?
A: 上下文切换触发条件：

```c
/*=== 1. 自愿让出CPU ===*/
// 睡眠
schedule();           // 主动调度
wait_event(...);      // 等待事件
msleep(100);          // 延时
mutex_lock(...);      // 等待锁

// 让出
sched_yield();        // SCHED_OTHER
yield();              // 内核中

/*=== 2. 被动抢占 ===*/
// 时间片用完
tick中断 -> scheduler_tick() -> resched_curr()

// 高优先级任务就绪
wake_up_process(p) -> check_preempt_curr()

// 从中断返回时检查
if (need_resched())
    schedule();

/*=== 3. 进程终止 ===*/
do_exit() -> schedule()

/*=== 4. 进程阻塞 ===*/
// I/O等待
read() -> 等待数据 -> schedule()

// 信号等待
pause() -> schedule()
```

抢占检查：
```c
// include/linux/preempt.h
#define need_resched() test_tsk_need_resched(current)

// 设置重调度标志
static inline void set_tsk_need_resched(struct task_struct *tsk)
{
    set_tsk_thread_flag(tsk, TIF_NEED_RESCHED);
}

// 内核抢占点
// 开启内核抢占时，以下情况可以抢占：
// 1. 从中断返回内核态
// 2. 释放锁时（preempt_enable()）
// 3. 显式调用preempt_check_resched()

void preempt_enable(void)
{
    if (--current->preempt_count == 0 && need_resched())
        preempt_schedule();
}
```
[Intermediate]

---

## 7. Kernel Threads (内核线程)

---

Q: What are kernel threads and how are they created?
A: 内核线程是只在内核空间运行的进程：

```c
// 创建内核线程
struct task_struct *kthread_create(int (*threadfn)(void *data),
                                   void *data,
                                   const char *namefmt, ...);

// 创建并立即运行
struct task_struct *kthread_run(int (*threadfn)(void *data),
                                void *data,
                                const char *namefmt, ...);

// 示例
static int my_thread_func(void *data)
{
    while (!kthread_should_stop()) {
        // 做一些工作
        do_work();
        
        // 睡眠等待
        schedule_timeout_interruptible(HZ);
    }
    return 0;
}

// 创建线程
struct task_struct *thread;
thread = kthread_run(my_thread_func, NULL, "my_thread");

// 停止线程
kthread_stop(thread);
```

内核线程特点：
```
+------------------------------------------------------------------+
|                      Kernel Thread                                |
+------------------------------------------------------------------+
|                                                                  |
|  task_struct                                                     |
|  +----------------------------------------------------------+   |
|  | mm = NULL         (没有用户空间内存)                      |   |
|  | active_mm = 借用  (使用上一个进程的mm)                   |   |
|  | flags |= PF_KTHREAD                                      |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  特点：                                                          |
|  - 只运行在内核空间                                              |
|  - 没有用户地址空间                                              |
|  - 不能被用户态程序直接访问                                       |
|  - 用于后台任务（ksoftirqd, kworker, migration等）              |
|                                                                  |
+------------------------------------------------------------------+
```

常见内核线程：
```bash
ps aux | grep "\[" | head
# [kthreadd]      - 内核线程创建者（PID 2）
# [ksoftirqd/0]   - 软中断处理
# [kworker/0:0H]  - 工作队列
# [migration/0]   - CPU迁移
# [rcu_preempt]   - RCU回调
# [watchdog/0]    - 看门狗
# [kswapd0]       - 内存交换
# [kcompactd0]    - 内存压缩
```
[Intermediate]

---

Q: How does the workqueue mechanism work?
A: 工作队列是延迟执行的机制：

```c
// 定义工作
struct work_struct {
    atomic_long_t data;
    struct list_head entry;
    work_func_t func;
};

// 静态初始化
DECLARE_WORK(my_work, my_work_handler);

// 动态初始化
struct work_struct work;
INIT_WORK(&work, my_work_handler);

// 处理函数
void my_work_handler(struct work_struct *work)
{
    // 在进程上下文中执行
    // 可以睡眠
}

// 调度执行
schedule_work(&work);                    // 使用系统工作队列
schedule_delayed_work(&dwork, delay);    // 延迟执行
queue_work(my_wq, &work);                // 使用自定义工作队列

// 创建自定义工作队列
struct workqueue_struct *my_wq;
my_wq = create_workqueue("my_wq");
my_wq = create_singlethread_workqueue("my_wq");
my_wq = alloc_workqueue("my_wq", WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);

// 销毁
destroy_workqueue(my_wq);
```

工作队列架构：
```
+------------------------------------------------------------------+
|                    Workqueue Architecture                         |
+------------------------------------------------------------------+
|                                                                  |
|  +----------------+     +----------------+     +----------------+ |
|  | work_struct 1  |     | work_struct 2  |     | work_struct 3  | |
|  +-------+--------+     +-------+--------+     +-------+--------+ |
|          |                      |                      |          |
|          +----------------------+----------------------+          |
|                                 |                                 |
|                    +------------v------------+                    |
|                    |   workqueue_struct      |                    |
|                    |   (工作队列)             |                    |
|                    +------------+------------+                    |
|                                 |                                 |
|           +---------+-----------+-----------+---------+           |
|           |         |           |           |         |           |
|     +-----v---+ +---v-----+ +---v-----+ +---v-----+              |
|     |pool_wq | |pool_wq  | |pool_wq  | |pool_wq  |              |
|     | CPU 0  | | CPU 1   | | CPU 2   | | CPU 3   |              |
|     +----+---+ +----+----+ +----+----+ +----+----+              |
|          |          |          |          |                       |
|     +----v---+ +----v----+ +----v----+ +----v----+              |
|     |kworker| |kworker  | |kworker  | |kworker  |              |
|     +--------+ +---------+ +---------+ +---------+              |
|                                                                  |
+------------------------------------------------------------------+
```
[Intermediate]

---

## 8. Process Hierarchy (进程层次结构)

---

Q: How are processes organized in Linux?
A: 进程形成树形层次结构：

```
init (PID 1)
    |
    +---> systemd-journald
    |
    +---> systemd-udevd
    |
    +---> sshd
    |       |
    |       +---> sshd (session)
    |               |
    |               +---> bash
    |                       |
    |                       +---> vim
    |                       |
    |                       +---> ps
    |
    +---> nginx
            |
            +---> nginx (worker)
            |
            +---> nginx (worker)

内核视角（task_struct关系）：
+------------------+
| task_struct (A)  |
| real_parent --+  |     +------------------+
| parent ------+--+----->| task_struct (P)  |
| children ----------+   | (父进程)          |
| sibling ----------+|   +------------------+
+------------------+||
                   ||
                   |+---> list_head链接其他子进程
                   |
                   +---> list_head链接子进程链表
```

遍历进程：
```c
// 遍历所有进程
struct task_struct *p;
for_each_process(p) {
    printk("%s [%d]\n", p->comm, p->pid);
}

// 遍历线程
struct task_struct *t;
for_each_thread(p, t) {
    printk("  thread: %d\n", t->pid);
}

// 遍历子进程
struct task_struct *child;
list_for_each_entry(child, &parent->children, sibling) {
    printk("child: %s [%d]\n", child->comm, child->pid);
}

// 获取父进程
struct task_struct *parent = current->real_parent;

// 获取线程组leader
struct task_struct *leader = current->group_leader;
```
[Intermediate]

---

Q: What is the difference between PID, TGID, and PGID?
A: 
| ID类型 | 含义 | 获取方式 |
|--------|------|----------|
| PID | 进程/线程唯一标识 | getpid()/gettid() |
| TGID | 线程组ID（等于主线程PID） | getpid() |
| PGID | 进程组ID | getpgrp()/getpgid() |
| SID | 会话ID | getsid() |

```
+------------------------------------------------------------------+
|                Session (SID = 100)                                |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |           Process Group (PGID = 100)                       |  |
|  |  +-------------------------------------------------------+ |  |
|  |  |  Process (PID = 100, TGID = 100)  [leader]           | |  |
|  |  |     Thread (PID = 101, TGID = 100)                   | |  |
|  |  |     Thread (PID = 102, TGID = 100)                   | |  |
|  |  +-------------------------------------------------------+ |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |           Process Group (PGID = 200)                       |  |
|  |  +------------------------+ +---------------------------+  |  |
|  |  | Process (PID=200)      | | Process (PID=300)         |  |  |
|  |  | TGID=200               | | TGID=300                  |  |  |
|  |  +------------------------+ +---------------------------+  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

内核实现：
```c
// 获取各种ID
pid_t pid = task_pid_nr(current);     // PID (线程ID)
pid_t tgid = task_tgid_nr(current);   // TGID (进程ID)
pid_t pgid = task_pgrp_nr(current);   // PGID
pid_t sid = task_session_nr(current); // SID

// getpid()返回TGID，而非PID
SYSCALL_DEFINE0(getpid)
{
    return task_tgid_vnr(current);  // 返回TGID
}

// gettid()返回真正的PID
SYSCALL_DEFINE0(gettid)
{
    return task_pid_vnr(current);   // 返回PID
}
```
[Intermediate]

---

## 9. Process Groups and Sessions (进程组和会话)

---

Q: What are process groups and sessions?
A: 进程组和会话用于作业控制：

```
Terminal Session (会话)：
+------------------------------------------------------------------+
|  Session Leader (bash, SID=1000)                                 |
|  Controlling Terminal: /dev/pts/0                                |
|                                                                  |
|  Foreground Process Group (前台进程组):                          |
|  +------------------------------------------------------------+  |
|  | $ ls -l | grep foo | wc -l                                 |  |
|  | PGID = 1001                                                |  |
|  | [ls] [grep] [wc]  <- 收到SIGINT(Ctrl+C)                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Background Process Groups (后台进程组):                         |
|  +------------------------------------------------------------+  |
|  | $ make &                                                   |  |
|  | PGID = 1002                                                |  |
|  | [make]                                                     |  |
|  +------------------------------------------------------------+  |
|  +------------------------------------------------------------+  |
|  | $ sleep 100 &                                              |  |
|  | PGID = 1003                                                |  |
|  | [sleep]                                                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

会话和进程组操作：
```c
// 创建新会话
pid_t sid = setsid();  // 调用进程成为会话leader和进程组leader

// 设置进程组
setpgid(pid, pgid);

// 获取前台进程组
pid_t fg_pgid = tcgetpgrp(fd);

// 设置前台进程组
tcsetpgrp(fd, pgid);

// 内核实现
// kernel/sys.c
SYSCALL_DEFINE0(setsid)
{
    struct task_struct *p = current;
    
    // 不能是进程组leader
    if (p->pid != p->tgid)
        return -EPERM;
    
    // 创建新会话
    p->signal->leader = 1;
    set_special_pids(task_pid(p));
    
    // 脱离控制终端
    p->signal->tty = NULL;
    
    return task_pgrp_nr(p);
}
```
[Intermediate]

---

## 10. Namespaces (命名空间)

---

Q: What are Linux namespaces?
A: 命名空间提供资源隔离：

```
+------------------------------------------------------------------+
|                    Linux Namespaces                               |
+------------------------------------------------------------------+
|                                                                  |
|  +------------------+  +------------------+  +------------------+ |
|  |  PID Namespace   |  |  NET Namespace   |  |  MNT Namespace   | |
|  | 隔离进程ID空间    |  | 隔离网络栈       |  | 隔离文件系统挂载  | |
|  +------------------+  +------------------+  +------------------+ |
|                                                                  |
|  +------------------+  +------------------+  +------------------+ |
|  |  UTS Namespace   |  |  IPC Namespace   |  | USER Namespace   | |
|  | 隔离主机名/域名   |  | 隔离IPC资源      |  | 隔离用户/组ID    | |
|  +------------------+  +------------------+  +------------------+ |
|                                                                  |
|  +------------------+  +------------------+                      |
|  | CGROUP Namespace |  | TIME Namespace   |                      |
|  | 隔离cgroup视图   |  | 隔离时钟         |                      |
|  +------------------+  +------------------+                      |
|                                                                  |
+------------------------------------------------------------------+
```

命名空间操作：
```c
// 创建新命名空间
clone(child_func, stack, 
      CLONE_NEWPID |   // 新PID命名空间
      CLONE_NEWNET |   // 新网络命名空间
      CLONE_NEWNS |    // 新挂载命名空间
      CLONE_NEWUTS |   // 新UTS命名空间
      CLONE_NEWIPC |   // 新IPC命名空间
      CLONE_NEWUSER |  // 新用户命名空间
      CLONE_NEWCGROUP, // 新cgroup命名空间
      arg);

// unshare - 当前进程进入新命名空间
unshare(CLONE_NEWNS);

// setns - 加入已存在的命名空间
int fd = open("/proc/PID/ns/net", O_RDONLY);
setns(fd, CLONE_NEWNET);
close(fd);

// 内核数据结构
struct nsproxy {
    atomic_t count;
    struct uts_namespace *uts_ns;
    struct ipc_namespace *ipc_ns;
    struct mnt_namespace *mnt_ns;
    struct pid_namespace *pid_ns_for_children;
    struct net *net_ns;
    struct time_namespace *time_ns;
    struct time_namespace *time_ns_for_children;
    struct cgroup_namespace *cgroup_ns;
};
```

PID命名空间层次：
```
Host PID Namespace
├── Container1 PID NS
│   └── PID 1 (init)
│       └── PID 2 (app)  <- 在host中看到的是PID 12345
│
└── Container2 PID NS
    └── PID 1 (init)
        └── PID 2 (app)  <- 在host中看到的是PID 12346
```
[Advanced]

---

## 11. Cgroups (控制组)

---

Q: What are cgroups and how do they work?
A: Cgroups用于资源限制和统计：

```
+------------------------------------------------------------------+
|                    Cgroups v2 Hierarchy                          |
+------------------------------------------------------------------+
|                                                                  |
|  /sys/fs/cgroup/                                                 |
|  └── system.slice/                                               |
|      ├── docker-xxx.scope/                                       |
|      │   ├── cgroup.controllers (cpu memory io pids)            |
|      │   ├── cpu.max (quota period)                             |
|      │   ├── memory.max                                          |
|      │   ├── memory.current                                      |
|      │   ├── io.max                                              |
|      │   └── pids.max                                            |
|      │                                                           |
|      └── user.slice/                                             |
|          └── user-1000.slice/                                    |
|                                                                  |
+------------------------------------------------------------------+
```

Cgroup控制器：
```c
// 主要控制器
cpu       // CPU时间分配
cpuset    // CPU和内存节点绑定
memory    // 内存限制
io        // I/O带宽限制
pids      // 进程数限制
devices   // 设备访问控制

// 使用示例
// 创建cgroup
mkdir /sys/fs/cgroup/mygroup

// 限制内存
echo 100M > /sys/fs/cgroup/mygroup/memory.max

// 限制CPU
echo "50000 100000" > /sys/fs/cgroup/mygroup/cpu.max  // 50%

// 添加进程
echo $PID > /sys/fs/cgroup/mygroup/cgroup.procs

// 查看当前使用
cat /sys/fs/cgroup/mygroup/memory.current
cat /sys/fs/cgroup/mygroup/cpu.stat
```

内核数据结构：
```c
// include/linux/cgroup.h
struct cgroup {
    struct cgroup_subsys_state self;
    unsigned long flags;
    
    int id;
    int level;
    int max_depth;
    
    struct cgroup_root *root;
    struct cgroup *parent;
    
    struct kernfs_node *kn;          // sysfs节点
    
    struct cgroup_file procs_file;
    struct cgroup_file events_file;
    
    // 子系统状态
    struct cgroup_subsys_state __rcu *subsys[CGROUP_SUBSYS_COUNT];
    
    // 成员进程
    struct list_head cset_links;
    
    // ...
};

// 进程关联
struct task_struct {
    struct css_set __rcu *cgroups;   // cgroup集合
    struct list_head cg_list;        // cgroup成员链表
};
```
[Advanced]

---

## 12. Process Credentials (进程凭证)

---

Q: What are process credentials?
A: 凭证包含进程的身份和权限信息：

```c
// include/linux/cred.h
struct cred {
    atomic_t usage;
    
    /*=== 用户/组ID ===*/
    kuid_t uid;          // 真实用户ID
    kgid_t gid;          // 真实组ID
    kuid_t suid;         // 保存的用户ID
    kgid_t sgid;         // 保存的组ID
    kuid_t euid;         // 有效用户ID
    kgid_t egid;         // 有效组ID
    kuid_t fsuid;        // 文件系统用户ID
    kgid_t fsgid;        // 文件系统组ID
    
    /*=== 附加组 ===*/
    unsigned securebits;
    kernel_cap_t cap_inheritable;  // 可继承能力
    kernel_cap_t cap_permitted;    // 许可能力
    kernel_cap_t cap_effective;    // 有效能力
    kernel_cap_t cap_bset;         // 能力边界集
    kernel_cap_t cap_ambient;      // 环境能力
    
    /*=== 其他 ===*/
    struct user_struct *user;
    struct user_namespace *user_ns;
    struct group_info *group_info; // 附加组列表
    
    // ...
};
```

各种UID的区别：
```
+--------+------------+------------------------------------------+
|  类型  |   字段     |  用途                                    |
+--------+------------+------------------------------------------+
|  RUID  |  uid       | 真实用户ID，标识谁启动了进程             |
|  EUID  |  euid      | 有效用户ID，用于权限检查                 |
|  SUID  |  suid      | 保存的用户ID，用于恢复权限               |
| FSUID  |  fsuid     | 文件系统用户ID，用于文件权限检查         |
+--------+------------+------------------------------------------+

示例：passwd程序
- 普通用户执行passwd
- RUID = 普通用户UID
- EUID = 0 (setuid位)
- 可以修改/etc/shadow
```

能力(Capabilities)：
```c
// 细粒度权限控制，替代全有或全无的root权限
CAP_CHOWN           // 更改文件所有者
CAP_DAC_OVERRIDE    // 绕过文件权限检查
CAP_KILL            // 发送信号给任意进程
CAP_NET_ADMIN       // 网络管理
CAP_NET_BIND_SERVICE // 绑定特权端口(<1024)
CAP_NET_RAW         // 使用RAW socket
CAP_SYS_ADMIN       // 系统管理（危险！）
CAP_SYS_BOOT        // 重启系统
CAP_SYS_PTRACE      // ptrace任意进程

// 检查能力
if (capable(CAP_NET_ADMIN)) {
    // 允许网络管理操作
}

// 用户空间设置能力
setcap cap_net_bind_service+ep /path/to/program
```
[Intermediate]

---

## 13. Real-Time Scheduling (实时调度)

---

Q: What are SCHED_FIFO and SCHED_RR?
A: 实时调度策略：

```
+------------------------------------------------------------------+
|                   Real-Time Scheduling                            |
+------------------------------------------------------------------+
|                                                                  |
|  SCHED_FIFO (先进先出):                                          |
|  - 运行直到阻塞或主动让出                                         |
|  - 同优先级内FIFO顺序                                            |
|  - 没有时间片概念                                                 |
|                                                                  |
|  优先级 99: [Task A] ----运行直到完成或阻塞---->                  |
|  优先级 50: [Task B] [Task C]  等待                               |
|                                                                  |
|  SCHED_RR (时间片轮转):                                          |
|  - 同优先级任务轮流执行                                           |
|  - 用完时间片后排到队尾                                           |
|  - 默认时间片100ms                                                |
|                                                                  |
|  优先级 50: [Task A]--时间片-->[Task B]--时间片-->[Task A]...     |
|                                                                  |
+------------------------------------------------------------------+
```

设置实时调度：
```c
// 用户空间
struct sched_param param = {
    .sched_priority = 50  // 1-99
};
sched_setscheduler(pid, SCHED_FIFO, &param);
sched_setscheduler(pid, SCHED_RR, &param);

// 内核实现
// kernel/sched/rt.c
const struct sched_class rt_sched_class = {
    .next           = &fair_sched_class,
    .enqueue_task   = enqueue_task_rt,
    .dequeue_task   = dequeue_task_rt,
    .yield_task     = yield_task_rt,
    .pick_next_task = pick_next_task_rt,
    .task_tick      = task_tick_rt,
    // ...
};

// RT调度实体
struct sched_rt_entity {
    struct list_head run_list;       // 运行队列链表
    unsigned long timeout;           // RR超时
    unsigned long watchdog_stamp;
    unsigned int time_slice;         // 时间片
    unsigned short on_rq;
    unsigned short on_list;
    
    struct sched_rt_entity *back;
    struct sched_rt_entity *parent;
    struct rt_rq *rt_rq;
    struct rt_rq *my_q;
};
```

实时调度注意事项：
```c
// 1. 必须有适当权限
if (!capable(CAP_SYS_NICE))
    return -EPERM;

// 2. 限制RT任务的CPU使用
// /proc/sys/kernel/sched_rt_runtime_us  (默认950000us)
// /proc/sys/kernel/sched_rt_period_us   (默认1000000us)
// RT任务最多占用95%的CPU，留5%给普通任务

// 3. 使用sched_setaffinity绑定CPU避免影响其他CPU

// 4. 优先级反转问题
// 使用优先级继承互斥锁
pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
```
[Advanced]

---

## 14. CPU Affinity (CPU亲和性)

---

Q: How does CPU affinity work?
A: CPU亲和性控制进程在哪些CPU上运行：

```c
// 设置CPU亲和性
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(0, &mask);  // 只在CPU 0上运行
CPU_SET(2, &mask);  // 也可以在CPU 2上运行
sched_setaffinity(pid, sizeof(mask), &mask);

// 获取CPU亲和性
sched_getaffinity(pid, sizeof(mask), &mask);
for (int i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &mask))
        printf("Can run on CPU %d\n", i);
}

// 内核数据结构
struct task_struct {
    int nr_cpus_allowed;     // 允许的CPU数
    cpumask_t cpus_mask;     // CPU掩码
    // 调度器选择CPU时会检查这个掩码
};
```

内核实现：
```c
// kernel/sched/core.c
SYSCALL_DEFINE3(sched_setaffinity, pid_t, pid, unsigned int, len,
                unsigned long __user *, user_mask_ptr)
{
    cpumask_var_t new_mask;
    struct task_struct *p;
    
    if (copy_from_user(new_mask, user_mask_ptr, len))
        return -EFAULT;
    
    p = find_process_by_pid(pid);
    
    // 设置亲和性
    return __sched_setaffinity(p, new_mask);
}

static int __sched_setaffinity(struct task_struct *p, 
                               const struct cpumask *new_mask)
{
    // 检查权限
    if (!check_same_owner(p) && !capable(CAP_SYS_NICE))
        return -EPERM;
    
    // 更新掩码
    cpumask_copy(&p->cpus_mask, new_mask);
    p->nr_cpus_allowed = cpumask_weight(new_mask);
    
    // 如果当前CPU不在新掩码中，迁移
    if (!cpumask_test_cpu(task_cpu(p), new_mask))
        set_cpus_allowed_ptr(p, new_mask);
    
    return 0;
}
```

使用场景：
```bash
# 命令行设置
taskset -c 0,2 ./program       # 运行时设置
taskset -pc 0,2 $PID           # 运行中修改

# 隔离CPU（内核启动参数）
isolcpus=2,3   # 隔离CPU 2和3，调度器不会自动使用它们

# NUMA感知
numactl --cpunodebind=0 --membind=0 ./program
```
[Intermediate]

---

## 15. Debugging Processes (进程调试)

---

Q: How to debug processes in Linux?
A: 
```bash
# 查看进程信息
ps aux
ps -ef
ps -eLf                     # 显示线程
pstree -p                   # 进程树

# 查看进程详细信息
cat /proc/$PID/status       # 状态信息
cat /proc/$PID/stat         # 统计信息
cat /proc/$PID/maps         # 内存映射
cat /proc/$PID/fd           # 打开的文件
cat /proc/$PID/stack        # 内核栈

# 查看调度信息
cat /proc/$PID/sched        # 调度统计
chrt -p $PID                # 调度策略和优先级

# 系统级调度信息
cat /proc/sched_debug       # 调度器调试信息
cat /proc/schedstat         # 调度统计

# 追踪系统调用
strace -f -p $PID           # 追踪系统调用
ltrace -p $PID              # 追踪库函数

# 性能分析
perf top                    # 实时热点
perf record -p $PID         # 记录性能数据
perf sched record           # 记录调度事件
perf sched latency          # 调度延迟分析

# 查看上下文切换
vmstat 1                    # cs列
pidstat -w -p $PID 1        # 进程上下文切换
cat /proc/$PID/status | grep ctxt  # 上下文切换次数
```

内核调试：
```c
// 添加调试打印
pr_debug("process %s state change: %ld -> %ld\n",
         current->comm, old_state, new_state);

// 使用trace_printk（高性能）
trace_printk("context switch: %d -> %d\n", prev->pid, next->pid);

// 使用ftrace
echo function_graph > /sys/kernel/debug/tracing/current_tracer
echo schedule > /sys/kernel/debug/tracing/set_ftrace_filter
cat /sys/kernel/debug/tracing/trace

// 使用kprobe
echo 'p:myprobe do_fork' > /sys/kernel/debug/tracing/kprobe_events
echo 1 > /sys/kernel/debug/tracing/events/kprobes/myprobe/enable
```
[Basic]

---

## 16. Summary (总结)

---

Q: Summarize Linux process management architecture.
A: 
```
+==================================================================+
||                LINUX PROCESS MANAGEMENT                        ||
+==================================================================+

核心数据结构:
+------------------+
| task_struct      | <-- 进程描述符（~6KB）
+--------+---------+
         |
         +---> mm_struct (内存管理)
         |
         +---> files_struct (打开文件)
         |
         +---> signal_struct (信号处理)
         |
         +---> cred (凭证/权限)
         |
         +---> nsproxy (命名空间)
         |
         +---> sched_entity (调度实体)


进程生命周期:
    fork/clone
        |
        v
    +---+---+       schedule()
    |RUNNING|<-------------------+
    +---+---+                    |
        |                        |
        |    +------------+      |
        +--->|INTERRUPTIBLE|-----+
        |    +------------+      |
        |                        |
        |    +-------------+     |
        +--->|UNINTERRUPTIBLE|---+
        |    +-------------+
        |
        |    exit()
        v
    +---+---+
    |ZOMBIE | ---> wait() ---> 销毁
    +-------+


调度架构:
    +------------------+
    |  stop_sched_class|  最高优先级
    +--------+---------+
             |
    +--------v---------+
    |   dl_sched_class |  SCHED_DEADLINE
    +--------+---------+
             |
    +--------v---------+
    |   rt_sched_class |  SCHED_FIFO/RR
    +--------+---------+
             |
    +--------v---------+
    |  fair_sched_class|  SCHED_NORMAL (CFS)
    +--------+---------+
             |
    +--------v---------+
    | idle_sched_class |  最低优先级
    +------------------+


资源隔离:
    Namespaces          Cgroups
    +-------------+     +-------------+
    | PID         |     | CPU限制     |
    | Network     |     | 内存限制    |
    | Mount       |     | I/O限制     |
    | UTS         |     | 进程数限制   |
    | IPC         |     | 设备访问    |
    | User        |     |             |
    +-------------+     +-------------+


关键API:
    fork() / clone()     创建进程/线程
    exec()               执行程序
    exit() / wait()      终止/等待
    schedule()           调度
    sched_setscheduler() 设置调度策略
    setpriority()        设置优先级
    sched_setaffinity()  设置CPU亲和性
```
[Basic]

---

*Total: 120+ cards covering Linux kernel process management*
