# Template Method vs Strategy: Kernel Boundary Clarification

## The Fundamental Distinction

```
+=============================================================================+
|                    TEMPLATE METHOD vs STRATEGY                               |
+=============================================================================+

    TEMPLATE METHOD                        STRATEGY
    ===============                        ========

    Framework defines algorithm            Framework defines WHAT to call
    with FIXED structure                   but not HOW algorithm works

    +---------------------------+          +---------------------------+
    |  framework_function() {   |          |  framework_function() {   |
    |      pre_step();          |          |      strategy->algo();    |
    |      ops->hook();   <--+  |          |  }                        |
    |      post_step();       | |          +---------------------------+
    |  }                      | |                    |
    +-------------------------|-+                    |
                              |                      v
                              |          +---------------------------+
                              |          |  algo_impl() {            |
                              +--------->|      // Entire algorithm  |
                                         |      step1();             |
                                         |      step2();             |
                                         |      step3();             |
                                         |  }                        |
                                         +---------------------------+

    FRAMEWORK CONTROLS:                    FRAMEWORK CONTROLS:
    - Entire algorithm structure           - Only which algo to call
    - When hook runs                       - Nothing else
    - What runs before/after
    - Error handling

    IMPLEMENTATION CONTROLS:               IMPLEMENTATION CONTROLS:
    - Only the specific work               - Entire algorithm
    - Nothing else                         - All steps
                                           - Internal ordering
```

**中文说明：**

模板方法与策略模式的根本区别：模板方法中，框架定义具有固定结构的算法，控制钩子何时运行、前后执行什么、错误处理；实现层只控制特定工作。策略模式中，框架只定义调用什么算法，不控制算法内部；实现层控制整个算法、所有步骤和内部顺序。

---

## Why Template Method is Preferred in Core Subsystems

### Control Flow Safety

```
    VFS READ PATH (Template Method):

    +--------------------------------------------------+
    |  vfs_read() controls EVERYTHING:                 |
    |                                                  |
    |  User calls read()                               |
    |       |                                          |
    |       v                                          |
    |  +--------------------------------------------+  |
    |  | vfs_read()                                 |  |
    |  |   1. Check file permissions      [FIXED]  |  |
    |  |   2. Security hook (LSM)         [FIXED]  |  |
    |  |   3. f_op->read()                [HOOK]   |  |
    |  |   4. Update access time          [FIXED]  |  |
    |  |   5. fsnotify                    [FIXED]  |  |
    |  +--------------------------------------------+  |
    |                                                  |
    |  Filesystem CANNOT skip steps 1, 2, 4, 5        |
    +--------------------------------------------------+

    IF VFS USED STRATEGY PATTERN (DANGEROUS):

    +--------------------------------------------------+
    |  User calls read()                               |
    |       |                                          |
    |       v                                          |
    |  +--------------------------------------------+  |
    |  | ext4_read_strategy()                       |  |
    |  |   // ext4 controls everything!             |  |
    |  |   // Might forget security check           |  |
    |  |   // Might skip fsnotify                   |  |
    |  |   // Inconsistent behavior                 |  |
    |  +--------------------------------------------+  |
    +--------------------------------------------------+
```

**中文说明：**

为什么核心子系统优先使用模板方法：在VFS读取路径中，`vfs_read()`控制一切，文件系统不能跳过权限检查、安全钩子、访问时间更新或fsnotify。如果使用策略模式，ext4可能忘记安全检查、跳过fsnotify、行为不一致。

### Invariant Enforcement

```
    TEMPLATE METHOD GUARANTEES:

    +----------------------------------------------------------+
    | INVARIANT: Lock held during hook execution               |
    |                                                          |
    |   template_function() {                                  |
    |       mutex_lock(&lock);    // <-- GUARANTEED            |
    |       ops->hook();          // Implementation runs here  |
    |       mutex_unlock(&lock);  // <-- GUARANTEED            |
    |   }                                                      |
    |                                                          |
    | Implementation can RELY on lock being held               |
    | Implementation CANNOT accidentally unlock early          |
    +----------------------------------------------------------+

    STRATEGY CANNOT GUARANTEE THIS:

    +----------------------------------------------------------+
    | NO INVARIANT:                                            |
    |                                                          |
    |   caller() {                                             |
    |       strategy->algorithm();  // Who locks?              |
    |   }                                                      |
    |                                                          |
    | Each strategy implementation must handle its own locking |
    | Inconsistent or missing locking is possible              |
    +----------------------------------------------------------+
```

---

## When Strategy is Appropriate in Kernel

Strategy is used when **entire algorithms are truly interchangeable** without ordering constraints:

### Example: I/O Scheduler

```c
/*
 * STRATEGY PATTERN: I/O Schedulers
 * 
 * Different scheduling algorithms are completely independent.
 * Each controls its own ordering, merging, and dispatch.
 */

struct elevator_ops {
    /* Complete algorithm replacement */
    elevator_merge_fn *elevator_merge_fn;
    elevator_dispatch_fn *elevator_dispatch_fn;
    elevator_add_req_fn *elevator_add_req_fn;
    /* ... */
};

/* CFQ, Deadline, NOOP are complete algorithm replacements */
/* Framework just calls them, doesn't wrap them */
```

### Example: TCP Congestion Control

```c
/*
 * STRATEGY PATTERN: TCP Congestion Control
 * 
 * Each algorithm is a complete, self-contained strategy.
 * Framework calls hooks but doesn't wrap with pre/post steps.
 */

struct tcp_congestion_ops {
    void (*ssthresh)(struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    /* ... */
};

/* Reno, CUBIC, Vegas are independent algorithms */
/* Each manages its own state machine */
```

### Why Strategy Works Here

```
    I/O SCHEDULER AS STRATEGY:

    +----------------------------------------------------------+
    |  WHY STRATEGY IS APPROPRIATE:                            |
    |                                                          |
    |  1. Algorithms are truly independent                     |
    |     - CFQ has its own fairness logic                     |
    |     - Deadline has its own timing logic                  |
    |     - They don't share a common "skeleton"               |
    |                                                          |
    |  2. No universal pre/post steps needed                   |
    |     - No security checks between scheduler steps         |
    |     - No notifications required after each step          |
    |                                                          |
    |  3. Algorithm controls its own invariants                |
    |     - Each scheduler manages its own locking             |
    |     - Internal state is scheduler-specific               |
    |                                                          |
    |  4. Safety is at algorithm boundary                      |
    |     - Block layer validates BEFORE calling scheduler     |
    |     - Scheduler output is validated AFTER                |
    +----------------------------------------------------------+
```

---

## Comparison Example

### Template Method Example: vfs_read()

```c
/*
 * TEMPLATE METHOD: VFS Read
 * Framework wraps implementation with mandatory steps
 */

ssize_t vfs_read(struct file *file, char __user *buf, 
                 size_t count, loff_t *pos)
{
    ssize_t ret;

    /* ===== PRE-HOOK: Mandatory steps ===== */
    if (!(file->f_mode & FMODE_READ))      /* Permission */
        return -EBADF;

    ret = rw_verify_area(READ, file, pos, count);  /* Validation */
    if (ret < 0)
        return ret;

    ret = security_file_permission(file, MAY_READ);  /* Security */
    if (ret)
        return ret;

    /* ===== HOOK: Filesystem-specific read ===== */
    ret = file->f_op->read(file, buf, count, pos);

    /* ===== POST-HOOK: Mandatory steps ===== */
    if (ret > 0)
        fsnotify_access(file);  /* Notification */
    inc_syscr(current);         /* Statistics */

    return ret;
}
```

### Strategy Example: I/O Scheduler Selection

```c
/*
 * STRATEGY PATTERN: I/O Scheduler
 * Framework just calls the selected algorithm
 */

void elv_dispatch_sort(struct request_queue *q)
{
    struct elevator_queue *e = q->elevator;

    /* Just call the strategy - no pre/post wrapping */
    if (e->ops->elevator_dispatch_fn)
        e->ops->elevator_dispatch_fn(q, 0);
}

/* Each scheduler is a complete algorithm */
int cfq_dispatch_requests(struct request_queue *q, int force)
{
    /* CFQ's complete dispatch logic */
    /* Own fairness, timing, grouping */
    /* No framework-imposed structure */
}

int deadline_dispatch_requests(struct request_queue *q, int force)
{
    /* Deadline's complete dispatch logic */
    /* Own deadline tracking, batch handling */
    /* Different from CFQ's structure */
}
```

---

## Misuse Example: What NOT to Do

### Anti-Pattern: Strategy Where Template Method Needed

```c
/*
 * BAD: Using Strategy pattern for file read
 * This is dangerous and would never be accepted in kernel
 */

/* WRONG: Each filesystem implements entire read path */
struct file_strategy {
    ssize_t (*full_read)(struct file *file, char *buf, size_t count);
};

/* ext4's "strategy" - DANGEROUS */
ssize_t ext4_full_read(struct file *file, char *buf, size_t count)
{
    /* Oops, forgot security check! */
    /* Oops, no fsnotify! */
    /* Each filesystem does its own thing */
    return ext4_internal_read(file, buf, count);
}

/* nfs's "strategy" - INCONSISTENT */
ssize_t nfs_full_read(struct file *file, char *buf, size_t count)
{
    /* Different security approach? */
    nfs_check_something();
    return nfs_internal_read(file, buf, count);
    /* Forgot statistics! */
}

/* PROBLEMS:
 * 1. Security checks inconsistent or missing
 * 2. No central audit point
 * 3. fsnotify might be skipped
 * 4. Statistics unreliable
 * 5. Adding new mandatory step requires changing ALL filesystems
 */
```

### Why This Misuse is Dangerous

```
    MISUSE CONSEQUENCES:

    +----------------------------------------------------------+
    |  If VFS used Strategy instead of Template Method:        |
    +----------------------------------------------------------+
    |                                                          |
    |  SECURITY RISK:                                          |
    |  - Some filesystems might skip security_file_permission  |
    |  - SELinux/AppArmor could be bypassed                    |
    |  - No central enforcement point                          |
    |                                                          |
    |  AUDIT FAILURE:                                          |
    |  - Cannot reliably log all file access                   |
    |  - inotify/fanotify would have gaps                      |
    |  - Forensic analysis impossible                          |
    |                                                          |
    |  MAINTENANCE NIGHTMARE:                                  |
    |  - Adding new security hook = change 50+ filesystems     |
    |  - Each filesystem could diverge                         |
    |  - Testing all paths infeasible                          |
    |                                                          |
    +----------------------------------------------------------+
```

**中文说明：**

如果VFS使用策略模式而不是模板方法：安全风险——某些文件系统可能跳过安全检查，SELinux可能被绕过；审计失败——无法可靠记录所有文件访问；维护噩梦——添加新安全钩子需要修改50多个文件系统。这就是为什么核心子系统必须使用模板方法。

---

## Decision Guide

```
+=============================================================================+
|                    WHEN TO USE WHICH PATTERN                                 |
+=============================================================================+

    USE TEMPLATE METHOD WHEN:              USE STRATEGY WHEN:
    ========================               ==================

    [ ] Mandatory pre/post steps exist     [ ] Algorithm is self-contained
    [ ] Security checks required           [ ] No universal pre/post steps
    [ ] Invariants must be enforced        [ ] Each impl manages own state
    [ ] Central audit needed               [ ] Algorithms are independent
    [ ] Lifecycle must be controlled       [ ] No shared skeleton possible
    [ ] Order matters                      [ ] Complete replacement OK

    EXAMPLES:                              EXAMPLES:
    - File operations (VFS)                - I/O schedulers
    - Network transmit                     - TCP congestion control
    - Device probe                         - Memory allocators
    - Block I/O submission                 - Compression algorithms

    RULE OF THUMB:
    +----------------------------------------------------------+
    |  If you need to guarantee something happens              |
    |  BEFORE and AFTER the implementation runs:               |
    |                                                          |
    |      --> Use Template Method                             |
    |                                                          |
    |  If the entire algorithm is replaceable with no          |
    |  required wrapping:                                      |
    |                                                          |
    |      --> Use Strategy                                    |
    +----------------------------------------------------------+
```

**中文说明：**

决策指南：当存在必须的前后步骤、需要安全检查、必须强制执行不变量、需要集中审计、必须控制生命周期、顺序重要时，使用模板方法。当算法自包含、没有通用前后步骤、每个实现管理自己的状态、算法相互独立、没有共享骨架、可以完全替换时，使用策略模式。经验法则：如果需要保证在实现运行前后发生某些事情，用模板方法；如果整个算法可替换且无需包装，用策略模式。

---

## Summary Table

| Aspect | Template Method | Strategy |
|--------|-----------------|----------|
| **Algorithm structure** | Fixed by framework | Defined by implementation |
| **Pre/post steps** | Mandatory, framework-controlled | None or implementation-defined |
| **Control flow** | Framework owns completely | Implementation owns completely |
| **Invariants** | Framework enforces | Implementation manages |
| **Adding new steps** | Change framework once | Change all implementations |
| **Security** | Centralized | Distributed |
| **Use in kernel** | VFS, net TX, device model | I/O sched, TCP CC, allocators |

---

## Key Insight

> **Template Method is about control flow ownership.**
> 
> When the framework MUST control what happens before, during, and after an operation—use Template Method.
>
> When the implementation owns the entire algorithm and the framework just selects which one to use—use Strategy.
>
> In Linux kernel core paths (I/O, networking, device management), Template Method dominates because **the framework must enforce invariants that individual implementations cannot be trusted to maintain consistently.**
