# Section 6: Time Management

Time management is fundamental to any RTOS. FreeRTOS provides time-based services through the tick mechanism, delays, and timeouts.

## 6.1 The Tick

### What the Tick Is

```
THE TICK: FREERTOS'S HEARTBEAT
+------------------------------------------------------------------+
|                                                                  |
|  Hardware Timer (SysTick on Cortex-M):                           |
|  +------------------------------------------------------------+  |
|  | Generates periodic interrupt                                |  |
|  | Frequency: configTICK_RATE_HZ (typically 100-1000 Hz)       |  |
|  +------------------------------------------------------------+  |
|                              |                                   |
|                              v                                   |
|  +------------------------------------------------------------+  |
|  | xPortSysTickHandler()                                       |  |
|  | -> Increment xTickCount                                     |  |
|  | -> Check delayed tasks                                      |  |
|  | -> Time slice handling                                      |  |
|  | -> Set PendSV if switch needed                              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Time -->                                                        |
|  |-----|-----|-----|-----|-----|-----|-----|-----|              |
|  t0    t1    t2    t3    t4    t5    t6    t7    ...            |
|  ^     ^     ^     ^     ^     ^     ^     ^                    |
|  Tick  Tick  Tick  Tick  Tick  Tick  Tick  Tick                 |
|  0     1     2     3     4     5     6     7                    |
|                                                                  |
|  One tick period = 1000ms / configTICK_RATE_HZ                   |
|  Example: 1000 Hz -> 1 tick = 1 ms                               |
|           100 Hz  -> 1 tick = 10 ms                              |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Tick是FreeRTOS的心跳。硬件定时器（Cortex-M上的SysTick）产生周期性中断，频率由configTICK_RATE_HZ定义（通常100-1000 Hz）。每次tick中断：递增xTickCount、检查延时任务、处理时间片、需要时触发上下文切换。

一个tick周期 = 1000ms / configTICK_RATE_HZ。例如：1000 Hz对应1ms，100 Hz对应10ms。

### Tick Frequency Trade-offs

```
+------------------------------------------------------------------+
|              TICK FREQUENCY TRADE-OFFS                           |
+------------------------------------------------------------------+
|                                                                  |
|  HIGH FREQUENCY (e.g., 1000 Hz):                                 |
|  +------------------------------------------------------------+  |
|  | + Fine time resolution (1 ms granularity)                  |  |
|  | + Faster response to time events                           |  |
|  | + Fairer round-robin (more frequent switches)              |  |
|  | - More CPU overhead (1000 ISRs per second)                 |  |
|  | - Higher power consumption                                 |  |
|  | - May not be achievable on slow CPUs                       |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  LOW FREQUENCY (e.g., 100 Hz):                                   |
|  +------------------------------------------------------------+  |
|  | + Lower CPU overhead                                        |  |
|  | + Lower power consumption                                   |  |
|  | + Works on slow CPUs                                        |  |
|  | - Coarse time resolution (10 ms granularity)               |  |
|  | - Slower response to time events                           |  |
|  | - Less fair round-robin                                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  TYPICAL CHOICES:                                                |
|  +------------------------------------------------------------+  |
|  | Industrial control: 1000 Hz (precise timing needed)        |  |
|  | Consumer devices:   100-500 Hz (balance)                   |  |
|  | Battery powered:    10-100 Hz (power critical)             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Tick频率权衡：

高频（如1000 Hz）：时间分辨率细（1ms粒度）、响应快、轮询更公平；但CPU开销大、功耗高、慢CPU可能无法实现。

低频（如100 Hz）：CPU开销低、功耗低、适合慢CPU；但时间分辨率粗（10ms粒度）、响应慢、轮询公平性差。

典型选择：工业控制1000 Hz（需要精确定时）、消费设备100-500 Hz（平衡）、电池供电10-100 Hz（功耗关键）。

### Tickless Idle (Power Saving)

```
TICKLESS IDLE MODE:
+------------------------------------------------------------------+
|                                                                  |
|  NORMAL MODE:                                                    |
|  +------------------------------------------------------------+  |
|  | CPU wakes for EVERY tick, even if idle                     |  |
|  |                                                             |  |
|  | Tick Tick Tick Tick Tick Tick Tick Tick                     |  |
|  |  |    |    |    |    |    |    |    |                       |  |
|  |  v    v    v    v    v    v    v    v                       |  |
|  | [Idle][Idle][Idle][Idle][Idle][Idle][Idle][Task]            |  |
|  |                                                             |  |
|  | CPU wakes 8 times, even though nothing to do until tick 7  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  TICKLESS MODE:                                                  |
|  +------------------------------------------------------------+  |
|  | CPU sleeps until next needed wake time                     |  |
|  |                                                             |  |
|  | Tick                               Tick                     |  |
|  |  |                                   |                      |  |
|  |  v                                   v                      |  |
|  | [--------- DEEP SLEEP -----------][Task]                    |  |
|  |                                                             |  |
|  | CPU wakes once, adds 7 to xTickCount to "catch up"         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  configUSE_TICKLESS_IDLE = 1 to enable                          |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

无tick空闲模式（省电）：

正常模式下，CPU每个tick都唤醒，即使空闲。8个tick唤醒8次，但可能直到第7个tick才有任务要做。

无tick模式下，CPU睡眠直到下一个需要唤醒的时间。CPU只唤醒一次，然后将7加到xTickCount来"追赶"。

设置configUSE_TICKLESS_IDLE = 1启用。需要移植层支持（配置低功耗定时器）。

---

## 6.2 Delays and Timeouts

### vTaskDelay - Relative Delay

```c
// Delay for xTicksToDelay ticks from NOW
void vTaskDelay( const TickType_t xTicksToDelay );

// Example:
vTaskDelay( 100 );  // Delay 100 ticks from now
```

```
vTaskDelay Behavior:
+------------------------------------------------------------------+
|                                                                  |
|  Current tick: 1000                                              |
|  vTaskDelay(100) called                                          |
|                                                                  |
|  Time --->                                                       |
|  |------|------|------|------|------|------|                     |
|  1000   ...    1050   ...    1100   1101                         |
|  ^                           ^                                   |
|  Called                      Task becomes READY                  |
|                              (wake tick = 1000 + 100 = 1100)     |
|                                                                  |
|  PROBLEM: Timing drift                                           |
|  +------------------------------------------------------------+  |
|  | If task processing takes variable time:                    |  |
|  |                                                             |  |
|  | Iteration 1: Process(50), Delay(100) -> Total=150          |  |
|  | Iteration 2: Process(70), Delay(100) -> Total=170          |  |
|  | Iteration 3: Process(30), Delay(100) -> Total=130          |  |
|  |                                                             |  |
|  | Period is NOT consistent 150 ticks                         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### vTaskDelayUntil - Absolute Delay

```c
// Delay until absolute tick count is reached
// Maintains consistent period regardless of processing time
BaseType_t xTaskDelayUntil( TickType_t * const pxPreviousWakeTime,
                            const TickType_t xTimeIncrement );

// Example:
TickType_t xLastWakeTime = xTaskGetTickCount();
for( ;; )
{
    // Process (variable time)
    DoWork();
    
    // Delay until 100 ticks after last wake
    xTaskDelayUntil( &xLastWakeTime, 100 );
}
```

```
vTaskDelayUntil Behavior:
+------------------------------------------------------------------+
|                                                                  |
|  Time --->                                                       |
|  |------|------|------|------|------|------|------|              |
|  0      100    200    300    400    500    600                   |
|  ^      ^      ^      ^      ^      ^      ^                     |
|  |      |      |      |      |      |      |                     |
|  Wake   Wake   Wake   Wake   Wake   Wake   Wake                  |
|  [work] [work] [work] [work] [work] [work]                       |
|  50     70     30     80     40     60     <- Variable work time |
|                                                                  |
|  Period is ALWAYS 100 ticks (absolute timing)                    |
|                                                                  |
|  Implementation:                                                 |
|  +------------------------------------------------------------+  |
|  | xLastWakeTime starts at 0                                  |  |
|  | After first delay: xLastWakeTime = 0 + 100 = 100           |  |
|  | After second: xLastWakeTime = 100 + 100 = 200              |  |
|  | Wake time is calculated from LAST wake, not from NOW       |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

vTaskDelay（相对延时）：从现在开始延时N个tick。问题是如果任务处理时间可变，总周期不一致（处理时间+延时）。

vTaskDelayUntil（绝对延时）：延时到绝对tick时刻。维持一致的周期，无论处理时间如何变化。唤醒时间从上次唤醒计算，而不是从当前时间。

用途：vTaskDelay用于简单等待；vTaskDelayUntil用于周期性任务（如传感器采样）。

### Timeout Handling

```
Timeouts in Blocking APIs:
+------------------------------------------------------------------+
|                                                                  |
|  xQueueReceive( queue, &data, timeout ):                         |
|                                                                  |
|  timeout = 0:                                                    |
|    - Check queue immediately                                     |
|    - Return immediately (pdPASS or errQUEUE_EMPTY)               |
|    - Never blocks                                                |
|                                                                  |
|  timeout = portMAX_DELAY:                                        |
|    - Block indefinitely until data available                     |
|    - Never times out (except if timeout is 32-bit max)           |
|                                                                  |
|  timeout = N (ticks):                                            |
|    - Block up to N ticks                                         |
|    - Return when: data received OR timeout expires               |
|    - Return value indicates which happened                       |
|                                                                  |
+------------------------------------------------------------------+

Timeout Implementation:
+------------------------------------------------------------------+
|                                                                  |
|  Task calls xQueueReceive(queue, &data, 100):                    |
|                                                                  |
|  1. Check if data available -> NO                                |
|  2. Calculate wake time = xTickCount + 100                       |
|  3. Add task to:                                                 |
|     - Queue's xTasksWaitingToReceive (for event)                 |
|     - xDelayedTaskList with wake time (for timeout)              |
|  4. Task blocks                                                  |
|                                                                  |
|  Wake conditions:                                                |
|  - Data arrives: Remove from both lists, return pdPASS           |
|  - Timeout: Remove from queue's list, return errQUEUE_EMPTY      |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

超时处理：FreeRTOS的阻塞API支持三种超时模式：

timeout = 0：立即检查，永不阻塞。
timeout = portMAX_DELAY：无限期阻塞直到事件发生。
timeout = N：最多阻塞N个tick，数据到达或超时返回。

超时实现：任务被加入两个列表——队列等待列表（等待数据）和延时任务列表（超时）。先满足的条件（数据到达或超时）触发唤醒。

---

## 6.3 Tick Processing

### xTaskIncrementTick (The Tick ISR Core)

```c
// Called from SysTick ISR
BaseType_t xTaskIncrementTick( void )
{
    BaseType_t xSwitchRequired = pdFALSE;
    
    // Increment tick count (with overflow handling)
    const TickType_t xConstTickCount = ++xTickCount;
    
    if( xConstTickCount == 0 )  // Overflow!
    {
        // Switch delayed task lists
        taskSWITCH_DELAYED_LISTS();
    }
    
    // Check if any delayed task should wake
    if( xConstTickCount >= xNextTaskUnblockTime )
    {
        // Process all tasks due to wake at this tick
        for( ;; )
        {
            // Get first item from delayed list
            pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
            xItemValue = listGET_LIST_ITEM_VALUE( &(pxTCB->xStateListItem) );
            
            if( xConstTickCount < xItemValue )
            {
                // This task's wake time is in the future
                xNextTaskUnblockTime = xItemValue;
                break;
            }
            
            // Remove from delayed list
            uxListRemove( &(pxTCB->xStateListItem) );
            // Also remove from any event list
            if( listLIST_ITEM_CONTAINER( &(pxTCB->xEventListItem) ) != NULL )
            {
                uxListRemove( &(pxTCB->xEventListItem) );
            }
            // Add to ready list
            prvAddTaskToReadyList( pxTCB );
            
            // Check if we need to switch
            if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
            {
                xSwitchRequired = pdTRUE;
            }
        }
    }
    
    // Time slicing: switch if another task at same priority
    #if( configUSE_TIME_SLICING == 1 )
    {
        if( listCURRENT_LIST_LENGTH( &pxReadyTasksLists[pxCurrentTCB->uxPriority] ) > 1 )
        {
            xSwitchRequired = pdTRUE;
        }
    }
    #endif
    
    return xSwitchRequired;
}
```

```
Tick Processing Flow:
+------------------------------------------------------------------+
|                                                                  |
|   SysTick fires                                                  |
|        |                                                         |
|        v                                                         |
|   xTaskIncrementTick()                                           |
|        |                                                         |
|        +-- xTickCount++                                          |
|        |                                                         |
|        +-- Overflow? -> Switch delayed lists                     |
|        |                                                         |
|        +-- xTickCount >= xNextTaskUnblockTime?                   |
|        |        |                                                |
|        |        +-- YES: Process all due tasks                   |
|        |        |        Move to ready list                      |
|        |        |        Update xNextTaskUnblockTime             |
|        |        |                                                |
|        |        +-- NO: Skip (O(1) check)                        |
|        |                                                         |
|        +-- Time slicing enabled?                                 |
|        |        |                                                |
|        |        +-- Other tasks at same priority? -> Switch      |
|        |                                                         |
|        +-- Return: Switch needed?                                |
|                 |                                                |
|                 v                                                |
|        Set PendSV if TRUE                                        |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

xTaskIncrementTick是tick ISR的核心，每次tick调用：

1. 递增xTickCount（处理溢出时切换延时列表）
2. 检查xNextTaskUnblockTime（O(1)快速检查）
3. 如果有任务到期，处理所有到期任务（从延时列表移到就绪列表）
4. 如果启用时间片且同优先级有其他任务，请求切换
5. 返回是否需要上下文切换

优化：xNextTaskUnblockTime缓存最近唤醒时间，大多数tick只需O(1)检查。

---

## Summary

```
TIME MANAGEMENT MENTAL MODEL:
+==================================================================+
||                                                                ||
||  1. Tick is the time base                                     ||
||     - Periodic interrupt (configTICK_RATE_HZ)                 ||
||     - Increments xTickCount                                   ||
||     - Checks delayed tasks, time slicing                      ||
||                                                                ||
||  2. vTaskDelay: Relative delay (from now)                     ||
||     - Simple but causes timing drift                          ||
||                                                                ||
||  3. vTaskDelayUntil: Absolute delay (consistent period)       ||
||     - For periodic tasks                                      ||
||                                                                ||
||  4. Timeouts: Bounded blocking                                ||
||     - Task in both event list AND delayed list                ||
||     - First condition wins                                    ||
||                                                                ||
||  5. Tickless idle: Sleep through idle periods                 ||
||     - Power saving for battery devices                        ||
||                                                                ||
+==================================================================+
```

| API | Use Case | Period Consistency |
|-----|----------|-------------------|
| `vTaskDelay(N)` | Simple wait | No (drifts) |
| `vTaskDelayUntil(&last, N)` | Periodic task | Yes |
| `xQueueReceive(..., timeout)` | Bounded wait | N/A |
| `portMAX_DELAY` | Infinite wait | N/A |

**Next Section**: [Inter-Task Communication](07-inter-task-communication.md)
