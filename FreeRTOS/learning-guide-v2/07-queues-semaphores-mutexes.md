# Section 7: Queues, Semaphores, and Mutexes (Unifying Model)

## 7.1 Why Queues Are the Fundamental Primitive

```
THE UNIFYING INSIGHT:
+==================================================================+
||                                                                ||
||  In FreeRTOS, queues, semaphores, and mutexes are ALL          ||
||  implemented using the SAME underlying Queue_t structure.      ||
||                                                                ||
||  +-------------------+                                         ||
||  |     Queue_t       |                                         ||
||  +-------------------+                                         ||
||  | pcHead            |  Pointer to queue storage               ||
||  | pcWriteTo         |  Next write position                    ||
||  | uxMessagesWaiting |  Count of items (or count for sem)      ||
||  | uxLength          |  Max items (or max count for sem)       ||
||  | uxItemSize        |  Size per item (0 for semaphore)        ||
||  +-------------------+                                         ||
||  | xTasksWaitingTo   |  Tasks blocked on receive/take          ||
||  |    Receive        |                                         ||
||  | xTasksWaitingTo   |  Tasks blocked on send/give             ||
||  |    Send           |                                         ||
||  +-------------------+                                         ||
||                                                                ||
||  DIFFERENTIATION BY CONFIGURATION:                             ||
||  +----------------------------------------------------------+  ||
||  | Message Queue:      uxItemSize > 0, uxLength > 0         |  ||
||  | Binary Semaphore:   uxItemSize = 0, uxLength = 1         |  ||
||  | Counting Semaphore: uxItemSize = 0, uxLength = N         |  ||
||  | Mutex:              uxItemSize = 0, uxLength = 1 + owner |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

统一洞察：在FreeRTOS中，队列、信号量和互斥锁都使用相同的Queue_t结构实现。

Queue_t关键成员：pcHead（队列存储指针）、pcWriteTo（下一个写位置）、uxMessagesWaiting（项计数或信号量计数）、uxLength（最大项数或最大计数）、uxItemSize（每项大小，信号量为0）、xTasksWaitingToReceive（等待接收/获取的阻塞任务）、xTasksWaitingToSend（等待发送/给出的阻塞任务）。

通过配置区分：
- 消息队列：uxItemSize > 0, uxLength > 0
- 二进制信号量：uxItemSize = 0, uxLength = 1
- 计数信号量：uxItemSize = 0, uxLength = N
- 互斥锁：uxItemSize = 0, uxLength = 1 + 拥有者

---

## 7.2 Message Queues

```
MESSAGE QUEUE OPERATION:
+==================================================================+
||                                                                ||
||  Queue with uxLength=3, uxItemSize=4 (4-byte items):           ||
||                                                                ||
||  EMPTY STATE:                                                  ||
||  +-------+-------+-------+                                     ||
||  |       |       |       |   uxMessagesWaiting = 0             ||
||  +-------+-------+-------+                                     ||
||  ^                                                             ||
||  pcWriteTo                                                     ||
||                                                                ||
||  AFTER xQueueSend(q, &itemA):                                  ||
||  +-------+-------+-------+                                     ||
||  | itemA |       |       |   uxMessagesWaiting = 1             ||
||  +-------+-------+-------+                                     ||
||          ^                                                     ||
||          pcWriteTo                                             ||
||                                                                ||
||  AFTER xQueueSend(q, &itemB):                                  ||
||  +-------+-------+-------+                                     ||
||  | itemA | itemB |       |   uxMessagesWaiting = 2             ||
||  +-------+-------+-------+                                     ||
||                  ^                                             ||
||                  pcWriteTo                                     ||
||                                                                ||
||  AFTER xQueueReceive(q, &buffer):                              ||
||  +-------+-------+-------+                                     ||
||  |       | itemB |       |   uxMessagesWaiting = 1             ||
||  +-------+-------+-------+   buffer now contains itemA         ||
||          ^                   (FIFO: first in, first out)       ||
||          pcReadFrom                                            ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

消息队列操作：队列uxLength=3，uxItemSize=4（4字节项）。

空状态：uxMessagesWaiting = 0，pcWriteTo指向开始。

xQueueSend后：itemA入队，uxMessagesWaiting = 1。

再xQueueSend后：itemB入队，uxMessagesWaiting = 2。

xQueueReceive后：itemA出队到buffer，uxMessagesWaiting = 1（FIFO：先进先出）。

### Queue API Summary

```c
/* Create queue */
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength,
                           UBaseType_t uxItemSize);

/* Send to queue (task context) */
BaseType_t xQueueSend(QueueHandle_t xQueue,
                      const void *pvItemToQueue,
                      TickType_t xTicksToWait);

/* Send from ISR */
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                              const void *pvItemToQueue,
                              BaseType_t *pxHigherPriorityTaskWoken);

/* Receive from queue (task context) */
BaseType_t xQueueReceive(QueueHandle_t xQueue,
                         void *pvBuffer,
                         TickType_t xTicksToWait);
```

---

## 7.3 Semaphores Built on Queues

```
BINARY SEMAPHORE = QUEUE WITH uxItemSize=0, uxLength=1:
+==================================================================+
||                                                                ||
||  xSemaphoreCreateBinary() internally does:                     ||
||  xQueueGenericCreate(1, 0, queueQUEUE_TYPE_BINARY_SEMAPHORE)   ||
||                       ^  ^                                     ||
||                       |  |                                     ||
||                       |  +-- Item size = 0 (no data stored)    ||
||                       +-- Length = 1 (binary: 0 or 1)          ||
||                                                                ||
||  "GIVE" = Increment count (if not already 1)                   ||
||  "TAKE" = Decrement count (blocks if already 0)                ||
||                                                                ||
||  STATE DIAGRAM:                                                ||
||                                                                ||
||  +-------+     Give()      +-------+                           ||
||  | Empty | --------------> | Full  |                           ||
||  | (0)   | <-------------- | (1)   |                           ||
||  +-------+     Take()      +-------+                           ||
||      |                         |                               ||
||      | Take() blocks           | Give() ignored                ||
||      v                         v                               ||
||                                                                ||
||  USE CASE: ISR-to-task signaling                               ||
||  +----------------------------------------------------------+  ||
||  | ISR detects event                                        |  ||
||  |   -> xSemaphoreGiveFromISR(sem)                          |  ||
||  |                                                          |  ||
||  | Task waiting                                             |  ||
||  |   -> xSemaphoreTake(sem, portMAX_DELAY)                  |  ||
||  |   -> unblocks when ISR gives                             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

二进制信号量=uxItemSize=0, uxLength=1的队列：

xSemaphoreCreateBinary()内部调用xQueueGenericCreate(1, 0, ...)。项大小=0（不存储数据），长度=1（二进制：0或1）。

"Give"=增加计数（如果不是1），"Take"=减少计数（如果已是0则阻塞）。

使用场景：ISR到任务的信号通知。ISR检测事件->xSemaphoreGiveFromISR，任务等待->xSemaphoreTake->ISR give时解除阻塞。

### Counting Semaphore

```
COUNTING SEMAPHORE = QUEUE WITH uxItemSize=0, uxLength=N:
+------------------------------------------------------------------+
|                                                                  |
|  xSemaphoreCreateCounting(maxCount, initialCount)                |
|  -> Queue with uxLength=maxCount, uxItemSize=0                   |
|                                                                  |
|  Example: Resource pool with 3 resources                         |
|                                                                  |
|  Initial: count = 3 (3 resources available)                      |
|                                                                  |
|  Task1 takes: count = 2                                          |
|  Task2 takes: count = 1                                          |
|  Task3 takes: count = 0                                          |
|  Task4 tries to take: BLOCKS (no resources)                      |
|                                                                  |
|  Task1 gives: count = 1, Task4 unblocks                          |
|                                                                  |
|  USE CASES:                                                      |
|  - Resource counting (N identical resources)                     |
|  - Event counting (count events from ISR)                        |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

计数信号量=uxItemSize=0, uxLength=N的队列：

xSemaphoreCreateCounting(maxCount, initialCount)创建uxLength=maxCount的队列。

示例：3个资源的资源池。初始count=3。Task1获取后count=2，Task2后count=1，Task3后count=0，Task4尝试获取时阻塞。Task1释放后count=1，Task4解除阻塞。

使用场景：资源计数（N个相同资源）、事件计数（计数来自ISR的事件）。

---

## 7.4 Mutexes: Special Queues with Ownership

```
MUTEX VS BINARY SEMAPHORE:
+==================================================================+
||                                                                ||
||  BINARY SEMAPHORE:                                             ||
||  +----------------------------------------------------------+  ||
||  | No ownership concept                                     |  ||
||  | Any task can give, any task can take                     |  ||
||  | Used for SIGNALING                                       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  MUTEX:                                                        ||
||  +----------------------------------------------------------+  ||
||  | Has ownership: only holder can release                   |  ||
||  | Has priority inheritance                                 |  ||
||  | Used for MUTUAL EXCLUSION                                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Queue_t for Mutex has extra fields:                           ||
||  +-------------------+                                         ||
||  |     Queue_t       |                                         ||
||  +-------------------+                                         ||
||  | ... base fields ...|                                        ||
||  +-------------------+                                         ||
||  | u.xSemaphore.     |                                         ||
||  |   xMutexHolder    |  TCB of task holding mutex              ||
||  |   uxRecursiveCall |  Recursive lock count                   ||
||  |   Count           |                                         ||
||  +-------------------+                                         ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

互斥锁vs二进制信号量：

二进制信号量：无拥有权概念，任何任务可give/take，用于信号通知。

互斥锁：有拥有权（只有持有者能释放）、有优先级继承、用于互斥。

互斥锁的Queue_t有额外字段：xMutexHolder（持有互斥锁的任务TCB）、uxRecursiveCallCount（递归锁计数）。

---

## 7.5 Priority Inheritance

```
THE PRIORITY INVERSION PROBLEM:
+==================================================================+
||                                                                ||
||  Without priority inheritance:                                 ||
||                                                                ||
||  Priority 3: TaskH (high) - needs mutex                        ||
||  Priority 2: TaskM (medium) - CPU intensive                    ||
||  Priority 1: TaskL (low) - holds mutex                         ||
||                                                                ||
||  Time -->                                                      ||
||                                                                ||
||  TaskL acquires mutex                                          ||
||  |                                                             ||
||  v                                                             ||
||  [TaskL holds mutex.........................]                  ||
||        |                                                       ||
||        v TaskH becomes ready, needs mutex                      ||
||        [TaskH BLOCKED waiting for mutex..........]             ||
||              |                                                 ||
||              v TaskM becomes ready (no mutex needed)           ||
||              [TaskM runs...............................]       ||
||                                            ^                   ||
||                                            |                   ||
||                                            TaskM keeps running ||
||                                            because TaskL (pri1)||
||                                            cannot preempt it!  ||
||                                                                ||
||  RESULT: High priority task blocked by MEDIUM priority task!   ||
||  This is PRIORITY INVERSION.                                   ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

优先级反转问题（无优先级继承）：

TaskH（优先级3，高）需要mutex，TaskM（优先级2，中）CPU密集，TaskL（优先级1，低）持有mutex。

TaskL获取mutex -> TaskH就绪需要mutex被阻塞 -> TaskM就绪（不需要mutex）开始运行 -> TaskM持续运行因为TaskL（优先级1）不能抢占它！

结果：高优先级任务被中优先级任务阻塞！这是优先级反转。

```
PRIORITY INHERITANCE SOLUTION:
+==================================================================+
||                                                                ||
||  With priority inheritance (FreeRTOS mutex):                   ||
||                                                                ||
||  Time -->                                                      ||
||                                                                ||
||  TaskL acquires mutex (priority 1)                             ||
||  |                                                             ||
||  v                                                             ||
||  [TaskL holds mutex (pri 1)]                                   ||
||        |                                                       ||
||        v TaskH needs mutex                                     ||
||        [TaskL INHERITS priority 3]                             ||
||        [TaskL runs at priority 3........]                      ||
||              |                                                 ||
||              v TaskM becomes ready (priority 2)                ||
||              [TaskM CANNOT preempt (pri 3 > pri 2)]            ||
||                    |                                           ||
||                    v TaskL releases mutex                      ||
||                    [TaskL priority restored to 1]              ||
||                    [TaskH acquires mutex, runs]                ||
||                                   |                            ||
||                                   v TaskH done                 ||
||                                   [TaskM finally runs]         ||
||                                                                ||
||  RESULT: High priority task runs as soon as possible           ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

优先级继承解决方案（FreeRTOS互斥锁）：

TaskL获取mutex（优先级1） -> TaskH需要mutex -> TaskL继承优先级3 -> TaskL以优先级3运行 -> TaskM就绪（优先级2）不能抢占（3>2） -> TaskL释放mutex，优先级恢复到1 -> TaskH获取mutex运行 -> TaskH完成后TaskM运行。

结果：高优先级任务尽快运行。

### When Priority Inheritance Matters

```
WHEN TO USE MUTEX (with priority inheritance):
+------------------------------------------------------------------+
|                                                                  |
|  GOOD USE CASES:                                                 |
|  - Protecting shared data structures                             |
|  - Short critical sections                                       |
|  - When multiple priority levels need same resource              |
|                                                                  |
|  BAD USE CASES (use semaphore instead):                          |
|  - Signaling between tasks                                       |
|  - ISR to task notification                                      |
|  - Resource counting                                             |
|                                                                  |
|  RULE OF THUMB:                                                  |
|  +------------------------------------------------------------+  |
|  | Signaling (producer/consumer): Binary semaphore            |  |
|  | Mutual exclusion (protect data): Mutex                     |  |
|  | Resource pool: Counting semaphore                          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

何时使用互斥锁（带优先级继承）：

好的用例：保护共享数据结构、短临界区、多优先级需要同一资源时。

坏的用例（用信号量替代）：任务间信号通知、ISR到任务通知、资源计数。

经验法则：信号通知用二进制信号量，互斥保护数据用Mutex，资源池用计数信号量。

---

## 7.6 Queue Internal Mechanism

```
BLOCKING ON QUEUE RECEIVE:
+==================================================================+
||                                                                ||
||  xQueueReceive(queue, &buffer, timeout) when queue empty:      ||
||                                                                ||
||  1. Enter critical section                                     ||
||  2. Check: uxMessagesWaiting == 0 (empty)                      ||
||  3. Add current task to xTasksWaitingToReceive list            ||
||  4. Set task state to Blocked                                  ||
||  5. Add to delayed list (for timeout)                          ||
||  6. Exit critical section                                      ||
||  7. Trigger context switch                                     ||
||  8. ... task sleeps ...                                        ||
||                                                                ||
||  When another task/ISR sends:                                  ||
||                                                                ||
||  9. xQueueSend sees task waiting in xTasksWaitingToReceive     ||
||  10. Removes task from waiting list                            ||
||  11. Adds task to ready list                                   ||
||  12. If higher priority, triggers context switch               ||
||                                                                ||
||  QUEUE STRUCTURE DURING BLOCKING:                              ||
||  +-------------------+                                         ||
||  |     Queue_t       |                                         ||
||  +-------------------+                                         ||
||  | uxMessagesWaiting |  = 0                                    ||
||  +-------------------+                                         ||
||  | xTasksWaitingTo   |  [TaskA] <-> [TaskB]                    ||
||  |    Receive        |  (tasks blocked on empty queue)         ||
||  +-------------------+                                         ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

队列接收时阻塞：xQueueReceive在队列空时：

1-7：进入临界区 -> 检查空 -> 加入xTasksWaitingToReceive列表 -> 设状态阻塞 -> 加入延迟列表 -> 退出临界区 -> 触发上下文切换 -> 任务睡眠。

当另一任务/ISR发送时（9-12）：xQueueSend看到等待任务 -> 从等待列表移除 -> 加入就绪列表 -> 如果更高优先级，触发上下文切换。

---

## Summary

```
QUEUES/SEMAPHORES/MUTEXES KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  UNIFYING CONCEPT:                                             ||
||  All built on Queue_t structure                                ||
||  - Queue: uxItemSize > 0 (stores data)                         ||
||  - Semaphore: uxItemSize = 0 (just count)                      ||
||  - Mutex: uxItemSize = 0 + ownership + priority inheritance    ||
||                                                                ||
||  CHOOSING THE RIGHT PRIMITIVE:                                 ||
||  +----------------------------------------------------------+  ||
||  | Need to pass data?     -> Queue                          |  ||
||  | ISR-to-task signal?    -> Binary Semaphore               |  ||
||  | Resource pool?         -> Counting Semaphore             |  ||
||  | Protect shared data?   -> Mutex                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  PRIORITY INHERITANCE:                                         ||
||  - Only mutexes have it                                        ||
||  - Prevents priority inversion                                 ||
||  - Task holding mutex inherits waiting task's priority         ||
||                                                                ||
||  FROM ISR:                                                     ||
||  - Always use FromISR variants                                 ||
||  - Check pxHigherPriorityTaskWoken                             ||
||  - Call portYIELD_FROM_ISR if needed                           ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Interrupts and ISR-Safe APIs](08-interrupts-and-isr-safe-apis.md)
