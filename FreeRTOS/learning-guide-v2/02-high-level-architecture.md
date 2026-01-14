# Section 2: High-Level FreeRTOS Architecture

## 2.1 The Big Picture

```
FREERTOS KERNEL ARCHITECTURE:
+==================================================================+
||                                                                ||
||                     APPLICATION LAYER                          ||
||  +----------------------------------------------------------+  ||
||  | Task 1    | Task 2    | Task 3    | Task N    |          |  ||
||  | (User)    | (User)    | (User)    | (User)    |          |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            | API Calls                         ||
||                            v                                   ||
||  +----------------------------------------------------------+  ||
||  |               FREERTOS KERNEL CORE                       |  ||
||  |                                                          |  ||
||  |  +----------+  +-----------+  +-----------+              |  ||
||  |  | SCHEDULER|  | QUEUES    |  | TIMERS    |              |  ||
||  |  | tasks.c  |  | queue.c   |  | timers.c  |              |  ||
||  |  +----------+  +-----------+  +-----------+              |  ||
||  |                                                          |  ||
||  |  +----------+  +-----------+  +-----------+              |  ||
||  |  | LISTS    |  | EVENT     |  | MEMORY    |              |  ||
||  |  | list.c   |  | GROUPS    |  | heap_x.c  |              |  ||
||  |  +----------+  +-----------+  +-----------+              |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            | Port Layer Interface              ||
||                            v                                   ||
||  +----------------------------------------------------------+  ||
||  |                    PORT LAYER                            |  ||
||  |  +------------------------------------------------------+|  ||
||  |  | port.c                 | portmacro.h                 ||  ||
||  |  | - Context switch       | - Type definitions          ||  ||
||  |  | - Tick interrupt       | - Critical section macros   ||  ||
||  |  | - Stack init           | - Yield macro               ||  ||
||  |  +------------------------------------------------------+|  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  +----------------------------------------------------------+  ||
||  |                    HARDWARE                              |  ||
||  |  CPU Registers | SysTick | NVIC | Stack Pointer          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

FreeRTOS内核架构从上到下：

应用层：用户任务（Task 1-N），通过API调用内核。

内核核心：调度器(tasks.c)、队列(queue.c)、定时器(timers.c)、列表(list.c)、事件组(event_groups.c)、内存管理(heap_x.c)。

移植层：port.c（上下文切换、时钟中断、栈初始化）和portmacro.h（类型定义、临界区宏、yield宏）。

硬件层：CPU寄存器、SysTick、NVIC、栈指针。

---

## 2.2 Kernel Core Components

### Component Dependency Graph

```
COMPONENT DEPENDENCIES:
+------------------------------------------------------------------+
|                                                                  |
|                        +---------------+                         |
|                        |   tasks.c     |                         |
|                        |  (Scheduler)  |                         |
|                        +-------+-------+                         |
|                                |                                 |
|           +--------------------+--------------------+            |
|           |                    |                    |            |
|           v                    v                    v            |
|   +---------------+    +---------------+    +---------------+    |
|   |   queue.c     |    |   timers.c    |    | event_groups.c|    |
|   | (IPC/Sync)    |    | (Soft Timers) |    | (Event Flags) |    |
|   +-------+-------+    +-------+-------+    +-------+-------+    |
|           |                    |                    |            |
|           +--------------------+--------------------+            |
|                                |                                 |
|                                v                                 |
|                        +---------------+                         |
|                        |    list.c     |                         |
|                        | (Foundation)  |                         |
|                        +---------------+                         |
|                                                                  |
|  SEPARATE (no kernel dependencies):                              |
|                        +---------------+                         |
|                        |   heap_x.c    |                         |
|                        |   (Memory)    |                         |
|                        +---------------+                         |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

组件依赖关系：tasks.c（调度器）是核心，依赖queue.c（IPC/同步）、timers.c（软件定时器）、event_groups.c（事件标志）。所有这些都依赖list.c（基础数据结构）。heap_x.c（内存管理）是独立的，无内核依赖。

### What Each Component Does

| Component | File | Responsibility |
|-----------|------|----------------|
| **Scheduler** | `tasks.c` | Task creation, deletion, state management, context switch triggering |
| **Lists** | `list.c` | Generic doubly-linked list used by all other components |
| **Queues** | `queue.c` | Message passing, semaphores, mutexes (all built on same structure) |
| **Timers** | `timers.c` | Software timers, callback execution in task context |
| **Event Groups** | `event_groups.c` | Multi-bit synchronization flags |
| **Memory** | `heap_1-5.c` | Dynamic allocation schemes (choose one) |

---

## 2.3 Thread Context vs Interrupt Context

```
EXECUTION CONTEXTS:
+==================================================================+
||                                                                ||
||  THREAD (TASK) CONTEXT:                                        ||
||  +----------------------------------------------------------+  ||
||  | - Runs at task priority level                            |  ||
||  | - Has own stack                                          |  ||
||  | - Can block (wait for events)                            |  ||
||  | - Can be preempted by higher priority task or ISR        |  ||
||  | - Uses standard FreeRTOS APIs                            |  ||
||  |                                                          |  ||
||  |   void MyTask(void *p)                                   |  ||
||  |   {                                                      |  ||
||  |       for(;;)                                            |  ||
||  |       {                                                  |  ||
||  |           xQueueReceive(q, &data, portMAX_DELAY); // OK  |  ||
||  |           process(data);                                 |  ||
||  |       }                                                  |  ||
||  |   }                                                      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  INTERRUPT CONTEXT:                                            ||
||  +----------------------------------------------------------+  ||
||  | - Runs at hardware interrupt priority                    |  ||
||  | - Uses MSP (main stack pointer)                          |  ||
||  | - CANNOT block                                           |  ||
||  | - Must be short                                          |  ||
||  | - Must use FromISR APIs only                             |  ||
||  |                                                          |  ||
||  |   void UART_IRQHandler(void)                             |  ||
||  |   {                                                      |  ||
||  |       BaseType_t xHigherPriorityTaskWoken = pdFALSE;     |  ||
||  |                                                          |  ||
||  |       xQueueSendFromISR(q, &byte,                        |  ||
||  |                         &xHigherPriorityTaskWoken);      |  ||
||  |                                                          |  ||
||  |       portYIELD_FROM_ISR(xHigherPriorityTaskWoken);      |  ||
||  |   }                                                      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

执行上下文分两种：

线程（任务）上下文：在任务优先级运行、有自己的栈、可以阻塞等待事件、可被更高优先级任务或ISR抢占、使用标准FreeRTOS API。

中断上下文：在硬件中断优先级运行、使用MSP（主栈指针）、不能阻塞、必须短小、必须只使用FromISR API。

### The Critical Boundary

```
CONTEXT BOUNDARY:
+------------------------------------------------------------------+
|                                                                  |
|  Task Context                 |    ISR Context                   |
|  (Can block)                  |    (Cannot block)                |
|                               |                                  |
|  xQueueSend()      -------->  |  xQueueSendFromISR()             |
|  xQueueReceive()   -------->  |  xQueueReceiveFromISR()          |
|  xSemaphoreTake()  -------->  |  xSemaphoreTakeFromISR()         |
|  xSemaphoreGive()  -------->  |  xSemaphoreGiveFromISR()         |
|  xTaskNotify()     -------->  |  xTaskNotifyFromISR()            |
|  vTaskDelay()      -------->  |  (NO EQUIVALENT - cannot block)  |
|                               |                                  |
|  RULES:                       |  RULES:                          |
|  - Can call any API           |  - FromISR only                  |
|  - Can block indefinitely     |  - Return quickly                |
|  - Scheduler manages timing   |  - Manual yield decision         |
|                               |                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

上下文边界规则：

任务上下文：可以调用任何API、可以无限阻塞、调度器管理时序。

ISR上下文：只能用FromISR API、必须快速返回、手动决定yield。

每个标准API都有对应的FromISR版本，但vTaskDelay没有等效的FromISR版本（因为ISR不能阻塞）。

---

## 2.4 What Is Configurable at Compile Time

FreeRTOS uses `FreeRTOSConfig.h` for compile-time configuration.

```
CONFIGURATION PHILOSOPHY:
+------------------------------------------------------------------+
|                                                                  |
|  WHY COMPILE-TIME CONFIGURATION?                                 |
|                                                                  |
|  1. Zero runtime overhead for unused features                    |
|  2. Smaller code size (unused code not compiled)                 |
|  3. Static analysis possible                                     |
|  4. Deterministic behavior (no runtime decisions)                |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |                    FreeRTOSConfig.h                        |  |
|  |                                                            |  |
|  |  SCHEDULER OPTIONS:                                        |  |
|  |  #define configUSE_PREEMPTION        1  // 0=cooperative   |  |
|  |  #define configUSE_TIME_SLICING      1  // Round-robin     |  |
|  |  #define configMAX_PRIORITIES        5  // 1-56 typically  |  |
|  |                                                            |  |
|  |  TICK OPTIONS:                                             |  |
|  |  #define configTICK_RATE_HZ          1000  // 1ms tick     |  |
|  |  #define configUSE_TICKLESS_IDLE     0     // Low power    |  |
|  |                                                            |  |
|  |  FEATURE TOGGLES:                                          |  |
|  |  #define configUSE_MUTEXES           1                     |  |
|  |  #define configUSE_RECURSIVE_MUTEXES 1                     |  |
|  |  #define configUSE_COUNTING_SEMAPHORES 1                   |  |
|  |  #define configUSE_TIMERS            1                     |  |
|  |  #define configUSE_EVENT_GROUPS      1                     |  |
|  |                                                            |  |
|  |  MEMORY OPTIONS:                                           |  |
|  |  #define configSUPPORT_STATIC_ALLOCATION  1                |  |
|  |  #define configSUPPORT_DYNAMIC_ALLOCATION 1                |  |
|  |  #define configTOTAL_HEAP_SIZE       (10 * 1024)           |  |
|  |                                                            |  |
|  |  DEBUG OPTIONS:                                            |  |
|  |  #define configCHECK_FOR_STACK_OVERFLOW  2                 |  |
|  |  #define configUSE_TRACE_FACILITY    1                     |  |
|  |  #define configASSERT(x) if(!(x)) { for(;;); }             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么是编译时配置？

1. 未使用功能零运行时开销
2. 更小代码大小（未使用代码不编译）
3. 可进行静态分析
4. 确定性行为（无运行时决策）

配置选项分类：调度器选项（抢占/协作、时间片、最大优先级）、时钟选项（tick频率、无tick空闲）、功能开关（互斥锁、计数信号量、定时器、事件组）、内存选项（静态/动态分配、堆大小）、调试选项（栈溢出检查、trace工具、断言）。

### Feature Toggle Impact on Code Size

```
CONDITIONAL COMPILATION EXAMPLE:
+------------------------------------------------------------------+
|                                                                  |
|  In queue.c:                                                     |
|                                                                  |
|  #if( configUSE_MUTEXES == 1 )                                   |
|      // ~500 bytes of mutex-specific code                        |
|      static void prvInitialiseMutex( Queue_t *pxNewQueue )       |
|      {                                                           |
|          ...                                                     |
|      }                                                           |
|  #endif                                                          |
|                                                                  |
|  If you don't need mutexes:                                      |
|  #define configUSE_MUTEXES  0                                    |
|  -> 500+ bytes saved                                             |
|                                                                  |
|  TYPICAL CODE SIZE:                                              |
|  +------------------------------------------------------------+  |
|  | Minimal config (tasks only):         ~4 KB                 |  |
|  | Full config (all features):          ~10 KB                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

条件编译示例：如果不需要互斥锁，设置configUSE_MUTEXES为0，可节省500+字节。

典型代码大小：最小配置（仅任务）约4KB，完整配置（所有功能）约10KB。

---

## 2.5 Data Flow in FreeRTOS

```
TYPICAL DATA FLOW PATTERN:
+==================================================================+
||                                                                ||
||  +----------+     Queue      +----------+     Queue           ||
||  | ISR      | -------------> | TaskA    | ----------------->  ||
||  | (detect) |   Raw Data     | (process)|    Commands         ||
||  +----------+                +----------+                      ||
||                                                   |            ||
||                                                   v            ||
||                                              +----------+      ||
||                                              | TaskB    |      ||
||                                              | (actuate)|      ||
||                                              +----------+      ||
||                                                                ||
||  Example: Sensor data acquisition                              ||
||                                                                ||
||  +----------+   +----------+   +----------+   +----------+    ||
||  | ADC_ISR  |-->| RawQueue |-->| FilterTsk|-->| CtrlQueue|    ||
||  +----------+   +----------+   +----------+   +----------+    ||
||                                                   |            ||
||                                                   v            ||
||                                              +----------+      ||
||                                              | MotorTask|      ||
||                                              +----------+      ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

典型数据流模式：ISR检测事件，通过队列发送原始数据到TaskA处理，TaskA处理后通过队列发送命令到TaskB执行。

示例：传感器数据采集。ADC_ISR -> RawQueue -> FilterTask -> CtrlQueue -> MotorTask。

---

## 2.6 Synchronization Flow

```
SYNCHRONIZATION PATTERNS:
+------------------------------------------------------------------+
|                                                                  |
|  MUTUAL EXCLUSION (Mutex):                                       |
|                                                                  |
|  TaskA             Mutex              TaskB                      |
|    |                 |                  |                        |
|    |--Take()-------->|                  |                        |
|    |<---Acquired-----|                  |                        |
|    |                 |<----Take()-------|                        |
|    |  [Critical      |    [BLOCKED]     |                        |
|    |   Section]      |                  |                        |
|    |--Give()-------->|                  |                        |
|    |                 |---Acquired------>|                        |
|    |                 |                  |                        |
|                                                                  |
|  SIGNALING (Binary Semaphore):                                   |
|                                                                  |
|  ISR              Semaphore           Task                       |
|    |                 |                  |                        |
|    |                 |<----Take()-------| (blocks)               |
|    |                 |     [BLOCKED]    |                        |
|    |--GiveFromISR()->|                  |                        |
|    |                 |---Unblock------->|                        |
|    |                 |                  | (runs)                 |
|                                                                  |
|  MULTI-EVENT (Event Group):                                      |
|                                                                  |
|  Task1    Task2    EventGroup    Task3                           |
|    |        |          |           |                             |
|    |        |          |<-Wait(BIT0|BIT1)                        |
|    |        |          |     [BLOCKED]                           |
|    |--SetBit0->        |           |                             |
|    |        |--SetBit1>|           |                             |
|    |        |          |-Unblock-->|                             |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

同步模式：

互斥（Mutex）：TaskA获取mutex进入临界区，TaskB尝试获取被阻塞，TaskA释放后TaskB获取。

信号（二进制信号量）：Task等待信号量被阻塞，ISR给出信号量，Task解除阻塞运行。

多事件（事件组）：Task3等待BIT0和BIT1都被设置，Task1设置BIT0，Task2设置BIT1，Task3解除阻塞。

---

## Summary

```
ARCHITECTURE MENTAL MODEL:
+==================================================================+
||                                                                ||
||  LAYERS (top to bottom):                                       ||
||  1. Application tasks (your code)                              ||
||  2. FreeRTOS API (task.h, queue.h, etc.)                       ||
||  3. Kernel core (tasks.c, queue.c, list.c)                     ||
||  4. Port layer (port.c, portmacro.h)                           ||
||  5. Hardware                                                   ||
||                                                                ||
||  KEY INSIGHTS:                                                 ||
||  - list.c is the foundation of everything                      ||
||  - queue.c implements queues, semaphores, AND mutexes          ||
||  - tasks.c is the scheduler heart                              ||
||  - Port layer is <500 lines typically                          ||
||  - Configuration is compile-time, not runtime                  ||
||                                                                ||
||  TWO EXECUTION CONTEXTS:                                       ||
||  - Task context: can block, use any API                        ||
||  - ISR context: cannot block, use FromISR APIs                 ||
||                                                                ||
+==================================================================+
```

**Next Section**: [FreeRTOS Source Tree Walkthrough](03-source-tree-walkthrough.md)
