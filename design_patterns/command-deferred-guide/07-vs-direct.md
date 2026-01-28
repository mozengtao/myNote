# Deferred vs Direct Execution

When to defer work versus executing directly.

---

## Decision

```
    MUST DEFER IF:
    - Need to sleep
    - In interrupt context
    - Work takes long time

    CAN EXECUTE DIRECTLY IF:
    - In process context
    - Work is fast
    - No sleeping needed
```

**中文说明：**

必须延迟：需要睡眠、在中断上下文、工作耗时长。可以直接执行：在进程上下文、工作快、不需要睡眠。

---

## Pattern

```c
void handle_event(struct device *dev)
{
    if (in_interrupt())
        schedule_work(&dev->work);
    else
        do_work(dev);
}
```

---

## Version

Based on **Linux kernel v3.2**.
