# Section 12: Common Beginner Mistakes (and Why They Happen)

## 12.1 Too Many Tasks

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  "I need to blink an LED, read a button, and send UART data.   ||
||   I'll create 3 tasks!"                                        ||
||                                                                ||
||  Task 1: LED blink (256 word stack)                            ||
||  Task 2: Button read (256 word stack)                          ||
||  Task 3: UART send (256 word stack)                            ||
||                                                                ||
||  RESULT: 3KB RAM just for stacks + 3 TCBs (~200 bytes each)    ||
||                                                                ||
+==================================================================+

THE ROOT CAUSE:
+------------------------------------------------------------------+
|                                                                  |
|  THINKING ERROR:                                                 |
|  "RTOS = every action gets its own task"                         |
|                                                                  |
|  This comes from confusing:                                      |
|  - Logical parallelism (things that SEEM concurrent)             |
|  - Physical parallelism (things that NEED concurrent execution)  |
|                                                                  |
+------------------------------------------------------------------+

THE FIX:
+------------------------------------------------------------------+
|                                                                  |
|  Ask: "Does this NEED to run independently?"                     |
|                                                                  |
|  LED blink: No, can be a timer callback                          |
|  Button read: No, can be polled or interrupt-driven              |
|  UART send: Maybe, depends on blocking requirements              |
|                                                                  |
|  BETTER DESIGN:                                                  |
|  - One main task that handles multiple simple things             |
|  - Separate tasks only for truly independent timing              |
|  - Use timers for periodic actions                               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

错误：为每个小功能创建任务（LED闪烁、按钮读取、UART发送各一个任务）。结果：3KB RAM仅用于栈+3个TCB。

根本原因：思维错误——"RTOS=每个动作一个任务"。混淆了逻辑并行（看起来并发）和物理并行（需要并发执行）。

修复：问"这真的需要独立运行吗？"LED闪烁可以是定时器回调，按钮可以轮询或中断驱动。更好设计：一个主任务处理多个简单事情，只为真正独立定时的创建单独任务，用定时器处理周期动作。

---

## 12.2 Misusing Priorities

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  "My sensor task is important, I'll give it priority 10!"      ||
||  "My comm task is also important, priority 10!"                ||
||  "Control task is most important, priority 10!"                ||
||                                                                ||
||  RESULT:                                                       ||
||  - Everything at same priority                                 ||
||  - Time slicing (round robin) between all                      ||
||  - No actual prioritization                                    ||
||  - Might as well be cooperative                                ||
||                                                                ||
+==================================================================+

THE ROOT CAUSE:
+------------------------------------------------------------------+
|                                                                  |
|  THINKING ERROR:                                                 |
|  "High priority = important to me" instead of                    |
|  "High priority = must preempt others when ready"                |
|                                                                  |
|  Priority is about SCHEDULING, not VALUE:                        |
|  - High priority task ALWAYS runs when ready                     |
|  - Low priority task ONLY runs when high priority blocked        |
|                                                                  |
+------------------------------------------------------------------+

THE FIX:
+------------------------------------------------------------------+
|                                                                  |
|  PRIORITY ASSIGNMENT GUIDELINES:                                 |
|                                                                  |
|  Highest: Hard real-time (motor control, safety)                 |
|           - Must meet deadlines or system fails                  |
|                                                                  |
|  High:    Soft real-time (audio, communication)                  |
|           - Should meet deadlines for quality                    |
|                                                                  |
|  Medium:  Important background (sensor polling)                  |
|           - Needs to run regularly                               |
|                                                                  |
|  Low:     Nice-to-have (logging, UI updates)                     |
|           - Can wait when system busy                            |
|                                                                  |
|  Lowest:  Idle work (statistics, self-test)                      |
|           - Runs only when nothing else to do                    |
|                                                                  |
|  EXAMPLE:                                                        |
|  Motor control:  Priority 4 (highest)                            |
|  Sensor reading: Priority 3                                      |
|  Communication:  Priority 2                                      |
|  Display update: Priority 1                                      |
|  Logging:        Priority 0 (above idle)                         |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

错误：所有任务都给高优先级（都是10）。结果：无实际优先级区分，变成时间片轮转。

根本原因：思维错误——"高优先级=对我重要"而非"高优先级=就绪时必须抢占其他"。优先级是关于调度，不是价值。

修复：优先级分配指南。最高：硬实时（电机控制、安全）。高：软实时（音频、通信）。中：重要后台（传感器轮询）。低：可选功能（日志、UI更新）。最低：空闲工作（统计、自检）。

---

## 12.3 Blocking in ISRs

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  void UART_IRQHandler(void)                                    ||
||  {                                                             ||
||      uint8_t data = UART->DR;                                  ||
||      xQueueSend(queue, &data, portMAX_DELAY);  /* WRONG! */    ||
||  }                                                             ||
||                                                                ||
||  RESULT:                                                       ||
||  - If queue is full, ISR tries to block                        ||
||  - No task context to block from                               ||
||  - System crash or undefined behavior                          ||
||                                                                ||
+==================================================================+

THE ROOT CAUSE:
+------------------------------------------------------------------+
|                                                                  |
|  THINKING ERROR:                                                 |
|  "API works the same everywhere"                                 |
|                                                                  |
|  Forgetting that ISR is fundamentally different:                 |
|  - No stack for blocking                                         |
|  - No task to suspend                                            |
|  - Must complete quickly                                         |
|                                                                  |
+------------------------------------------------------------------+

THE FIX:
+------------------------------------------------------------------+
|                                                                  |
|  void UART_IRQHandler(void)                                      |
|  {                                                               |
|      BaseType_t woken = pdFALSE;                                 |
|      uint8_t data = UART->DR;                                    |
|                                                                  |
|      /* Use FromISR with timeout = 0 */                          |
|      if (xQueueSendFromISR(queue, &data, &woken) != pdPASS)      |
|      {                                                           |
|          /* Handle overflow - maybe increment counter */         |
|          overflowCount++;                                        |
|      }                                                           |
|      portYIELD_FROM_ISR(woken);                                  |
|  }                                                               |
|                                                                  |
|  RULES FOR ISRs:                                                 |
|  1. Always use FromISR API variants                              |
|  2. Never wait/block                                             |
|  3. Keep ISR short                                               |
|  4. Handle errors gracefully (no blocking fallback)              |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

错误：在ISR中使用阻塞API（xQueueSend带portMAX_DELAY）。结果：队列满时ISR尝试阻塞，无任务上下文可阻塞，系统崩溃。

根本原因：思维错误——"API在任何地方工作相同"。忘记ISR根本不同：无阻塞的栈、无可挂起的任务、必须快速完成。

修复：使用FromISR API变体，超时=0，处理失败情况，调用portYIELD_FROM_ISR。ISR规则：总是用FromISR、永不等待/阻塞、保持ISR短、优雅处理错误。

---

## 12.4 Using Delays Instead of Events

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  void ButtonTask(void *p)                                      ||
||  {                                                             ||
||      for(;;)                                                   ||
||      {                                                         ||
||          if (GPIO_ReadPin(BUTTON_PIN) == 0)                    ||
||          {                                                     ||
||              HandleButtonPress();                              ||
||          }                                                     ||
||          vTaskDelay(pdMS_TO_TICKS(10));  /* Poll every 10ms */ ||
||      }                                                         ||
||  }                                                             ||
||                                                                ||
||  PROBLEMS:                                                     ||
||  - Wastes CPU (polls 100 times/sec even with no presses)       ||
||  - 10ms worst-case latency                                     ||
||  - Higher tick rate = more overhead                            ||
||                                                                ||
+==================================================================+

THE ROOT CAUSE:
+------------------------------------------------------------------+
|                                                                  |
|  THINKING ERROR:                                                 |
|  "Polling is simple and it works"                                |
|                                                                  |
|  Coming from super-loop mindset where everything polls.          |
|  Not leveraging RTOS event-driven capabilities.                  |
|                                                                  |
+------------------------------------------------------------------+

THE FIX:
+------------------------------------------------------------------+
|                                                                  |
|  /* Configure button interrupt */                                |
|  void EXTI_IRQHandler(void)                                      |
|  {                                                               |
|      BaseType_t woken = pdFALSE;                                 |
|      xSemaphoreGiveFromISR(buttonSem, &woken);                   |
|      EXTI_ClearFlag();                                           |
|      portYIELD_FROM_ISR(woken);                                  |
|  }                                                               |
|                                                                  |
|  void ButtonTask(void *p)                                        |
|  {                                                               |
|      for(;;)                                                     |
|      {                                                           |
|          /* Blocks until button pressed - ZERO CPU while waiting */
|          xSemaphoreTake(buttonSem, portMAX_DELAY);               |
|          HandleButtonPress();                                    |
|      }                                                           |
|  }                                                               |
|                                                                  |
|  BENEFITS:                                                       |
|  - Zero CPU usage while waiting                                  |
|  - Instant response (interrupt latency only)                     |
|  - Clean separation of detection and handling                    |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

错误：用延迟轮询而非事件（每10ms轮询按钮）。问题：浪费CPU（无按压时每秒轮询100次）、10ms最坏延迟、更高tick率=更多开销。

根本原因：思维错误——"轮询简单且有效"。来自超级循环思维，未利用RTOS事件驱动能力。

修复：配置按钮中断，ISR给信号量，任务等待信号量。好处：等待时零CPU使用、即时响应（仅中断延迟）、检测和处理清晰分离。

---

## 12.5 Treating FreeRTOS Like Linux

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  "I'll just malloc() a buffer when I need it"                  ||
||  "I'll create a thread for this temporary work"                ||
||  "The kernel will protect me from stack overflow"              ||
||  "I can use any library, it's just C"                          ||
||                                                                ||
||  REALITY CHECK:                                                ||
||  - malloc may fragment heap, fail unpredictably                ||
||  - Task creation is expensive, not free                        ||
||  - NO MMU = no automatic protection                            ||
||  - Libraries may not be thread-safe or RTOS-aware              ||
||                                                                ||
+==================================================================+

THE ROOT CAUSE:
+------------------------------------------------------------------+
|                                                                  |
|  THINKING ERROR:                                                 |
|  "An RTOS is like a small Linux"                                 |
|                                                                  |
|  FreeRTOS is NOT a general-purpose OS:                           |
|  - No memory protection                                          |
|  - No virtual memory                                             |
|  - No process isolation                                          |
|  - No extensive runtime                                          |
|  - Resources are YOUR responsibility                             |
|                                                                  |
+------------------------------------------------------------------+

THE FIX:
+------------------------------------------------------------------+
|                                                                  |
|  MINDSET SHIFT:                                                  |
|                                                                  |
|  Linux mindset        ->  FreeRTOS mindset                       |
|  ----------------         ----------------                       |
|  malloc freely        ->  Pre-allocate at startup                |
|  Create threads       ->  Create tasks once at init              |
|  Kernel protects      ->  YOU protect (no MMU)                   |
|  Any library works    ->  Check thread-safety first              |
|  Grow stack as needed ->  Size stack correctly upfront           |
|  Exceptions caught    ->  Bugs crash the system                  |
|                                                                  |
|  EMBEDDED DISCIPLINE:                                            |
|  - Know your memory budget                                       |
|  - Verify all allocations at startup                             |
|  - Test with stack overflow detection                            |
|  - Audit library thread-safety                                   |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

错误：把FreeRTOS当Linux用。"需要时malloc缓冲区"、"为临时工作创建线程"、"内核会保护我免受栈溢出"。

现实：malloc可能碎片化、不可预测失败；任务创建昂贵；无MMU=无自动保护；库可能非线程安全。

根本原因：思维错误——"RTOS是小型Linux"。FreeRTOS不是通用OS：无内存保护、无虚拟内存、无进程隔离、资源是你的责任。

修复：思维转变。Linux思维->FreeRTOS思维：自由malloc->启动时预分配、创建线程->初始化时创建一次任务、内核保护->你保护、任何库->先检查线程安全、按需增长栈->提前正确确定栈大小。

---

## 12.6 Sharing Data Without Protection

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  volatile int sharedCounter = 0;                               ||
||                                                                ||
||  void Task1(void *p)                                           ||
||  {                                                             ||
||      for(;;) { sharedCounter++; }                              ||
||  }                                                             ||
||                                                                ||
||  void Task2(void *p)                                           ||
||  {                                                             ||
||      for(;;) { printf("%d\n", sharedCounter); }                ||
||  }                                                             ||
||                                                                ||
||  PROBLEMS:                                                     ||
||  - sharedCounter++ is NOT atomic (read-modify-write)           ||
||  - Task2 might see partial update                              ||
||  - Race condition                                              ||
||                                                                ||
+==================================================================+

THE ROOT CAUSE:
+------------------------------------------------------------------+
|                                                                  |
|  THINKING ERROR:                                                 |
|  "volatile makes it safe for multithreading"                     |
|                                                                  |
|  volatile prevents compiler optimization, NOT race conditions:   |
|  - Compiler won't cache value in register                        |
|  - But read-modify-write still NOT atomic                        |
|  - Another task can interrupt between read and write             |
|                                                                  |
+------------------------------------------------------------------+

THE FIX:
+------------------------------------------------------------------+
|                                                                  |
|  OPTION 1: Mutex (for complex data)                              |
|  int sharedCounter = 0;                                          |
|  SemaphoreHandle_t mutex;                                        |
|                                                                  |
|  void Task1(void *p)                                             |
|  {                                                               |
|      for(;;)                                                     |
|      {                                                           |
|          xSemaphoreTake(mutex, portMAX_DELAY);                   |
|          sharedCounter++;                                        |
|          xSemaphoreGive(mutex);                                  |
|      }                                                           |
|  }                                                               |
|                                                                  |
|  OPTION 2: Critical section (for short operations)               |
|  taskENTER_CRITICAL();                                           |
|  sharedCounter++;                                                |
|  taskEXIT_CRITICAL();                                            |
|                                                                  |
|  OPTION 3: Atomic operations (if available)                      |
|  __atomic_fetch_add(&sharedCounter, 1, __ATOMIC_SEQ_CST);        |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

错误：无保护共享数据。volatile int sharedCounter，两个任务同时访问。问题：sharedCounter++不是原子的（读-改-写），Task2可能看到部分更新，竞态条件。

根本原因：思维错误——"volatile使多线程安全"。volatile防止编译器优化，不防止竞态条件。

修复：选项1用互斥锁（复杂数据）、选项2用临界区（短操作）、选项3用原子操作（如果可用）。

---

## Summary

```
COMMON MISTAKES AND FIXES:
+==================================================================+
||                                                                ||
||  MISTAKE                    | FIX                              ||
||  ---------------------------+----------------------------------||
||  Too many tasks             | Ask "needs independent timing?"  ||
||  All same priority          | Assign by timing requirements    ||
||  Blocking in ISR            | Always use FromISR APIs          ||
||  Polling with delays        | Use events/semaphores            ||
||  Treating like Linux        | Know your resource constraints   ||
||  Unprotected shared data    | Mutex/critical section/atomic    ||
||                                                                ||
||  CORE PRINCIPLE:                                               ||
||  FreeRTOS is a TOOL, not a SAFETY NET.                         ||
||  It gives you control, not protection.                         ||
||  You must understand what you're doing.                        ||
||                                                                ||
+==================================================================+
```

**Next Section**: [How to Learn FreeRTOS from Source Code](13-learning-from-source.md)
