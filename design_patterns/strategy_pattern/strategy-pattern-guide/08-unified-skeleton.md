# Minimal Unified Strategy Skeleton (Kernel Style)

## Generic Strategy Pattern in C

```
+=============================================================================+
|                    UNIFIED STRATEGY PATTERN SKELETON                         |
+=============================================================================+

                         +-------------------+
                         |   User/Caller     |
                         +-------------------+
                                  |
                                  | calls
                                  v
    +----------------------------------------------------------+
    |                  CORE LAYER (Mechanism)                   |
    |  +----------------------------------------------------+  |
    |  |           core_operation()                         |  |
    |  |            (Uses Strategy)                         |  |
    |  +----------------------------------------------------+  |
    |  |                                                    |  |
    |  |  /* Core does mechanism work */                    |  |
    |  |  prepare_context();                                |  |
    |  |                                                    |  |
    |  |  /* Delegate ENTIRE DECISION to strategy */        |  |
    |  |  result = strategy->decide_a(ctx);                 |  |
    |  |  strategy->action_b(ctx, result);                  |  |
    |  |  strategy->finalize_c(ctx);                        |  |
    |  |                                                    |  |
    |  |  /* Core handles result */                         |  |
    |  |  apply_result(result);                             |  |
    |  |                                                    |  |
    |  +----------------------------------------------------+  |
    +----------------------------------------------------------+
                                  |
                    +-------------+-------------+
                    |             |             |
                    v             v             v
              +---------+   +---------+   +---------+
              |Strategy |   |Strategy |   |Strategy |
              |    A    |   |    B    |   |    C    |
              +---------+   +---------+   +---------+
              | decide_a|   | decide_a|   | decide_a|
              | action_b|   | action_b|   | action_b|
              | finalize|   | finalize|   | finalize|
              +---------+   +---------+   +---------+
              (CFS)         (RT)          (CUBIC)

    KEY DIFFERENCES FROM TEMPLATE METHOD:

    +----------------------------------------------------+
    | TEMPLATE METHOD          | STRATEGY                |
    |--------------------------+-------------------------|
    | pre_step()               | (no wrapping)           |
    | hook() <-- one function  | decide_a()              |
    | post_step()              | action_b()              |
    |                          | finalize_c()            |
    | Framework enforces order | Strategy owns algorithm |
    +----------------------------------------------------+
```

**中文说明：**

上图展示了统一的策略模式骨架。用户调用核心层的操作，核心层执行机制工作后将整个决策委托给策略。策略包含多个协同工作的函数（`decide_a`、`action_b`、`finalize_c`）形成完整算法。与模板方法的关键区别：模板方法有前后步骤包装一个钩子，框架强制顺序；策略没有包装，多个函数协同工作，策略拥有整个算法。

---

## The Skeleton Code

```c
/*
 * UNIFIED STRATEGY PATTERN SKELETON FOR LINUX KERNEL
 * 
 * This skeleton captures the essence of Strategy as used
 * in scheduler, TCP congestion control, I/O scheduler, and LSM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================
 * CONTEXT: State operated on by strategies
 * ========================================================== */
struct algorithm_context {
    int id;
    void *private_data;          /* Strategy-specific state */
    unsigned long metrics[4];    /* For tracking */
};

/* ==========================================================
 * STRATEGY OPS TABLE: The Replaceable Algorithm
 * All functions work together as ONE coherent algorithm
 * ========================================================== */
struct algorithm_ops {
    const char *name;
    
    /* Initialize strategy-specific state */
    void *(*init)(struct algorithm_context *ctx);
    
    /* Clean up strategy-specific state */
    void (*exit)(void *state);
    
    /* === THE ALGORITHM FUNCTIONS === */
    /* These work together to implement the policy */
    
    /* Make a decision */
    int (*decide)(void *state, struct algorithm_context *ctx);
    
    /* Take action based on decision */
    void (*act)(void *state, struct algorithm_context *ctx, int decision);
    
    /* Handle events */
    void (*on_event)(void *state, struct algorithm_context *ctx, int event);
    
    /* Get algorithm-specific metrics */
    void (*get_stats)(void *state, unsigned long *stats);
};

/* ==========================================================
 * SUBSYSTEM OBJECT: Contains context and current strategy
 * ========================================================== */
struct subsystem_object {
    const char *name;
    struct algorithm_context ctx;
    
    /* Current strategy */
    const struct algorithm_ops *ops;
    void *strategy_state;
};

/* ==========================================================
 * STRATEGY A: Aggressive Algorithm
 * ========================================================== */

struct aggressive_state {
    int aggression_level;
    unsigned long decisions_made;
};

static void *aggressive_init(struct algorithm_context *ctx)
{
    struct aggressive_state *state = calloc(1, sizeof(*state));
    state->aggression_level = 10;
    printf("  [AGGRESSIVE] Initialized with level %d\n", 
           state->aggression_level);
    return state;
}

static void aggressive_exit(void *data)
{
    struct aggressive_state *state = data;
    printf("  [AGGRESSIVE] Exit: made %lu decisions\n", state->decisions_made);
    free(state);
}

static int aggressive_decide(void *data, struct algorithm_context *ctx)
{
    struct aggressive_state *state = data;
    int decision = state->aggression_level * 2;  /* Aggressive! */
    state->decisions_made++;
    printf("  [AGGRESSIVE] Decide: %d (aggressive strategy)\n", decision);
    return decision;
}

static void aggressive_act(void *data, struct algorithm_context *ctx, 
                            int decision)
{
    printf("  [AGGRESSIVE] Act: applying decision %d aggressively\n", decision);
    ctx->metrics[0] += decision;
}

static void aggressive_on_event(void *data, struct algorithm_context *ctx,
                                 int event)
{
    struct aggressive_state *state = data;
    printf("  [AGGRESSIVE] Event %d: increasing aggression\n", event);
    state->aggression_level++;
}

static void aggressive_get_stats(void *data, unsigned long *stats)
{
    struct aggressive_state *state = data;
    stats[0] = state->decisions_made;
    stats[1] = state->aggression_level;
}

static const struct algorithm_ops aggressive_ops = {
    .name = "aggressive",
    .init = aggressive_init,
    .exit = aggressive_exit,
    .decide = aggressive_decide,
    .act = aggressive_act,
    .on_event = aggressive_on_event,
    .get_stats = aggressive_get_stats,
};

/* ==========================================================
 * STRATEGY B: Conservative Algorithm
 * ========================================================== */

struct conservative_state {
    int caution_factor;
    unsigned long decisions_made;
};

static void *conservative_init(struct algorithm_context *ctx)
{
    struct conservative_state *state = calloc(1, sizeof(*state));
    state->caution_factor = 2;
    printf("  [CONSERVATIVE] Initialized with caution factor %d\n",
           state->caution_factor);
    return state;
}

static void conservative_exit(void *data)
{
    struct conservative_state *state = data;
    printf("  [CONSERVATIVE] Exit: made %lu decisions\n", state->decisions_made);
    free(state);
}

static int conservative_decide(void *data, struct algorithm_context *ctx)
{
    struct conservative_state *state = data;
    int decision = 10 / state->caution_factor;  /* Conservative! */
    state->decisions_made++;
    printf("  [CONSERVATIVE] Decide: %d (conservative strategy)\n", decision);
    return decision;
}

static void conservative_act(void *data, struct algorithm_context *ctx,
                              int decision)
{
    printf("  [CONSERVATIVE] Act: applying decision %d carefully\n", decision);
    ctx->metrics[0] += decision;
}

static void conservative_on_event(void *data, struct algorithm_context *ctx,
                                   int event)
{
    struct conservative_state *state = data;
    printf("  [CONSERVATIVE] Event %d: increasing caution\n", event);
    state->caution_factor++;
}

static void conservative_get_stats(void *data, unsigned long *stats)
{
    struct conservative_state *state = data;
    stats[0] = state->decisions_made;
    stats[1] = state->caution_factor;
}

static const struct algorithm_ops conservative_ops = {
    .name = "conservative",
    .init = conservative_init,
    .exit = conservative_exit,
    .decide = conservative_decide,
    .act = conservative_act,
    .on_event = conservative_on_event,
    .get_stats = conservative_get_stats,
};

/* ==========================================================
 * STRATEGY C: Adaptive Algorithm
 * ========================================================== */

struct adaptive_state {
    int current_mode;  /* 0 = conservative, 1 = aggressive */
    unsigned long decisions_made;
    unsigned long mode_switches;
};

static void *adaptive_init(struct algorithm_context *ctx)
{
    struct adaptive_state *state = calloc(1, sizeof(*state));
    state->current_mode = 0;  /* Start conservative */
    printf("  [ADAPTIVE] Initialized in conservative mode\n");
    return state;
}

static void adaptive_exit(void *data)
{
    struct adaptive_state *state = data;
    printf("  [ADAPTIVE] Exit: %lu decisions, %lu mode switches\n",
           state->decisions_made, state->mode_switches);
    free(state);
}

static int adaptive_decide(void *data, struct algorithm_context *ctx)
{
    struct adaptive_state *state = data;
    int decision;
    
    if (state->current_mode == 0) {
        decision = 5;  /* Conservative */
    } else {
        decision = 15; /* Aggressive */
    }
    
    state->decisions_made++;
    printf("  [ADAPTIVE] Decide: %d (mode=%s)\n", 
           decision, state->current_mode ? "aggressive" : "conservative");
    return decision;
}

static void adaptive_act(void *data, struct algorithm_context *ctx,
                          int decision)
{
    printf("  [ADAPTIVE] Act: applying decision %d adaptively\n", decision);
    ctx->metrics[0] += decision;
}

static void adaptive_on_event(void *data, struct algorithm_context *ctx,
                               int event)
{
    struct adaptive_state *state = data;
    
    /* Switch mode based on events */
    if (event > 5 && state->current_mode == 0) {
        state->current_mode = 1;
        state->mode_switches++;
        printf("  [ADAPTIVE] Event %d: switching to AGGRESSIVE mode\n", event);
    } else if (event < 3 && state->current_mode == 1) {
        state->current_mode = 0;
        state->mode_switches++;
        printf("  [ADAPTIVE] Event %d: switching to CONSERVATIVE mode\n", event);
    } else {
        printf("  [ADAPTIVE] Event %d: staying in current mode\n", event);
    }
}

static void adaptive_get_stats(void *data, unsigned long *stats)
{
    struct adaptive_state *state = data;
    stats[0] = state->decisions_made;
    stats[1] = state->mode_switches;
    stats[2] = state->current_mode;
}

static const struct algorithm_ops adaptive_ops = {
    .name = "adaptive",
    .init = adaptive_init,
    .exit = adaptive_exit,
    .decide = adaptive_decide,
    .act = adaptive_act,
    .on_event = adaptive_on_event,
    .get_stats = adaptive_get_stats,
};

/* ==========================================================
 * CORE SUBSYSTEM (MECHANISM)
 * ========================================================== */

/* Registry of available strategies */
static const struct algorithm_ops *available_strategies[] = {
    &aggressive_ops,
    &conservative_ops,
    &adaptive_ops,
    NULL,
};

/* Find strategy by name */
static const struct algorithm_ops *find_strategy(const char *name)
{
    for (int i = 0; available_strategies[i]; i++) {
        if (strcmp(available_strategies[i]->name, name) == 0)
            return available_strategies[i];
    }
    return NULL;
}

/* Set strategy for object (runtime selection) */
static int set_strategy(struct subsystem_object *obj, const char *name)
{
    const struct algorithm_ops *ops = find_strategy(name);
    if (!ops) {
        printf("[CORE] Unknown strategy: %s\n", name);
        return -1;
    }
    
    /* Exit old strategy if any */
    if (obj->ops && obj->ops->exit && obj->strategy_state)
        obj->ops->exit(obj->strategy_state);
    
    obj->ops = ops;
    printf("[CORE] Set strategy to: %s\n", ops->name);
    
    /* Initialize new strategy */
    if (ops->init)
        obj->strategy_state = ops->init(&obj->ctx);
    
    return 0;
}

/* Core: Process operation using strategy */
static void core_process(struct subsystem_object *obj)
{
    int decision;
    
    printf("[CORE] core_process: object %s\n", obj->name);
    
    /* MECHANISM: Prepare context */
    printf("[CORE] Preparing context...\n");
    
    /* STRATEGY: Make decision (strategy decides HOW) */
    if (obj->ops && obj->ops->decide) {
        decision = obj->ops->decide(obj->strategy_state, &obj->ctx);
    } else {
        decision = 0;
    }
    
    /* STRATEGY: Act on decision (strategy decides HOW) */
    if (obj->ops && obj->ops->act) {
        obj->ops->act(obj->strategy_state, &obj->ctx, decision);
    }
    
    /* MECHANISM: Apply result */
    printf("[CORE] Applied result, metric now %lu\n", obj->ctx.metrics[0]);
}

/* Core: Handle event using strategy */
static void core_handle_event(struct subsystem_object *obj, int event)
{
    printf("[CORE] core_handle_event: event %d\n", event);
    
    /* STRATEGY: Let strategy handle the event */
    if (obj->ops && obj->ops->on_event) {
        obj->ops->on_event(obj->strategy_state, &obj->ctx, event);
    }
}

/* Core: Get statistics from strategy */
static void core_get_stats(struct subsystem_object *obj)
{
    unsigned long stats[4] = {0};
    
    if (obj->ops && obj->ops->get_stats) {
        obj->ops->get_stats(obj->strategy_state, stats);
    }
    
    printf("[CORE] Statistics for %s (strategy=%s):\n",
           obj->name, obj->ops ? obj->ops->name : "none");
    printf("[CORE]   stat[0]=%lu stat[1]=%lu stat[2]=%lu\n",
           stats[0], stats[1], stats[2]);
    printf("[CORE]   cumulative metric=%lu\n", obj->ctx.metrics[0]);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("================================================\n");
    printf("UNIFIED STRATEGY PATTERN SKELETON DEMONSTRATION\n");
    printf("================================================\n");

    /* Create subsystem objects */
    struct subsystem_object obj1 = { .name = "object_1" };
    struct subsystem_object obj2 = { .name = "object_2" };

    /* === Different strategies for different objects === */
    printf("\n=== CONFIGURING STRATEGIES ===\n\n");
    set_strategy(&obj1, "aggressive");
    set_strategy(&obj2, "conservative");

    /* === Process with different strategies === */
    printf("\n=== PROCESSING (Different Behaviors) ===\n");
    
    printf("\n--- Object 1 (aggressive) ---\n");
    core_process(&obj1);
    core_process(&obj1);
    
    printf("\n--- Object 2 (conservative) ---\n");
    core_process(&obj2);
    core_process(&obj2);

    /* === Handle events === */
    printf("\n=== HANDLING EVENTS ===\n");
    
    printf("\n--- Object 1 event ---\n");
    core_handle_event(&obj1, 5);
    core_process(&obj1);
    
    printf("\n--- Object 2 event ---\n");
    core_handle_event(&obj2, 5);
    core_process(&obj2);

    /* === Runtime strategy switch === */
    printf("\n=== RUNTIME STRATEGY SWITCH ===\n");
    
    printf("\n--- Switching object 1 to adaptive ---\n");
    set_strategy(&obj1, "adaptive");
    
    core_process(&obj1);
    core_handle_event(&obj1, 10);  /* Should trigger mode switch */
    core_process(&obj1);
    core_handle_event(&obj1, 1);   /* Should trigger mode switch back */
    core_process(&obj1);

    /* === Final statistics === */
    printf("\n=== FINAL STATISTICS ===\n\n");
    core_get_stats(&obj1);
    printf("\n");
    core_get_stats(&obj2);

    printf("\n================================================\n");
    printf("KEY OBSERVATIONS:\n");
    printf("1. Core doesn't know algorithm details\n");
    printf("2. Multiple ops work together as one algorithm\n");
    printf("3. Strategy maintains its own state\n");
    printf("4. Runtime switching is supported\n");
    printf("5. No framework wrapping - strategy owns algorithm\n");
    printf("================================================\n");

    return 0;
}
```

---

## Mapping to Real Kernel Subsystems

```
+=============================================================================+
|                    SKELETON TO SUBSYSTEM MAPPING                             |
+=============================================================================+

    SKELETON COMPONENT         SCHEDULER        TCP CC           I/O SCHED
    ==================         =========        ======           =========

    algorithm_ops              sched_class      tcp_congestion   elevator_ops
                                                _ops

    init()                     (implicit)       .init            .elevator_init

    exit()                     (implicit)       .release         .elevator_exit

    decide()                   .pick_next_task  .ssthresh        .elevator_
                                                .cong_avoid      dispatch_fn

    act()                      .put_prev_task   (cwnd update)    .elevator_
                               .set_curr_task                    add_req_fn

    on_event()                 .task_tick       .pkts_acked      .elevator_
                               .task_fork       .cwnd_event      completed_req

    get_stats()                (proc/sched)     (tcp_info)       (sysfs)

    set_strategy()             task->           setsockopt       sysfs write
                               sched_class      TCP_CONGESTION

+=============================================================================+

    SKELETON COMPONENT         MEMORY POLICY    LSM
    ==================         =============    ===

    algorithm_ops              mempolicy        security_
                               modes            operations

    init()                     (policy create)  (module init)

    decide()                   policy_zonelist  .inode_permission
                               (which node?)    .bprm_check

    act()                      (allocate from   (allow/deny)
                               chosen node)

    set_strategy()             set_mempolicy    (boot time
                               mbind            only)
```

**中文说明：**

骨架到子系统的映射：`algorithm_ops`对应调度器的`sched_class`、TCP的`tcp_congestion_ops`、I/O调度的`elevator_ops`、内存策略的mempolicy模式、LSM的`security_operations`。`init()/exit()`对应各子系统的初始化/清理。`decide()`对应调度器的`pick_next_task`、TCP的`ssthresh/cong_avoid`、I/O的`dispatch_fn`。`set_strategy()`对应任务的`sched_class`设置、TCP的`setsockopt`、I/O的sysfs写入、内存策略的`set_mempolicy`。

---

## Key Properties of This Skeleton

### 1. Strategy Owns the Algorithm

```c
/* Multiple ops work together - not isolated hooks */
decision = ops->decide(state, ctx);   /* First part of algorithm */
ops->act(state, ctx, decision);       /* Second part of algorithm */
ops->on_event(state, ctx, event);     /* Third part of algorithm */

/* These are NOT wrapped by framework pre/post steps */
```

### 2. Strategy Maintains Own State

```c
/* Each strategy has its own state structure */
struct aggressive_state {
    int aggression_level;      /* Strategy-specific! */
    unsigned long decisions_made;
};

struct conservative_state {
    int caution_factor;        /* Different state! */
    unsigned long decisions_made;
};

/* State is opaque to core */
void *strategy_state;
```

### 3. Runtime Selection Supported

```c
/* Strategy can be changed at runtime */
set_strategy(&obj, "aggressive");
/* ... later ... */
set_strategy(&obj, "conservative");  /* Switch! */
```

### 4. No Framework Wrapping

```c
/* Core calls strategy directly - no pre/post enforcement */
void core_process(obj) {
    /* NO: lock(); check(); */
    decision = ops->decide(...);
    ops->act(...);
    /* NO: notify(); unlock(); */
}
```

---

## Summary

This skeleton demonstrates the essential structure of Strategy in Linux kernel:

1. **Ops table represents complete algorithm**: All functions work together
2. **Strategy maintains own state**: Private data for each algorithm
3. **Core provides mechanism only**: Knows when, not how
4. **Runtime selection**: Strategy can be chosen/changed
5. **No framework wrapping**: Strategy owns its algorithm completely

The pattern is fundamentally about **separating policy from mechanism** — the core provides the infrastructure, the strategy provides the decision-making algorithm.
