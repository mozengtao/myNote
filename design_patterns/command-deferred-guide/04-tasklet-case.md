# Case 2: Tasklets

Tasklets run in softirq context with per-tasklet serialization.

---

## Overview

```
    TASKLET PROPERTIES:
    - Softirq context (cannot sleep)
    - Same tasklet serialized
    - Different tasklets can parallel
```

**中文说明：**

Tasklet在softirq上下文运行，不能睡眠，同一tasklet被序列化执行。

---

## Key Functions

```c
/* Initialize */
tasklet_init(&tasklet, func, data);
DECLARE_TASKLET(name, func, data);

/* Schedule */
tasklet_schedule(&tasklet);

/* Control */
tasklet_disable(&tasklet);
tasklet_enable(&tasklet);
tasklet_kill(&tasklet);
```

---

## Example

```c
void my_tasklet_fn(unsigned long data)
{
    struct my_device *dev = (void *)data;
    /* Cannot sleep! */
    process_rx(dev);
}

void irq_handler(struct my_device *dev)
{
    tasklet_schedule(&dev->tasklet);
}
```

---

## Tasklet vs Workqueue

| Aspect | Tasklet | Workqueue |
|--------|---------|-----------|
| Sleep | No | Yes |
| Context | Softirq | Process |
| Speed | Fast | Slower |

---

## Version

Based on **Linux kernel v3.2**.
