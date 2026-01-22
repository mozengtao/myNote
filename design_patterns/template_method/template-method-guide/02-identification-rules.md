# Kernel-Level Identification Rules

## How to Recognize Template Method in Kernel Source

```
+=============================================================================+
|                    TEMPLATE METHOD ANATOMY                                   |
+=============================================================================+

    void core_subsystem_operation(struct context *ctx)
    {                                                      |
        /* FIXED: Pre-hook invariant setup */              |  TEMPLATE
        lock_acquire(&ctx->lock);                          |  (FIXED)
        if (!validate_state(ctx))                          |
            goto out_unlock;                               |
                                                           |
        /* FIXED: Security/permission check */             |
        if (security_check(ctx) < 0)                       |
            goto out_unlock;                               |
        ------------------------------------               |
        /* VARIATION: Implementation hook */               |  HOOK
        if (ctx->ops && ctx->ops->do_work)                 |  (VARIES)
            ret = ctx->ops->do_work(ctx);                  |
        ------------------------------------               |
        /* FIXED: Post-hook cleanup */                     |  TEMPLATE
        update_statistics(ctx);                            |  (FIXED)
        notify_completion(ctx);                            |
                                                           |
    out_unlock:                                            |
        lock_release(&ctx->lock);                          |
    }

    STRUCTURE:
    +------------------+
    |   PRE-HOOK       |  <-- Framework: locking, validation, security
    +------------------+
    |   HOOK CALL      |  <-- Implementation: actual work
    +------------------+
    |   POST-HOOK      |  <-- Framework: cleanup, notification, unlock
    +------------------+
```

**中文说明：**

上图展示了模板方法的典型结构。函数分为三个部分：(1) 钩子前处理——框架负责加锁、验证、安全检查；(2) 钩子调用——调用实现层的实际工作函数；(3) 钩子后处理——框架负责清理、通知、解锁。识别模板方法的关键是找到这种"固定-变化-固定"的结构。

---

## Typical Naming Patterns

### Core Function Prefixes

| Prefix | Subsystem | Example |
|--------|-----------|---------|
| `vfs_*` | Virtual File System | `vfs_read()`, `vfs_write()`, `vfs_open()` |
| `dev_*` | Device/Network | `dev_queue_xmit()`, `dev_open()` |
| `submit_*` | Block/IO | `submit_bio()`, `submit_bh()` |
| `blk_*` | Block layer | `blk_queue_bio()`, `blk_run_queue()` |
| `device_*` | Device model | `device_add()`, `device_del()` |
| `driver_*` | Driver core | `driver_register()`, `driver_probe_device()` |
| `bus_*` | Bus subsystem | `bus_add_device()`, `bus_probe_device()` |
| `class_*` | Device class | `class_device_add()` |
| `kobject_*` | Kobject core | `kobject_add()`, `kobject_del()` |
| `napi_*` | NAPI polling | `napi_poll()`, `napi_schedule()` |
| `sk_*` | Socket | `sk_alloc()`, `sk_free()` |

### Hook Table Structure Names

| Pattern | Subsystem | Example |
|---------|-----------|---------|
| `*_operations` | VFS | `file_operations`, `inode_operations` |
| `*_ops` | Various | `net_device_ops`, `block_device_operations` |
| `*_driver` | Drivers | `pci_driver`, `platform_driver` |
| `*_type` | Types with ops | `file_system_type`, `kobj_type` |

---

## The Five Identification Rules

### Rule 1: Look for Pre/Hook/Post Structure

```
    PATTERN TO FIND:

    core_function(ctx) {
        // PRE: Framework-owned setup
        lock(...)
        check(...)

        // HOOK: Single variation point
        ctx->ops->hook(ctx)

        // POST: Framework-owned cleanup
        cleanup(...)
        unlock(...)
    }

    RED FLAG IF MISSING:
    - No locking around hook     --> Probably not Template Method
    - No validation before hook  --> Might be raw callback
    - No cleanup after hook      --> Might be Strategy pattern
```

**中文说明：**

规则1：寻找"前置/钩子/后置"结构。模板方法必须在钩子调用前有框架级别的设置（加锁、检查），在钩子调用后有框架级别的清理（解锁、状态更新）。如果缺少这些，可能不是模板方法。

### Rule 2: Check Who Owns the Entry Point

```
    TEMPLATE METHOD:                       NOT TEMPLATE METHOD:

    /* Entry point in VFS core */          /* Entry point in driver */
    ssize_t vfs_read(file, buf, count)     ssize_t my_driver_read(...)
    {                                      {
        // Core controls entry                 // Driver controls entry
        ...                                    ...
        file->f_op->read(...)                  core_helper(...)
        ...                                    ...
    }                                      }

    User calls: vfs_read()                 User calls: my_driver_read()
    Framework owns entry: YES              Framework owns entry: NO

    RULE: If implementation owns entry point, it is NOT Template Method
```

**中文说明：**

规则2：检查谁拥有入口点。模板方法中，用户调用的是核心层函数（如`vfs_read()`），核心层再调用实现层的钩子。如果用户直接调用实现层函数，或实现层控制入口点，则不是模板方法。

### Rule 3: Verify Framework Enforces Invariants

```
    INVARIANT EXAMPLES:

    +------------------+------------------------------------------+
    | Subsystem        | Invariant Enforced by Framework          |
    +------------------+------------------------------------------+
    | VFS              | inode->i_mutex held during read/write    |
    | Network TX       | Queue lock held during xmit              |
    | Block            | Request queue state consistent           |
    | Device Model     | kobject reference valid during probe     |
    +------------------+------------------------------------------+

    HOW TO CHECK:

    1. Find the core function
    2. Look for lock_acquire() BEFORE hook
    3. Look for lock_release() AFTER hook
    4. If lock spans the hook call --> Template Method likely
```

**中文说明：**

规则3：验证框架是否强制执行不变量。查看核心函数是否在钩子调用前获取锁、在钩子调用后释放锁。如果锁跨越了钩子调用，很可能是模板方法。不变量包括VFS中的inode互斥锁、网络发送的队列锁等。

### Rule 4: Count the Ops Table Usage Points

```
    TEMPLATE METHOD:                       STRATEGY:

    struct file_operations fops;           struct sched_class sched;

    /* ops used at ONE specific point      /* ops used as complete
       within larger algorithm */             algorithm replacement */

    vfs_read() {                           schedule() {
        ...                                    ...
        f_op->read()    <-- ONE POINT          class->pick_next_task()
        ...                                    class->put_prev_task()
    }                                          class->enqueue_task()
                                               ...
    vfs_write() {                          }
        ...
        f_op->write()   <-- ONE POINT      /* Multiple ops called
        ...                                   as complete algorithm */
    }

    /* Each core function calls            /* One function uses
       ONE hook from table */                 MANY hooks from table */

    RULE: Template Method = ONE hook per core function
          Strategy = MANY hooks compose algorithm
```

**中文说明：**

规则4：计算ops表的使用点数量。模板方法中，每个核心函数通常只调用ops表中的一个钩子（如`vfs_read()`只调用`f_op->read()`）。策略模式中，一个函数可能调用ops表中的多个钩子来组成完整算法。

### Rule 5: Check If Implementation Can Control Flow

```
    TEMPLATE METHOD:                       NOT TEMPLATE METHOD:

    Implementation CANNOT:                 Implementation CAN:
    - Skip pre-checks                      - Decide when to run
    - Avoid post-cleanup                   - Skip steps
    - Change execution order               - Change order
    - Bypass security hooks                - Control flow

    TEST: Can the hook return special
          value to skip post-processing?

    vfs_read() {
        ret = f_op->read()
        // Post-processing ALWAYS runs
        // regardless of ret value
        fsnotify_access(file)              <-- Cannot be skipped
    }

    RULE: If implementation can skip/bypass
          framework code --> NOT Template Method
```

**中文说明：**

规则5：检查实现能否控制流程。模板方法中，实现层无法跳过前置检查、避免后置清理、改变执行顺序或绕过安全钩子。测试方法：钩子能否通过返回特殊值来跳过后处理？如果能，则不是模板方法。

---

## Summary Checklist

```
+=============================================================================+
|                    TEMPLATE METHOD IDENTIFICATION CHECKLIST                  |
+=============================================================================+

    When examining a function pointer table, ask:

    [ ] 1. PRE/HOOK/POST STRUCTURE
        Is there a core function with setup -> hook -> cleanup?

    [ ] 2. ENTRY POINT OWNERSHIP
        Does the framework own the entry point that users call?

    [ ] 3. INVARIANT ENFORCEMENT
        Does the framework acquire/release locks around the hook?

    [ ] 4. SINGLE HOOK PER CORE FUNCTION
        Does each core function call only ONE hook from the table?

    [ ] 5. IMPLEMENTATION CANNOT CONTROL FLOW
        Is the implementation unable to skip or bypass framework code?

    SCORING:
    5/5 = Definitely Template Method
    4/5 = Likely Template Method
    3/5 = Might be Template Method, investigate further
    2/5 = Probably not Template Method
    1/5 = Definitely not Template Method
```

**中文说明：**

模板方法识别清单：(1) 是否有前置/钩子/后置结构？(2) 框架是否拥有入口点？(3) 框架是否在钩子周围强制执行不变量（如锁）？(4) 每个核心函数是否只调用一个钩子？(5) 实现层是否无法控制流程？5项全满足则必定是模板方法，4项可能是，3项需进一步调查，2项以下则不是。

---

## Red Flags: NOT Template Method

```
    RED FLAG 1: Implementation owns entry
    =====================================
    /* Driver directly called */
    my_driver_ioctl(...)     <-- User calls driver directly

    RED FLAG 2: No framework wrapping
    =================================
    /* Ops called without wrapper */
    inode->i_op->create(...)  <-- Direct call, no vfs_create()

    RED FLAG 3: Multiple hooks compose algorithm
    ============================================
    /* Many hooks called together */
    policy->enqueue()
    policy->dequeue()
    policy->pick_next()      <-- Strategy, not Template Method

    RED FLAG 4: Hook can bypass post-processing
    ===========================================
    if (hook() == SKIP_REST)
        return;              <-- Implementation controls flow
    post_process();

    RED FLAG 5: No locking around hook
    ==================================
    /* Hook called without protection */
    ops->dangerous_hook()    <-- No invariants enforced
```

**中文说明：**

非模板方法的危险信号：(1) 实现层拥有入口点；(2) 没有框架包装函数；(3) 多个钩子组成算法（策略模式）；(4) 钩子可以跳过后处理；(5) 钩子调用周围没有锁保护。

---

## Quick Reference: Common Template Methods in Linux

| Core Function | Hook Called | Subsystem |
|---------------|-------------|-----------|
| `vfs_read()` | `f_op->read` | VFS |
| `vfs_write()` | `f_op->write` | VFS |
| `vfs_open()` | `f_op->open` | VFS |
| `dev_queue_xmit()` | `ops->ndo_start_xmit` | Network |
| `napi_poll()` | `napi->poll` | NAPI |
| `submit_bio()` | `q->make_request_fn` | Block |
| `device_add()` | `drv->probe` (indirectly) | Device Model |
| `driver_probe_device()` | `drv->probe` | Device Model |

| NOT Template Method | Why | Pattern Instead |
|---------------------|-----|-----------------|
| `schedule()` | Uses many scheduler ops | Strategy |
| `kfree()` | No hook at all | Direct function |
| Direct `i_op->*` calls | No wrapper | Raw dispatch |
