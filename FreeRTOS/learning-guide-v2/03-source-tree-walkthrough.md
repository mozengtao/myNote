# Section 3: FreeRTOS Source Tree Walkthrough

## 3.1 Repository Layout Overview

```
FREERTOS KERNEL SOURCE TREE:
+==================================================================+
||                                                                ||
||  FreeRTOS-Kernel/                                              ||
||  |                                                             ||
||  +-- croutine.c          Co-routines (legacy, rarely used)     ||
||  +-- event_groups.c      Event group implementation            ||
||  +-- list.c              Generic list (foundation)             ||
||  +-- queue.c             Queues, semaphores, mutexes           ||
||  +-- stream_buffer.c     Stream/message buffers                ||
||  +-- tasks.c             Scheduler and task management         ||
||  +-- timers.c            Software timers                       ||
||  |                                                             ||
||  +-- include/            Public headers (API)                  ||
||  |   +-- FreeRTOS.h      Main include file                     ||
||  |   +-- task.h          Task API                              ||
||  |   +-- queue.h         Queue/semaphore/mutex API             ||
||  |   +-- semphr.h        Semaphore macros (wraps queue.h)      ||
||  |   +-- timers.h        Timer API                             ||
||  |   +-- event_groups.h  Event group API                       ||
||  |   +-- list.h          List structure definitions            ||
||  |   +-- portable.h      Port layer interface                  ||
||  |   +-- projdefs.h      Project definitions (pdTRUE, etc.)    ||
||  |                                                             ||
||  +-- portable/           Architecture-specific code            ||
||      +-- GCC/            GCC compiler ports                    ||
||      |   +-- ARM_CM4F/   ARM Cortex-M4F port                   ||
||      |   +-- ARM_CM3/    ARM Cortex-M3 port                    ||
||      |   +-- RISC-V/     RISC-V port                           ||
||      |   +-- ...                                               ||
||      +-- IAR/            IAR compiler ports                    ||
||      +-- MemMang/        Memory allocators                     ||
||          +-- heap_1.c    Never-free allocator                  ||
||          +-- heap_2.c    Best-fit, no coalesce                 ||
||          +-- heap_3.c    Wrapper around malloc                 ||
||          +-- heap_4.c    Best-fit with coalescing              ||
||          +-- heap_5.c    heap_4 + multiple regions             ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

FreeRTOS内核源码树结构：

根目录核心文件：croutine.c（协程，遗留功能）、event_groups.c（事件组）、list.c（通用列表，基础）、queue.c（队列/信号量/互斥锁）、stream_buffer.c（流/消息缓冲）、tasks.c（调度器和任务管理）、timers.c（软件定时器）。

include目录：公共头文件（API），包括FreeRTOS.h（主头文件）、task.h、queue.h、semphr.h、timers.h、event_groups.h、list.h、portable.h、projdefs.h。

portable目录：架构相关代码，按编译器和处理器分类，还包含MemMang内存分配器（heap_1到heap_5）。

---

## 3.2 Core Source Files Deep Dive

### `list.c` - The Foundation

```
WHY list.c IS CENTRAL:
+------------------------------------------------------------------+
|                                                                  |
|  Everything in FreeRTOS uses lists:                              |
|                                                                  |
|  +------------------+    +----------------------------------+    |
|  | Ready tasks      | -> | pxReadyTasksLists[priority]      |    |
|  +------------------+    +----------------------------------+    |
|                                                                  |
|  +------------------+    +----------------------------------+    |
|  | Delayed tasks    | -> | xDelayedTaskList                 |    |
|  +------------------+    +----------------------------------+    |
|                                                                  |
|  +------------------+    +----------------------------------+    |
|  | Tasks waiting on | -> | Queue_t.xTasksWaitingToReceive   |    |
|  | queue            |    +----------------------------------+    |
|  +------------------+                                            |
|                                                                  |
|  +------------------+    +----------------------------------+    |
|  | Active timers    | -> | xActiveTimerList                 |    |
|  +------------------+    +----------------------------------+    |
|                                                                  |
|  LIST STRUCTURE (doubly-linked, intrusive):                      |
|                                                                  |
|  +------+     +------+     +------+     +------+                 |
|  | End  |<--->| Item |<--->| Item |<--->| Item |                 |
|  | Mark |     |  A   |     |  B   |     |  C   |                 |
|  +------+     +------+     +------+     +------+                 |
|     ^                                      |                     |
|     +--------------------------------------+                     |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么list.c是核心？FreeRTOS中所有东西都使用列表：

- 就绪任务 -> pxReadyTasksLists[priority]
- 延迟任务 -> xDelayedTaskList
- 等待队列的任务 -> Queue_t.xTasksWaitingToReceive
- 活动定时器 -> xActiveTimerList

列表结构是双向链表，使用侵入式设计（列表项嵌入在所有者结构中）。

**Key Operations in list.c:**

| Function | Purpose | Time Complexity |
|----------|---------|-----------------|
| `vListInitialise()` | Set up empty list | O(1) |
| `vListInsertEnd()` | Add item at end | O(1) |
| `vListInsert()` | Add in sorted order | O(n) |
| `uxListRemove()` | Remove item | O(1) |

### `tasks.c` - The Scheduler Heart

```
tasks.c RESPONSIBILITIES:
+------------------------------------------------------------------+
|                                                                  |
|  TASK LIFECYCLE MANAGEMENT:                                      |
|  +------------------------------------------------------------+  |
|  | xTaskCreate()       - Create new task, allocate TCB+stack  |  |
|  | vTaskDelete()       - Mark task for deletion               |  |
|  | vTaskSuspend()      - Move task to suspended list          |  |
|  | vTaskResume()       - Move task back to ready list         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  SCHEDULING:                                                     |
|  +------------------------------------------------------------+  |
|  | vTaskStartScheduler() - Start RTOS, never returns          |  |
|  | vTaskSwitchContext()  - Select next task to run            |  |
|  | taskYIELD()           - Request context switch             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  TIME MANAGEMENT:                                                |
|  +------------------------------------------------------------+  |
|  | xTaskIncrementTick()  - Called from tick ISR               |  |
|  | vTaskDelay()          - Block task for N ticks             |  |
|  | vTaskDelayUntil()     - Block until absolute time          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  GLOBAL STATE MANAGED:                                           |
|  +------------------------------------------------------------+  |
|  | pxCurrentTCB           - Pointer to running task           |  |
|  | pxReadyTasksLists[]    - Array of ready lists by priority  |  |
|  | xDelayedTaskList       - Tasks waiting for time            |  |
|  | xTickCount             - System tick counter               |  |
|  | uxSchedulerSuspended   - Scheduler lock counter            |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

tasks.c职责：

任务生命周期管理：xTaskCreate（创建任务，分配TCB+栈）、vTaskDelete（标记删除）、vTaskSuspend（移到挂起列表）、vTaskResume（移回就绪列表）。

调度：vTaskStartScheduler（启动RTOS，不返回）、vTaskSwitchContext（选择下一个运行任务）、taskYIELD（请求上下文切换）。

时间管理：xTaskIncrementTick（从tick ISR调用）、vTaskDelay（阻塞N个tick）、vTaskDelayUntil（阻塞到绝对时间）。

管理的全局状态：pxCurrentTCB（运行中任务指针）、pxReadyTasksLists[]（按优先级的就绪列表数组）、xDelayedTaskList（等待时间的任务）、xTickCount（系统tick计数器）、uxSchedulerSuspended（调度器锁计数器）。

### `queue.c` - The Unifying Primitive

```
WHY QUEUES ARE FUNDAMENTAL:
+------------------------------------------------------------------+
|                                                                  |
|  Queue structure serves multiple purposes:                       |
|                                                                  |
|  +-------------------+                                           |
|  |     Queue_t       |                                           |
|  +-------------------+                                           |
|  | pcHead            | ----+                                     |
|  | pcWriteTo         |     |    USED FOR:                        |
|  | uxMessagesWaiting |     |    - Message queues                 |
|  | uxLength          |     |    - Binary semaphores              |
|  | uxItemSize        | <---+    - Counting semaphores            |
|  +-------------------+          - Mutexes                        |
|  | xTasksWaitingTo   |          - Recursive mutexes              |
|  |    Receive        |                                           |
|  | xTasksWaitingTo   |    Same structure, different config:      |
|  |    Send           |    uxItemSize=0 -> semaphore/mutex        |
|  +-------------------+    uxItemSize>0 -> message queue          |
|                                                                  |
|  API MAPPING:                                                    |
|  +------------------------------------------------------------+  |
|  | xQueueCreate()         -> Queue with items                 |  |
|  | xSemaphoreCreateBinary() -> Queue, uxItemSize=0, len=1     |  |
|  | xSemaphoreCreateCounting() -> Queue, uxItemSize=0, len=N   |  |
|  | xSemaphoreCreateMutex() -> Queue + priority inheritance    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么队列是基础？Queue_t结构服务于多种目的：消息队列、二进制信号量、计数信号量、互斥锁、递归互斥锁。

同一结构，不同配置：uxItemSize=0表示信号量/互斥锁，uxItemSize>0表示消息队列。

API映射：xQueueCreate创建带项的队列，xSemaphoreCreateBinary创建uxItemSize=0/len=1的队列，xSemaphoreCreateCounting创建uxItemSize=0/len=N的队列，xSemaphoreCreateMutex创建队列+优先级继承。

---

## 3.3 The `portable/` Directory

```
WHAT "PORTABLE" REALLY MEANS:
+==================================================================+
||                                                                ||
||  FreeRTOS Kernel Core                                          ||
||  +----------------------------------------------------------+  ||
||  | Platform-independent code                                |  ||
||  | (tasks.c, queue.c, list.c, timers.c)                     |  ||
||  |                                                          |  ||
||  | Needs these operations but doesn't know HOW:             |  ||
||  | - Save/restore CPU registers                             |  ||
||  | - Set up initial task stack                              |  ||
||  | - Configure tick timer                                   |  ||
||  | - Enter/exit critical sections                           |  ||
||  +----------------------------------------------------------+  ||
||                          |                                     ||
||                          | Defined interface                   ||
||                          v                                     ||
||  +----------------------------------------------------------+  ||
||  |                    Port Layer                            |  ||
||  |                                                          |  ||
||  |  portable/GCC/ARM_CM4F/                                  |  ||
||  |  +------------------------------------------------------+|  ||
||  |  | port.c           portmacro.h                         ||  ||
||  |  | - portSAVE_CTX   - StackType_t                       ||  ||
||  |  | - portRESTORE_CTX- BaseType_t                        ||  ||
||  |  | - vPortSVCHandler- portSTACK_GROWTH                  ||  ||
||  |  | - xPortPendSVHnd - portYIELD()                       ||  ||
||  |  | - SysTick_Handler- portENTER_CRITICAL()              ||  ||
||  |  +------------------------------------------------------+|  ||
||  +----------------------------------------------------------+  ||
||                          |                                     ||
||                          v                                     ||
||  +----------------------------------------------------------+  ||
||  |              ARM Cortex-M4F Hardware                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

"可移植"的真正含义：FreeRTOS内核核心是平台无关代码（tasks.c、queue.c、list.c、timers.c），它需要这些操作但不知道如何实现：保存/恢复CPU寄存器、设置初始任务栈、配置tick定时器、进入/退出临界区。

移植层（如portable/GCC/ARM_CM4F/）提供这些实现：port.c包含上下文保存/恢复、中断处理程序；portmacro.h定义类型（StackType_t、BaseType_t）、栈增长方向、yield宏、临界区宏。

### Port Layer Responsibilities

| Responsibility | Implementation |
|----------------|----------------|
| **Type definitions** | `portmacro.h`: `StackType_t`, `BaseType_t`, `TickType_t` |
| **Stack setup** | `pxPortInitialiseStack()` - Create fake exception frame |
| **Context switch** | `xPortPendSVHandler` - Save/restore registers |
| **First task start** | `vPortSVCHandler` - Load first task context |
| **Critical sections** | `portENTER_CRITICAL()`, `portEXIT_CRITICAL()` |
| **Yield trigger** | `portYIELD()` - Pend PendSV exception |
| **Tick source** | `SysTick_Handler` - Increment tick, check delays |

### Typical Port Size

```
PORT LAYER SIZE (ARM Cortex-M4F):
+------------------------------------------------------------------+
|                                                                  |
|  port.c:       ~400 lines (mostly comments + assembly)           |
|  portmacro.h:  ~200 lines (macros + type definitions)            |
|                                                                  |
|  TOTAL: ~600 lines to support an entire architecture             |
|                                                                  |
|  This is intentionally small because:                            |
|  - Less code = fewer bugs                                        |
|  - Easier to audit                                               |
|  - Easier to port to new platforms                               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

移植层大小（ARM Cortex-M4F）：port.c约400行（主要是注释+汇编），portmacro.h约200行（宏+类型定义）。总计约600行支持整个架构。

故意设计得小的原因：代码少=bug少、更容易审计、更容易移植到新平台。

---

## 3.4 The `include/` Directory

```
HEADER FILE ORGANIZATION:
+------------------------------------------------------------------+
|                                                                  |
|  FreeRTOS.h  (Always include first)                              |
|  +------------------------------------------------------------+  |
|  | #include "FreeRTOSConfig.h"  // Your configuration         |  |
|  | #include "projdefs.h"         // pdTRUE, pdFALSE, etc.     |  |
|  | #include "portable.h"         // Port interface            |  |
|  |                                                            |  |
|  | Kernel version macros                                      |  |
|  | Default configuration values                               |  |
|  | Common type definitions                                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  MODULE HEADERS:                                                 |
|  +-------------------+   +-------------------+                   |
|  | task.h            |   | queue.h           |                   |
|  | - TaskHandle_t    |   | - QueueHandle_t   |                   |
|  | - xTaskCreate     |   | - xQueueCreate    |                   |
|  | - vTaskDelay      |   | - xQueueSend      |                   |
|  | - vTaskDelete     |   | - xQueueReceive   |                   |
|  +-------------------+   +-------------------+                   |
|                                                                  |
|  +-------------------+   +-------------------+                   |
|  | semphr.h          |   | timers.h          |                   |
|  | (Macros wrapping  |   | - TimerHandle_t   |                   |
|  |  queue.h)         |   | - xTimerCreate    |                   |
|  | - xSemaphoreGive  |   | - xTimerStart     |                   |
|  | - xSemaphoreTake  |   +-------------------+                   |
|  +-------------------+                                           |
|                                                                  |
|  TYPICAL INCLUDE ORDER:                                          |
|  +------------------------------------------------------------+  |
|  | #include "FreeRTOS.h"    // Always first                   |  |
|  | #include "task.h"        // Then modules you use           |  |
|  | #include "queue.h"                                         |  |
|  | #include "semphr.h"                                        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

头文件组织：

FreeRTOS.h（始终首先包含）：包含FreeRTOSConfig.h（你的配置）、projdefs.h（pdTRUE、pdFALSE等）、portable.h（移植接口），还有内核版本宏、默认配置值、通用类型定义。

模块头文件：task.h（任务API）、queue.h（队列API）、semphr.h（信号量宏，包装queue.h）、timers.h（定时器API）。

典型包含顺序：先FreeRTOS.h，然后是你使用的模块。

---

## 3.5 Why the Kernel Is Split This Way

```
DESIGN PRINCIPLES BEHIND THE SPLIT:
+------------------------------------------------------------------+
|                                                                  |
|  PRINCIPLE 1: Separation of concerns                             |
|  +------------------------------------------------------------+  |
|  | list.c    -> Generic data structure, no RTOS knowledge     |  |
|  | tasks.c   -> Scheduling, no knowledge of sync primitives   |  |
|  | queue.c   -> Sync primitives, uses scheduler services      |  |
|  | timers.c  -> Time-based callbacks, builds on tasks+queues  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  PRINCIPLE 2: Minimal coupling                                   |
|  +------------------------------------------------------------+  |
|  | Each .c file includes only what it needs                   |  |
|  | No circular dependencies (except through task.h)           |  |
|  | Port layer has clean interface                             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  PRINCIPLE 3: Optional features                                  |
|  +------------------------------------------------------------+  |
|  | Don't need timers? Don't compile timers.c                  |  |
|  | Don't need event groups? Don't compile event_groups.c      |  |
|  | Co-routines? Legacy, can ignore croutine.c                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  PRINCIPLE 4: Testability                                        |
|  +------------------------------------------------------------+  |
|  | Each component can be unit tested independently            |  |
|  | list.c can run on host without any port                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

内核分割的设计原则：

原则1：关注点分离。list.c是通用数据结构（无RTOS知识）、tasks.c是调度（不了解同步原语）、queue.c是同步原语（使用调度器服务）、timers.c是基于时间的回调（建立在tasks+queues之上）。

原则2：最小耦合。每个.c文件只包含需要的、无循环依赖（除通过task.h）、移植层有干净接口。

原则3：可选功能。不需要定时器就不编译timers.c、不需要事件组就不编译event_groups.c、协程是遗留功能可忽略croutine.c。

原则4：可测试性。每个组件可独立单元测试、list.c可在主机上无需移植层运行。

---

## 3.6 File Reading Order for Learning

```
RECOMMENDED READING ORDER:
+==================================================================+
||                                                                ||
||  PHASE 1: Understand the foundation                            ||
||  +----------------------------------------------------------+  ||
||  | 1. list.h + list.c                                       |  ||
||  |    - Understand List_t and ListItem_t                    |  ||
||  |    - See how intrusive lists work                        |  ||
||  |    - ~200 lines total, easy to understand                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  PHASE 2: Understand task management                           ||
||  +----------------------------------------------------------+  ||
||  | 2. task.h (public API)                                   |  ||
||  |    - Learn the task API surface                          |  ||
||  |                                                          |  ||
||  | 3. tasks.c (first pass - TCB and lists)                  |  ||
||  |    - Find TCB_t definition                               |  ||
||  |    - Find pxReadyTasksLists                              |  ||
||  |    - Understand task states                              |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  PHASE 3: Understand the port layer                            ||
||  +----------------------------------------------------------+  ||
||  | 4. portmacro.h (your target architecture)                |  ||
||  |    - Type definitions                                    |  ||
||  |    - Critical section macros                             |  ||
||  |                                                          |  ||
||  | 5. port.c (your target architecture)                     |  ||
||  |    - Context switch implementation                       |  ||
||  |    - Stack initialization                                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  PHASE 4: Understand synchronization                           ||
||  +----------------------------------------------------------+  ||
||  | 6. queue.h (public API)                                  |  ||
||  | 7. queue.c (Queue_t structure, key functions)            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  LATER: timers.c, event_groups.c, stream_buffer.c              ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

推荐阅读顺序：

阶段1（理解基础）：list.h + list.c，理解List_t和ListItem_t，看侵入式列表如何工作，共约200行易于理解。

阶段2（理解任务管理）：task.h（公共API）学习任务API表面，tasks.c（第一遍-TCB和列表）找TCB_t定义、pxReadyTasksLists、理解任务状态。

阶段3（理解移植层）：portmacro.h（你的目标架构）的类型定义和临界区宏，port.c的上下文切换实现和栈初始化。

阶段4（理解同步）：queue.h（公共API）和queue.c（Queue_t结构和关键函数）。

稍后：timers.c、event_groups.c、stream_buffer.c。

---

## Summary

```
SOURCE TREE KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  CORE FILES (always compiled):                                 ||
||  - tasks.c    ~3000 lines   Scheduler                          ||
||  - list.c     ~200 lines    Foundation                         ||
||  - queue.c    ~2500 lines   IPC/sync primitives                ||
||                                                                ||
||  OPTIONAL FILES:                                               ||
||  - timers.c         Software timers                            ||
||  - event_groups.c   Event flags                                ||
||  - stream_buffer.c  Stream/message buffers                     ||
||  - croutine.c       Co-routines (legacy)                       ||
||                                                                ||
||  PORT LAYER:                                                   ||
||  - ~600 lines for a typical port                               ||
||  - Implements: context switch, tick, critical sections         ||
||                                                                ||
||  MEMORY MANAGEMENT:                                            ||
||  - Choose ONE heap_x.c file                                    ||
||  - Or provide your own pvPortMalloc/vPortFree                  ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Task Model: The Core Abstraction](04-task-model.md)
