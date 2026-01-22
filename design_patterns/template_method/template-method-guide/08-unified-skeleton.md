# Minimal Unified Template Method Skeleton (Kernel Style)

## Generic Template Method Pattern in C

```
+=============================================================================+
|                    UNIFIED TEMPLATE METHOD SKELETON                          |
+=============================================================================+

                         +-------------------+
                         |   User/Caller     |
                         +-------------------+
                                  |
                                  | calls
                                  v
    +----------------------------------------------------------+
    |                  CORE LAYER (Framework)                   |
    |  +----------------------------------------------------+  |
    |  |           core_subsystem_operation()               |  |
    |  |            (TEMPLATE METHOD)                       |  |
    |  +----------------------------------------------------+  |
    |  |                                                    |  |
    |  |  1. PRE-HOOK PHASE (Fixed)                         |  |
    |  |     +----------------------------+                 |  |
    |  |     | - Acquire locks            |                 |  |
    |  |     | - Validate state           |                 |  |
    |  |     | - Check permissions        |                 |  |
    |  |     | - Setup context            |                 |  |
    |  |     +----------------------------+                 |  |
    |  |                    |                               |  |
    |  |                    v                               |  |
    |  |  2. HOOK PHASE (Variable)                          |  |
    |  |     +============================+                 |  |
    |  |     ||  ops->hook(ctx)          ||                 |  |
    |  |     ||  Implementation code     ||-----------------+--+---> DRIVER
    |  |     +============================+                 |  |
    |  |                    |                               |  |
    |  |                    v                               |  |
    |  |  3. POST-HOOK PHASE (Fixed)                        |  |
    |  |     +----------------------------+                 |  |
    |  |     | - Update statistics        |                 |  |
    |  |     | - Send notifications       |                 |  |
    |  |     | - Cleanup/release          |                 |  |
    |  |     | - Release locks            |                 |  |
    |  |     +----------------------------+                 |  |
    |  |                                                    |  |
    |  +----------------------------------------------------+  |
    +----------------------------------------------------------+
```

**中文说明：**

上图展示了统一的模板方法骨架。用户调用核心层的模板方法函数，该函数分为三个阶段：(1) 钩子前阶段（固定）——获取锁、验证状态、检查权限、设置上下文；(2) 钩子阶段（可变）——调用实现层的钩子函数；(3) 钩子后阶段（固定）——更新统计、发送通知、清理资源、释放锁。核心层控制整个流程，实现层只填充钩子。

---

## The Skeleton Code

```c
/*
 * UNIFIED TEMPLATE METHOD SKELETON FOR LINUX KERNEL
 * 
 * This skeleton captures the essence of Template Method as used
 * across VFS, networking, block layer, and device model.
 */

#include <stdio.h>
#include <stdlib.h>

/* ==========================================================
 * CONTEXT: Passed through the template
 * Contains state needed by both framework and implementation
 * ========================================================== */
struct subsystem_context {
    void *private_data;      /* Implementation-specific data */
    int state;               /* Current state */
    int lock_held;           /* Lock state (simulation) */
    
    /* Statistics */
    unsigned long operations;
    unsigned long errors;
};

/* ==========================================================
 * OPERATIONS TABLE: Variation points
 * Implementation fills in these function pointers
 * ========================================================== */
struct subsystem_operations {
    /*
     * Primary hook: implementation-specific work
     * Called between pre and post phases
     * Returns: 0 on success, negative on error
     */
    int (*do_work)(struct subsystem_context *ctx);
    
    /*
     * Optional hooks for specific extension points
     */
    void (*prepare)(struct subsystem_context *ctx);
    void (*complete)(struct subsystem_context *ctx);
};

/* ==========================================================
 * OBJECT: Contains context and operations
 * This is what the framework operates on
 * ========================================================== */
struct subsystem_object {
    const char *name;
    struct subsystem_context ctx;
    const struct subsystem_operations *ops;
};

/* ==========================================================
 * PRE-HOOK FIXED STEPS
 * These run BEFORE the implementation hook
 * ========================================================== */

static int acquire_lock(struct subsystem_context *ctx)
{
    if (ctx->lock_held) {
        printf("  [FRAMEWORK] ERROR: lock already held (deadlock)\n");
        return -1;
    }
    ctx->lock_held = 1;
    printf("  [FRAMEWORK] Lock acquired\n");
    return 0;
}

static int validate_state(struct subsystem_context *ctx)
{
    if (ctx->state < 0) {
        printf("  [FRAMEWORK] ERROR: invalid state\n");
        return -1;
    }
    printf("  [FRAMEWORK] State validated: %d\n", ctx->state);
    return 0;
}

static int check_permissions(struct subsystem_context *ctx)
{
    /* In real kernel: LSM hooks, capability checks, etc. */
    printf("  [FRAMEWORK] Permission check passed\n");
    return 0;
}

static void setup_context(struct subsystem_context *ctx)
{
    /* Prepare any framework-level context */
    printf("  [FRAMEWORK] Context setup complete\n");
}

/* ==========================================================
 * POST-HOOK FIXED STEPS
 * These run AFTER the implementation hook
 * ========================================================== */

static void update_statistics(struct subsystem_context *ctx, int result)
{
    ctx->operations++;
    if (result < 0)
        ctx->errors++;
    printf("  [FRAMEWORK] Statistics: ops=%lu, errors=%lu\n",
           ctx->operations, ctx->errors);
}

static void send_notifications(struct subsystem_context *ctx)
{
    /* In real kernel: fsnotify, netlink, uevent, etc. */
    printf("  [FRAMEWORK] Notifications sent\n");
}

static void cleanup_context(struct subsystem_context *ctx)
{
    printf("  [FRAMEWORK] Context cleanup complete\n");
}

static void release_lock(struct subsystem_context *ctx)
{
    ctx->lock_held = 0;
    printf("  [FRAMEWORK] Lock released\n");
}

/* ==========================================================
 * TEMPLATE METHOD: core_subsystem_operation()
 * 
 * This is the main entry point users call.
 * Framework controls entire execution; implementation
 * only provides specific behavior at the hook point.
 * ========================================================== */
int core_subsystem_operation(struct subsystem_object *obj)
{
    struct subsystem_context *ctx = &obj->ctx;
    const struct subsystem_operations *ops = obj->ops;
    int ret = 0;

    printf("[TEMPLATE METHOD] START: %s\n", obj->name);

    /* ========================================
     * PRE-HOOK PHASE: Framework establishes
     * invariants that implementation can rely on
     * ======================================== */
    
    printf("  [FRAMEWORK] === PRE-HOOK PHASE ===\n");

    /* Step 1: Acquire lock (serialization) */
    ret = acquire_lock(ctx);
    if (ret < 0)
        return ret;

    /* Step 2: Validate state (preconditions) */
    ret = validate_state(ctx);
    if (ret < 0)
        goto out_unlock;

    /* Step 3: Check permissions (security) */
    ret = check_permissions(ctx);
    if (ret < 0)
        goto out_unlock;

    /* Step 4: Setup context */
    setup_context(ctx);

    /* Optional: implementation prepare hook */
    if (ops && ops->prepare)
        ops->prepare(ctx);

    /* ========================================
     * HOOK PHASE: Implementation does actual work
     * Context is fully prepared, locks held
     * ======================================== */
    
    printf("  [FRAMEWORK] === HOOK PHASE ===\n");
    printf("  [FRAMEWORK] >>> Calling implementation hook\n");

    if (ops && ops->do_work) {
        ret = ops->do_work(ctx);
    } else {
        printf("  [FRAMEWORK] WARNING: no do_work hook\n");
        ret = 0;
    }

    printf("  [FRAMEWORK] <<< Implementation returned: %d\n", ret);

    /* ========================================
     * POST-HOOK PHASE: Framework cleans up
     * and maintains invariants
     * ======================================== */
    
    printf("  [FRAMEWORK] === POST-HOOK PHASE ===\n");

    /* Optional: implementation complete hook */
    if (ops && ops->complete)
        ops->complete(ctx);

    /* Step 1: Update statistics (always, even on error) */
    update_statistics(ctx, ret);

    /* Step 2: Send notifications (if successful) */
    if (ret >= 0)
        send_notifications(ctx);

    /* Step 3: Cleanup context */
    cleanup_context(ctx);

out_unlock:
    /* Step 4: Release lock (always) */
    release_lock(ctx);

    printf("[TEMPLATE METHOD] END: %s, result=%d\n\n", obj->name, ret);
    return ret;
}

/* ==========================================================
 * EXAMPLE IMPLEMENTATIONS
 * Different implementations provide different behavior
 * while framework structure remains constant
 * ========================================================== */

/* Implementation A: Simple success path */
static int impl_a_work(struct subsystem_context *ctx)
{
    printf("    [IMPL A] Doing implementation A work\n");
    printf("    [IMPL A] Processing data: %p\n", ctx->private_data);
    printf("    [IMPL A] Work completed successfully\n");
    return 0;
}

static const struct subsystem_operations impl_a_ops = {
    .do_work = impl_a_work,
    .prepare = NULL,
    .complete = NULL,
};

/* Implementation B: With prepare/complete hooks */
static void impl_b_prepare(struct subsystem_context *ctx)
{
    printf("    [IMPL B] Prepare: allocating resources\n");
}

static int impl_b_work(struct subsystem_context *ctx)
{
    printf("    [IMPL B] Doing implementation B work\n");
    printf("    [IMPL B] Complex processing...\n");
    return 0;
}

static void impl_b_complete(struct subsystem_context *ctx)
{
    printf("    [IMPL B] Complete: releasing resources\n");
}

static const struct subsystem_operations impl_b_ops = {
    .do_work = impl_b_work,
    .prepare = impl_b_prepare,
    .complete = impl_b_complete,
};

/* Implementation C: Error case */
static int impl_c_work(struct subsystem_context *ctx)
{
    printf("    [IMPL C] Attempting work...\n");
    printf("    [IMPL C] ERROR: operation failed!\n");
    return -1;  /* Failure */
}

static const struct subsystem_operations impl_c_ops = {
    .do_work = impl_c_work,
};

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("================================================\n");
    printf("UNIFIED TEMPLATE METHOD SKELETON DEMONSTRATION\n");
    printf("================================================\n\n");

    /* Object with implementation A */
    struct subsystem_object obj_a = {
        .name = "object_A",
        .ctx = { .state = 1, .lock_held = 0 },
        .ops = &impl_a_ops,
    };

    /* Object with implementation B */
    struct subsystem_object obj_b = {
        .name = "object_B",
        .ctx = { .state = 2, .lock_held = 0 },
        .ops = &impl_b_ops,
    };

    /* Object with failing implementation */
    struct subsystem_object obj_c = {
        .name = "object_C",
        .ctx = { .state = 3, .lock_held = 0 },
        .ops = &impl_c_ops,
    };

    /* Object with invalid state */
    struct subsystem_object obj_bad = {
        .name = "object_BAD",
        .ctx = { .state = -1, .lock_held = 0 },
        .ops = &impl_a_ops,
    };

    /* All use the SAME template method function */
    printf("=== Calling with implementation A ===\n");
    core_subsystem_operation(&obj_a);

    printf("=== Calling with implementation B ===\n");
    core_subsystem_operation(&obj_b);

    printf("=== Calling with implementation C (will fail) ===\n");
    core_subsystem_operation(&obj_c);

    printf("=== Calling with bad state (pre-hook fails) ===\n");
    core_subsystem_operation(&obj_bad);

    return 0;
}
```

---

## Mapping to Real Kernel Subsystems

```
+=============================================================================+
|                    SKELETON TO SUBSYSTEM MAPPING                             |
+=============================================================================+

    SKELETON COMPONENT         VFS              NETDEV           DEVICE MODEL
    ==================         ===              ======           ============

    core_subsystem_operation   vfs_read()       dev_queue_xmit   device_add()
                                                                 
    acquire_lock               mutex_lock       __netif_tx_lock  (implicit)
                               (i_mutex)        
                                                                 
    validate_state             FMODE check      netif_running    device valid
                               
    check_permissions          security_        (qdisc)          pm_runtime_
                               file_permission                   get_sync

    ops->do_work               f_op->read       ndo_start_xmit   drv->probe

    update_statistics          inc_syscr        tx_packets++     (driver_bound)

    send_notifications         fsnotify_        (qdisc notify)   kobject_uevent
                               access

    release_lock               mutex_unlock     __netif_tx_      (implicit)
                                                unlock

+=============================================================================+
|                    SKELETON TO SUBSYSTEM MAPPING (continued)                 |
+=============================================================================+

    SKELETON COMPONENT         BLOCK LAYER      NAPI
    ==================         ===========      ====

    core_subsystem_operation   submit_bio()     net_rx_action()

    acquire_lock               (queue lock)     local_irq_disable

    validate_state             bio valid        napi scheduled

    check_permissions          (implicit)       (budget check)

    ops->do_work               make_request_fn  napi->poll

    update_statistics          task_io_account  (budget tracking)

    send_notifications         trace_block_*    (completion)

    release_lock               (queue unlock)   local_irq_enable
```

**中文说明：**

上表展示了统一骨架如何映射到实际内核子系统。每个骨架组件在VFS、网络设备、设备模型、块层、NAPI中都有对应实现。例如，`acquire_lock`在VFS中是`mutex_lock(i_mutex)`，在网络中是`__netif_tx_lock`。`ops->do_work`在VFS中是`f_op->read`，在网络中是`ndo_start_xmit`，在设备模型中是`drv->probe`。这证明了模板方法模式在内核中的一致性。

---

## How This Pattern Enforces Architectural Boundaries

```
    BOUNDARY ENFORCEMENT:

    +-----------------------------------------------------------------+
    |                     CORE LAYER                                  |
    +-----------------------------------------------------------------+
    |                                                                 |
    |  OWNS:                            GUARANTEES:                   |
    |  - Entry point                    - Locks always acquired       |
    |  - Lock acquisition               - State always valid          |
    |  - State validation               - Permissions always checked  |
    |  - Permission checking            - Statistics always updated   |
    |  - Statistics                     - Notifications always sent   |
    |  - Notifications                  - Cleanup always happens      |
    |  - Cleanup                                                      |
    |                                                                 |
    +-----------------------------------------------------------------+
                                |
                                | boundary
                                v
    +-----------------------------------------------------------------+
    |                  IMPLEMENTATION LAYER                           |
    +-----------------------------------------------------------------+
    |                                                                 |
    |  CAN ONLY:                        CANNOT:                       |
    |  - Do specific work               - Skip pre-checks             |
    |  - Use provided context           - Avoid post-cleanup          |
    |  - Return success/failure         - Release locks early         |
    |  - Access own data                - Modify framework state      |
    |                                   - Control execution order     |
    |                                                                 |
    +-----------------------------------------------------------------+
```

**中文说明：**

模板方法通过明确的层次划分强制执行架构边界。核心层拥有入口点、锁、状态验证、权限检查、统计、通知和清理，保证这些操作总是执行。实现层只能做特定工作、使用提供的上下文、返回成功/失败、访问自己的数据；不能跳过检查、避免清理、提前释放锁、修改框架状态或控制执行顺序。

---

## Key Properties of This Skeleton

### 1. Framework Controls Entry

```c
/* Users call the framework function, never the implementation */
int core_subsystem_operation(obj);  /* <-- This is the API */

/* NOT: */
int impl_do_work(ctx);  /* <-- Implementation is internal */
```

### 2. Invariants Established Before Hook

```c
/* By the time ops->do_work() runs: */
/*   - Lock is held */
/*   - State is valid */
/*   - Permissions are checked */
/*   - Context is set up */
```

### 3. Cleanup Guaranteed After Hook

```c
/* After ops->do_work() returns: */
/*   - Statistics updated (even on error) */
/*   - Lock released (even on error) */
/*   - Cleanup runs (even on error) */
```

### 4. Implementation Cannot Skip Steps

```c
/* The only way to run do_work is through the template */
/* There is no direct path to the hook */
/* Framework wrapping is mandatory */
```

---

## Summary

This skeleton demonstrates the essential structure of Template Method in Linux kernel:

1. **Single entry point** in framework code
2. **Pre-hook phase** establishes invariants
3. **Hook call** delegates to implementation
4. **Post-hook phase** maintains invariants
5. **Error paths** also go through cleanup

Every kernel subsystem using Template Method follows this structure, with subsystem-specific details filled in.
