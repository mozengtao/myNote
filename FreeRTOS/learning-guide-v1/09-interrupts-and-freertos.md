# Section 9: Interrupts and FreeRTOS

Interrupts are the interface between hardware and software. Understanding how FreeRTOS interacts with interrupts is critical for writing correct, real-time systems.

## 9.1 ISR Rules (Critical)

### The Fundamental Rule

```
+==================================================================+
||                                                                ||
||  ISR CODE MUST NEVER BLOCK                                    ||
||                                                                ||
||  - No vTaskDelay()                                            ||
||  - No xQueueReceive() with timeout > 0                        ||
||  - No xSemaphoreTake() with timeout > 0                       ||
||  - No waiting for ANYTHING                                    ||
||                                                                ||
||  WHY: ISR runs with interrupts disabled (or priority masked)  ||
||       Blocking would freeze the system                        ||
||                                                                ||
+==================================================================+
```

### FromISR API Functions

```
TASK API vs ISR API:
+------------------------------------------------------------------+
|                                                                  |
|  TASK CONTEXT                    | ISR CONTEXT                   |
|  (can block)                     | (must NOT block)              |
|  --------------------------------+-------------------------------+
|  xQueueSend()                    | xQueueSendFromISR()           |
|  xQueueReceive()                 | xQueueReceiveFromISR()        |
|  xSemaphoreGive()                | xSemaphoreGiveFromISR()       |
|  xSemaphoreTake()                | xSemaphoreTakeFromISR()       |
|  xEventGroupSetBits()            | xEventGroupSetBitsFromISR()   |
|  xTaskNotifyGive()               | vTaskNotifyGiveFromISR()      |
|  xStreamBufferSend()             | xStreamBufferSendFromISR()    |
|                                                                  |
+------------------------------------------------------------------+

FromISR FUNCTION SIGNATURE:
+------------------------------------------------------------------+
|                                                                  |
|  BaseType_t xQueueSendFromISR(                                   |
|      QueueHandle_t xQueue,                                       |
|      const void *pvItemToQueue,                                  |
|      BaseType_t *pxHigherPriorityTaskWoken  // <-- CRITICAL      |
|  );                                                              |
|                                                                  |
|  pxHigherPriorityTaskWoken:                                      |
|  - Set to pdTRUE if a higher priority task was woken             |
|  - ISR should yield if TRUE (request context switch)             |
|  - Enables immediate response to high-priority events            |
|                                                                  |
+------------------------------------------------------------------+
```

### Correct ISR Pattern

```c
// CORRECT ISR PATTERN:
void UART_IRQHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t data;
    
    // Read data from hardware
    data = UART->DATA;
    
    // Send to queue (non-blocking)
    xQueueSendFromISR( xUartQueue, &data, &xHigherPriorityTaskWoken );
    
    // Clear interrupt flag
    UART->INT_CLEAR = 1;
    
    // Yield if a higher priority task was woken
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
```

```
ISR EXECUTION FLOW:
+------------------------------------------------------------------+
|                                                                  |
|  Hardware Event                                                  |
|       |                                                          |
|       v                                                          |
|  +----------------+                                              |
|  | ISR runs       |                                              |
|  | - Read HW      |                                              |
|  | - xQueue...    |----> pxHigherPriorityTaskWoken = pdTRUE      |
|  |   FromISR()    |      (woke a task higher than current)       |
|  | - Clear IRQ    |                                              |
|  +-------+--------+                                              |
|          |                                                       |
|          v                                                       |
|  +----------------+                                              |
|  | portYIELD_     |                                              |
|  | FROM_ISR()     |----> Sets PendSV (if xHigher... == pdTRUE)   |
|  +-------+--------+                                              |
|          |                                                       |
|          v                                                       |
|  ISR returns, PendSV fires (lowest priority)                     |
|          |                                                       |
|          v                                                       |
|  Context switch to higher priority task                          |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

ISR基本规则：ISR代码绝不能阻塞。没有vTaskDelay、没有带超时的xQueueReceive、没有等待任何东西。原因：ISR在中断禁用或优先级屏蔽的情况下运行，阻塞会冻结系统。

FromISR API：任务上下文API可以阻塞，ISR上下文必须使用FromISR版本。关键参数pxHigherPriorityTaskWoken：如果唤醒了更高优先级任务则设为pdTRUE，ISR应该在结束时yield。

正确的ISR模式：读硬件数据、用FromISR发送到队列、清除中断标志、调用portYIELD_FROM_ISR。

---

## 9.2 Interrupt Priority Rules

### Cortex-M Priority Model

```
CORTEX-M INTERRUPT PRIORITIES:
+------------------------------------------------------------------+
|                                                                  |
|  Priority NUMBER vs Priority LEVEL:                              |
|  +------------------------------------------------------------+  |
|  | LOWER number = HIGHER priority (confusing!)                |  |
|  |                                                             |  |
|  | Priority 0 = Highest (NMI, HardFault)                      |  |
|  | Priority 1 = Very high                                     |  |
|  | ...                                                         |  |
|  | Priority 255 = Lowest                                      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  configMAX_SYSCALL_INTERRUPT_PRIORITY:                           |
|  +------------------------------------------------------------+  |
|  | Interrupts with priority NUMBER >= this value can call     |  |
|  | FreeRTOS FromISR functions                                 |  |
|  |                                                             |  |
|  | Example: configMAX_SYSCALL_INTERRUPT_PRIORITY = 5          |  |
|  |                                                             |  |
|  | Priority 0-4: CANNOT call FreeRTOS (too high priority)     |  |
|  | Priority 5-255: CAN call FreeRTOS FromISR functions        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+

PRIORITY SPLIT:
+------------------------------------------------------------------+
|                                                                  |
|   Priority 0   |  NMI, HardFault (reserved)                      |
|   Priority 1   |                                                 |
|   Priority 2   |  Cannot use FreeRTOS                            |
|   Priority 3   |  (above configMAX_SYSCALL_INTERRUPT_PRIORITY)   |
|   Priority 4   |                                                 |
|  ------------- | configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 ----- |
|   Priority 5   |                                                 |
|   Priority 6   |  CAN use FreeRTOS FromISR functions             |
|   ...          |  (at or below configMAX_SYSCALL...)             |
|   Priority 255 |                                                 |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Cortex-M优先级模型：数字越低优先级越高（容易混淆！）。优先级0最高（NMI、HardFault），优先级255最低。

configMAX_SYSCALL_INTERRUPT_PRIORITY：优先级数字>=此值的中断可以调用FreeRTOS FromISR函数。例如设为5：优先级0-4不能调用FreeRTOS（优先级太高），优先级5-255可以调用FromISR函数。

---

## 9.3 Why Violating These Rules Breaks the Kernel

### Scenario: Blocking in ISR

```
WHAT HAPPENS IF YOU CALL vTaskDelay() IN ISR:
+------------------------------------------------------------------+
|                                                                  |
|  1. ISR runs (interrupts disabled/masked)                        |
|  2. vTaskDelay() called                                          |
|  3. Task is moved to delayed list                                |
|  4. Scheduler tries to switch to another task                    |
|  5. Context switch code runs...                                  |
|  6. ...but we're still in ISR context!                           |
|  7. Stack is wrong, interrupt state is wrong                     |
|  8. SYSTEM CRASHES or UNDEFINED BEHAVIOR                         |
|                                                                  |
+------------------------------------------------------------------+
```

### Scenario: Wrong Priority

```
WHAT HAPPENS IF ISR PRIORITY > configMAX_SYSCALL_INTERRUPT_PRIORITY:
+------------------------------------------------------------------+
|                                                                  |
|  1. High priority ISR fires during FreeRTOS critical section     |
|  2. FreeRTOS is manipulating internal data structures            |
|  3. ISR calls xQueueSendFromISR()                                |
|  4. xQueueSendFromISR modifies SAME data structures              |
|  5. DATA CORRUPTION - lists broken, counts wrong                 |
|  6. SYSTEM CRASHES later (hard to debug!)                        |
|                                                                  |
|  WHY: FreeRTOS critical sections use BASEPRI to mask             |
|       interrupts up to configMAX_SYSCALL_INTERRUPT_PRIORITY      |
|       Higher priority ISRs are NOT masked                        |
|                                                                  |
+------------------------------------------------------------------+
```

```
CRITICAL SECTION MECHANISM (Cortex-M):
+------------------------------------------------------------------+
|                                                                  |
|  taskENTER_CRITICAL():                                           |
|    - Sets BASEPRI to configMAX_SYSCALL_INTERRUPT_PRIORITY        |
|    - Masks interrupts with priority >= that value                |
|    - Higher priority ISRs still run!                             |
|                                                                  |
|  taskEXIT_CRITICAL():                                            |
|    - Restores previous BASEPRI                                   |
|    - Enables masked interrupts                                   |
|                                                                  |
|  portDISABLE_INTERRUPTS():                                       |
|    - Same as taskENTER_CRITICAL (mask up to threshold)           |
|                                                                  |
|  This is NOT the same as disabling ALL interrupts!               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

在ISR中阻塞会发生什么：ISR运行时中断禁用，调用vTaskDelay会移动任务到延时列表，调度器尝试切换但仍在ISR上下文，栈和中断状态错误，系统崩溃。

ISR优先级错误会发生什么：高优先级ISR在FreeRTOS临界区期间触发，FreeRTOS正在操作内部数据结构，ISR调用FromISR函数修改相同数据结构，数据损坏，系统稍后崩溃（难以调试）。

原因：FreeRTOS临界区使用BASEPRI屏蔽优先级>=configMAX_SYSCALL_INTERRUPT_PRIORITY的中断，更高优先级的中断不被屏蔽。

---

## 9.4 ISR Best Practices

```
ISR BEST PRACTICES:
+==================================================================+
||                                                                ||
||  1. KEEP ISRs SHORT                                           ||
||     - Read/write hardware                                     ||
||     - Send to queue / give semaphore                          ||
||     - Request context switch                                  ||
||     - RETURN                                                  ||
||                                                                ||
||  2. DEFER PROCESSING TO TASKS                                 ||
||     - ISR: Capture data, signal task                          ||
||     - Task: Process data, make decisions                      ||
||                                                                ||
||  3. USE CORRECT API                                           ||
||     - FromISR functions ONLY in ISR                           ||
||     - Always check pxHigherPriorityTaskWoken                  ||
||     - Always call portYIELD_FROM_ISR at end                   ||
||                                                                ||
||  4. SET CORRECT PRIORITY                                      ||
||     - Below configMAX_SYSCALL_INTERRUPT_PRIORITY              ||
||     - Or don't call any FreeRTOS functions                    ||
||                                                                ||
||  5. DON'T HOLD RESOURCES ACROSS ISR BOUNDARIES                ||
||     - No mutex held while waiting for ISR                     ||
||                                                                ||
+==================================================================+
```

### The ISR -> Queue -> Task Pattern

```
RECOMMENDED: ISR -> QUEUE -> TASK PATTERN
+------------------------------------------------------------------+
|                                                                  |
|  Hardware        ISR              Queue           Task           |
|  +------+    +--------+        +---------+    +------------+     |
|  |Sensor|--->|Read HW |------->|Data     |--->|Process     |     |
|  |      |    |Send to |        |Buffer   |    |Make        |     |
|  |      |    |Queue   |        |         |    |Decisions   |     |
|  +------+    +--------+        +---------+    +------------+     |
|                                                                  |
|  ISR: ~10 cycles                Queue: Thread-safe buffer        |
|  Task: Unlimited processing    Decouples ISR from processing     |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

ISR最佳实践：
1. 保持ISR简短：读写硬件、发送队列/给信号量、请求上下文切换、返回
2. 将处理延迟到任务：ISR捕获数据并发信号，任务处理数据
3. 使用正确的API：ISR中只用FromISR函数，检查pxHigherPriorityTaskWoken，最后调用portYIELD_FROM_ISR
4. 设置正确优先级：低于configMAX_SYSCALL_INTERRUPT_PRIORITY，或不调用任何FreeRTOS函数
5. 不要跨ISR边界持有资源

推荐模式：ISR -> 队列 -> 任务。ISR读硬件发送到队列（~10周期），任务从队列接收并处理（无限处理时间）。队列作为线程安全缓冲区解耦ISR和处理。

---

## Summary

```
INTERRUPT RULES SUMMARY:
+==================================================================+
||                                                                ||
||  RULE 1: ISRs NEVER BLOCK                                     ||
||                                                                ||
||  RULE 2: Use FromISR functions in ISR context                 ||
||                                                                ||
||  RULE 3: Respect configMAX_SYSCALL_INTERRUPT_PRIORITY         ||
||                                                                ||
||  RULE 4: Always handle pxHigherPriorityTaskWoken              ||
||                                                                ||
||  RULE 5: Keep ISRs minimal, defer work to tasks               ||
||                                                                ||
||  VIOLATION: Hard-to-debug crashes, data corruption            ||
||                                                                ||
+==================================================================+
```

| Rule | Violation Consequence |
|------|----------------------|
| Blocking in ISR | System freeze or crash |
| Wrong API | Undefined behavior, corruption |
| Wrong priority | Data structure corruption |
| Missing yield | Delayed response to events |
| Long ISR | Missed interrupts, poor real-time |

**Next Section**: [Memory Management](10-memory-management.md)
