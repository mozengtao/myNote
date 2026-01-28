# Final Mental Model: Command/Deferred Execution Pattern

## One-Paragraph Summary

The Command/Deferred Execution pattern encapsulates work to be performed later, typically in a different execution context. The kernel provides three main mechanisms: **workqueues** (work_struct scheduled to kernel worker threads, can sleep), **tasklets** (scheduled from softirq context, cannot sleep, serialized per-tasklet), and **softirqs** (kernel-defined bottom halves, highest priority, cannot sleep). The pattern enables interrupt handlers to quickly acknowledge hardware then defer heavy processing. Work is represented as a structure (work_struct, tasklet_struct) containing a callback function and embedded in the user's structure, with container_of recovering context in the callback.

**中文总结：**

命令/延迟执行模式将工作封装起来以便稍后在不同执行上下文中执行。内核提供三种主要机制：**workqueue**（work_struct调度到内核工作线程，可以睡眠）、**tasklet**（从softirq上下文调度，不能睡眠，每个tasklet串行化）、**softirq**（内核定义的底半部，最高优先级，不能睡眠）。该模式使中断处理程序能快速确认硬件然后延迟重处理。工作表示为结构体（work_struct、tasklet_struct），包含回调函数并嵌入用户结构体中，回调中用container_of恢复上下文。

---

## Quick Reference Card

```
+=============================================================================+
|              DEFERRED EXECUTION QUICK REFERENCE                              |
+=============================================================================+

    WORKQUEUE:
    ----------
    struct work_struct work;
    INIT_WORK(&work, callback);
    schedule_work(&work);
    
    void callback(struct work_struct *work) {
        struct my_dev *dev = container_of(work, struct my_dev, work);
        /* Can sleep here */
    }

    TASKLET:
    --------
    struct tasklet_struct tasklet;
    tasklet_init(&tasklet, callback, data);
    tasklet_schedule(&tasklet);
    
    void callback(unsigned long data) {
        struct my_dev *dev = (struct my_dev *)data;
        /* Cannot sleep */
    }

    CHOOSING MECHANISM:
    -------------------
    Need to sleep?         --> Workqueue
    Need lowest latency?   --> Softirq (if available) or Tasklet
    Dynamic allocation?    --> Tasklet or Workqueue
    Serialization needed?  --> Tasklet (auto-serialized)
```

---

## Pattern Comparison

```
    DIRECT CALL:                DEFERRED:
    
    irq_handler() {             irq_handler() {
        heavy_work();  [BAD]        schedule_work(&work);
    }                           }
                                
    IRQ blocked too long        worker_thread() {
                                    heavy_work();  [OK]
                                }
```
