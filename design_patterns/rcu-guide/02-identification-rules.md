# Identification Rules: RCU Pattern

Five concrete rules to identify RCU usage in Linux kernel source code.

---

## Rule 1: Look for rcu_read_lock/unlock

```c
/* RCU read-side critical section */
rcu_read_lock();
/* ... access RCU-protected data ... */
rcu_read_unlock();

/* Also variants: */
rcu_read_lock_bh();      /* + disable softirq */
rcu_read_lock_sched();   /* preempt disable */

/* SIGNAL: rcu_read_* calls bracket data access */
```

**中文说明：**

规则1：查找rcu_read_lock/unlock——RCU读侧临界区的标志。

---

## Rule 2: Look for rcu_dereference

```c
/* Safe pointer access */
struct my_data *p;

rcu_read_lock();
p = rcu_dereference(global_ptr);  /* <-- RCU dereference */
if (p)
    use(p->field);
rcu_read_unlock();

/* Variants: */
rcu_dereference_check()
rcu_dereference_protected()
rcu_dereference_raw()

/* SIGNAL: rcu_dereference* used to access pointers */
```

---

## Rule 3: Look for rcu_assign_pointer

```c
/* Publishing new data */
struct my_data *new = kmalloc(...);
/* ... initialize new ... */

rcu_assign_pointer(global_ptr, new);  /* <-- Publish */

/* SIGNAL: rcu_assign_pointer used to update pointers */
```

**中文说明：**

规则3：查找rcu_assign_pointer——发布新数据的标志。

---

## Rule 4: Look for synchronize_rcu or call_rcu

```c
/* Synchronous wait */
old = global_ptr;
rcu_assign_pointer(global_ptr, new);
synchronize_rcu();  /* <-- Wait for readers */
kfree(old);

/* Asynchronous callback */
call_rcu(&old->rcu_head, my_free_callback);  /* <-- Async */

/* SIGNAL: Grace period waiting indicates RCU */
```

---

## Rule 5: Look for RCU-Specific Data Structures

```c
/* Structures with rcu_head for deferred freeing */
struct my_data {
    int field;
    struct rcu_head rcu;  /* <-- RCU callback head */
};

/* RCU-protected list operations */
list_add_rcu()
list_del_rcu()
list_for_each_entry_rcu()
hlist_add_head_rcu()
hlist_del_rcu()

/* SIGNAL: *_rcu suffix on list operations */
```

**中文说明：**

规则5：查找RCU特定数据结构——rcu_head成员、*_rcu后缀的列表操作。

---

## Summary Checklist

```
+=============================================================================+
|                    RCU IDENTIFICATION CHECKLIST                              |
+=============================================================================+

    [ ] 1. rcu_read_lock() / rcu_read_unlock()
        Read-side critical section markers
    
    [ ] 2. rcu_dereference()
        Safe pointer access within read-side
    
    [ ] 3. rcu_assign_pointer()
        Publishing new pointer values
    
    [ ] 4. synchronize_rcu() / call_rcu()
        Grace period waiting
    
    [ ] 5. struct rcu_head / *_rcu list operations
        RCU-aware data structures

    SCORING:
    2+ indicators = RCU pattern in use
```

---

## Red Flags: NOT RCU

```
    THESE ARE NOT RCU:
    ==================

    1. Regular spinlocks/rwlocks
       spin_lock(), read_lock(), write_lock()
    
    2. Regular pointer access
       p = global_ptr;  /* No rcu_dereference */
    
    3. Immediate free after update
       global_ptr = new;
       kfree(old);  /* No synchronize_rcu! */
```

---

## Version

Based on **Linux kernel v3.2**.
