# RCU Anti-Patterns

Common mistakes to avoid when using RCU.

---

## Anti-Pattern 1: Sleeping in RCU Read Section

```c
/* BAD: Sleeping while holding rcu_read_lock */
void bad_function(void)
{
    rcu_read_lock();
    
    p = rcu_dereference(global_ptr);
    kmalloc(..., GFP_KERNEL);  /* MAY SLEEP! */
    mutex_lock(&my_mutex);      /* MAY SLEEP! */
    msleep(100);                /* DEFINITELY SLEEPS! */
    
    rcu_read_unlock();
}

/* CORRECT: No sleeping in read-side */
void good_function(void)
{
    struct my_data local_copy;
    
    rcu_read_lock();
    p = rcu_dereference(global_ptr);
    local_copy = *p;  /* Copy what we need */
    rcu_read_unlock();
    
    /* Now safe to sleep */
    process(&local_copy);
}
```

**中文说明：**

反模式1：RCU读段中睡眠——rcu_read_lock期间不能调用可能睡眠的函数。正确做法是复制需要的数据，解锁后再处理。

---

## Anti-Pattern 2: Missing synchronize_rcu Before Free

```c
/* BAD: Free without waiting */
void bad_delete(void)
{
    old = global_ptr;
    rcu_assign_pointer(global_ptr, new);
    kfree(old);  /* CRASH! Readers may still use old */
}

/* CORRECT: Wait for readers */
void good_delete(void)
{
    old = global_ptr;
    rcu_assign_pointer(global_ptr, new);
    synchronize_rcu();  /* Wait for all readers */
    kfree(old);  /* Now safe */
}

/* ALSO CORRECT: Async free */
void good_delete_async(void)
{
    old = global_ptr;
    rcu_assign_pointer(global_ptr, new);
    call_rcu(&old->rcu_head, my_free_callback);
}
```

---

## Anti-Pattern 3: Using Regular Pointer Access

```c
/* BAD: No rcu_dereference */
void bad_reader(void)
{
    rcu_read_lock();
    p = global_ptr;  /* No memory barrier! */
    use(p->field);   /* May see stale/torn data */
    rcu_read_unlock();
}

/* CORRECT: Use rcu_dereference */
void good_reader(void)
{
    rcu_read_lock();
    p = rcu_dereference(global_ptr);  /* Proper ordering */
    use(p->field);
    rcu_read_unlock();
}
```

**中文说明：**

反模式3：使用普通指针访问——必须用rcu_dereference确保内存顺序正确。

---

## Anti-Pattern 4: Modifying RCU-Protected Data In Place

```c
/* BAD: Modifying in place */
void bad_update(int new_value)
{
    rcu_read_lock();
    p = rcu_dereference(global_ptr);
    p->value = new_value;  /* WRONG! Readers see partial update */
    rcu_read_unlock();
}

/* CORRECT: Copy-modify-publish */
void good_update(int new_value)
{
    struct my_data *old, *new;
    
    old = global_ptr;
    new = kmalloc(sizeof(*new), GFP_KERNEL);
    *new = *old;
    new->value = new_value;
    
    rcu_assign_pointer(global_ptr, new);
    synchronize_rcu();
    kfree(old);
}
```

---

## Anti-Pattern 5: Long RCU Read Sections

```c
/* BAD: Holding rcu_read_lock too long */
void bad_long_reader(void)
{
    rcu_read_lock();
    
    /* Long-running operation */
    for (i = 0; i < 1000000; i++) {
        do_work();  /* Blocks grace period! */
    }
    
    rcu_read_unlock();
}

/* CORRECT: Keep read sections short */
void good_short_reader(void)
{
    struct my_data copy;
    
    rcu_read_lock();
    copy = *rcu_dereference(global_ptr);
    rcu_read_unlock();
    
    /* Long work outside RCU */
    for (i = 0; i < 1000000; i++) {
        do_work_with(&copy);
    }
}
```

---

## Summary Checklist

```
+=============================================================================+
|                    RCU SAFE USAGE CHECKLIST                                  |
+=============================================================================+

    [X] Never sleep in rcu_read_lock section
    [X] Always synchronize_rcu before freeing
    [X] Use rcu_dereference for pointer access
    [X] Copy-on-write for updates (don't modify in place)
    [X] Keep read sections short
    [X] Use rcu_assign_pointer for publishing
```

---

## Version

Based on **Linux kernel v3.2** RCU usage.
