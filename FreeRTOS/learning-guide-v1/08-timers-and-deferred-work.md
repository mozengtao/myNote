# Section 8: Timers and Deferred Work

Software timers allow scheduling work to run at a future time without dedicating a task to timing. Understanding how timers work internally reveals key FreeRTOS design patterns.

## 8.1 Software Timers

### Timer Architecture

```
TIMER SYSTEM ARCHITECTURE:
+------------------------------------------------------------------+
|                                                                  |
|  Application Tasks:                                              |
|  +--------------------+  +--------------------+                   |
|  | Task A             |  | Task B             |                   |
|  | xTimerStart(t1)    |  | xTimerStop(t2)     |                   |
|  +--------+-----------+  +--------+-----------+                   |
|           |                       |                              |
|           v                       v                              |
|  +------------------------------------------------------------+  |
|  |              Timer Command Queue                            |  |
|  |  [START,t1,now] -> [STOP,t2] -> [RESET,t3] -> ...          |  |
|  +---------------------------+--------------------------------+  |
|                              |                                   |
|                              v                                   |
|  +------------------------------------------------------------+  |
|  |              Timer Daemon Task (prvTimerTask)               |  |
|  |  - Runs at configTIMER_TASK_PRIORITY                        |  |
|  |  - Blocks on command queue                                  |  |
|  |  - Processes commands                                       |  |
|  |  - Calls callbacks in TASK CONTEXT                          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

定时器系统架构：应用任务调用定时器API（如xTimerStart）时，命令被发送到定时器命令队列。定时器守护任务（prvTimerTask）以configTIMER_TASK_PRIORITY优先级运行，阻塞等待命令队列，处理命令并在任务上下文中调用回调函数。

### Timer Service Task (Daemon)

```
TIMER DAEMON TASK (prvTimerTask in timers.c):
+------------------------------------------------------------------+
|                                                                  |
|  static portTASK_FUNCTION( prvTimerTask, pvParameters )          |
|  {                                                               |
|      for( ;; )                                                   |
|      {                                                           |
|          // Get next expiry time                                 |
|          xNextExpireTime = prvGetNextExpireTime(&xListWasEmpty); |
|                                                                  |
|          // Block until: command arrives OR timer expires        |
|          prvProcessTimerOrBlockTask(xNextExpireTime, xListWasEmpty); |
|                                                                  |
|          // Process any commands in queue                        |
|          prvProcessReceivedCommands();                           |
|      }                                                           |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+

DAEMON TASK FLOW:
+------------------------------------------------------------------+
|                                                                  |
|   Start                                                          |
|     |                                                            |
|     v                                                            |
|   Get next timer expiry time                                     |
|     |                                                            |
|     v                                                            |
|   +-----------------------------------+                          |
|   | Block on command queue with       |                          |
|   | timeout = time until next expiry  |                          |
|   +----------------+------------------+                          |
|                    |                                             |
|        +-----------+-----------+                                 |
|        |                       |                                 |
|        v                       v                                 |
|   Command arrived         Timeout (timer expired)                |
|        |                       |                                 |
|        v                       v                                 |
|   Process command         Call timer callback                    |
|   (start/stop/reset)      Reload if auto-reload                  |
|        |                       |                                 |
|        +-----------+-----------+                                 |
|                    |                                             |
|                    v                                             |
|              Loop back                                           |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

定时器守护任务循环流程：
1. 获取下一个定时器到期时间
2. 阻塞在命令队列上，超时时间=距离下一次到期的时间
3. 如果命令到达，处理命令（启动/停止/重置）
4. 如果超时（定时器到期），调用回调函数，如果是自动重载则重新加载
5. 循环

### Why Callbacks Run in Task Context

```
ISR CONTEXT vs TASK CONTEXT:
+------------------------------------------------------------------+
|                                                                  |
|  IF CALLBACKS RAN IN ISR (Hardware Timer ISR):                   |
|  +------------------------------------------------------------+  |
|  | RESTRICTIONS:                                               |  |
|  | - Cannot call blocking functions                           |  |
|  | - Must be SHORT (delays all other interrupts)              |  |
|  | - Must use FromISR API only                                |  |
|  | - Limited stack (ISR stack)                                |  |
|  | - Cannot access some peripherals                           |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WITH DAEMON TASK (FreeRTOS approach):                           |
|  +------------------------------------------------------------+  |
|  | FREEDOMS:                                                   |  |
|  | - CAN call any API (but shouldn't block long)              |  |
|  | - Can do more work                                         |  |
|  | - Uses daemon task's stack (configurable)                  |  |
|  | - Can be preempted if higher priority task wakes           |  |
|  | - Simpler programming model                                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+

DESIGN TRADE-OFF:
+------------------------------------------------------------------+
|                                                                  |
|  Latency: ISR callback is faster (no task switch)                |
|  Flexibility: Task callback is more flexible                     |
|                                                                  |
|  FreeRTOS chose FLEXIBILITY because:                             |
|  - Most timer callbacks do significant work                      |
|  - Blocking in callbacks is common (send to queue, etc.)         |
|  - ISR execution time should be minimized                        |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么回调在任务上下文中运行？

如果在ISR中运行：不能调用阻塞函数、必须短小（延迟所有其他中断）、只能使用FromISR API、栈有限。

在守护任务中运行：可以调用任何API（但不应长时间阻塞）、可以做更多工作、使用可配置的任务栈、可以被更高优先级任务抢占、编程模型更简单。

FreeRTOS选择灵活性是因为：大多数定时器回调做重要工作，回调中阻塞很常见（如发送到队列），ISR执行时间应该最小化。

### Timer Structure

```
Timer_t (from timers.c):
+------------------------------------------------------------------+
|                                                                  |
|  typedef struct tmrTimerControl                                  |
|  {                                                               |
|      const char *pcTimerName;        // For debugging            |
|      ListItem_t xTimerListItem;      // Links to active list     |
|      TickType_t xTimerPeriodInTicks; // Timer period             |
|      void *pvTimerID;                // User ID                  |
|      TimerCallbackFunction_t pxCallbackFunction;                 |
|      uint8_t ucStatus;               // Active, auto-reload flags|
|  } Timer_t;                                                      |
|                                                                  |
+------------------------------------------------------------------+

ACTIVE TIMER LIST (sorted by expiry time):
+------------------------------------------------------------------+
|                                                                  |
|  xActiveTimerList:                                               |
|                                                                  |
|  xListEnd <-> [expiry=100] <-> [expiry=150] <-> [expiry=300]     |
|                    ^                                             |
|                    First to expire                               |
|                                                                  |
|  When tick reaches 100:                                          |
|  - Remove first timer                                            |
|  - Call its callback                                             |
|  - If auto-reload: Re-insert with expiry = 100 + period          |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 8.2 Timer Commands

### Command Types

```
TIMER COMMANDS:
+------------------------------------------------------------------+
|                                                                  |
|  Command          | Action                                       |
|  -----------------+----------------------------------------------+
|  tmrCOMMAND_START | Add timer to active list                     |
|  tmrCOMMAND_STOP  | Remove timer from active list                |
|  tmrCOMMAND_RESET | Restart timer (stop + start)                 |
|  tmrCOMMAND_CHANGE_PERIOD | Change period and restart           |
|  tmrCOMMAND_DELETE | Delete timer                                |
|                                                                  |
+------------------------------------------------------------------+

WHY COMMANDS (not direct manipulation)?
+------------------------------------------------------------------+
|                                                                  |
|  1. THREAD SAFETY:                                               |
|     - Multiple tasks can send commands concurrently              |
|     - Queue serializes access to timer lists                     |
|     - No mutex needed in application code                        |
|                                                                  |
|  2. ISR COMPATIBILITY:                                           |
|     - xTimerStartFromISR() sends command from ISR                |
|     - Command is processed later by daemon task                  |
|     - ISR doesn't manipulate timer lists directly                |
|                                                                  |
|  3. DEFERRED EXECUTION:                                          |
|     - Start time can be specified (not just "now")               |
|     - Command includes timestamp for accurate timing             |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

定时器命令类型：START（添加到活动列表）、STOP（从活动列表移除）、RESET（停止+启动）、CHANGE_PERIOD（改变周期并重启）、DELETE（删除定时器）。

为什么使用命令而不是直接操作？
1. 线程安全：多任务可以并发发送命令，队列序列化对定时器列表的访问
2. ISR兼容：xTimerStartFromISR从ISR发送命令，守护任务稍后处理
3. 延迟执行：可以指定启动时间，命令包含时间戳以确保精确定时

---

## Summary

```
TIMER MENTAL MODEL:
+==================================================================+
||                                                                ||
||  1. Timers are NOT hardware timers                            ||
||     - Built on top of the tick                                ||
||     - Resolution = tick period                                ||
||                                                                ||
||  2. Command-based architecture                                ||
||     - API sends command to queue                              ||
||     - Daemon task processes commands                          ||
||     - Thread-safe, ISR-safe                                   ||
||                                                                ||
||  3. Callbacks run in TASK context                             ||
||     - Can use full API (mostly)                               ||
||     - Don't block for long (starves other timers)             ||
||                                                                ||
||  4. Daemon task priority matters                              ||
||     - Higher = more responsive timers                         ||
||     - Lower = less impact on other tasks                      ||
||                                                                ||
||  5. One-shot vs Auto-reload                                   ||
||     - One-shot: Fires once, then stops                        ||
||     - Auto-reload: Fires repeatedly at period                 ||
||                                                                ||
+==================================================================+
```

| Timer Type | Behavior | Use Case |
|------------|----------|----------|
| One-shot | Fires once, stops | Timeout, delayed action |
| Auto-reload | Fires repeatedly | Periodic task, polling |

**Next Section**: [Interrupts and FreeRTOS](09-interrupts-and-freertos.md)
