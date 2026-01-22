# Common Kernel Anti-Patterns with Strategy

## Overview

```
+=============================================================================+
|                    STRATEGY ANTI-PATTERNS                                    |
+=============================================================================+

    CORRECT STRATEGY USE                   ANTI-PATTERNS
    ==================                     ==============

    Core: mechanism                        1. STRATEGY FOR LIFECYCLE
    Strategy: policy                          (Need Template Method instead)

    +-------------------+                  2. UNTRUSTED CODE CONTROL
    |     CORE          |                     (Letting strategy control
    +-------------------+                      execution order)
           |
           | delegates policy              3. FRAGMENTED ALGORITHM
           v                                  (Algorithm split across
    +-------------------+                      multiple strategies)
    |    STRATEGY       |
    | (complete algo)   |
    +-------------------+
```

**中文说明：**

上图对比了正确的策略使用与三种反模式。正确使用：核心负责机制，策略负责策略，核心将完整的策略委托给策略。反模式：(1) 对生命周期使用策略（应该用模板方法）；(2) 让不受信任的代码控制执行顺序；(3) 算法分散在多个策略中。

---

## Anti-Pattern 1: Using Strategy Where Lifecycle Control is Required

### The Problem

```
    WRONG: Strategy for Device Lifecycle
    =====================================

    +-------------------------------------------------------------------+
    |  /* BAD: Treating device lifecycle as a strategy */               |
    |                                                                   |
    |  struct device_strategy {                                         |
    |      int (*full_init)(struct device *dev);                        |
    |      void (*full_cleanup)(struct device *dev);                    |
    |  };                                                               |
    |                                                                   |
    |  void bad_device_add(struct device *dev) {                        |
    |      /* Just call the strategy - no control! */                   |
    |      dev->strategy->full_init(dev);                               |
    |  }                                                                |
    |                                                                   |
    |  /* Driver A's implementation */                                  |
    |  int driver_a_init(struct device *dev) {                          |
    |      send_uevent();        /* Before sysfs?! */                   |
    |      create_sysfs();       /* After uevent?! */                   |
    |      init_hardware();      /* When? */                            |
    |      /* Order is wrong, but strategy allows it! */                |
    |  }                                                                |
    +-------------------------------------------------------------------+

    PROBLEMS:
    - Device visible to userspace before initialized
    - Hotplug races
    - Sysfs hierarchy inconsistent
    - PM integration broken
```

**中文说明：**

对需要生命周期控制的场景使用策略会导致问题。如果设备生命周期被当作策略，驱动可以任意顺序执行uevent、sysfs创建、硬件初始化。结果：设备在初始化前对用户空间可见、热插拔竞态、sysfs层次结构不一致、电源管理集成破坏。

### Why This is Wrong

```
    DEVICE LIFECYCLE REQUIRES STRICT ORDER:

    CORRECT (Template Method):           WRONG (Strategy):
    =========================           ==================

    device_add() {                      device_add() {
        kobject_add();     // 1. sysfs      strategy->init();
        bus_add_device();  // 2. bus           // Any order?!
        pm_init();         // 3. PM            // PM when?!
        drv->probe();      // 4. driver        // Sysfs when?!
        uevent();          // 5. notify        // Order varies!
    }                                   }

    ORDER IS MANDATORY                  ORDER IS ARBITRARY
    Framework enforces                  Strategy controls

    RESULT: Consistent, safe            RESULT: Chaos, bugs
```

### The Correct Approach

```c
/*
 * CORRECT: Use Template Method for lifecycle
 */

/* Template method - framework controls order */
int device_add(struct device *dev)
{
    /* === FIXED STEP 1: Sysfs registration === */
    error = kobject_add(&dev->kobj, ...);
    
    /* === FIXED STEP 2: Bus registration === */
    error = bus_add_device(dev);
    
    /* === FIXED STEP 3: PM initialization === */
    pm_runtime_enable(dev);
    
    /* === HOOK: Driver-specific probe === */
    bus_probe_device(dev);  /* Calls drv->probe() */
    
    /* === FIXED STEP 4: Userspace notification === */
    kobject_uevent(&dev->kobj, KOBJ_ADD);
    
    return 0;
}

/* Driver ONLY implements probe() - one hook */
/* Cannot change lifecycle order */
```

---

## Anti-Pattern 2: Allowing Untrusted Code to Control Execution Order

### The Problem

```
    WRONG: Strategy Controls Core Execution
    =======================================

    +-------------------------------------------------------------------+
    |  /* BAD: Strategy can call back into core */                      |
    |                                                                   |
    |  struct dangerous_ops {                                           |
    |      void (*do_work)(struct context *ctx);                        |
    |  };                                                               |
    |                                                                   |
    |  /* Core function */                                              |
    |  void core_operation(struct context *ctx) {                       |
    |      lock(ctx);                                                   |
    |      ctx->ops->do_work(ctx);  /* Strategy runs */                 |
    |      unlock(ctx);                                                 |
    |  }                                                                |
    |                                                                   |
    |  /* Malicious/buggy strategy */                                   |
    |  void bad_strategy_work(struct context *ctx) {                    |
    |      core_operation(ctx);  /* CALLBACK INTO CORE - DEADLOCK! */   |
    |      unlock(ctx);          /* RELEASE LOCK EARLY - RACE! */       |
    |      do_bad_things();                                             |
    |      lock(ctx);            /* Try to restore - too late! */       |
    |  }                                                                |
    +-------------------------------------------------------------------+

    PROBLEMS:
    - Recursion/deadlock possible
    - Lock protocol violated
    - Core invariants broken
    - Unpredictable behavior
```

**中文说明：**

让策略控制核心执行顺序会导致问题。如果策略可以回调核心或操作核心的锁，可能导致：递归/死锁、锁协议被破坏、核心不变量被打破、行为不可预测。

### Why This is Wrong

```
    STRATEGY SHOULD BE A LEAF:

    CORRECT:                             WRONG:
    ========                             ======

    Core                                 Core
      |                                    |
      v                                    v
    Strategy                             Strategy
      |                                    |
      v                                    +---> Core (callback!)
    (Does work)                                   |
      |                                           v
      v                                         Strategy (again!)
    Returns                                       |
                                                  v
    ONE-WAY CALL                                RECURSION!
    Strategy doesn't call core           Strategy calls back into core
```

### The Correct Approach

```c
/*
 * CORRECT: Strategy is pure computation, no callbacks
 */

/* Strategy interface - pure functions */
struct safe_ops {
    /* Returns decision, doesn't call core */
    int (*decide)(struct context *ctx);
    
    /* Pure computation, no side effects on core */
    void (*compute)(struct context *ctx, int decision);
};

/* Core function */
void core_operation(struct context *ctx)
{
    int decision;
    
    lock(ctx);
    
    /* Strategy just computes - no callbacks */
    decision = ctx->ops->decide(ctx);
    ctx->ops->compute(ctx, decision);
    
    /* Core maintains invariants */
    unlock(ctx);
}

/*
 * Strategy implementation - cannot call back
 */
int safe_decide(struct context *ctx)
{
    /* Just compute and return - no core calls */
    return ctx->value > threshold ? ACTION_A : ACTION_B;
}
```

---

## Anti-Pattern 3: Fragmenting One Algorithm Across Multiple Strategies

### The Problem

```
    WRONG: Split Algorithm
    ======================

    +-------------------------------------------------------------------+
    |  /* BAD: One algorithm split into multiple strategy interfaces */ |
    |                                                                   |
    |  struct front_half_ops {                                          |
    |      void (*prepare)(struct context *ctx);                        |
    |      int (*decide)(struct context *ctx);                          |
    |  };                                                               |
    |                                                                   |
    |  struct back_half_ops {                                           |
    |      void (*execute)(struct context *ctx, int decision);          |
    |      void (*cleanup)(struct context *ctx);                        |
    |  };                                                               |
    |                                                                   |
    |  void bad_design(struct context *ctx) {                           |
    |      ctx->front_ops->prepare(ctx);                                |
    |      decision = ctx->front_ops->decide(ctx);                      |
    |                                                                   |
    |      /* Problem: front and back might be from different algos! */ |
    |      ctx->back_ops->execute(ctx, decision);                       |
    |      ctx->back_ops->cleanup(ctx);                                 |
    |  }                                                                |
    +-------------------------------------------------------------------+

    PROBLEMS:
    - Algorithm coherence broken
    - State mismatch between front/back
    - Can't reason about correctness
    - Impossible to test
```

**中文说明：**

将一个算法分散到多个策略接口会导致问题。如果算法被分成front_half和back_half两个独立的策略，它们可能来自不同的算法实现，导致：算法一致性破坏、前后半部分状态不匹配、无法推理正确性、无法测试。

### Why This is Wrong

```
    ALGORITHM MUST BE ATOMIC:

    CORRECT:                             WRONG:
    ========                             ======

    struct algorithm_ops {               struct front_ops { ... };
        .prepare()                       struct back_ops { ... };
        .decide()                        
        .execute()         <-- ALL       front = algo_A->front;
        .cleanup()             IN ONE    back = algo_B->back;  // MIX!
    };                                   
                                         BROKEN:
    All functions from same algo         - A's prepare with B's execute?
    Coherent state machine               - Incompatible state assumptions
    Can reason about behavior            - Undefined behavior
```

### The Correct Approach

```c
/*
 * CORRECT: Complete algorithm in one structure
 */

struct algorithm_ops {
    const char *name;
    
    /* ALL algorithm functions together */
    void (*init)(struct context *ctx);
    void (*prepare)(struct context *ctx);
    int (*decide)(struct context *ctx);
    void (*execute)(struct context *ctx, int decision);
    void (*cleanup)(struct context *ctx);
    void (*exit)(struct context *ctx);
    
    /* Cannot mix - whole structure assigned atomically */
};

/* Core ensures atomicity */
void set_algorithm(struct context *ctx, struct algorithm_ops *new_algo)
{
    /* Exit old algorithm completely */
    if (ctx->algo && ctx->algo->exit)
        ctx->algo->exit(ctx);
    
    /* Install new algorithm completely */
    ctx->algo = new_algo;
    
    if (new_algo && new_algo->init)
        new_algo->init(ctx);
}

/* Usage - all ops from same algorithm */
void operation(struct context *ctx)
{
    ctx->algo->prepare(ctx);
    decision = ctx->algo->decide(ctx);
    ctx->algo->execute(ctx, decision);
    ctx->algo->cleanup(ctx);
    /* All coherent - from same algorithm */
}
```

---

## Why These Anti-Patterns Are Actively Avoided

### Kernel Review Process Catches These

```
    REVIEW RED FLAGS:

    1. "Why isn't this using device model?"
       -> Strategy for lifecycle detected

    2. "This strategy can call back into the subsystem"
       -> Callback inversion detected

    3. "These ops should be in the same structure"
       -> Fragmented algorithm detected

    MAINTAINERS REJECT:
    - Patches that use wrong pattern
    - Code that violates subsystem architecture
    - Designs that break invariants
```

### Historical Examples

```
    REAL KERNEL DESIGN DECISIONS:

    1. VFS: Template Method
       - Early Linux had less structure
       - Bugs in inconsistent filesystem behavior
       - VFS was hardened with mandatory wrappers
       
    2. Scheduler: Strategy
       - Originally O(n), then O(1), then CFS
       - sched_class abstraction enabled evolution
       - Different policies can coexist
       
    3. Device Model: Template Method
       - Wild West of driver init in 2.4
       - 2.6 introduced strict lifecycle
       - Hotplug, PM, sysfs all depend on order
```

---

## Summary: Anti-Pattern Detection

```
+=============================================================================+
|                    ANTI-PATTERN DETECTION CHECKLIST                          |
+=============================================================================+

    [ ] STRATEGY FOR LIFECYCLE SIGNALS:
        - Different implementations have different ordering
        - No mandatory pre/post steps
        - Lifecycle-sensitive operations (sysfs, PM, notifications)
        --> Should be Template Method

    [ ] UNTRUSTED CONTROL SIGNALS:
        - Strategy can call back into core
        - Strategy can manipulate core locks
        - Strategy can change core state directly
        --> Strategy should be pure/leaf

    [ ] FRAGMENTED ALGORITHM SIGNALS:
        - Algorithm split across multiple ops tables
        - Some ops from one impl, some from another
        - State assumptions differ between parts
        --> Should be single atomic ops table

    IF ANY OF THESE ARE PRESENT:
    +----------------------------------------------------------+
    |  REFACTOR:                                               |
    |  1. Lifecycle -> Template Method with strict order       |
    |  2. Control -> Make strategy pure, no callbacks          |
    |  3. Fragment -> Combine into single atomic ops table     |
    +----------------------------------------------------------+
```

**中文说明：**

反模式检测清单：(1) 策略用于生命周期的信号——不同实现有不同顺序、没有强制前后步骤、涉及生命周期敏感操作，应该用模板方法；(2) 不受信任控制的信号——策略可以回调核心、操作核心锁、直接改变核心状态，策略应该是纯函数/叶子节点；(3) 分散算法的信号——算法分散在多个ops表、部分ops来自一个实现部分来自另一个、状态假设不同，应该是单一原子ops表。

---

## Prevention Guidelines

| Anti-Pattern | Prevention | Review Question |
|--------------|------------|-----------------|
| Strategy for Lifecycle | Use Template Method | "Does order matter?" |
| Untrusted Control | Make strategy pure | "Can this call back?" |
| Fragmented Algorithm | Single ops table | "Can these be mixed?" |

> **The Key Principle:**
>
> Strategy is for **policy decisions** where the entire algorithm varies.
>
> Strategy is NOT for **lifecycle control** where order must be enforced.
>
> Strategy implementations should be **pure leaves** that compute and return, not control points that call back into the core.
