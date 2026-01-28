# Observer/Notifier Anti-Patterns

Common mistakes to avoid when implementing Observer/Notifier patterns in kernel-style C code.

---

## Anti-Pattern 1: Recursive Notification

```
+=============================================================================+
|                    ANTI-PATTERN: RECURSIVE NOTIFICATION                      |
+=============================================================================+

    THE PROBLEM:
    ============

    Notifier callback triggers another notification on the same chain.

    call_chain(EVENT_A)
         |
         +---> handler1() --> call_chain(EVENT_B)  <-- RECURSIVE!
                                   |
                                   +---> handler1() --> ???
                                                      (infinite loop)
```

**中文说明：**

反模式1：递归通知——通知器回调触发同一链上的另一个通知，可能导致无限循环。

```c
/* BAD: Handler triggers same chain */
int my_handler(struct notifier_block *nb,
               unsigned long event, void *data)
{
    if (event == NETDEV_UP) {
        dev_close(dev);  /* Triggers NETDEV_DOWN, calls my_handler */
    }
    return NOTIFY_OK;
}

/* CORRECT: Defer the action */
int my_handler(struct notifier_block *nb,
               unsigned long event, void *data)
{
    if (event == NETDEV_UP) {
        schedule_work(&close_work);  /* Handle later */
    }
    return NOTIFY_OK;
}
```

---

## Anti-Pattern 2: Blocking in Atomic Notifier

```c
/* BAD: Sleeping in atomic context */
int bad_atomic_handler(struct notifier_block *nb,
                       unsigned long event, void *data)
{
    kmalloc(size, GFP_KERNEL);  /* May sleep - BAD */
    mutex_lock(&my_mutex);       /* May sleep - BAD */
    return NOTIFY_OK;
}

/* CORRECT: Use non-sleeping operations */
int good_atomic_handler(struct notifier_block *nb,
                        unsigned long event, void *data)
{
    kmalloc(size, GFP_ATOMIC);  /* Won't sleep */
    spin_lock(&my_spinlock);     /* Won't sleep */
    schedule_work(&deferred);    /* Defer sleeping work */
    return NOTIFY_OK;
}
```

---

## Anti-Pattern 3: Holding Locks While Calling Chain

```c
/* BAD: Deadlock risk */
void bad_function(void)
{
    spin_lock(&my_lock);
    call_notifier_chain(&chain, event, data);  /* Handler may need my_lock */
    spin_unlock(&my_lock);
}

/* CORRECT: Release lock first */
void good_function(void)
{
    spin_lock(&my_lock);
    local_copy = *shared_data;
    spin_unlock(&my_lock);
    
    call_notifier_chain(&chain, event, &local_copy);  /* Safe */
}
```

---

## Anti-Pattern 4: Unbalanced Register/Unregister

```c
/* BAD: Forgetting to unregister */
int my_init(void)
{
    register_my_notifier(&my_nb);
    if (error)
        return -EINVAL;  /* Leaked registration! */
    return 0;
}

/* CORRECT: Always clean up */
int my_init(void)
{
    int ret;
    ret = register_my_notifier(&my_nb);
    if (ret)
        return ret;
    
    ret = do_other_init();
    if (ret) {
        unregister_my_notifier(&my_nb);  /* Clean up */
        return ret;
    }
    return 0;
}
```

---

## Anti-Pattern 5: Long-Running Handlers

```c
/* BAD: Blocking the chain */
int slow_handler(struct notifier_block *nb,
                 unsigned long event, void *data)
{
    heavy_computation();  /* 100ms */
    network_io();         /* 500ms */
    return NOTIFY_OK;
}

/* CORRECT: Defer heavy work */
int fast_handler(struct notifier_block *nb,
                 unsigned long event, void *data)
{
    saved_data = *(struct event_data *)data;
    schedule_work(&deferred_work);  /* Handle later */
    return NOTIFY_OK;
}
```

---

## Summary Checklist

```
    [X] Don't recursively trigger same chain
    [X] Don't sleep in atomic notifier handlers
    [X] Release locks before calling chain
    [X] Always unregister on cleanup/error
    [X] Keep handlers short
    [X] Check return values when needed
    [X] Don't modify chain during iteration
```

---

## Version

Based on **Linux kernel v3.2** notifier implementation.
