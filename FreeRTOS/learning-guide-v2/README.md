# FreeRTOS Learning Guide (v2)

A comprehensive guide to learning FreeRTOS from its source code.

## Overview

This guide teaches FreeRTOS systematically, using the actual source code as the primary learning material. It is designed for developers who know C well and understand basic OS concepts, but are new to FreeRTOS specifically.

## Design Principles

This guide follows these principles:

- **Source code first**: Explanations reference actual files and data structures
- **Why before how**: Design intent explained before implementation details
- **Bottom-up approach**: From hardware through kernel to application
- **Constraint-aware**: Always connects design to embedded constraints

## Contents

| Section | Title | Focus |
|---------|-------|-------|
| [01](01-what-freertos-is-and-is-not.md) | What FreeRTOS Is — and What It Is NOT | Problem domain, comparisons, terminology |
| [02](02-high-level-architecture.md) | High-Level FreeRTOS Architecture | Components, contexts, configuration |
| [03](03-source-tree-walkthrough.md) | FreeRTOS Source Tree Walkthrough | File organization, reading order |
| [04](04-task-model.md) | Task Model: The Core Abstraction | TCB, states, stack, creation |
| [05](05-scheduling.md) | Scheduling: How FreeRTOS Actually Runs Code | Priorities, lists, context switch |
| [06](06-timing-and-delays.md) | Timing and Delays | Tick, delays, jitter, tickless |
| [07](07-queues-semaphores-mutexes.md) | Queues, Semaphores, and Mutexes | Unifying model, priority inheritance |
| [08](08-interrupts-and-isr-safe-apis.md) | Interrupts and ISR-Safe APIs | Context rules, FromISR pattern |
| [09](09-memory-management.md) | Memory Management in FreeRTOS | Heap models, static allocation |
| [10](10-configuration-philosophy.md) | Configuration Philosophy | FreeRTOSConfig.h deep dive |
| [11](11-real-projects.md) | How FreeRTOS Is Used in Real Projects | Architecture, patterns, drivers |
| [12](12-common-mistakes.md) | Common Beginner Mistakes | Root causes and fixes |
| [13](13-learning-from-source.md) | How to Learn FreeRTOS from Source Code | Reading order, debugging, experiments |
| [14](14-final-mental-model.md) | Final Mental Model | Guarantees, when to use |

## Quick Reference

### Key Source Files

| File | Purpose |
|------|---------|
| `list.c` | Generic doubly-linked list (foundation) |
| `tasks.c` | Scheduler, task management (~3000 lines) |
| `queue.c` | Queues, semaphores, mutexes (~2500 lines) |
| `timers.c` | Software timer implementation |
| `event_groups.c` | Event group implementation |
| `portable/[compiler]/[arch]/port.c` | Context switch, tick, critical sections |
| `portable/[compiler]/[arch]/portmacro.h` | Type definitions, macros |
| `portable/MemMang/heap_4.c` | Recommended heap implementation |

### Key Data Structures

```
TCB_t (Task Control Block)
+-- pxTopOfStack      Stack pointer (MUST be first)
+-- xStateListItem    For ready/blocked/delayed lists
+-- xEventListItem    For queue/semaphore wait lists
+-- uxPriority        Task priority
+-- pxStack           Stack base address
+-- pcTaskName        Debug name

Queue_t (also Semaphore, Mutex)
+-- pcHead            Buffer start
+-- uxMessagesWaiting Item count
+-- uxLength          Max items
+-- uxItemSize        Bytes per item (0 for semaphore)
+-- xTasksWaitingToReceive/Send  Wait lists

List_t / ListItem_t
+-- Intrusive doubly-linked list
+-- Used by all kernel components
```

### Scheduler Lists

```
pxReadyTasksLists[configMAX_PRIORITIES]  - Tasks ready to run
xDelayedTaskList1, xDelayedTaskList2     - Tasks waiting for time
xPendingReadyList                        - Ready while scheduler suspended
xSuspendedTaskList                       - Explicitly suspended tasks
pxCurrentTCB                             - Currently running task
```

### Context Switch Flow (ARM Cortex-M)

```
1. PendSV triggered (lowest priority exception)
2. Hardware saves R0-R3, R12, LR, PC, xPSR to current stack
3. Handler saves R4-R11, updates current TCB's stack pointer
4. vTaskSwitchContext() selects next task
5. Handler restores R4-R11 from new task's stack
6. Exception return restores R0-R3, R12, LR, PC, xPSR
7. New task resumes execution
```

### ISR-Safe API Pattern

```c
void MyISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* Do ISR work */
    xQueueSendFromISR(queue, &data, &xHigherPriorityTaskWoken);
    
    /* Yield if higher priority task woken */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Learning Path

**Week 1: Foundation**
- Read sections 1-3
- Study `list.c` (small, self-contained)
- Set up a development environment

**Week 2: Core Concepts**
- Read sections 4-6
- Study `tasks.c` structures
- Debug task creation and context switch

**Week 3: Synchronization**
- Read sections 7-8
- Study `queue.c` 
- Experiment with queues and semaphores

**Week 4: Integration**
- Read sections 9-11
- Study a port layer
- Build a multi-task application

**Week 5: Mastery**
- Read sections 12-14
- Explore edge cases
- Read more source code

## Target Audience

This guide is designed for:
- Embedded systems developers new to FreeRTOS
- Developers moving from super-loop to RTOS
- Engineers who want to understand RTOS internals
- Those preparing for safety-critical development

## Prerequisites

- Strong C programming skills
- Understanding of:
  - Stack and heap concepts
  - Interrupt handling
  - Basic CPU architecture (registers, context)
  - Pointers and data structures

## Note on Diagrams

All diagrams in this guide use ASCII art for maximum portability and version control friendliness. Each diagram includes Chinese explanations (中文说明) for bilingual accessibility.

## License

This guide is part of the FreeRTOS documentation. FreeRTOS is licensed under the MIT license.
