# Strategy vs Template Method: Kernel Boundary (Revisited)

## The Hard Boundary

```
+=============================================================================+
|                    THE FUNDAMENTAL DISTINCTION                               |
+=============================================================================+

    TEMPLATE METHOD                        STRATEGY
    ===============                        ========

    "I control what happens,               "I know when to act,
     you fill in one piece"                 you decide how to act"

    CONTROL FLOW OWNERSHIP:                ALGORITHM OWNERSHIP:

    +-------------------+                  +-------------------+
    | vfs_read() {      |                  | schedule() {      |
    |   check_perms();  | Framework        |   ...             |
    |   security();     | owns all         |   class->         |
    |   f_op->read();   | of this          |    pick_next();   | Strategy
    |   fsnotify();     |                  |   class->         | owns all
    |   inc_syscr();    |                  |    put_prev();    | of this
    | }                 |                  | }                 |
    +-------------------+                  +-------------------+

    Framework: "I guarantee pre/post"      Core: "You decide everything"
    Implementation: "I just do the work"   Strategy: "I own the algorithm"
```

**中文说明：**

模板方法与策略的根本区别：模板方法说"我控制发生什么，你填充一个部分"——框架拥有整个控制流，实现只做具体工作。策略说"我知道何时行动，你决定如何行动"——核心知道何时，策略拥有整个算法。`vfs_read()`中框架拥有权限检查、安全检查、通知、统计，实现只填充`read`。`schedule()`中策略拥有`pick_next`、`put_prev`等整个算法。

---

## Why Strategy Gives Power to Implementation

```
    STRATEGY: Implementation Controls Algorithm
    ===========================================

    +-------------------------------------------------------------------+
    |  The core DELEGATES decisions to the strategy                     |
    |                                                                   |
    |  schedule() {                                                     |
    |      // Core doesn't know HOW to pick next task                   |
    |      // Core doesn't know HOW long task should run                |
    |      // Core doesn't know WHAT data structures to use             |
    |                                                                   |
    |      next = class->pick_next_task(rq);  // Strategy decides!      |
    |      class->put_prev_task(rq, prev);    // Strategy decides!      |
    |  }                                                                |
    |                                                                   |
    |  CFS STRATEGY:                    RT STRATEGY:                    |
    |  - Uses red-black tree            - Uses priority arrays          |
    |  - Picks leftmost node            - Picks highest priority        |
    |  - Updates vruntime               - Checks time slices            |
    |                                                                   |
    |  COMPLETELY DIFFERENT ALGORITHMS                                  |
    |  Core doesn't care - just asks for decisions                      |
    +-------------------------------------------------------------------+
```

### Strategy's Power

| Aspect | Strategy Controls |
|--------|-------------------|
| **Data structures** | CFS uses rb-tree, RT uses arrays |
| **Decision criteria** | CFS uses vruntime, RT uses priority |
| **Internal state** | Each strategy has private state |
| **Event handling** | Each strategy responds differently |
| **Metrics** | Each strategy tracks different things |

---

## Why Template Method Restricts Implementation

```
    TEMPLATE METHOD: Implementation is Constrained
    ==============================================

    +-------------------------------------------------------------------+
    |  The framework WRAPS the implementation                           |
    |                                                                   |
    |  vfs_read() {                                                     |
    |      // Framework GUARANTEES these happen                         |
    |      check_mode();                  // MUST happen                |
    |      ret = security_check();        // MUST happen                |
    |      if (ret) return ret;                                         |
    |                                                                   |
    |      ret = f_op->read(...);         // Implementation runs here   |
    |                                                                   |
    |      // Framework GUARANTEES these happen                         |
    |      if (ret > 0) fsnotify();       // MUST happen                |
    |      inc_syscr();                   // MUST happen                |
    |  }                                                                |
    |                                                                   |
    |  IMPLEMENTATION CANNOT:                                           |
    |  - Skip security check                                            |
    |  - Avoid fsnotify                                                 |
    |  - Control when it runs                                           |
    |  - Change the sequence                                            |
    +-------------------------------------------------------------------+
```

### Implementation's Restrictions

| Aspect | Framework Controls |
|--------|-------------------|
| **Entry point** | Framework function is called |
| **Pre-conditions** | Always checked before implementation |
| **Post-processing** | Always happens after implementation |
| **Locking** | Framework acquires/releases |
| **Error handling** | Framework decides on errors |

---

## Why Confusing the Two Causes Architectural Bugs

### Bug Type 1: Using Strategy Where Template Method Needed

```c
/*
 * WRONG: Strategy pattern for file operations
 * This would be a security disaster
 */

/* Bad design: filesystem as strategy */
struct file_strategy {
    ssize_t (*full_read)(struct file *file, char *buf, size_t count);
};

ssize_t bad_vfs_read(struct file *file, char *buf, size_t count)
{
    /* Just call the strategy - no wrapping */
    return file->strategy->full_read(file, buf, count);
}

/* ext4's implementation - DANGEROUS */
ssize_t ext4_full_read(struct file *file, char *buf, size_t count)
{
    /* Security check? Optional! */
    /* fsnotify? Who knows! */
    /* Statistics? Maybe forgot! */
    return do_read_somehow(file, buf, count);
}

/*
 * PROBLEMS:
 * 1. Security can be bypassed
 * 2. Audit trail incomplete
 * 3. Each filesystem implements differently
 * 4. No central enforcement point
 */
```

**中文说明：**

在需要模板方法的地方使用策略会导致安全灾难。如果VFS使用策略模式而非模板方法，每个文件系统自己实现完整的读取，安全检查可能被跳过、审计不完整、每个文件系统实现不同、没有中央强制点。

### Bug Type 2: Using Template Method Where Strategy Needed

```c
/*
 * WRONG: Template Method pattern for scheduling
 * This would be inflexible and inefficient
 */

/* Bad design: scheduler as template method */
struct task_struct *bad_schedule(struct rq *rq)
{
    /* Framework-enforced pre-steps */
    disable_preemption();
    lock_rq(rq);
    check_something();
    
    /* Single hook - just picks next task */
    next = ops->pick_next_task(rq);  /* ONLY this varies */
    
    /* Framework-enforced post-steps */
    do_context_switch();
    unlock_rq(rq);
    enable_preemption();
    
    return next;
}

/*
 * PROBLEMS:
 * 1. Can't have different put_prev_task behaviors
 * 2. Can't have different state update sequences
 * 3. RT and CFS need different handling - impossible!
 * 4. Framework locks at wrong granularity for RT
 */
```

**中文说明：**

在需要策略的地方使用模板方法会导致僵化和低效。如果调度器使用模板方法，只有`pick_next_task`可变，无法有不同的`put_prev_task`行为、无法有不同的状态更新序列、RT和CFS需要不同处理但做不到、框架锁的粒度对RT不合适。

### Bug Type 3: Hybrid Mess

```c
/*
 * WRONG: Confused hybrid design
 * Neither fish nor fowl
 */

/* Half strategy, half template method */
void confused_operation(struct context *ctx)
{
    /* Some framework wrapping... */
    lock(ctx);
    
    /* But then multiple strategy calls... */
    ctx->ops->step_a(ctx);
    ctx->ops->step_b(ctx);
    ctx->ops->step_c(ctx);
    
    /* More framework wrapping... */
    notify(ctx);
    unlock(ctx);
}

/*
 * PROBLEMS:
 * 1. Is framework wrapping mandatory? Who knows!
 * 2. Do step_a, step_b, step_c form one algorithm? Unclear!
 * 3. Can strategy skip notify()? Maybe!
 * 4. Maintainers can't understand the contract
 */
```

---

## Pure Strategy Example

```c
/*
 * PURE STRATEGY: TCP Congestion Control
 * Strategy owns the entire congestion algorithm
 */

/* Strategy interface */
struct tcp_congestion_ops {
    void (*init)(struct sock *sk);
    u32 (*ssthresh)(struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    /* All work together as one algorithm */
};

/* Core usage - NO WRAPPING */
void tcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    /* Just delegate to strategy */
    /* No framework pre-steps */
    /* No framework post-steps */
    inet_csk(sk)->icsk_ca_ops->cong_avoid(sk, ack, in_flight);
}

/* CUBIC implementation - owns entire algorithm */
void cubictcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    struct bictcp *ca = inet_csk_ca(sk);
    
    if (ca->cwnd < ca->ssthresh) {
        /* Slow start - CUBIC's way */
        tcp_slow_start(tp);
    } else {
        /* Congestion avoidance - CUBIC's way */
        bictcp_update(ca, tp->snd_cwnd);
        /* Uses cubic function, maintains cubic state */
    }
}

/*
 * WHY THIS IS CORRECT:
 * - Each congestion algorithm is self-contained
 * - CUBIC, Reno, Vegas are completely different
 * - No universal pre/post steps make sense
 * - Algorithm selection per-socket
 */
```

---

## Pure Template Method Example

```c
/*
 * PURE TEMPLATE METHOD: VFS Read
 * Framework wraps single hook with mandatory steps
 */

/* Hook interface */
struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    /* Other hooks for other operations */
};

/* Template method - MANDATORY WRAPPING */
ssize_t vfs_read(struct file *file, char __user *buf, 
                 size_t count, loff_t *pos)
{
    /* === PRE-HOOK: Mandatory framework steps === */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    
    ret = rw_verify_area(READ, file, pos, count);
    if (ret < 0)
        return ret;
    
    ret = security_file_permission(file, MAY_READ);
    if (ret)
        return ret;
    
    /* === HOOK: Single variation point === */
    ret = file->f_op->read(file, buf, count, pos);
    
    /* === POST-HOOK: Mandatory framework steps === */
    if (ret > 0)
        fsnotify_access(file);
    inc_syscr(current);
    
    return ret;
}

/*
 * WHY THIS IS CORRECT:
 * - Security MUST be checked for all reads
 * - Notification MUST happen for all reads
 * - Statistics MUST be updated for all reads
 * - Filesystem just provides the actual read logic
 */
```

---

## Incorrect Hybrid Example (What NOT to Design)

```c
/*
 * INCORRECT: Don't mix patterns like this
 * This is a maintenance nightmare
 */

/* Bad: Confused interface */
struct confused_ops {
    /* Looks like strategy... */
    void (*algo_part_a)(struct context *ctx);
    void (*algo_part_b)(struct context *ctx);
    
    /* But also has single hooks... */
    int (*check_permission)(struct context *ctx);
};

/* Bad: Confused usage */
void confused_function(struct context *ctx)
{
    /* Template-method style wrapping... */
    lock(ctx);
    
    /* But is this mandatory? */
    if (ctx->ops->check_permission)
        ctx->ops->check_permission(ctx);
    
    /* Strategy-style algorithm... */
    ctx->ops->algo_part_a(ctx);
    ctx->ops->algo_part_b(ctx);
    
    /* More wrapping... */
    unlock(ctx);
    
    /* Is this always called? */
    maybe_notify(ctx);
}

/*
 * PROBLEMS:
 * 1. Is lock() required? Implementation might assume yes
 * 2. Is check_permission part of the algorithm? Unclear
 * 3. Can implementation skip algo_part_b? Maybe
 * 4. Is notify mandatory? Who knows
 * 5. Contract is unclear, bugs will follow
 */
```

---

## Decision Matrix

```
+=============================================================================+
|                    PATTERN SELECTION GUIDE                                   |
+=============================================================================+

    QUESTION                           TEMPLATE METHOD    STRATEGY
    ========                           ===============    ========

    Must enforce pre/post steps?       YES               No
    
    Is entire algorithm replaceable?   No                YES
    
    Single hook per operation?         YES               No (multiple)
    
    Framework owns control flow?       YES               No
    
    Need runtime algorithm switch?     Unusual           YES
    
    Security/audit must be central?    YES               N/A
    
    Multiple ops form one unit?        No                YES
    
    Policy decision?                   No                YES
    
    Lifecycle management?              YES               No

    USE TEMPLATE METHOD FOR:           USE STRATEGY FOR:
    - File operations (VFS)            - Scheduling policies
    - Network TX path                  - Congestion control
    - Device lifecycle                 - I/O scheduling
    - Block I/O submission             - Memory policies
                                       - Security modules
```

**中文说明：**

模式选择指南：必须强制前后步骤？——模板方法。整个算法可替换？——策略。每操作一个钩子？——模板方法。框架拥有控制流？——模板方法。需要运行时算法切换？——策略。安全/审计必须集中？——模板方法。多个ops形成一个单元？——策略。策略决策？——策略。生命周期管理？——模板方法。

---

## Summary

| Aspect | Template Method | Strategy |
|--------|-----------------|----------|
| **Control flow** | Framework owns | Core delegates |
| **Hook count** | One per function | Multiple per algorithm |
| **Pre/post steps** | Mandatory | None |
| **State ownership** | Framework | Strategy |
| **Algorithm completeness** | Partial (one hook) | Complete (all ops) |
| **Selection timing** | Object creation | Runtime |
| **Use case** | Lifecycle, security | Policy, algorithms |
| **Kernel examples** | VFS, network TX | Scheduler, TCP CC |

> **The Golden Rule:**
>
> If you need to **guarantee** something happens before and after, use **Template Method**.
>
> If you need to **replace** an entire algorithm, use **Strategy**.
>
> If you're not sure which, ask: "Does the framework need to enforce invariants, or does it just need to delegate a decision?"
