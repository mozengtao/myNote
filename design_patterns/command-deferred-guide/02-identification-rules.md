# Identification Rules: Command/Deferred Execution Pattern

Five concrete rules to identify deferred execution patterns in Linux kernel source code.

---

## Rule 1: Work Structure Definitions

```
    SIGNAL: Structures for encapsulating work

    /* Workqueue */
    struct work_struct {
        atomic_long_t data;
        struct list_head entry;
        work_func_t func;
    };

    /* Tasklet */
    struct tasklet_struct {
        struct tasklet_struct *next;
        unsigned long state;
        atomic_t count;
        void (*func)(unsigned long);
        unsigned long data;
    };

    NAMING PATTERNS:
    - *_work
    - *_tasklet
    - struct work_struct
    - struct tasklet_struct
```

**中文说明：**

规则1：工作结构定义。信号：封装工作的结构体，如work_struct、tasklet_struct。命名模式包括*_work、*_tasklet等。

---

## Rule 2: Scheduling Functions

```
    SIGNAL: Functions that queue work for later

    /* Workqueue scheduling */
    schedule_work(&work);           /* Default queue */
    queue_work(wq, &work);          /* Specific queue */
    schedule_delayed_work(&work, delay);

    /* Tasklet scheduling */
    tasklet_schedule(&tasklet);
    tasklet_hi_schedule(&tasklet);  /* High priority */

    /* Softirq raising */
    raise_softirq(SOFTIRQ_TYPE);

    NAMING PATTERNS:
    - schedule_*
    - queue_*
    - *_schedule
    - raise_softirq
```

---

## Rule 3: Work Functions

```
    SIGNAL: Callback functions executed later

    /* Work function signature */
    void my_work_fn(struct work_struct *work);

    /* Tasklet function signature */
    void my_tasklet_fn(unsigned long data);

    /* Softirq handler signature */
    void my_softirq_handler(struct softirq_action *);


    PATTERN:
    - Function pointer in work structure
    - Called by worker/softirq, not directly
    - Often accesses deferred data
```

**中文说明：**

规则3：工作函数。信号：稍后执行的回调函数。工作函数有特定的签名，由工作者/软中断调用而非直接调用，通常访问延迟的数据。

---

## Rule 4: Initialization Macros

```
    SIGNAL: Macros for setting up deferred work

    /* Static work initialization */
    DECLARE_WORK(name, func);
    DECLARE_DELAYED_WORK(name, func);

    /* Dynamic work initialization */
    INIT_WORK(&work, func);
    INIT_DELAYED_WORK(&work, func);

    /* Tasklet initialization */
    DECLARE_TASKLET(name, func, data);
    tasklet_init(&tasklet, func, data);

    /* Workqueue creation */
    create_workqueue("name");
    create_singlethread_workqueue("name");
```

---

## Rule 5: Context Markers

```
    SIGNAL: Comments or code indicating context requirements

    /* Comments indicating deferred execution */
    /* Called from interrupt context */
    /* Must not sleep */
    /* Schedule for process context */

    /* Code patterns */
    if (in_interrupt()) {
        schedule_work(&work);  /* Defer if in IRQ */
    } else {
        do_work_directly();    /* Safe to do now */
    }

    /* GFP flags indicating context */
    kmalloc(..., GFP_ATOMIC);  /* Can't sleep */
    kmalloc(..., GFP_KERNEL);  /* Can sleep */
```

**中文说明：**

规则5：上下文标记。信号：注释或代码指示上下文要求。常见注释包括"从中断上下文调用"、"不能睡眠"、"调度到进程上下文"。代码模式包括检查in_interrupt()来决定是否延迟。

---

## Summary: 5 Identification Rules

```
+=============================================================================+
|                    DEFERRED EXECUTION IDENTIFICATION                         |
+=============================================================================+

    RULE 1: WORK STRUCTURES
    -----------------------
    [ ] struct work_struct
    [ ] struct tasklet_struct
    
    RULE 2: SCHEDULING FUNCTIONS
    ----------------------------
    [ ] schedule_work(), queue_work()
    [ ] tasklet_schedule()
    [ ] raise_softirq()
    
    RULE 3: WORK FUNCTIONS
    ----------------------
    [ ] void func(struct work_struct *)
    [ ] void func(unsigned long)
    
    RULE 4: INITIALIZATION
    ----------------------
    [ ] INIT_WORK, DECLARE_WORK
    [ ] tasklet_init, DECLARE_TASKLET
    
    RULE 5: CONTEXT MARKERS
    -----------------------
    [ ] in_interrupt() checks
    [ ] GFP_ATOMIC usage
    [ ] "cannot sleep" comments

    IF 3+ MATCH: Deferred Execution Pattern
```

---

## Red Flags: NOT Deferred Execution

```
    THESE ARE NOT DEFERRED EXECUTION:
    =================================

    1. Direct function calls
       - No scheduling, immediate execution

    2. Kernel threads doing main work
       - Thread IS the work, not deferred

    3. Simple callbacks
       - Called immediately, not queued
```

---

## Version

Based on **Linux kernel v3.2** deferred execution patterns.
