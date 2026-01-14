# FreeRTOS Kernel Learning Guide

A comprehensive guide to understanding FreeRTOS internals from source code.

## Overview

This learning guide teaches FreeRTOS **systematically from its source code**. By the end, you will:

- Understand **why FreeRTOS is structured the way it is**
- Navigate the FreeRTOS source tree with confidence
- Understand how the **scheduler, tasks, queues, timers, and interrupts** work internally
- Know how to **port FreeRTOS to a new platform**
- Correctly use FreeRTOS to build **real embedded products**
- Avoid common beginner and intermediate FreeRTOS mistakes

## Table of Contents

### Part 1: Foundations

1. [What Problem FreeRTOS Is Solving](01-what-problem-freertos-is-solving.md)
   - Life without an RTOS
   - What an RTOS provides
   - Why FreeRTOS exists

2. [High-Level FreeRTOS Architecture](02-high-level-architecture.md)
   - Big picture overview
   - Core architectural principles
   - What FreeRTOS is NOT

3. [FreeRTOS Source Tree Tour](03-source-tree-tour.md)
   - Core source files (tasks.c, queue.c, list.c, etc.)
   - Header files
   - Portable layer
   - Demo and examples

### Part 2: Core Mechanisms

4. [The Scheduler (Heart of FreeRTOS)](04-scheduler.md)
   - Task states
   - Task Control Block (TCB)
   - Priority-based preemptive scheduling
   - Context switching

5. [Lists: The Hidden Backbone](05-lists.md)
   - Why lists matter
   - Intrusive linked lists
   - How lists power everything

6. [Time Management](06-time-management.md)
   - The tick
   - Delays and timeouts
   - Tick processing

7. [Inter-Task Communication](07-inter-task-communication.md)
   - Queues
   - Semaphores and mutexes
   - Event groups

8. [Timers and Deferred Work](08-timers-and-deferred-work.md)
   - Software timers
   - Timer daemon task
   - Why callbacks run in task context

### Part 3: Critical Topics

9. [Interrupts and FreeRTOS](09-interrupts-and-freertos.md)
   - ISR rules
   - FromISR API functions
   - Interrupt priority rules
   - Why violating rules breaks the kernel

10. [Memory Management](10-memory-management.md)
    - Heap models (heap_1 through heap_5)
    - Stack management
    - Stack overflow detection

### Part 4: Practical Application

11. [Porting FreeRTOS](11-porting-freertos.md)
    - What must be implemented
    - CPU and timer requirements
    - Context save/restore

12. [Using FreeRTOS in Real Projects](12-using-freertos-in-real-projects.md)
    - Typical application architecture
    - Common design patterns
    - Common beginner mistakes

13. [Debugging and Observability](13-debugging-and-observability.md)
    - Debugging task issues
    - Stack watermarking
    - Trace tools

14. [Mental Model Summary](14-mental-model-summary.md)
    - Complete picture
    - Key invariants
    - Expert thinking
    - When NOT to use an RTOS

## How to Use This Guide

1. **Read in order** - Each section builds on previous ones
2. **Follow along in source code** - Open the referenced files as you read
3. **Focus on WHY** - Understand design decisions, not just API
4. **Build something** - Apply concepts to a real project

## Prerequisites

- Basic C programming
- Understanding of embedded systems concepts
- Access to FreeRTOS kernel source code

## Source Code Reference

This guide references the FreeRTOS-Kernel repository structure:

```
FreeRTOS-Kernel/
+-- tasks.c              # Task management
+-- queue.c              # Queues, semaphores, mutexes
+-- list.c               # Linked list implementation
+-- timers.c             # Software timers
+-- event_groups.c       # Event groups
+-- stream_buffer.c      # Stream and message buffers
+-- include/             # Header files
+-- portable/            # Port layer
    +-- GCC/ARM_CM4F/    # Example port
    +-- MemMang/         # Memory allocators
```

## License

This learning guide is provided for educational purposes alongside the MIT-licensed FreeRTOS kernel.
