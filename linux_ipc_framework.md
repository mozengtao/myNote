# Linux 进程间通信框架深入讲解

基于 Linux 3.2 内核源码分析

---

## 目录

- [IPC 机制概述](#ipc-机制概述)
- [管道 (Pipe)](#管道-pipe)
- [System V IPC](#system-v-ipc)
- [POSIX IPC](#posix-ipc)
- [信号 (Signal)](#信号-signal)
- [Unix 域 Socket](#unix-域-socket)
- [共享内存映射 (mmap)](#共享内存映射-mmap)
- [关键源码文件](#关键源码文件)

---

## IPC 机制概述

### IPC 机制分类

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Linux IPC 机制                                      │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                        数据传输类                                      │  │
│  │                                                                        │  │
│  │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │  │
│  │   │    管道     │  │ 消息队列    │  │ Unix Socket │  │   Socket    │  │  │
│  │   │   (pipe)    │  │ (msgqueue)  │  │   (AF_UNIX) │  │  (AF_INET)  │  │  │
│  │   └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                        共享内存类                                      │  │
│  │                                                                        │  │
│  │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                   │  │
│  │   │  共享内存    │  │    mmap     │  │   tmpfs     │                   │  │
│  │   │   (shm)     │  │  (匿名映射)  │  │ (/dev/shm)  │                   │  │
│  │   └─────────────┘  └─────────────┘  └─────────────┘                   │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                        同步机制类                                      │  │
│  │                                                                        │  │
│  │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                   │  │
│  │   │   信号量    │  │    futex    │  │   文件锁    │                   │  │
│  │   │ (semaphore) │  │             │  │   (flock)   │                   │  │
│  │   └─────────────┘  └─────────────┘  └─────────────┘                   │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                        通知机制类                                      │  │
│  │                                                                        │  │
│  │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                   │  │
│  │   │    信号     │  │  eventfd    │  │  signalfd   │                   │  │
│  │   │  (signal)   │  │             │  │             │                   │  │
│  │   └─────────────┘  └─────────────┘  └─────────────┘                   │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### IPC 机制对比

| 机制 | 通信范围 | 数据流向 | 同步 | 特点 |
|------|---------|---------|------|------|
| 管道 | 父子进程 | 单向 | 隐式 | 简单，内核缓冲 |
| 命名管道 | 任意进程 | 单向 | 隐式 | 需要文件系统 |
| 消息队列 | 任意进程 | 双向 | 隐式 | 消息边界，优先级 |
| 共享内存 | 任意进程 | 双向 | 显式 | 最快，需同步 |
| 信号量 | 任意进程 | N/A | 同步 | 用于同步 |
| 信号 | 任意进程 | 单向 | 异步 | 通知，有限数据 |
| Socket | 任意/网络 | 双向 | 隐式 | 最灵活 |

---

## 管道 (Pipe)

### 管道结构

```c
// include/linux/pipe_fs_i.h
struct pipe_inode_info {
    wait_queue_head_t wait;             // 等待队列
    unsigned int nrbufs, curbuf;        // 缓冲区数和当前位置
    unsigned int buffers;               // 缓冲区总数
    unsigned int readers;               // 读者计数
    unsigned int writers;               // 写者计数
    unsigned int waiting_writers;       // 等待的写者
    struct pipe_buffer *bufs;           // 缓冲区数组
    struct fasync_struct *fasync_readers;
    struct fasync_struct *fasync_writers;
};

struct pipe_buffer {
    struct page *page;                  // 数据页
    unsigned int offset;                // 页内偏移
    unsigned int len;                   // 数据长度
    const struct pipe_buf_operations *ops;
    unsigned int flags;
};
```

### 管道工作原理

```
                         管道内核缓冲区
                              │
        ┌─────────────────────┴─────────────────────┐
        │                                           │
        ▼                                           │
┌──────────────────────────────────────────────────────────────────────────┐
│  pipe_buffer[0] │ pipe_buffer[1] │ ... │ pipe_buffer[PIPE_BUFFERS-1]    │
│       ┌───┐     │      ┌───┐     │     │         ┌───┐                  │
│       │Page│     │      │Page│     │     │         │Page│                  │
│       └───┘     │      └───┘     │     │         └───┘                  │
└──────────────────────────────────────────────────────────────────────────┘
        ↑                                           ↑
        │                                           │
    curbuf (读位置)                            nrbufs (写位置)

写进程 (write)                              读进程 (read)
      │                                          │
      ▼                                          ▼
  pipe_write()                              pipe_read()
      │                                          │
      ├── 等待空间可用                           ├── 等待数据可用
      │   (如果满)                               │   (如果空)
      │                                          │
      ├── 复制数据到缓冲区                       ├── 从缓冲区复制数据
      │                                          │
      └── 唤醒读者                               └── 唤醒写者
```

### pipe() 创建流程

```
pipe(int fd[2])
      │
      ▼
sys_pipe()
      │
      ▼
do_pipe2()
      │
      ├── 分配 pipe_inode_info
      │
      ├── 分配缓冲区页面
      │
      ├── 创建两个文件描述符
      │   fd[0] ── 读端
      │   fd[1] ── 写端
      │
      └── 设置 file_operations
          .read  = pipe_read
          .write = pipe_write
          .poll  = pipe_poll
```

### 命名管道 (FIFO)

```c
// 创建
mkfifo("/tmp/myfifo", 0666);  // 或 mknod

// 打开
int fd = open("/tmp/myfifo", O_RDONLY);  // 读端
int fd = open("/tmp/myfifo", O_WRONLY);  // 写端

// 底层: 使用相同的 pipe_inode_info
// 区别: 有文件系统路径，可在无关进程间使用
```

---

## System V IPC

### 通用 IPC 结构

```c
// include/linux/ipc.h
struct kern_ipc_perm {
    spinlock_t      lock;
    int             deleted;
    int             id;                 // IPC 标识符
    key_t           key;                // 用户提供的 key
    uid_t           uid;                // 所有者 UID
    gid_t           gid;                // 所有者 GID
    uid_t           cuid;               // 创建者 UID
    gid_t           cgid;               // 创建者 GID
    mode_t          mode;               // 权限
    unsigned long   seq;                // 序列号
};
```

### 共享内存 (SHM)

```c
// ipc/shm.c
struct shmid_kernel {
    struct kern_ipc_perm    shm_perm;
    struct file             *shm_file;      // 关联的文件 (shmem)
    unsigned long           shm_nattch;     // 连接计数
    unsigned long           shm_segsz;      // 段大小
    time_t                  shm_atim;       // 最后连接时间
    time_t                  shm_dtim;       // 最后断开时间
    time_t                  shm_ctim;       // 最后修改时间
    pid_t                   shm_cprid;      // 创建者 PID
    pid_t                   shm_lprid;      // 最后操作者 PID
    // ...
};
```

#### 共享内存使用流程

```
进程 A                                进程 B
   │                                     │
   ▼                                     │
shmget(key, size, IPC_CREAT)            │
   │                                     │
   │  创建共享内存段                     │
   │  返回 shmid                         │
   │                                     │
   ▼                                     ▼
shmat(shmid, NULL, 0)              shmget(key, 0, 0)
   │                                     │
   │  映射到进程地址空间                 │  获取 shmid
   │  返回地址 ptr_a                     │
   │                                     ▼
   │                               shmat(shmid, NULL, 0)
   │                                     │
   │                                     │  映射到进程地址空间
   │                                     │  返回地址 ptr_b
   │                                     │
   ▼                                     ▼
*ptr_a = data   ─────────────────►   data = *ptr_b
   │             (共享同一物理内存)       │
   ▼                                     ▼
shmdt(ptr_a)                        shmdt(ptr_b)
   │                                     │
   │  解除映射                           │  解除映射
   │                                     │
   ▼                                     │
shmctl(shmid, IPC_RMID, NULL)           │
   │                                     │
   └─────────────────────────────────────┘
               删除共享内存
```

#### 共享内存内核实现

```
shmget()
    │
    ▼
newseg()  ── 创建 shmid_kernel
    │
    ├── 分配 shmid_kernel 结构
    │
    └── shmem_file_setup()  ── 创建 tmpfs 文件
            │
            └── 用于实际存储共享内存数据

shmat()
    │
    ▼
do_shmat()
    │
    ├── 获取 shmid_kernel
    │
    └── do_mmap()  ── 映射到进程地址空间
            │
            ├── 创建 VMA
            │
            └── 设置 vm_ops = shm_vm_ops

实际页面分配: 缺页时通过 shmem_fault() 分配
```

### 消息队列 (MSG)

```c
// ipc/msg.c
struct msg_queue {
    struct kern_ipc_perm q_perm;
    time_t q_stime;             // 最后发送时间
    time_t q_rtime;             // 最后接收时间
    time_t q_ctime;             // 最后修改时间
    unsigned long q_cbytes;     // 当前字节数
    unsigned long q_qnum;       // 消息数
    unsigned long q_qbytes;     // 最大字节数
    pid_t q_lspid;              // 最后发送者 PID
    pid_t q_lrpid;              // 最后接收者 PID
    struct list_head q_messages; // 消息链表
    struct list_head q_receivers; // 等待接收者
    struct list_head q_senders;   // 等待发送者
};

struct msg_msg {
    struct list_head m_list;
    long m_type;                // 消息类型
    int m_ts;                   // 消息大小
    struct msg_msgseg *next;    // 分段消息
    void *security;
    // 消息数据紧随其后
};
```

#### 消息队列操作

```
发送进程                              接收进程
    │                                    │
    ▼                                    ▼
msgsnd(msqid, &msg, len, 0)        msgrcv(msqid, &msg, len, type, 0)
    │                                    │
    ▼                                    ▼
do_msgsnd()                        do_msgrcv()
    │                                    │
    ├── 检查权限和限制                   ├── 检查权限
    │                                    │
    ├── 分配 msg_msg 结构               ├── 查找匹配消息
    │                                    │   (按 type 匹配)
    ├── 复制用户数据                     │
    │                                    ├── 如果无消息:
    ├── 加入消息链表                     │   ├── IPC_NOWAIT → 返回错误
    │                                    │   └── 否则 → 睡眠等待
    └── 唤醒等待的接收者                 │
                                         ├── 复制到用户空间
                                         │
                                         └── 从链表移除消息
```

### 信号量 (SEM)

```c
// ipc/sem.c
struct sem_array {
    struct kern_ipc_perm    sem_perm;
    time_t                  sem_otime;      // 最后操作时间
    time_t                  sem_ctime;      // 最后修改时间
    struct sem              *sem_base;      // 信号量数组
    struct list_head        list_id;
    int                     sem_nsems;      // 信号量个数
    int                     complex_count;  // 复杂操作计数
};

struct sem {
    int semval;             // 信号量值
    int sempid;             // 最后操作的 PID
    struct list_head sem_pending; // 等待队列
};
```

#### 信号量操作

```c
// 获取/创建信号量集
int semid = semget(key, nsems, IPC_CREAT | 0666);

// 信号量操作
struct sembuf sops[] = {
    { 0, -1, 0 },   // 信号量0减1 (P操作/等待)
    { 1, +1, 0 },   // 信号量1加1 (V操作/释放)
};
semop(semid, sops, 2);

// 控制操作
semctl(semid, 0, SETVAL, 1);  // 设置值
semctl(semid, 0, GETVAL);     // 获取值
semctl(semid, 0, IPC_RMID);   // 删除
```

---

## POSIX IPC

### POSIX vs System V

| 特性 | System V | POSIX |
|------|----------|-------|
| 命名 | key_t 数值 | 字符串路径 |
| API | msgget/msgsnd | mq_open/mq_send |
| 同步 | semop | sem_wait |
| 实现 | 内核 | 内核/用户空间 |

### POSIX 消息队列

```c
// ipc/mqueue.c
struct mqueue_inode_info {
    spinlock_t lock;
    struct inode vfs_inode;
    
    struct msg_msg **messages;      // 消息数组
    struct mq_attr attr;            // 队列属性
    
    struct sigevent notify;         // 异步通知
    struct pid *notify_owner;
    struct user_struct *user;
};

// 使用示例
mqd_t mq = mq_open("/myqueue", O_CREAT | O_RDWR, 0666, &attr);
mq_send(mq, msg, len, priority);
mq_receive(mq, buf, len, &priority);
mq_close(mq);
mq_unlink("/myqueue");
```

### POSIX 信号量

```c
// 命名信号量 (在 /dev/shm 中)
sem_t *sem = sem_open("/mysem", O_CREAT, 0666, 1);
sem_wait(sem);      // P 操作
sem_post(sem);      // V 操作
sem_close(sem);
sem_unlink("/mysem");

// 无名信号量 (共享内存中)
sem_t sem;
sem_init(&sem, 1, 1);  // pshared=1 表示进程间共享
sem_wait(&sem);
sem_post(&sem);
sem_destroy(&sem);
```

### POSIX 共享内存

```c
// 创建/打开 (在 /dev/shm 中)
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, size);

// 映射
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

// 使用...

// 清理
munmap(ptr, size);
close(fd);
shm_unlink("/myshm");
```

---

## 信号 (Signal)

### 信号结构

```c
// include/linux/sched.h (task_struct 中)
struct signal_struct {
    atomic_t        sigcnt;
    atomic_t        live;
    wait_queue_head_t wait_chldexit;
    
    struct sigpending shared_pending;   // 线程组共享信号
    
    // 限制
    struct rlimit rlim[RLIM_NLIMITS];
    
    // ...
};

// include/linux/signal.h
struct sigpending {
    struct list_head list;              // 待处理信号链表
    sigset_t signal;                    // 待处理信号位图
};

struct sigqueue {
    struct list_head list;
    int flags;
    siginfo_t info;                     // 信号详细信息
    struct user_struct *user;
};
```

### 信号处理流程

```
发送信号                                    接收信号
    │                                           │
    ▼                                           │
kill(pid, sig)                                  │
    │                                           │
    ▼                                           │
sys_kill()                                      │
    │                                           │
    ▼                                           │
do_send_sig_info()                              │
    │                                           │
    ├── 权限检查                                 │
    │                                           │
    └── __send_signal()                         │
            │                                   │
            ├── 分配 sigqueue                   │
            │                                   │
            ├── 加入 pending 链表               │
            │                                   │
            ├── 设置 sigpending 位图            │
            │                                   │
            └── signal_wake_up()                │
                    │                           │
                    └── 唤醒目标进程            │
                            │                   │
                            └───────────────────┘
                                                │
                              从内核返回用户空间前
                                                │
                                                ▼
                                    do_signal()
                                                │
                                        ┌───────┴───────┐
                                        │               │
                                        ▼               ▼
                                    有处理函数?     默认处理
                                        │               │
                                        ▼               ▼
                                handle_signal()    do_signal_stop()
                                        │           或 do_group_exit()
                                        │
                                        ▼
                                设置用户栈帧
                                跳转到处理函数
                                        │
                                        ▼
                                执行 handler(sig)
                                        │
                                        ▼
                                sigreturn() 返回
```

### 常用信号

```c
#define SIGHUP      1   // 终端挂起
#define SIGINT      2   // 中断 (Ctrl+C)
#define SIGQUIT     3   // 退出 (Ctrl+\)
#define SIGILL      4   // 非法指令
#define SIGABRT     6   // 异常终止
#define SIGFPE      8   // 浮点异常
#define SIGKILL     9   // 强制终止 (不可捕获)
#define SIGSEGV    11   // 段错误
#define SIGPIPE    13   // 管道破裂
#define SIGALRM    14   // 定时器
#define SIGTERM    15   // 终止请求
#define SIGCHLD    17   // 子进程状态改变
#define SIGSTOP    19   // 停止 (不可捕获)
#define SIGCONT    18   // 继续
#define SIGUSR1    10   // 用户定义1
#define SIGUSR2    12   // 用户定义2
```

---

## Unix 域 Socket

### 结构

```c
// include/net/af_unix.h
struct unix_sock {
    struct sock         sk;
    struct unix_address *addr;
    struct path         path;           // 绑定的文件路径
    struct mutex        readlock;
    struct sock         *peer;          // 对端 socket
    struct sock         *other;
    struct list_head    link;
    atomic_long_t       inflight;
    spinlock_t          lock;
    wait_queue_head_t   peer_wait;
};
```

### Unix Socket 类型

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Unix 域 Socket                                     │
│                                                                              │
│   SOCK_STREAM (类似 TCP):                                                    │
│   ┌─────────┐                     ┌─────────┐                               │
│   │ Server  │◄───────────────────►│ Client  │                               │
│   │ socket  │    连接的 socket    │ socket  │                               │
│   └─────────┘                     └─────────┘                               │
│      bind()                         connect()                                │
│      listen()                                                                │
│      accept() ─► 新 socket                                                   │
│                                                                              │
│   SOCK_DGRAM (类似 UDP):                                                     │
│   ┌─────────┐                     ┌─────────┐                               │
│   │ Server  │◄─────────────────────│ Client  │                               │
│   │ socket  │    数据报            │ socket  │                               │
│   └─────────┘                     └─────────┘                               │
│      bind()                         sendto()                                 │
│      recvfrom()                                                              │
│                                                                              │
│   SOCK_SEQPACKET (有序可靠数据报):                                           │
│   - 连接式                                                                   │
│   - 保持消息边界                                                             │
│   - 可靠传输                                                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Unix Socket 特性

```c
// 优势:
// 1. 比本地 TCP 更高效 (无需网络协议栈)
// 2. 支持传递文件描述符
// 3. 支持传递凭证 (PID, UID, GID)

// 传递文件描述符
struct msghdr msg;
struct cmsghdr *cmsg;
char buf[CMSG_SPACE(sizeof(int))];

msg.msg_control = buf;
msg.msg_controllen = sizeof(buf);

cmsg = CMSG_FIRSTHDR(&msg);
cmsg->cmsg_level = SOL_SOCKET;
cmsg->cmsg_type = SCM_RIGHTS;
cmsg->cmsg_len = CMSG_LEN(sizeof(int));
*((int *)CMSG_DATA(cmsg)) = fd;

sendmsg(sock, &msg, 0);
```

---

## 共享内存映射 (mmap)

### 映射类型

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           mmap 映射类型                                      │
│                                                                              │
│   MAP_SHARED (共享映射):                                                     │
│   ┌─────────┐     ┌─────────┐     ┌─────────┐                              │
│   │ 进程 A  │────►│ 物理页  │◄────│ 进程 B  │                              │
│   │ VMA     │     │  (共享) │     │ VMA     │                              │
│   └─────────┘     └─────────┘     └─────────┘                              │
│   - 修改对其他进程可见                                                       │
│   - 修改会写回文件 (文件映射时)                                              │
│                                                                              │
│   MAP_PRIVATE (私有映射):                                                    │
│   ┌─────────┐     ┌─────────┐                                               │
│   │ 进程 A  │────►│ 物理页  │  (写时复制)                                   │
│   │ VMA     │     │  (共享) │                                               │
│   └─────────┘     └─────────┘                                               │
│                         │                                                    │
│                    写操作 ▼                                                  │
│   ┌─────────┐     ┌─────────┐     ┌─────────┐                              │
│   │ 进程 A  │────►│ 副本    │     │ 原页面  │◄──── 其他进程                  │
│   │ VMA     │     │  (私有) │     │  (共享) │                              │
│   └─────────┘     └─────────┘     └─────────┘                              │
│                                                                              │
│   MAP_ANONYMOUS (匿名映射):                                                  │
│   - 不关联文件                                                               │
│   - 用于进程间共享内存 (fork 后子进程共享)                                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 共享匿名映射用于 IPC

```c
// 父子进程共享内存
void *ptr = mmap(NULL, size, 
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS,
                  -1, 0);

pid_t pid = fork();
if (pid == 0) {
    // 子进程: 可以访问 ptr
    ((int *)ptr)[0] = 42;
} else {
    // 父进程: 等待并读取
    wait(NULL);
    printf("%d\n", ((int *)ptr)[0]);  // 输出 42
}
```

### /dev/shm 和 tmpfs

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         /dev/shm (tmpfs)                                     │
│                                                                              │
│   特点:                                                                       │
│   - 文件存储在内存中                                                          │
│   - 支持文件系统操作                                                          │
│   - 自动清理 (系统重启)                                                       │
│   - POSIX 共享内存的后端                                                      │
│                                                                              │
│   使用方式:                                                                   │
│   1. 创建文件: open("/dev/shm/mydata", O_CREAT | O_RDWR)                     │
│   2. 设置大小: ftruncate(fd, size)                                           │
│   3. 映射:     mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)     │
│                                                                              │
│   等价于 shm_open() + mmap()                                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 关键源码文件

### 管道

| 文件 | 功能 |
|------|------|
| `fs/pipe.c` | 管道实现 |
| `include/linux/pipe_fs_i.h` | 管道结构定义 |

### System V IPC

| 文件 | 功能 |
|------|------|
| `ipc/util.c` | IPC 通用工具 |
| `ipc/shm.c` | 共享内存 |
| `ipc/msg.c` | 消息队列 |
| `ipc/sem.c` | 信号量 |
| `ipc/namespace.c` | IPC 命名空间 |

### POSIX IPC

| 文件 | 功能 |
|------|------|
| `ipc/mqueue.c` | POSIX 消息队列 |
| `mm/shmem.c` | tmpfs/共享内存 |

### 信号

| 文件 | 功能 |
|------|------|
| `kernel/signal.c` | 信号处理 |
| `arch/x86/kernel/signal.c` | x86 信号处理 |

### Unix Socket

| 文件 | 功能 |
|------|------|
| `net/unix/af_unix.c` | Unix 域 socket |

### 同步原语

| 文件 | 功能 |
|------|------|
| `kernel/futex.c` | futex |
| `fs/eventfd.c` | eventfd |
| `fs/signalfd.c` | signalfd |

---

## 总结

### IPC 选择指南

| 场景 | 推荐机制 | 原因 |
|------|---------|------|
| 父子进程简单通信 | 管道 | 简单高效 |
| 大量数据共享 | 共享内存 | 零拷贝 |
| 小消息传递 | 消息队列 | 消息边界 |
| 本地网络编程风格 | Unix Socket | 灵活、高效 |
| 进程同步 | 信号量/futex | 专用同步 |
| 异步通知 | 信号/eventfd | 事件驱动 |

### 性能对比

```
共享内存 > Unix Socket > 管道 > 消息队列
 (最快)                            (最慢)
   ↑                                  ↑
零拷贝                           内核复制
```

---

*本文档基于 Linux 3.2 内核源码分析*

