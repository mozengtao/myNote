# Section 6: Timing and Delays

## 6.1 Tick-Based Time

```
THE TICK: FUNDAMENTAL TIME UNIT:
+==================================================================+
||                                                                ||
||  FreeRTOS measures time in TICKS, not milliseconds             ||
||                                                                ||
||  configTICK_RATE_HZ = 1000  ->  1 tick = 1 ms                  ||
||  configTICK_RATE_HZ = 100   ->  1 tick = 10 ms                 ||
||  configTICK_RATE_HZ = 10    ->  1 tick = 100 ms                ||
||                                                                ||
||  TIME FLOW:                                                    ||
||                                                                ||
||  xTickCount:  0    1    2    3    4    5    6    7    8        ||
||               |    |    |    |    |    |    |    |    |        ||
||  SysTick:     ^    ^    ^    ^    ^    ^    ^    ^    ^        ||
||               |    |    |    |    |    |    |    |    |        ||
||             ISR  ISR  ISR  ISR  ISR  ISR  ISR  ISR  ISR        ||
||                                                                ||
||  Between ticks: Tasks run, ISRs happen                         ||
||  At each tick:  xTickCount++, check delayed tasks              ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Tick：基本时间单位。FreeRTOS用tick而不是毫秒来测量时间。

configTICK_RATE_HZ = 1000 -> 1 tick = 1 ms
configTICK_RATE_HZ = 100  -> 1 tick = 10 ms
configTICK_RATE_HZ = 10   -> 1 tick = 100 ms

时间流：xTickCount从0开始，每次SysTick中断递增。tick之间：任务运行，ISR发生。每个tick：xTickCount++，检查延迟任务。

### Converting Between Ticks and Time

```c
/* The proper way to convert time to ticks */
#define pdMS_TO_TICKS(xTimeInMs)    \
    ((TickType_t)(((TickType_t)(xTimeInMs) * \
    (TickType_t)configTICK_RATE_HZ) / (TickType_t)1000U))

/* Examples at configTICK_RATE_HZ = 1000 */
pdMS_TO_TICKS(1)     /* = 1 tick */
pdMS_TO_TICKS(100)   /* = 100 ticks */
pdMS_TO_TICKS(1000)  /* = 1000 ticks */

/* Examples at configTICK_RATE_HZ = 100 */
pdMS_TO_TICKS(1)     /* = 0 ticks! (rounded down) */
pdMS_TO_TICKS(10)    /* = 1 tick */
pdMS_TO_TICKS(100)   /* = 10 ticks */
```

```
TICK RATE CONSIDERATIONS:
+------------------------------------------------------------------+
|                                                                  |
|  HIGHER TICK RATE (e.g., 1000 Hz):                               |
|  + Finer time resolution (1ms granularity)                       |
|  + More responsive delays                                        |
|  - More CPU overhead (ISR every 1ms)                             |
|  - More power consumption                                        |
|                                                                  |
|  LOWER TICK RATE (e.g., 100 Hz):                                 |
|  + Less CPU overhead                                             |
|  + Lower power consumption                                       |
|  - Coarser time resolution (10ms granularity)                    |
|  - vTaskDelay(1) delays 10ms minimum                             |
|                                                                  |
|  TYPICAL CHOICE: 100-1000 Hz                                     |
|  - 1000 Hz for responsive systems                                |
|  - 100 Hz for battery-powered devices                            |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Tick率考虑：

高tick率（如1000 Hz）：更细时间分辨率（1ms粒度）、更响应的延迟，但更多CPU开销（每1ms一个ISR）、更多功耗。

低tick率（如100 Hz）：更少CPU开销、更低功耗，但更粗时间分辨率（10ms粒度）、vTaskDelay(1)最少延迟10ms。

典型选择：100-1000 Hz。1000 Hz用于响应系统，100 Hz用于电池供电设备。

---

## 6.2 vTaskDelay() vs vTaskDelayUntil()

### vTaskDelay() - Relative Delay

```c
void vTaskDelay(TickType_t xTicksToDelay);

/* Example: Delay 100ms */
void MyTask(void *p)
{
    for(;;)
    {
        do_work();              /* Takes variable time */
        vTaskDelay(pdMS_TO_TICKS(100));  /* Then delay 100ms */
    }
}
```

```
vTaskDelay() TIMING:
+==================================================================+
||                                                                ||
||  do_work() takes 30ms, delay is 100ms:                         ||
||                                                                ||
||  |---30ms---|--------100ms--------|---30ms---|----100ms----    ||
||  | do_work  |      BLOCKED        | do_work  |   BLOCKED       ||
||  |          |    (vTaskDelay)     |          | (vTaskDelay)    ||
||  |----------|---------------------|----------|-------------    ||
||  ^          ^                     ^          ^                 ||
||  0ms       30ms                 130ms      160ms               ||
||                                                                ||
||  Period = work_time + delay_time = 130ms (variable!)           ||
||                                                                ||
||  PROBLEM: Period varies with work_time                         ||
||  If do_work() takes 50ms:                                      ||
||  Period = 50 + 100 = 150ms                                     ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

vTaskDelay() - 相对延迟：延迟从调用时开始计算。

示例：do_work()花30ms，延迟100ms。周期=工作时间+延迟时间=130ms（可变！）

问题：周期随工作时间变化。如果do_work()花50ms：周期=50+100=150ms。

### vTaskDelayUntil() - Absolute Delay

```c
void vTaskDelayUntil(TickType_t *pxPreviousWakeTime,
                     TickType_t xTimeIncrement);

/* Example: Wake every 100ms exactly */
void MyTask(void *p)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        do_work();              /* Takes variable time */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
```

```
vTaskDelayUntil() TIMING:
+==================================================================+
||                                                                ||
||  xLastWakeTime starts at 0, increment is 100ms:                ||
||                                                                ||
||  |---30ms---|----70ms----|---30ms---|----70ms----|             ||
||  | do_work  |  BLOCKED   | do_work  |  BLOCKED   |             ||
||  |          | (delay 70) |          | (delay 70) |             ||
||  |----------|------------|----------|------------|             ||
||  ^                       ^                       ^             ||
||  0ms                   100ms                   200ms           ||
||                                                                ||
||  Period = EXACTLY 100ms (regardless of work_time)              ||
||                                                                ||
||  xLastWakeTime updates: 0 -> 100 -> 200 -> 300 -> ...          ||
||                                                                ||
||  IF do_work() takes 50ms:                                      ||
||  |---50ms---|---50ms----|---50ms---|---50ms----|               ||
||  | do_work  | BLOCKED   | do_work  | BLOCKED   |               ||
||  |----------|-----------|----------|-----------|               ||
||  ^                      ^                      ^               ||
||  0ms                  100ms                  200ms             ||
||                                                                ||
||  Period still EXACTLY 100ms!                                   ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

vTaskDelayUntil() - 绝对延迟：延迟到绝对时间点。

示例：xLastWakeTime从0开始，增量100ms。周期=正好100ms（无论工作时间多少）。

xLastWakeTime更新：0 -> 100 -> 200 -> 300 -> ...

如果do_work()花50ms：周期仍然正好100ms！

### Why Absolute Delays Matter

```
REAL-TIME PERIODIC TASKS:
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO: Control loop must run at exactly 100 Hz (10ms)        |
|                                                                  |
|  Using vTaskDelay(pdMS_TO_TICKS(10)):                            |
|  - Work takes 2-5ms (variable)                                   |
|  - Actual period: 12-15ms (DRIFT accumulates!)                   |
|  - After 100 iterations: 1200-1500ms instead of 1000ms           |
|  - Control loop timing corrupted                                 |
|                                                                  |
|  Using vTaskDelayUntil(..., pdMS_TO_TICKS(10)):                  |
|  - Work takes 2-5ms (variable)                                   |
|  - Actual period: EXACTLY 10ms                                   |
|  - After 100 iterations: EXACTLY 1000ms                          |
|  - Control loop timing preserved                                 |
|                                                                  |
|  RULE:                                                           |
|  +------------------------------------------------------------+  |
|  | Use vTaskDelay() for "wait at least N ticks"               |  |
|  | Use vTaskDelayUntil() for "run every N ticks"              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么绝对延迟重要？

场景：控制循环必须正好100 Hz运行（10ms）。

使用vTaskDelay(10)：工作花2-5ms（可变），实际周期12-15ms（漂移累积！），100次迭代后：1200-1500ms而不是1000ms，控制循环定时损坏。

使用vTaskDelayUntil(..., 10)：工作花2-5ms（可变），实际周期正好10ms，100次迭代后：正好1000ms，控制循环定时保持。

规则：用vTaskDelay()表示"至少等待N个tick"，用vTaskDelayUntil()表示"每N个tick运行一次"。

---

## 6.3 Tick Rate Trade-offs

```
TICK RATE SELECTION GUIDE:
+==================================================================+
||                                                                ||
||  configTICK_RATE_HZ   Resolution   ISR Overhead   Power        ||
||  ------------------   ----------   ------------   -----        ||
||  10 Hz                100 ms       Very Low       Excellent    ||
||  100 Hz               10 ms        Low            Good         ||
||  1000 Hz              1 ms         Medium         Fair         ||
||  10000 Hz             0.1 ms       High           Poor         ||
||                                                                ||
||  OVERHEAD CALCULATION (100 MHz CPU):                           ||
||  +----------------------------------------------------------+  ||
||  | Tick ISR takes ~200 cycles (typical)                     |  ||
||  |                                                          |  ||
||  | At 1000 Hz:                                              |  ||
||  | 200 cycles / 100,000,000 cycles/sec * 1000 Hz = 0.2%     |  ||
||  |                                                          |  ||
||  | At 10000 Hz:                                             |  ||
||  | 200 cycles / 100,000,000 cycles/sec * 10000 Hz = 2%      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  COMMON CHOICES:                                               ||
||  +----------------------------------------------------------+  ||
||  | Battery device:     100-250 Hz                           |  ||
||  | Industrial control: 1000 Hz                              |  ||
||  | Audio processing:   1000-4000 Hz                         |  ||
||  | Motor control:      Up to 10000 Hz                       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Tick率选择指南：

| configTICK_RATE_HZ | 分辨率 | ISR开销 | 功耗 |
|-------------------|-------|--------|------|
| 10 Hz | 100 ms | 非常低 | 优秀 |
| 100 Hz | 10 ms | 低 | 良好 |
| 1000 Hz | 1 ms | 中等 | 一般 |
| 10000 Hz | 0.1 ms | 高 | 差 |

开销计算（100 MHz CPU）：Tick ISR花约200周期。在1000 Hz：0.2%开销。在10000 Hz：2%开销。

常见选择：电池设备100-250 Hz，工业控制1000 Hz，音频处理1000-4000 Hz，电机控制最高10000 Hz。

---

## 6.4 Jitter Sources

```
WHAT CAUSES TIMING JITTER:
+==================================================================+
||                                                                ||
||  1. INTERRUPT LATENCY                                          ||
||  +----------------------------------------------------------+  ||
||  | SysTick ready to fire at T=1000                          |  ||
||  | But higher priority ISR running until T=1005             |  ||
||  | SysTick finally runs at T=1005                           |  ||
||  | Jitter: 5 ticks                                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  2. CRITICAL SECTIONS                                          ||
||  +----------------------------------------------------------+  ||
||  | Task in critical section (interrupts disabled)           |  ||
||  | SysTick pending but cannot run                           |  ||
||  | Critical section ends, SysTick runs late                 |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  3. TICK RESOLUTION                                            ||
||  +----------------------------------------------------------+  ||
||  | vTaskDelay(pdMS_TO_TICKS(5)) at 1000 Hz = 5 ticks        |  ||
||  | But: delay starts between ticks                          |  ||
||  |                                                          |  ||
||  |   Tick:  |-------|-------|-------|-------|-------|       |  ||
||  |          0       1       2       3       4       5       |  ||
||  |                ^                           ^              |  ||
||  |             delay                       wake             |  ||
||  |            starts                       time             |  ||
||  |          (at 0.8)                      (at 5)            |  ||
||  |                                                          |  ||
||  | Actual delay: 4.2 ticks (not 5!)                         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  4. SCHEDULER OVERHEAD                                         ||
||  +----------------------------------------------------------+  ||
||  | Context switch takes time                                |  ||
||  | Multiple tasks at same priority = round-robin delay      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

什么导致定时抖动：

1. 中断延迟：SysTick准备在T=1000触发，但更高优先级ISR运行到T=1005，SysTick最终在T=1005运行，抖动5个tick。

2. 临界区：任务在临界区（中断禁用），SysTick挂起但不能运行，临界区结束后SysTick延迟运行。

3. Tick分辨率：vTaskDelay(5)在1000 Hz=5 tick，但延迟在tick之间开始。如果在tick 0.8开始，唤醒在tick 5，实际延迟4.2 tick（不是5！）。

4. 调度器开销：上下文切换需要时间，同优先级多任务=轮转延迟。

### Minimizing Jitter

```
JITTER REDUCTION STRATEGIES:
+------------------------------------------------------------------+
|                                                                  |
|  1. Keep ISRs SHORT                                              |
|     - Do minimum work in ISR                                     |
|     - Defer processing to task via queue/semaphore               |
|                                                                  |
|  2. Keep CRITICAL SECTIONS SHORT                                 |
|     - Only protect what's necessary                              |
|     - Use finer-grained locking                                  |
|                                                                  |
|  3. Use HIGHER PRIORITY for time-critical tasks                  |
|     - They preempt lower priority tasks immediately              |
|     - Minimize time between event and response                   |
|                                                                  |
|  4. Use HARDWARE TIMERS for sub-tick precision                   |
|     - FreeRTOS tick is coarse (1ms typically)                    |
|     - Hardware timer can be 1us or better                        |
|                                                                  |
|  5. Consider TICKLESS mode for low power                         |
|     - But adds complexity                                        |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

减少抖动策略：

1. 保持ISR短：在ISR中做最少工作，通过队列/信号量推迟处理到任务。
2. 保持临界区短：只保护必要的，使用更细粒度锁定。
3. 对时间关键任务使用更高优先级：它们立即抢占低优先级任务，最小化事件和响应之间的时间。
4. 对亚tick精度使用硬件定时器：FreeRTOS tick粗（通常1ms），硬件定时器可达1us或更好。
5. 考虑低功耗的无tick模式：但增加复杂性。

---

## 6.5 Tickless Idle Mode (Conceptual)

```
TICKLESS IDLE: SAVING POWER:
+==================================================================+
||                                                                ||
||  NORMAL MODE (ticks keep running):                             ||
||                                                                ||
||  |-----|-----|-----|-----|-----|-----|-----|-----|-----|       ||
||  ^     ^     ^     ^     ^     ^     ^     ^     ^     ^       ||
||  |     |     |     |     |     |     |     |     |     |       ||
|| tick  tick  tick  tick  tick  tick  tick  tick  tick  tick     ||
||                                                                ||
||  [Idle][Idle][Idle][Idle][Idle][Idle][Idle][Idle][Idle][Task]  ||
||                                                                ||
||  CPU wakes up every tick even when idle!                       ||
||                                                                ||
||  TICKLESS MODE (configUSE_TICKLESS_IDLE = 1):                  ||
||                                                                ||
||  When entering idle:                                           ||
||  1. Check: next task wakes in 50 ticks                         ||
||  2. Program timer to wake in 50 ticks                          ||
||  3. Stop SysTick                                               ||
||  4. Enter low-power mode                                       ||
||  5. ... CPU sleeps for 50 ticks ...                            ||
||  6. Timer fires, CPU wakes                                     ||
||  7. Add 50 to xTickCount                                       ||
||  8. Resume normal operation                                    ||
||                                                                ||
||  |-----|---------------------------------------------|-----|   ||
||  ^                                                   ^     ^   ||
|| tick                                               tick  tick  ||
||                                                                ||
||  [Task][......Deep Sleep (50 tick periods)......][Idle][Task]  ||
||                                                                ||
||  CPU sleeps for extended periods = major power savings         ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

无tick空闲模式：省电。

正常模式（tick持续运行）：CPU每个tick都醒来，即使空闲时！

无tick模式（configUSE_TICKLESS_IDLE = 1）：
1. 进入空闲时检查：下个任务50 tick后唤醒
2. 编程定时器50 tick后唤醒
3. 停止SysTick
4. 进入低功耗模式
5. ...CPU睡眠50个tick...
6. 定时器触发，CPU唤醒
7. 给xTickCount加50
8. 恢复正常操作

CPU长时间睡眠=显著省电。

### Tickless Mode Trade-offs

| Aspect | Normal Mode | Tickless Mode |
|--------|-------------|---------------|
| Power consumption | Higher | Much lower |
| Implementation complexity | Simple | Complex |
| Timing accuracy | Better | May have more jitter |
| Wake-up latency | Constant (1 tick max) | Variable |
| Best for | Performance-critical | Battery-powered |

---

## Summary

```
TIMING KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  TIME UNIT:                                                    ||
||  - Tick, not milliseconds                                      ||
||  - Use pdMS_TO_TICKS() for conversion                          ||
||  - Tick rate is compile-time choice (100-1000 Hz typical)      ||
||                                                                ||
||  DELAY FUNCTIONS:                                              ||
||  - vTaskDelay(): relative, "wait at least N ticks"             ||
||  - vTaskDelayUntil(): absolute, "run every N ticks"            ||
||  - Choose based on whether you need periodic execution         ||
||                                                                ||
||  JITTER SOURCES:                                               ||
||  - Interrupt latency                                           ||
||  - Critical sections                                           ||
||  - Tick resolution (worst case: 1 tick - epsilon)              ||
||  - Scheduler overhead                                          ||
||                                                                ||
||  TICKLESS MODE:                                                ||
||  - Saves power by suppressing ticks during idle                ||
||  - Port-specific implementation                                ||
||  - Trade-off: complexity vs power savings                      ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Queues, Semaphores, and Mutexes](07-queues-semaphores-mutexes.md)
