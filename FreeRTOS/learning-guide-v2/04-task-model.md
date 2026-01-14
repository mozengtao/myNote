# Section 4: Task Model - The Core Abstraction

## 4.1 What a FreeRTOS Task Really Is

```
TASK = INDEPENDENT EXECUTION CONTEXT:
+==================================================================+
||                                                                ||
||  A task is an abstraction that gives you:                      ||
||                                                                ||
||  +------------------+    +----------------------------------+  ||
||  | Own stack        | -> | Local variables, function calls  |  ||
||  +------------------+    +----------------------------------+  ||
||                                                                ||
||  +------------------+    +----------------------------------+  ||
||  | Saved CPU state  | -> | Registers when not running       |  ||
||  +------------------+    +----------------------------------+  ||
||                                                                ||
||  +------------------+    +----------------------------------+  ||
||  | Priority         | -> | When it runs relative to others  |  ||
||  +------------------+    +----------------------------------+  ||
||                                                                ||
||  +------------------+    +----------------------------------+  ||
||  | State            | -> | Running, Ready, Blocked, etc.    |  ||
||  +------------------+    +----------------------------------+  ||
||                                                                ||
||  THE ILLUSION:                                                 ||
||  Each task believes it has the CPU to itself.                  ||
||  The scheduler creates this illusion by rapidly switching.     ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

任务=独立执行上下文。一个任务是给你的抽象：

- 自己的栈：局部变量、函数调用
- 保存的CPU状态：不运行时的寄存器
- 优先级：相对于其他任务何时运行
- 状态：运行中、就绪、阻塞等

这是一个幻象：每个任务认为CPU属于自己，调度器通过快速切换创造这个幻象。

### Task Function Signature

```c
/* Every task function has this signature */
void TaskFunction(void *pvParameters)
{
    /* Initialization (runs once) */
    int local_var = 0;
    
    /* Main loop (runs forever) */
    for(;;)
    {
        /* Do work */
        local_var++;
        
        /* Block or yield */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    /* NEVER REACHED - tasks don't return */
    /* If you remove the infinite loop, the system crashes */
}
```

```
WHY TASKS NEVER RETURN:
+------------------------------------------------------------------+
|                                                                  |
|  PROBLEM: Where would a task return TO?                          |
|                                                                  |
|  Normal function:                                                |
|  main() -> funcA() -> funcB() -> return -> return -> main()      |
|                                                                  |
|  Task function:                                                  |
|  scheduler -> TaskFunc() -> ??? (no caller to return to)         |
|                                                                  |
|  SOLUTIONS:                                                      |
|  1. Infinite loop (most common)                                  |
|  2. Call vTaskDelete(NULL) to self-delete                        |
|                                                                  |
|  In port.c, prvTaskExitError() catches accidental returns:       |
|  static void prvTaskExitError(void)                              |
|  {                                                               |
|      configASSERT(uxCriticalNesting == ~0UL);  /* Always fails */|
|      for(;;);                                                    |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么任务不能返回？

问题：任务返回到哪里？正常函数有调用者可返回，任务函数从调度器启动，没有调用者。

解决方案：1）无限循环（最常见），2）调用vTaskDelete(NULL)自删除。

在port.c中，prvTaskExitError()捕获意外返回——它总是失败并进入无限循环。

---

## 4.2 Comparison: FreeRTOS Tasks vs POSIX Threads vs Linux Processes

```
COMPARISON TABLE:
+------------------------------------------------------------------+
|                                                                  |
|  Feature        | FreeRTOS Task | POSIX Thread | Linux Process   |
|  ---------------+---------------+--------------+-----------------+
|  Address space  | Shared        | Shared       | Isolated        |
|  MMU required   | No            | Yes (usually)| Yes             |
|  Stack          | Own, fixed    | Own, fixed   | Own, growable   |
|  Heap           | Shared        | Shared       | Own (COW)       |
|  Creation cost  | ~100 cycles   | ~10K cycles  | ~100K cycles    |
|  Context switch | ~100 cycles   | ~1K cycles   | ~10K cycles     |
|  Isolation      | None          | Minimal      | Full (by MMU)   |
|  Priority       | Fixed*        | Configurable | Configurable    |
|  Scheduling     | Cooperative/  | Preemptive   | Preemptive      |
|                 | Preemptive    |              |                 |
|  ---------------+---------------+--------------+-----------------+
|  * Can change at runtime but typically fixed                     |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

对比表：

| 特性 | FreeRTOS任务 | POSIX线程 | Linux进程 |
|------|-------------|----------|-----------|
| 地址空间 | 共享 | 共享 | 隔离 |
| 需要MMU | 否 | 是（通常）| 是 |
| 栈 | 自己的，固定 | 自己的，固定 | 自己的，可增长 |
| 堆 | 共享 | 共享 | 自己的（COW）|
| 创建开销 | ~100周期 | ~10K周期 | ~100K周期 |
| 上下文切换 | ~100周期 | ~1K周期 | ~10K周期 |
| 隔离 | 无 | 最小 | 完全（靠MMU）|

```
KEY INSIGHT:
+------------------------------------------------------------------+
|                                                                  |
|  FreeRTOS tasks are LIGHTWEIGHT because:                         |
|                                                                  |
|  1. No address space switch (no MMU operations)                  |
|  2. No TLB flush                                                 |
|  3. No page table manipulation                                   |
|  4. Just save/restore ~20 registers                              |
|                                                                  |
|  TRADE-OFF:                                                      |
|  - Fast and efficient                                            |
|  - But NO memory protection between tasks                        |
|  - A bug in one task can corrupt another task's memory           |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

关键洞察：FreeRTOS任务轻量因为：无地址空间切换（无MMU操作）、无TLB刷新、无页表操作、只保存/恢复约20个寄存器。

权衡：快速高效，但任务间无内存保护，一个任务的bug可以破坏另一个任务的内存。

---

## 4.3 The Task Control Block (TCB)

```
TCB STRUCTURE (from tasks.c):
+==================================================================+
||                                                                ||
||  typedef struct tskTaskControlBlock                            ||
||  {                                                             ||
||      /* CRITICAL: Must be first member for context switch */   ||
||      volatile StackType_t *pxTopOfStack;                       ||
||                                                                ||
||      /* List items for scheduler lists */                      ||
||      ListItem_t xStateListItem;    /* Ready/Blocked/etc list */||
||      ListItem_t xEventListItem;    /* Event wait list */       ||
||                                                                ||
||      /* Scheduling info */                                     ||
||      UBaseType_t uxPriority;       /* Task priority */         ||
||      UBaseType_t uxBasePriority;   /* For priority inherit */  ||
||                                                                ||
||      /* Stack management */                                    ||
||      StackType_t *pxStack;         /* Stack base address */    ||
||                                                                ||
||      /* Debug/identification */                                ||
||      char pcTaskName[configMAX_TASK_NAME_LEN];                 ||
||                                                                ||
||      /* Optional members (compile-time config) */              ||
||      #if configUSE_MUTEXES == 1                                ||
||          UBaseType_t uxMutexesHeld;                            ||
||      #endif                                                    ||
||      #if configUSE_TRACE_FACILITY == 1                         ||
||          UBaseType_t uxTCBNumber;                              ||
||          UBaseType_t uxTaskNumber;                             ||
||      #endif                                                    ||
||      ...                                                       ||
||  } TCB_t;                                                      ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

TCB结构（来自tasks.c）：

关键成员（必须是第一个成员用于上下文切换）：pxTopOfStack（栈顶指针）。

调度器列表项：xStateListItem（就绪/阻塞等列表）、xEventListItem（事件等待列表）。

调度信息：uxPriority（任务优先级）、uxBasePriority（用于优先级继承）。

栈管理：pxStack（栈基地址）。

调试/标识：pcTaskName（任务名称）。

可选成员（编译时配置）：uxMutexesHeld（如果启用互斥锁）、uxTCBNumber/uxTaskNumber（如果启用trace）。

### Why pxTopOfStack Must Be First

```
CONTEXT SWITCH ASSEMBLY REQUIREMENT:
+------------------------------------------------------------------+
|                                                                  |
|  During context switch, assembly code needs to:                  |
|  1. Save current registers to current task's stack              |
|  2. Load pxCurrentTCB (pointer to TCB)                          |
|  3. Get stack pointer from TCB                                  |
|  4. Restore registers from new task's stack                     |
|                                                                  |
|  If pxTopOfStack is at offset 0:                                 |
|                                                                  |
|  LDR R0, =pxCurrentTCB    ; Load TCB pointer                    |
|  LDR R0, [R0]             ; Dereference to get TCB address      |
|  LDR SP, [R0, #0]         ; SP = TCB->pxTopOfStack (offset 0!)  |
|                                                                  |
|  If it were at offset 8:                                        |
|                                                                  |
|  LDR R0, =pxCurrentTCB                                          |
|  LDR R0, [R0]                                                   |
|  LDR SP, [R0, #8]         ; Extra offset calculation            |
|                                                                  |
|  Keeping it at offset 0 means:                                  |
|  - Simpler assembly                                             |
|  - Portable across architectures                                |
|  - Slightly faster context switch                               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么pxTopOfStack必须是第一个？

上下文切换汇编要求：保存当前寄存器到当前任务栈、加载pxCurrentTCB（TCB指针）、从TCB获取栈指针、从新任务栈恢复寄存器。

如果pxTopOfStack在偏移0：LDR SP, [R0, #0]直接获取。

如果在偏移8：需要额外偏移计算。

保持在偏移0意味着：更简单的汇编、跨架构可移植、稍快的上下文切换。

---

## 4.4 Stack Ownership and Layout

```
TASK MEMORY LAYOUT:
+==================================================================+
||                                                                ||
||  Each task owns its TCB and stack (allocated together):        ||
||                                                                ||
||  xTaskCreate() allocates:                                      ||
||  +----------------------------------------------------------+  ||
||  |                    TCB (Task Control Block)              |  ||
||  +----------------------------------------------------------+  ||
||  |                                                          |  ||
||  |                    Stack Memory                          |  ||
||  |                    (configMINIMAL_STACK_SIZE words)      |  ||
||  |                                                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Stack grows DOWN on ARM (portSTACK_GROWTH = -1):              ||
||                                                                ||
||  High Address +------------------------------------------+     ||
||               |                                          |     ||
||               |  [Free stack space]                      |     ||
||               |                                          |     ||
||               +------------------------------------------+     ||
||               |  Saved R4-R11 (context switch)          |     ||
||               +------------------------------------------+     ||
||               |  Saved R0-R3, R12, LR, PC, xPSR         |     ||
||               |  (exception frame)                       |     ||
||  pxTopOfStack +------------------------------------------+     ||
||               |                                          |     ||
||               |  [Used by current function calls]        |     ||
||               |                                          |     ||
||  Low Address  +------------------------------------------+     ||
||       ^                                                        ||
||       |                                                        ||
||       pxStack (stack base, for overflow detection)             ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

任务内存布局：每个任务拥有自己的TCB和栈（一起分配）。xTaskCreate()分配TCB和栈内存。

在ARM上栈向下增长（portSTACK_GROWTH = -1）：

高地址：空闲栈空间
-> 保存的R4-R11（上下文切换）
-> 保存的R0-R3、R12、LR、PC、xPSR（异常帧）
-> pxTopOfStack指向这里
-> 当前函数调用使用的空间
低地址：pxStack（栈基，用于溢出检测）

### Stack Size Considerations

```
SIZING YOUR STACK:
+------------------------------------------------------------------+
|                                                                  |
|  FACTORS AFFECTING STACK USAGE:                                  |
|                                                                  |
|  1. Local variables in task function and called functions        |
|     void MyTask(void *p)                                         |
|     {                                                            |
|         char buffer[256];   // 256 bytes on stack                |
|         int counters[100];  // 400 bytes on stack                |
|     }                                                            |
|                                                                  |
|  2. Function call depth                                          |
|     TaskFunc() -> ParseData() -> ValidateCRC() -> ...            |
|     Each call: ~16-32 bytes for return address + saved regs      |
|                                                                  |
|  3. ISR preemption (if nested interrupts enabled)                |
|     ISR uses task's stack on some architectures                  |
|                                                                  |
|  4. Context switch overhead (~64 bytes on ARM Cortex-M)          |
|                                                                  |
|  TYPICAL SIZES:                                                  |
|  +------------------------------------------------------------+  |
|  | Simple task (LED blink):       128-256 words               |  |
|  | Medium task (protocol handler): 256-512 words              |  |
|  | Complex task (TCP/IP):          1024+ words                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  1 word = 4 bytes on 32-bit ARM                                  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

栈大小确定因素：

1. 任务函数和调用函数中的局部变量（如256字节buffer、400字节数组）
2. 函数调用深度（每次调用约16-32字节）
3. ISR抢占（某些架构ISR使用任务栈）
4. 上下文切换开销（ARM Cortex-M约64字节）

典型大小：简单任务128-256字、中等任务256-512字、复杂任务1024+字。在32位ARM上1字=4字节。

---

## 4.5 Task States

```
TASK STATE MACHINE:
+==================================================================+
||                                                                ||
||                   +-------------+                              ||
||    xTaskCreate()  |   READY     |  Highest priority ready      ||
||    ------------->|             |------------------------+      ||
||                   +------+------+                       |      ||
||                          |                              |      ||
||        vTaskSuspend()    |    vTaskResume()             |      ||
||          +---------------+----------------+             |      ||
||          |                                |             |      ||
||          v                                |             v      ||
||   +-------------+                  +------+------+             ||
||   |  SUSPENDED  |                  |   RUNNING   |             ||
||   | (explicit)  |                  | (only ONE)  |             ||
||   +-------------+                  +------+------+             ||
||          ^                                |                    ||
||          |                                |                    ||
||          |         vTaskDelay()           |                    ||
||          |         xQueueReceive()        |                    ||
||          |         xSemaphoreTake()       |                    ||
||          |               |                |                    ||
||          |               v                |                    ||
||          |        +-------------+         |                    ||
||          +--------|   BLOCKED   |---------+                    ||
||    vTaskSuspend() | (waiting)   | Event/timeout occurs         ||
||                   +-------------+                              ||
||                                                                ||
||  STATE -> LIST MAPPING:                                        ||
||  +----------------------------------------------------------+  ||
||  | READY    -> pxReadyTasksLists[priority]                  |  ||
||  | BLOCKED  -> xDelayedTaskList or queue's wait list        |  ||
||  | SUSPENDED-> xSuspendedTaskList                           |  ||
||  | RUNNING  -> pxCurrentTCB (not in any list)               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

任务状态机：

READY（就绪）：任务准备运行，等待CPU
RUNNING（运行中）：正在执行，只有一个任务处于此状态
BLOCKED（阻塞）：等待事件（延迟、队列、信号量等）
SUSPENDED（挂起）：被显式挂起，不参与调度

状态到列表的映射：
- READY -> pxReadyTasksLists[priority]
- BLOCKED -> xDelayedTaskList或队列的等待列表
- SUSPENDED -> xSuspendedTaskList
- RUNNING -> pxCurrentTCB（不在任何列表中）

### State Transitions and Their Triggers

| From | To | Trigger |
|------|-----|---------|
| - | Ready | `xTaskCreate()` |
| Ready | Running | Scheduler selects (highest priority) |
| Running | Ready | Preempted by higher priority, time slice expired |
| Running | Blocked | `vTaskDelay()`, `xQueueReceive()`, `xSemaphoreTake()` |
| Blocked | Ready | Timeout expires, event occurs |
| Any | Suspended | `vTaskSuspend()` |
| Suspended | Ready | `vTaskResume()` |
| Any | Deleted | `vTaskDelete()` |

---

## 4.6 Task Creation Deep Dive

```
xTaskCreate() FLOW:
+==================================================================+
||                                                                ||
||  xTaskCreate(TaskFunc, "Name", stackSize, params, priority,    ||
||              &handle)                                          ||
||                                                                ||
||  Step 1: Allocate memory                                       ||
||  +----------------------------------------------------------+  ||
||  | TCB = pvPortMalloc(sizeof(TCB_t))                        |  ||
||  | Stack = pvPortMalloc(stackSize * sizeof(StackType_t))    |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Step 2: Initialize TCB                                        ||
||  +----------------------------------------------------------+  ||
||  | pxNewTCB->pxStack = pxStack                              |  ||
||  | pxNewTCB->uxPriority = uxPriority                        |  ||
||  | strcpy(pxNewTCB->pcTaskName, pcName)                     |  ||
||  | vListInitialiseItem(&pxNewTCB->xStateListItem)           |  ||
||  | vListInitialiseItem(&pxNewTCB->xEventListItem)           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Step 3: Initialize stack (port-specific)                      ||
||  +----------------------------------------------------------+  ||
||  | pxNewTCB->pxTopOfStack =                                 |  ||
||  |     pxPortInitialiseStack(pxTopOfStack,                  |  ||
||  |                           TaskFunc,                      |  ||
||  |                           pvParameters)                  |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Step 4: Add to ready list                                     ||
||  +----------------------------------------------------------+  ||
||  | prvAddTaskToReadyList(pxNewTCB)                          |  ||
||  | -> vListInsertEnd(&pxReadyTasksLists[priority], ...)     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Step 5: Maybe switch context                                  ||
||  +----------------------------------------------------------+  ||
||  | if (newTask->priority > currentTask->priority)           |  ||
||  |     taskYIELD()                                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

xTaskCreate()流程：

步骤1：分配内存。TCB = pvPortMalloc分配TCB，Stack = pvPortMalloc分配栈。

步骤2：初始化TCB。设置栈指针、优先级、名称，初始化列表项。

步骤3：初始化栈（移植层特定）。调用pxPortInitialiseStack创建伪造的异常帧。

步骤4：添加到就绪列表。调用prvAddTaskToReadyList -> vListInsertEnd。

步骤5：可能切换上下文。如果新任务优先级高于当前任务，调用taskYIELD。

### Stack Initialization Detail

```
pxPortInitialiseStack() CREATES FAKE EXCEPTION FRAME:
+------------------------------------------------------------------+
|                                                                  |
|  When a task first runs, the CPU restores this frame as if      |
|  returning from an interrupt:                                    |
|                                                                  |
|  High Address                                                    |
|  +--------------------+                                          |
|  | xPSR (0x01000000)  | <- Thumb bit set                        |
|  +--------------------+                                          |
|  | PC (TaskFunction)  | <- Where execution will start           |
|  +--------------------+                                          |
|  | LR (ExitError)     | <- Catches accidental returns           |
|  +--------------------+                                          |
|  | R12 (0)            |                                          |
|  +--------------------+                                          |
|  | R3 (0)             |                                          |
|  +--------------------+                                          |
|  | R2 (0)             |                                          |
|  +--------------------+                                          |
|  | R1 (0)             |                                          |
|  +--------------------+                                          |
|  | R0 (pvParameters)  | <- Task function argument               |
|  +--------------------+                                          |
|  | R11 (0)            | <- Manually saved registers             |
|  | R10 (0)            |                                          |
|  | R9 (0)             |                                          |
|  | R8 (0)             |                                          |
|  | R7 (0)             |                                          |
|  | R6 (0)             |                                          |
|  | R5 (0)             |                                          |
|  | R4 (0)             |                                          |
|  +--------------------+                                          |
|  pxTopOfStack ->                                                 |
|  Low Address                                                     |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

pxPortInitialiseStack()创建伪异常帧：当任务首次运行时，CPU恢复这个帧，就像从中断返回一样。

帧内容（从高地址到低地址）：
- xPSR (0x01000000)：设置Thumb位
- PC (TaskFunction)：执行将从这里开始
- LR (ExitError)：捕获意外返回
- R12, R3-R0：R0是任务函数参数(pvParameters)
- R11-R4：手动保存的寄存器，初始化为0

pxTopOfStack指向帧底部。

---

## Summary

```
TASK MODEL KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  WHAT A TASK IS:                                               ||
||  - Independent execution context with own stack                ||
||  - Scheduled by priority                                       ||
||  - Can block waiting for events                                ||
||  - Never returns (infinite loop or self-delete)                ||
||                                                                ||
||  TCB CONTAINS:                                                 ||
||  - Stack pointer (MUST be first member)                        ||
||  - List items (for scheduler lists)                            ||
||  - Priority information                                        ||
||  - Stack base (for overflow detection)                         ||
||  - Debug name                                                  ||
||                                                                ||
||  KEY DIFFERENCES FROM THREADS/PROCESSES:                       ||
||  - No memory protection                                        ||
||  - Much lighter weight (~100 cycle context switch)             ||
||  - Fixed priority (usually)                                    ||
||  - Statically sized stacks                                     ||
||                                                                ||
||  DESIGN CONSTRAINTS:                                           ||
||  - Size stack carefully (no automatic growth)                  ||
||  - Minimize local variables in deeply nested calls             ||
||  - Enable stack overflow checking during development           ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Scheduling: How FreeRTOS Actually Runs Code](05-scheduling.md)
