# Case 1: Workqueues

Workqueues run in process context where sleeping is allowed.

---

## Overview

```
+=============================================================================+
|                    WORKQUEUE MECHANISM                                       |
+=============================================================================+

    +-------------+     +-------------+     +-------------+
    | IRQ Handler | --> | Work Item   | --> | Worker      |
    +-------------+     | (queued)    |     | Thread      |
                        +-------------+     +-------------+
                                                   |
                                            (can sleep!)
```

**中文说明：**

工作队列在进程上下文运行，可以睡眠。

---

## Key Functions

```c
/* Initialize */
INIT_WORK(&work, work_fn);

/* Schedule */
schedule_work(&work);
queue_work(wq, &work);

/* Synchronize */
flush_work(&work);
cancel_work_sync(&work);
```

---

## Example

```c
struct my_device {
    struct work_struct work;
    int pending_data;
};

void my_work_fn(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, work);
    /* Can sleep here */
    kmalloc(..., GFP_KERNEL);
}

void irq_handler(struct my_device *dev, int data)
{
    dev->pending_data = data;
    schedule_work(&dev->work);
}
```

---

## Version

Based on **Linux kernel v3.2**.
