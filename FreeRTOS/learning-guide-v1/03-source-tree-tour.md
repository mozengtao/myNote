# Section 3: FreeRTOS Source Tree Tour (Critical Section)

This section walks through the FreeRTOS kernel source code **directory by directory**. Understanding this structure is essential for debugging, porting, and extending FreeRTOS.

## 3.1 FreeRTOS Kernel Root Directory

```
FreeRTOS-Kernel/
|
+-- tasks.c              # Task management (THE core file)
+-- queue.c              # Queues, semaphores, mutexes
+-- list.c               # Linked list implementation
+-- timers.c             # Software timers
+-- event_groups.c       # Event groups
+-- stream_buffer.c      # Stream and message buffers
+-- croutine.c           # Co-routines (legacy, rarely used)
|
+-- include/             # Header files
|   +-- FreeRTOS.h       # Master include, configuration
|   +-- task.h           # Task API
|   +-- queue.h          # Queue API
|   +-- semphr.h         # Semaphore API (wrapper around queue.h)
|   +-- timers.h         # Timer API
|   +-- event_groups.h   # Event groups API
|   +-- list.h           # List data structure
|   +-- portable.h       # Port abstraction
|   +-- projdefs.h       # Project definitions
|   +-- ...
|
+-- portable/            # Hardware/compiler specific code
|   +-- GCC/             # GCC compiler ports
|   |   +-- ARM_CM4F/    # Cortex-M4 with FPU
|   |   +-- ARM_CM3/     # Cortex-M3
|   |   +-- RISC-V/      # RISC-V
|   |   +-- ...
|   +-- IAR/             # IAR compiler ports
|   +-- Keil/            # Keil compiler ports
|   +-- MemMang/         # Memory management schemes
|   |   +-- heap_1.c     # Simplest, no free
|   |   +-- heap_2.c     # Best fit, no coalescing
|   |   +-- heap_3.c     # Wraps standard malloc
|   |   +-- heap_4.c     # First fit, coalescing
|   |   +-- heap_5.c     # heap_4 + multiple regions
|   +-- ...
|
+-- examples/            # Example configurations
```

**Chinese Explanation (中文说明):**

FreeRTOS内核的根目录包含六个核心源文件和三个重要子目录。核心源文件分别负责：tasks.c（任务管理，最核心）、queue.c（队列、信号量、互斥锁）、list.c（链表数据结构）、timers.c（软件定时器）、event_groups.c（事件组）、stream_buffer.c（流和消息缓冲区）。

include/目录包含所有头文件，portable/目录包含硬件和编译器特定代码（移植层），examples/目录包含示例配置。理解这个结构是深入学习FreeRTOS的基础。

---

## 3.2 Core Source Files

### tasks.c - The Heart of FreeRTOS (~8800 lines)

`tasks.c` is the largest and most important file. It contains:

```
tasks.c Contents:
+------------------------------------------------------------------+
|                                                                  |
|  DATA STRUCTURES:                                                |
|  +------------------------------------------------------------+  |
|  | TCB_t (Task Control Block)                                 |  |
|  | - pxTopOfStack: Stack pointer                              |  |
|  | - xStateListItem: Links task to state list                 |  |
|  | - xEventListItem: Links task to event wait list            |  |
|  | - uxPriority: Task priority                                |  |
|  | - pxStack: Stack base address                              |  |
|  | - pcTaskName: Task name for debugging                      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  GLOBAL STATE:                                                   |
|  +------------------------------------------------------------+  |
|  | pxCurrentTCB: Currently running task                       |  |
|  | pxReadyTasksLists[]: One list per priority level           |  |
|  | xDelayedTaskList1/2: Delayed tasks                         |  |
|  | xPendingReadyList: Tasks woken while scheduler suspended   |  |
|  | xSuspendedTaskList: Suspended tasks                        |  |
|  | xTasksWaitingTermination: Deleted tasks awaiting cleanup   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  KEY FUNCTIONS:                                                  |
|  +------------------------------------------------------------+  |
|  | xTaskCreate() / xTaskCreateStatic()                        |  |
|  | vTaskDelete()                                              |  |
|  | vTaskDelay() / vTaskDelayUntil()                           |  |
|  | vTaskSuspend() / vTaskResume()                             |  |
|  | vTaskPrioritySet() / uxTaskPriorityGet()                   |  |
|  | vTaskStartScheduler()                                      |  |
|  | vTaskSwitchContext() [called by port]                      |  |
|  | xTaskIncrementTick() [called by tick ISR]                  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Why tasks.c is so large:**
- Task state machine (Ready, Blocked, Suspended, Deleted)
- All list management for task queues
- Context switch preparation
- Tick processing
- Stack overflow checking
- Runtime statistics
- Trace hooks

**Chinese Explanation (中文说明):**

tasks.c是FreeRTOS最大、最重要的文件，约8800行代码。它包含：

数据结构：TCB_t（任务控制块）存储每个任务的状态，包括栈指针、状态列表项、事件列表项、优先级、栈基地址和任务名称。

全局状态：pxCurrentTCB（当前运行的任务）、pxReadyTasksLists（就绪任务列表数组，每个优先级一个）、xDelayedTaskList（延时任务列表）、xSuspendedTaskList（挂起任务列表）等。

关键函数：任务创建/删除、延时、挂起/恢复、优先级操作、调度器启动、上下文切换、tick处理等。

### queue.c - Inter-Task Communication (~3400 lines)

`queue.c` implements:
- Queues (FIFO message passing)
- Semaphores (binary and counting)
- Mutexes (with priority inheritance)

```
queue.c Core Structure:
+------------------------------------------------------------------+
|                                                                  |
|  Queue_t Structure:                                              |
|  +------------------------------------------------------------+  |
|  | pcHead, pcWriteTo: Buffer management pointers              |  |
|  | xTasksWaitingToSend: Tasks blocked trying to send          |  |
|  | xTasksWaitingToReceive: Tasks blocked trying to receive    |  |
|  | uxMessagesWaiting: Current item count                      |  |
|  | uxLength: Maximum items                                    |  |
|  | uxItemSize: Bytes per item                                 |  |
|  | cRxLock, cTxLock: ISR-safe locking mechanism               |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  When used as SEMAPHORE:                                         |
|  +------------------------------------------------------------+  |
|  | uxItemSize = 0 (no data transferred)                       |  |
|  | uxLength = 1 for binary, N for counting                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  When used as MUTEX:                                             |
|  +------------------------------------------------------------+  |
|  | pcHead = NULL (indicates mutex type)                       |  |
|  | xMutexHolder: Task that holds the mutex                    |  |
|  | uxRecursiveCallCount: For recursive mutexes                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Key insight**: Semaphores and mutexes are just special queues. This is why `semphr.h` is a thin wrapper around `queue.h`.

**Chinese Explanation (中文说明):**

queue.c实现了队列、信号量和互斥锁，约3400行代码。关键洞察：信号量和互斥锁只是特殊的队列。

Queue_t结构体包含：缓冲区管理指针(pcHead, pcWriteTo)、等待发送和接收的任务列表、消息计数、最大项数、每项大小、ISR安全的锁机制。

作为信号量使用时：uxItemSize=0（不传输数据），uxLength=1（二值）或N（计数）。

作为互斥锁使用时：pcHead=NULL（标识互斥锁类型），额外存储持有者任务和递归计数。

### list.c - The Data Structure Foundation (~250 lines)

`list.c` is small but critical. Every task state and every kernel object uses lists.

```
list.c Structures:
+------------------------------------------------------------------+
|                                                                  |
|  ListItem_t:                                                     |
|  +------------------------------------------------------------+  |
|  | xItemValue: Sort key (e.g., wake time, priority)           |  |
|  | pxNext: Next item in list                                  |  |
|  | pxPrevious: Previous item in list                          |  |
|  | pvOwner: Pointer back to containing TCB                    |  |
|  | pxContainer: Pointer to list this item is in               |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  List_t:                                                         |
|  +------------------------------------------------------------+  |
|  | uxNumberOfItems: Current count                             |  |
|  | pxIndex: Walking pointer for round-robin                   |  |
|  | xListEnd: Sentinel node (value = MAX)                      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Functions:                                                      |
|  +------------------------------------------------------------+  |
|  | vListInitialise(): Set up empty list                       |  |
|  | vListInsert(): Insert sorted by xItemValue                 |  |
|  | vListInsertEnd(): Insert at tail (round-robin fairness)    |  |
|  | uxListRemove(): Remove item, return remaining count        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Why intrusive lists?**

```
Traditional List (external):          Intrusive List (FreeRTOS):
+--------+     +--------+             +--------+
| Node 1 |---->| Node 2 |             | TCB    |
| data * |     | data * |             | +----+ |
+--------+     +--------+             | |List| |
    |              |                  | |Item| |
    v              v                  | +----+ |
+------+       +------+               +--------+
| TCB1 |       | TCB2 |                    |
+------+       +------+                    v
                                      [directly linked]
Extra allocation: YES                 Extra allocation: NO
Cache locality: POOR                  Cache locality: GOOD
```

**Chinese Explanation (中文说明):**

list.c虽然只有约250行，但是至关重要。每个任务状态和每个内核对象都使用链表。

ListItem_t结构体包含：xItemValue（排序键，如唤醒时间或优先级）、pxNext/pxPrevious（链表指针）、pvOwner（指向拥有者TCB的指针）、pxContainer（指向所在列表的指针）。

List_t结构体包含：uxNumberOfItems（项数）、pxIndex（用于轮询的遍历指针）、xListEnd（哨兵节点，值为最大）。

FreeRTOS使用侵入式链表：ListItem_t嵌入在TCB内部，而不是分配单独的节点指向TCB。这避免了额外内存分配，提高了缓存局部性。

### timers.c - Software Timers (~1300 lines)

```
timers.c Architecture:
+------------------------------------------------------------------+
|                                                                  |
|  Timer_t Structure:                                              |
|  +------------------------------------------------------------+  |
|  | pcTimerName: Name for debugging                            |  |
|  | xTimerListItem: Links timer to active list                 |  |
|  | xTimerPeriodInTicks: Timer period                          |  |
|  | pvTimerID: User-provided ID                                |  |
|  | pxCallbackFunction: Called when timer expires              |  |
|  | ucStatus: Active/auto-reload flags                         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Timer Daemon Task:                                              |
|  +------------------------------------------------------------+  |
|  | - Runs at configTIMER_TASK_PRIORITY                        |  |
|  | - Processes timer command queue                            |  |
|  | - Calls callbacks in TASK context (not ISR!)               |  |
|  | - Manages two timer lists (for overflow handling)          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Timer Command Queue:                                            |
|  +------------------------------------------------------------+  |
|  | xTimerStart(), xTimerStop(), etc. send commands here       |  |
|  | Daemon task dequeues and processes                         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Why a daemon task?** Timer callbacks run in task context, allowing them to:
- Call blocking API functions
- Take longer without affecting ISR latency
- Use full API without `FromISR` restrictions

**Chinese Explanation (中文说明):**

timers.c实现软件定时器，约1300行。Timer_t结构体包含定时器名称、列表项、周期、用户ID、回调函数和状态标志。

关键设计：定时器守护任务。定时器API（如xTimerStart）不直接执行操作，而是向定时器命令队列发送命令。守护任务从队列中取出命令并处理，在任务上下文中调用回调函数。

为什么使用守护任务？定时器回调在任务上下文运行，可以调用阻塞API、执行较长时间而不影响ISR延迟、使用完整API而无FromISR限制。

### event_groups.c - Bitmask Synchronization (~900 lines)

```
event_groups.c Structure:
+------------------------------------------------------------------+
|                                                                  |
|  EventGroup_t:                                                   |
|  +------------------------------------------------------------+  |
|  | uxEventBits: The actual bit flags                          |  |
|  | xTasksWaitingForBits: List of waiting tasks                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Wait Modes:                                                     |
|  +------------------------------------------------------------+  |
|  | Wait for ANY: Task wakes if ANY waited bit is set          |  |
|  | Wait for ALL: Task wakes only if ALL waited bits are set   |  |
|  | Clear on exit: Automatically clear bits after waking       |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Use Case Example (Synchronization Barrier):                     |
|  +------------------------------------------------------------+  |
|  | Task A: xEventGroupSetBits(eg, BIT_A_DONE)                 |  |
|  | Task B: xEventGroupSetBits(eg, BIT_B_DONE)                 |  |
|  | Task C: xEventGroupWaitBits(eg, BIT_A|BIT_B, ALL, ...)     |  |
|  | -> Task C wakes when BOTH A and B have set their bits      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

event_groups.c实现基于位掩码的同步，约900行。EventGroup_t结构体包含：uxEventBits（实际的位标志）和xTasksWaitingForBits（等待任务列表）。

等待模式：等待ANY（任一等待位被设置即唤醒）、等待ALL（所有等待位都被设置才唤醒）、退出时清除（唤醒后自动清除位）。

典型用例是同步屏障：任务A设置BIT_A_DONE，任务B设置BIT_B_DONE，任务C等待BIT_A|BIT_B全部设置。当A和B都完成时，C才被唤醒。

### stream_buffer.c - Lock-Free Byte Streams (~1100 lines)

```
stream_buffer.c Design:
+------------------------------------------------------------------+
|                                                                  |
|  StreamBuffer_t (Internal):                                      |
|  +------------------------------------------------------------+  |
|  | pucBuffer: Byte array storage                              |  |
|  | xLength: Total buffer size                                 |  |
|  | xHead: Write position                                      |  |
|  | xTail: Read position                                       |  |
|  | xTriggerLevelBytes: Wake threshold                         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Lock-Free Operation (single reader, single writer):             |
|  +------------------------------------------------------------+  |
|  | Writer updates xHead AFTER writing data                    |  |
|  | Reader updates xTail AFTER reading data                    |  |
|  | -> No mutex needed for 1 producer + 1 consumer             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Message Buffer = Stream Buffer + Length Prefix:                 |
|  +------------------------------------------------------------+  |
|  | Each message prefixed with size_t length                   |  |
|  | sbRECEIVE_COMPLETED() returns full message or nothing      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

stream_buffer.c实现无锁字节流，约1100行。StreamBuffer_t包含字节数组缓冲区、长度、头指针（写位置）、尾指针（读位置）和触发级别。

无锁操作（单读单写）：写者在写入数据后更新xHead，读者在读取数据后更新xTail。单生产者+单消费者场景无需互斥锁。

消息缓冲区=流缓冲区+长度前缀：每条消息前面加上size_t长度，接收函数返回完整消息或什么都不返回（原子性）。

---

## 3.3 FreeRTOS/include Directory

### FreeRTOS.h - The Master Configuration Header

```
FreeRTOS.h Role:
+------------------------------------------------------------------+
|                                                                  |
|  1. Include FreeRTOSConfig.h (user configuration)                |
|     #include "FreeRTOSConfig.h"                                  |
|                                                                  |
|  2. Provide defaults for missing config options                  |
|     #ifndef configUSE_PREEMPTION                                 |
|         #define configUSE_PREEMPTION 1                           |
|     #endif                                                       |
|                                                                  |
|  3. Include portable layer                                       |
|     #include "portable.h"  // Gets portmacro.h                   |
|                                                                  |
|  4. Define kernel-wide types and macros                          |
|     typedef ... BaseType_t;                                      |
|     #define pdTRUE  1                                            |
|     #define pdFALSE 0                                            |
|     #define pdPASS  pdTRUE                                       |
|     #define pdFAIL  pdFALSE                                      |
|                                                                  |
+------------------------------------------------------------------+
```

**Configuration Injection Pattern:**

```
User's Project/                         FreeRTOS/include/
+------------------------+              +------------------------+
| FreeRTOSConfig.h       |    <----     | FreeRTOS.h             |
| #define configUSE_..   |   includes   | #include "FreeRTOS-    |
| #define configMAX_..   |              |           Config.h"    |
+------------------------+              +------------------------+
         |                                        |
         |                                        v
         |                              Fills in defaults
         |                                        |
         +----------------------------------------+
                          |
                          v
                 Fully configured kernel
```

**Chinese Explanation (中文说明):**

FreeRTOS.h是主配置头文件，作用包括：
1. 包含用户的FreeRTOSConfig.h
2. 为缺失的配置选项提供默认值
3. 包含移植层(portable.h)
4. 定义内核级类型和宏（如pdTRUE、pdFALSE）

配置注入模式：用户项目包含FreeRTOSConfig.h定义配置选项，FreeRTOS.h包含它并填充默认值，形成完整配置的内核。

### task.h - Task API

Key API functions:

| Function | Purpose |
|----------|---------|
| `xTaskCreate()` | Create task with dynamic allocation |
| `xTaskCreateStatic()` | Create task with static allocation |
| `vTaskDelete()` | Delete task |
| `vTaskDelay()` | Delay relative to now |
| `vTaskDelayUntil()` | Delay to absolute time |
| `vTaskSuspend()` | Suspend task indefinitely |
| `vTaskResume()` | Resume suspended task |
| `vTaskPrioritySet()` | Change priority |
| `uxTaskPriorityGet()` | Get priority |
| `vTaskStartScheduler()` | Start RTOS |
| `vTaskEndScheduler()` | Stop RTOS (rarely used) |

### queue.h and semphr.h

```
semphr.h is a WRAPPER around queue.h:

+---------------------+          +---------------------+
| semphr.h            |          | queue.h             |
+---------------------+          +---------------------+
| xSemaphoreCreate-   |   --->   | xQueueCreateStatic()|
|   Binary()          |          |   with size=0       |
|                     |          |                     |
| xSemaphoreCreate-   |   --->   | xQueueCreate()      |
|   Counting()        |          |   with size=0       |
|                     |          |                     |
| xSemaphoreTake()    |   --->   | xQueueReceive()     |
|                     |          |                     |
| xSemaphoreGive()    |   --->   | xQueueSend()        |
+---------------------+          +---------------------+
```

**Chinese Explanation (中文说明):**

task.h定义任务API：创建、删除、延时、挂起、恢复、优先级操作、调度器启动等。

semphr.h是queue.h的封装：xSemaphoreCreateBinary()调用xQueueCreate()（size=0），xSemaphoreTake()调用xQueueReceive()，xSemaphoreGive()调用xQueueSend()。理解这一点有助于调试——信号量问题实际上是队列问题。

### portable.h - Port Abstraction

```
portable.h defines the PORT INTERFACE:
+------------------------------------------------------------------+
|                                                                  |
|  Memory allocation:                                              |
|    void *pvPortMalloc(size_t xSize);                             |
|    void vPortFree(void *pv);                                     |
|                                                                  |
|  Stack initialization:                                           |
|    StackType_t *pxPortInitialiseStack(...)                       |
|                                                                  |
|  Scheduler control:                                              |
|    BaseType_t xPortStartScheduler(void);                         |
|    void vPortEndScheduler(void);                                 |
|                                                                  |
|  Includes portmacro.h (port-specific definitions)                |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 3.4 FreeRTOS/portable Directory

This directory contains ALL hardware and compiler specific code.

### Directory Structure

```
portable/
+-- GCC/                    # GCC compiler
|   +-- ARM_CM0/            # Cortex-M0
|   +-- ARM_CM3/            # Cortex-M3
|   +-- ARM_CM4F/           # Cortex-M4 with FPU
|   +-- ARM_CM7/            # Cortex-M7
|   +-- RISC-V/             # RISC-V
|   +-- ...
+-- IAR/                    # IAR compiler
|   +-- ARM_CM4F/
|   +-- ...
+-- Keil/                   # Keil compiler
|   +-- ARM_CM4F/
|   +-- ...
+-- MemMang/                # Memory allocators
|   +-- heap_1.c            # Allocate only, never free
|   +-- heap_2.c            # Best fit, no coalescing
|   +-- heap_3.c            # Wrap malloc/free
|   +-- heap_4.c            # First fit + coalescing
|   +-- heap_5.c            # heap_4 + non-contiguous regions
+-- template/               # Template for new ports
```

### What a Port Contains

Each port directory (e.g., `GCC/ARM_CM4F/`) contains:

```
ARM_CM4F/
+-- port.c              # C implementation
|   - pxPortInitialiseStack(): Set up initial task stack
|   - xPortStartScheduler(): Configure tick timer, start first task
|   - vPortYield(): Trigger context switch
|   - xPortPendSVHandler(): Context switch ISR (in assembly)
|   - xPortSysTickHandler(): Tick interrupt handler
|
+-- portmacro.h         # Port-specific macros and types
    - StackType_t: Stack element type (uint32_t for CM4)
    - BaseType_t: Native efficient type (int32_t for CM4)
    - TickType_t: Tick counter type (uint32_t or uint16_t)
    - portSTACK_GROWTH: -1 (descending) or +1 (ascending)
    - portBYTE_ALIGNMENT: Stack alignment (8 for CM4)
    - portYIELD(): Macro to request context switch
    - portENTER_CRITICAL(): Disable interrupts, save state
    - portEXIT_CRITICAL(): Restore interrupt state
    - portDISABLE_INTERRUPTS(): Disable all interrupts
    - portENABLE_INTERRUPTS(): Enable all interrupts
```

**Chinese Explanation (中文说明):**

portable/目录包含所有硬件和编译器特定代码。目录按编译器（GCC、IAR、Keil等）组织，每个编译器目录下按处理器架构组织。

每个移植包含：
- port.c：C实现，包括栈初始化、调度器启动、yield触发、上下文切换ISR、tick处理
- portmacro.h：移植特定宏和类型，包括栈类型、基本类型、栈增长方向、对齐、yield宏、临界区宏

### Cortex-M Context Switch (port.c example)

```
Cortex-M PendSV Handler (Context Switch):
+------------------------------------------------------------------+
|                                                                  |
|   1. Hardware automatically saves: R0-R3, R12, LR, PC, xPSR      |
|                                                                  |
|   2. PendSV handler (in port.c):                                 |
|      a. Get PSP (process stack pointer)                          |
|      b. Save remaining registers: R4-R11                         |
|      c. Save PSP to current TCB                                  |
|      d. Call vTaskSwitchContext()                                |
|         - Selects highest priority ready task                    |
|         - Updates pxCurrentTCB                                   |
|      e. Load PSP from new TCB                                    |
|      f. Restore R4-R11 from new task's stack                     |
|      g. Return (hardware restores R0-R3, R12, LR, PC, xPSR)      |
|                                                                  |
+------------------------------------------------------------------+

    Task A Stack              Task B Stack
    +------------+            +------------+
    | xPSR       | <-- auto   | xPSR       |
    | PC (ret)   |    saved   | PC (ret)   |
    | LR         |    by      | LR         |
    | R12        |    HW      | R12        |
    | R3-R0      |            | R3-R0      |
    +------------+            +------------+
    | R11-R4     | <-- saved  | R11-R4     |
    +------------+    by SW   +------------+
    | ...        |            | ...        |
         ^                         ^
         |                         |
    Current PSP              New PSP
```

**Chinese Explanation (中文说明):**

Cortex-M上下文切换使用PendSV中断。硬件自动保存R0-R3、R12、LR、PC、xPSR到任务栈。PendSV处理程序：获取PSP、保存R4-R11到当前任务栈、保存PSP到当前TCB、调用vTaskSwitchContext（选择最高优先级就绪任务、更新pxCurrentTCB）、从新TCB加载PSP、从新任务栈恢复R4-R11、返回（硬件恢复其余寄存器）。

---

## 3.5 FreeRTOS/Demo and Examples

### Why Demos Exist

```
Demo Purpose:
+------------------------------------------------------------------+
|                                                                  |
|  1. Prove the port works on specific hardware                    |
|  2. Show correct FreeRTOSConfig.h for that platform              |
|  3. Demonstrate API usage patterns                               |
|  4. Provide working startup code                                 |
|                                                                  |
+------------------------------------------------------------------+
```

### Why They Are Intentionally Verbose

```
Demo Code Style (intentionally explicit):

// VERBOSE BUT CLEAR:
TaskHandle_t xTaskHandle = NULL;

xTaskCreate(
    vTaskFunction,           // Task function
    "TaskName",              // Name for debugging
    configMINIMAL_STACK_SIZE,// Stack size
    NULL,                    // Parameters
    tskIDLE_PRIORITY + 1,    // Priority
    &xTaskHandle             // Handle output
);

if( xTaskHandle == NULL )
{
    // Handle creation failure
}

// NOT THIS (compact but unclear):
xTaskCreate(vTask, "T", 128, 0, 1, 0);
```

### How to Extract Patterns Without Copying Blindly

```
+------------------------------------------------------------------+
|  EXTRACT                     | DON'T COPY                        |
+------------------------------------------------------------------+
| FreeRTOSConfig.h structure   | Exact config values (wrong for   |
|                              | your hardware)                    |
+------------------------------------------------------------------+
| Interrupt priority setup     | Demo-specific peripheral code    |
+------------------------------------------------------------------+
| Startup sequence:            | Infinite loops that toggle LEDs  |
|   1. Hardware init           |                                   |
|   2. Create tasks            |                                   |
|   3. Start scheduler         |                                   |
+------------------------------------------------------------------+
| Pattern: ISR -> Queue ->     | Specific demo logic              |
| Worker Task                  |                                   |
+------------------------------------------------------------------+
| Stack size estimation        | Magic numbers without comments   |
| methodology                  |                                   |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Demo存在的原因：证明移植在特定硬件上工作、展示正确的FreeRTOSConfig.h、演示API使用模式、提供可工作的启动代码。

Demo故意详细：使用完整的参数名和注释，便于学习，即使代码更长。

如何提取模式而不盲目复制：提取FreeRTOSConfig.h结构（但不是具体值）、中断优先级设置方法、启动序列、ISR->队列->工作任务模式。不要复制：演示特定的外设代码、LED闪烁循环、没有注释的魔术数字。

---

## Summary: Why Files Are Split This Way

| File | Responsibility | Rationale |
|------|----------------|-----------|
| `tasks.c` | Task lifecycle, scheduler | Core functionality together |
| `queue.c` | All queue-based IPC | Queues, semas, mutexes share 80% code |
| `list.c` | Generic list | Used by everything, changes rarely |
| `timers.c` | Software timers | Optional feature, separate module |
| `event_groups.c` | Bit-based sync | Different sync model, separate |
| `stream_buffer.c` | Byte streams | Lock-free design, specialized |

```
Dependency Graph:
                    +-------------+
                    |   tasks.c   |
                    +------+------+
                           |
          +----------------+----------------+
          |                |                |
    +-----v-----+    +-----v-----+    +-----v-----+
    |  queue.c  |    | timers.c  |    |event_grps |
    +-----------+    +-----+-----+    +-----------+
                           |
                     +-----v-----+
                     | queue.c   |  (timer uses queue)
                     +-----------+

All depend on: list.c, port layer
```

**Chinese Explanation (中文说明):**

文件这样划分的原因：
- tasks.c：核心功能放在一起（任务生命周期、调度器）
- queue.c：队列、信号量、互斥锁共享80%代码
- list.c：被所有组件使用，很少修改
- timers.c：可选功能，独立模块
- event_groups.c：不同的同步模型
- stream_buffer.c：无锁设计，专用

依赖图显示timers.c使用queue.c（定时器命令队列），所有组件都依赖list.c和移植层。

**Next Section**: [The Scheduler (The Heart of FreeRTOS)](04-scheduler.md)
