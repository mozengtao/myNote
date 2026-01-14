# Section 11: Porting FreeRTOS (Conceptual)

FreeRTOS is designed to be portable across different processors and compilers. Understanding what a port requires helps you use existing ports correctly and create new ones if needed.

## 11.1 What Must Be Implemented to Port FreeRTOS

### Port Layer Components

```
PORT LAYER REQUIREMENTS:
+------------------------------------------------------------------+
|                                                                  |
|  portmacro.h: TYPE DEFINITIONS AND MACROS                        |
|  +------------------------------------------------------------+  |
|  | StackType_t     - Type for stack elements                  |  |
|  | BaseType_t      - Efficient native type for booleans       |  |
|  | UBaseType_t     - Unsigned version                         |  |
|  | TickType_t      - Type for tick counter                    |  |
|  | portSTACK_GROWTH - Direction stack grows (-1 or +1)        |  |
|  | portBYTE_ALIGNMENT - Required memory alignment             |  |
|  | portYIELD()     - Trigger context switch                   |  |
|  | portENTER_CRITICAL() - Enter critical section              |  |
|  | portEXIT_CRITICAL() - Exit critical section                |  |
|  | portDISABLE_INTERRUPTS() - Disable interrupts              |  |
|  | portENABLE_INTERRUPTS() - Enable interrupts                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  port.c: IMPLEMENTATION                                          |
|  +------------------------------------------------------------+  |
|  | pxPortInitialiseStack() - Set up initial task stack        |  |
|  | xPortStartScheduler() - Start first task, tick timer       |  |
|  | vPortEndScheduler() - Stop scheduler (rarely used)         |  |
|  | Context switch handler (ISR, usually in assembly)          |  |
|  | Tick handler (ISR)                                         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

## 11.2 CPU Requirements

```
MINIMUM CPU REQUIREMENTS:
+------------------------------------------------------------------+
|                                                                  |
|  1. STACK POINTER                                                |
|     - CPU must have a stack pointer register                     |
|     - Must be able to save/restore it                            |
|                                                                  |
|  2. INTERRUPT CAPABILITY                                         |
|     - Must be able to generate periodic interrupt (tick)         |
|     - Must be able to disable/enable interrupts                  |
|                                                                  |
|  3. CONTEXT SAVE/RESTORE                                         |
|     - Must be able to save all registers to stack                |
|     - Must be able to restore all registers from stack           |
|                                                                  |
|  4. ENOUGH RAM                                                   |
|     - Minimum ~1-2KB for kernel + one task                       |
|     - Each task needs stack space                                |
|                                                                  |
+------------------------------------------------------------------+
```

## 11.3 Timer Requirements

```
TICK TIMER REQUIREMENTS:
+------------------------------------------------------------------+
|                                                                  |
|  1. PERIODIC INTERRUPT SOURCE                                    |
|     - Typically SysTick on Cortex-M                              |
|     - Or general-purpose timer                                   |
|     - Frequency = configTICK_RATE_HZ                             |
|                                                                  |
|  2. PRIORITY                                                     |
|     - Must be at or below configMAX_SYSCALL_INTERRUPT_PRIORITY   |
|     - So FreeRTOS critical sections can mask it                  |
|                                                                  |
|  3. SETUP IN xPortStartScheduler()                               |
|     - Configure timer period                                     |
|     - Enable timer interrupt                                     |
|     - Start timer                                                |
|                                                                  |
+------------------------------------------------------------------+
```

## 11.4 Context Save/Restore

```
CONTEXT SWITCH IMPLEMENTATION:
+------------------------------------------------------------------+
|                                                                  |
|  CORTEX-M EXAMPLE:                                               |
|                                                                  |
|  Hardware saves (automatically on exception entry):              |
|  +------------------------------------------------------------+  |
|  | R0-R3, R12, LR, PC, xPSR (8 registers)                     |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Software saves (in PendSV handler):                             |
|  +------------------------------------------------------------+  |
|  | R4-R11 (8 registers)                                       |  |
|  | + FPU registers if using floating point                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  PendSV Handler pseudocode:                                      |
|  +------------------------------------------------------------+  |
|  | 1. Get PSP (Process Stack Pointer)                         |  |
|  | 2. Push R4-R11 to task's stack                             |  |
|  | 3. Save PSP to current TCB (pxCurrentTCB->pxTopOfStack)    |  |
|  | 4. Call vTaskSwitchContext() to select new task            |  |
|  | 5. Load PSP from new TCB                                   |  |
|  | 6. Pop R4-R11 from new task's stack                        |  |
|  | 7. Return (hardware restores R0-R3, R12, LR, PC, xPSR)     |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

移植FreeRTOS需要实现：

portmacro.h：类型定义（StackType_t、BaseType_t、TickType_t等）和宏（栈增长方向、对齐、yield、临界区、中断控制）。

port.c：栈初始化(pxPortInitialiseStack)、调度器启动(xPortStartScheduler)、上下文切换处理程序（通常是汇编）、tick处理程序。

CPU要求：栈指针寄存器、中断能力、上下文保存/恢复能力、足够RAM。

定时器要求：周期性中断源、正确的优先级、在xPortStartScheduler中设置。

上下文切换：Cortex-M上硬件自动保存R0-R3,R12,LR,PC,xPSR，软件在PendSV中保存R4-R11，切换TCB，恢复新任务的寄存器。

---

## Summary

```
PORTING CHECKLIST:
+==================================================================+
||                                                                ||
||  [ ] portmacro.h with correct types and macros                ||
||  [ ] port.c with stack initialization                         ||
||  [ ] Context switch handler (PendSV on Cortex-M)              ||
||  [ ] Tick timer setup and handler (SysTick on Cortex-M)       ||
||  [ ] Critical section implementation                          ||
||  [ ] FreeRTOSConfig.h for target                             ||
||  [ ] Test with simple blinky example                         ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Using FreeRTOS in Real Projects](12-using-freertos-in-real-projects.md)
