# WHERE｜源代码地图

## 1. 入口点

```
SCHEDULER SOURCE CODE ENTRY POINTS
+=============================================================================+
|                                                                              |
|  KERNEL/SCHED/ DIRECTORY STRUCTURE (Linux 3.2)                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  kernel/                                                                 │ |
|  │  ├── sched.c              ◄── Core scheduler (Linux 3.2)                │ |
|  │  │   (later split into kernel/sched/*.c in newer kernels)               │ |
|  │  │                                                                       │ |
|  │  ├── sched_fair.c         ◄── CFS implementation                        │ |
|  │  ├── sched_rt.c           ◄── Real-time scheduler                       │ |
|  │  ├── sched_idletask.c     ◄── Idle task scheduler                       │ |
|  │  ├── sched_stoptask.c     ◄── Stop task scheduler                       │ |
|  │  │                                                                       │ |
|  │  ├── sched_autogroup.c    ◄── Autogroup (desktop interactivity)         │ |
|  │  ├── sched_debug.c        ◄── /proc/sched_debug support                 │ |
|  │  ├── sched_stats.h        ◄── Statistics structures                     │ |
|  │  │                                                                       │ |
|  │  ├── fork.c               ◄── Process creation (copy_process)           │ |
|  │  ├── exit.c               ◄── Process termination (do_exit)             │ |
|  │  ├── signal.c             ◄── Signal handling                           │ |
|  │  └── ...                                                                 │ |
|  │                                                                          │ |
|  │  include/linux/                                                          │ |
|  │  ├── sched.h              ◄── task_struct, sched_class definitions       │ |
|  │  └── ...                                                                 │ |
|  │                                                                          │ |
|  │  NOTE: In newer kernels (3.x+), sched.c is split into:                   │ |
|  │  kernel/sched/                                                           │ |
|  │  ├── core.c               ◄── Core scheduler logic                      │ |
|  │  ├── fair.c               ◄── CFS                                       │ |
|  │  ├── rt.c                 ◄── Real-time                                 │ |
|  │  ├── deadline.c           ◄── SCHED_DEADLINE (newer)                    │ |
|  │  ├── idle.c               ◄── Idle task                                 │ |
|  │  ├── stop_task.c          ◄── Stop task                                 │ |
|  │  ├── clock.c              ◄── Scheduler clock                           │ |
|  │  ├── cpuacct.c            ◄── CPU accounting                            │ |
|  │  ├── debug.c              ◄── Debug interface                           │ |
|  │  └── sched.h              ◄── Internal header                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KEY FILES FOR ARCHITECTURE UNDERSTANDING                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  MUST READ (in order):                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. include/linux/sched.h (~3000 lines)                          │    │ |
|  │  │     • task_struct definition                                     │    │ |
|  │  │     • sched_class definition                                     │    │ |
|  │  │     • State constants (TASK_RUNNING, etc.)                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. kernel/sched.c (or kernel/sched/core.c)                      │    │ |
|  │  │     • schedule() - the main entry point                          │    │ |
|  │  │     • __schedule() - core scheduling logic                       │    │ |
|  │  │     • context_switch() - switching between tasks                 │    │ |
|  │  │     • wake_up_process() - waking sleeping tasks                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. kernel/sched_fair.c (or kernel/sched/fair.c)                 │    │ |
|  │  │     • fair_sched_class definition                                │    │ |
|  │  │     • enqueue_task_fair() - adding to CFS                        │    │ |
|  │  │     • pick_next_task_fair() - selecting next task                │    │ |
|  │  │     • update_curr() - vruntime update                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. kernel/fork.c                                                │    │ |
|  │  │     • do_fork() - process creation                               │    │ |
|  │  │     • copy_process() - duplicating task_struct                   │    │ |
|  │  │     • sched_fork() - scheduler initialization for new task       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**kernel/sched/ 目录结构（Linux 3.2）**：
- `kernel/sched.c`：核心调度器（在更新的内核中拆分为 `kernel/sched/*.c`）
- `kernel/sched_fair.c`：CFS 实现
- `kernel/sched_rt.c`：实时调度器
- `kernel/sched_idletask.c`：空闲任务调度器
- `kernel/fork.c`：进程创建（`copy_process`）
- `kernel/exit.c`：进程终止（`do_exit`）
- `include/linux/sched.h`：`task_struct`、`sched_class` 定义

**必读文件（按顺序）**：
1. `include/linux/sched.h`：`task_struct`、`sched_class`、状态常量
2. `kernel/sched.c`：`schedule()`、`context_switch()`、`wake_up_process()`
3. `kernel/sched_fair.c`：CFS 实现，`enqueue_task_fair()`、`pick_next_task_fair()`
4. `kernel/fork.c`：`do_fork()`、`copy_process()`、`sched_fork()`

---

## 2. 架构锚点

```
ARCHITECTURAL ANCHORS: STRUCT TASK_STRUCT
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  TASK_STRUCT: THE UNIVERSAL IDENTITY OF EXECUTION                        │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Location: include/linux/sched.h                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct task_struct (simplified view):                           │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* IDENTITY */                                             │ │    │ |
|  │  │  │  pid_t pid;                    /* Process ID */             │ │    │ |
|  │  │  │  pid_t tgid;                   /* Thread group ID */        │ │    │ |
|  │  │  │  char comm[TASK_COMM_LEN];     /* Executable name */        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* STATE (scheduler sees this) */                          │ │    │ |
|  │  │  │  volatile long state;          /* RUNNING/SLEEPING/etc */   │ │    │ |
|  │  │  │  void *stack;                  /* Kernel stack */           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* SCHEDULING (scheduler's primary data) */                │ │    │ |
|  │  │  │  int prio, static_prio;        /* Dynamic/static prio */    │ │    │ |
|  │  │  │  const struct sched_class *sched_class; /* Ops table! */    │ │    │ |
|  │  │  │  struct sched_entity se;       /* CFS scheduling entity */  │ │    │ |
|  │  │  │  struct sched_rt_entity rt;    /* RT scheduling entity */   │ │    │ |
|  │  │  │  unsigned int policy;          /* Scheduling policy */      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* MEMORY (mm subsystem sees this) */                      │ │    │ |
|  │  │  │  struct mm_struct *mm;         /* User address space */     │ │    │ |
|  │  │  │  struct mm_struct *active_mm;  /* Active mm */              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* FILES (VFS sees this) */                                │ │    │ |
|  │  │  │  struct files_struct *files;   /* Open files table */       │ │    │ |
|  │  │  │  struct fs_struct *fs;         /* Filesystem context */     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* SIGNALS */                                              │ │    │ |
|  │  │  │  struct signal_struct *signal; /* Signal handling */        │ │    │ |
|  │  │  │  struct sighand_struct *sighand;                            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* FAMILY TREE */                                          │ │    │ |
|  │  │  │  struct task_struct *parent;   /* Parent process */         │ │    │ |
|  │  │  │  struct list_head children;    /* Child processes */        │ │    │ |
|  │  │  │  struct list_head sibling;     /* Sibling linkage */        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* CREDENTIALS */                                          │ │    │ |
|  │  │  │  const struct cred *cred;      /* Credentials */            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  /* NAMESPACES */                                           │ │    │ |
|  │  │  │  struct nsproxy *nsproxy;      /* Namespace proxy */        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CROSS-SUBSYSTEM RELATIONSHIPS:                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                         task_struct                              │    │ |
|  │  │                              │                                   │    │ |
|  │  │          ┌──────────────────┼──────────────────┐                 │    │ |
|  │  │          │                  │                  │                 │    │ |
|  │  │          ▼                  ▼                  ▼                 │    │ |
|  │  │    ┌──────────┐      ┌──────────┐      ┌──────────┐              │    │ |
|  │  │    │ SCHEDULER│      │  MEMORY  │      │   VFS    │              │    │ |
|  │  │    │          │      │          │      │          │              │    │ |
|  │  │    │ sched_   │      │ mm_struct│      │ files_   │              │    │ |
|  │  │    │ entity   │      │          │      │ struct   │              │    │ |
|  │  │    │ sched_   │      │ vm_area_ │      │ fs_struct│              │    │ |
|  │  │    │ class    │      │ struct   │      │          │              │    │ |
|  │  │    └──────────┘      └──────────┘      └──────────┘              │    │ |
|  │  │          │                  │                  │                 │    │ |
|  │  │          ▼                  ▼                  ▼                 │    │ |
|  │  │    ┌──────────┐      ┌──────────┐      ┌──────────┐              │    │ |
|  │  │    │   rq     │      │ page     │      │ inode    │              │    │ |
|  │  │    │ (runq)   │      │ tables   │      │ dentry   │              │    │ |
|  │  │    └──────────┘      └──────────┘      └──────────┘              │    │ |
|  │  │                                                                  │    │ |
|  │  │  task_struct is the HUB connecting all subsystems               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**task_struct：执行的通用身份**

`task_struct` 是连接所有子系统的中心枢纽：

- **身份**：`pid`、`tgid`、`comm`（可执行文件名）
- **状态**：`state`（运行/睡眠等）、`stack`（内核栈）
- **调度**：`prio`、`sched_class`（操作表！）、`sched_entity`、`policy`
- **内存**：`mm`（用户地址空间）
- **文件**：`files`（打开文件表）、`fs`（文件系统上下文）
- **信号**：`signal`、`sighand`
- **家族树**：`parent`、`children`、`sibling`
- **凭证**：`cred`
- **命名空间**：`nsproxy`

**跨子系统关系**：
- `task_struct` → 调度器（`sched_entity` → `rq`）
- `task_struct` → 内存（`mm_struct` → 页表）
- `task_struct` → VFS（`files_struct` → `inode`/`dentry`）

---

## 3. 控制中心

```
CONTROL HUBS: SCHEDULE() AND CONTEXT_SWITCH()
+=============================================================================+
|                                                                              |
|  SCHEDULE() - THE SCHEDULER ENTRY POINT                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Location: kernel/sched.c (or kernel/sched/core.c)                       │ |
|  │                                                                          │ |
|  │  Call sites (who calls schedule()):                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. VOLUNTARY - Task explicitly yields:                          │    │ |
|  │  │     • schedule() called directly                                 │    │ |
|  │  │     • yield() → schedule()                                       │    │ |
|  │  │     • mutex_lock() → schedule()  (if contended)                  │    │ |
|  │  │     • wait_event() → schedule()                                  │    │ |
|  │  │     • io_schedule() → schedule() (I/O wait)                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. INVOLUNTARY - Kernel forces reschedule:                      │    │ |
|  │  │     • Timer interrupt → scheduler_tick() → set TIF_NEED_RESCHED  │    │ |
|  │  │     • Return from syscall → check TIF_NEED_RESCHED → schedule()  │    │ |
|  │  │     • Return from interrupt → check TIF_NEED_RESCHED → schedule()│    │ |
|  │  │     • preempt_schedule() (if CONFIG_PREEMPT)                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. WAKEUP - Another task becomes runnable:                      │    │ |
|  │  │     • wake_up_process() → try_to_wake_up() → maybe resched       │    │ |
|  │  │     • complete() → wake_up_process()                             │    │ |
|  │  │     • up() (semaphore) → wake_up_process()                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CODE STRUCTURE:                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  asmlinkage void __sched schedule(void)                          │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct task_struct *tsk = current;                          │    │ |
|  │  │                                                                  │    │ |
|  │  │      sched_submit_work(tsk);     /* flush pending work */        │    │ |
|  │  │      __schedule();               /* do the actual scheduling */  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static void __sched __schedule(void)                            │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct rq *rq;                                              │    │ |
|  │  │      struct task_struct *prev, *next;                            │    │ |
|  │  │                                                                  │    │ |
|  │  │      preempt_disable();                                          │    │ |
|  │  │      cpu = smp_processor_id();                                   │    │ |
|  │  │      rq = cpu_rq(cpu);            /* get this CPU's run queue */ │    │ |
|  │  │      prev = rq->curr;             /* current task */             │    │ |
|  │  │                                                                  │    │ |
|  │  │      raw_spin_lock_irq(&rq->lock);                               │    │ |
|  │  │                                                                  │    │ |
|  │  │      /* Deactivate if not runnable */                            │    │ |
|  │  │      if (prev->state && !(preempt_count() & PREEMPT_ACTIVE)) {   │    │ |
|  │  │          if (signal_pending_state(prev->state, prev))            │    │ |
|  │  │              prev->state = TASK_RUNNING;                         │    │ |
|  │  │          else                                                    │    │ |
|  │  │              deactivate_task(rq, prev, DEQUEUE_SLEEP);           │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      prev->sched_class->put_prev_task(rq, prev);                 │    │ |
|  │  │      next = pick_next_task(rq);   /* SELECT NEXT TASK! */        │    │ |
|  │  │                                                                  │    │ |
|  │  │      if (likely(prev != next)) {                                 │    │ |
|  │  │          rq->curr = next;                                        │    │ |
|  │  │          context_switch(rq, prev, next);  /* SWITCH! */          │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      raw_spin_unlock_irq(&rq->lock);                             │    │ |
|  │  │      preempt_enable();                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CONTEXT_SWITCH() - THE ACTUAL SWITCH                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  static inline void                                                      │ |
|  │  context_switch(struct rq *rq, struct task_struct *prev,                 │ |
|  │                 struct task_struct *next)                                │ |
|  │  {                                                                       │ |
|  │      struct mm_struct *mm, *oldmm;                                       │ |
|  │                                                                          │ |
|  │      prepare_task_switch(rq, prev, next);                                │ |
|  │                                                                          │ |
|  │      mm = next->mm;                                                      │ |
|  │      oldmm = prev->active_mm;                                            │ |
|  │                                                                          │ |
|  │      /* STEP 1: Switch address space */                                  │ |
|  │      if (!mm) {                             /* kernel thread */          │ |
|  │          next->active_mm = oldmm;           /* borrow prev's mm */       │ |
|  │          atomic_inc(&oldmm->mm_count);                                   │ |
|  │          enter_lazy_tlb(oldmm, next);                                    │ |
|  │      } else                                 /* user process */           │ |
|  │          switch_mm(oldmm, mm, next);        /* SWITCH PAGE TABLES */     │ |
|  │                                                                          │ |
|  │      /* STEP 2: Switch registers and stack */                            │ |
|  │      switch_to(prev, next, prev);           /* ARCH-SPECIFIC! */         │ |
|  │                                                                          │ |
|  │      /* After this point, we are running as 'next' */                    │ |
|  │                                                                          │ |
|  │      barrier();                                                          │ |
|  │      finish_task_switch(this_rq(), prev);                                │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  KEY INSIGHT:                                                            │ |
|  │  After switch_to(), the code continues but now as 'next' task            │ |
|  │  The 'prev' pointer now points to what WAS running before               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**schedule() - 调度器入口点**

**调用场景**：
1. **自愿**：任务显式让出（`schedule()`、`yield()`、`mutex_lock()`、`wait_event()`）
2. **非自愿**：内核强制重调度（定时器中断 → `scheduler_tick()` → 设置 `TIF_NEED_RESCHED`）
3. **唤醒**：另一个任务变为可运行（`wake_up_process()` → `try_to_wake_up()`）

**代码结构**：
- 禁用抢占
- 获取当前 CPU 的运行队列
- 如果当前任务不再可运行，停用它
- 调用 `put_prev_task()`（类回调）
- 调用 `pick_next_task()` 选择下一个任务
- 如果不同，调用 `context_switch()`

**context_switch() - 实际切换**：
- **步骤 1**：切换地址空间（`switch_mm()` 切换页表）
- **步骤 2**：切换寄存器和栈（`switch_to()` 架构相关）
- **关键洞察**：`switch_to()` 之后，代码继续运行但现在是 `next` 任务

---

## 4. 如何在代码中追踪调度决策

```
TRACING A SCHEDULING DECISION
+=============================================================================+
|                                                                              |
|  EXAMPLE: TRACING A TIMER-BASED PREEMPTION                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  STEP 1: Timer interrupt fires                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  arch/x86/kernel/apic/apic.c:                                    │    │ |
|  │  │  local_apic_timer_interrupt()                                    │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  tick_handle_periodic() or tick_sched_timer()                    │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  update_process_times(user_mode(regs))                           │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  scheduler_tick()      ◄── kernel/sched.c                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  STEP 2: scheduler_tick() updates accounting                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  void scheduler_tick(void)                                       │    │ |
|  │  │  {                                                               │    │ |
|  │  │      int cpu = smp_processor_id();                               │    │ |
|  │  │      struct rq *rq = cpu_rq(cpu);                                │    │ |
|  │  │      struct task_struct *curr = rq->curr;                        │    │ |
|  │  │                                                                  │    │ |
|  │  │      raw_spin_lock(&rq->lock);                                   │    │ |
|  │  │      update_rq_clock(rq);                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │      curr->sched_class->task_tick(rq, curr, 0);  ◄── CLASS CALL! │    │ |
|  │  │                                                                  │    │ |
|  │  │      raw_spin_unlock(&rq->lock);                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  STEP 3: CFS task_tick checks if preemption needed                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static void task_tick_fair(struct rq *rq, struct task_struct *curr,│  │ |
|  │  │                             int queued)                          │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct cfs_rq *cfs_rq;                                      │    │ |
|  │  │      struct sched_entity *se = &curr->se;                        │    │ |
|  │  │                                                                  │    │ |
|  │  │      for_each_sched_entity(se) {                                 │    │ |
|  │  │          cfs_rq = cfs_rq_of(se);                                 │    │ |
|  │  │          entity_tick(cfs_rq, se, queued);                        │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static void entity_tick(...)                                    │    │ |
|  │  │  {                                                               │    │ |
|  │  │      update_curr(cfs_rq);    ◄── UPDATE VRUNTIME!                │    │ |
|  │  │                                                                  │    │ |
|  │  │      if (cfs_rq->nr_running > 1)                                 │    │ |
|  │  │          check_preempt_tick(cfs_rq, curr);  ◄── CHECK PREEMPT!   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  STEP 4: check_preempt_tick may set TIF_NEED_RESCHED                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static void check_preempt_tick(struct cfs_rq *cfs_rq,           │    │ |
|  │  │                                 struct sched_entity *curr)       │    │ |
|  │  │  {                                                               │    │ |
|  │  │      unsigned long ideal_runtime, delta_exec;                    │    │ |
|  │  │                                                                  │    │ |
|  │  │      ideal_runtime = sched_slice(cfs_rq, curr);                  │    │ |
|  │  │      delta_exec = curr->sum_exec_runtime - curr->prev_sum_exec_runtime;│ |
|  │  │                                                                  │    │ |
|  │  │      if (delta_exec > ideal_runtime) {                           │    │ |
|  │  │          resched_task(rq_of(cfs_rq)->curr);  ◄── SET THE FLAG!   │    │ |
|  │  │          return;                                                 │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      /* Also check if another task has much lower vruntime */    │    │ |
|  │  │      se = __pick_first_entity(cfs_rq);                           │    │ |
|  │  │      if (se && delta > ideal_runtime)                            │    │ |
|  │  │          resched_task(rq_of(cfs_rq)->curr);                      │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void resched_task(struct task_struct *p)                        │    │ |
|  │  │  {                                                               │    │ |
|  │  │      set_tsk_need_resched(p);  ◄── TIF_NEED_RESCHED = 1          │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  STEP 5: Return from interrupt checks the flag                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  arch/x86/kernel/entry_64.S (or entry_32.S):                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ret_from_intr:                                                  │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      testl $_TIF_NEED_RESCHED, %ecx                              │    │ |
|  │  │      jnz schedule_preempt           ◄── IF SET, CALL SCHEDULE   │    │ |
|  │  │      ...                                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  schedule_preempt:                                               │    │ |
|  │  │      call schedule                   ◄── ENTER SCHEDULER!        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  STEP 6: schedule() picks next task                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  __schedule() → pick_next_task() → fair_sched_class.pick_next_task()│  │ |
|  │  │                                                                  │    │ |
|  │  │  pick_next_task_fair() returns task with smallest vruntime       │    │ |
|  │  │                                                                  │    │ |
|  │  │  context_switch() to that task                                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**追踪基于定时器的抢占**：

1. **定时器中断触发**：`local_apic_timer_interrupt()` → `scheduler_tick()`
2. **scheduler_tick() 更新统计**：调用 `curr->sched_class->task_tick()`（类调用）
3. **CFS task_tick 检查是否需要抢占**：
   - `update_curr()` 更新 vruntime
   - `check_preempt_tick()` 检查是否超过理想运行时间
4. **check_preempt_tick 可能设置 TIF_NEED_RESCHED**：
   - 如果运行时间超过理想时间片，调用 `resched_task()` 设置标志
5. **从中断返回时检查标志**：
   - 如果 `TIF_NEED_RESCHED` 设置，跳转到 `schedule`
6. **schedule() 选择下一个任务**：
   - `pick_next_task_fair()` 返回 vruntime 最小的任务
   - `context_switch()` 切换到该任务

---

## 5. 推荐阅读顺序

```
RECOMMENDED READING ORDER
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PHASE 1: UNDERSTAND THE DATA STRUCTURES (理解数据结构)                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. include/linux/sched.h                                        │    │ |
|  │  │     Focus on:                                                    │    │ |
|  │  │     • struct task_struct (find the scheduling-related fields)    │    │ |
|  │  │     • struct sched_entity (understand vruntime, load_weight)     │    │ |
|  │  │     • struct sched_class (the ops-table interface)               │    │ |
|  │  │     • State constants (TASK_RUNNING, TASK_INTERRUPTIBLE, etc.)   │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. kernel/sched.c (top of file)                                 │    │ |
|  │  │     Focus on:                                                    │    │ |
|  │  │     • struct rq definition                                       │    │ |
|  │  │     • struct cfs_rq definition                                   │    │ |
|  │  │     • DEFINE_PER_CPU for runqueues                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 2: UNDERSTAND THE MAIN FLOW (理解主流程)                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  3. kernel/sched.c: schedule() and __schedule()                  │    │ |
|  │  │     • Trace the logic step by step                               │    │ |
|  │  │     • Understand when deactivate_task is called                  │    │ |
|  │  │     • See how pick_next_task is called                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. kernel/sched.c: pick_next_task()                             │    │ |
|  │  │     • See the fast path (CFS only)                               │    │ |
|  │  │     • See the slow path (iterate classes)                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  5. kernel/sched.c: context_switch()                             │    │ |
|  │  │     • switch_mm (address space switch)                           │    │ |
|  │  │     • switch_to (register switch) - arch-specific                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 3: UNDERSTAND CFS (理解 CFS)                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  6. kernel/sched_fair.c: fair_sched_class definition             │    │ |
|  │  │     • See all the function pointers                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  7. kernel/sched_fair.c: update_curr()                           │    │ |
|  │  │     • How vruntime is updated                                    │    │ |
|  │  │     • The weighted calculation                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  8. kernel/sched_fair.c: enqueue_task_fair()                     │    │ |
|  │  │     • How tasks are added to the red-black tree                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  9. kernel/sched_fair.c: pick_next_task_fair()                   │    │ |
|  │  │     • How the leftmost node is selected                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 4: UNDERSTAND LIFECYCLE (理解生命周期)                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  10. kernel/fork.c: do_fork() and copy_process()                 │    │ |
|  │  │      • How task_struct is allocated                              │    │ |
|  │  │      • sched_fork() initializes scheduling                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  11. kernel/sched.c: wake_up_new_task()                          │    │ |
|  │  │      • How new task enters the run queue                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  12. kernel/exit.c: do_exit()                                    │    │ |
|  │  │      • How task exits and becomes zombie                         │    │ |
|  │  │      • Final schedule() that never returns                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 5: UNDERSTAND PREEMPTION (理解抢占)                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  13. kernel/sched.c: scheduler_tick()                            │    │ |
|  │  │      • Called on every timer tick                                │    │ |
|  │  │      • Calls task_tick via sched_class                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  14. kernel/sched_fair.c: task_tick_fair()                       │    │ |
|  │  │      • check_preempt_tick() logic                                │    │ |
|  │  │      • When TIF_NEED_RESCHED is set                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  15. arch/x86/kernel/entry_64.S: ret_from_intr                   │    │ |
|  │  │      • How schedule is called on return from interrupt           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 6: ADVANCED TOPICS (高级主题)                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  16. kernel/sched_rt.c: Real-time scheduling                     │    │ |
|  │  │  17. kernel/sched.c: Load balancing (load_balance, etc.)         │    │ |
|  │  │  18. kernel/sched.c: CPU affinity (set_cpus_allowed)             │    │ |
|  │  │  19. kernel/cpuset.c: cpusets integration                        │    │ |
|  │  │  20. kernel/cgroup.c: cgroup scheduling                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  TIPS FOR READING                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  • Use cscope or ctags for navigation                                   │ |
|  │  • grep for function names to find callers                              │ |
|  │  • Read the comments - kernel code is well commented                    │ |
|  │  • Focus on the happy path first, then error handling                   │ |
|  │  • Use ftrace to observe actual execution:                              │ |
|  │    echo function_graph > /sys/kernel/debug/tracing/current_tracer      │ |
|  │    echo schedule > /sys/kernel/debug/tracing/set_ftrace_filter         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**阶段 1：理解数据结构**
1. `include/linux/sched.h`：`task_struct`、`sched_entity`、`sched_class`、状态常量
2. `kernel/sched.c` 顶部：`rq`、`cfs_rq` 定义

**阶段 2：理解主流程**
3. `schedule()` 和 `__schedule()`
4. `pick_next_task()`
5. `context_switch()`

**阶段 3：理解 CFS**
6. `fair_sched_class` 定义
7. `update_curr()`：vruntime 更新
8. `enqueue_task_fair()`
9. `pick_next_task_fair()`

**阶段 4：理解生命周期**
10. `do_fork()` 和 `copy_process()`
11. `wake_up_new_task()`
12. `do_exit()`

**阶段 5：理解抢占**
13. `scheduler_tick()`
14. `task_tick_fair()` 和 `check_preempt_tick()`
15. `arch/x86/kernel/entry_64.S`

**阶段 6：高级主题**
16. 实时调度、负载均衡、CPU 亲和性、cgroup 调度

**阅读技巧**：
- 使用 cscope 或 ctags 导航
- 先关注正常路径，再关注错误处理
- 使用 ftrace 观察实际执行
