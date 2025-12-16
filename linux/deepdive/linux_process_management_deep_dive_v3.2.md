# Linux Kernel Process Management Deep Dive (v3.2)
## A Code-Level Walkthrough of Process Creation, Execution, and Termination

---

## Table of Contents
1. [Subsystem Context (Big Picture)](#1-subsystem-context-big-picture)
2. [Directory & File Map](#2-directory--file-map)
3. [Core Data Structures](#3-core-data-structures)
4. [Entry Points & Call Paths](#4-entry-points--call-paths)
5. [Core Workflows](#5-core-workflows)
6. [Important Algorithms & Mechanisms](#6-important-algorithms--mechanisms)
7. [Concurrency & Synchronization](#7-concurrency--synchronization)
8. [Performance Considerations](#8-performance-considerations)
9. [Common Pitfalls & Bugs](#9-common-pitfalls--bugs)
10. [How to Read This Code Yourself](#10-how-to-read-this-code-yourself)
11. [Summary & Mental Model](#11-summary--mental-model)
12. [What to Study Next](#12-what-to-study-next)

---

## 1. Subsystem Context (Big Picture)

### What Kernel Subsystem Are We Studying?

The **Process Management** subsystem handles the complete lifecycle of processes (and threads) in Linux:
- **Creation**: `fork()`, `clone()`, `vfork()`, kernel threads
- **Execution**: `exec()` family, loading programs
- **Termination**: `exit()`, signal-based termination, zombie reaping
- **Identity**: PIDs, TGIDs, process groups, sessions
- **Hierarchy**: Parent-child relationships, orphan handling

### What Problem Does It Solve?

1. **Process Abstraction**: Provides the fundamental abstraction of an executing program
2. **Resource Isolation**: Each process has its own address space, file descriptors, credentials
3. **Sharing Control**: Fine-grained control over what parent/child share (via clone flags)
4. **Process Hierarchy**: Maintains parent-child tree for signal delivery and wait()
5. **Thread Support**: Enables multiple execution contexts sharing resources (CLONE_THREAD)
6. **Namespace Isolation**: Foundation for containers (PID, network, mount namespaces)

### Where It Sits in the Overall Kernel Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER SPACE                                      │
│                                                                              │
│    fork()    clone()    vfork()    execve()    exit()    wait()             │
│       │         │          │          │          │         │                 │
└───────┼─────────┼──────────┼──────────┼──────────┼─────────┼─────────────────┘
        │         │          │          │          │         │
        └─────────┴──────────┴─────┬────┴──────────┴─────────┘
                                   │ System Call Interface
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              KERNEL SPACE                                    │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                    PROCESS MANAGEMENT SUBSYSTEM                        │ │
│  │                                                                        │ │
│  │   ┌─────────────────────────────────────────────────────────────────┐  │ │
│  │   │                     PROCESS TREE                                │  │ │
│  │   │                                                                 │  │ │
│  │   │                      init (PID 1)                               │  │ │
│  │   │                    ┌────┴─────┐                                 │  │ │
│  │   │                    │          │                                 │  │ │
│  │   │                 bash        systemd                             │  │ │
│  │   │               ┌──┴──┐      ┌──┴──┐                              │  │ │
│  │   │              vim   grep   nginx  ...                            │  │ │
│  │   │                          ┌──┴──┐                                │  │ │
│  │   │                      worker worker (threads)                    │  │ │
│  │   └─────────────────────────────────────────────────────────────────┘  │ │
│  │                                                                        │ │
│  │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                │ │
│  │   │   fork.c     │  │   exit.c     │  │   exec.c     │                │ │
│  │   │  copy_process│  │   do_exit    │  │  do_execve   │                │ │
│  │   │  do_fork     │  │   wait       │  │  load_binary │                │ │
│  │   └──────────────┘  └──────────────┘  └──────────────┘                │ │
│  │                                                                        │ │
│  │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                │ │
│  │   │   pid.c      │  │  kthread.c   │  │  signal.c    │                │ │
│  │   │  PID alloc   │  │ kernel thread│  │  delivery    │                │ │
│  │   └──────────────┘  └──────────────┘  └──────────────┘                │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                            │                                                 │
│         ┌──────────────────┼──────────────────┐                             │
│         ▼                  ▼                  ▼                             │
│  ┌─────────────┐   ┌─────────────┐    ┌─────────────┐                       │
│  │  Scheduler  │   │   Memory    │    │   File      │                       │
│  │ (task runs) │   │ Management  │    │  System     │                       │
│  │             │   │ (mm_struct) │    │(files_struct)│                       │
│  └─────────────┘   └─────────────┘    └─────────────┘                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

**图解说明：**
- 进程管理是用户空间程序的基石，处理进程的创建、执行和终止
- 核心文件：fork.c（进程创建）、exit.c（进程终止）、exec.c（程序加载）
- 进程树以init为根，所有用户进程都是init的后代
- 与调度器（决定谁运行）、内存管理（mm_struct）、文件系统（files_struct）紧密交互
- PID子系统负责进程标识符的分配和命名空间隔离

### How This Subsystem Interacts with Others

| Subsystem | Interaction |
|-----------|-------------|
| **Scheduler** | fork() creates task_struct, scheduler decides when it runs |
| **Memory (mm)** | fork() duplicates/shares address space (COW), exec() replaces it |
| **VFS/Files** | fork() shares/duplicates file descriptors |
| **Signals** | exit() sends SIGCHLD, signal delivery affects task state |
| **Namespaces** | clone() can create new PID/network/mount namespaces |
| **Credentials** | fork() inherits credentials, exec() may change them |
| **Cgroups** | Processes belong to cgroups, inherit on fork |

---

## 2. Directory & File Map

```
kernel/
│
├── fork.c               → Process creation: do_fork(), copy_process()
│                          SLAB caches for task_struct, mm_struct, etc.
│                          ~1773 lines - the "birth" of processes
│
├── exit.c               → Process termination: do_exit(), do_group_exit()
│                          wait() family, zombie reaping, exit_notify()
│                          ~1862 lines - the "death" of processes
│
├── exec.c               → Program execution: do_execve()
│                          Binary format handlers
│                          ~1700 lines (in fs/exec.c)
│
├── pid.c                → PID allocation and management
│                          pid_namespace support, struct pid operations
│                          ~572 lines
│
├── pid_namespace.c      → PID namespace creation/destruction
│                          ~200 lines
│
├── kthread.c            → Kernel thread creation: kthread_create()
│                          kthreadd daemon
│                          ~444 lines
│
├── signal.c             → Signal handling (interacts heavily with process mgmt)
│                          send_signal(), get_signal_to_deliver()
│                          ~3000+ lines
│
├── ptrace.c             → Process tracing (debugging support)
│                          PTRACE_* operations
│                          ~900 lines
│
├── sys.c                → Various syscalls: setpgid(), setsid(), getpid(), etc.
│
├── cred.c               → Process credentials management
│
└── nsproxy.c            → Namespace proxy management

include/linux/
│
├── sched.h              → Core structures: task_struct, signal_struct
│                          Task states, clone flags, scheduler interface
│
├── pid.h                → struct pid, struct upid, pid_type enum
│
├── pid_namespace.h      → struct pid_namespace
│
├── kthread.h            → Kernel thread API
│
└── init_task.h          → init_task definition (PID 0/1)

fs/
│
└── exec.c               → Actual execve implementation, binary formats

arch/<arch>/kernel/
│
├── process.c            → Architecture-specific: copy_thread(), context switching
│
└── entry_*.S            → System call entry points (fork, clone, exit)
```

### Why Is the Code Split This Way?

1. **fork.c** - Process birth: contains all logic for duplicating a process. This is complex because it must handle selective sharing of resources via clone flags.

2. **exit.c** - Process death: handles resource cleanup, zombie creation, and parent notification. Includes wait() implementation because waiting and exit are tightly coupled.

3. **exec.c** (in fs/) - Lives in fs/ because program loading is fundamentally about reading executable files and setting up memory mappings.

4. **pid.c** - Separated because PID allocation is a distinct concern with its own data structures (bitmap-based allocation, namespace support).

5. **kthread.c** - Kernel threads are special (no user-space component, different creation path), so they get their own file.

---

## 3. Core Data Structures

### 3.1 The Process Descriptor (task_struct)

The `task_struct` is the most important data structure in Linux - it represents a process/thread.

```c
// include/linux/sched.h, lines 1220-1550 (excerpt)

struct task_struct {
    /*======================= STATE & IDENTITY =======================*/
    volatile long state;           // Task state: TASK_RUNNING, etc.
    void *stack;                   // Kernel stack pointer
    atomic_t usage;                // Reference count
    unsigned int flags;            // PF_* flags (PF_EXITING, etc.)
    unsigned int ptrace;           // Ptrace flags

    /*======================= SCHEDULING =======================*/
    int on_rq;                     // On runqueue?
    int prio, static_prio, normal_prio;
    unsigned int rt_priority;
    const struct sched_class *sched_class;
    struct sched_entity se;        // CFS scheduling entity
    struct sched_rt_entity rt;     // RT scheduling entity
    unsigned int policy;           // SCHED_NORMAL, SCHED_FIFO, etc.
    cpumask_t cpus_allowed;        // CPU affinity mask

    /*======================= PROCESS IDs =======================*/
    pid_t pid;                     // Process ID (unique per-task)
    pid_t tgid;                    // Thread Group ID (== leader's pid)
    struct pid_link pids[PIDTYPE_MAX]; // Links to struct pid

    /*======================= PROCESS TREE =======================*/
    struct task_struct *real_parent; // Biological parent
    struct task_struct *parent;      // Parent for signals (may differ if ptraced)
    struct list_head children;       // List of my children
    struct list_head sibling;        // Link in parent's children list
    struct task_struct *group_leader; // Thread group leader

    /*======================= THREADING =======================*/
    struct list_head thread_group;   // All threads in this thread group

    /*======================= MEMORY =======================*/
    struct mm_struct *mm;           // User address space (NULL for kernel threads)
    struct mm_struct *active_mm;    // Active mm (kernel threads borrow)

    /*======================= FILES & FS =======================*/
    struct fs_struct *fs;           // Filesystem info (cwd, root)
    struct files_struct *files;     // Open file descriptors

    /*======================= NAMESPACES =======================*/
    struct nsproxy *nsproxy;        // Namespace proxy

    /*======================= SIGNALS =======================*/
    struct signal_struct *signal;   // Shared signal state (thread group)
    struct sighand_struct *sighand; // Signal handlers (shared by threads)
    sigset_t blocked, real_blocked; // Blocked signals
    struct sigpending pending;      // Private pending signals

    /*======================= CREDENTIALS =======================*/
    const struct cred __rcu *real_cred; // Objective credentials
    const struct cred __rcu *cred;      // Subjective credentials

    /*======================= MISC =======================*/
    char comm[TASK_COMM_LEN];       // Executable name
    int exit_state;                 // EXIT_ZOMBIE, EXIT_DEAD
    int exit_code, exit_signal;
    struct thread_struct thread;    // CPU-specific state (registers, etc.)
    // ... many more fields
};
```

**内存布局图：**

```
                          task_struct (~4KB, SLAB allocated)
┌─────────────────────────────────────────────────────────────────────────────┐
│ state = TASK_RUNNING         │ usage (refcount) = 2                         │
├──────────────────────────────┴──────────────────────────────────────────────┤
│                                                                             │
│  pid = 1234                   tgid = 1234      ← 主线程: pid == tgid       │
│                                                                             │
│  OR for a thread:                                                           │
│  pid = 1235                   tgid = 1234      ← 工作线程: pid != tgid     │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                           PROCESS TREE LINKS                                │
│                                                                             │
│  real_parent ──────────────────────► parent's task_struct                   │
│  parent ───────────────────────────► parent (or ptracer)                    │
│  children ─────────────────────────► list_head of my children               │
│  sibling ──────────────────────────► next/prev in parent's children         │
│  group_leader ─────────────────────► thread group leader task_struct        │
│  thread_group ─────────────────────► circular list of threads               │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                           SHARED RESOURCES                                  │
│                                                                             │
│  mm ───────────────────────────────► mm_struct (address space)              │
│  files ────────────────────────────► files_struct (open files)              │
│  fs ───────────────────────────────► fs_struct (cwd, root)                  │
│  signal ───────────────────────────► signal_struct (shared in thread group) │
│  sighand ──────────────────────────► sighand_struct (signal handlers)       │
│  nsproxy ──────────────────────────► nsproxy (namespaces)                   │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  stack ────────────────────────────► thread_info + kernel stack (8KB/16KB)  │
│                                                                             │
│         ┌──────────────────────────────────────────────────┐                │
│         │ KERNEL STACK                                      │ HIGH          │
│         │    (grows down)                                   │ ADDR          │
│         │         ↓                                         │               │
│         │                                                   │               │
│         │                                                   │               │
│         │         ↑                                         │               │
│         │    struct thread_info                             │               │
│         │    (at bottom of stack)                           │ LOW           │
│         └──────────────────────────────────────────────────┘ ADDR          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**task_struct分配：**
```c
// kernel/fork.c:112-115
# define alloc_task_struct_node(node)
    kmem_cache_alloc_node(task_struct_cachep, GFP_KERNEL, node)
```
使用SLAB分配器，因为task_struct频繁分配/释放且大小固定。

### 3.2 Thread Group and Threads

**Linux线程模型**：Linux中线程就是共享资源的进程，使用相同的task_struct。

```
                    Thread Group (Process with 3 threads)

     Thread Group Leader (tgid=1000, pid=1000)
    ┌──────────────────────────────────────────────────────────┐
    │  task_struct                                              │
    │  pid = 1000, tgid = 1000                                  │
    │  group_leader = self                                      │
    │  signal ─────────────────┐                                │
    │  sighand ────────────────┼───┐                            │
    │  mm ─────────────────────┼───┼───┐                        │
    │  files ──────────────────┼───┼───┼───┐                    │
    │  thread_group: ◄────┐    │   │   │   │                    │
    └─────────────────────┼────┴───┴───┴───┴────────────────────┘
                          │         │   │   │
                          │    ┌────┘   │   │
                          │    ▼        │   │
     Thread 2 (pid=1001)  │  ┌─────────────────────────────────────┐
    ┌─────────────────────┼──│  signal_struct (SHARED)             │
    │  task_struct        │  │  - nr_threads = 3                   │
    │  pid = 1001         │  │  - shared_pending (signals to group)│
    │  tgid = 1000  ◄─────┼──│  - group_exit_code                  │
    │  group_leader ──────┼──└─────────────────────────────────────┘
    │  signal ────────────┘        │   │
    │  sighand ────────────────────┘   │
    │  mm ─────────────────────────────┘   (SHARED)
    │  files ──────────────────────────────┘
    │  thread_group: ◄────┐
    └─────────────────────┼────────────────────────────────────────┘
                          │
                          │
     Thread 3 (pid=1002)  │
    ┌─────────────────────┼────────────────────────────────────────┐
    │  task_struct        │                                        │
    │  pid = 1002         │                                        │
    │  tgid = 1000        │  (All point to same shared resources)  │
    │  group_leader ──────┼──► Leader's task_struct                │
    │  signal, sighand, mm, files ───► (SHARED)                    │
    │  thread_group: ─────┘                                        │
    └──────────────────────────────────────────────────────────────┘

Key insight:
- getpid() returns tgid (all threads see same PID)
- gettid() returns pid (unique per thread)
- kill(pid) delivers to thread group
- tkill(tid) delivers to specific thread
```

### 3.3 The PID Structure

```c
// include/linux/pid.h:57-65

struct pid
{
    atomic_t count;                    // Reference count
    unsigned int level;                // Deepest namespace level
    struct hlist_head tasks[PIDTYPE_MAX]; // Lists of tasks using this PID
    struct rcu_head rcu;               // For RCU-safe freeing
    struct upid numbers[1];            // Array of (nr, namespace) pairs
};

struct upid {
    int nr;                            // PID number in this namespace
    struct pid_namespace *ns;          // The namespace
    struct hlist_node pid_chain;       // Hash chain for lookup
};

enum pid_type {
    PIDTYPE_PID,    // Process ID (unique per task)
    PIDTYPE_PGID,   // Process Group ID
    PIDTYPE_SID,    // Session ID
    PIDTYPE_MAX
};
```

**PID命名空间可视化：**

```
                    PID Namespace Hierarchy

                      init_pid_ns (level 0)
                      ┌─────────────────────────────────────┐
                      │  PID 1 = init                       │
                      │  PID 2 = kthreadd                   │
                      │  PID 1000 = container_init          │──┐
                      │  PID 1001 = container_process_A     │  │
                      │  ...                                │  │
                      └─────────────────────────────────────┘  │
                                                               │
                                                               ▼
                      Container PID namespace (level 1)
                      ┌─────────────────────────────────────┐
                      │  PID 1 = container_init (!)         │
                      │  PID 2 = container_process_A        │
                      │  (These are the SAME tasks seen     │
                      │   with different PIDs)              │
                      └─────────────────────────────────────┘

struct pid for container_init:
┌──────────────────────────────────────────────────────────────┐
│  count = 2                                                   │
│  level = 1  (exists in 2 namespaces: level 0 and 1)          │
│  tasks[PIDTYPE_PID] ──► list of task_structs                 │
│  numbers[0] = { nr=1000, ns=&init_pid_ns }   ← 宿主机视角     │
│  numbers[1] = { nr=1, ns=container_pid_ns } ← 容器内视角     │
└──────────────────────────────────────────────────────────────┘
```

### 3.4 Signal Structures

```c
// include/linux/sched.h:526-606

struct signal_struct {
    atomic_t        sigcnt;              // Reference count
    atomic_t        live;                // Number of live threads
    int             nr_threads;          // Total threads in group

    wait_queue_head_t wait_chldexit;     // For wait4() - parent waits here

    /* Thread group signal handling */
    struct task_struct *curr_target;     // Current signal target
    struct sigpending  shared_pending;   // Signals pending to entire group

    /* Thread group exit support */
    int             group_exit_code;
    int             notify_count;
    struct task_struct *group_exit_task;
    int             group_stop_count;
    unsigned int    flags;               // SIGNAL_STOP_STOPPED, etc.

    /* Timers and accounting (shared by all threads) */
    struct hrtimer real_timer;           // ITIMER_REAL
    struct pid *leader_pid;
    cputime_t utime, stime, cutime, cstime; // CPU time accounting
    unsigned long nvcsw, nivcsw;         // Context switch counts
    unsigned long min_flt, maj_flt;      // Page fault counts
    
    struct tty_struct *tty;              // Controlling terminal
    // ...
};

struct sighand_struct {
    atomic_t        count;
    struct k_sigaction action[_NSIG];    // Signal handlers array
    spinlock_t      siglock;             // Protects this and pending signals
    wait_queue_head_t signalfd_wqh;
};
```

**信号数据结构关系：**

```
Thread Group (3 threads)

┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ task_struct  │     │ task_struct  │     │ task_struct  │
│   (Thread 1) │     │   (Thread 2) │     │   (Thread 3) │
│              │     │              │     │              │
│ pending ──┐  │     │ pending ──┐  │     │ pending ──┐  │
│           │  │     │           │  │     │           │  │
│ (private  │  │     │ (private  │  │     │ (private  │  │
│  signals) │  │     │  signals) │  │     │  signals) │  │
│           ▼  │     │           ▼  │     │           ▼  │
│ ┌────────┐   │     │ ┌────────┐   │     │ ┌────────┐   │
│ │ SIGUSR1│   │     │ │ (empty)│   │     │ │ SIGALRM│   │
│ └────────┘   │     │ └────────┘   │     │ └────────┘   │
│              │     │              │     │              │
│ signal ──────┼─────┼──────────────┼─────┼──┐           │
│ sighand ─────┼─────┼──────────────┼─────┼──┼───┐       │
└──────────────┘     └──────────────┘     └──┼───┼───────┘
                                             │   │
      ┌──────────────────────────────────────┘   │
      ▼                                          │
┌─────────────────────────────────────────┐      │
│         signal_struct (SHARED)          │      │
│                                         │      │
│  shared_pending:                        │      │
│  ┌────────┐ ┌────────┐                  │      │
│  │ SIGINT │ │SIGTERM │  (to any thread) │      │
│  └────────┘ └────────┘                  │      │
│                                         │      │
│  wait_chldexit: (parents wait here)     │      │
│  nr_threads = 3                         │      │
│  live = 3                               │      │
└─────────────────────────────────────────┘      │
                                                 │
      ┌──────────────────────────────────────────┘
      ▼
┌─────────────────────────────────────────┐
│       sighand_struct (SHARED)           │
│                                         │
│  action[SIGINT]  = { sa_handler = ... } │
│  action[SIGTERM] = { sa_handler = ... } │
│  action[SIGUSR1] = { sa_handler = ... } │
│  ...                                    │
│  siglock: (protects all pending queues) │
└─────────────────────────────────────────┘

Signal delivery rules:
- shared_pending: 发送给整个线程组的信号（如kill(pid)）
- pending (private): 发送给特定线程的信号（如pthread_kill）
- 任何非阻塞该信号的线程都可能处理shared_pending中的信号
```

### 3.5 Memory Descriptor (mm_struct)

```c
// include/linux/mm_types.h (simplified)

struct mm_struct {
    struct vm_area_struct *mmap;        // VMA list head
    struct rb_root mm_rb;               // VMA red-black tree
    
    pgd_t *pgd;                         // Page Global Directory (page tables)
    atomic_t mm_users;                  // How many users (sharing via clone)
    atomic_t mm_count;                  // Reference count
    
    int map_count;                      // Number of VMAs
    struct rw_semaphore mmap_sem;       // Protects VMA list/tree
    
    unsigned long start_code, end_code;
    unsigned long start_data, end_data;
    unsigned long start_brk, brk;       // Heap boundaries
    unsigned long start_stack;
    unsigned long arg_start, arg_end;
    unsigned long env_start, env_end;
    
    // ... many more fields
};
```

---

## 4. Entry Points & Call Paths

### 4.1 Key Entry Points

| Entry Point | Trigger | Purpose |
|-------------|---------|---------|
| `do_fork()` | fork(), clone(), vfork() syscalls | Create new process/thread |
| `do_execve()` | execve() syscall | Replace process image |
| `do_exit()` | exit() syscall, fatal signal, kernel errors | Terminate process |
| `do_group_exit()` | exit_group() syscall | Terminate all threads |
| `do_wait()` | wait(), waitpid(), wait4() | Wait for child state change |
| `kernel_thread()` | Kernel internal | Create kernel thread |
| `kthread_create()` | Kernel modules/drivers | Create kernel thread (preferred) |

### 4.2 The Fork Path (do_fork → copy_process)

```
User calls fork()/clone()/vfork()
            │
            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  sys_fork() / sys_clone() / sys_vfork()         [arch/x86/kernel/process.c]│
│       │                                                                   │
│       └── do_fork(clone_flags, ...)             [kernel/fork.c:1461]      │
│            │                                                              │
│            ├── Validate clone_flags                                       │
│            │   • CLONE_THREAD requires CLONE_SIGHAND                      │
│            │   • CLONE_SIGHAND requires CLONE_VM                          │
│            │   • Various sanity checks                                    │
│            │                                                              │
│            ├── copy_process(clone_flags, ...)   [kernel/fork.c:1042]      │
│            │   │                                                          │
│            │   │  ┌───────────────────────────────────────────────────┐   │
│            │   │  │ PHASE 1: Allocate and Initialize task_struct     │   │
│            │   │  └───────────────────────────────────────────────────┘   │
│            │   ├── dup_task_struct(current)                               │
│            │   │   ├── alloc_task_struct_node()   ← SLAB分配             │
│            │   │   ├── alloc_thread_info_node()   ← 内核栈               │
│            │   │   ├── arch_dup_task_struct()     ← 复制父进程task       │
│            │   │   └── setup_thread_stack()       ← 设置thread_info      │
│            │   │                                                          │
│            │   │  ┌───────────────────────────────────────────────────┐   │
│            │   │  │ PHASE 2: Copy/Share Resources (based on flags)   │   │
│            │   │  └───────────────────────────────────────────────────┘   │
│            │   ├── copy_creds()          ← 凭证（通常复制）               │
│            │   ├── sched_fork()          ← 调度器初始化                   │
│            │   ├── copy_files()          ← CLONE_FILES ? 共享 : 复制     │
│            │   ├── copy_fs()             ← CLONE_FS ? 共享 : 复制        │
│            │   ├── copy_sighand()        ← CLONE_SIGHAND ? 共享 : 复制   │
│            │   ├── copy_signal()         ← CLONE_THREAD ? 共享 : 新建    │
│            │   ├── copy_mm()             ← CLONE_VM ? 共享 : 复制(COW)   │
│            │   ├── copy_namespaces()     ← CLONE_NEW* ? 新建 : 共享      │
│            │   ├── copy_io()             ← I/O上下文                     │
│            │   └── copy_thread()         ← 架构相关，设置寄存器          │
│            │   │                                                          │
│            │   │  ┌───────────────────────────────────────────────────┐   │
│            │   │  │ PHASE 3: Allocate PID and Link into Trees        │   │
│            │   │  └───────────────────────────────────────────────────┘   │
│            │   ├── alloc_pid()           ← 分配PID                        │
│            │   │                                                          │
│            │   ├── write_lock_irq(&tasklist_lock)  ← 锁定进程树         │
│            │   │                                                          │
│            │   ├── (Set up parent/child relationships)                   │
│            │   │   • p->real_parent = current (or current->real_parent)  │
│            │   │   • list_add_tail(&p->sibling, &parent->children)       │
│            │   │   • list_add_tail_rcu(&p->tasks, &init_task.tasks)      │
│            │   │                                                          │
│            │   ├── (Set up thread group if CLONE_THREAD)                 │
│            │   │   • p->group_leader = current->group_leader             │
│            │   │   • list_add_tail_rcu(&p->thread_group, ...)            │
│            │   │                                                          │
│            │   ├── attach_pid(p, PIDTYPE_PID/PGID/SID)                   │
│            │   │                                                          │
│            │   └── write_unlock_irq(&tasklist_lock)                      │
│            │                                                              │
│            │   Returns: new task_struct pointer (or ERR_PTR on failure)  │
│            │                                                              │
│            ├── (If vfork: initialize completion)                         │
│            │                                                              │
│            ├── wake_up_new_task(p)          ← 唤醒新进程，加入调度器      │
│            │                                                              │
│            └── (If vfork: wait_for_completion() until child execs/exits) │
│                                                                           │
│            Returns: child's PID to parent, 0 to child                    │
└───────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Clone Flags Reference

```c
// include/linux/sched.h:7-31

#define CLONE_VM        0x00000100  // 共享虚拟内存（线程必需）
#define CLONE_FS        0x00000200  // 共享文件系统信息（cwd, root, umask）
#define CLONE_FILES     0x00000400  // 共享文件描述符表
#define CLONE_SIGHAND   0x00000800  // 共享信号处理程序
#define CLONE_THREAD    0x00010000  // 同一线程组（共享PID）
#define CLONE_NEWNS     0x00020000  // 新建挂载命名空间
#define CLONE_NEWUTS    0x04000000  // 新建UTS命名空间
#define CLONE_NEWIPC    0x08000000  // 新建IPC命名空间
#define CLONE_NEWUSER   0x10000000  // 新建用户命名空间
#define CLONE_NEWPID    0x20000000  // 新建PID命名空间
#define CLONE_NEWNET    0x40000000  // 新建网络命名空间
#define CLONE_VFORK     0x00004000  // 父进程等待子进程exec/exit
#define CLONE_PARENT    0x00008000  // 与父进程同级（共享父进程）
```

**常见组合：**
```
fork()  = clone(SIGCHLD, ...)
          创建独立进程，只复制，不共享

vfork() = clone(CLONE_VFORK | CLONE_VM | SIGCHLD, ...)
          共享地址空间，父进程阻塞（用于立即exec）

pthread_create() ≈ clone(CLONE_VM | CLONE_FS | CLONE_FILES | 
                         CLONE_SIGHAND | CLONE_THREAD | 
                         CLONE_SYSVSEM | CLONE_SETTLS | ...)
          完全线程：共享几乎所有资源
```

### 4.4 The Exit Path (do_exit)

```
Process calls exit()/exit_group() or receives fatal signal
            │
            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  do_exit(exit_code)                               [kernel/exit.c:891]     │
│       │                                                                   │
│       ├── 检查：不能在中断上下文退出                                       │
│       ├── 检查：不能杀死idle进程（PID 0）                                  │
│       ├── 设置 PF_EXITING 标志                                           │
│       │                                                                   │
│       │  ┌───────────────────────────────────────────────────┐           │
│       │  │ PHASE 1: Cleanup Shared Resources                 │           │
│       │  └───────────────────────────────────────────────────┘           │
│       ├── exit_signals(tsk)        ← 设置PF_EXITING，处理信号            │
│       │                                                                   │
│       ├── group_dead = atomic_dec_and_test(&tsk->signal->live)           │
│       │   // 检查是否是线程组最后一个成员                                  │
│       │                                                                   │
│       ├── exit_mm(tsk)             ← 释放mm_struct (如果最后一个用户)    │
│       ├── exit_sem(tsk)            ← 释放System V信号量                  │
│       ├── exit_shm(tsk)            ← 释放共享内存                        │
│       ├── exit_files(tsk)          ← 释放文件描述符表                    │
│       ├── exit_fs(tsk)             ← 释放fs_struct                       │
│       ├── exit_thread()            ← 架构相关清理                        │
│       │                                                                   │
│       │  ┌───────────────────────────────────────────────────┐           │
│       │  │ PHASE 2: Notify Parent and Become Zombie         │           │
│       │  └───────────────────────────────────────────────────┘           │
│       ├── exit_notify(tsk, group_dead)                                   │
│       │   │                                                              │
│       │   ├── forget_original_parent(tsk)                                │
│       │   │   // 把所有子进程过继给init或线程组其他成员                   │
│       │   │   for each child:                                            │
│       │   │       if (child's group dead) → reparent to init             │
│       │   │       else → reparent to thread group leader                 │
│       │   │                                                              │
│       │   ├── write_lock_irq(&tasklist_lock)                             │
│       │   │                                                              │
│       │   ├── do_notify_parent(tsk, sig)                                 │
│       │   │   // 发送SIGCHLD给父进程                                      │
│       │   │   // 唤醒在wait_chldexit上等待的父进程                        │
│       │   │                                                              │
│       │   ├── tsk->exit_state = EXIT_ZOMBIE (or EXIT_DEAD)               │
│       │   │   // 如果父进程已设置SA_NOCLDWAIT或信号被忽略: EXIT_DEAD      │
│       │   │   // 否则: EXIT_ZOMBIE，等待父进程wait()                      │
│       │   │                                                              │
│       │   ├── write_unlock_irq(&tasklist_lock)                           │
│       │   │                                                              │
│       │   └── if (autoreap) release_task(tsk)                            │
│       │       // EXIT_DEAD状态直接释放，不等待父进程                      │
│       │                                                                   │
│       │  ┌───────────────────────────────────────────────────┐           │
│       │  │ PHASE 3: Final Schedule - Never Returns          │           │
│       │  └───────────────────────────────────────────────────┘           │
│       ├── tsk->state = TASK_DEAD                                         │
│       ├── schedule()               ← 调度到其他进程，永不返回            │
│       └── BUG()                    ← 如果返回了，说明内核有bug            │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

### 4.5 The Wait Path

```
Parent calls wait()/waitpid()/wait4()
            │
            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  do_wait(wo)                                     [kernel/exit.c]          │
│       │                                                                   │
│       └── Loop: do_wait_thread(wo, tsk) for each child                   │
│            │                                                              │
│            ├── Check if child matches (pid, options)                     │
│            │                                                              │
│            ├── wait_consider_task(wo, tsk)                               │
│            │   │                                                         │
│            │   ├── if (tsk->exit_state == EXIT_ZOMBIE)                   │
│            │   │   └── wait_task_zombie(wo, tsk)                         │
│            │   │       ├── Collect exit_code and rusage                  │
│            │   │       ├── Copy to user space (wo->wo_stat, wo->wo_rusage)│
│            │   │       └── release_task(tsk)  ← 彻底释放task_struct      │
│            │   │           ├── __exit_signal()  (update parent's stats) │
│            │   │           ├── __unhash_process()  (remove from tables) │
│            │   │           ├── put_task_struct()  (decrement refcount)  │
│            │   │           └── → free_task() when refcount hits 0       │
│            │   │                                                         │
│            │   ├── if (tsk->exit_state == EXIT_DEAD)                    │
│            │   │   └── Skip (already reaped)                            │
│            │   │                                                         │
│            │   └── if ((options & WNOHANG) && no state change)          │
│            │       └── Return immediately                                │
│            │                                                              │
│            └── If no child ready and !(WNOHANG):                         │
│                └── schedule() on signal->wait_chldexit                   │
│                    (sleep until child exits or is stopped)               │
│                                                                           │
│  Returns: child's PID (or 0 or -1 on error)                              │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Core Workflows

### 5.1 Process Creation: fork() Detailed

```c
// kernel/fork.c - Simplified copy_process() flow

static struct task_struct *copy_process(unsigned long clone_flags, ...)
{
    struct task_struct *p;
    
    /* 1. Validate clone flags */
    if ((clone_flags & CLONE_THREAD) && !(clone_flags & CLONE_SIGHAND))
        return ERR_PTR(-EINVAL);
    if ((clone_flags & CLONE_SIGHAND) && !(clone_flags & CLONE_VM))
        return ERR_PTR(-EINVAL);
        
    /* 2. Allocate new task_struct (copy from parent) */
    p = dup_task_struct(current);
    
    /* 3. Initialize fields that shouldn't be inherited */
    p->did_exec = 0;
    p->utime = p->stime = 0;  // Reset CPU time
    INIT_LIST_HEAD(&p->children);
    INIT_LIST_HEAD(&p->sibling);
    
    /* 4. Copy or share resources based on flags */
    sched_fork(p);                        // 总是执行 - 调度器初始化
    copy_files(clone_flags, p);           // CLONE_FILES控制
    copy_fs(clone_flags, p);              // CLONE_FS控制
    copy_sighand(clone_flags, p);         // CLONE_SIGHAND控制
    copy_signal(clone_flags, p);          // CLONE_THREAD控制
    copy_mm(clone_flags, p);              // CLONE_VM控制（最复杂！）
    copy_thread(clone_flags, stack_start, stack_size, p, regs);
    
    /* 5. Allocate PID */
    pid = alloc_pid(p->nsproxy->pid_ns);
    p->pid = pid_nr(pid);
    p->tgid = (clone_flags & CLONE_THREAD) ? current->tgid : p->pid;
    
    /* 6. Link into process tree (under tasklist_lock) */
    write_lock_irq(&tasklist_lock);
    
    if (clone_flags & (CLONE_PARENT|CLONE_THREAD))
        p->real_parent = current->real_parent;
    else
        p->real_parent = current;
        
    if (clone_flags & CLONE_THREAD) {
        p->group_leader = current->group_leader;
        list_add_tail_rcu(&p->thread_group, &p->group_leader->thread_group);
        current->signal->nr_threads++;
    } else {
        p->group_leader = p;
        list_add_tail(&p->sibling, &p->real_parent->children);
        list_add_tail_rcu(&p->tasks, &init_task.tasks);  // 全局进程列表
    }
    
    attach_pid(p, PIDTYPE_PID, pid);
    
    write_unlock_irq(&tasklist_lock);
    
    return p;
}
```

### 5.2 Copy-on-Write Memory (copy_mm)

```c
// kernel/fork.c - copy_mm() and dup_mmap()

static int copy_mm(unsigned long clone_flags, struct task_struct *tsk)
{
    struct mm_struct *mm, *oldmm = current->mm;
    
    /* Kernel threads don't have mm */
    if (!oldmm) {
        tsk->mm = NULL;
        return 0;
    }
    
    /* CLONE_VM: Share the address space (threads) */
    if (clone_flags & CLONE_VM) {
        atomic_inc(&oldmm->mm_users);
        tsk->mm = oldmm;
        return 0;
    }
    
    /* Fork: Create new mm with COW mappings */
    mm = dup_mm(tsk);  // Allocate new mm_struct
    tsk->mm = mm;
    return 0;
}

// dup_mmap() copies all VMAs but marks pages as read-only (COW)
static int dup_mmap(struct mm_struct *mm, struct mm_struct *oldmm)
{
    for (mpnt = oldmm->mmap; mpnt; mpnt = mpnt->vm_next) {
        // Allocate new VMA, copy properties
        tmp = kmem_cache_alloc(vm_area_cachep, GFP_KERNEL);
        *tmp = *mpnt;  // Copy VMA metadata
        
        // Copy page table entries (this is where COW magic happens)
        copy_page_range(mm, oldmm, mpnt);
        // Pages are marked read-only in BOTH parent and child
        // First write triggers page fault → cow_user_page()
    }
}
```

**COW流程图：**

```
                      BEFORE FORK
    ┌──────────────────────────────────────────────────────────┐
    │  Parent Process (mm_struct A)                            │
    │                                                          │
    │  VMA: [0x400000 - 0x401000] (code, r-x)                 │
    │  VMA: [0x7000000 - 0x7001000] (data, rw-)               │
    │       │                                                  │
    │       └────────► Physical Page 0x1234 (actual data)     │
    │                  PTE flags: rw-                          │
    └──────────────────────────────────────────────────────────┘
    
                      AFTER FORK (COW)
    ┌──────────────────────────────────────────────────────────┐
    │  Parent Process (mm_struct A)                            │
    │  VMA: [0x7000000 - 0x7001000] (data, rw-)               │
    │       │                                                  │
    │       └────────┬─► Physical Page 0x1234                 │
    │                │   PTE flags: r-- (READ ONLY!)          │
    │                │   page->_count = 2                      │
    │                │                                         │
    ├────────────────┼────────────────────────────────────────┤
    │  Child Process (mm_struct B)                             │
    │  VMA: [0x7000000 - 0x7001000] (data, rw-)               │
    │       │                                                  │
    │       └────────┘                                         │
    └──────────────────────────────────────────────────────────┘
    
                      AFTER CHILD WRITES
    ┌──────────────────────────────────────────────────────────┐
    │  Parent Process                                          │
    │  VMA: [0x7000000] ────────► Physical Page 0x1234        │
    │                             (now exclusive, rw-)         │
    │                             page->_count = 1             │
    │                                                          │
    ├──────────────────────────────────────────────────────────┤
    │  Child Process                                           │
    │  VMA: [0x7000000] ────────► Physical Page 0x5678 (NEW)  │
    │                             (copy of data, rw-)          │
    │                             page->_count = 1             │
    │                                                          │
    │  Write fault triggered cow_user_page():                  │
    │  1. Allocate new page                                    │
    │  2. Copy data from original                              │
    │  3. Update PTE to point to new page                      │
    │  4. Make both pages writable                             │
    └──────────────────────────────────────────────────────────┘
```

### 5.3 Process Termination (do_exit) Detailed

```c
// kernel/exit.c - do_exit() simplified

NORET_TYPE void do_exit(long code)
{
    struct task_struct *tsk = current;
    int group_dead;
    
    /* Can't exit from interrupt context or as idle task */
    if (unlikely(in_interrupt()))
        panic("Aiee, killing interrupt handler!");
    if (unlikely(!tsk->pid))
        panic("Attempted to kill the idle task!");
    
    /* Set PF_EXITING to prevent signals and other threads */
    exit_signals(tsk);
    
    /* Check if we're the last thread in the group */
    group_dead = atomic_dec_and_test(&tsk->signal->live);
    
    /* Release resources */
    exit_mm(tsk);           // Release memory (complex!)
    exit_files(tsk);        // Close file descriptors
    exit_fs(tsk);           // Release fs_struct
    
    /* Notify parent, reparent children, become zombie */
    exit_notify(tsk, group_dead);
    
    /* Final schedule, never returns */
    tsk->state = TASK_DEAD;
    schedule();
    BUG();  /* Should never reach here */
}
```

### 5.4 Zombie Process Lifecycle

```
                    PROCESS LIFECYCLE

    fork()           execve()           exit()           wait()
      │                │                  │                │
      ▼                ▼                  ▼                ▼
┌─────────┐      ┌─────────┐        ┌─────────┐      ┌─────────┐
│ Created │─────►│ Running │───────►│ Zombie  │─────►│  Reaped │
│  (R)    │      │  (R/S)  │        │   (Z)   │      │ (freed) │
└─────────┘      └─────────┘        └─────────┘      └─────────┘
     │                │                  │
     │                │                  │
     │           Signal (SIGKILL/       │
     │           SIGTERM) or            │
     │           natural exit           │
     │                │                  │
     │                ▼                  │
     │          do_exit():               │
     │          - Release resources      │
     │          - Reparent children      │
     │          - Send SIGCHLD           │
     │          - exit_state = ZOMBIE    │
     │          - schedule() away        │
     │                                   │
     │                                   │
     └───────────────────────────────────┘
                                    Parent does
                                    wait()/waitpid():
                                    - release_task()
                                    - Free task_struct
                                    - Free PID


ZOMBIE STATE (exit_state = EXIT_ZOMBIE):
┌─────────────────────────────────────────────────────────────────┐
│  Zombie task_struct                                             │
│                                                                 │
│  • mm = NULL (released)                                        │
│  • files = NULL (closed)                                        │
│  • Most resources freed                                         │
│                                                                 │
│  Still retained:                                                │
│  • task_struct itself (~4KB)                                    │
│  • PID (cannot be reused)                                       │
│  • exit_code (for parent to collect)                           │
│  • Resource usage statistics                                    │
│  • Link in process tree                                         │
│                                                                 │
│  Why zombie exists:                                             │
│  Parent needs to collect exit status via wait()                │
│  Prevents PID reuse until parent acknowledges child death      │
└─────────────────────────────────────────────────────────────────┘

ORPHAN HANDLING (parent dies before child):
┌─────────────────────────────────────────────────────────────────┐
│  forget_original_parent() in exit_notify():                     │
│                                                                 │
│  1. For each child of dying process:                           │
│     - If child's thread group is dead: reparent to init        │
│     - Else: reparent to surviving thread in group              │
│                                                                 │
│  2. Any zombie children get adopted by init                    │
│     init will wait() on them → proper cleanup                  │
│                                                                 │
│  This prevents zombie accumulation when parent dies            │
│  without calling wait()                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 PID Allocation (Bitmap-based)

```c
// kernel/pid.c - alloc_pid() uses bitmap for O(1) allocation

/*
 * PID分配使用位图，每个位代表一个PID是否已分配
 * 默认最大PID = 32768 (可通过/proc/sys/kernel/pid_max调整)
 * 位图大小 = 32768 / 8 = 4KB (一页)
 */

struct pidmap {
    atomic_t nr_free;        // 该页中空闲PID数
    void *page;              // 位图页
};

struct pid_namespace {
    struct pidmap pidmap[PIDMAP_ENTRIES];  // 位图数组
    int last_pid;                          // 上次分配的PID
    struct task_struct *child_reaper;      // 该命名空间的init进程
    // ...
};

// 分配算法
struct pid *alloc_pid(struct pid_namespace *ns)
{
    struct pid *pid;
    
    // 从last_pid开始找下一个空闲位
    pid->numbers[ns->level].nr = alloc_pidmap(ns);
    
    // 递归为所有祖先命名空间分配PID
    for (i = ns->level - 1; i >= 0; i--) {
        pid->numbers[i].nr = alloc_pidmap(ns->parent);
    }
    
    // 插入哈希表
    hlist_add_head_rcu(&pid->numbers[i].pid_chain, &pid_hash[...]);
    
    return pid;
}
```

**位图可视化：**

```
PID Bitmap (假设pid_max = 64 for simplicity)
┌────────────────────────────────────────────────────────────────────┐
│ Bit: 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 ... 63       │
│      1  1  1  1  0  1  1  0  1  0  0  0  0  0  0  0     0        │
│      │  │  │  │     │  │     │                                   │
│      │  │  │  │     │  │     └─ PID 8 (allocated)                │
│      │  │  │  │     │  └─────── PID 6 (allocated)                │
│      │  │  │  │     └────────── PID 5 (allocated)                │
│      │  │  │  └──────────────── PID 3 (allocated)                │
│      │  │  └─────────────────── PID 2 (kthreadd)                 │
│      │  └────────────────────── PID 1 (init)                     │
│      └───────────────────────── PID 0 (idle/swapper)             │
│                                                                   │
│ last_pid = 8                                                      │
│ Next allocation: scan from bit 9, find first 0 → PID 9           │
└────────────────────────────────────────────────────────────────────┘

alloc_pidmap() algorithm:
1. Start scanning from (last_pid + 1)
2. find_next_zero_bit() to find free slot
3. test_and_set_bit() atomically claims it
4. If wrap-around occurs (hit pid_max), restart from RESERVED_PIDS
5. Update last_pid
```

### 6.2 Process Tree Operations

```c
// 进程树使用双向链表实现

// 遍历所有子进程
list_for_each_entry(child, &parent->children, sibling) {
    // child->sibling 链接在 parent->children 链表中
}

// 遍历线程组中的所有线程
struct task_struct *t;
t = leader;
do {
    // Process thread t
} while_each_thread(leader, t);

// while_each_thread宏展开为：
while ((t = next_thread(t)) != leader)

// next_thread使用thread_group链表
static inline struct task_struct *next_thread(const struct task_struct *p)
{
    return list_entry_rcu(p->thread_group.next,
                          struct task_struct, thread_group);
}
```

**进程树结构：**

```
                        init_task (PID 0, swapper)
                              │
                              │ tasks list
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       GLOBAL TASK LIST (init_task.tasks)                    │
│  init_task ◄──► task_A ◄──► task_B ◄──► task_C ◄──► task_D ◄──► ...       │
│  (circular doubly-linked list via 'tasks' field)                            │
└─────────────────────────────────────────────────────────────────────────────┘

                        Parent-Child Tree View

                            init (PID 1)
                                │
            ┌───────────────────┼───────────────────┐
            │                   │                   │
            ▼                   ▼                   ▼
         bash (100)        systemd-logind      nginx (500)
            │                  (200)                │
     ┌──────┴──────┐                        ┌──────┴──────┐
     │             │                        │             │
     ▼             ▼                        ▼             ▼
  vim (101)    grep (102)              worker (501)  worker (502)
                                       (thread)      (thread)

Tree linkage:
┌──────────────────────────────────────────────────────────────────────────┐
│ bash (PID 100):                                                          │
│   real_parent ──────► init's task_struct                                 │
│   children ─────────► list containing vim(101), grep(102)                │
│   sibling ──────────► next sibling under init (systemd-logind)           │
├──────────────────────────────────────────────────────────────────────────┤
│ vim (PID 101):                                                           │
│   real_parent ──────► bash's task_struct                                 │
│   children ─────────► empty list                                         │
│   sibling ──────────► grep (next child of bash)                          │
└──────────────────────────────────────────────────────────────────────────┘

Thread Group linkage (nginx with 2 worker threads):
┌──────────────────────────────────────────────────────────────────────────┐
│ nginx master (PID 500, TGID 500):                                        │
│   group_leader ────► self                                                │
│   thread_group ────► circular list: ◄──► worker1 ◄──► worker2 ◄──►      │
│   signal ──────────► shared signal_struct                                │
├──────────────────────────────────────────────────────────────────────────┤
│ worker1 (PID 501, TGID 500):                                             │
│   group_leader ────► nginx master                                        │
│   thread_group ────► (links to master and worker2)                       │
│   signal ──────────► same signal_struct as master                        │
├──────────────────────────────────────────────────────────────────────────┤
│ worker2 (PID 502, TGID 500):                                             │
│   group_leader ────► nginx master                                        │
│   thread_group ────► (links to worker1 and master)                       │
│   signal ──────────► same signal_struct as master                        │
└──────────────────────────────────────────────────────────────────────────┘
```

### 6.3 Kernel Thread Creation

```c
// kernel/kthread.c - 内核线程创建流程

/*
 * 内核线程由kthreadd守护进程统一创建，确保干净的执行环境
 */

struct task_struct *kthread_create(int (*threadfn)(void *data),
                                   void *data, const char namefmt[], ...)
{
    struct kthread_create_info create;
    
    create.threadfn = threadfn;
    create.data = data;
    init_completion(&create.done);
    
    // 将创建请求加入队列
    spin_lock(&kthread_create_lock);
    list_add_tail(&create.list, &kthread_create_list);
    spin_unlock(&kthread_create_lock);
    
    // 唤醒kthreadd
    wake_up_process(kthreadd_task);
    
    // 等待kthreadd完成创建
    wait_for_completion(&create.done);
    
    return create.result;
}

// kthreadd主循环
int kthreadd(void *unused)
{
    for (;;) {
        set_current_state(TASK_INTERRUPTIBLE);
        
        if (list_empty(&kthread_create_list))
            schedule();  // 无任务则睡眠
            
        __set_current_state(TASK_RUNNING);
        
        while (!list_empty(&kthread_create_list)) {
            struct kthread_create_info *create;
            create = list_entry(kthread_create_list.next, ...);
            list_del_init(&create->list);
            
            create_kthread(create);  // 调用kernel_thread()
        }
    }
}
```

**内核线程创建流程：**

```
Driver/Module                    kthreadd (PID 2)                New Thread
     │                                │                              
     │ kthread_create()               │                              
     ├───────────────────────────────►│                              
     │ (add to create_list)           │                              
     │ wake_up_process(kthreadd)      │                              
     │                                │                              
     │                      ┌─────────┴─────────┐                    
     │                      │ Wake up, dequeue  │                    
     │                      │ create request    │                    
     │                      └─────────┬─────────┘                    
     │                                │                              
     │                      create_kthread():                        
     │                      kernel_thread(kthread, ...)              
     │                                │                              
     │                                ├─────────────────────────────►│
     │                                │     (new task created)       │
     │                                │                              │
     │                                │     kthread() entry:         │
     │                                │     - init self.exited       │
     │◄───────────────────────────────│◄────complete(&create.done)   │
     │ (wait returns)                 │     - schedule() (sleep)     │
     │                                │                              │
     │ wake_up_process(new)           │                              │
     ├────────────────────────────────┼─────────────────────────────►│
     │                                │     (wake up, run threadfn)  │
     │                                │                              │
```

---

## 7. Concurrency & Synchronization

### 7.1 Key Locks in Process Management

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PROCESS MANAGEMENT LOCKS                                 │
└─────────────────────────────────────────────────────────────────────────────┘

1. tasklist_lock (rwlock)
   位置: kernel/fork.c - DEFINE_RWLOCK(tasklist_lock)
   保护: 进程树结构、父子关系、线程组
   获取方式:
     - read_lock(&tasklist_lock)   → 遍历进程树
     - write_lock(&tasklist_lock)  → 修改进程树

2. sighand->siglock (spinlock)
   位置: 每个sighand_struct内
   保护: 信号处理程序、pending信号队列
   
3. p->alloc_lock (spinlock)
   位置: 每个task_struct内
   保护: mm, files, fs等资源的分配/释放
   
4. pidmap_lock (spinlock)
   位置: kernel/pid.c
   保护: PID位图分配
   
5. mm->mmap_sem (rw_semaphore)
   位置: 每个mm_struct内
   保护: VMA列表/树

锁序规则 (必须按此顺序获取以避免死锁):
  p->alloc_lock
    → tasklist_lock (read or write)
      → sighand->siglock
        → mm->mmap_sem
```

### 7.2 RCU in Process Management

```c
// 进程树遍历使用RCU保护

// 读端（无锁）
rcu_read_lock();
for_each_process(p) {
    // p通过RCU保护，在rcu_read_lock()期间有效
    // 即使进程退出，task_struct也不会被立即释放
}
rcu_read_unlock();

// for_each_process宏
#define for_each_process(p) \
    for (p = &init_task; (p = next_task(p)) != &init_task; )

// next_task使用RCU
#define next_task(p) \
    list_entry_rcu((p)->tasks.next, struct task_struct, tasks)

// 写端（修改进程树）
write_lock_irq(&tasklist_lock);
// ... 修改进程树 ...
write_unlock_irq(&tasklist_lock);
// 然后call_rcu()延迟释放task_struct
```

### 7.3 Reference Counting

```c
// task_struct有两个引用计数

struct task_struct {
    atomic_t usage;  // 主引用计数
    // ...
};

// 获取引用
void get_task_struct(struct task_struct *t)
{
    atomic_inc(&t->usage);
}

// 释放引用
void put_task_struct(struct task_struct *t)
{
    if (atomic_dec_and_test(&t->usage))
        __put_task_struct(t);  // 真正释放
}

// 释放流程
void __put_task_struct(struct task_struct *tsk)
{
    WARN_ON(!tsk->exit_state);      // 必须已经exit
    WARN_ON(atomic_read(&tsk->usage)); // 引用计数必须为0
    
    exit_creds(tsk);
    put_signal_struct(tsk->signal);
    free_task(tsk);  // 释放task_struct和内核栈
}
```

**引用计数场景：**

```
Scenario 1: Normal exit and reap
┌────────────────────────────────────────────────────────────────┐
│ fork()    → usage = 2 (one for "current", one for parent ref) │
│ ...                                                            │
│ exit()    → release resources, become zombie                  │
│           → usage still = 2                                    │
│ wait()    → parent collects, release_task()                   │
│           → put_task_struct() → usage = 1                      │
│           → (still held by some kernel path or RCU)            │
│ RCU grace period → final put_task_struct() → usage = 0        │
│                 → __put_task_struct() → free                   │
└────────────────────────────────────────────────────────────────┘

Scenario 2: Task reference held longer
┌────────────────────────────────────────────────────────────────┐
│ get_task_struct(p) by /proc code  → usage++                   │
│ Process p exits                   → zombie                     │
│ Parent waits                      → release_task()             │
│ But usage > 0!                    → task_struct NOT freed      │
│ /proc code finishes               → put_task_struct()          │
│ Now usage = 0                     → __put_task_struct()        │
└────────────────────────────────────────────────────────────────┘
```

---

## 8. Performance Considerations

### 8.1 Fork Performance

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        FORK PERFORMANCE FACTORS                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  FAST (O(1) or small constant):                                            │
│  • task_struct allocation (SLAB cache)                                     │
│  • PID allocation (bitmap scan, usually hits quickly)                      │
│  • Most resource copying (just incrementing refcounts for CLONE_*)         │
│                                                                             │
│  SLOW (O(n) where n = number of VMAs or pages):                            │
│  • copy_mm() / dup_mmap() - must copy all VMA structures                  │
│  • copy_page_range() - must set up COW for all PTEs                       │
│  • For a process with 1000 VMAs, this is significant                      │
│                                                                             │
│  OPTIMIZATION: vfork()                                                      │
│  • Skips copy_mm entirely (CLONE_VM)                                       │
│  • Parent blocks until child exec()s or _exit()s                          │
│  • Used by shells for "fork+exec" pattern                                  │
│                                                                             │
│  OPTIMIZATION: clone(CLONE_VM) for threads                                 │
│  • No mm copying at all                                                     │
│  • pthread_create is fast                                                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Memory Layout for Cache Efficiency

```c
// task_struct字段按访问模式组织

struct task_struct {
    /* HOT: 调度器频繁访问 */
    volatile long state;      // offset 0
    int on_rq;
    int prio;
    struct sched_entity se;   // 调度实体，调度器最常访问
    
    /* MEDIUM: fork/exit路径 */
    struct mm_struct *mm;
    struct files_struct *files;
    
    /* COLD: 很少访问 */
    struct latency_record latency_record[LT_SAVECOUNT];
    // ...
};

// SLAB cache对齐确保task_struct在好的边界上
task_struct_cachep = kmem_cache_create("task_struct",
    sizeof(struct task_struct),
    ARCH_MIN_TASKALIGN,  // Usually L1_CACHE_BYTES (64 bytes)
    SLAB_PANIC | SLAB_NOTRACK, NULL);
```

### 8.3 Scalability Limits

| 操作 | 可伸缩性 | 瓶颈 |
|------|----------|------|
| fork() | 受VMA数量影响 | copy_mm中的循环 |
| 遍历所有进程 | O(n) | tasklist_lock争用 |
| PID分配 | 接近O(1) | pidmap_lock（短临界区） |
| 信号发送到进程组 | O(threads) | siglock争用 |
| wait() | O(children) | 需要遍历children链表 |

---

## 9. Common Pitfalls & Bugs

### 9.1 Typical Mistakes

| 错误 | 后果 | 正确做法 |
|------|------|----------|
| fork()后不检查返回值 | 父子都执行相同代码 | `if (pid == 0) child_code(); else parent_code();` |
| fork()后不wait() | 僵尸进程累积 | 调用wait()或设置SIGCHLD为SIG_IGN |
| 在fork()后两边都close(fd) | 可能导致问题 | 只在需要的进程中close |
| 线程中调用fork() | 只复制调用线程 | 避免或使用pthread_atfork() |
| 信号处理不考虑线程 | 信号可能送到错误线程 | 使用pthread_sigmask控制 |

### 9.2 Kernel Developer Mistakes

```c
// 错误1: 在持有tasklist_lock时睡眠
write_lock(&tasklist_lock);
copy_to_user(...);  // 可能睡眠！死锁风险
write_unlock(&tasklist_lock);

// 正确: 先复制到内核缓冲区
char buf[...];
memcpy(buf, ...);
write_lock(&tasklist_lock);
// ... 操作 ...
write_unlock(&tasklist_lock);
copy_to_user(user_buf, buf, ...);

// 错误2: 不检查task_struct是否还活着
struct task_struct *p = find_task_by_vpid(pid);
// p可能在下一行之前退出！
access(p->mm, ...);  // 危险

// 正确: 使用RCU或获取引用
rcu_read_lock();
p = find_task_by_vpid(pid);
if (p) get_task_struct(p);
rcu_read_unlock();
if (p) {
    // 安全使用
    put_task_struct(p);
}
```

### 9.3 Historical Issues in v3.2

1. **OOM killer和fork之间的竞争**：在高内存压力下fork()可能被错误地OOM
2. **vfork()与信号的交互**：子进程在exec前收到信号可能导致父进程永远阻塞
3. **clone(CLONE_NEWPID)后无法创建更多进程**：新PID命名空间的init（PID 1）需要正确设置

---

## 10. How to Read This Code Yourself

### 10.1 Recommended Reading Order

```
第一阶段：理解进程创建
1. kernel/fork.c: do_fork() 入口函数
2. kernel/fork.c: copy_process() 核心逻辑
3. kernel/fork.c: dup_task_struct() 任务复制
4. include/linux/sched.h: struct task_struct

第二阶段：理解资源管理
5. kernel/fork.c: copy_mm() 地址空间
6. kernel/fork.c: copy_files() 文件描述符
7. kernel/fork.c: copy_sighand() 信号处理

第三阶段：理解进程退出
8.  kernel/exit.c: do_exit() 入口
9.  kernel/exit.c: exit_mm() 释放内存
10. kernel/exit.c: exit_notify() 通知父进程

第四阶段：理解进程等待
11. kernel/exit.c: do_wait() 
12. kernel/exit.c: wait_task_zombie()
13. kernel/exit.c: release_task()

第五阶段：PID和命名空间
14. kernel/pid.c: alloc_pid()
15. include/linux/pid.h: struct pid
16. kernel/pid_namespace.c
```

### 10.2 Useful Search Commands

```bash
# 查找克隆标志定义
grep -n "^#define CLONE_" include/linux/sched.h

# 查找do_fork调用者
grep -rn "do_fork" arch/x86/kernel/

# 查找task_struct字段使用
grep -rn "->real_parent" kernel/

# 查找所有exit相关函数
grep -n "^void.*exit" kernel/exit.c

# 使用cscope查找copy_process的调用者
# cscope -d, 然后输入 'c copy_process'
```

### 10.3 Debug Interfaces

```bash
# 查看进程树
ps axjf

# 查看线程
ps -eLf

# 查看特定进程的详细信息
cat /proc/<pid>/status
cat /proc/<pid>/stat

# 查看系统中所有线程数
cat /proc/sys/kernel/threads-max
cat /proc/loadavg  # 最后一个数字是总进程数

# 查看僵尸进程
ps aux | grep Z

# strace跟踪fork/exec/exit
strace -f -e trace=process command
```

---

## 11. Summary & Mental Model

### One-Paragraph Summary

Linux进程管理围绕`task_struct`展开，这是表示进程/线程的核心数据结构。`fork()`通过`copy_process()`创建新任务，根据clone标志决定哪些资源共享、哪些复制；Copy-on-Write优化使fork在大多数情况下非常快速。进程以树形结构组织，init(PID 1)为根，每个进程维护children和sibling链表。线程是共享地址空间和信号处理的进程，通过`CLONE_THREAD`标志创建，同一线程组共享`signal_struct`和`mm_struct`。进程退出时调用`do_exit()`释放资源，变为僵尸等待父进程`wait()`收割；孤儿进程会被过继给init。PID通过位图分配，支持命名空间实现容器隔离。

### Key Invariants

1. **进程树完整性**: 所有进程都能追溯到init（除了PID 0的idle）
2. **TGID一致性**: 线程组中所有线程的tgid相同，等于组长的pid
3. **资源引用计数**: mm_users、files->count等确保共享资源不会过早释放
4. **僵尸状态必要性**: exit后必须保留task_struct直到父进程wait()
5. **clone标志依赖**: CLONE_THREAD → CLONE_SIGHAND → CLONE_VM

### Mental Model

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  把进程想象成一个"独立的执行实体"：                                          │
│                                                                             │
│  • 每个进程有自己的"身份证"（task_struct）                                  │
│  • 身份证上记录着：                                                         │
│    - 个人信息：PID、状态、优先级                                            │
│    - 家庭关系：父进程、子进程列表                                           │
│    - 财产（资源）：地址空间、打开的文件、信号处理程序                        │
│                                                                             │
│  fork() = 克隆身份证，但财产采用"共享所有权"或"复印件"                      │
│  • CLONE_VM: 共享房子（地址空间）→ 这就是线程                              │
│  • 不带CLONE_VM: 复印房子蓝图（COW），实际盖房子时才花钱                    │
│                                                                             │
│  exit() = 销毁身份证，但要先：                                              │
│  1. 把财产还给系统（释放资源）                                              │
│  2. 给家长发通知（SIGCHLD）                                                 │
│  3. 把孩子过继给福利院（reparent到init）                                    │
│  4. 留下"死亡证明"（僵尸状态）直到家长确认收到                              │
│                                                                             │
│  wait() = 家长去民政局确认孩子的死亡，领取"死亡证明"副本                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. What to Study Next

### Recommended Learning Order

| 顺序 | 子系统 | 与进程管理的关系 |
|------|--------|------------------|
| 1 | **调度器 (Scheduler)** | 进程创建后如何被调度执行 |
| 2 | **内存管理 (mm)** | fork的COW实现、地址空间管理 |
| 3 | **信号 (Signals)** | 进程间通信、终止机制 |
| 4 | **文件系统 (VFS)** | files_struct、fs_struct的使用 |
| 5 | **exec加载器** | 程序如何被加载执行 |
| 6 | **命名空间 (Namespaces)** | 容器隔离实现 |
| 7 | **cgroups** | 资源限制和统计 |
| 8 | **ptrace** | 调试器实现 |

### Related Files to Study

```
kernel/fork.c              - 进程创建
kernel/exit.c              - 进程退出
fs/exec.c                  - 程序加载
kernel/signal.c            - 信号处理
kernel/ptrace.c            - 进程跟踪
kernel/pid.c               - PID管理
kernel/pid_namespace.c     - PID命名空间
kernel/nsproxy.c           - 命名空间代理
kernel/cred.c              - 凭证管理
include/linux/sched.h      - 核心数据结构
```

---

## Appendix: Quick Reference

### A. Process States

| State | Value | Description |
|-------|-------|-------------|
| TASK_RUNNING | 0 | 可运行（在运行队列或正在CPU上） |
| TASK_INTERRUPTIBLE | 1 | 可中断睡眠（可被信号唤醒） |
| TASK_UNINTERRUPTIBLE | 2 | 不可中断睡眠（不响应信号） |
| __TASK_STOPPED | 4 | 已停止（SIGSTOP/SIGTSTP） |
| __TASK_TRACED | 8 | 被ptrace跟踪 |
| EXIT_ZOMBIE | 16 | 僵尸（等待父进程wait） |
| EXIT_DEAD | 32 | 最终死亡状态 |
| TASK_DEAD | 64 | 正在退出 |

### B. Important /proc Files

```bash
/proc/<pid>/status      # 进程状态摘要
/proc/<pid>/stat        # 详细统计（单行）
/proc/<pid>/maps        # 内存映射
/proc/<pid>/fd/         # 打开的文件描述符
/proc/<pid>/task/       # 线程列表
/proc/<pid>/children    # 子进程列表
/proc/<pid>/ns/         # 命名空间链接
```

### C. Key System Calls

| Syscall | Purpose |
|---------|---------|
| fork() | 创建子进程（完整复制） |
| vfork() | 创建子进程（共享地址空间，父阻塞） |
| clone() | 创建进程/线程（细粒度控制） |
| execve() | 加载新程序 |
| exit() | 终止当前线程 |
| exit_group() | 终止整个线程组 |
| wait4() | 等待子进程状态变化 |
| waitid() | 等待子进程（更多选项） |
| getpid() | 获取进程ID（实际是TGID） |
| gettid() | 获取线程ID（真正的PID） |
| getppid() | 获取父进程ID |
| setpgid() | 设置进程组 |
| setsid() | 创建新会话 |

---

**Author**: Linux Kernel Study Guide  
**Kernel Version**: 3.2.0 ("Saber-toothed Squirrel")  
**Last Updated**: Based on kernel source analysis

