# Section 12: Using FreeRTOS in Real Projects

This section covers practical application of FreeRTOS in production embedded systems.

## 12.1 Typical Application Architecture

### Recommended Startup Structure

```c
/*
 * RECOMMENDED APPLICATION STRUCTURE
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Hardware-specific includes */
#include "board.h"

/* Task handles */
static TaskHandle_t xSensorTaskHandle = NULL;
static TaskHandle_t xProcessTaskHandle = NULL;
static TaskHandle_t xCommTaskHandle = NULL;

/* Queues */
static QueueHandle_t xSensorQueue = NULL;
static QueueHandle_t xCommQueue = NULL;

/*-----------------------------------------------------------*/

int main( void )
{
    /* 1. Hardware initialization (clocks, peripherals) */
    Board_Init();
    
    /* 2. Create kernel objects BEFORE starting scheduler */
    xSensorQueue = xQueueCreate( 10, sizeof( SensorData_t ) );
    xCommQueue = xQueueCreate( 5, sizeof( CommMessage_t ) );
    
    configASSERT( xSensorQueue != NULL );
    configASSERT( xCommQueue != NULL );
    
    /* 3. Create tasks */
    xTaskCreate( vSensorTask, "Sensor", 256, NULL, 3, &xSensorTaskHandle );
    xTaskCreate( vProcessTask, "Process", 512, NULL, 2, &xProcessTaskHandle );
    xTaskCreate( vCommTask, "Comm", 256, NULL, 1, &xCommTaskHandle );
    
    /* 4. Start the scheduler - NEVER RETURNS */
    vTaskStartScheduler();
    
    /* 5. Should never reach here */
    for( ;; )
    {
        /* Error: scheduler failed to start */
    }
    
    return 0;  /* Never reached */
}
```

```
APPLICATION STARTUP FLOW:
+------------------------------------------------------------------+
|                                                                  |
|   Power On / Reset                                               |
|        |                                                         |
|        v                                                         |
|   Hardware Init (clocks, pins, peripherals)                      |
|        |                                                         |
|        v                                                         |
|   Create kernel objects (queues, semaphores, mutexes)            |
|        |                                                         |
|        v                                                         |
|   Create tasks                                                   |
|        |                                                         |
|        v                                                         |
|   vTaskStartScheduler()                                          |
|        |                                                         |
|        +-------> Creates idle task                               |
|        |         Creates timer task (if enabled)                 |
|        |         Starts tick timer                               |
|        |         Starts first task                               |
|        |                                                         |
|        X         main() NEVER RETURNS                            |
|                                                                  |
|   Task execution begins...                                       |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

推荐的启动结构：
1. 硬件初始化（时钟、引脚、外设）
2. 创建内核对象（队列、信号量、互斥锁）——在启动调度器之前
3. 创建任务
4. 启动调度器（vTaskStartScheduler）——永不返回
5. main永远不会返回，如果返回说明出错

---

## 12.2 Common Design Patterns

### One Task Per Responsibility

```
RECOMMENDED: SEPARATION OF CONCERNS
+------------------------------------------------------------------+
|                                                                  |
|  [Sensor Task]  --->  [Processing Task]  --->  [Comm Task]       |
|                                                                  |
|  Responsibility:      Responsibility:         Responsibility:    |
|  - Read hardware      - Algorithm             - Protocol         |
|  - Timing             - Decisions             - Networking       |
|  - Raw data           - Filtering             - Buffering        |
|                                                                  |
+------------------------------------------------------------------+

NOT RECOMMENDED: MONOLITHIC TASK
+------------------------------------------------------------------+
|                                                                  |
|  [Monster Task]                                                  |
|  - Read sensors                                                  |
|  - Process data                                                  |
|  - Handle communication                                          |
|  - Update display                                                |
|  - ...everything                                                 |
|                                                                  |
|  Problems:                                                       |
|  - Hard to test                                                  |
|  - Hard to maintain                                              |
|  - Blocking in one area affects all others                       |
|  - No clear priority structure                                   |
|                                                                  |
+------------------------------------------------------------------+
```

### Message-Passing Over Shared Memory

```
RECOMMENDED: MESSAGE PASSING
+------------------------------------------------------------------+
|                                                                  |
|  [Producer Task]                    [Consumer Task]              |
|        |                                  |                      |
|        | xQueueSend(data)                 | xQueueReceive(data)  |
|        v                                  v                      |
|  +--------------------------------------------------+            |
|  |                    QUEUE                         |            |
|  |  [ data ] [ data ] [ data ] ...                  |            |
|  +--------------------------------------------------+            |
|                                                                  |
|  - Data is COPIED into queue                                     |
|  - No shared state                                               |
|  - No mutex needed                                               |
|  - Easy to reason about                                          |
|                                                                  |
+------------------------------------------------------------------+

AVOID: SHARED MEMORY WITH MUTEX
+------------------------------------------------------------------+
|                                                                  |
|  [Task A]              [Shared Data]              [Task B]       |
|     |                      |                          |          |
|     | xMutexTake()         |           xMutexTake()   |          |
|     |--------------------->|<-------------------------|          |
|     | read/write           |           read/write     |          |
|     | xMutexGive()         |           xMutexGive()   |          |
|                                                                  |
|  Problems:                                                       |
|  - Priority inversion risk                                       |
|  - Deadlock risk (if multiple mutexes)                           |
|  - Harder to reason about                                        |
|  - Bugs appear far from cause                                    |
|                                                                  |
+------------------------------------------------------------------+
```

### ISR -> Queue -> Worker Task Pattern

```
THE CLASSIC PATTERN:
+------------------------------------------------------------------+
|                                                                  |
|  Hardware          ISR                 Queue        Worker Task  |
|                                                                  |
|  [Interrupt]  -> [Read HW]  ->  [Queue]  ->  [Process]           |
|                  [Send to Q]                 [Decide]            |
|                  [Yield if                   [Act]               |
|                   needed]                                        |
|                                                                  |
|  ISR: Minimal work (~10 instructions)                            |
|  Queue: Decouples ISR from processing                            |
|  Task: Can take as long as needed                                |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

每个职责一个任务：传感器任务读硬件、处理任务运算、通信任务处理协议。避免单一巨型任务做所有事情。

消息传递优于共享内存：使用队列传递数据，数据被复制，无共享状态，无需互斥锁。避免使用互斥锁保护共享数据——优先级反转风险、死锁风险、难以推理。

ISR -> 队列 -> 工作任务模式：ISR最小化工作（读硬件、发送队列、yield），队列解耦ISR和处理，任务可以花任意时间处理。

---

## 12.3 Common Beginner Mistakes

### Too Many Tasks

```
MISTAKE: TASK PER FUNCTION
+------------------------------------------------------------------+
|                                                                  |
|  BAD:                                                            |
|  +-------------+  +-------------+  +-------------+               |
|  | Read Temp   |  | Read Humid  |  | Read Press  |               |
|  | Task        |  | Task        |  | Task        |               |
|  +-------------+  +-------------+  +-------------+               |
|  3 tasks, 3 stacks, 3 context switches for related work          |
|                                                                  |
|  GOOD:                                                           |
|  +----------------------------------------+                      |
|  | Sensor Task                            |                      |
|  | - Read all sensors in sequence         |                      |
|  | - One stack, one context               |                      |
|  +----------------------------------------+                      |
|                                                                  |
+------------------------------------------------------------------+
```

### Wrong Priorities

```
MISTAKE: ARBITRARY PRIORITIES
+------------------------------------------------------------------+
|                                                                  |
|  BAD:                                                            |
|  Display Task:   Priority 5 (high)    <- Why high?               |
|  Sensor Task:    Priority 1 (low)     <- Critical data?          |
|  Safety Task:    Priority 2           <- Should be highest!      |
|                                                                  |
|  GOOD (Priority by criticality):                                 |
|  Safety Task:    Priority 5 (highest) <- Most critical           |
|  Motor Control:  Priority 4           <- Time sensitive          |
|  Sensor Task:    Priority 3           <- Important data          |
|  Communication:  Priority 2           <- Can wait                |
|  Display Task:   Priority 1 (lowest)  <- User can wait           |
|  Idle:           Priority 0           <- Only when nothing else  |
|                                                                  |
+------------------------------------------------------------------+
```

### Blocking in ISRs

```
MISTAKE: ANY BLOCKING IN ISR
+------------------------------------------------------------------+
|                                                                  |
|  BAD:                                                            |
|  void UART_IRQHandler(void)                                      |
|  {                                                               |
|      data = UART->DATA;                                          |
|      xQueueSend(q, &data, portMAX_DELAY);  // <-- BLOCKS!        |
|  }                                                               |
|                                                                  |
|  GOOD:                                                           |
|  void UART_IRQHandler(void)                                      |
|  {                                                               |
|      BaseType_t xHigherPriorityTaskWoken = pdFALSE;              |
|      data = UART->DATA;                                          |
|      xQueueSendFromISR(q, &data, &xHigherPriorityTaskWoken);     |
|      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);               |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+
```

### Stack Overflows

```
MISTAKE: UNDERSIZED STACKS
+------------------------------------------------------------------+
|                                                                  |
|  xTaskCreate(..., 64, ...);  // Only 64 words = 256 bytes        |
|                                                                  |
|  Task does:                                                      |
|  - sprintf() with format string    (100+ bytes stack)            |
|  - Nested function calls           (varies)                      |
|  - Local arrays                    (as declared)                 |
|                                                                  |
|  RESULT: Silent corruption, random crashes                       |
|                                                                  |
|  PREVENTION:                                                     |
|  - Enable configCHECK_FOR_STACK_OVERFLOW = 2                     |
|  - Use uxTaskGetStackHighWaterMark() to tune                     |
|  - Start with larger stacks, reduce after testing                |
|                                                                  |
+------------------------------------------------------------------+
```

### Overusing Mutexes

```
MISTAKE: MUTEX FOR EVERYTHING
+------------------------------------------------------------------+
|                                                                  |
|  BAD:                                                            |
|  Mutex for: shared flag            <- Use atomic or queue        |
|  Mutex for: counter                <- Use atomic                 |
|  Mutex for: single-writer data     <- Use queue or notification  |
|                                                                  |
|  Mutexes add:                                                    |
|  - Priority inversion risk                                       |
|  - Blocking overhead                                             |
|  - Complexity                                                    |
|                                                                  |
|  GOOD: Use mutex ONLY for:                                       |
|  - True mutual exclusion (single hardware resource)              |
|  - Complex multi-field updates                                   |
|  - When simpler mechanisms won't work                            |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

常见错误：

任务过多：不要为每个小功能创建任务，相关功能放在同一任务中。

优先级错误：按关键性设置优先级，安全任务最高，显示任务最低。

ISR中阻塞：在ISR中绝不使用阻塞调用，使用FromISR版本。

栈溢出：不要低估栈大小，启用溢出检测，使用watermark调优。

过度使用互斥锁：优先使用队列和通知，互斥锁仅用于真正的互斥场景。

---

## Summary

```
REAL PROJECT GUIDELINES:
+==================================================================+
||                                                                ||
||  ARCHITECTURE:                                                 ||
||  - One task per responsibility                                ||
||  - Message passing over shared memory                         ||
||  - ISR -> Queue -> Task pattern                              ||
||                                                                ||
||  STARTUP:                                                     ||
||  - Init HW -> Create objects -> Create tasks -> Start         ||
||  - main() never returns                                       ||
||                                                                ||
||  COMMON MISTAKES TO AVOID:                                    ||
||  - Too many tasks                                             ||
||  - Wrong priorities                                           ||
||  - Blocking in ISRs                                           ||
||  - Undersized stacks                                          ||
||  - Mutex overuse                                              ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Debugging and Observability](13-debugging-and-observability.md)
