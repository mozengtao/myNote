# Section 5: Scheduling - How FreeRTOS Actually Runs Code

## 5.1 Priority-Based Preemptive Scheduling

```
THE FUNDAMENTAL RULE:
+==================================================================+
||                                                                ||
||  "The highest priority READY task ALWAYS runs."                ||
||                                                                ||
||  Priority 3: [TaskC] ----RUNNING                               ||
||  Priority 2: [TaskB] ----READY (waiting for CPU)               ||
||  Priority 1: [TaskA] ----READY (waiting for CPU)               ||
||  Priority 0: [Idle]  ----READY (runs when nothing else ready)  ||
||                                                                ||
||  If TaskC blocks:                                              ||
||                                                                ||
||  Priority 3: [TaskC] ----BLOCKED (waiting for event)           ||
||  Priority 2: [TaskB] ----RUNNING (highest ready)               ||
||  Priority 1: [TaskA] ----READY                                 ||
||  Priority 0: [Idle]  ----READY                                 ||
||                                                                ||
||  If TaskC becomes ready again:                                 ||
||                                                                ||
||  Priority 3: [TaskC] ----RUNNING (immediately preempts TaskB)  ||
||  Priority 2: [TaskB] ----READY (preempted)                     ||
||  Priority 1: [TaskA] ----READY                                 ||
||  Priority 0: [Idle]  ----READY                                 ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

基本规则："最高优先级的就绪任务总是运行。"

示例：
- 优先级3的TaskC正在运行
- 优先级2的TaskB就绪，等待CPU
- 优先级1的TaskA就绪
- 优先级0的空闲任务就绪

如果TaskC阻塞：TaskB立即成为最高优先级就绪任务并运行。

如果TaskC再次就绪：TaskC立即抢占TaskB运行。

### Preemption Points

```
WHEN DOES CONTEXT SWITCH HAPPEN?
+------------------------------------------------------------------+
|                                                                  |
|  1. EXPLICIT BLOCKING (task calls blocking API):                 |
|     vTaskDelay()                                                 |
|     xQueueReceive() with wait time                               |
|     xSemaphoreTake() with wait time                              |
|     ulTaskNotifyTake() with wait time                            |
|                                                                  |
|     Current task -> Blocked, next ready task runs                |
|                                                                  |
|  2. HIGHER PRIORITY BECOMES READY:                               |
|     ISR gives semaphore that high-priority task waits on         |
|     Timer expires, unblocking high-priority task                 |
|     Another task resumes a suspended high-priority task          |
|                                                                  |
|     High priority task immediately preempts current              |
|                                                                  |
|  3. TICK INTERRUPT (if time slicing enabled):                    |
|     configUSE_TIME_SLICING == 1                                  |
|     Multiple tasks at same priority                              |
|     Round-robin at each tick                                     |
|                                                                  |
|  4. EXPLICIT YIELD:                                              |
|     taskYIELD()                                                  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

上下文切换何时发生？

1. 显式阻塞：任务调用阻塞API（vTaskDelay、xQueueReceive等），当前任务变阻塞，下一个就绪任务运行。

2. 更高优先级变就绪：ISR给出高优先级任务等待的信号量、定时器到期解除高优先级任务阻塞、其他任务恢复挂起的高优先级任务。高优先级任务立即抢占当前任务。

3. Tick中断（如果启用时间片）：configUSE_TIME_SLICING == 1，同优先级多个任务，每个tick轮转。

4. 显式yield：taskYIELD()。

---

## 5.2 Cooperative vs Preemptive Modes

```
SCHEDULING MODE COMPARISON:
+==================================================================+
||                                                                ||
||  PREEMPTIVE (configUSE_PREEMPTION = 1):                        ||
||  +----------------------------------------------------------+  ||
||  | Default and recommended mode                             |  ||
||  |                                                          |  ||
||  | Task can be interrupted at ANY time by:                  |  ||
||  | - Higher priority task becoming ready                    |  ||
||  | - Time slice (if same priority tasks exist)              |  ||
||  |                                                          |  ||
||  | Pros:                                                    |  ||
||  | + High priority tasks get CPU immediately                |  ||
||  | + Better responsiveness                                  |  ||
||  | + Easier to reason about priorities                      |  ||
||  |                                                          |  ||
||  | Cons:                                                    |  ||
||  | - Need to protect shared data (critical sections)        |  ||
||  | - More stack usage (nested preemption)                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  COOPERATIVE (configUSE_PREEMPTION = 0):                       ||
||  +----------------------------------------------------------+  ||
||  | Task runs until it EXPLICITLY yields or blocks           |  ||
||  |                                                          |  ||
||  | taskYIELD() or blocking call required to switch          |  ||
||  |                                                          |  ||
||  | Pros:                                                    |  ||
||  | + Simpler (no unexpected preemption)                     |  ||
||  | + Less need for critical sections                        |  ||
||  | + Easier porting from super-loop code                    |  ||
||  |                                                          |  ||
||  | Cons:                                                    |  ||
||  | - High priority task must wait for low priority to yield |  ||
||  | - Worse responsiveness                                   |  ||
||  | - One buggy task can starve others                       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

调度模式比较：

抢占式（configUSE_PREEMPTION = 1）：默认推荐模式。任务可在任何时候被更高优先级任务或时间片中断。优点：高优先级任务立即获得CPU、更好响应性、更容易理解优先级。缺点：需要保护共享数据、更多栈使用。

协作式（configUSE_PREEMPTION = 0）：任务运行直到显式yield或阻塞。需要taskYIELD()或阻塞调用才能切换。优点：更简单（无意外抢占）、更少临界区需求、更容易从超级循环移植。缺点：高优先级任务必须等待低优先级yield、更差响应性、一个有bug的任务可饿死其他任务。

---

## 5.3 Time Slicing (Round Robin)

```
TIME SLICING WITHIN SAME PRIORITY:
+==================================================================+
||                                                                ||
||  configUSE_TIME_SLICING = 1 (default)                          ||
||  configTICK_RATE_HZ = 1000 (1ms tick)                          ||
||                                                                ||
||  Three tasks at priority 2, all ready:                         ||
||                                                                ||
||  Tick 0    Tick 1    Tick 2    Tick 3    Tick 4                ||
||  |---------|---------|---------|---------|---------|           ||
||  | TaskA   | TaskB   | TaskC   | TaskA   | TaskB   |           ||
||  | runs    | runs    | runs    | runs    | runs    |           ||
||  |---------|---------|---------|---------|---------|           ||
||                                                                ||
||  Each task gets one tick (1ms) before switching to next        ||
||  at the same priority level.                                   ||
||                                                                ||
||  IF configUSE_TIME_SLICING = 0:                                ||
||  +----------------------------------------------------------+  ||
||  | TaskA runs until it blocks or higher priority ready     |  ||
||  | TaskB and TaskC never run (starved)                      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

同优先级内的时间片：

configUSE_TIME_SLICING = 1（默认），configTICK_RATE_HZ = 1000（1ms tick）。

三个优先级2的任务都就绪：每个任务在切换到同优先级下一个任务之前获得一个tick（1ms）。TaskA -> TaskB -> TaskC -> TaskA...

如果configUSE_TIME_SLICING = 0：TaskA运行直到阻塞或更高优先级就绪，TaskB和TaskC永远不运行（饿死）。

---

## 5.4 The Ready Lists

```
READY LISTS STRUCTURE (from tasks.c):
+==================================================================+
||                                                                ||
||  static List_t pxReadyTasksLists[configMAX_PRIORITIES];        ||
||                                                                ||
||  Index = Priority                                              ||
||                                                                ||
||  pxReadyTasksLists[4]: [TaskH] <-> [TaskI]                     ||
||  pxReadyTasksLists[3]: [TaskE] <-> [TaskF] <-> [TaskG]         ||
||  pxReadyTasksLists[2]: [TaskC] <-> [TaskD]                     ||
||  pxReadyTasksLists[1]: [TaskA] <-> [TaskB]                     ||
||  pxReadyTasksLists[0]: [Idle]                                  ||
||                                                                ||
||  uxTopReadyPriority = 4  (tracks highest non-empty list)       ||
||                                                                ||
||  SELECTING NEXT TASK (taskSELECT_HIGHEST_PRIORITY_TASK):       ||
||  +----------------------------------------------------------+  ||
||  | 1. Start at uxTopReadyPriority                           |  ||
||  | 2. Get first task from pxReadyTasksLists[priority]       |  ||
||  | 3. That task becomes pxCurrentTCB                        |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  TIME COMPLEXITY:                                              ||
||  +----------------------------------------------------------+  ||
||  | Generic selection: O(1) using uxTopReadyPriority         |  ||
||  | Port-optimized: O(1) using CLZ instruction               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

就绪列表结构（来自tasks.c）：

static List_t pxReadyTasksLists[configMAX_PRIORITIES]，索引=优先级。

每个优先级有一个任务链表。uxTopReadyPriority跟踪最高非空列表。

选择下一个任务：从uxTopReadyPriority开始，获取该优先级列表的第一个任务，该任务成为pxCurrentTCB。

时间复杂度：通用选择O(1)使用uxTopReadyPriority，移植层优化O(1)使用CLZ指令。

### Port-Optimized Task Selection

```
OPTIMIZED PRIORITY TRACKING (ARM Cortex-M):
+------------------------------------------------------------------+
|                                                                  |
|  When configUSE_PORT_OPTIMISED_TASK_SELECTION = 1:               |
|                                                                  |
|  Uses a 32-bit bitmap + CLZ (Count Leading Zeros) instruction:   |
|                                                                  |
|  uxTopReadyPriority (bitmap):                                    |
|  Bit:  31 30 29 ... 4  3  2  1  0                               |
|        0  0  0  ... 1  1  1  0  1                                |
|                     ^  ^  ^     ^                                |
|                     |  |  |     |                                |
|                     |  |  |     +-- Pri 0 has ready tasks        |
|                     |  |  +-------- Pri 2 has ready tasks        |
|                     |  +----------- Pri 3 has ready tasks        |
|                     +-------------- Pri 4 has ready tasks        |
|                                                                  |
|  Finding highest priority:                                       |
|  CLZ(uxTopReadyPriority) = 27  (27 leading zeros)               |
|  Highest priority = 31 - 27 = 4                                 |
|                                                                  |
|  ONE INSTRUCTION to find highest priority task!                  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

优化的优先级跟踪（ARM Cortex-M）：

当configUSE_PORT_OPTIMISED_TASK_SELECTION = 1时，使用32位位图+CLZ（计算前导零）指令。

uxTopReadyPriority是位图，每位表示对应优先级是否有就绪任务。

查找最高优先级：CLZ(位图)返回前导零数量，最高优先级=31-前导零数。

一条指令找到最高优先级任务！

---

## 5.5 The Delayed Lists

```
DELAYED TASK MANAGEMENT:
+==================================================================+
||                                                                ||
||  Two delayed lists (for tick counter overflow handling):       ||
||                                                                ||
||  xDelayedTaskList1 <--+                                        ||
||                       +--> pxDelayedTaskList (current)         ||
||  xDelayedTaskList2 <--+                                        ||
||                       +--> pxOverflowDelayedTaskList           ||
||                                                                ||
||  List is SORTED by wake time (xItemValue in list item):        ||
||                                                                ||
||  pxDelayedTaskList:                                            ||
||  +-------+    +-------+    +-------+    +-------+              ||
||  | End   |<-->| TaskA |<-->| TaskB |<-->| TaskC |              ||
||  | Marker|    | wake: |    | wake: |    | wake: |              ||
||  |       |    | 1050  |    | 1200  |    | 1500  |              ||
||  +-------+    +-------+    +-------+    +-------+              ||
||                                                                ||
||  Current tick: 1000                                            ||
||  Only need to check first item!                                ||
||                                                                ||
||  At tick 1050: TaskA moves to ready list                       ||
||  At tick 1200: TaskB moves to ready list                       ||
||  At tick 1500: TaskC moves to ready list                       ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

延迟任务管理：

两个延迟列表（处理tick计数器溢出）：xDelayedTaskList1和xDelayedTaskList2。pxDelayedTaskList指向当前使用的，pxOverflowDelayedTaskList指向溢出用的。

列表按唤醒时间排序（使用列表项的xItemValue）：只需检查第一项！

示例：当前tick=1000，TaskA唤醒时间1050，TaskB唤醒时间1200，TaskC唤醒时间1500。

在tick 1050：TaskA移到就绪列表。在tick 1200：TaskB移到就绪列表。

### Tick Overflow Handling

```
WHY TWO DELAYED LISTS?
+------------------------------------------------------------------+
|                                                                  |
|  TickType_t is typically 32-bit unsigned                         |
|  Counts from 0 to 4,294,967,295 then wraps to 0                  |
|                                                                  |
|  At 1000 Hz tick rate:                                           |
|  Overflow every ~49.7 days                                       |
|                                                                  |
|  PROBLEM:                                                        |
|  Current tick: 4,294,967,200                                     |
|  Task delays for 200 ticks                                       |
|  Wake time: 4,294,967,200 + 200 = 104 (wrapped!)                 |
|                                                                  |
|  If we put this in same list sorted by value:                    |
|  Wake 104 would appear BEFORE wake 4,294,967,200                 |
|  Task would wake immediately (wrong!)                            |
|                                                                  |
|  SOLUTION: Two lists                                             |
|  +------------------------------------------------------------+  |
|  | pxDelayedTaskList:        Tasks waking before overflow    |  |
|  | pxOverflowDelayedTaskList: Tasks waking after overflow    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  When tick overflows:                                            |
|  Swap the two list pointers                                      |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么两个延迟列表？

TickType_t通常是32位无符号，从0计数到4,294,967,295然后回绕到0。在1000 Hz tick率下，约49.7天溢出一次。

问题：当前tick=4,294,967,200，任务延迟200 tick，唤醒时间=104（回绕了！）。如果放在同一个按值排序的列表中，104会出现在4,294,967,200之前，任务会立即唤醒（错误！）。

解决方案：两个列表。pxDelayedTaskList存放溢出前唤醒的任务，pxOverflowDelayedTaskList存放溢出后唤醒的任务。当tick溢出时，交换两个列表指针。

---

## 5.6 Context Switch Mechanism

```
CONTEXT SWITCH FLOW (ARM Cortex-M):
+==================================================================+
||                                                                ||
||  TRIGGER: PendSV exception (lowest priority exception)         ||
||                                                                ||
||  1. SAVE CURRENT TASK CONTEXT                                  ||
||  +----------------------------------------------------------+  ||
||  | Hardware automatically saves: R0-R3, R12, LR, PC, xPSR   |  ||
||  | (pushed to task's stack by exception entry)              |  ||
||  |                                                          |  ||
||  | Software (in PendSV handler) saves: R4-R11               |  ||
||  | (pushed manually)                                        |  ||
||  |                                                          |  ||
||  | Update pxCurrentTCB->pxTopOfStack = SP                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  2. SELECT NEXT TASK                                           ||
||  +----------------------------------------------------------+  ||
||  | vTaskSwitchContext()                                     |  ||
||  | -> taskSELECT_HIGHEST_PRIORITY_TASK()                    |  ||
||  | -> pxCurrentTCB = highest priority ready task            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  3. RESTORE NEW TASK CONTEXT                                   ||
||  +----------------------------------------------------------+  ||
||  | SP = pxCurrentTCB->pxTopOfStack                          |  ||
||  | Pop R4-R11 (software restore)                            |  ||
||  | Exception return restores R0-R3, R12, LR, PC, xPSR       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  4. NEW TASK RESUMES EXECUTION                                 ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

上下文切换流程（ARM Cortex-M）：

触发：PendSV异常（最低优先级异常）

1. 保存当前任务上下文：硬件自动保存R0-R3、R12、LR、PC、xPSR（异常入口压入任务栈），软件（在PendSV处理程序中）保存R4-R11（手动压入），更新pxCurrentTCB->pxTopOfStack = SP。

2. 选择下一个任务：vTaskSwitchContext() -> taskSELECT_HIGHEST_PRIORITY_TASK() -> pxCurrentTCB = 最高优先级就绪任务。

3. 恢复新任务上下文：SP = pxCurrentTCB->pxTopOfStack，弹出R4-R11（软件恢复），异常返回恢复R0-R3、R12、LR、PC、xPSR。

4. 新任务恢复执行。

### Why PendSV?

```
WHY USE PENDSV FOR CONTEXT SWITCH?
+------------------------------------------------------------------+
|                                                                  |
|  PendSV characteristics:                                         |
|  - Lowest priority exception (configurable)                      |
|  - Can be "pended" (scheduled for later)                         |
|  - Will not interrupt higher priority ISRs                       |
|                                                                  |
|  SCENARIO: Higher priority ISR makes a task ready                |
|                                                                  |
|  Without PendSV (context switch in ISR):                         |
|  +------------------------------------------------------------+  |
|  | ISR runs                                                   |  |
|  |   -> gives semaphore                                       |  |
|  |   -> high priority task ready                              |  |
|  |   -> CONTEXT SWITCH NOW (while in ISR!)                    |  |
|  |   -> Problems: nested interrupts, stack issues             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  With PendSV:                                                    |
|  +------------------------------------------------------------+  |
|  | ISR runs                                                   |  |
|  |   -> gives semaphore                                       |  |
|  |   -> high priority task ready                              |  |
|  |   -> PEND PendSV (schedule context switch)                 |  |
|  | ISR returns                                                |  |
|  | Any other pending higher-priority ISRs run                 |  |
|  | Finally, PendSV runs (lowest priority)                     |  |
|  |   -> CONTEXT SWITCH NOW (clean, at base level)             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么使用PendSV进行上下文切换？

PendSV特性：最低优先级异常（可配置）、可"挂起"（安排稍后执行）、不会中断更高优先级ISR。

场景：更高优先级ISR使任务就绪。

不用PendSV（在ISR中切换上下文）：ISR运行->给信号量->高优先级任务就绪->立即上下文切换（在ISR中！）->问题：嵌套中断、栈问题。

用PendSV：ISR运行->给信号量->高优先级任务就绪->挂起PendSV->ISR返回->其他更高优先级ISR运行->最后PendSV运行（最低优先级）->上下文切换（干净，在基础级别）。

---

## 5.7 Role of SysTick

```
SYSTICK: THE HEARTBEAT OF FREERTOS:
+==================================================================+
||                                                                ||
||  SysTick fires every 1/configTICK_RATE_HZ seconds              ||
||  (typically every 1ms at 1000 Hz)                              ||
||                                                                ||
||  SYSTICK HANDLER RESPONSIBILITIES:                             ||
||  +----------------------------------------------------------+  ||
||  | 1. Increment xTickCount                                  |  ||
||  |                                                          |  ||
||  | 2. Check delayed tasks                                   |  ||
||  |    - First task in pxDelayedTaskList                     |  ||
||  |    - If wake time <= xTickCount, move to ready list      |  ||
||  |    - Repeat until first task's wake time > xTickCount    |  ||
||  |                                                          |  ||
||  | 3. Check if context switch needed                        |  ||
||  |    - Time slicing: same priority task waiting?           |  ||
||  |    - Higher priority task became ready?                  |  ||
||  |    - If yes, pend PendSV                                 |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CODE FLOW (simplified):                                       ||
||  +----------------------------------------------------------+  ||
||  | void xPortSysTickHandler(void)                           |  ||
||  | {                                                        |  ||
||  |     portSET_INTERRUPT_MASK();  // Enter critical         |  ||
||  |     if (xTaskIncrementTick() == pdTRUE)                  |  ||
||  |     {                                                    |  ||
||  |         portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;  |  ||
||  |         // Pend PendSV for context switch                |  ||
||  |     }                                                    |  ||
||  |     portCLEAR_INTERRUPT_MASK();                          |  ||
||  | }                                                        |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

SysTick：FreeRTOS的心跳：

SysTick每1/configTICK_RATE_HZ秒触发一次（通常1000 Hz时每1ms）。

SysTick处理程序职责：
1. 递增xTickCount
2. 检查延迟任务：检查pxDelayedTaskList的第一个任务，如果唤醒时间<=xTickCount，移到就绪列表，重复直到第一个任务唤醒时间>xTickCount
3. 检查是否需要上下文切换：时间片（同优先级任务等待？）、更高优先级任务就绪？如果是，挂起PendSV

---

## 5.8 Why the Scheduler Is Simple by Design

```
SIMPLICITY IS A FEATURE:
+==================================================================+
||                                                                ||
||  FreeRTOS scheduler does NOT have:                             ||
||  +----------------------------------------------------------+  ||
||  | - Dynamic priority adjustment (like Linux CFS)           |  ||
||  | - Deadline-based scheduling (like EDF)                   |  ||
||  | - CPU affinity (except in SMP version)                   |  ||
||  | - Fairness guarantees                                    |  ||
||  | - Load balancing                                         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  WHY NOT?                                                      ||
||                                                                ||
||  1. DETERMINISM over throughput                                ||
||     Complex schedulers have unpredictable behavior             ||
||     Simple scheduler: O(1) always                              ||
||                                                                ||
||  2. SMALL CODE SIZE                                            ||
||     Complex scheduler might be 10KB+                           ||
||     FreeRTOS scheduler: ~2KB                                   ||
||                                                                ||
||  3. DEVELOPER CONTROL                                          ||
||     You assign priorities                                      ||
||     You know exactly what will run and when                    ||
||     No "magic" that changes behavior                           ||
||                                                                ||
||  4. VERIFICATION                                               ||
||     Simple code can be formally verified                       ||
||     SafeRTOS is a formally verified version                    ||
||                                                                ||
||  "The scheduler is simple because simplicity enables           ||
||   determinism, and determinism is what real-time requires."    ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

简单是一个特性：

FreeRTOS调度器没有：动态优先级调整（像Linux CFS）、基于截止时间的调度（像EDF）、CPU亲和性（除SMP版本外）、公平性保证、负载均衡。

为什么没有？

1. 确定性优于吞吐量：复杂调度器有不可预测行为，简单调度器总是O(1)。

2. 小代码大小：复杂调度器可能10KB+，FreeRTOS调度器约2KB。

3. 开发者控制：你分配优先级，你确切知道什么会运行和何时运行，无"魔法"改变行为。

4. 验证：简单代码可形式化验证，SafeRTOS是形式化验证版本。

"调度器简单是因为简单性带来确定性，而确定性是实时系统所需要的。"

---

## Summary

```
SCHEDULING KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  FUNDAMENTAL RULE:                                             ||
||  Highest priority ready task always runs                       ||
||                                                                ||
||  PREEMPTION POINTS:                                            ||
||  - Blocking API calls                                          ||
||  - Higher priority task becomes ready                          ||
||  - Tick interrupt (time slicing)                               ||
||  - Explicit yield                                              ||
||                                                                ||
||  KEY DATA STRUCTURES:                                          ||
||  - pxReadyTasksLists[]: One list per priority                  ||
||  - pxDelayedTaskList: Sorted by wake time                      ||
||  - pxCurrentTCB: Currently running task                        ||
||  - uxTopReadyPriority: Highest non-empty ready list            ||
||                                                                ||
||  CONTEXT SWITCH MECHANISM:                                     ||
||  - PendSV exception (ARM)                                      ||
||  - Save R4-R11 + update stack pointer                          ||
||  - Select new task                                             ||
||  - Restore new task's registers                                ||
||                                                                ||
||  DESIGN PHILOSOPHY:                                            ||
||  Simple, deterministic, O(1), verifiable                       ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Timing and Delays](06-timing-and-delays.md)
