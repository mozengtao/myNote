# Learning Path and Source Reading Guide (Linux v3.2)

## Overview

```
+=============================================================================+
|                    LEARNING PATH OVERVIEW                                    |
+=============================================================================+

    RECOMMENDED ORDER:

    1. TCP Congestion Control (Clearest strategy)
       |
       +---> net/ipv4/tcp_cong.c
       +---> net/ipv4/tcp_cubic.c
       +---> include/net/tcp.h

    2. I/O Scheduler (Complete algorithm swap)
       |
       +---> block/elevator.c
       +---> block/deadline-iosched.c
       +---> block/noop-iosched.c

    3. CPU Scheduler (Most complex strategy)
       |
       +---> kernel/sched.c
       +---> kernel/sched_fair.c
       +---> kernel/sched_rt.c

    4. Memory Policy (Runtime selection)
       |
       +---> mm/mempolicy.c
       +---> include/linux/mempolicy.h

    5. LSM (Security policy)
       |
       +---> security/security.c
       +---> security/selinux/hooks.c
```

**中文说明：**

推荐学习顺序：(1) TCP拥塞控制——最清晰的策略实现；(2) I/O调度器——完整的算法替换；(3) CPU调度器——最复杂的策略；(4) 内存策略——运行时选择；(5) LSM——安全策略。

---

## Phase 1: TCP Congestion Control

### Files to Read

| File | What to Look For |
|------|------------------|
| `include/net/tcp.h` | `struct tcp_congestion_ops` definition |
| `net/ipv4/tcp_cong.c` | Registration, selection mechanism |
| `net/ipv4/tcp_cubic.c` | CUBIC algorithm implementation |
| `net/ipv4/tcp_input.c` | Where congestion ops are called |

### Key Structures

```c
/* Strategy interface - in include/net/tcp.h */
struct tcp_congestion_ops {
    void (*init)(struct sock *sk);
    u32 (*ssthresh)(struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);
    char name[TCP_CA_NAME_MAX];
};

/* Per-socket strategy selection */
struct inet_connection_sock {
    /* ... */
    const struct tcp_congestion_ops *icsk_ca_ops;
    /* ... */
};
```

### Key Functions to Trace

```
    STRATEGY REGISTRATION:
    ======================

    net/ipv4/tcp_cong.c:32
    int tcp_register_congestion_control(struct tcp_congestion_ops *ca)

    LOOK FOR:
    - How algorithm is added to available list
    - Module ownership handling


    STRATEGY SELECTION:
    ===================

    net/ipv4/tcp_cong.c:180
    int tcp_set_congestion_control(struct sock *sk, const char *name)

    LOOK FOR:
    - Finding algorithm by name
    - Switching from old to new
    - Initialization of new algorithm


    STRATEGY USAGE:
    ===============

    net/ipv4/tcp_input.c:~3200
    void tcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)

    LOOK FOR:
    - Direct call to strategy function
    - No framework wrapping
```

### Reading Exercise

```
    EXERCISE 1: Trace Congestion Control

    1. Open include/net/tcp.h
    2. Find struct tcp_congestion_ops
    3. List all the functions in the ops table
    4. Understand: these ALL work together as one algorithm

    5. Open net/ipv4/tcp_cubic.c
    6. Find cubictcp_cong_avoid()
    7. Trace how it updates cwnd
    8. Note: completely different from Reno

    9. Open net/ipv4/tcp_cong.c
    10. Find tcp_set_congestion_control()
    11. Understand: setsockopt() ends up here

    QUESTIONS:
    - How does core know which algorithm to use?
    - Can two sockets use different algorithms?
    - What happens during algorithm switch?
```

---

## Phase 2: I/O Scheduler (Elevator)

### Files to Read

| File | What to Look For |
|------|------------------|
| `include/linux/elevator.h` | `struct elevator_ops` definition |
| `block/elevator.c` | Registration, selection, dispatching |
| `block/deadline-iosched.c` | Deadline scheduler implementation |
| `block/noop-iosched.c` | Simplest scheduler (NOOP) |
| `block/cfq-iosched.c` | Complex fair queuing scheduler |

### Key Structures

```c
/* Strategy interface - in include/linux/elevator.h */
struct elevator_ops {
    elevator_merge_fn *elevator_merge_fn;
    elevator_dispatch_fn *elevator_dispatch_fn;
    elevator_add_req_fn *elevator_add_req_fn;
    elevator_queue_empty_fn *elevator_queue_empty_fn;
    elevator_init_fn *elevator_init_fn;
    elevator_exit_fn *elevator_exit_fn;
    /* ... more ops ... */
};

/* Per-queue strategy selection */
struct request_queue {
    /* ... */
    struct elevator_queue *elevator;
    /* ... */
};
```

### Key Functions to Trace

```
    STRATEGY REGISTRATION:
    ======================

    block/elevator.c:~100
    int elv_register(struct elevator_type *e)

    LOOK FOR:
    - Adding to elevator_list
    - Default elevator selection


    STRATEGY SELECTION:
    ===================

    block/elevator.c:~900
    static int elevator_switch(struct request_queue *q, struct elevator_type *new_e)

    LOOK FOR:
    - Draining existing requests
    - Switching elevators
    - Initialization


    STRATEGY USAGE:
    ===============

    block/elevator.c:~600
    int elv_dispatch_sort(struct request_queue *q)

    LOOK FOR:
    - Call to elevator_dispatch_fn
    - No wrapping - direct delegation
```

### Reading Exercise

```
    EXERCISE 2: Compare Schedulers

    1. Open block/noop-iosched.c
    2. Find noop_dispatch()
    3. Note: just returns from FIFO queue

    4. Open block/deadline-iosched.c
    5. Find deadline_dispatch_requests()
    6. Note: maintains sorted list + FIFOs
    7. Much more complex than NOOP!

    5. Open block/elevator.c
    6. Find elv_dispatch_sort()
    7. See how it delegates to the elevator

    QUESTIONS:
    - How do you change elevator at runtime?
    - What's in /sys/block/sda/queue/scheduler?
    - Why would you choose deadline over cfq?
```

---

## Phase 3: CPU Scheduler

### Files to Read

| File | What to Look For |
|------|------------------|
| `kernel/sched.c` | Core scheduler, `struct sched_class` |
| `kernel/sched_fair.c` | CFS implementation |
| `kernel/sched_rt.c` | Real-time scheduler |
| `include/linux/sched.h` | Task structure with sched_class |

### Key Structures

```c
/* Strategy interface - in kernel/sched.c */
struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    /* ... more ops ... */
};

/* Per-task strategy selection */
struct task_struct {
    /* ... */
    const struct sched_class *sched_class;
    /* ... */
};
```

### Key Functions to Trace

```
    STRATEGY CHAIN:
    ===============

    kernel/sched.c:~150
    #define for_each_class(class) \
        for (class = sched_class_highest; class; class = class->next)

    LOOK FOR:
    - Priority ordering: stop -> rt -> fair -> idle
    - How pick_next_task iterates


    STRATEGY SELECTION:
    ===================

    kernel/sched.c:~4700
    static void __setscheduler(struct rq *rq, struct task_struct *p, int policy)

    LOOK FOR:
    - SCHED_FIFO/RR -> rt_sched_class
    - SCHED_NORMAL -> fair_sched_class


    STRATEGY USAGE:
    ===============

    kernel/sched.c:~4000
    static inline struct task_struct *
    pick_next_task(struct rq *rq)

    LOOK FOR:
    - Iteration through classes
    - Each class's pick_next_task called
    - First non-NULL wins
```

### Reading Exercise

```
    EXERCISE 3: Trace Schedule

    1. Open kernel/sched.c
    2. Find schedule() function (~4100)
    3. Trace to pick_next_task()
    4. See class iteration

    5. Open kernel/sched_fair.c
    6. Find pick_next_task_fair()
    7. Understand rb-tree and vruntime

    8. Open kernel/sched_rt.c
    9. Find pick_next_task_rt()
    10. Understand priority array

    QUESTIONS:
    - How does RT preempt CFS?
    - What's vruntime and why does CFS use it?
    - How does task_tick() differ between classes?
```

---

## Phase 4: Memory Policy

### Files to Read

| File | What to Look For |
|------|------------------|
| `include/linux/mempolicy.h` | `struct mempolicy`, modes |
| `mm/mempolicy.c` | Policy implementation |
| `mm/page_alloc.c` | How policy affects allocation |

### Key Structures

```c
/* Strategy modes - in include/linux/mempolicy.h */
enum {
    MPOL_DEFAULT,
    MPOL_PREFERRED,
    MPOL_BIND,
    MPOL_INTERLEAVE,
};

struct mempolicy {
    atomic_t refcnt;
    unsigned short mode;
    union {
        short preferred_node;
        nodemask_t nodes;
    } v;
};
```

### Key Functions to Trace

```
    STRATEGY SELECTION:
    ===================

    mm/mempolicy.c:~1100
    SYSCALL_DEFINE3(set_mempolicy, ...)

    LOOK FOR:
    - Mode and nodemask validation
    - Policy creation and assignment


    STRATEGY USAGE:
    ===============

    mm/mempolicy.c:~1700
    struct zonelist *policy_zonelist(gfp_t gfp, struct mempolicy *policy)

    LOOK FOR:
    - Switch on policy mode
    - Different node selection per mode
```

---

## Phase 5: LSM

### Files to Read

| File | What to Look For |
|------|------------------|
| `include/linux/security.h` | `struct security_operations` |
| `security/security.c` | Hook invocation |
| `security/selinux/hooks.c` | SELinux implementation |
| `security/apparmor/lsm.c` | AppArmor implementation |

### Key Structures

```c
/* Strategy interface - in include/linux/security.h */
struct security_operations {
    char name[SECURITY_NAME_MAX + 1];

    int (*inode_permission)(struct inode *inode, int mask);
    int (*file_open)(struct file *file, const struct cred *cred);
    int (*bprm_check_security)(struct linux_binprm *bprm);
    /* ... 150+ hooks ... */
};
```

### Key Functions to Trace

```
    STRATEGY REGISTRATION:
    ======================

    security/security.c:~60
    int register_security(struct security_operations *ops)


    STRATEGY USAGE:
    ===============

    security/security.c:~500
    int security_inode_permission(struct inode *inode, int mask)

    LOOK FOR:
    - Direct call to security_ops->inode_permission
    - No wrapping (strategy decides everything)
```

---

## How to Trace Algorithm Replacement

```
    TRACING AN ALGORITHM SWITCH:

    1. Find the selection function:
       - tcp_set_congestion_control()
       - elevator_switch()
       - __setscheduler()
       - set_mempolicy()
       - register_security()

    2. Find where old algorithm is released:
       - old->release() or old->exit()
       - State cleanup

    3. Find where new algorithm is installed:
       - pointer assignment
       - new->init() call

    4. Find where algorithm is used:
       - ops->function_name() calls
       - No wrapper functions

    5. Verify strategy characteristics:
       - Multiple ops called together?
       - No framework pre/post?
       - Strategy owns state?
```

---

## Source File Quick Reference (v3.2)

### TCP Congestion Control

| File | Key Content |
|------|-------------|
| `include/net/tcp.h:~800` | `struct tcp_congestion_ops` |
| `net/ipv4/tcp_cong.c:32` | `tcp_register_congestion_control()` |
| `net/ipv4/tcp_cong.c:180` | `tcp_set_congestion_control()` |
| `net/ipv4/tcp_cubic.c` | CUBIC implementation |

### I/O Scheduler

| File | Key Content |
|------|-------------|
| `include/linux/elevator.h:~40` | `struct elevator_ops` |
| `block/elevator.c:100` | `elv_register()` |
| `block/elevator.c:900` | `elevator_switch()` |
| `block/deadline-iosched.c` | Deadline implementation |

### CPU Scheduler

| File | Key Content |
|------|-------------|
| `kernel/sched.c:~1300` | `struct sched_class` |
| `kernel/sched.c:~4100` | `schedule()` |
| `kernel/sched.c:~4000` | `pick_next_task()` |
| `kernel/sched_fair.c` | CFS implementation |
| `kernel/sched_rt.c` | RT implementation |

### Memory Policy

| File | Key Content |
|------|-------------|
| `include/linux/mempolicy.h` | Policy modes and struct |
| `mm/mempolicy.c:~1100` | `sys_set_mempolicy()` |
| `mm/mempolicy.c:~1700` | `policy_zonelist()` |

### LSM

| File | Key Content |
|------|-------------|
| `include/linux/security.h:~1300` | `struct security_operations` |
| `security/security.c:60` | `register_security()` |
| `security/security.c:500+` | Security hook functions |
