# Kernel-Level Identification Rules for Strategy

## Structural Signals of Strategy Pattern

```
+=============================================================================+
|                    STRATEGY PATTERN ANATOMY                                  |
+=============================================================================+

    STRATEGY OPS TABLE:
    +-----------------------------------------------------------------------+
    |  struct xxx_ops / struct xxx_class {                                  |
    |      /* Multiple functions that work together */                      |
    |      void (*operation_a)(context);     <-- Part of algorithm          |
    |      void (*operation_b)(context);     <-- Part of algorithm          |
    |      int  (*decision_c)(context);      <-- Part of algorithm          |
    |      void (*operation_d)(context);     <-- Part of algorithm          |
    |  };                                                                   |
    |                                                                       |
    |  ALL OPERATIONS FORM ONE COHERENT ALGORITHM                           |
    |  CORE CALLS MULTIPLE OPS FOR ONE LOGICAL OPERATION                    |
    +-----------------------------------------------------------------------+

    CORE USAGE PATTERN:
    +-----------------------------------------------------------------------+
    |  void core_function(context) {                                        |
    |      /* Core might do some mechanism work */                          |
    |      ...                                                              |
    |                                                                       |
    |      /* Then delegate to strategy - multiple calls */                 |
    |      strategy->operation_a(context);                                  |
    |      result = strategy->decision_c(context);                          |
    |      if (result)                                                      |
    |          strategy->operation_b(context);                              |
    |                                                                       |
    |      /* Core does NOT wrap with mandatory pre/post */                 |
    |      /* Strategy controls the algorithm logic */                      |
    |  }                                                                    |
    +-----------------------------------------------------------------------+

    CONTRAST WITH TEMPLATE METHOD:
    +-----------------------------------------------------------------------+
    |  void template_function(context) {                                    |
    |      lock();                      <-- Framework PRE step              |
    |      check_permissions();         <-- Framework PRE step              |
    |      ops->single_hook(context);   <-- ONE hook called                 |
    |      notify();                    <-- Framework POST step             |
    |      unlock();                    <-- Framework POST step             |
    |  }                                                                    |
    +-----------------------------------------------------------------------+
```

**中文说明：**

策略模式的结构信号：策略ops表包含多个协同工作的函数，所有操作形成一个完整的算法。核心使用模式：核心函数可能做一些机制工作，然后委托给策略——调用多个ops函数，核心不用强制的前后步骤包装，策略控制算法逻辑。对比模板方法：模板函数有框架的前后步骤（加锁、检查权限、通知、解锁），中间只调用一个钩子。

---

## How to Identify Algorithm-Level Replaceability

### Signal 1: Multiple Ops Called Together

```
    STRATEGY: Multiple ops for one logical operation

    schedule() calls:
        class->put_prev_task()
        class->pick_next_task()
        class->set_curr_task()

    tcp_cong_avoid() calls:
        ca_ops->cong_avoid()     // or uses state from:
        ca_ops->ssthresh()       // multiple related ops

    elv_dispatch_sort() calls:
        e->ops->elevator_dispatch_fn()
        e->ops->elevator_completed_req_fn()


    TEMPLATE METHOD: One op per framework function

    vfs_read() calls:
        f_op->read()             // ONLY this one hook

    dev_queue_xmit() calls:
        ops->ndo_start_xmit()    // ONLY this one hook
```

### Signal 2: Ops Table Represents Complete Algorithm

```
    STRATEGY OPS: Algorithm completeness
    +---------------------------------------------------------------+
    |  struct sched_class {                                         |
    |      /* Complete scheduling algorithm */                      |
    |      .enqueue_task      // Add to run queue                   |
    |      .dequeue_task      // Remove from run queue              |
    |      .yield_task        // Voluntarily yield                  |
    |      .check_preempt     // Check if should preempt            |
    |      .pick_next_task    // Select next task                   |
    |      .put_prev_task     // Handle preempted task              |
    |      .task_tick         // Timer tick handling                |
    |      .task_fork         // New task handling                  |
    |      .prio_changed      // Priority change handling           |
    |      ...                                                      |
    |  };                                                           |
    |                                                               |
    |  EVERY ASPECT of scheduling is in this table                  |
    |  Core scheduler just orchestrates timing                      |
    +---------------------------------------------------------------+

    TEMPLATE METHOD OPS: Hook collection
    +---------------------------------------------------------------+
    |  struct file_operations {                                     |
    |      /* Collection of independent hooks */                    |
    |      .open             // Called by vfs_open()                |
    |      .read             // Called by vfs_read()                |
    |      .write            // Called by vfs_write()               |
    |      .llseek           // Called by vfs_llseek()              |
    |      .release          // Called by fput()                    |
    |      ...                                                      |
    |  };                                                           |
    |                                                               |
    |  Each is called by DIFFERENT framework functions              |
    |  Each is wrapped with framework pre/post logic                |
    +---------------------------------------------------------------+
```

**中文说明：**

信号1：一次逻辑操作调用多个ops。策略中，`schedule()`调用`put_prev_task`、`pick_next_task`、`set_curr_task`。模板方法中，`vfs_read()`只调用`f_op->read`。

信号2：ops表代表完整算法。`sched_class`包含调度的每个方面（入队、出队、让出、抢占检查、选择下一个等），核心调度器只协调时机。`file_operations`是钩子集合，每个由不同的框架函数调用，每个被框架前后逻辑包装。

---

## Typical Naming Patterns

### Strategy Pattern Names

| Pattern | Examples | Indicates |
|---------|----------|-----------|
| `*_class` | `sched_class`, `blk_mq_ops` | Complete algorithm class |
| `*_ops` (standalone) | `tcp_congestion_ops`, `elevator_ops` | Replaceable algorithm |
| `*_algo` | `crypto_alg` | Algorithm implementation |
| `*_policy` | `cpufreq_policy` | Policy selection |

### Template Method Names

| Pattern | Examples | Indicates |
|---------|----------|-----------|
| `*_operations` | `file_operations`, `inode_operations` | Hook collection |
| `*_ops` (with wrapper) | `net_device_ops`, `block_device_operations` | Wrapped hooks |

### Context Clues

```
    STRATEGY INDICATORS:

    /* Registration function that "registers" an algorithm */
    int tcp_register_congestion_control(struct tcp_congestion_ops *ca);
    int elv_register(struct elevator_type *e);

    /* Selection mechanism */
    setsockopt(fd, TCP_CONGESTION, "cubic", ...);
    echo deadline > /sys/block/sda/queue/scheduler

    /* Multiple implementations compiled in */
    CONFIG_TCP_CONG_CUBIC=y
    CONFIG_TCP_CONG_RENO=y
    CONFIG_IOSCHED_DEADLINE=y
    CONFIG_IOSCHED_CFQ=y


    TEMPLATE METHOD INDICATORS:

    /* Framework wrapper functions */
    ssize_t vfs_read(struct file *file, ...);
    int dev_queue_xmit(struct sk_buff *skb);
    int device_add(struct device *dev);

    /* Ops assigned to object, called through framework */
    file->f_op = &ext4_file_operations;
    dev->netdev_ops = &e1000_netdev_ops;
```

---

## The Five Identification Rules

### Rule 1: Count the Ops Called Per Core Function

```
    STRATEGY:
    +---------------------------------------------------+
    | Core function calls MULTIPLE ops from same table  |
    |                                                   |
    | schedule():                                       |
    |     class->put_prev_task()                        |
    |     class->pick_next_task()                       |
    |     class->set_curr_task()                        |
    |                                                   |
    | Multiple ops = ONE algorithm being executed       |
    +---------------------------------------------------+

    TEMPLATE METHOD:
    +---------------------------------------------------+
    | Each framework function calls ONE op              |
    |                                                   |
    | vfs_read():  f_op->read()                         |
    | vfs_write(): f_op->write()                        |
    | vfs_open():  f_op->open()                         |
    |                                                   |
    | One op = one variation point in framework flow    |
    +---------------------------------------------------+

    RULE: If a single logical operation calls 2+ ops from same table,
          likely Strategy. If each framework func calls exactly 1 op,
          likely Template Method.
```

**中文说明：**

规则1：计算每个核心函数调用多少个ops。策略中，核心函数调用同一表中的多个ops（如`schedule()`调用`put_prev_task`、`pick_next_task`等），多个ops=执行一个算法。模板方法中，每个框架函数调用一个op（如`vfs_read()`只调用`read`），一个op=框架流程中的一个变化点。

### Rule 2: Check for Framework Wrapping

```
    STRATEGY:
    +---------------------------------------------------+
    | NO mandatory pre/post steps around strategy call  |
    |                                                   |
    | pick_next_task() {                                |
    |     /* No framework locking here */               |
    |     /* No framework validation */                 |
    |     return class->pick_next_task(rq);             |
    |     /* No framework post-processing */            |
    | }                                                 |
    +---------------------------------------------------+

    TEMPLATE METHOD:
    +---------------------------------------------------+
    | Mandatory pre/post steps enforced by framework    |
    |                                                   |
    | vfs_read() {                                      |
    |     check_fmode();           /* PRE: validation */|
    |     security_check();        /* PRE: security */  |
    |     ret = f_op->read(...);   /* HOOK */           |
    |     fsnotify_access();       /* POST: notify */   |
    |     inc_syscr();             /* POST: stats */    |
    | }                                                 |
    +---------------------------------------------------+

    RULE: If implementation runs WITHOUT framework wrapping,
          likely Strategy. If framework enforces pre/post,
          likely Template Method.
```

**中文说明：**

规则2：检查是否有框架包装。策略中，策略调用周围没有强制的前后步骤，策略直接运行。模板方法中，框架强制执行前后步骤（验证、安全检查、通知、统计）。

### Rule 3: Examine State Ownership

```
    STRATEGY:
    +---------------------------------------------------+
    | Strategy owns its own data structures             |
    |                                                   |
    | CFS owns: rb-tree, vruntime, load weights         |
    | RT owns:  priority arrays, bitmaps                |
    | CUBIC owns: cwnd state, cubic parameters          |
    |                                                   |
    | Each strategy = independent state machine         |
    +---------------------------------------------------+

    TEMPLATE METHOD:
    +---------------------------------------------------+
    | Framework owns the data structures                |
    | Implementation just operates on framework data    |
    |                                                   |
    | VFS owns: inode, file, dentry                     |
    | Filesystem just fills in data                     |
    +---------------------------------------------------+

    RULE: If the ops table implementation maintains its own
          private state machine, likely Strategy. If it operates
          on framework-owned data, likely Template Method.
```

**中文说明：**

规则3：检查状态所有权。策略中，策略拥有自己的数据结构：CFS有红黑树、vruntime；RT有优先级数组；CUBIC有cwnd状态。每个策略是独立的状态机。模板方法中，框架拥有数据结构（VFS拥有inode、file、dentry），实现只是操作框架数据。

### Rule 4: Look for Selection Mechanism

```
    STRATEGY:
    +---------------------------------------------------+
    | Explicit mechanism to SELECT which strategy       |
    |                                                   |
    | /* Boot-time selection */                         |
    | elevator=deadline                                 |
    |                                                   |
    | /* Runtime selection */                           |
    | setsockopt(fd, TCP_CONGESTION, "cubic");          |
    |                                                   |
    | /* Sysfs/procfs selection */                      |
    | echo cfq > /sys/block/sda/queue/scheduler        |
    |                                                   |
    | /* Per-object assignment */                       |
    | task->sched_class = &fair_sched_class;            |
    +---------------------------------------------------+

    TEMPLATE METHOD:
    +---------------------------------------------------+
    | Ops assigned at object creation, rarely changed   |
    | No "selection" mechanism                          |
    |                                                   |
    | inode->i_fop = &ext4_file_operations;             |
    | /* This is object's ops, not selected policy */   |
    +---------------------------------------------------+

    RULE: If there's a mechanism to switch between implementations
          at runtime or boot time, likely Strategy. If ops are
          fixed per object type, likely Template Method.
```

**中文说明：**

规则4：寻找选择机制。策略有明确的机制来选择哪个策略：启动时选择（`elevator=deadline`）、运行时选择（`setsockopt`）、sysfs选择（`echo cfq > ...`）、每对象分配。模板方法中，ops在对象创建时分配，很少改变，没有"选择"机制。

### Rule 5: Check Algorithm Coherence

```
    STRATEGY:
    +---------------------------------------------------+
    | All ops in table MUST be from same implementation |
    | Mixing would break algorithm correctness          |
    |                                                   |
    | /* WRONG: */                                      |
    | ops->enqueue = cfs_enqueue;                       |
    | ops->pick_next = rt_pick_next;  /* Breaks! */     |
    |                                                   |
    | Algorithms are atomic units                       |
    +---------------------------------------------------+

    TEMPLATE METHOD:
    +---------------------------------------------------+
    | Ops can potentially be mixed (though unusual)     |
    | Each is independent hook                          |
    |                                                   |
    | /* Unusual but not architecturally broken: */     |
    | fops->read = generic_read;                        |
    | fops->write = custom_write;                       |
    +---------------------------------------------------+

    RULE: If mixing implementations would break correctness,
          it's Strategy. If each op is independent, likely
          Template Method.
```

**中文说明：**

规则5：检查算法一致性。策略中，表中所有ops必须来自同一实现，混合会破坏算法正确性（如CFS的enqueue和RT的pick_next混用会出错）。算法是原子单元。模板方法中，ops可以潜在地混合（虽然不常见），每个是独立钩子。

---

## Summary Checklist

```
+=============================================================================+
|                    STRATEGY IDENTIFICATION CHECKLIST                         |
+=============================================================================+

    When examining an ops table, ask:

    [ ] 1. MULTIPLE OPS CALLED TOGETHER
        Does a single core operation call multiple ops from this table?

    [ ] 2. NO FRAMEWORK WRAPPING
        Do ops run without mandatory pre/post framework steps?

    [ ] 3. STRATEGY OWNS STATE
        Does the implementation maintain its own state machine?

    [ ] 4. SELECTION MECHANISM EXISTS
        Can the user/system choose between implementations?

    [ ] 5. ALGORITHM COHERENCE REQUIRED
        Would mixing ops from different implementations break things?

    SCORING:
    5/5 = Definitely Strategy
    4/5 = Likely Strategy
    3/5 = Mixed pattern, investigate further
    2/5 = Probably Template Method
    1/5 = Definitely Template Method
```

**中文说明：**

策略识别清单：(1) 一次核心操作是否调用多个ops？(2) ops是否无框架包装地运行？(3) 实现是否维护自己的状态机？(4) 是否存在选择机制？(5) 是否要求算法一致性（混合会破坏）？5项全满足则必定是策略，4项可能是策略，3项是混合模式需进一步调查，2项以下可能是模板方法。

---

## Red Flags: NOT Strategy

```
    RED FLAG 1: Framework wraps with pre/post
    ==========================================
    vfs_read() {
        check();              <-- Framework pre
        f_op->read();         <-- Hook
        notify();             <-- Framework post
    }
    --> Template Method, not Strategy

    RED FLAG 2: Each framework func calls one op
    =============================================
    vfs_read()  -> f_op->read
    vfs_write() -> f_op->write
    vfs_open()  -> f_op->open
    --> Template Method (hook per function)

    RED FLAG 3: Framework owns data structures
    ==========================================
    struct inode *inode;      // VFS owns this
    inode->i_op->lookup();    // Filesystem operates on it
    --> Template Method (framework data)

    RED FLAG 4: No selection mechanism
    ==================================
    /* Ops fixed at object creation */
    file->f_op = &ext4_file_operations;
    /* No way to "switch" to nfs_file_operations */
    --> Template Method (type-bound ops)

    RED FLAG 5: Ops are independent
    ================================
    /* read() doesn't need write() */
    /* open() doesn't depend on release() */
    --> Template Method (independent hooks)
```

---

## Quick Reference: Common Strategies in Linux

| Subsystem | Strategy Structure | Selection Mechanism |
|-----------|-------------------|---------------------|
| Scheduler | `struct sched_class` | Per-task (`task->sched_class`) |
| TCP CC | `struct tcp_congestion_ops` | Per-socket, sysctl |
| I/O Sched | `struct elevator_ops` | Per-queue, boot param |
| Security | LSM hooks | Boot-time module |
| Crypto | `struct crypto_alg` | Algorithm name |

| NOT Strategy | Why | Pattern Instead |
|--------------|-----|-----------------|
| `file_operations` | Framework wraps each call | Template Method |
| `net_device_ops` | Single hook per entry point | Template Method |
| `inode_operations` | Framework controls lifecycle | Template Method |
