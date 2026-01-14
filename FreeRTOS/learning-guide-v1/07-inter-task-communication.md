# Section 7: Inter-Task Communication

Tasks need to communicate and synchronize. FreeRTOS provides several mechanisms, each with different characteristics and use cases.

## 7.1 Queues

### How Queues Work Internally

```
QUEUE STRUCTURE (from queue.c):
+------------------------------------------------------------------+
|                                                                  |
|  Queue_t:                                                        |
|  +------------------------------------------------------------+  |
|  | pcHead          | Start of storage buffer                  |  |
|  | pcWriteTo       | Next write position                      |  |
|  | pcTail          | End of storage buffer                    |  |
|  | pcReadFrom      | Last read position                       |  |
|  +------------------------------------------------------------+  |
|  | xTasksWaitingToSend    | Blocked senders (list)             |  |
|  | xTasksWaitingToReceive | Blocked receivers (list)           |  |
|  +------------------------------------------------------------+  |
|  | uxMessagesWaiting | Current item count                      |  |
|  | uxLength          | Maximum items                           |  |
|  | uxItemSize        | Bytes per item                          |  |
|  +------------------------------------------------------------+  |
|  | cRxLock, cTxLock  | ISR locking mechanism                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### Queue Memory Layout

```
Queue of 3 items, item size = 4 bytes:
+------------------------------------------------------------------+
|                                                                  |
|  Queue_t (control structure):                                    |
|  +---------------------------+                                   |
|  | pcHead ---------------+   |                                   |
|  | pcWriteTo ----------+ |   |                                   |
|  | pcTail ----------+  | |   |                                   |
|  | pcReadFrom ---+  |  | |   |                                   |
|  | ...           |  |  | |   |                                   |
|  +---------------+--+--+-+---+                                   |
|                  |  |  | |                                       |
|                  v  v  v v                                       |
|  Storage:        +--+--+--+--+--+--+--+--+--+--+--+--+           |
|                  |  Item 0  |  Item 1  |  Item 2  |             |
|                  | (4 bytes)| (4 bytes)| (4 bytes)|             |
|                  +----------+----------+----------+             |
|                  ^                                ^              |
|                  pcHead                          pcTail          |
|                                                                  |
+------------------------------------------------------------------+
```

### Why Queues Copy Data (Not Pointers)

```
DATA COPYING (FreeRTOS default):
+------------------------------------------------------------------+
|                                                                  |
|  xQueueSend( queue, &myData, timeout ):                          |
|                                                                  |
|  Task A Stack:           Queue Storage:                          |
|  +------------+          +------------+                          |
|  | myData     |  COPY    | myData     |                          |
|  | value=42   | =======> | value=42   |                          |
|  +------------+          +------------+                          |
|                                                                  |
|  AFTER xQueueSend returns, Task A can modify/free myData         |
|  Queue has its OWN COPY of the data                              |
|                                                                  |
+------------------------------------------------------------------+

WHY COPY?
+------------------------------------------------------------------+
|                                                                  |
|  1. LIFETIME SAFETY:                                             |
|     - Task A could return and destroy stack variable             |
|     - Queue copy survives independent of sender's lifetime       |
|                                                                  |
|  2. NO MEMORY MANAGEMENT COMPLEXITY:                             |
|     - No "who frees this pointer?" ambiguity                     |
|     - No reference counting needed                               |
|                                                                  |
|  3. INTERRUPT SAFETY:                                            |
|     - ISR can send data from stack safely                        |
|     - Data is copied before ISR returns                          |
|                                                                  |
|  WHEN TO PASS POINTERS:                                          |
|     - Large data (copy overhead too high)                        |
|     - Use static/heap memory with clear ownership                |
|     - Example: Queue of pointers to pre-allocated buffers        |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

队列内部结构：Queue_t包含存储缓冲区指针（pcHead、pcTail、pcWriteTo、pcReadFrom）、等待发送/接收的任务列表、消息计数、最大项数、每项大小、ISR锁机制。

为什么复制数据而不是指针？
1. 生命周期安全：发送者返回后可能销毁栈变量，队列副本独立存在
2. 无内存管理复杂性：无"谁释放指针"歧义，无需引用计数
3. 中断安全：ISR可以安全地从栈发送数据

何时传递指针：大数据（复制开销太高）、使用静态/堆内存且所有权明确。

### Blocking Semantics

```
QUEUE BLOCKING:
+------------------------------------------------------------------+
|                                                                  |
|  xQueueSend (Queue Full):                                        |
|  +------------------------------------------------------------+  |
|  | 1. Check: Is queue full?                                   |  |
|  | 2. If full and timeout > 0:                                |  |
|  |    - Add task to xTasksWaitingToSend                       |  |
|  |    - Add task to delayed list (for timeout)                |  |
|  |    - Block                                                 |  |
|  | 3. Wake when: Space available OR timeout                   |  |
|  | 4. Return: pdPASS (sent) or errQUEUE_FULL (timeout)        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  xQueueReceive (Queue Empty):                                    |
|  +------------------------------------------------------------+  |
|  | 1. Check: Is queue empty?                                  |  |
|  | 2. If empty and timeout > 0:                               |  |
|  |    - Add task to xTasksWaitingToReceive                    |  |
|  |    - Add task to delayed list (for timeout)                |  |
|  |    - Block                                                 |  |
|  | 3. Wake when: Data available OR timeout                    |  |
|  | 4. Return: pdPASS (received) or errQUEUE_EMPTY (timeout)   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+

PRIORITY-BASED WAKE:
+------------------------------------------------------------------+
|                                                                  |
|  Multiple tasks blocked on same queue:                           |
|                                                                  |
|  xTasksWaitingToReceive:                                         |
|  [Task Pri=3] -> [Task Pri=1] -> [Task Pri=2]                    |
|       ^                                                          |
|       Highest priority wakes first!                              |
|                                                                  |
|  List is SORTED BY PRIORITY (actually by inverse priority)       |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

队列阻塞语义：

发送时队列满：如果timeout>0，任务加入xTasksWaitingToSend列表和延时列表，然后阻塞。有空间或超时时唤醒。

接收时队列空：如果timeout>0，任务加入xTasksWaitingToReceive列表和延时列表，然后阻塞。有数据或超时时唤醒。

优先级唤醒：多个任务阻塞在同一队列时，最高优先级的任务先唤醒（列表按优先级排序）。

---

## 7.2 Semaphores and Mutexes

### Semaphores as Special Queues

```
SEMAPHORE = QUEUE WITH SIZE 0:
+------------------------------------------------------------------+
|                                                                  |
|  Binary Semaphore:                                               |
|  +------------------------------------------------------------+  |
|  | uxLength = 1                                               |  |
|  | uxItemSize = 0  (no data, just count)                      |  |
|  | uxMessagesWaiting = 0 or 1                                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  xSemaphoreGive() = xQueueSend()                                 |
|  xSemaphoreTake() = xQueueReceive()                              |
|                                                                  |
|  Counting Semaphore:                                             |
|  +------------------------------------------------------------+  |
|  | uxLength = N (max count)                                   |  |
|  | uxItemSize = 0                                             |  |
|  | uxMessagesWaiting = current count (0 to N)                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### Binary Semaphore Use Case

```
BINARY SEMAPHORE: EVENT SIGNALING
+------------------------------------------------------------------+
|                                                                  |
|  ISR signals event, task processes it:                           |
|                                                                  |
|  ISR:                          Task:                             |
|  +--------------------+        +-----------------------------+   |
|  | Data arrives       |        | for(;;) {                   |   |
|  | xSemaphoreGive-    |        |     xSemaphoreTake(sem,     |   |
|  |   FromISR(sem)     |------->|       portMAX_DELAY);       |   |
|  | // Signal task     |        |     // Process data          |   |
|  +--------------------+        |     ProcessData();           |   |
|                                | }                            |   |
|                                +-----------------------------+   |
|                                                                  |
|  Semaphore count: 0 -> 1 (give) -> 0 (take)                      |
|                                                                  |
+------------------------------------------------------------------+
```

### Counting Semaphore Use Case

```
COUNTING SEMAPHORE: RESOURCE POOL
+------------------------------------------------------------------+
|                                                                  |
|  Pool of 3 hardware resources (e.g., DMA channels):              |
|                                                                  |
|  Semaphore count = 3 (initially)                                 |
|                                                                  |
|  Task A: xSemaphoreTake() -> count=2, Task A uses resource 1     |
|  Task B: xSemaphoreTake() -> count=1, Task B uses resource 2     |
|  Task C: xSemaphoreTake() -> count=0, Task C uses resource 3     |
|  Task D: xSemaphoreTake() -> BLOCKS (count=0, none available)    |
|                                                                  |
|  Task A: xSemaphoreGive() -> count=1, Task D unblocks            |
|                                                                  |
+------------------------------------------------------------------+
```

### Mutex vs Binary Semaphore

```
KEY DIFFERENCE: OWNERSHIP
+------------------------------------------------------------------+
|                                                                  |
|  BINARY SEMAPHORE:                                               |
|  +------------------------------------------------------------+  |
|  | - Any task can Give (signal)                               |  |
|  | - Any task can Take (wait)                                 |  |
|  | - NO ownership concept                                     |  |
|  | - Used for: Signaling between tasks/ISRs                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  MUTEX:                                                          |
|  +------------------------------------------------------------+  |
|  | - Only task that Took can Give                             |  |
|  | - HAS ownership (xMutexHolder in Queue_t)                  |  |
|  | - Supports PRIORITY INHERITANCE                            |  |
|  | - Used for: Protecting shared resources                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### Priority Inheritance (Why It Exists)

```
PRIORITY INVERSION PROBLEM:
+------------------------------------------------------------------+
|                                                                  |
|  Without Priority Inheritance:                                   |
|                                                                  |
|  Time -->                                                        |
|                                                                  |
|  Task H (High)   : [blocked on mutex]---[blocked]---[blocked]    |
|  Task M (Medium) :           [================RUNNING==========] |
|  Task L (Low)    : [=MUTEX=] [preempted]                         |
|                      ^                                           |
|                      Task L holds mutex                          |
|                                                                  |
|  PROBLEM:                                                        |
|  - Task H waits for mutex (held by Task L)                       |
|  - Task M preempts Task L (M > L)                                |
|  - Task H waits for Task M (lower priority!) to finish           |
|  - Task H's priority is effectively INVERTED to below M         |
|                                                                  |
+------------------------------------------------------------------+

WITH PRIORITY INHERITANCE:
+------------------------------------------------------------------+
|                                                                  |
|  Time -->                                                        |
|                                                                  |
|  Task H (High)   : [blocked on mutex]--[RUNNING]                 |
|  Task M (Medium) :           [blocked]--[RUNNING]                |
|  Task L (Low)    : [=MUTEX=][pri=H]===][give]                    |
|                      ^        ^                                  |
|                      L holds  L inherits H's priority            |
|                      mutex    while holding mutex                |
|                                                                  |
|  SOLUTION:                                                       |
|  - When Task H blocks on mutex, Task L inherits H's priority     |
|  - Task L cannot be preempted by Task M                          |
|  - Task L finishes quickly, gives mutex                          |
|  - Task L returns to original priority                           |
|  - Task H runs immediately                                       |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

信号量是特殊队列：uxItemSize=0（无数据，仅计数）。二值信号量uxLength=1，计数信号量uxLength=N。

二值信号量用例：事件信号（ISR到任务通信）。
计数信号量用例：资源池管理（如DMA通道）。

互斥锁vs二值信号量：关键区别是所有权。互斥锁只有持有者能释放，支持优先级继承。

优先级反转问题：低优先级任务持有互斥锁，高优先级任务等待，中等优先级任务抢占低优先级任务——高优先级任务被中等优先级任务间接阻塞。

优先级继承解决方案：高优先级任务阻塞时，持有互斥锁的低优先级任务继承高优先级，快速完成后释放，高优先级任务立即运行。

---

## 7.3 Event Groups

### Bitmask-Based Synchronization

```
EVENT GROUP STRUCTURE:
+------------------------------------------------------------------+
|                                                                  |
|  EventGroup_t:                                                   |
|  +------------------------------------------------------------+  |
|  | uxEventBits: 8, 16, or 24 bits (platform dependent)        |  |
|  |                                                             |  |
|  |   Bit 0: EVENT_SENSOR_READY                                |  |
|  |   Bit 1: EVENT_DATA_PROCESSED                              |  |
|  |   Bit 2: EVENT_DISPLAY_UPDATED                             |  |
|  |   ...                                                       |  |
|  +------------------------------------------------------------+  |
|  | xTasksWaitingForBits: List of waiting tasks                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### Event Group Operations

```
WAIT FOR ANY vs WAIT FOR ALL:
+------------------------------------------------------------------+
|                                                                  |
|  #define BIT_A (1 << 0)                                          |
|  #define BIT_B (1 << 1)                                          |
|  #define BIT_C (1 << 2)                                          |
|                                                                  |
|  WAIT FOR ANY (OR):                                              |
|  +------------------------------------------------------------+  |
|  | xEventGroupWaitBits(eg, BIT_A | BIT_B, pdFALSE,            |  |
|  |                     pdFALSE, timeout);                      |  |
|  |                                                             |  |
|  | Task wakes when: BIT_A set OR BIT_B set OR both            |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WAIT FOR ALL (AND):                                             |
|  +------------------------------------------------------------+  |
|  | xEventGroupWaitBits(eg, BIT_A | BIT_B, pdFALSE,            |  |
|  |                     pdTRUE, timeout);  // <-- ALL          |  |
|  |                                                             |  |
|  | Task wakes when: BIT_A set AND BIT_B set                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CLEAR ON EXIT:                                                  |
|  +------------------------------------------------------------+  |
|  | xEventGroupWaitBits(eg, BIT_A, pdTRUE, ...);               |  |
|  |                          // ^-- clear bit after wake       |  |
|  |                                                             |  |
|  | Useful for one-shot events                                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### When Event Groups Are Superior to Semaphores

```
USE CASE: SYNCHRONIZATION BARRIER
+------------------------------------------------------------------+
|                                                                  |
|  Wait for multiple tasks to complete initialization:             |
|                                                                  |
|  WITH SEMAPHORES (awkward):                                      |
|  +------------------------------------------------------------+  |
|  | Need 3 semaphores for 3 tasks                              |  |
|  | Coordinator must take all 3 in sequence                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WITH EVENT GROUP (elegant):                                     |
|  +------------------------------------------------------------+  |
|  | #define TASK_A_DONE (1<<0)                                 |  |
|  | #define TASK_B_DONE (1<<1)                                 |  |
|  | #define TASK_C_DONE (1<<2)                                 |  |
|  | #define ALL_DONE (TASK_A_DONE|TASK_B_DONE|TASK_C_DONE)     |  |
|  |                                                             |  |
|  | Task A: xEventGroupSetBits(eg, TASK_A_DONE);               |  |
|  | Task B: xEventGroupSetBits(eg, TASK_B_DONE);               |  |
|  | Task C: xEventGroupSetBits(eg, TASK_C_DONE);               |  |
|  |                                                             |  |
|  | Coordinator:                                                |  |
|  | xEventGroupWaitBits(eg, ALL_DONE, pdTRUE, pdTRUE, timeout);|  |
|  | // Wakes when ALL THREE bits are set                       |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

事件组：基于位掩码的同步。uxEventBits是8/16/24位（取决于平台），每个位代表一个事件。

等待模式：
- WAIT FOR ANY（OR）：任一位被设置即唤醒
- WAIT FOR ALL（AND）：所有位都被设置才唤醒
- CLEAR ON EXIT：唤醒后清除等待的位

事件组优于信号量的场景：同步屏障。等待多个任务完成——用信号量需要多个信号量和顺序获取，用事件组只需设置位和等待ALL。

---

## Summary

```
INTER-TASK COMMUNICATION MENTAL MODEL:
+==================================================================+
||                                                                ||
||  QUEUES:                                                      ||
||  - Pass DATA between tasks                                    ||
||  - Copy semantics (safe, but overhead)                        ||
||  - Blocking send/receive                                      ||
||                                                                ||
||  BINARY SEMAPHORE:                                            ||
||  - Signal EVENTS (no data)                                    ||
||  - ISR -> Task notification                                   ||
||  - No ownership                                               ||
||                                                                ||
||  COUNTING SEMAPHORE:                                          ||
||  - Manage RESOURCE POOLS                                      ||
||  - Track available resources                                  ||
||                                                                ||
||  MUTEX:                                                       ||
||  - PROTECT shared resources                                   ||
||  - Ownership (only holder can release)                        ||
||  - Priority inheritance                                       ||
||                                                                ||
||  EVENT GROUP:                                                 ||
||  - MULTIPLE CONDITIONS                                        ||
||  - Wait for ANY or ALL bits                                   ||
||  - Synchronization barriers                                   ||
||                                                                ||
+==================================================================+
```

| Mechanism | Data? | Ownership? | Priority Inheritance? | Best For |
|-----------|-------|------------|----------------------|----------|
| Queue | Yes (copy) | No | No | Message passing |
| Binary Sem | No | No | No | Event signaling |
| Counting Sem | No | No | No | Resource counting |
| Mutex | No | Yes | Yes | Resource protection |
| Event Group | Bits | No | No | Multi-condition sync |

**Next Section**: [Timers and Deferred Work](08-timers-and-deferred-work.md)
