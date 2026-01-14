# Section 13: How to Learn FreeRTOS Effectively from Source Code

## 13.1 Recommended Reading Order

```
PHASE 1: FOUNDATION (Day 1-2)
+==================================================================+
||                                                                ||
||  START HERE:                                                   ||
||                                                                ||
||  1. list.h (~100 lines)                                        ||
||     - Understand List_t and ListItem_t structures              ||
||     - See the intrusive linked list design                     ||
||                                                                ||
||  2. list.c (~200 lines)                                        ||
||     - Study vListInitialise, vListInsert, uxListRemove         ||
||     - Trace through operations mentally                        ||
||                                                                ||
||  WHY START HERE:                                               ||
||  - Small, self-contained                                       ||
||  - No RTOS dependencies                                        ||
||  - Foundation for EVERYTHING else                              ||
||                                                                ||
+==================================================================+

PHASE 2: TASK MODEL (Day 3-5)
+==================================================================+
||                                                                ||
||  3. task.h (public API)                                        ||
||     - Learn the task API surface                               ||
||     - Note the handle types                                    ||
||                                                                ||
||  4. tasks.c - FIRST PASS (structures only)                     ||
||     - Find TCB_t definition (search "typedef struct")          ||
||     - Find pxReadyTasksLists, xDelayedTaskList                 ||
||     - Find pxCurrentTCB                                        ||
||     - DON'T read functions yet                                 ||
||                                                                ||
||  5. tasks.c - SECOND PASS (key functions)                      ||
||     - xTaskCreate / prvInitialiseTaskLists                     ||
||     - vTaskStartScheduler                                      ||
||     - xTaskIncrementTick                                       ||
||     - vTaskSwitchContext                                       ||
||                                                                ||
+==================================================================+

PHASE 3: PORT LAYER (Day 6-7)
+==================================================================+
||                                                                ||
||  6. portmacro.h (your target architecture)                     ||
||     - Type definitions                                         ||
||     - Critical section macros                                  ||
||     - portYIELD macro                                          ||
||                                                                ||
||  7. port.c (your target architecture)                          ||
||     - pxPortInitialiseStack (stack setup)                      ||
||     - xPortStartScheduler                                      ||
||     - vPortSVCHandler, xPortPendSVHandler (context switch)     ||
||                                                                ||
||  TIP: Read with hardware reference manual open                 ||
||                                                                ||
+==================================================================+

PHASE 4: SYNCHRONIZATION (Day 8-10)
+==================================================================+
||                                                                ||
||  8. queue.h (public API)                                       ||
||     - Note the generic queue functions                         ||
||                                                                ||
||  9. semphr.h                                                   ||
||     - See how semaphore macros wrap queue functions            ||
||                                                                ||
||  10. queue.c - FIRST PASS                                      ||
||      - Find Queue_t structure                                  ||
||      - Understand the union for semaphore/mutex data           ||
||                                                                ||
||  11. queue.c - SECOND PASS                                     ||
||      - xQueueGenericSend (and SendFromISR)                     ||
||      - xQueueReceive                                           ||
||      - prvUnlockQueue                                          ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

推荐阅读顺序：

阶段1（基础，第1-2天）：list.h和list.c。理解List_t和ListItem_t结构，看侵入式链表设计。从这里开始因为小、独立、是其他一切的基础。

阶段2（任务模型，第3-5天）：task.h学API表面，tasks.c第一遍只看结构（TCB_t、pxReadyTasksLists等），第二遍看关键函数（xTaskCreate、vTaskStartScheduler等）。

阶段3（移植层，第6-7天）：portmacro.h看类型定义和宏，port.c看栈设置和上下文切换。建议同时打开硬件参考手册。

阶段4（同步，第8-10天）：queue.h和semphr.h看API和宏包装，queue.c看Queue_t结构和关键函数。

---

## 13.2 What to Ignore Initially

```
SKIP THESE ON FIRST READ:
+==================================================================+
||                                                                ||
||  1. CO-ROUTINES (croutine.c, croutine.h)                       ||
||     - Legacy feature                                           ||
||     - Not used in modern designs                               ||
||     - Will confuse you                                         ||
||                                                                ||
||  2. MPU SUPPORT                                                ||
||     - Look for #if portUSING_MPU_WRAPPERS                      ||
||     - Skip these sections                                      ||
||     - Complex and rare                                         ||
||                                                                ||
||  3. SMP SUPPORT                                                ||
||     - Look for #if configNUMBER_OF_CORES                       ||
||     - Skip these sections                                      ||
||     - Multi-core is advanced topic                             ||
||                                                                ||
||  4. STREAM BUFFERS (stream_buffer.c)                           ||
||     - Useful but not core                                      ||
||     - Read after understanding queues                          ||
||                                                                ||
||  5. TRACE MACROS                                               ||
||     - traceTASK_CREATE, etc.                                   ||
||     - For debugging tools                                      ||
||     - Treat as no-ops initially                                ||
||                                                                ||
||  6. ASSERTS AND VALIDATION                                     ||
||     - configASSERT() calls                                     ||
||     - Useful but not core logic                                ||
||     - Focus on what happens when asserts pass                  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

初读时跳过：

1. 协程（croutine.c）：遗留功能，现代设计不用。
2. MPU支持：#if portUSING_MPU_WRAPPERS部分，复杂且少用。
3. SMP支持：#if configNUMBER_OF_CORES部分，多核是高级主题。
4. 流缓冲（stream_buffer.c）：有用但非核心，理解队列后再读。
5. Trace宏：调试工具用，初始当作空操作。
6. 断言和验证：configASSERT()调用，聚焦断言通过时的逻辑。

---

## 13.3 What to Trace with a Debugger

```
DEBUGGING EXERCISES:
+==================================================================+
||                                                                ||
||  EXERCISE 1: Task Creation                                     ||
||  +----------------------------------------------------------+  ||
||  | 1. Set breakpoint at xTaskCreate                         |  ||
||  | 2. Step through to see:                                  |  ||
||  |    - Memory allocation for TCB and stack                 |  ||
||  |    - TCB field initialization                            |  ||
||  |    - pxPortInitialiseStack creating fake exception frame |  ||
||  |    - Task being added to ready list                      |  ||
||  | 3. Examine pxReadyTasksLists after creation              |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXERCISE 2: Context Switch                                    ||
||  +----------------------------------------------------------+  ||
||  | 1. Create two tasks at different priorities              |  ||
||  | 2. Set breakpoint at vTaskSwitchContext                  |  ||
||  | 3. Observe:                                              |  ||
||  |    - pxCurrentTCB value before and after                 |  ||
||  |    - Which task is selected                              |  ||
||  |    - Stack pointer changes                               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXERCISE 3: Blocking on Queue                                 ||
||  +----------------------------------------------------------+  ||
||  | 1. Create queue and task that receives from it           |  ||
||  | 2. Set breakpoint at xQueueReceive                       |  ||
||  | 3. Step through to see:                                  |  ||
||  |    - Task added to xTasksWaitingToReceive                |  ||
||  |    - Task moved to delayed list (if timeout)             |  ||
||  |    - Context switch to another task                      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXERCISE 4: Tick Interrupt                                    ||
||  +----------------------------------------------------------+  ||
||  | 1. Set breakpoint at xTaskIncrementTick                  |  ||
||  | 2. Have a task blocked with vTaskDelay                   |  ||
||  | 3. Observe:                                              |  ||
||  |    - xTickCount incrementing                             |  ||
||  |    - Delayed task being checked                          |  ||
||  |    - Task moved to ready list when delay expires         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

调试练习：

练习1（任务创建）：在xTaskCreate设断点，步进看TCB和栈的内存分配、TCB字段初始化、pxPortInitialiseStack创建伪异常帧、任务加入就绪列表。

练习2（上下文切换）：创建两个不同优先级任务，在vTaskSwitchContext设断点，观察pxCurrentTCB变化、选择哪个任务、栈指针变化。

练习3（队列阻塞）：创建队列和接收任务，在xQueueReceive设断点，步进看任务加入xTasksWaitingToReceive、移到延迟列表、上下文切换。

练习4（Tick中断）：在xTaskIncrementTick设断点，有任务用vTaskDelay阻塞，观察xTickCount递增、检查延迟任务、延迟到期时任务移到就绪列表。

---

## 13.4 Safe Experimentation

```
EXPERIMENTS TO TRY:
+==================================================================+
||                                                                ||
||  EXPERIMENT 1: Priority Inversion                              ||
||  +----------------------------------------------------------+  ||
||  | Create 3 tasks: Low, Medium, High priority               |  ||
||  | Low takes mutex, High needs same mutex                   |  ||
||  | Medium is CPU-bound (no blocking)                        |  ||
||  |                                                          |  ||
||  | First: Use binary semaphore (no priority inheritance)    |  ||
||  | Observe: High waits while Medium runs                    |  ||
||  |                                                          |  ||
||  | Then: Use mutex (with priority inheritance)              |  ||
||  | Observe: Low inherits High's priority, High runs sooner  |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXPERIMENT 2: Stack Overflow                                  ||
||  +----------------------------------------------------------+  ||
||  | Create task with very small stack (64 words)             |  ||
||  | Have it call deeply nested functions                     |  ||
||  | Enable configCHECK_FOR_STACK_OVERFLOW = 2                |  ||
||  | Observe: vApplicationStackOverflowHook called            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXPERIMENT 3: Queue Full Behavior                             ||
||  +----------------------------------------------------------+  ||
||  | Create queue length 5                                    |  ||
||  | Producer sends faster than consumer receives             |  ||
||  | Try with timeout 0, 100ms, portMAX_DELAY                 |  ||
||  | Observe: Return values, blocking behavior                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXPERIMENT 4: Time Slicing                                    ||
||  +----------------------------------------------------------+  ||
||  | Create 3 tasks at SAME priority                          |  ||
||  | Each toggles different LED in loop                       |  ||
||  | Observe: Round-robin execution                           |  ||
||  |                                                          |  ||
||  | Then: Set configUSE_TIME_SLICING = 0                     |  ||
||  | Observe: Only first task runs (no sharing)               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

实验：

实验1（优先级反转）：创建低/中/高三优先级任务，低持有mutex，高需要同一mutex，中CPU密集。先用二进制信号量观察高等待中运行，再用mutex观察优先级继承。

实验2（栈溢出）：创建小栈任务（64字），深度嵌套调用，启用溢出检查，观察钩子被调用。

实验3（队列满行为）：创建长度5队列，生产者比消费者快，尝试不同超时（0、100ms、portMAX_DELAY），观察返回值和阻塞行为。

实验4（时间片）：创建3个同优先级任务各闪烁不同LED，观察轮转；然后设configUSE_TIME_SLICING=0，观察只有第一个任务运行。

---

## 13.5 Building Mental Models

```
MENTAL MODEL BUILDING TECHNIQUES:
+==================================================================+
||                                                                ||
||  1. DRAW THE DATA STRUCTURES                                   ||
||  +----------------------------------------------------------+  ||
||  | For each component, draw:                                |  ||
||  | - The structures (TCB, Queue_t, List_t)                  |  ||
||  | - How they link together                                 |  ||
||  | - What changes during operations                         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  2. TRACE STATE TRANSITIONS                                    ||
||  +----------------------------------------------------------+  ||
||  | For key operations, trace:                               |  ||
||  | - Initial state                                          |  ||
||  | - Each step's effect on state                            |  ||
||  | - Final state                                            |  ||
||  |                                                          |  ||
||  | Example: What happens when task calls vTaskDelay(100)?   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  3. ASK "WHY" QUESTIONS                                        ||
||  +----------------------------------------------------------+  ||
||  | For each design decision, ask:                           |  ||
||  | - Why is TCB_t.pxTopOfStack first member?                |  ||
||  | - Why two delayed lists?                                 |  ||
||  | - Why use PendSV for context switch?                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  4. CONNECT TO HARDWARE                                        ||
||  +----------------------------------------------------------+  ||
||  | Understand the hardware basis:                           |  ||
||  | - How does SysTick work?                                 |  ||
||  | - What does PendSV do?                                   |  ||
||  | - How does BASEPRI mask interrupts?                      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

建立心智模型的技术：

1. 画数据结构：对每个组件画结构（TCB、Queue_t、List_t）、它们如何链接、操作期间什么变化。

2. 跟踪状态转换：对关键操作跟踪初始状态、每步效果、最终状态。例如：任务调用vTaskDelay(100)时发生什么？

3. 问"为什么"：对每个设计决策问为什么。TCB_t.pxTopOfStack为什么是第一个成员？为什么两个延迟列表？为什么用PendSV上下文切换？

4. 连接到硬件：理解硬件基础。SysTick如何工作？PendSV做什么？BASEPRI如何屏蔽中断？

---

## Summary

```
LEARNING APPROACH SUMMARY:
+==================================================================+
||                                                                ||
||  READ IN ORDER:                                                ||
||  list.c -> tasks.c (structures) -> port.c -> queue.c           ||
||                                                                ||
||  SKIP INITIALLY:                                               ||
||  Co-routines, MPU, SMP, trace macros                           ||
||                                                                ||
||  DEBUG TO UNDERSTAND:                                          ||
||  Task creation, context switch, blocking, tick handling        ||
||                                                                ||
||  EXPERIMENT SAFELY:                                            ||
||  Priority inversion, stack overflow, queue behavior            ||
||                                                                ||
||  BUILD MENTAL MODELS:                                          ||
||  Draw structures, trace states, ask why, connect to hardware   ||
||                                                                ||
||  KEY INSIGHT:                                                  ||
||  FreeRTOS is small enough to fully understand.                 ||
||  The entire kernel is ~6000 lines of well-documented C.        ||
||  Take the time to truly understand it.                         ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Final Mental Model](14-final-mental-model.md)
