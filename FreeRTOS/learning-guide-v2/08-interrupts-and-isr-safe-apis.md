# Section 8: Interrupts and ISR-Safe APIs

## 8.1 Task Context vs ISR Context

```
TWO EXECUTION CONTEXTS:
+==================================================================+
||                                                                ||
||  TASK CONTEXT:                                                 ||
||  +----------------------------------------------------------+  ||
||  | - Runs at task priority level (software priority)        |  ||
||  | - Uses Process Stack Pointer (PSP)                       |  ||
||  | - Has own stack (allocated per task)                     |  ||
||  | - CAN block and wait                                     |  ||
||  | - CAN be preempted by ISR or higher priority task        |  ||
||  | - Uses standard FreeRTOS APIs                            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  ISR CONTEXT:                                                  ||
||  +----------------------------------------------------------+  ||
||  | - Runs at hardware priority level (NVIC priority)        |  ||
||  | - Uses Main Stack Pointer (MSP)                          |  ||
||  | - Shares stack with other ISRs                           |  ||
||  | - CANNOT block (would hang system)                       |  ||
||  | - Can preempt tasks and lower priority ISRs              |  ||
||  | - Must use FromISR APIs ONLY                             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXECUTION TIMELINE:                                           ||
||                                                                ||
||  |----Task A----|         |----Task A----|         |--Task B--|
||                 |         |                                    ||
||                 v         ^                                    ||
||              [ISR runs]                                        ||
||                                                                ||
||  ISR preempts task, runs to completion, task resumes           ||
||  (unless ISR made higher priority task ready)                  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

两种执行上下文：

任务上下文：在任务优先级（软件优先级）运行、使用进程栈指针(PSP)、有自己的栈、可以阻塞等待、可被ISR或更高优先级任务抢占、使用标准FreeRTOS API。

ISR上下文：在硬件优先级（NVIC优先级）运行、使用主栈指针(MSP)、与其他ISR共享栈、不能阻塞（会挂起系统）、可抢占任务和更低优先级ISR、必须只使用FromISR API。

---

## 8.2 Why FreeRTOS Has FromISR APIs

```
THE FUNDAMENTAL PROBLEM:
+==================================================================+
||                                                                ||
||  Standard API (e.g., xQueueSend):                              ||
||  +----------------------------------------------------------+  ||
||  | 1. Enter critical section                                |  ||
||  | 2. Check if queue full                                   |  ||
||  | 3. If full, ADD CURRENT TASK TO WAIT LIST                |  ||
||  | 4. BLOCK current task                                    |  ||
||  | 5. TRIGGER CONTEXT SWITCH                                |  ||
||  | 6. ... wait for space ...                                |  ||
||  | 7. Resume, send item                                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  In ISR context, steps 3-6 are IMPOSSIBLE:                     ||
||  - ISR doesn't have a "current task" to block                  ||
||  - ISR cannot sleep/wait                                       ||
||  - Context switch during ISR corrupts state                    ||
||                                                                ||
||  FromISR API (e.g., xQueueSendFromISR):                        ||
||  +----------------------------------------------------------+  ||
||  | 1. Enter critical section (may mask interrupts)          |  ||
||  | 2. Check if queue full                                   |  ||
||  | 3. If full, RETURN IMMEDIATELY with error                |  ||
||  | 4. If not full, send item                                |  ||
||  | 5. Check if this woke a higher priority task             |  ||
||  | 6. Return (caller decides about context switch)          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

根本问题：

标准API（如xQueueSend）：进入临界区 -> 检查队列满 -> 如果满，把当前任务加入等待列表 -> 阻塞当前任务 -> 触发上下文切换 -> 等待空间 -> 恢复，发送项。

在ISR上下文中，步骤3-6不可能：ISR没有"当前任务"可阻塞、ISR不能睡眠/等待、ISR期间上下文切换会损坏状态。

FromISR API（如xQueueSendFromISR）：进入临界区 -> 检查队列满 -> 如果满，立即返回错误 -> 如果不满，发送项 -> 检查是否唤醒了更高优先级任务 -> 返回（调用者决定上下文切换）。

---

## 8.3 The FromISR Pattern

```c
/* CORRECT ISR PATTERN */
void UART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t byte;
    
    /* Read data from hardware */
    byte = UART->DR;
    
    /* Send to queue (non-blocking) */
    xQueueSendFromISR(xRxQueue, &byte, &xHigherPriorityTaskWoken);
    
    /* Request context switch if a higher priority task was woken */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

```
THE xHigherPriorityTaskWoken MECHANISM:
+==================================================================+
||                                                                ||
||  SCENARIO: Low priority task running, high priority blocked    ||
||                                                                ||
||  Before ISR:                                                   ||
||  +----------------------------------------------------------+  ||
||  | Running: TaskL (priority 1)                              |  ||
||  | Blocked: TaskH (priority 3, waiting on queue)            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  ISR gives to queue:                                           ||
||  +----------------------------------------------------------+  ||
||  | xQueueSendFromISR() sees TaskH waiting                   |  ||
||  | Moves TaskH to ready list                                |  ||
||  | Sets *pxHigherPriorityTaskWoken = pdTRUE                 |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  ISR checks flag:                                              ||
||  +----------------------------------------------------------+  ||
||  | portYIELD_FROM_ISR(pdTRUE)                               |  ||
||  | -> Pends PendSV for context switch                       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  After ISR returns:                                            ||
||  +----------------------------------------------------------+  ||
||  | PendSV runs (lowest priority exception)                  |  ||
||  | Context switch to TaskH                                  |  ||
||  | TaskH runs immediately                                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

xHigherPriorityTaskWoken机制：

场景：低优先级任务运行，高优先级任务阻塞。

ISR前：运行TaskL（优先级1），阻塞TaskH（优先级3，等待队列）。

ISR给队列：xQueueSendFromISR看到TaskH等待 -> 移动TaskH到就绪列表 -> 设置*pxHigherPriorityTaskWoken = pdTRUE。

ISR检查标志：portYIELD_FROM_ISR(pdTRUE) -> 挂起PendSV进行上下文切换。

ISR返回后：PendSV运行 -> 上下文切换到TaskH -> TaskH立即运行。

---

## 8.4 What Can Go Wrong

```
MISTAKE 1: Using standard API in ISR
+------------------------------------------------------------------+
|                                                                  |
|  void UART_IRQHandler(void)                                      |
|  {                                                               |
|      xQueueSend(queue, &data, portMAX_DELAY);  /* WRONG! */      |
|  }                                                               |
|                                                                  |
|  RESULT:                                                         |
|  - If queue is full, ISR tries to block                          |
|  - Scheduler called from ISR context                             |
|  - System crash or undefined behavior                            |
|                                                                  |
+------------------------------------------------------------------+

MISTAKE 2: Forgetting to check/yield
+------------------------------------------------------------------+
|                                                                  |
|  void UART_IRQHandler(void)                                      |
|  {                                                               |
|      xQueueSendFromISR(queue, &data, NULL);  /* No yield check */|
|  }                                                               |
|                                                                  |
|  RESULT:                                                         |
|  - High priority task woken but doesn't run immediately          |
|  - Must wait until next tick or next context switch point        |
|  - Increased latency for high priority task                      |
|                                                                  |
+------------------------------------------------------------------+

MISTAKE 3: Long ISR blocking other interrupts
+------------------------------------------------------------------+
|                                                                  |
|  void DataProcess_IRQHandler(void)                               |
|  {                                                               |
|      for (int i = 0; i < 1000; i++)                              |
|      {                                                           |
|          process_sample(buffer[i]);  /* Takes too long! */       |
|      }                                                           |
|  }                                                               |
|                                                                  |
|  RESULT:                                                         |
|  - Other interrupts delayed                                      |
|  - System tick delayed (timing corrupted)                        |
|  - Higher priority ISRs delayed                                  |
|                                                                  |
|  SOLUTION: Defer work to task                                    |
|  void DataProcess_IRQHandler(void)                               |
|  {                                                               |
|      BaseType_t woken = pdFALSE;                                 |
|      xQueueSendFromISR(bufferQueue, &buffer, &woken);            |
|      portYIELD_FROM_ISR(woken);                                  |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

错误1：在ISR中使用标准API。如果队列满，ISR尝试阻塞 -> 从ISR上下文调用调度器 -> 系统崩溃或未定义行为。

错误2：忘记检查/yield。高优先级任务被唤醒但不立即运行，必须等到下一个tick或上下文切换点，增加高优先级任务延迟。

错误3：长ISR阻塞其他中断。其他中断延迟、系统tick延迟（定时损坏）、更高优先级ISR延迟。解决方案：将工作推迟到任务。

---

## 8.5 Interrupt Priority Rules

```
ARM CORTEX-M INTERRUPT PRIORITIES:
+==================================================================+
||                                                                ||
||  NVIC Priority (0 = highest, 255 = lowest on 8-bit):           ||
||                                                                ||
||  +----------------------------------------------------------+  ||
||  | Priority 0-3:  Reserved for critical system interrupts   |  ||
||  |                (NMI, HardFault, etc.)                     |  ||
||  +----------------------------------------------------------+  ||
||  | Priority 4:    Highest user interrupt                    |  ||
||  | Priority 5:    configMAX_SYSCALL_INTERRUPT_PRIORITY     |  ||
||  |                ^^^ FreeRTOS ISRs must be >= this ^^^     |  ||
||  | ...                                                      |  ||
||  | Priority 10:   SysTick, PendSV (kernel interrupts)       |  ||
||  | Priority 11:   Low priority user ISR                     |  ||
||  | ...                                                      |  ||
||  | Priority 255:  Lowest priority                           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  RULE: ISRs using FreeRTOS FromISR APIs must have priority    ||
||        >= configMAX_SYSCALL_INTERRUPT_PRIORITY                 ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

ARM Cortex-M中断优先级（0=最高，255=最低，8位）：

优先级0-3：保留给关键系统中断（NMI、HardFault等）。
优先级4：最高用户中断。
优先级5：configMAX_SYSCALL_INTERRUPT_PRIORITY——使用FreeRTOS FromISR API的ISR必须>=这个值。
优先级10：SysTick、PendSV（内核中断）。
优先级11+：低优先级用户ISR。

规则：使用FromISR API的ISR必须优先级>=configMAX_SYSCALL_INTERRUPT_PRIORITY。

```
WHY THIS PRIORITY RULE?
+------------------------------------------------------------------+
|                                                                  |
|  FreeRTOS critical sections use BASEPRI register:                |
|                                                                  |
|  portENTER_CRITICAL() sets BASEPRI = MAX_SYSCALL_PRIORITY        |
|  -> Masks interrupts at MAX_SYSCALL_PRIORITY and below           |
|  -> Higher priority interrupts still run!                        |
|                                                                  |
|  If ISR with priority < MAX_SYSCALL_PRIORITY uses FromISR:       |
|  +------------------------------------------------------------+  |
|  | 1. Task enters critical section (BASEPRI set)             |  |
|  | 2. High priority ISR fires (NOT masked by BASEPRI)        |  |
|  | 3. ISR calls xQueueSendFromISR                            |  |
|  | 4. FromISR modifies kernel data structures                |  |
|  | 5. BUT task was in middle of modifying them too!          |  |
|  | 6. DATA CORRUPTION                                        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CORRECT: All ISRs using FreeRTOS APIs are masked by BASEPRI    |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么有这个优先级规则？

FreeRTOS临界区使用BASEPRI寄存器：portENTER_CRITICAL()设置BASEPRI=MAX_SYSCALL_PRIORITY -> 屏蔽该优先级及以下的中断 -> 更高优先级中断仍运行！

如果优先级<MAX_SYSCALL_PRIORITY的ISR使用FromISR：任务进入临界区 -> 高优先级ISR触发（不被BASEPRI屏蔽） -> ISR调用xQueueSendFromISR -> FromISR修改内核数据结构 -> 但任务正在修改它们！ -> 数据损坏。

正确做法：所有使用FreeRTOS API的ISR都被BASEPRI屏蔽。

---

## 8.6 Critical Sections

```
CRITICAL SECTION TYPES:
+==================================================================+
||                                                                ||
||  TASK CONTEXT:                                                 ||
||  +----------------------------------------------------------+  ||
||  | taskENTER_CRITICAL()                                     |  ||
||  | {                                                        |  ||
||  |     // Protected code                                    |  ||
||  |     // No task switch, no FreeRTOS ISRs                  |  ||
||  | }                                                        |  ||
||  | taskEXIT_CRITICAL()                                      |  ||
||  |                                                          |  ||
||  | Implementation: Sets BASEPRI, increments nesting counter |  ||
||  | Can be nested (counter tracks depth)                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  ISR CONTEXT:                                                  ||
||  +----------------------------------------------------------+  ||
||  | UBaseType_t savedMask;                                   |  ||
||  | savedMask = taskENTER_CRITICAL_FROM_ISR();               |  ||
||  | {                                                        |  ||
||  |     // Protected code in ISR                             |  ||
||  | }                                                        |  ||
||  | taskEXIT_CRITICAL_FROM_ISR(savedMask);                   |  ||
||  |                                                          |  ||
||  | Implementation: Saves/restores BASEPRI                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  DISABLING ALL INTERRUPTS (avoid if possible):                 ||
||  +----------------------------------------------------------+  ||
||  | taskDISABLE_INTERRUPTS()                                 |  ||
||  | {                                                        |  ||
||  |     // Ultra-short critical code only!                   |  ||
||  | }                                                        |  ||
||  | taskENABLE_INTERRUPTS()                                  |  ||
||  |                                                          |  ||
||  | Implementation: Sets PRIMASK (all interrupts disabled)   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

临界区类型：

任务上下文：taskENTER_CRITICAL()/taskEXIT_CRITICAL()。实现：设置BASEPRI，递增嵌套计数器。可嵌套。

ISR上下文：taskENTER_CRITICAL_FROM_ISR()/taskEXIT_CRITICAL_FROM_ISR()。实现：保存/恢复BASEPRI。

禁用所有中断（尽量避免）：taskDISABLE_INTERRUPTS()/taskENABLE_INTERRUPTS()。实现：设置PRIMASK（所有中断禁用）。只用于超短临界代码！

---

## Summary

```
INTERRUPTS AND ISR KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  TWO CONTEXTS:                                                 ||
||  - Task: can block, uses standard APIs                         ||
||  - ISR: cannot block, must use FromISR APIs                    ||
||                                                                ||
||  FromISR PATTERN:                                              ||
||  1. Declare xHigherPriorityTaskWoken = pdFALSE                 ||
||  2. Call FromISR API with &xHigherPriorityTaskWoken            ||
||  3. Call portYIELD_FROM_ISR(xHigherPriorityTaskWoken)          ||
||                                                                ||
||  PRIORITY RULES:                                               ||
||  - ISRs using FreeRTOS must have priority >=                   ||
||    configMAX_SYSCALL_INTERRUPT_PRIORITY                        ||
||  - Higher priority ISRs cannot use FreeRTOS APIs               ||
||                                                                ||
||  BEST PRACTICES:                                               ||
||  - Keep ISRs short                                             ||
||  - Defer work to tasks via queues/semaphores                   ||
||  - Never block in ISR                                          ||
||  - Use correct critical section APIs for context               ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Memory Management in FreeRTOS](09-memory-management.md)
