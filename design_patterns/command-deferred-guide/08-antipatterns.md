# Deferred Execution Anti-Patterns

Common mistakes to avoid with deferred execution.

---

## Anti-Pattern 1: Sleeping in Wrong Context

```c
/* BAD: Sleeping in softirq/tasklet context */
void my_tasklet_fn(unsigned long data)
{
    kmalloc(size, GFP_KERNEL);  /* BUG! */
    mutex_lock(&mutex);          /* BUG! */
}

/* CORRECT: Use workqueue for sleeping */
void my_work_fn(struct work_struct *work)
{
    kmalloc(size, GFP_KERNEL);  /* OK */
    mutex_lock(&mutex);          /* OK */
}
```

**中文说明：**

反模式1：在softirq/tasklet上下文中睡眠是错误的。正确做法是使用workqueue。

---

## Anti-Pattern 2: Double Scheduling

```c
/* BAD: No check for already scheduled */
void irq_handler(void)
{
    schedule_work(&work);  /* First call OK */
    schedule_work(&work);  /* What if first not done? */
}

/* CORRECT: Check pending status */
void irq_handler(void)
{
    if (!work_pending(&work))
        schedule_work(&work);
}
```

---

## Anti-Pattern 3: Use-After-Free

```c
/* BAD: Freeing while work pending */
void cleanup(struct my_device *dev)
{
    kfree(dev);  /* BAD: work might still run! */
}

/* CORRECT: Cancel work first */
void cleanup(struct my_device *dev)
{
    cancel_work_sync(&dev->work);
    kfree(dev);  /* Safe now */
}
```

---

## Anti-Pattern 4: Lost Data

```c
/* BAD: Overwriting data before work runs */
void irq_handler(struct device *dev)
{
    dev->pending = new_data;  /* Overwrites old! */
    schedule_work(&dev->work);
}

/* CORRECT: Queue or accumulate data */
void irq_handler(struct device *dev)
{
    enqueue(&dev->data_queue, new_data);
    schedule_work(&dev->work);
}
```

---

## Summary

```
    [X] Don't sleep in softirq/tasklet
    [X] Check if already scheduled
    [X] Cancel work before freeing
    [X] Don't lose data on re-schedule
```

---

## Version

Based on **Linux kernel v3.2** deferred execution patterns.
