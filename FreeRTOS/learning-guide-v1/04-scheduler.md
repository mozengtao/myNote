# Section 4: The Scheduler (The Heart of FreeRTOS)

The scheduler is the core of any RTOS. Understanding how FreeRTOS schedules tasks is essential for writing correct, efficient, and predictable embedded systems.

## 4.1 Task States

FreeRTOS tasks exist in one of several states. The scheduler's job is to transition tasks between states and select which task runs.

### State Diagram

```
                         +-----------------------------+
                         |                             |
                         v                             |
+=========+         +=========+         +=========+    |
|         |  Create |         |  Select |         |    |
|  (New)  |-------->|  READY  |-------->| RUNNING |----+
|         |         |         |<--------|         |    | Preempted
+=========+         +====+====+  Yield  +====+====+    | or
                         ^                   |         | Time slice
                         |                   |         |
               +---------+                   |         |
               |                             |         |
          Event|                             |         |
          occurs                             |         |
               |         +=========+         |         |
               |         |         |  Delay/ |         |
               +---------| BLOCKED |<--------+         |
               |         |         |  Wait   |         |
               |         +=========+         |         |
               |                             |         |
               |         +=========+         |         |
               |         |         |Suspend  |         |
               +---------+SUSPENDED|<--------+---------+
                         |         |  vTaskSuspend()
                         +=========+
                              |
                              | vTaskDelete()
                              v
                         +=========+
                         |         |
                         | DELETED |
                         |         |
                         +=========+
```

**Chinese Explanation (中文说明):**

FreeRTOS任务存在于以下状态之一：

就绪(Ready)：任务可以运行，等待调度器选择。
运行(Running)：任务正在CPU上执行。
阻塞(Blocked)：任务等待事件（延时到期、队列数据、信号量等）。
挂起(Suspended)：任务被明确挂起，直到被恢复。
删除(Deleted)：任务被删除，等待清理。

### State Definitions

| State | Definition | Cause | Exit Condition |
|-------|------------|-------|----------------|
| **Ready** | Can run, waiting for CPU | Created, unblocked, resumed | Selected by scheduler |
| **Running** | Currently executing | Selected by scheduler | Preempted, yields, blocks, suspends |
| **Blocked** | Waiting for event | Called blocking API (delay, queue, etc.) | Event occurs or timeout |
| **Suspended** | Explicitly paused | `vTaskSuspend()` called | `vTaskResume()` called |
| **Deleted** | Marked for deletion | `vTaskDelete()` called | Idle task frees memory |

### Why These States Exist

```
+------------------------------------------------------------------+
|  DESIGN RATIONALE                                                |
+------------------------------------------------------------------+
|                                                                  |
|  READY vs RUNNING distinction:                                   |
|  - Multiple tasks may be "able to run"                           |
|  - Only ONE task is "actually running" (single core)             |
|  - Scheduler picks from READY, puts into RUNNING                 |
|                                                                  |
|  BLOCKED state:                                                  |
|  - Tasks shouldn't spin-wait (wastes CPU)                        |
|  - Blocking allows other tasks to run                            |
|  - Events are tracked in kernel, not polled                      |
|                                                                  |
|  SUSPENDED state:                                                |
|  - Sometimes you need to "pause" a task indefinitely             |
|  - Different from BLOCKED: no automatic wake condition           |
|  - Useful for debug, mode changes                                |
|                                                                  |
|  DELETED state:                                                  |
|  - Can't free memory from within the task itself                 |
|  - Idle task does cleanup when nothing else runs                 |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么需要这些状态？

Ready vs Running：多个任务可能"能够运行"，但只有一个任务"正在运行"（单核）。调度器从Ready选择，放入Running。

Blocked状态：任务不应该忙等待（浪费CPU），阻塞允许其他任务运行，事件在内核中跟踪而不是轮询。

Suspended状态：有时需要无限期"暂停"任务，与Blocked不同：没有自动唤醒条件，用于调试、模式切换。

Deleted状态：不能从任务内部释放内存（会破坏当前栈），Idle任务在没有其他任务运行时清理。

---

## 4.2 Task Control Block (TCB)

The TCB is the data structure that represents a task. Every task has one TCB.

### TCB_t Structure (from tasks.c)

```
TCB_t Structure Layout:
+------------------------------------------------------------------+
| MUST BE FIRST:                                                   |
+------------------------------------------------------------------+
| pxTopOfStack    | Pointer to current stack top                   |
|                 | Used by context switch (assembly accesses it)  |
+------------------------------------------------------------------+
| xMPUSettings    | MPU configuration (if MPU port)                 |
|                 | MUST BE SECOND if present                       |
+------------------------------------------------------------------+
|                                                                  |
| TASK IDENTIFICATION:                                             |
+------------------------------------------------------------------+
| pcTaskName[]    | Human-readable name (for debugging)            |
| uxTCBNumber     | Unique ID for trace tools                       |
| uxTaskNumber    | User-assignable number                          |
+------------------------------------------------------------------+
|                                                                  |
| SCHEDULING:                                                      |
+------------------------------------------------------------------+
| uxPriority      | Current priority (0 = lowest)                   |
| uxBasePriority  | Original priority (for priority inheritance)   |
| xStateListItem  | Links task to state list (ready/blocked/etc.)  |
| xEventListItem  | Links task to event wait list                  |
+------------------------------------------------------------------+
|                                                                  |
| STACK:                                                           |
+------------------------------------------------------------------+
| pxStack         | Pointer to stack START (for overflow check)    |
| pxEndOfStack    | Pointer to stack END (if stack grows up)       |
+------------------------------------------------------------------+
|                                                                  |
| OPTIONAL FEATURES:                                               |
+------------------------------------------------------------------+
| uxCriticalNesting | Critical section nesting depth               |
| uxMutexesHeld     | Count of held mutexes                         |
| ulRunTimeCounter  | CPU time used (if stats enabled)              |
| ulNotifiedValue[] | Task notification values                       |
| ucNotifyState[]   | Notification pending flags                     |
+------------------------------------------------------------------+
```

### TCB Memory Layout

```
Memory View:

TCB (statically or dynamically allocated):
+---------------------------+  <-- TCB start address
| pxTopOfStack        ------|--+
+---------------------------+  |
| xStateListItem            |  |
+---------------------------+  |
| xEventListItem            |  |
+---------------------------+  |
| uxPriority                |  |
+---------------------------+  |
| pxStack             ------|--|-+
+---------------------------+  | |
| pcTaskName[]              |  | |
+---------------------------+  | |
| ...other fields...        |  | |
+---------------------------+  | |
                               | |
Stack (separate allocation):   | |
+---------------------------+  | |
| (stack bottom)            |<-|-+
| ...                       |  |
| Local variables           |  |
| Return addresses          |  |
| Saved registers           |  |
| (stack top)               |<-+
+---------------------------+
```

**Chinese Explanation (中文说明):**

TCB（任务控制块）是代表任务的数据结构。每个任务有一个TCB。

关键字段：
- pxTopOfStack：必须是第一个字段，指向当前栈顶，上下文切换时汇编代码直接访问。
- xStateListItem/xEventListItem：链表项，用于将任务链接到各种内核列表。
- uxPriority：当前优先级，uxBasePriority：基础优先级（用于优先级继承）。
- pxStack：栈起始地址，用于栈溢出检查。
- 可选字段：临界区嵌套计数、持有的互斥锁数、CPU时间统计、任务通知等。

### Why pxTopOfStack Must Be First

```c
// From vPortSVCHandler (ARM Cortex-M):
__asm volatile (
    "ldr r3, =pxCurrentTCB  \n"  // Get address of pxCurrentTCB
    "ldr r1, [r3]           \n"  // r1 = pxCurrentTCB (TCB address)
    "ldr r0, [r1]           \n"  // r0 = first word of TCB = pxTopOfStack
    ...
);

// Assembly assumes: TCB address + 0 = stack pointer
// If pxTopOfStack is not first, context switch BREAKS
```

**Chinese Explanation (中文说明):**

为什么pxTopOfStack必须是第一个字段？

上下文切换的汇编代码假设TCB的第一个字（偏移量0）就是栈指针。代码直接解引用pxCurrentTCB来获取栈指针，无需计算偏移。如果pxTopOfStack不在第一个位置，上下文切换会失败。

---

## 4.3 Priority-Based Preemptive Scheduling

### How Priorities Work

```
Priority Levels (configMAX_PRIORITIES = 5 example):
+------------------------------------------------------------------+
|                                                                  |
|  Priority 4: [Highest] Motor Control (runs first if ready)       |
|  Priority 3: Communication Task                                  |
|  Priority 2: Sensor Processing                                   |
|  Priority 1: Display Update                                      |
|  Priority 0: [Lowest] Idle Task (always ready)                   |
|                                                                  |
+------------------------------------------------------------------+

Ready Lists (array of lists, one per priority):

pxReadyTasksLists[4]: [ MotorTask ] -> [end]
pxReadyTasksLists[3]: [ CommTask ] -> [end]
pxReadyTasksLists[2]: [ SensorTask1 ] -> [ SensorTask2 ] -> [end]
pxReadyTasksLists[1]: [ DisplayTask ] -> [end]
pxReadyTasksLists[0]: [ IdleTask ] -> [end]
                 ^
                 |
    uxTopReadyPriority = 4 (tracks highest non-empty level)
```

### Scheduling Rule

```
THE FUNDAMENTAL RULE:
+==================================================================+
||                                                                ||
||  The HIGHEST PRIORITY task that is READY will ALWAYS run.     ||
||                                                                ||
||  If multiple tasks at same priority: ROUND-ROBIN within       ||
||  that priority level (one time slice each).                   ||
||                                                                ||
+==================================================================+
```

### Task Selection Algorithm

```c
// Simplified from taskSELECT_HIGHEST_PRIORITY_TASK macro in tasks.c

// Find highest priority with ready tasks:
UBaseType_t uxTopPriority = uxTopReadyPriority;

while( listLIST_IS_EMPTY( &pxReadyTasksLists[ uxTopPriority ] ) )
{
    --uxTopPriority;
    // Will always find IdleTask at priority 0
}

// Round-robin within that priority:
listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &pxReadyTasksLists[ uxTopPriority ] );
```

**Chinese Explanation (中文说明):**

优先级调度规则：最高优先级的就绪任务总是运行。如果同优先级有多个任务，在该优先级内轮询（每个任务一个时间片）。

任务选择算法：从uxTopReadyPriority（最高优先级）开始向下搜索，找到第一个非空的就绪列表。在该优先级内使用listGET_OWNER_OF_NEXT_ENTRY进行轮询选择。IdleTask总是存在于优先级0，保证总有任务可运行。

### What "Deterministic" Means

```
Deterministic Scheduling:

Given:
  - Task A at priority 3, BLOCKED
  - Task B at priority 2, RUNNING
  - Task C at priority 1, READY

When Task A unblocks:
  - Task A IMMEDIATELY preempts Task B
  - NOT "Task A runs next time scheduler checks"
  - NOT "Task A runs after Task B's time slice"
  - Task B is moved to READY, Task A moves to RUNNING

Timing guarantee:
  - Preemption happens within ONE TICK or less
  - Bounded by: interrupt latency + context switch time
  - Typically: 1-10 microseconds on Cortex-M
```

**Chinese Explanation (中文说明):**

"确定性"调度意味着：高优先级任务解除阻塞后立即抢占低优先级任务。不是"下次调度器检查时运行"，不是"当前任务时间片结束后运行"，而是立即。

时间保证：抢占在一个tick内或更短时间内发生，受限于中断延迟+上下文切换时间。在Cortex-M上通常为1-10微秒。

---

## 4.4 Context Switching

### What Triggers a Context Switch

```
Context Switch Triggers:
+------------------------------------------------------------------+
|                                                                  |
|  1. TICK INTERRUPT (SysTick):                                    |
|     - Time slice expired for current task                        |
|     - Delayed task's wake time reached                           |
|     - Periodic check for higher priority ready task              |
|                                                                  |
|  2. EXPLICIT YIELD:                                              |
|     - Task calls taskYIELD()                                     |
|     - Task calls blocking API (xQueueReceive, vTaskDelay, etc.)  |
|                                                                  |
|  3. UNBLOCK EVENT:                                               |
|     - ISR wakes higher priority task (xSemaphoreGiveFromISR)     |
|     - Task wakes higher priority task (xTaskNotifyGive)          |
|                                                                  |
|  4. PRIORITY CHANGE:                                             |
|     - vTaskPrioritySet() makes another task higher priority      |
|     - Priority inheritance raises a task's priority              |
|                                                                  |
+------------------------------------------------------------------+
```

### SysTick and PendSV

```
Cortex-M Context Switch Architecture:
+------------------------------------------------------------------+
|                                                                  |
|   SysTick_Handler:           PendSV_Handler:                     |
|   +--------------------+     +--------------------+              |
|   | Increment tick     |     | Save context       |              |
|   | Check delayed      |     |   (R4-R11)         |              |
|   | tasks              |     | vTaskSwitchContext |              |
|   | If switch needed:  |     |   (select next)    |              |
|   |   Set PendSV flag  |---->| Restore context    |              |
|   +--------------------+     |   (R4-R11)         |              |
|                              | Return to new task |              |
|                              +--------------------+              |
|                                                                  |
+------------------------------------------------------------------+

WHY PENDSV?
  - SysTick is high priority (can't do long operations)
  - PendSV is lowest priority (deferred)
  - Ensures ISRs complete before context switch
  - Cleaner interrupt nesting
```

**Chinese Explanation (中文说明):**

触发上下文切换的情况：
1. Tick中断：时间片到期、延时任务唤醒时间到达、周期性检查
2. 显式让出：taskYIELD()、阻塞API调用
3. 解除阻塞事件：ISR唤醒高优先级任务、任务唤醒高优先级任务
4. 优先级改变：vTaskPrioritySet()、优先级继承

为什么使用PendSV？SysTick是高优先级中断（不能做长操作），PendSV是最低优先级（延迟处理）。这确保所有ISR完成后才进行上下文切换，保持中断嵌套清晰。

### Deferred Interrupt Handling

```
Why Deferred Context Switch?
+------------------------------------------------------------------+
|                                                                  |
|  WRONG: Switch context inside SysTick:                           |
|                                                                  |
|  [High Priority ISR]                                             |
|        |                                                         |
|        v                                                         |
|  [SysTick ISR] ---> [Context Switch] ---> [New Task]             |
|        ^                                                         |
|        |                                                         |
|  Problem: High priority ISR still pending!                       |
|  New task runs before high priority ISR completes!               |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  CORRECT: Defer to PendSV:                                       |
|                                                                  |
|  [High Priority ISR] completes first                             |
|        |                                                         |
|        v                                                         |
|  [SysTick ISR] sets PendSV flag                                  |
|        |                                                         |
|        v                                                         |
|  [Other ISRs] complete                                           |
|        |                                                         |
|        v                                                         |
|  [PendSV ISR] (lowest priority) does context switch              |
|        |                                                         |
|        v                                                         |
|  [New Task] runs                                                 |
|                                                                  |
+------------------------------------------------------------------+
```

### Context Switch Code Flow

```
vTaskSwitchContext() Flow (from tasks.c):
+------------------------------------------------------------------+
|                                                                  |
|  1. If scheduler suspended:                                      |
|     - Set xYieldPending = pdTRUE                                 |
|     - Return (don't switch now)                                  |
|                                                                  |
|  2. Check for stack overflow (if enabled)                        |
|     - taskCHECK_FOR_STACK_OVERFLOW()                             |
|                                                                  |
|  3. Select highest priority ready task:                          |
|     - taskSELECT_HIGHEST_PRIORITY_TASK()                         |
|     - Updates pxCurrentTCB                                       |
|                                                                  |
|  4. Trace hooks (if tracing enabled)                             |
|     - traceTASK_SWITCHED_IN()                                    |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

延迟上下文切换的原因：如果在SysTick内部切换，可能有高优先级ISR还在等待处理。使用PendSV（最低优先级）延迟切换，确保所有ISR完成后才切换到新任务。

vTaskSwitchContext()流程：
1. 如果调度器挂起，设置xYieldPending并返回
2. 检查栈溢出（如果启用）
3. 选择最高优先级就绪任务，更新pxCurrentTCB
4. 调用跟踪钩子（如果启用）

---

## Summary

```
SCHEDULER MENTAL MODEL:
+==================================================================+
||                                                                ||
||  1. Every task has a TCB with state and priority              ||
||                                                                ||
||  2. Ready tasks are in priority-indexed lists                 ||
||                                                                ||
||  3. Highest priority ready task ALWAYS runs                   ||
||                                                                ||
||  4. Context switch is triggered by:                           ||
||     - Tick (time slice / wake time)                           ||
||     - Event (task unblocked)                                  ||
||     - Yield (task gives up CPU)                               ||
||                                                                ||
||  5. PendSV performs actual switch at lowest ISR priority      ||
||                                                                ||
||  6. Switch time is bounded and deterministic                  ||
||                                                                ||
+==================================================================+
```

| Concept | Implementation | File |
|---------|----------------|------|
| Task state | TCB_t.xStateListItem in state list | tasks.c |
| Priority | pxReadyTasksLists[priority] | tasks.c |
| Selection | taskSELECT_HIGHEST_PRIORITY_TASK | tasks.c |
| Trigger | xTaskIncrementTick, portYIELD | tasks.c, port.c |
| Switch | vTaskSwitchContext, PendSV | tasks.c, port.c |

**Next Section**: [Lists: The Hidden Backbone](05-lists.md)
