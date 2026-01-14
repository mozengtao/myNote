# Section 2: High-Level FreeRTOS Architecture

## 2.1 Big Picture Overview

FreeRTOS is a **small preemptive kernel** with optional services. It is not an operating system in the Linux/Windows sense - it is a scheduler with coordination primitives.

### FreeRTOS System Architecture

```
+===========================================================================+
||                         APPLICATION LAYER                               ||
||  +---------------+  +---------------+  +---------------+  +----------+  ||
||  | Task 1        |  | Task 2        |  | Task 3        |  | Task N   |  ||
||  | (User Code)   |  | (User Code)   |  | (User Code)   |  | ...      |  ||
||  +-------+-------+  +-------+-------+  +-------+-------+  +----+-----+  ||
||          |                  |                  |               |        ||
+===========|==================|==================|===============|=========+
            |                  |                  |               |
            v                  v                  v               v
+===========================================================================+
||                        FREERTOS KERNEL LAYER                            ||
||                                                                         ||
||  +------------------------------------------------------------------+   ||
||  |                        SCHEDULER                                 |   ||
||  |  - Task state management (Ready, Blocked, Suspended)             |   ||
||  |  - Priority-based selection                                      |   ||
||  |  - Context switching                                             |   ||
||  +------------------------------------------------------------------+   ||
||                                                                         ||
||  +----------------+  +----------------+  +----------------+              ||
||  | QUEUES         |  | SEMAPHORES     |  | EVENT GROUPS   |             ||
||  | - Message pass |  | - Signaling    |  | - Bit-based    |             ||
||  | - Data copy    |  | - Counting     |  |   sync         |             ||
||  +----------------+  +----------------+  +----------------+              ||
||                                                                         ||
||  +----------------+  +----------------+  +----------------+              ||
||  | MUTEXES        |  | TIMERS         |  | STREAM BUFFERS |             ||
||  | - Priority     |  | - One-shot     |  | - Byte streams |             ||
||  |   inheritance  |  | - Auto-reload  |  | - Lock-free    |             ||
||  +----------------+  +----------------+  +----------------+              ||
||                                                                         ||
+===========================================================================+
            |                  |                  |
            v                  v                  v
+===========================================================================+
||                         PORT LAYER                                      ||
||  +------------------------------------------------------------------+   ||
||  |  portmacro.h: Type definitions, critical sections                |   ||
||  |  port.c: Context switch, tick setup, stack initialization        |   ||
||  +------------------------------------------------------------------+   ||
+===========================================================================+
            |                  |                  |
            v                  v                  v
+===========================================================================+
||                         HARDWARE                                        ||
||  +----------------+  +----------------+  +----------------+              ||
||  | CPU CORE       |  | SYSTICK TIMER  |  | NVIC           |             ||
||  | - Registers    |  | - Tick source  |  | - Interrupts   |             ||
||  | - Stack        |  | - Period       |  | - Priorities   |             ||
||  +----------------+  +----------------+  +----------------+              ||
+===========================================================================+
```

**Chinese Explanation (中文说明):**

FreeRTOS架构分为四层：应用层（用户任务代码）、内核层（调度器和同步原语）、移植层（硬件抽象）和硬件层（CPU、定时器、中断控制器）。

应用层包含用户编写的任务，每个任务是一个独立的执行线程。内核层是FreeRTOS的核心，包括调度器（管理任务状态和上下文切换）以及各种内核对象（队列、信号量、互斥锁、事件组、定时器、流缓冲区）。移植层将FreeRTOS与特定硬件隔离，包含类型定义、临界区实现、上下文切换代码等。硬件层是实际的MCU，包括CPU核心、系统定时器和中断控制器。

### Key Components

```
                    +------------------+
                    |   Application    |
                    +--------+---------+
                             |
        +--------------------+--------------------+
        |                    |                    |
        v                    v                    v
+---------------+    +---------------+    +---------------+
|    Tasks      |    | Kernel Objects|    |    Timers     |
| (tasks.c)     |    | (queue.c)     |    | (timers.c)    |
+-------+-------+    +-------+-------+    +-------+-------+
        |                    |                    |
        +--------------------+--------------------+
                             |
                             v
                    +------------------+
                    |   List Engine    |
                    |   (list.c)       |
                    +--------+---------+
                             |
                             v
                    +------------------+
                    |   Port Layer     |
                    |   (port.c)       |
                    +--------+---------+
                             |
                             v
                    +------------------+
                    |    Hardware      |
                    +------------------+
```

**Chinese Explanation (中文说明):**

FreeRTOS的关键组件形成一个依赖层次：应用程序使用任务、内核对象和定时器；所有这些组件都依赖于列表引擎(list.c)进行数据组织；列表引擎和调度器都依赖于移植层与硬件交互。这种分层设计使得FreeRTOS可以在不修改核心代码的情况下移植到新的硬件平台。

---

## 2.2 Core Architectural Principles

### Written in C (Not C++)

FreeRTOS is written in ANSI C for:

| Reason | Explanation |
|--------|-------------|
| **Portability** | Every MCU has a C compiler |
| **Predictability** | No hidden constructors, no exceptions, no RTTI |
| **Size** | No C++ runtime overhead |
| **Transparency** | What you see is what you get |

The code uses a consistent naming convention:

```c
/* Function naming: v = void return, x = BaseType_t return, u = unsigned */
void vTaskDelete( TaskHandle_t xTask );
BaseType_t xTaskCreate( ... );
UBaseType_t uxTaskPriorityGet( ... );

/* Variable naming: px = pointer, ux = unsigned, x = BaseType_t */
TCB_t *pxCurrentTCB;
UBaseType_t uxCurrentNumberOfTasks;
BaseType_t xSchedulerRunning;

/* Type naming: _t suffix for typedef */
typedef struct tskTaskControlBlock TCB_t;
typedef uint32_t TickType_t;
```

**Chinese Explanation (中文说明):**

FreeRTOS使用ANSI C编写而非C++，原因有四：可移植性（每个MCU都有C编译器）、可预测性（没有隐式构造函数、异常或RTTI）、体积小（没有C++运行时开销）、透明性（代码行为明确）。

代码使用一致的命名约定：函数名以v(void返回)、x(BaseType_t返回)、u(unsigned返回)开头；变量名以px(指针)、ux(无符号)、x(BaseType_t)开头；类型名以_t后缀结尾。这种命名约定使代码自文档化。

### Mostly Single-Threaded Internally

Even though FreeRTOS manages multiple tasks, the kernel itself is mostly single-threaded:

```
+-------------------------------------------------------------------+
|                    KERNEL EXECUTION MODEL                         |
+-------------------------------------------------------------------+
|                                                                   |
|   Task Context:                                                   |
|   +-----------------------------------------------------------+   |
|   | Task A runs          | Task B runs          | Task C runs |   |
|   +-----------------------------------------------------------+   |
|                    ^                 ^                 ^          |
|                    |                 |                 |          |
|   +----------------+-----------------+-----------------+-------+  |
|   |        Context switches happen here (one at a time)       |  |
|   +-----------------------------------------------------------+  |
|                                                                   |
|   Kernel operations protected by:                                 |
|   1. taskENTER_CRITICAL() / taskEXIT_CRITICAL()                   |
|   2. vTaskSuspendAll() / xTaskResumeAll()                         |
|                                                                   |
|   Only ONE task or ISR modifies kernel state at a time            |
|                                                                   |
+-------------------------------------------------------------------+
```

**Why single-threaded kernel?**
- Simpler correctness reasoning
- No kernel locks needed (for single-core)
- Lower overhead
- Easier to analyze for real-time guarantees

**Chinese Explanation (中文说明):**

尽管FreeRTOS管理多个任务，但内核本身大部分是单线程的。在任何时刻，只有一个任务或ISR在修改内核状态。这通过两种保护机制实现：临界区(taskENTER_CRITICAL)和调度器挂起(vTaskSuspendAll)。

单线程内核设计的好处是：更简单的正确性推理、无需内核锁（单核情况）、更低的开销、更容易进行实时分析。这是FreeRTOS简单性哲学的体现——用最简单的机制实现可靠的功能。

### Event-Driven

FreeRTOS is fundamentally event-driven:

```
+------------------+     +------------------+     +------------------+
|  Task BLOCKED    | --> |  EVENT OCCURS    | --> |  Task READY      |
|  (waiting)       |     |  (wake trigger)  |     |  (can run)       |
+------------------+     +------------------+     +------------------+

Events that wake tasks:
  - Tick: vTaskDelay() timeout expires
  - Queue: Data arrives in queue task is waiting on
  - Semaphore: Semaphore becomes available
  - Event Group: Bits task is waiting for are set
  - Notification: Task receives direct notification
```

**No polling loop inside kernel**:
```c
/* WRONG mental model - NOT how FreeRTOS works */
while(1) {
    check_if_any_task_needs_to_wake();  // POLLING
    run_highest_priority_ready_task();
}

/* CORRECT mental model - event-driven */
// When SysTick fires:
xTaskIncrementTick();  // Check delayed tasks, O(1) if none due

// When queue receives data:
xQueueSend();          // Wake ONE waiting task directly

// Kernel only does work when events occur
```

**Chinese Explanation (中文说明):**

FreeRTOS是事件驱动的，而不是轮询的。任务在等待事件时进入阻塞状态，不消耗CPU。当事件发生时（tick到期、队列有数据、信号量可用等），内核直接唤醒等待该事件的任务。

这与轮询模型不同：轮询模型会不断检查"是否有任务需要唤醒"，而事件驱动模型只在事件发生时执行工作。这使得空闲时CPU可以进入低功耗状态，事件响应也更快。

### Interrupt-Aware but Interrupt-Safe

```
+-------------------------------------------------------------------+
|                  ISR / TASK BOUNDARY                              |
+-------------------------------------------------------------------+
|                                                                   |
|   ISR CONTEXT                    |    TASK CONTEXT                |
|   (Cannot block)                 |    (Can block)                 |
|   +---------------------------+  |  +---------------------------+ |
|   | xQueueSendFromISR()       |  |  | xQueueSend()              | |
|   | xSemaphoreGiveFromISR()   |  |  | xSemaphoreTake()          | |
|   | xTaskNotifyFromISR()      |  |  | vTaskDelay()              | |
|   | NO vTaskDelay()!          |  |  | All blocking OK           | |
|   +---------------------------+  |  +---------------------------+ |
|              |                   |             |                  |
|              v                   |             v                  |
|   +---------------------------+  |  +---------------------------+ |
|   | Sets flag:                |  |  | Blocks on event:          | |
|   | "Higher priority task     |  |  | "I'll wait until data     | |
|   |  was woken"               |  |  |  arrives"                  | |
|   +---------------------------+  |  +---------------------------+ |
|              |                   |                                |
|              v                   |                                |
|   portYIELD_FROM_ISR()          |                                |
|   (Request context switch       |                                |
|    at end of ISR)               |                                |
|                                                                   |
+-------------------------------------------------------------------+
```

**Key insight**: ISRs cannot block. They must complete quickly. FreeRTOS provides separate `FromISR` API functions that:
1. Never block
2. Return whether a context switch is needed
3. Let the ISR decide when to yield

**Chinese Explanation (中文说明):**

FreeRTOS区分ISR上下文和任务上下文。ISR不能阻塞（必须快速完成），所以FreeRTOS提供单独的`FromISR`后缀API函数。这些函数：
1. 永不阻塞
2. 返回是否需要上下文切换
3. 让ISR决定何时切换

例如，`xQueueSend()`可能阻塞等待队列空间，但`xQueueSendFromISR()`如果队列满会立即返回失败。ISR使用FromISR函数发送数据，然后调用`portYIELD_FROM_ISR()`在ISR结束时触发上下文切换。

---

## 2.3 What FreeRTOS Is NOT

Understanding what FreeRTOS is NOT helps set correct expectations.

### Not Linux

```
+-------------------+---------------------------+---------------------------+
|     Feature       |         Linux             |        FreeRTOS           |
+-------------------+---------------------------+---------------------------+
| File system       | ext4, btrfs, NFS, etc.    | None (use FatFS addon)    |
| Network stack     | Full TCP/IP               | None (use FreeRTOS+TCP)   |
| Shell             | bash, zsh                 | None                      |
| Users/permissions | Multi-user, root/user     | None                      |
| Dynamic loading   | .so, dlopen()             | None                      |
| Device drivers    | Kernel modules            | None (just ISRs)          |
| Kernel size       | ~10 MB compressed         | ~5-10 KB                  |
+-------------------+---------------------------+---------------------------+
```

### Not POSIX

```
+-------------------+---------------------------+---------------------------+
|     Concept       |         POSIX             |        FreeRTOS           |
+-------------------+---------------------------+---------------------------+
| Thread create     | pthread_create()          | xTaskCreate()             |
| Mutex             | pthread_mutex_*()         | xSemaphoreCreateMutex()   |
| Semaphore         | sem_*()                   | xSemaphoreCreateBinary()  |
| Message queue     | mq_*()                    | xQueueCreate()            |
| Signals           | kill(), signal()          | Task notifications        |
| fork()            | Supported                 | Not possible              |
| errno             | Per-thread                | Optional per-task         |
+-------------------+---------------------------+---------------------------+
```

**Chinese Explanation (中文说明):**

FreeRTOS不是Linux：没有文件系统、网络栈、shell、用户权限、动态加载或设备驱动框架。这些功能需要通过附加库（如FatFS、FreeRTOS+TCP）添加。

FreeRTOS也不是POSIX：API完全不同（虽然有POSIX兼容层），不支持fork()，信号机制也不同。FreeRTOS的API针对嵌入式系统优化，比POSIX更简单、开销更小。

### Not a Process Model (Shared Memory)

```
Linux Process Model:                    FreeRTOS Task Model:
+-------------------+                   +-------------------+
| Process A         |                   | Task A            |
| +--------------+  |                   | +--------------+  |
| | Virtual      |  |                   | | Stack A      |  |
| | Address      |  |                   | +--------------+  |
| | Space        |  |                   |        |          |
| | 0x0000-0xFFFF|  |                   |        v          |
| +--------------+  |                   | Shared Physical   |
+-------------------+                   | Memory            |
        |                               |        ^          |
        v                               |        |          |
     MMU Translation                    | +--------------+  |
        |                               | | Stack B      |  |
        v                               | +--------------+  |
+-------------------+                   | Task B            |
| Physical RAM      |                   +-------------------+
| (isolated)        |
+-------------------+                   ALL TASKS SEE SAME
                                        MEMORY ADDRESSES
```

**Consequence**: In FreeRTOS, a bug in any task can corrupt memory used by any other task. There is no isolation (unless using MPU regions).

**Chinese Explanation (中文说明):**

FreeRTOS不是进程模型，而是共享内存模型。所有任务共享相同的物理地址空间，没有虚拟内存隔离。这意味着：一个任务的bug可能破坏其他任务使用的内存。

这是设计权衡的结果：大多数MCU没有MMU（内存管理单元），即使有也会增加内存和性能开销。FreeRTOS可以选择使用MPU（内存保护单元）提供有限的保护，但这是可选的，且功能不如完整的MMU。

### No Virtual Memory

```
With Virtual Memory (Linux):            Without Virtual Memory (FreeRTOS):

+-------------------+                   +-------------------+
| malloc(1GB)       |                   | pvPortMalloc(1GB) |
| -> Probably OK!   |                   | -> NULL (no RAM)  |
| -> Pages swapped  |                   | -> No swap        |
|    to disk        |                   | -> No overcommit  |
+-------------------+                   +-------------------+

| Stack overflow:   |                   | Stack overflow:   |
| -> SIGSEGV        |                   | -> Silent         |
| -> Core dump      |                   |    corruption     |
| -> Debuggable     |                   | -> Hard to debug  |
+-------------------+                   +-------------------+
```

**Why this matters**:
- Must know exact memory requirements at compile time
- Stack sizes must be carefully chosen
- No safety net for memory errors

**Chinese Explanation (中文说明):**

FreeRTOS没有虚拟内存。这意味着：
- 无法分配超过物理RAM的内存
- 没有交换空间或过度提交
- 栈溢出可能导致静默的内存损坏，而不是明确的错误

这要求开发者在编译时就知道确切的内存需求，必须仔细选择每个任务的栈大小。没有Linux那样的安全网（segfault和core dump）来捕获内存错误。

### No System Calls

```
Linux:                                  FreeRTOS:
+-------------------+                   +-------------------+
| User Space        |                   | Task Code         |
| +-------------+   |                   | +-------------+   |
| | Application |   |                   | | Application |   |
| +------+------+   |                   | +------+------+   |
|        |          |                   |        |          |
|        v          |                   |        v          |
| +-------------+   |                   | +--------------+  |
| | System Call |   |                   | | Direct Call  |  |
| | Trap to     |   |                   | | (same addr   |  |
| | kernel mode |   |                   | |  space)      |  |
| +------+------+   |                   | +--------------+  |
|        |          |                   |        |          |
|        v          |                   |        v          |
| +-------------+   |                   | +--------------+  |
| | Kernel Space|   |                   | | Kernel Code  |  |
| | (privileged)|   |                   | | (same mode)  |  |
| +-------------+   |                   | +--------------+  |
+-------------------+                   +-------------------+
```

**Why no system calls?**
- Single address space
- No user/kernel privilege separation (on most ports)
- Function calls are just function calls
- Lower overhead

**Chinese Explanation (中文说明):**

FreeRTOS没有系统调用。在Linux中，应用程序必须通过系统调用陷入内核模式才能执行特权操作。在FreeRTOS中，所有代码在同一地址空间和同一特权级别运行（大多数情况下）。调用FreeRTOS API就是普通的函数调用，没有模式切换开销。

这使得FreeRTOS更快、更简单，但也意味着没有安全隔离——任何代码都可以直接访问内核数据结构。

---

## Summary

```
+================================================================+
|                   FREERTOS AT A GLANCE                         |
+================================================================+
|                                                                |
|  IS:                           IS NOT:                         |
|  - Scheduler                   - Operating system              |
|  - Task manager                - File system                   |
|  - Synchronization primitives  - Network stack                 |
|  - Event-driven                - Device driver framework       |
|  - Interrupt-aware             - Process model                 |
|  - Portable                    - POSIX compatible              |
|  - Small (~5-10KB)             - Feature-rich                  |
|  - Deterministic               - Fair scheduling               |
|                                                                |
+================================================================+
```

| Aspect | Design Choice | Reason |
|--------|---------------|--------|
| Language | C | Portability, predictability |
| Threading | Single-threaded kernel | Simplicity, no locks |
| Events | Event-driven | Efficiency, low latency |
| ISR | Separate FromISR APIs | Non-blocking ISR requirement |
| Memory | Shared address space | No MMU on most MCUs |
| Privilege | Same privilege level | Simplicity, performance |

**Next Section**: [FreeRTOS Source Tree Tour](03-source-tree-tour.md)
