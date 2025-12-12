# 嵌入式设计模式 - 并发和资源管理模式

本文档介绍嵌入式系统中用于并发和资源管理的设计模式，包括完整的 C 语言代码示例。

---

## 目录

1. [循环执行器模式 (Cyclic Executive Pattern)](#1-循环执行器模式)
2. [静态优先级模式 (Static Priority Pattern)](#2-静态优先级模式)
3. [临界区模式 (Critical Region Pattern)](#3-临界区模式)
4. [保护调用模式 (Guarded Call Pattern)](#4-保护调用模式)
5. [排队模式 (Queuing Pattern)](#5-排队模式)
6. [汇合模式 (Rendezvous Pattern)](#6-汇合模式)
7. [同时锁定模式 (Simultaneous Locking Pattern)](#7-同时锁定模式)
8. [有序锁定模式 (Ordered Locking Pattern)](#8-有序锁定模式)

---

## 1. 循环执行器模式

### 架构图

```
+------------------------------------------------------------------+
|                  CYCLIC EXECUTIVE PATTERN                         |
+------------------------------------------------------------------+

    Simple Infinite Loop:
    
    +------------------+
    |   while (1) {    |
    |  +------------+  |
    |  |   Task 1   |--+---> Run to completion
    |  +------------+  |
    |       |          |
    |       v          |
    |  +------------+  |
    |  |   Task 2   |--+---> Run to completion
    |  +------------+  |
    |       |          |
    |       v          |
    |  +------------+  |
    |  |   Task 3   |--+---> Run to completion
    |  +------------+  |
    |       |          |
    |       +----------+
    |   }              |
    +------------------+


    Time-Sliced Cyclic Executive:
    
    Frame Period = 10ms
    
    |<---- Frame ---->|<---- Frame ---->|<---- Frame ---->|
    +--+--+--+--------+--+--+--+--------+--+--+--+--------+
    |T1|T2|T3| Idle   |T1|T2|T3| Idle   |T1|T2|T3| Idle   |
    +--+--+--+--------+--+--+--+--------+--+--+--+--------+
    ^                  ^                  ^
    Timer Tick        Timer Tick        Timer Tick
```

**中文说明：**
- 循环执行器是最简单的调度方式
- 所有任务按固定顺序循环执行
- 每个任务必须运行到完成才会执行下一个
- 适用于资源受限的小型系统和需要高可预测性的安全系统

### 完整代码示例

```c
/*============================================================================
 * 循环执行器模式示例 - 简单嵌入式系统调度
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 任务定义
 *---------------------------------------------------------------------------*/
typedef void (*task_fn)(void);

typedef struct {
    const char *name;
    task_fn execute;
    uint32_t period;            /* 执行周期（帧数） */
    uint32_t last_frame;        /* 上次执行的帧 */
    uint32_t execution_count;
    uint32_t wcet_us;          /* 最坏执行时间（微秒） */
} cyclic_task_t;

#define MAX_TASKS 10
static cyclic_task_t g_tasks[MAX_TASKS];
static int g_task_count = 0;

/*---------------------------------------------------------------------------
 * 帧管理
 *---------------------------------------------------------------------------*/
#define FRAME_PERIOD_MS 10      /* 帧周期 10ms */

static volatile uint32_t g_frame_count = 0;
static volatile bool g_frame_ready = false;
static uint32_t g_tick_ms = 0;

/* 模拟定时器中断 */
void timer_isr(void) {
    g_frame_ready = true;
    g_frame_count++;
}

void simulate_time(uint32_t ms) {
    g_tick_ms += ms;
    if (g_tick_ms % FRAME_PERIOD_MS == 0) {
        timer_isr();
    }
}

/*---------------------------------------------------------------------------
 * 循环执行器实现
 *---------------------------------------------------------------------------*/
int cyclic_register_task(const char *name, task_fn fn, uint32_t period, uint32_t wcet) {
    if (g_task_count >= MAX_TASKS) {
        return -1;
    }
    
    cyclic_task_t *task = &g_tasks[g_task_count];
    task->name = name;
    task->execute = fn;
    task->period = period;
    task->last_frame = 0;
    task->execution_count = 0;
    task->wcet_us = wcet;
    
    printf("[Cyclic] Registered task '%s' (period=%u frames, wcet=%uus)\n",
           name, period, wcet);
    
    return g_task_count++;
}

/* 关键点：检查是否可调度（所有任务在帧内完成） */
bool cyclic_check_schedulability(void) {
    uint32_t total_wcet = 0;
    
    for (int i = 0; i < g_task_count; i++) {
        /* 计算每帧的最坏执行时间 */
        total_wcet += g_tasks[i].wcet_us;
    }
    
    uint32_t frame_time_us = FRAME_PERIOD_MS * 1000;
    
    printf("[Cyclic] Total WCET: %uus, Frame time: %uus\n", total_wcet, frame_time_us);
    
    if (total_wcet > frame_time_us) {
        printf("[Cyclic] WARNING: Tasks may not complete in frame!\n");
        return false;
    }
    
    return true;
}

/* 关键点：循环执行器主循环 */
void cyclic_executive_run(uint32_t max_frames) {
    printf("[Cyclic] Starting cyclic executive (max %u frames)\n", max_frames);
    
    cyclic_check_schedulability();
    
    while (g_frame_count < max_frames) {
        /* 关键点：等待帧开始 */
        while (!g_frame_ready) {
            /* 在实际系统中可以进入低功耗模式 */
            simulate_time(1);
        }
        g_frame_ready = false;
        
        printf("\n--- Frame %u ---\n", g_frame_count);
        
        /* 关键点：按顺序执行所有到期任务 */
        for (int i = 0; i < g_task_count; i++) {
            cyclic_task_t *task = &g_tasks[i];
            
            /* 检查任务是否应该在此帧执行 */
            if ((g_frame_count - task->last_frame) >= task->period) {
                printf("  [Task] Running '%s'\n", task->name);
                
                /* 关键点：任务运行到完成 */
                task->execute();
                
                task->last_frame = g_frame_count;
                task->execution_count++;
            }
        }
    }
    
    /* 打印统计 */
    printf("\n=== Execution Statistics ===\n");
    for (int i = 0; i < g_task_count; i++) {
        printf("  %s: %u executions\n", 
               g_tasks[i].name, g_tasks[i].execution_count);
    }
}

/*---------------------------------------------------------------------------
 * 示例任务
 *---------------------------------------------------------------------------*/
void task_read_sensors(void) {
    printf("    -> Reading sensors...\n");
    /* 模拟传感器读取 */
}

void task_process_data(void) {
    printf("    -> Processing data...\n");
    /* 模拟数据处理 */
}

void task_update_display(void) {
    printf("    -> Updating display...\n");
    /* 模拟显示更新 */
}

void task_check_buttons(void) {
    printf("    -> Checking buttons...\n");
    /* 模拟按键检测 */
}

void task_heartbeat(void) {
    printf("    -> Heartbeat LED toggle\n");
    /* 模拟心跳指示 */
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void cyclic_executive_example(void) {
    printf("=== Cyclic Executive Pattern Demo ===\n\n");
    
    /* 关键点：注册任务（周期以帧数为单位） */
    cyclic_register_task("Sensors", task_read_sensors, 1, 1000);     /* 每帧 */
    cyclic_register_task("Process", task_process_data, 2, 2000);     /* 每2帧 */
    cyclic_register_task("Display", task_update_display, 5, 3000);   /* 每5帧 */
    cyclic_register_task("Buttons", task_check_buttons, 1, 500);     /* 每帧 */
    cyclic_register_task("Heartbeat", task_heartbeat, 10, 100);      /* 每10帧 */
    
    printf("\n");
    
    /* 运行循环执行器 */
    cyclic_executive_run(15);
}
```

---

## 2. 静态优先级模式

### 架构图

```
+------------------------------------------------------------------+
|                  STATIC PRIORITY PATTERN                          |
+------------------------------------------------------------------+

    Priority Queue:
    
    Priority 1 (Highest)  +---------+
                          | Task A  |  --> Run first
                          +---------+
    
    Priority 2            +---------+
                          | Task B  |  --> Run if A blocked
                          +---------+
    
    Priority 3            +---------+
                          | Task C  |  --> Run if A,B blocked
                          +---------+
    
    Priority 4 (Lowest)   +---------+
                          | Idle    |  --> Run if all blocked
                          +---------+


    Preemption Example:
    
    Time -->
    
    Task C (Low)    [====]          [====]
                         |          ^
    Task B (Med)         [====]     |
                              |     |
    Task A (High)             [====]+
                              ^
                              |
                         A becomes ready
                         and preempts B
```

**中文说明：**
- 静态优先级调度基于固定的任务优先级
- 高优先级任务就绪时会抢占低优先级任务
- 优先级通常根据任务的紧急程度或截止时间分配
- 适用于实时系统，强调响应性而非公平性

### 完整代码示例

```c
/*============================================================================
 * 静态优先级模式示例 - 简单抢占式调度器
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 任务状态和优先级定义
 *---------------------------------------------------------------------------*/
typedef enum {
    TASK_STATE_SUSPENDED,       /* 挂起 */
    TASK_STATE_READY,          /* 就绪 */
    TASK_STATE_RUNNING,        /* 运行 */
    TASK_STATE_BLOCKED         /* 阻塞 */
} task_state_t;

typedef void (*task_entry_t)(void *arg);

typedef struct {
    const char *name;
    uint8_t priority;           /* 0 = 最高优先级 */
    task_state_t state;
    task_entry_t entry;
    void *arg;
    
    /* 简化的栈模拟 */
    uint32_t stack[128];
    uint32_t *sp;
    
    /* 统计 */
    uint32_t run_count;
    uint32_t preempt_count;
} task_tcb_t;

#define MAX_PRIORITY_TASKS 8
static task_tcb_t g_tasks[MAX_PRIORITY_TASKS];
static int g_task_count = 0;
static task_tcb_t *g_current_task = NULL;

/*---------------------------------------------------------------------------
 * 调度器实现
 *---------------------------------------------------------------------------*/
/* 关键点：找到最高优先级的就绪任务 */
static task_tcb_t *find_highest_priority_ready(void) {
    task_tcb_t *highest = NULL;
    
    for (int i = 0; i < g_task_count; i++) {
        task_tcb_t *task = &g_tasks[i];
        
        if (task->state == TASK_STATE_READY) {
            if (highest == NULL || task->priority < highest->priority) {
                highest = task;
            }
        }
    }
    
    return highest;
}

/* 关键点：调度函数 - 选择并切换到最高优先级任务 */
void scheduler_run(void) {
    task_tcb_t *next = find_highest_priority_ready();
    
    if (next == NULL) {
        printf("[Scheduler] No ready tasks\n");
        return;
    }
    
    if (g_current_task != NULL && g_current_task->state == TASK_STATE_RUNNING) {
        /* 关键点：检查是否需要抢占 */
        if (next->priority < g_current_task->priority) {
            printf("[Scheduler] Preempting '%s' (pri=%d) for '%s' (pri=%d)\n",
                   g_current_task->name, g_current_task->priority,
                   next->name, next->priority);
            
            g_current_task->state = TASK_STATE_READY;
            g_current_task->preempt_count++;
        } else if (next == g_current_task) {
            /* 继续运行当前任务 */
            return;
        }
    }
    
    /* 切换到新任务 */
    g_current_task = next;
    g_current_task->state = TASK_STATE_RUNNING;
    g_current_task->run_count++;
    
    printf("[Scheduler] Running '%s' (priority=%d)\n", 
           g_current_task->name, g_current_task->priority);
    
    /* 执行任务（简化：直接调用） */
    g_current_task->entry(g_current_task->arg);
}

/* 创建任务 */
int task_create(const char *name, uint8_t priority, task_entry_t entry, void *arg) {
    if (g_task_count >= MAX_PRIORITY_TASKS) {
        return -1;
    }
    
    task_tcb_t *task = &g_tasks[g_task_count];
    task->name = name;
    task->priority = priority;
    task->state = TASK_STATE_SUSPENDED;
    task->entry = entry;
    task->arg = arg;
    task->run_count = 0;
    task->preempt_count = 0;
    
    printf("[Task] Created '%s' with priority %d\n", name, priority);
    
    return g_task_count++;
}

/* 任务控制 */
void task_ready(int task_id) {
    if (task_id >= 0 && task_id < g_task_count) {
        g_tasks[task_id].state = TASK_STATE_READY;
        printf("[Task] '%s' is now READY\n", g_tasks[task_id].name);
    }
}

void task_block(void) {
    if (g_current_task) {
        g_current_task->state = TASK_STATE_BLOCKED;
        printf("[Task] '%s' is now BLOCKED\n", g_current_task->name);
    }
}

void task_complete(void) {
    if (g_current_task) {
        g_current_task->state = TASK_STATE_SUSPENDED;
        printf("[Task] '%s' completed\n", g_current_task->name);
    }
}

/*---------------------------------------------------------------------------
 * 示例任务
 *---------------------------------------------------------------------------*/
void high_priority_task(void *arg) {
    (void)arg;
    printf("  [HIGH] Handling urgent event!\n");
    task_complete();
}

void medium_priority_task(void *arg) {
    (void)arg;
    printf("  [MED] Processing data...\n");
    task_complete();
}

void low_priority_task(void *arg) {
    (void)arg;
    printf("  [LOW] Background work...\n");
    task_complete();
}

void idle_task(void *arg) {
    (void)arg;
    printf("  [IDLE] System idle\n");
    /* Idle 任务不完成，持续运行 */
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void static_priority_example(void) {
    printf("=== Static Priority Pattern Demo ===\n\n");
    
    /* 创建任务（优先级数字越小越高） */
    int task_high = task_create("HighPri", 1, high_priority_task, NULL);
    int task_med = task_create("MedPri", 5, medium_priority_task, NULL);
    int task_low = task_create("LowPri", 10, low_priority_task, NULL);
    int task_idle = task_create("Idle", 255, idle_task, NULL);
    
    printf("\n--- Scenario 1: Only low priority ready ---\n");
    task_ready(task_low);
    scheduler_run();
    
    printf("\n--- Scenario 2: Medium and Idle ready ---\n");
    task_ready(task_med);
    task_ready(task_idle);
    scheduler_run();
    
    printf("\n--- Scenario 3: High priority event! ---\n");
    task_ready(task_low);
    task_ready(task_med);
    task_ready(task_high);  /* 关键点：高优先级任务就绪 */
    scheduler_run();        /* 应该运行高优先级任务 */
    
    printf("\n--- Continue with remaining tasks ---\n");
    scheduler_run();  /* 运行下一个最高优先级 */
    scheduler_run();  /* 运行下一个 */
    
    /* 打印统计 */
    printf("\n=== Task Statistics ===\n");
    for (int i = 0; i < g_task_count; i++) {
        printf("  %s: runs=%u, preempts=%u\n",
               g_tasks[i].name, g_tasks[i].run_count, g_tasks[i].preempt_count);
    }
}
```

---

## 3. 临界区模式

### 架构图

```
+------------------------------------------------------------------+
|                   CRITICAL REGION PATTERN                         |
+------------------------------------------------------------------+

    Without Critical Region:
    
    Task A                     Task B
       |                          |
       v                          |
    Read X (=5)                   |
       |                          v
       |                       Read X (=5)
       v                          |
    X = X + 1                     |
       |                          v
       v                       X = X + 1
    Write X (=6)                  |
       |                          v
       |                       Write X (=6)  <-- ERROR! Should be 7!


    With Critical Region:
    
    Task A                     Task B
       |                          |
       v                          |
    ENTER_CRITICAL               |
       |                          |
    Read X (=5)                   |
    X = X + 1                     |
    Write X (=6)                  |
       |                          |
    EXIT_CRITICAL                 |
       |                          v
       |                       ENTER_CRITICAL (waits)
       |                          |
       v                       Read X (=6)
                               X = X + 1
                               Write X (=7)  <-- Correct!
                                  |
                               EXIT_CRITICAL
```

**中文说明：**
- 临界区保护共享资源的访问不被中断或抢占
- 通过禁用中断或任务切换来实现
- 临界区应尽量短，避免影响系统响应性
- 适用于简单的原子操作保护

### 完整代码示例

```c
/*============================================================================
 * 临界区模式示例 - 保护共享数据
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 临界区实现
 *---------------------------------------------------------------------------*/
/* 模拟中断状态 */
static volatile bool g_interrupts_enabled = true;
static volatile uint32_t g_critical_nesting = 0;

/* 关键点：保存中断状态并禁用中断 */
typedef uint32_t critical_state_t;

critical_state_t critical_enter(void) {
    critical_state_t state = g_interrupts_enabled;
    
    /* 关键点：禁用中断 */
    g_interrupts_enabled = false;
    g_critical_nesting++;
    
    printf("    [CRITICAL] Enter (nesting=%u)\n", g_critical_nesting);
    
    return state;
}

/* 关键点：恢复中断状态 */
void critical_exit(critical_state_t state) {
    g_critical_nesting--;
    
    printf("    [CRITICAL] Exit (nesting=%u)\n", g_critical_nesting);
    
    /* 只有嵌套为0且原来是使能的才恢复 */
    if (g_critical_nesting == 0 && state) {
        g_interrupts_enabled = true;
    }
}

/* 便捷宏 */
#define ENTER_CRITICAL()    critical_state_t __state = critical_enter()
#define EXIT_CRITICAL()     critical_exit(__state)

/*---------------------------------------------------------------------------
 * 共享数据结构（需要保护）
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t counter;
    uint32_t checksum;
    uint8_t buffer[16];
    uint32_t head;
    uint32_t tail;
} shared_data_t;

static shared_data_t g_shared = {0};

/*---------------------------------------------------------------------------
 * 不安全的访问（错误示例）
 *---------------------------------------------------------------------------*/
void unsafe_increment(void) {
    /* 关键点：非原子操作，可能被中断打断 */
    uint32_t temp = g_shared.counter;  /* 读 */
    /* --- 如果这里被中断... --- */
    temp = temp + 1;                    /* 改 */
    /* --- 数据可能不一致 --- */
    g_shared.counter = temp;            /* 写 */
}

/*---------------------------------------------------------------------------
 * 安全的临界区访问
 *---------------------------------------------------------------------------*/
/* 关键点：使用临界区保护的原子递增 */
void safe_increment(void) {
    ENTER_CRITICAL();
    
    /* 关键点：这段代码不会被中断 */
    g_shared.counter++;
    g_shared.checksum += 1;  /* 保持数据一致性 */
    
    EXIT_CRITICAL();
}

/* 安全的缓冲区操作 */
bool safe_buffer_put(uint8_t data) {
    bool success = false;
    
    ENTER_CRITICAL();
    
    uint32_t next_head = (g_shared.head + 1) % sizeof(g_shared.buffer);
    
    if (next_head != g_shared.tail) {
        g_shared.buffer[g_shared.head] = data;
        g_shared.head = next_head;
        success = true;
    }
    
    EXIT_CRITICAL();
    
    return success;
}

bool safe_buffer_get(uint8_t *data) {
    bool success = false;
    
    ENTER_CRITICAL();
    
    if (g_shared.head != g_shared.tail) {
        *data = g_shared.buffer[g_shared.tail];
        g_shared.tail = (g_shared.tail + 1) % sizeof(g_shared.buffer);
        success = true;
    }
    
    EXIT_CRITICAL();
    
    return success;
}

/*---------------------------------------------------------------------------
 * 嵌套临界区示例
 *---------------------------------------------------------------------------*/
void nested_critical_example(void) {
    printf("\n--- Nested Critical Region ---\n");
    
    ENTER_CRITICAL();
    printf("  Outer critical section\n");
    
    {
        ENTER_CRITICAL();  /* 关键点：嵌套临界区 */
        printf("    Inner critical section\n");
        g_shared.counter += 10;
        EXIT_CRITICAL();   /* 退出内层，中断仍禁用 */
    }
    
    printf("  Back to outer\n");
    g_shared.counter += 1;
    
    EXIT_CRITICAL();  /* 退出外层，中断恢复 */
    
    printf("  Counter = %u\n", g_shared.counter);
}

/*---------------------------------------------------------------------------
 * 模拟并发场景
 *---------------------------------------------------------------------------*/
void simulate_concurrent_access(void) {
    printf("\n--- Simulating Concurrent Access ---\n");
    
    g_shared.counter = 0;
    
    /* 模拟多次"并发"访问 */
    printf("\nUsing safe_increment:\n");
    for (int i = 0; i < 5; i++) {
        safe_increment();
        printf("  After increment %d: counter=%u\n", i+1, g_shared.counter);
    }
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void critical_region_example(void) {
    printf("=== Critical Region Pattern Demo ===\n");
    
    /* 基本临界区使用 */
    printf("\n--- Basic Critical Region ---\n");
    ENTER_CRITICAL();
    printf("  Inside critical region\n");
    g_shared.counter = 100;
    EXIT_CRITICAL();
    printf("  Outside critical region, counter=%u\n", g_shared.counter);
    
    /* 嵌套临界区 */
    nested_critical_example();
    
    /* 缓冲区操作 */
    printf("\n--- Buffer Operations ---\n");
    for (int i = 0; i < 5; i++) {
        safe_buffer_put('A' + i);
        printf("  Put '%c'\n", 'A' + i);
    }
    
    uint8_t data;
    while (safe_buffer_get(&data)) {
        printf("  Got '%c'\n", data);
    }
    
    /* 并发模拟 */
    simulate_concurrent_access();
}
```

---

## 4. 保护调用模式

### 架构图

```
+------------------------------------------------------------------+
|                    GUARDED CALL PATTERN                           |
+------------------------------------------------------------------+

    Mutex-Protected Resource Access:
    
    Task A                   Resource              Task B
       |                        |                     |
       v                        |                     |
    mutex_lock()               |                     |
       |                        |                     |
       +-------LOCKED---------->|                     |
       |                        |                     v
    access_resource()           |                mutex_lock()
       |                        |                     |
       |                        |                  BLOCKED
    mutex_unlock()             |                  (waiting)
       |                        |                     |
       +------UNLOCKED--------->|                     |
       |                        |                     |
       v                        |                     v
    (continue)                  |                  ACQUIRED
                                |                     |
                                |<-----access---------|
                                |                     |
                                |                mutex_unlock()


    Guarded Call Structure:
    
    +------------------+
    |  Guarded Object  |
    +------------------+
    | - mutex          |
    | - data           |
    +------------------+
    | + lock()         |
    | + unlock()       |
    | + access()       |<-- Calls lock/unlock internally
    +------------------+
```

**中文说明：**
- 保护调用模式使用互斥锁保护共享资源
- 调用者在访问资源前获取锁，完成后释放
- 如果锁被占用，调用者会被阻塞等待
- 确保同一时刻只有一个任务访问资源

### 完整代码示例

```c
/*============================================================================
 * 保护调用模式示例 - 互斥锁保护共享资源
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 简化的互斥锁实现
 *---------------------------------------------------------------------------*/
typedef struct {
    volatile bool locked;
    const char *owner;
    uint32_t lock_count;
} mutex_t;

#define MUTEX_INIT  {false, NULL, 0}

void mutex_init(mutex_t *mutex) {
    mutex->locked = false;
    mutex->owner = NULL;
    mutex->lock_count = 0;
}

/* 关键点：获取互斥锁 */
bool mutex_lock(mutex_t *mutex, const char *caller) {
    /* 在真实系统中，这里需要原子操作或禁用中断 */
    
    if (mutex->locked) {
        printf("    [Mutex] %s: BLOCKED (owned by %s)\n", caller, mutex->owner);
        /* 真实系统中会在这里阻塞等待 */
        return false;
    }
    
    mutex->locked = true;
    mutex->owner = caller;
    mutex->lock_count++;
    
    printf("    [Mutex] %s: Acquired lock (#%u)\n", caller, mutex->lock_count);
    return true;
}

/* 关键点：释放互斥锁 */
void mutex_unlock(mutex_t *mutex, const char *caller) {
    if (!mutex->locked || mutex->owner != caller) {
        printf("    [Mutex] %s: ERROR - invalid unlock!\n", caller);
        return;
    }
    
    printf("    [Mutex] %s: Released lock\n", caller);
    
    mutex->locked = false;
    mutex->owner = NULL;
    
    /* 真实系统中会在这里唤醒等待的任务 */
}

/*---------------------------------------------------------------------------
 * 受保护的共享资源
 *---------------------------------------------------------------------------*/
typedef struct {
    mutex_t mutex;              /* 关键点：每个资源有自己的互斥锁 */
    char data[64];
    int value;
    uint32_t access_count;
} guarded_resource_t;

void guarded_resource_init(guarded_resource_t *res) {
    mutex_init(&res->mutex);
    memset(res->data, 0, sizeof(res->data));
    res->value = 0;
    res->access_count = 0;
}

/* 关键点：受保护的写操作 */
bool guarded_write(guarded_resource_t *res, const char *caller, const char *data, int value) {
    /* 获取锁 */
    if (!mutex_lock(&res->mutex, caller)) {
        return false;
    }
    
    /* 关键点：临界区 - 安全访问共享数据 */
    strncpy(res->data, data, sizeof(res->data) - 1);
    res->value = value;
    res->access_count++;
    
    printf("    [Resource] %s wrote: data='%s', value=%d\n", caller, data, value);
    
    /* 释放锁 */
    mutex_unlock(&res->mutex, caller);
    return true;
}

/* 关键点：受保护的读操作 */
bool guarded_read(guarded_resource_t *res, const char *caller, char *data, int *value) {
    if (!mutex_lock(&res->mutex, caller)) {
        return false;
    }
    
    /* 读取数据 */
    if (data) {
        strcpy(data, res->data);
    }
    if (value) {
        *value = res->value;
    }
    res->access_count++;
    
    printf("    [Resource] %s read: data='%s', value=%d\n", caller, res->data, res->value);
    
    mutex_unlock(&res->mutex, caller);
    return true;
}

/*---------------------------------------------------------------------------
 * 高级：带超时的保护调用
 *---------------------------------------------------------------------------*/
typedef struct {
    mutex_t mutex;
    int balance;
} bank_account_t;

void account_init(bank_account_t *acc, int initial) {
    mutex_init(&acc->mutex);
    acc->balance = initial;
}

/* 关键点：转账操作需要锁定两个账户 */
bool account_transfer(bank_account_t *from, bank_account_t *to, 
                      int amount, const char *caller) {
    printf("\n  [Transfer] %s: $%d\n", caller, amount);
    
    /* 先锁定源账户 */
    if (!mutex_lock(&from->mutex, caller)) {
        return false;
    }
    
    /* 检查余额 */
    if (from->balance < amount) {
        printf("    [Transfer] Insufficient funds!\n");
        mutex_unlock(&from->mutex, caller);
        return false;
    }
    
    /* 锁定目标账户 */
    if (!mutex_lock(&to->mutex, caller)) {
        mutex_unlock(&from->mutex, caller);
        return false;
    }
    
    /* 关键点：两个账户都锁定后执行转账 */
    from->balance -= amount;
    to->balance += amount;
    
    printf("    [Transfer] Success! From balance: $%d, To balance: $%d\n",
           from->balance, to->balance);
    
    /* 按顺序解锁 */
    mutex_unlock(&to->mutex, caller);
    mutex_unlock(&from->mutex, caller);
    
    return true;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void guarded_call_example(void) {
    printf("=== Guarded Call Pattern Demo ===\n");
    
    /* 初始化资源 */
    guarded_resource_t shared_resource;
    guarded_resource_init(&shared_resource);
    
    /* 模拟多个任务访问 */
    printf("\n--- Multiple Tasks Accessing Resource ---\n");
    
    printf("\nTask A:\n");
    guarded_write(&shared_resource, "TaskA", "Hello from A", 100);
    
    printf("\nTask B:\n");
    guarded_write(&shared_resource, "TaskB", "Hello from B", 200);
    
    printf("\nTask C (read):\n");
    char data[64];
    int value;
    guarded_read(&shared_resource, "TaskC", data, &value);
    
    printf("\n  Access count: %u\n", shared_resource.access_count);
    
    /* 银行账户转账示例 */
    printf("\n--- Bank Transfer Example ---\n");
    
    bank_account_t account_a, account_b;
    account_init(&account_a, 1000);
    account_init(&account_b, 500);
    
    printf("\nInitial: A=$1000, B=$500\n");
    
    account_transfer(&account_a, &account_b, 300, "Transfer1");
    account_transfer(&account_b, &account_a, 100, "Transfer2");
    
    printf("\nFinal: A=$%d, B=$%d\n", account_a.balance, account_b.balance);
}
```

---

## 5. 排队模式

### 架构图

```
+------------------------------------------------------------------+
|                      QUEUING PATTERN                              |
+------------------------------------------------------------------+

    Message Queue Architecture:
    
    Producer(s)                Queue                Consumer(s)
    +--------+              +---------+             +--------+
    | Task A |--put()----->|  [msg1] |---get()--->| Task X |
    +--------+             |  [msg2] |             +--------+
    +--------+             |  [msg3] |             +--------+
    | Task B |--put()----->|  [msg4] |---get()--->| Task Y |
    +--------+             |  [ ... ]|             +--------+
                           +---------+
                           Protected by
                             Mutex


    Message Flow:
    
    +-------+     +---------------+     +----------+
    | Event |---->| Create Message|---->| Enqueue  |
    +-------+     | (copy data)   |     | (atomic) |
                  +---------------+     +----+-----+
                                             |
                  +---------------+          |
                  |   Process     |<---------+
                  | (in consumer) |    Dequeue
                  +---------------+    (atomic)
```

**中文说明：**
- 排队模式使用消息队列实现任务间的异步通信
- 消息通过值传递，避免共享数据的竞态条件
- 队列本身通过互斥锁保护，确保操作原子性
- 生产者和消费者解耦，支持不同速率处理

### 完整代码示例

```c
/*============================================================================
 * 排队模式示例 - 消息队列通信
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 消息定义
 *---------------------------------------------------------------------------*/
typedef enum {
    MSG_TYPE_SENSOR_DATA,
    MSG_TYPE_BUTTON_EVENT,
    MSG_TYPE_COMMAND,
    MSG_TYPE_ALARM
} message_type_t;

typedef struct {
    message_type_t type;
    uint32_t timestamp;
    union {
        struct {
            float temperature;
            float humidity;
        } sensor;
        struct {
            uint8_t button_id;
            bool pressed;
        } button;
        struct {
            uint8_t cmd_id;
            uint32_t param;
        } command;
        struct {
            uint8_t level;
            char description[32];
        } alarm;
    } data;
} message_t;

/*---------------------------------------------------------------------------
 * 消息队列实现
 *---------------------------------------------------------------------------*/
#define QUEUE_SIZE 16

typedef struct {
    message_t buffer[QUEUE_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
    
    /* 简化的互斥锁 */
    volatile bool locked;
    
    /* 统计 */
    uint32_t total_put;
    uint32_t total_get;
    uint32_t overflow_count;
} message_queue_t;

void queue_init(message_queue_t *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->locked = false;
    q->total_put = 0;
    q->total_get = 0;
    q->overflow_count = 0;
}

/* 关键点：原子入队操作 */
bool queue_put(message_queue_t *q, const message_t *msg) {
    /* 获取锁 */
    while (q->locked) { /* 自旋等待 */ }
    q->locked = true;
    
    bool success = false;
    
    if (q->count < QUEUE_SIZE) {
        /* 关键点：复制消息到队列（值传递） */
        memcpy(&q->buffer[q->head], msg, sizeof(message_t));
        q->head = (q->head + 1) % QUEUE_SIZE;
        q->count++;
        q->total_put++;
        success = true;
        
        printf("  [Queue] Put msg type=%d, count=%u\n", msg->type, q->count);
    } else {
        q->overflow_count++;
        printf("  [Queue] OVERFLOW! Message lost.\n");
    }
    
    /* 释放锁 */
    q->locked = false;
    
    return success;
}

/* 关键点：原子出队操作 */
bool queue_get(message_queue_t *q, message_t *msg) {
    /* 获取锁 */
    while (q->locked) { /* 自旋等待 */ }
    q->locked = true;
    
    bool success = false;
    
    if (q->count > 0) {
        /* 关键点：从队列复制消息（值传递） */
        memcpy(msg, &q->buffer[q->tail], sizeof(message_t));
        q->tail = (q->tail + 1) % QUEUE_SIZE;
        q->count--;
        q->total_get++;
        success = true;
        
        printf("  [Queue] Got msg type=%d, count=%u\n", msg->type, q->count);
    }
    
    /* 释放锁 */
    q->locked = false;
    
    return success;
}

bool queue_is_empty(message_queue_t *q) {
    return q->count == 0;
}

bool queue_is_full(message_queue_t *q) {
    return q->count >= QUEUE_SIZE;
}

/*---------------------------------------------------------------------------
 * 生产者任务
 *---------------------------------------------------------------------------*/
static message_queue_t g_main_queue;
static uint32_t g_timestamp = 0;

void producer_sensor(void) {
    message_t msg;
    msg.type = MSG_TYPE_SENSOR_DATA;
    msg.timestamp = g_timestamp++;
    msg.data.sensor.temperature = 25.5f + (g_timestamp % 10) * 0.1f;
    msg.data.sensor.humidity = 60.0f;
    
    printf("\n[Producer/Sensor] Sending sensor data\n");
    queue_put(&g_main_queue, &msg);
}

void producer_button(uint8_t button_id, bool pressed) {
    message_t msg;
    msg.type = MSG_TYPE_BUTTON_EVENT;
    msg.timestamp = g_timestamp++;
    msg.data.button.button_id = button_id;
    msg.data.button.pressed = pressed;
    
    printf("\n[Producer/Button] Sending button event (btn=%d, %s)\n",
           button_id, pressed ? "pressed" : "released");
    queue_put(&g_main_queue, &msg);
}

void producer_alarm(uint8_t level, const char *desc) {
    message_t msg;
    msg.type = MSG_TYPE_ALARM;
    msg.timestamp = g_timestamp++;
    msg.data.alarm.level = level;
    strncpy(msg.data.alarm.description, desc, sizeof(msg.data.alarm.description) - 1);
    
    printf("\n[Producer/Alarm] Sending alarm (level=%d)\n", level);
    queue_put(&g_main_queue, &msg);
}

/*---------------------------------------------------------------------------
 * 消费者任务
 *---------------------------------------------------------------------------*/
void consumer_process_message(const message_t *msg) {
    printf("  [Consumer] Processing message type=%d, ts=%u\n", 
           msg->type, msg->timestamp);
    
    switch (msg->type) {
        case MSG_TYPE_SENSOR_DATA:
            printf("    Sensor: temp=%.1f°C, humidity=%.1f%%\n",
                   msg->data.sensor.temperature,
                   msg->data.sensor.humidity);
            break;
            
        case MSG_TYPE_BUTTON_EVENT:
            printf("    Button %d %s\n",
                   msg->data.button.button_id,
                   msg->data.button.pressed ? "PRESSED" : "RELEASED");
            break;
            
        case MSG_TYPE_ALARM:
            printf("    ALARM [Level %d]: %s\n",
                   msg->data.alarm.level,
                   msg->data.alarm.description);
            break;
            
        default:
            printf("    Unknown message type!\n");
            break;
    }
}

void consumer_task(void) {
    printf("\n[Consumer] Checking queue...\n");
    
    message_t msg;
    
    /* 关键点：处理队列中的所有消息 */
    while (queue_get(&g_main_queue, &msg)) {
        consumer_process_message(&msg);
    }
    
    printf("[Consumer] Queue empty\n");
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void queuing_example(void) {
    printf("=== Queuing Pattern Demo ===\n");
    
    /* 初始化队列 */
    queue_init(&g_main_queue);
    
    /* 模拟生产者发送消息 */
    printf("\n--- Producers sending messages ---\n");
    producer_sensor();
    producer_button(1, true);
    producer_sensor();
    producer_alarm(2, "High temperature!");
    producer_button(1, false);
    
    /* 消费者处理消息 */
    printf("\n--- Consumer processing ---\n");
    consumer_task();
    
    /* 统计 */
    printf("\n=== Queue Statistics ===\n");
    printf("  Total put: %u\n", g_main_queue.total_put);
    printf("  Total get: %u\n", g_main_queue.total_get);
    printf("  Overflow:  %u\n", g_main_queue.overflow_count);
    printf("  Current count: %u\n", g_main_queue.count);
}
```

---

## 6. 汇合模式

### 架构图

```
+------------------------------------------------------------------+
|                     RENDEZVOUS PATTERN                            |
+------------------------------------------------------------------+

    Multiple Tasks Synchronizing:
    
    Task A    Task B    Task C    Rendezvous Point
       |         |         |            |
       v         |         |            |
    arrive()     |         |     +------+------+
       |         v         |     | Waiting: 1  |
       |      arrive()     |     | Expected: 3 |
       |         |         v     +------+------+
       |         |      arrive()        |
       |         |         |     +------+------+
       |         |         |     | Waiting: 3  |
    (blocked) (blocked) (blocked)| All arrived!|
       |         |         |     +------+------+
       |         |         |            |
       v         v         v     Release all!
    continue   continue  continue


    Barrier Synchronization:
    
    Phase 1          Barrier          Phase 2
    
    Task A: [====]      |
    Task B: [==]        |      All wait here
    Task C: [======]    |      until everyone
                       \|/     arrives
                        v
                     [=====]
                     [=====]
                     [=====]
```

**中文说明：**
- 汇合模式用于同步多个任务，使它们在某一点会合
- 所有任务到达汇合点后才能继续执行
- 常用于并行计算中的阶段同步（屏障）
- 可以处理任意复杂的同步条件

### 完整代码示例

```c
/*============================================================================
 * 汇合模式示例 - 多任务同步屏障
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 汇合点（Rendezvous）结构
 *---------------------------------------------------------------------------*/
#define MAX_PARTICIPANTS 8

typedef struct {
    const char *name;
    uint8_t expected_count;             /* 期望的参与者数量 */
    volatile uint8_t arrived_count;     /* 已到达的数量 */
    volatile bool participants[MAX_PARTICIPANTS];  /* 参与者状态 */
    volatile bool released;             /* 是否已释放 */
    
    /* 回调函数 */
    void (*on_all_arrived)(void);
} rendezvous_t;

void rendezvous_init(rendezvous_t *rv, const char *name, uint8_t count) {
    rv->name = name;
    rv->expected_count = count;
    rv->arrived_count = 0;
    rv->released = false;
    rv->on_all_arrived = NULL;
    
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        rv->participants[i] = false;
    }
    
    printf("[Rendezvous] '%s' created, expecting %d participants\n", 
           name, count);
}

void rendezvous_set_callback(rendezvous_t *rv, void (*callback)(void)) {
    rv->on_all_arrived = callback;
}

/* 关键点：任务到达汇合点 */
bool rendezvous_arrive(rendezvous_t *rv, uint8_t participant_id, const char *task_name) {
    if (participant_id >= MAX_PARTICIPANTS) {
        return false;
    }
    
    /* 标记到达 */
    if (!rv->participants[participant_id]) {
        rv->participants[participant_id] = true;
        rv->arrived_count++;
        
        printf("[Rendezvous] '%s': %s arrived (%d/%d)\n",
               rv->name, task_name, rv->arrived_count, rv->expected_count);
    }
    
    /* 关键点：检查是否所有参与者都已到达 */
    if (rv->arrived_count >= rv->expected_count && !rv->released) {
        rv->released = true;
        
        printf("[Rendezvous] '%s': All participants arrived! Releasing...\n",
               rv->name);
        
        if (rv->on_all_arrived) {
            rv->on_all_arrived();
        }
    }
    
    return rv->released;
}

/* 关键点：等待所有参与者（阻塞） */
void rendezvous_wait(rendezvous_t *rv, uint8_t participant_id, const char *task_name) {
    /* 先到达 */
    rendezvous_arrive(rv, participant_id, task_name);
    
    /* 关键点：如果还没释放，等待 */
    if (!rv->released) {
        printf("[Rendezvous] '%s': %s waiting...\n", rv->name, task_name);
        
        /* 在真实系统中会阻塞任务 */
        while (!rv->released) {
            /* 自旋等待或让出 CPU */
        }
    }
    
    printf("[Rendezvous] '%s': %s proceeding!\n", rv->name, task_name);
}

/* 重置汇合点 */
void rendezvous_reset(rendezvous_t *rv) {
    rv->arrived_count = 0;
    rv->released = false;
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        rv->participants[i] = false;
    }
    printf("[Rendezvous] '%s' reset\n", rv->name);
}

/*---------------------------------------------------------------------------
 * 高级：条件汇合
 *---------------------------------------------------------------------------*/
typedef struct {
    rendezvous_t base;
    bool (*condition)(void);    /* 额外的释放条件 */
} conditional_rendezvous_t;

void conditional_rv_init(conditional_rendezvous_t *crv, const char *name,
                         uint8_t count, bool (*condition)(void)) {
    rendezvous_init(&crv->base, name, count);
    crv->condition = condition;
}

bool conditional_rv_arrive(conditional_rendezvous_t *crv, 
                           uint8_t id, const char *name) {
    rendezvous_arrive(&crv->base, id, name);
    
    /* 关键点：需要额外条件也满足 */
    if (crv->base.arrived_count >= crv->base.expected_count) {
        if (crv->condition && !crv->condition()) {
            printf("[CondRV] '%s': All arrived but condition not met!\n",
                   crv->base.name);
            return false;
        }
    }
    
    return crv->base.released;
}

/*---------------------------------------------------------------------------
 * 模拟任务
 *---------------------------------------------------------------------------*/
static rendezvous_t g_barrier;

void phase_complete_callback(void) {
    printf("\n>>> All tasks completed phase! Starting next phase... <<<\n\n");
}

void task_worker(uint8_t id, const char *name, uint32_t work_units) {
    printf("[%s] Starting work (%u units)\n", name, work_units);
    
    /* 模拟工作 */
    for (uint32_t i = 0; i < work_units; i++) {
        printf("[%s] Working... %u/%u\n", name, i + 1, work_units);
    }
    
    printf("[%s] Work complete, arriving at barrier\n", name);
    
    /* 关键点：到达屏障等待其他任务 */
    rendezvous_wait(&g_barrier, id, name);
    
    printf("[%s] Passed barrier, continuing...\n", name);
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void rendezvous_example(void) {
    printf("=== Rendezvous Pattern Demo ===\n\n");
    
    /* 创建屏障，期望3个参与者 */
    rendezvous_init(&g_barrier, "PhaseBarrier", 3);
    rendezvous_set_callback(&g_barrier, phase_complete_callback);
    
    /* 模拟三个任务，工作量不同 */
    printf("--- Phase 1: All tasks working ---\n\n");
    
    /* 任务按完成顺序到达 */
    task_worker(0, "TaskA", 2);  /* 最先完成 */
    task_worker(1, "TaskB", 3);  /* 第二完成 */
    task_worker(2, "TaskC", 1);  /* 最后完成但工作量少 */
    
    /* 注意：上面的顺序模拟是顺序的，真实系统中是并发的 */
    
    /* 重置并演示第二阶段 */
    printf("\n--- Phase 2: Reset and repeat ---\n\n");
    rendezvous_reset(&g_barrier);
    
    task_worker(2, "TaskC", 1);
    task_worker(0, "TaskA", 1);
    task_worker(1, "TaskB", 1);
    
    printf("\n=== Demo Complete ===\n");
}

/*---------------------------------------------------------------------------
 * 实际应用示例：多传感器数据融合
 *---------------------------------------------------------------------------*/
typedef struct {
    float temperature;
    float pressure;
    float humidity;
    uint32_t timestamp;
} sensor_fusion_data_t;

static sensor_fusion_data_t g_fusion_data;
static rendezvous_t g_sensor_sync;

void sensor_temp_task(void) {
    printf("[TempSensor] Reading...\n");
    g_fusion_data.temperature = 25.5f;
    rendezvous_arrive(&g_sensor_sync, 0, "TempSensor");
}

void sensor_pressure_task(void) {
    printf("[PressureSensor] Reading...\n");
    g_fusion_data.pressure = 101.3f;
    rendezvous_arrive(&g_sensor_sync, 1, "PressureSensor");
}

void sensor_humidity_task(void) {
    printf("[HumiditySensor] Reading...\n");
    g_fusion_data.humidity = 65.0f;
    rendezvous_arrive(&g_sensor_sync, 2, "HumiditySensor");
}

void fusion_callback(void) {
    printf("\n[Fusion] All sensor data ready!\n");
    printf("[Fusion] Temp=%.1f, Pressure=%.1f, Humidity=%.1f\n",
           g_fusion_data.temperature,
           g_fusion_data.pressure,
           g_fusion_data.humidity);
}

void sensor_fusion_example(void) {
    printf("\n=== Sensor Fusion Example ===\n\n");
    
    rendezvous_init(&g_sensor_sync, "SensorSync", 3);
    rendezvous_set_callback(&g_sensor_sync, fusion_callback);
    
    /* 传感器以不同速度完成 */
    sensor_humidity_task();
    sensor_temp_task();
    sensor_pressure_task();
}
```

---

## 7. 同时锁定模式

### 架构图

```
+------------------------------------------------------------------+
|                 SIMULTANEOUS LOCKING PATTERN                      |
+------------------------------------------------------------------+

    Deadlock Scenario (Problem):
    
    Task A              Task B
       |                   |
       v                   v
    lock(R1)           lock(R2)
       |                   |
       v                   v
    lock(R2) --+      +-- lock(R1)
       |       |      |       |
    BLOCKED <--+      +---> BLOCKED
       |                   |
       v                   v
    DEADLOCK!          DEADLOCK!


    Simultaneous Locking (Solution):
    
    Task A                    Task B
       |                         |
       v                         v
    try_lock_all(R1, R2)     try_lock_all(R1, R2)
       |                         |
    SUCCESS                   FAILED (all or nothing)
       |                         |
       v                         v
    [use R1, R2]              [wait/retry]
       |                         |
       v                         |
    unlock_all(R1, R2)           |
       |                         v
       v                      try_lock_all(R1, R2)
    (continue)                   |
                              SUCCESS
```

**中文说明：**
- 同时锁定模式采用"全有或全无"策略获取多个锁
- 要么一次性获取所有需要的锁，要么一个也不获取
- 避免"持有部分资源并请求其他资源"导致的死锁
- 适用于需要同时访问多个资源的场景

### 完整代码示例

```c
/*============================================================================
 * 同时锁定模式示例 - 避免死锁
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 资源和锁定义
 *---------------------------------------------------------------------------*/
typedef struct {
    const char *name;
    volatile bool locked;
    const char *owner;
} resource_lock_t;

#define MAX_LOCKS 8

void lock_init(resource_lock_t *lock, const char *name) {
    lock->name = name;
    lock->locked = false;
    lock->owner = NULL;
}

/* 尝试获取单个锁（非阻塞） */
bool try_lock_single(resource_lock_t *lock, const char *owner) {
    /* 在真实系统中需要原子操作 */
    if (lock->locked) {
        return false;
    }
    lock->locked = true;
    lock->owner = owner;
    return true;
}

void unlock_single(resource_lock_t *lock) {
    lock->locked = false;
    lock->owner = NULL;
}

/*---------------------------------------------------------------------------
 * 同时锁定实现
 *---------------------------------------------------------------------------*/
/* 关键点：一次性尝试获取所有锁 */
bool try_lock_all(resource_lock_t *locks[], int count, const char *owner) {
    printf("  [SimLock] %s trying to lock %d resources: ", owner, count);
    
    for (int i = 0; i < count; i++) {
        printf("%s ", locks[i]->name);
    }
    printf("\n");
    
    /* 关键点：按顺序尝试获取所有锁 */
    int acquired = 0;
    
    for (int i = 0; i < count; i++) {
        if (try_lock_single(locks[i], owner)) {
            acquired++;
        } else {
            /* 关键点：获取失败，释放已获取的所有锁 */
            printf("  [SimLock] Failed to lock '%s', rolling back...\n",
                   locks[i]->name);
            
            for (int j = 0; j < acquired; j++) {
                unlock_single(locks[j]);
            }
            
            printf("  [SimLock] %s: All locks released (all-or-nothing)\n", owner);
            return false;
        }
    }
    
    printf("  [SimLock] %s: All %d locks acquired!\n", owner, count);
    return true;
}

/* 关键点：释放所有锁 */
void unlock_all(resource_lock_t *locks[], int count, const char *owner) {
    printf("  [SimLock] %s releasing %d locks\n", owner, count);
    
    for (int i = 0; i < count; i++) {
        if (locks[i]->owner == owner) {
            unlock_single(locks[i]);
        }
    }
}

/*---------------------------------------------------------------------------
 * 带重试的同时锁定
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t max_retries;
    uint32_t retry_delay_ms;
    uint32_t total_attempts;
    uint32_t successful_locks;
    uint32_t failed_locks;
} sim_lock_stats_t;

static sim_lock_stats_t g_stats = {0};

bool try_lock_all_with_retry(resource_lock_t *locks[], int count, 
                              const char *owner, uint32_t max_retries) {
    uint32_t attempts = 0;
    
    while (attempts < max_retries) {
        g_stats.total_attempts++;
        
        if (try_lock_all(locks, count, owner)) {
            g_stats.successful_locks++;
            return true;
        }
        
        attempts++;
        printf("  [SimLock] %s: Retry %u/%u\n", owner, attempts, max_retries);
        
        /* 实际系统中这里会有延迟 */
        /* delay_ms(g_stats.retry_delay_ms); */
    }
    
    g_stats.failed_locks++;
    printf("  [SimLock] %s: Max retries exceeded!\n", owner);
    return false;
}

/*---------------------------------------------------------------------------
 * 实际应用：银行转账（需要锁定两个账户）
 *---------------------------------------------------------------------------*/
typedef struct {
    const char *name;
    int balance;
    resource_lock_t lock;
} bank_account_t;

void account_init(bank_account_t *acc, const char *name, int balance) {
    acc->name = name;
    acc->balance = balance;
    lock_init(&acc->lock, name);
}

bool transfer_money(bank_account_t *from, bank_account_t *to, 
                   int amount, const char *transaction_id) {
    printf("\n[Transfer] %s: $%d from %s to %s\n",
           transaction_id, amount, from->name, to->name);
    
    /* 关键点：同时锁定两个账户 */
    resource_lock_t *locks[2] = {&from->lock, &to->lock};
    
    if (!try_lock_all_with_retry(locks, 2, transaction_id, 3)) {
        printf("[Transfer] %s: FAILED - could not acquire locks\n",
               transaction_id);
        return false;
    }
    
    /* 检查余额 */
    if (from->balance < amount) {
        printf("[Transfer] %s: FAILED - insufficient funds\n", transaction_id);
        unlock_all(locks, 2, transaction_id);
        return false;
    }
    
    /* 执行转账 */
    from->balance -= amount;
    to->balance += amount;
    
    printf("[Transfer] %s: SUCCESS - %s=$%d, %s=$%d\n",
           transaction_id, from->name, from->balance, to->name, to->balance);
    
    /* 释放锁 */
    unlock_all(locks, 2, transaction_id);
    
    return true;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void simultaneous_locking_example(void) {
    printf("=== Simultaneous Locking Pattern Demo ===\n");
    
    /* 基本示例 */
    printf("\n--- Basic Example ---\n");
    
    resource_lock_t lock_a, lock_b, lock_c;
    lock_init(&lock_a, "ResourceA");
    lock_init(&lock_b, "ResourceB");
    lock_init(&lock_c, "ResourceC");
    
    /* 任务1尝试锁定 A 和 B */
    resource_lock_t *task1_locks[] = {&lock_a, &lock_b};
    bool task1_success = try_lock_all(task1_locks, 2, "Task1");
    
    if (task1_success) {
        /* 任务2尝试锁定 B 和 C（B 已被锁定） */
        resource_lock_t *task2_locks[] = {&lock_b, &lock_c};
        bool task2_success = try_lock_all(task2_locks, 2, "Task2");
        
        printf("  Task2 result: %s\n", task2_success ? "SUCCESS" : "FAILED");
        
        /* 任务1释放锁 */
        unlock_all(task1_locks, 2, "Task1");
        
        /* 任务2再次尝试 */
        task2_success = try_lock_all(task2_locks, 2, "Task2");
        printf("  Task2 retry result: %s\n", task2_success ? "SUCCESS" : "FAILED");
        
        if (task2_success) {
            unlock_all(task2_locks, 2, "Task2");
        }
    }
    
    /* 银行转账示例 */
    printf("\n--- Bank Transfer Example ---\n");
    
    bank_account_t alice, bob, charlie;
    account_init(&alice, "Alice", 1000);
    account_init(&bob, "Bob", 500);
    account_init(&charlie, "Charlie", 750);
    
    printf("\nInitial balances: Alice=$%d, Bob=$%d, Charlie=$%d\n",
           alice.balance, bob.balance, charlie.balance);
    
    transfer_money(&alice, &bob, 200, "T1");
    transfer_money(&bob, &charlie, 100, "T2");
    transfer_money(&charlie, &alice, 50, "T3");
    
    printf("\nFinal balances: Alice=$%d, Bob=$%d, Charlie=$%d\n",
           alice.balance, bob.balance, charlie.balance);
    
    /* 统计 */
    printf("\n=== Lock Statistics ===\n");
    printf("  Total attempts: %u\n", g_stats.total_attempts);
    printf("  Successful: %u\n", g_stats.successful_locks);
    printf("  Failed: %u\n", g_stats.failed_locks);
}
```

---

## 8. 有序锁定模式

### 架构图

```
+------------------------------------------------------------------+
|                   ORDERED LOCKING PATTERN                         |
+------------------------------------------------------------------+

    Circular Wait (Deadlock):
    
    Task A             Task B
       |                  |
    lock(R1)          lock(R2)
       |                  |
       v                  v
    wait(R2) <------- wait(R1)
       ^                  |
       |                  |
       +------------------+
          Circular Wait!


    Ordered Locking (No Deadlock):
    
    Resource Order: R1 < R2 < R3
    
    Task A                   Task B
       |                        |
       v                        v
    lock(R1) first          lock(R1) first  <-- Same order!
       |                        |
       v                     BLOCKED (R1 busy)
    lock(R2) second             |
       |                        |
    [use R1, R2]                |
       |                        |
    unlock(R2)                  |
    unlock(R1)                  |
       |                        v
       v                     lock(R1) acquired
    (continue)                  |
                                v
                             lock(R2)
                                |
                             [use R1, R2]
```

**中文说明：**
- 有序锁定模式对所有资源进行全局排序
- 所有任务必须按照相同的顺序获取锁
- 避免循环等待，从而防止死锁
- 即使只需要部分资源，也按顺序获取

### 完整代码示例

```c
/*============================================================================
 * 有序锁定模式示例 - 防止循环等待
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 有序资源定义
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t order;             /* 关键点：资源的全局顺序 */
    const char *name;
    volatile bool locked;
    const char *owner;
} ordered_resource_t;

void ordered_resource_init(ordered_resource_t *res, uint32_t order, const char *name) {
    res->order = order;
    res->name = name;
    res->locked = false;
    res->owner = NULL;
}

/* 获取锁 */
bool ordered_lock(ordered_resource_t *res, const char *owner) {
    if (res->locked) {
        printf("    [OrdLock] %s waiting for '%s' (order=%u)\n",
               owner, res->name, res->order);
        /* 在真实系统中会阻塞 */
        return false;
    }
    
    res->locked = true;
    res->owner = owner;
    printf("    [OrdLock] %s acquired '%s' (order=%u)\n",
           owner, res->name, res->order);
    return true;
}

void ordered_unlock(ordered_resource_t *res) {
    printf("    [OrdLock] %s released '%s'\n", res->owner, res->name);
    res->locked = false;
    res->owner = NULL;
}

/*---------------------------------------------------------------------------
 * 有序锁定管理器
 *---------------------------------------------------------------------------*/
#define MAX_ORDERED_RESOURCES 16

typedef struct {
    ordered_resource_t *resources[MAX_ORDERED_RESOURCES];
    int count;
    uint32_t last_locked_order;     /* 上次锁定的资源顺序 */
    bool order_violation;
} ordered_lock_context_t;

void ordered_ctx_init(ordered_lock_context_t *ctx) {
    ctx->count = 0;
    ctx->last_locked_order = 0;
    ctx->order_violation = false;
}

/* 关键点：检查并强制执行锁定顺序 */
bool ordered_ctx_lock(ordered_lock_context_t *ctx, 
                      ordered_resource_t *res, 
                      const char *owner) {
    /* 关键点：检查顺序是否正确 */
    if (res->order <= ctx->last_locked_order && ctx->count > 0) {
        printf("  [OrderViolation] %s: Trying to lock '%s' (order=%u) "
               "but last locked order was %u!\n",
               owner, res->name, res->order, ctx->last_locked_order);
        ctx->order_violation = true;
        return false;
    }
    
    if (ordered_lock(res, owner)) {
        ctx->resources[ctx->count++] = res;
        ctx->last_locked_order = res->order;
        return true;
    }
    
    return false;
}

/* 按逆序释放所有锁 */
void ordered_ctx_unlock_all(ordered_lock_context_t *ctx) {
    /* 关键点：按获取的逆序释放 */
    for (int i = ctx->count - 1; i >= 0; i--) {
        ordered_unlock(ctx->resources[i]);
    }
    ctx->count = 0;
    ctx->last_locked_order = 0;
}

/*---------------------------------------------------------------------------
 * 资源排序辅助
 *---------------------------------------------------------------------------*/
/* 对资源数组按顺序排序 */
void sort_resources(ordered_resource_t *resources[], int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (resources[j]->order > resources[j + 1]->order) {
                ordered_resource_t *temp = resources[j];
                resources[j] = resources[j + 1];
                resources[j + 1] = temp;
            }
        }
    }
}

/* 关键点：按顺序获取多个锁 */
bool lock_multiple_ordered(ordered_resource_t *resources[], int count, 
                           const char *owner) {
    printf("\n  [OrdLock] %s requesting %d resources\n", owner, count);
    
    /* 关键点：先排序，确保按顺序获取 */
    ordered_resource_t *sorted[MAX_ORDERED_RESOURCES];
    for (int i = 0; i < count; i++) {
        sorted[i] = resources[i];
    }
    sort_resources(sorted, count);
    
    printf("  [OrdLock] Lock order: ");
    for (int i = 0; i < count; i++) {
        printf("%s(%u) ", sorted[i]->name, sorted[i]->order);
    }
    printf("\n");
    
    /* 按顺序获取 */
    ordered_lock_context_t ctx;
    ordered_ctx_init(&ctx);
    
    for (int i = 0; i < count; i++) {
        if (!ordered_ctx_lock(&ctx, sorted[i], owner)) {
            /* 获取失败，释放已获取的锁 */
            ordered_ctx_unlock_all(&ctx);
            return false;
        }
    }
    
    return true;
}

/*---------------------------------------------------------------------------
 * 实际应用：数据库操作（需要锁定多个表）
 *---------------------------------------------------------------------------*/
typedef struct {
    ordered_resource_t lock;
    char data[64];
    int record_count;
} database_table_t;

/* 表的锁定顺序（全局定义） */
#define TABLE_ORDER_USERS       1
#define TABLE_ORDER_ORDERS      2
#define TABLE_ORDER_PRODUCTS    3
#define TABLE_ORDER_INVENTORY   4

void table_init(database_table_t *table, uint32_t order, const char *name) {
    ordered_resource_init(&table->lock, order, name);
    table->record_count = 0;
}

/* 模拟数据库事务 */
bool db_transaction(database_table_t *tables[], int table_count, 
                    const char *txn_name, void (*operation)(void)) {
    printf("\n=== Transaction: %s ===\n", txn_name);
    
    /* 关键点：获取需要的表锁（按顺序） */
    ordered_resource_t *locks[MAX_ORDERED_RESOURCES];
    for (int i = 0; i < table_count; i++) {
        locks[i] = &tables[i]->lock;
    }
    
    if (!lock_multiple_ordered(locks, table_count, txn_name)) {
        printf("[Transaction] %s: FAILED - could not acquire locks\n", txn_name);
        return false;
    }
    
    /* 执行操作 */
    printf("  [Transaction] %s: Executing...\n", txn_name);
    if (operation) {
        operation();
    }
    
    /* 释放锁（按逆序） */
    printf("  [Transaction] %s: Releasing locks (reverse order)\n", txn_name);
    for (int i = table_count - 1; i >= 0; i--) {
        ordered_unlock(&tables[i]->lock);
    }
    
    printf("[Transaction] %s: COMMITTED\n", txn_name);
    return true;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
static database_table_t g_users, g_orders, g_products, g_inventory;

void create_order_operation(void) {
    printf("    -> Checking user...\n");
    printf("    -> Adding order record...\n");
    printf("    -> Updating inventory...\n");
}

void update_product_operation(void) {
    printf("    -> Updating product info...\n");
    printf("    -> Adjusting inventory...\n");
}

void ordered_locking_example(void) {
    printf("=== Ordered Locking Pattern Demo ===\n");
    
    /* 初始化表（带顺序） */
    table_init(&g_users, TABLE_ORDER_USERS, "Users");
    table_init(&g_orders, TABLE_ORDER_ORDERS, "Orders");
    table_init(&g_products, TABLE_ORDER_PRODUCTS, "Products");
    table_init(&g_inventory, TABLE_ORDER_INVENTORY, "Inventory");
    
    printf("\nTable lock order:\n");
    printf("  Users(1) < Orders(2) < Products(3) < Inventory(4)\n");
    
    /* 事务1：创建订单（需要 Users, Orders, Inventory） */
    database_table_t *txn1_tables[] = {&g_users, &g_orders, &g_inventory};
    db_transaction(txn1_tables, 3, "CreateOrder", create_order_operation);
    
    /* 事务2：更新产品（需要 Products, Inventory） */
    database_table_t *txn2_tables[] = {&g_products, &g_inventory};
    db_transaction(txn2_tables, 2, "UpdateProduct", update_product_operation);
    
    /* 演示顺序违规（应该被检测到） */
    printf("\n--- Demonstrating Order Violation Detection ---\n");
    
    ordered_lock_context_t ctx;
    ordered_ctx_init(&ctx);
    
    printf("\nTrying to lock in wrong order:\n");
    ordered_ctx_lock(&ctx, &g_inventory.lock, "BadTask");  /* 先锁高顺序 */
    ordered_ctx_lock(&ctx, &g_users.lock, "BadTask");      /* 再锁低顺序 - 违规! */
    
    if (ctx.order_violation) {
        printf("\n[Result] Order violation detected - deadlock prevented!\n");
    }
    
    ordered_ctx_unlock_all(&ctx);
}
```

---

## 总结

| 模式 | 适用场景 | 核心机制 | 避免的问题 |
|------|----------|----------|------------|
| 循环执行器 | 小型系统/安全系统 | 简单循环调度 | 复杂性 |
| 静态优先级 | 实时系统 | 优先级抢占 | 延迟响应 |
| 临界区 | 简单原子操作 | 禁用中断 | 数据竞争 |
| 保护调用 | 共享资源访问 | 互斥锁 | 并发冲突 |
| 排队 | 任务间通信 | 消息队列 | 竞态条件 |
| 汇合 | 多任务同步 | 屏障同步 | 同步问题 |
| 同时锁定 | 多资源访问 | 全有或全无 | 死锁 |
| 有序锁定 | 多资源访问 | 顺序获取 | 循环等待 |

