# Section 13: Debugging and Observability

Debugging RTOS applications is challenging because execution order depends on scheduling and events. FreeRTOS provides several tools to help understand system behavior.

## 13.1 How to Debug Task Issues

### Common Task Problems and Diagnosis

```
TASK DEBUGGING CHECKLIST:
+------------------------------------------------------------------+
|                                                                  |
|  SYMPTOM: Task never runs                                        |
|  +------------------------------------------------------------+  |
|  | Check: Was task created successfully? (check return value) |  |
|  | Check: Is priority correct? (higher task always ready?)    |  |
|  | Check: Is task blocked waiting for something?              |  |
|  | Check: Is scheduler running? (vTaskStartScheduler called?) |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  SYMPTOM: Task runs but crashes                                  |
|  +------------------------------------------------------------+  |
|  | Check: Stack overflow? (enable stack checking)             |  |
|  | Check: Null pointer dereference?                           |  |
|  | Check: Using task API in ISR? (need FromISR version)       |  |
|  | Check: Blocking in critical section?                       |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  SYMPTOM: Task appears to freeze                                 |
|  +------------------------------------------------------------+  |
|  | Check: Deadlock? (waiting for mutex held by blocked task)  |  |
|  | Check: Infinite loop in task code?                         |  |
|  | Check: Waiting on queue that never receives data?          |  |
|  | Check: Lower priority than running task?                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  SYMPTOM: System timing is wrong                                 |
|  +------------------------------------------------------------+  |
|  | Check: configTICK_RATE_HZ setting                          |  |
|  | Check: Timer interrupt priority (must be low enough)       |  |
|  | Check: Using vTaskDelay vs vTaskDelayUntil?                |  |
|  | Check: Long critical sections blocking ticks?              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### Using Task State Information

```c
/* Get task state programmatically */
eTaskState eTaskGetState( TaskHandle_t xTask );

/* Returns one of:
 * eRunning   - Task is executing
 * eReady     - Task is ready to run
 * eBlocked   - Task is blocked (delay, queue, semaphore)
 * eSuspended - Task is suspended
 * eDeleted   - Task has been deleted but memory not freed
 */

/* Get detailed task information */
void vTaskGetInfo( TaskHandle_t xTask,
                   TaskStatus_t *pxTaskStatus,
                   BaseType_t xGetFreeStackSpace,
                   eTaskState eState );

/* TaskStatus_t contains:
 * - Task handle, name, number
 * - Current state, priority, base priority
 * - Stack high water mark
 * - Runtime counter
 */
```

**Chinese Explanation (中文说明):**

调试任务问题清单：

任务不运行：检查创建是否成功、优先级是否正确、是否阻塞、调度器是否启动。

任务崩溃：检查栈溢出、空指针、ISR中使用错误API、临界区中阻塞。

任务冻结：检查死锁、无限循环、等待永不到来的数据、优先级问题。

时序错误：检查tick频率、定时器中断优先级、delay类型、长临界区。

---

## 13.2 Stack Watermarking

### High Water Mark Concept

```
STACK WATERMARKING:
+------------------------------------------------------------------+
|                                                                  |
|  Stack filled with 0xA5 at task creation:                        |
|                                                                  |
|  Stack Top (low address):                                        |
|  +----------------------------------------------------------+    |
|  | [A5][A5][A5][A5][A5][A5][A5][A5] <- Unused (fill pattern) |    |
|  +----------------------------------------------------------+    |
|  | [A5][A5][A5][A5][A5][A5][A5][A5] <- Unused                |    |
|  +----------------------------------------------------------+    |
|  | [xx][xx][xx][xx][xx][xx][xx][xx] <- Used (modified)       |    |
|  +----------------------------------------------------------+    |
|  | [xx][xx][xx][xx][xx][xx][xx][xx] <- Used                  |    |
|  +----------------------------------------------------------+    |
|  Stack Bottom (high address):                                    |
|                                                                  |
|  High Water Mark = boundary between A5 and used                  |
|  (closest point to overflow)                                     |
|                                                                  |
+------------------------------------------------------------------+
```

### Using Stack Watermark API

```c
/* Get minimum ever free stack space (in words) */
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask );

/* Example usage */
void vMonitorTask( void *pvParameters )
{
    UBaseType_t uxHighWaterMark;
    
    for( ;; )
    {
        /* Check this task's stack */
        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        
        if( uxHighWaterMark < 20 )  /* Less than 20 words free */
        {
            /* WARNING: Stack nearly full! */
            Log( "Stack warning: %d words free", uxHighWaterMark );
        }
        
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}
```

```
WATERMARK INTERPRETATION:
+------------------------------------------------------------------+
|                                                                  |
|  High Water Mark      | Interpretation                           |
|  ---------------------+------------------------------------------+
|  > 50% of stack       | Stack oversized, can reduce              |
|  25-50% of stack      | Good safety margin                       |
|  10-25% of stack      | Acceptable, monitor carefully            |
|  < 10% of stack       | DANGER: Increase stack size!             |
|  0                    | OVERFLOW OCCURRED!                       |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

栈水印：任务创建时用0xA5填充栈。运行时，使用过的部分被覆盖。高水位标记是0xA5和已使用区域的边界——最接近溢出的点。

uxTaskGetStackHighWaterMark返回最小空闲栈空间（字数）。解释：>50%栈过大可减小，25-50%良好，10-25%可接受需监控，<10%危险需增大，0表示已溢出。

---

## 13.3 Trace Tools (Conceptually)

### Built-in Trace Macros

```
FREERTOS TRACE MACROS (in FreeRTOS.h):
+------------------------------------------------------------------+
|                                                                  |
|  traceTASK_SWITCHED_IN()      - Called when task starts running  |
|  traceTASK_SWITCHED_OUT()     - Called when task stops running   |
|  traceTASK_CREATE()           - Called when task is created      |
|  traceTASK_DELETE()           - Called when task is deleted      |
|  traceQUEUE_SEND()            - Called when queue send succeeds  |
|  traceQUEUE_RECEIVE()         - Called when queue receive works  |
|  traceBLOCKING_ON_QUEUE_SEND()- Called when blocked on send      |
|  ...and many more                                                |
|                                                                  |
|  Define these macros to capture trace data:                      |
|  #define traceTASK_SWITCHED_IN() MyTraceFunction(pxCurrentTCB)   |
|                                                                  |
+------------------------------------------------------------------+
```

### Commercial Trace Tools

```
TRACE TOOL OPTIONS:
+------------------------------------------------------------------+
|                                                                  |
|  Percepio Tracealyzer:                                           |
|  - Visual timeline of task execution                             |
|  - CPU load analysis                                             |
|  - Object usage graphs                                           |
|  - Event history                                                 |
|                                                                  |
|  SEGGER SystemView:                                              |
|  - Real-time recording                                           |
|  - Timeline visualization                                        |
|  - CPU profiling                                                 |
|  - Integration with SEGGER debuggers                             |
|                                                                  |
+------------------------------------------------------------------+
```

### Runtime Statistics

```c
/* Enable with configGENERATE_RUN_TIME_STATS = 1 */

/* Configure timer for stats (must be 10-20x tick frequency) */
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()  ConfigureTimerForStats()
#define portGET_RUN_TIME_COUNTER_VALUE()          GetTimerValue()

/* Get stats for all tasks */
void vTaskGetRunTimeStats( char *pcWriteBuffer );

/* Example output:
 * Task            Abs Time      % Time
 * ****************************************
 * IDLE            1234567       45%
 * Sensor          456789        17%
 * Process         345678        13%
 * Comm            234567         9%
 */
```

**Chinese Explanation (中文说明):**

跟踪宏：FreeRTOS提供跟踪宏（如traceTASK_SWITCHED_IN），可定义这些宏来捕获跟踪数据。

商业工具：Percepio Tracealyzer和SEGGER SystemView提供可视化时间线、CPU负载分析、事件历史等。

运行时统计：启用configGENERATE_RUN_TIME_STATS，配置高频定时器，使用vTaskGetRunTimeStats获取每个任务的CPU使用率。

---

## Summary

```
DEBUGGING MENTAL MODEL:
+==================================================================+
||                                                                ||
||  1. PREVENTIVE: Enable stack overflow checking                ||
||                                                                ||
||  2. DIAGNOSTIC: Use stack watermarking                        ||
||     - uxTaskGetStackHighWaterMark()                           ||
||                                                                ||
||  3. VISIBILITY: Use runtime stats                             ||
||     - vTaskGetRunTimeStats()                                  ||
||     - vTaskList()                                             ||
||                                                                ||
||  4. DEEP ANALYSIS: Trace tools                                ||
||     - Tracealyzer, SystemView                                 ||
||     - Trace macros for custom logging                         ||
||                                                                ||
||  5. DEBUGGING APPROACH:                                       ||
||     - Reproduce consistently                                  ||
||     - Check task states                                       ||
||     - Check stack usage                                       ||
||     - Check for blocking/deadlock                             ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Mental Model Summary](14-mental-model-summary.md)
