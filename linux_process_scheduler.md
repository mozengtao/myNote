# Linux 进程调度框架深入讲解

基于 Linux 3.2 内核源码分析

---

## 目录

- [进程调度架构概述](#进程调度架构概述)
- [核心数据结构](#核心数据结构)
- [调度类与调度策略](#调度类与调度策略)
- [CFS 完全公平调度器](#cfs-完全公平调度器)
- [实时调度器](#实时调度器)
- [调度时机与上下文切换](#调度时机与上下文切换)
- [多核负载均衡](#多核负载均衡)
- [关键源码文件](#关键源码文件)

---

## 进程调度架构概述

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              进程调度子系统                                   │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                         调度核心 (kernel/sched.c)                       │ │
│  │                                                                         │ │
│  │   schedule() ──► pick_next_task() ──► context_switch()                 │ │
│  │                         │                                               │ │
│  │         ┌───────────────┼───────────────┐                              │ │
│  │         ▼               ▼               ▼                              │ │
│  │   ┌──────────┐    ┌──────────┐    ┌──────────┐                         │ │
│  │   │   Stop   │    │    RT    │    │   CFS    │  ◄── 调度类              │ │
│  │   │  Class   │    │  Class   │    │  Class   │                         │ │
│  │   │ (最高优先)│    │(实时调度)│    │(普通进程)│                         │ │
│  │   └──────────┘    └──────────┘    └──────────┘                         │ │
│  │         │               │               │                              │ │
│  │         ▼               ▼               ▼                              │ │
│  │   stop_sched_class  rt_sched_class  fair_sched_class                   │ │
│  │                          │               │                             │ │
│  │                          ▼               ▼                             │ │
│  │                    ┌──────────┐    ┌──────────┐                        │ │
│  │                    │ SCHED_RR │    │SCHED_NORMAL                       │ │
│  │                    │SCHED_FIFO│    │SCHED_BATCH│                       │ │
│  │                    └──────────┘    │SCHED_IDLE │                       │ │
│  │                                    └──────────┘                        │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                    │                                         │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                          运行队列 (Run Queue)                           │ │
│  │                                                                         │ │
│  │   ┌─────────────────────────────────────────────────────────────────┐  │ │
│  │   │                    Per-CPU Run Queue                            │  │ │
│  │   │                                                                 │  │ │
│  │   │   CPU0: rq ──► cfs_rq ──► rb_tree (红黑树)                      │  │ │
│  │   │              ──► rt_rq ──► prio_array (优先级数组)               │  │ │
│  │   │                                                                 │  │ │
│  │   │   CPU1: rq ──► cfs_rq ──► rb_tree                               │  │ │
│  │   │              ──► rt_rq ──► prio_array                           │  │ │
│  │   │   ...                                                           │  │ │
│  │   └─────────────────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                    │                                         │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                          负载均衡 (Load Balance)                        │ │
│  │                                                                         │ │
│  │   load_balance() ──► find_busiest_queue() ──► move_tasks()             │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 调度目标

1. **公平性**: 进程公平共享 CPU 时间
2. **响应性**: 快速响应交互式进程
3. **吞吐量**: 最大化 CPU 利用率
4. **实时性**: 满足实时任务的截止期限
5. **节能**: 在可能时让 CPU 进入低功耗状态

---

## 核心数据结构

### 1. struct task_struct - 进程描述符 (调度相关字段)

```c
// include/linux/sched.h
struct task_struct {
    volatile long state;                // 进程状态
    
    int prio;                           // 动态优先级
    int static_prio;                    // 静态优先级 (nice 值决定)
    int normal_prio;                    // 普通优先级
    unsigned int rt_priority;           // 实时优先级 (0-99)
    
    const struct sched_class *sched_class; // 调度类
    struct sched_entity se;             // CFS 调度实体
    struct sched_rt_entity rt;          // RT 调度实体
    
    unsigned int policy;                // 调度策略
    int nr_cpus_allowed;                // 允许运行的 CPU 数
    cpumask_t cpus_allowed;             // 允许运行的 CPU 掩码
    
    struct sched_info sched_info;       // 调度统计信息
    
    struct list_head tasks;             // 进程链表
    
    struct mm_struct *mm;               // 内存描述符
    struct mm_struct *active_mm;        // 活动内存描述符
    
    pid_t pid;                          // 进程 ID
    pid_t tgid;                         // 线程组 ID
    
    // ...
};

// 进程状态
#define TASK_RUNNING        0   // 运行中或就绪
#define TASK_INTERRUPTIBLE  1   // 可中断睡眠
#define TASK_UNINTERRUPTIBLE 2  // 不可中断睡眠
#define __TASK_STOPPED      4   // 停止
#define __TASK_TRACED       8   // 被跟踪
```

### 2. struct rq - 运行队列

```c
// kernel/sched.c
struct rq {
    raw_spinlock_t lock;                // 队列锁
    
    unsigned long nr_running;           // 可运行进程数
    
    u64 nr_switches;                    // 上下文切换次数
    
    struct cfs_rq cfs;                  // CFS 运行队列
    struct rt_rq rt;                    // RT 运行队列
    
    struct task_struct *curr;           // 当前运行进程
    struct task_struct *idle;           // idle 进程
    struct task_struct *stop;           // stop 进程
    
    unsigned long next_balance;         // 下次负载均衡时间
    
    struct mm_struct *prev_mm;          // 前一个进程的 mm
    
    u64 clock;                          // 队列时钟
    u64 clock_task;                     // 任务时钟
    
    int cpu;                            // 所属 CPU
    int online;                         // 是否在线
    
    struct sched_avg avg;               // 平均负载
    
    // ...
};
```

### 3. struct sched_entity - CFS 调度实体

```c
// include/linux/sched.h
struct sched_entity {
    struct load_weight load;            // 权重
    struct rb_node run_node;            // 红黑树节点
    struct list_head group_node;        // 组链表节点
    unsigned int on_rq;                 // 是否在运行队列
    
    u64 exec_start;                     // 开始执行时间
    u64 sum_exec_runtime;               // 总执行时间
    u64 vruntime;                       // 虚拟运行时间 ★
    u64 prev_sum_exec_runtime;          // 前一次总执行时间
    
    u64 nr_migrations;                  // 迁移次数
    
#ifdef CONFIG_FAIR_GROUP_SCHED
    struct sched_entity *parent;        // 父实体
    struct cfs_rq *cfs_rq;              // 所属 CFS 队列
    struct cfs_rq *my_q;                // 拥有的 CFS 队列
#endif
};
```

### 4. struct cfs_rq - CFS 运行队列

```c
// kernel/sched.c
struct cfs_rq {
    struct load_weight load;            // 队列总权重
    unsigned long nr_running;           // 可运行实体数
    
    u64 exec_clock;                     // 执行时钟
    u64 min_vruntime;                   // 最小虚拟运行时间
    
    struct rb_root tasks_timeline;      // 红黑树根 ★
    struct rb_node *rb_leftmost;        // 最左节点 (最小 vruntime)
    
    struct sched_entity *curr;          // 当前运行实体
    struct sched_entity *next;          // 下一个实体
    struct sched_entity *last;          // 上一个实体
    struct sched_entity *skip;          // 跳过的实体
    
    // ...
};
```

---

## 调度类与调度策略

### 调度类层次

```c
// kernel/sched.c
// 调度类优先级: stop > rt > fair > idle
static const struct sched_class stop_sched_class;   // 最高优先级
static const struct sched_class rt_sched_class;     // 实时
static const struct sched_class fair_sched_class;   // 普通 (CFS)
static const struct sched_class idle_sched_class;   // 空闲

// 调度类结构
struct sched_class {
    const struct sched_class *next;     // 下一个调度类
    
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)(struct rq *rq);
    
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);
    
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    
    void (*set_curr_task)(struct rq *rq);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)(struct task_struct *p);
    
    void (*switched_from)(struct rq *rq, struct task_struct *p);
    void (*switched_to)(struct rq *rq, struct task_struct *p);
    void (*prio_changed)(struct rq *rq, struct task_struct *p, int oldprio);
    
    // ...
};
```

### 调度策略

```c
// 调度策略
#define SCHED_NORMAL    0   // 普通进程 (CFS)
#define SCHED_FIFO      1   // 实时先进先出
#define SCHED_RR        2   // 实时时间片轮转
#define SCHED_BATCH     3   // 批处理 (CFS，低优先级)
#define SCHED_IDLE      5   // 空闲调度 (最低优先级)
```

### 调度类选择流程

```
pick_next_task()
      │
      ├── stop_sched_class.pick_next_task()
      │         │
      │         └── 有 stop 任务? ──► 返回
      │
      ├── rt_sched_class.pick_next_task()
      │         │
      │         └── 有实时任务? ──► 返回
      │
      ├── fair_sched_class.pick_next_task()
      │         │
      │         └── 有普通任务? ──► 返回
      │
      └── idle_sched_class.pick_next_task()
                │
                └── 返回 idle 进程
```

---

## CFS 完全公平调度器

### 核心思想

CFS 通过 **虚拟运行时间 (vruntime)** 实现公平调度：
- 每个进程维护一个 vruntime
- vruntime 增长速度与进程权重成反比
- 总是选择 vruntime 最小的进程运行

### vruntime 计算

```
vruntime += 实际运行时间 × (NICE_0_LOAD / 进程权重)

权重表 (nice 值对应的权重):
nice  权重         vruntime 增长倍率
-20   88761        0.12x   (运行慢，得到更多CPU时间)
-10   9548         0.11x
  0   1024         1.00x   (基准)
 10   110          9.31x
 19   15           68.27x  (运行快，得到更少CPU时间)
```

### CFS 红黑树结构

```
                    cfs_rq->tasks_timeline (红黑树)
                              │
                              ▼
                         ┌────────┐
                         │ 根节点  │
                         │vrt=50  │
                         └────┬───┘
                    ┌─────────┴─────────┐
                    ▼                   ▼
               ┌────────┐          ┌────────┐
               │  红    │          │  黑    │
               │vrt=30  │          │vrt=70  │
               └────┬───┘          └────┬───┘
            ┌───────┴───────┐          │
            ▼               ▼          ▼
       ┌────────┐      ┌────────┐  ┌────────┐
       │  黑    │      │  黑    │  │  红    │
       │vrt=20  │      │vrt=40  │  │vrt=90  │
       └────────┘      └────────┘  └────────┘
            ↑
       rb_leftmost ──► vruntime 最小，下次运行
```

### CFS 调度流程

```
scheduler_tick() (时钟中断)
      │
      ▼
task_tick_fair()
      │
      ├── 更新当前进程的 vruntime
      │
      └── 检查是否需要重新调度
               │
               ▼
          check_preempt_tick()
               │
               ├── 计算理想运行时间
               │   ideal_runtime = period × (进程权重/队列总权重)
               │
               ├── 实际运行时间 > 理想运行时间?
               │         │
               │         └── 是 ──► resched_task() 设置 TIF_NEED_RESCHED
               │
               └── 当前进程 vruntime > 最小 vruntime + 阈值?
                         │
                         └── 是 ──► resched_task()

schedule() (调度主函数)
      │
      ▼
pick_next_task_fair()
      │
      ├── put_prev_entity() ── 将当前进程放回红黑树
      │
      └── pick_next_entity()
               │
               ▼
          __pick_first_entity()
               │
               └── 返回 rb_leftmost (vruntime 最小的进程)
```

### CFS 核心函数

```c
// kernel/sched_fair.c

// 更新 vruntime
static void update_curr(struct cfs_rq *cfs_rq)
{
    struct sched_entity *curr = cfs_rq->curr;
    u64 now = rq_of(cfs_rq)->clock_task;
    u64 delta_exec;
    
    delta_exec = now - curr->exec_start;
    curr->exec_start = now;
    
    curr->sum_exec_runtime += delta_exec;
    
    // 核心: 计算 vruntime 增量
    curr->vruntime += calc_delta_fair(delta_exec, curr);
    
    update_min_vruntime(cfs_rq);
}

// 计算公平的 delta
static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
    if (se->load.weight != NICE_0_LOAD)
        delta = __calc_delta(delta, NICE_0_LOAD, &se->load);
    return delta;
}

// 选择下一个进程
static struct sched_entity *pick_next_entity(struct cfs_rq *cfs_rq)
{
    struct rb_node *left = cfs_rq->rb_leftmost;
    
    if (!left)
        return NULL;
    
    return rb_entry(left, struct sched_entity, run_node);
}
```

---

## 实时调度器

### RT 优先级

```
优先级范围:
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  0 ─────────────────── 99 │ 100 ────────────────────────────── 139         │
│  ◄── 实时优先级 (RT) ────►│◄──────── 普通优先级 (nice) ──────────►         │
│       (SCHED_FIFO/RR)     │         (SCHED_NORMAL/BATCH)                    │
│                           │                                                  │
│  数值越小优先级越高        │  对应 nice -20 到 nice +19                       │
│                           │                                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### RT 运行队列

```c
// kernel/sched.c
struct rt_rq {
    struct rt_prio_array active;        // 优先级数组
    unsigned long rt_nr_running;        // 运行中的 RT 任务数
    
    u64 rt_time;                        // 已使用的 RT 时间
    u64 rt_runtime;                     // RT 时间限制
    
    int overloaded;                     // 是否过载
    struct plist_head pushable_tasks;   // 可推送任务
};

struct rt_prio_array {
    DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1);  // 优先级位图
    struct list_head queue[MAX_RT_PRIO];    // 每优先级一个队列
};
```

### RT 调度流程

```
                    RT 优先级数组
                          
   bitmap:  [1][1][0][0][1][0]...[0]
             ↓  ↓        ↓
   优先级:   0  1        4
             │  │        │
             ▼  ▼        ▼
   queue[0]: task_a ─► task_b
   queue[1]: task_c
   queue[4]: task_d ─► task_e ─► task_f
             
pick_next_task_rt():
   1. 找到 bitmap 中第一个置位的优先级 (最高优先级)
   2. 返回该优先级队列的第一个任务
   
SCHED_FIFO: 一直运行直到阻塞或更高优先级抢占
SCHED_RR:   时间片轮转，时间片用完后移到队列尾部
```

---

## 调度时机与上下文切换

### 调度时机

```
1. 进程主动放弃 CPU
   ├── 调用 schedule() 
   ├── 等待 I/O (sleep)
   └── 等待锁 (mutex_lock)

2. 时钟中断 (scheduler_tick)
   ├── 进程时间片用完
   └── 更高优先级进程就绪

3. 进程唤醒 (wake_up)
   └── 唤醒的进程优先级更高

4. 进程创建 (fork)
   └── 子进程可能抢占父进程

5. 进程优先级改变
   └── setpriority, sched_setscheduler

6. 处理器被添加/移除
   └── CPU 热插拔
```

### TIF_NEED_RESCHED 标志

```c
// 设置重新调度标志
void resched_task(struct task_struct *p)
{
    set_tsk_need_resched(p);
    // 设置 thread_info->flags 的 TIF_NEED_RESCHED 位
}

// 检查点 (会检查 TIF_NEED_RESCHED):
// - 系统调用返回用户空间前
// - 中断返回用户空间前
// - 中断返回内核空间前 (如果允许内核抢占)
```

### 上下文切换流程

```
schedule()
    │
    ▼
__schedule()
    │
    ├── 禁用抢占 preempt_disable()
    │
    ├── 获取当前 rq 锁
    │
    ├── 如果当前进程需要睡眠，从运行队列移除
    │
    ├── pick_next_task() ── 选择下一个进程
    │
    ├── 如果 next != prev:
    │       │
    │       ▼
    │   context_switch()
    │       │
    │       ├── prepare_task_switch()
    │       │
    │       ├── switch_mm()  ── 切换地址空间
    │       │       │
    │       │       ├── 更新 CR3 寄存器 (页表基址)
    │       │       └── 刷新 TLB (如需要)
    │       │
    │       └── switch_to()  ── 切换 CPU 上下文
    │               │
    │               ├── 保存 prev 的寄存器到内核栈
    │               │   (EIP, ESP, EBP, EBX, ESI, EDI)
    │               │
    │               ├── 切换内核栈指针
    │               │
    │               └── 恢复 next 的寄存器
    │
    └── 释放锁，启用抢占
```

### switch_to 宏 (x86)

```c
// arch/x86/include/asm/system.h
#define switch_to(prev, next, last)                    \
do {                                                   \
    asm volatile(                                      \
        "pushfl\n\t"           /* 保存标志 */         \
        "pushl %%ebp\n\t"      /* 保存 EBP */        \
        "movl %%esp,%[prev_sp]\n\t" /* 保存 ESP */   \
        "movl %[next_sp],%%esp\n\t" /* 切换 ESP */   \
        "movl $1f,%[prev_ip]\n\t"   /* 保存返回地址 */\
        "pushl %[next_ip]\n\t"      /* 压入返回地址 */\
        "jmp __switch_to\n"    /* 调用 C 函数 */     \
        "1:\t"                 /* next 返回到这里 */ \
        "popl %%ebp\n\t"       /* 恢复 EBP */        \
        "popfl"                /* 恢复标志 */         \
        : /* 输出 */                                  \
        : /* 输入 */                                  \
        : "memory");                                   \
} while (0)
```

---

## 多核负载均衡

### 调度域层次

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           调度域 (Scheduling Domains)                        │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        NUMA 域 (跨节点)                              │   │
│   │                                                                      │   │
│   │   ┌─────────────────────────┐   ┌─────────────────────────┐         │   │
│   │   │       Node 0 域         │   │       Node 1 域         │         │   │
│   │   │                         │   │                         │         │   │
│   │   │ ┌─────────┐ ┌─────────┐ │   │ ┌─────────┐ ┌─────────┐ │         │   │
│   │   │ │ 物理核 0 │ │ 物理核 1 │ │   │ │ 物理核 2 │ │ 物理核 3 │ │         │   │
│   │   │ │         │ │         │ │   │ │         │ │         │ │         │   │
│   │   │ │CPU0 CPU1│ │CPU2 CPU3│ │   │ │CPU4 CPU5│ │CPU6 CPU7│ │         │   │
│   │   │ │(超线程) │ │(超线程) │ │   │ │(超线程) │ │(超线程) │ │         │   │
│   │   │ └─────────┘ └─────────┘ │   │ └─────────────────────┘ │         │   │
│   │   └─────────────────────────┘   └─────────────────────────┘         │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   负载均衡频率: 同核 SMT > 同 Node > 跨 Node                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 负载均衡流程

```
load_balance() (软中断或定时器触发)
      │
      ├── find_busiest_group()
      │         │
      │         └── 在调度域中找到最忙的调度组
      │
      ├── find_busiest_queue()
      │         │
      │         └── 在调度组中找到最忙的 CPU
      │
      └── move_tasks()
                │
                ├── 从最忙 CPU 迁移任务到当前 CPU
                │
                └── 更新两个 CPU 的负载统计

迁移条件:
  - 负载不均衡超过阈值
  - 任务的 cpus_allowed 允许在目标 CPU 运行
  - 任务没有被 pin 到当前 CPU
```

### push/pull 迁移

```
RT 任务迁移:
  
  push_rt_task():
    当前 CPU 有多个 RT 任务时，
    将非最高优先级任务推送到其他 CPU

  pull_rt_task():
    当前 CPU 即将运行低优先级任务时，
    尝试从其他 CPU 拉取高优先级 RT 任务
```

---

## 关键源码文件

| 文件 | 功能 |
|------|------|
| `kernel/sched.c` | 调度器核心 |
| `kernel/sched_fair.c` | CFS 调度类 |
| `kernel/sched_rt.c` | RT 调度类 |
| `kernel/sched_idletask.c` | Idle 调度类 |
| `kernel/sched_stoptask.c` | Stop 调度类 |
| `kernel/sched_cpupri.c` | CPU 优先级管理 |
| `kernel/sched_clock.c` | 调度时钟 |
| `kernel/sched_debug.c` | 调度调试信息 |
| `kernel/sched_autogroup.c` | 自动分组调度 |
| `arch/x86/kernel/process.c` | x86 上下文切换 |

---

## 总结

### 调度器核心机制

1. **调度类层次**: stop > rt > fair > idle
2. **CFS 公平调度**: vruntime 红黑树
3. **RT 实时调度**: 优先级数组
4. **负载均衡**: 调度域层次结构

### 关键概念

| 概念 | 说明 |
|------|------|
| vruntime | 虚拟运行时间，CFS 公平性基础 |
| nice | 普通进程优先级 (-20 到 +19) |
| rt_priority | 实时优先级 (0 到 99) |
| 时间片 | RT 默认 100ms |
| TIF_NEED_RESCHED | 需要重新调度标志 |

### 设计亮点

1. **O(log N) 选择**: CFS 红黑树
2. **O(1) RT 选择**: 优先级位图
3. **Per-CPU 队列**: 减少锁竞争
4. **层次化负载均衡**: 优先同核/同节点

---

*本文档基于 Linux 3.2 内核源码分析*

