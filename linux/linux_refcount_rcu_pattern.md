# Linux Kernel Refcount + RCU Pattern (v3.2)

## Overview

This document explains how **refcounting and RCU work together** in the Linux kernel to achieve safe, high-performance concurrent access to shared data structures.

---

## Why Refcount Alone is Insufficient

```
+------------------------------------------------------------------+
|  THE PROBLEM WITH REFCOUNT-ONLY                                  |
+------------------------------------------------------------------+

    Thread A (reader):           Thread B (remover):
    
    1. Find object in list       1. Remove from list
    2. Read refcount             2. Decrement refcount
    3. Increment refcount        3. refcount == 0?
       |                            |
       |   RACE CONDITION!          |
       v                            v
    4. Use object                4. Free object!
    
    Thread A increments refcount of FREED memory!
    
+------------------------------------------------------------------+

    TIME ─────────────────────────────────────────────────>
    
    Thread A:  [ find ] [ read ref ] [ inc ref ] [ use obj ]
                   |          |           |          |
    Thread B:  [ del ]  [ dec ref ]  [ free!!! ]     |
                                          |          |
                                          +----X-----+
                                          USE-AFTER-FREE!
```

**The core problem:**

```c
/* BROKEN: Race between lookup and refcount */
struct obj *find_object(int id)
{
    struct obj *obj;
    
    spin_lock(&list_lock);
    list_for_each_entry(obj, &object_list, list) {
        if (obj->id == id) {
            /* BUG: Object may be freed between unlock and get */
            spin_unlock(&list_lock);
            obj_get(obj);  /* Too late! */
            return obj;
        }
    }
    spin_unlock(&list_lock);
    return NULL;
}
```

**中文解释：**
- 仅使用引用计数的问题：查找和增加引用之间存在竞争
- 在释放锁后、增加引用前，对象可能已被释放
- 结果：访问已释放内存，系统崩溃

---

## RCU Mental Model

```
+------------------------------------------------------------------+
|  RCU = READ-COPY-UPDATE                                          |
+------------------------------------------------------------------+

    KEY INSIGHT:
    +----------------------------------------------------------+
    | Readers don't need to acquire any lock                    |
    | Writers wait for all readers to finish before freeing    |
    +----------------------------------------------------------+

+------------------------------------------------------------------+
|  RCU TIMELINE                                                    |
+------------------------------------------------------------------+

    TIME ─────────────────────────────────────────────────────>
    
    READERS:    [--- read ---]    [--- read ---]   [--- read ---]
                      |                 |                |
    GRACE PERIOD:     |=================|================|
                      ^                                   ^
                      |                                   |
    WRITER:     [ remove from list ]             [ safe to free ]
                      |                                   |
                      +---------- WAIT ------------------+

+------------------------------------------------------------------+
|  RCU GUARANTEES                                                  |
+------------------------------------------------------------------+

    1. Readers see a CONSISTENT view
       - Either old data (before update) 
       - Or new data (after update)
       - NEVER partial/torn data
    
    2. Writers wait for ALL pre-existing readers
       - After synchronize_rcu(), no old readers exist
       - Safe to free old data
    
    3. No reader blocking
       - rcu_read_lock() does NOT sleep
       - rcu_read_lock() does NOT spin
       - Near-zero overhead
```

**中文解释：**
- RCU（读-复制-更新）的核心思想：
  1. 读者不需要获取任何锁
  2. 写者等待所有读者完成后才释放
  3. 读者看到一致的视图（旧数据或新数据，不会是部分数据）
  4. 读者不阻塞，几乎零开销

---

## The Combined Pattern: Refcount + RCU

```
+------------------------------------------------------------------+
|  REFCOUNT + RCU = SAFE LOOKUP + LONG-TERM REFERENCE              |
+------------------------------------------------------------------+

    Thread A (reader):           Thread B (remover):
    
    1. rcu_read_lock()           1. spin_lock(&list_lock)
    2. Find object in list       2. Remove from list
    3. Try to get refcount       3. spin_unlock(&list_lock)
       (atomic, may fail)        4. synchronize_rcu()
    4. rcu_read_unlock()            (wait for readers)
    5. If got ref: use object    5. put_ref() → free
    
+------------------------------------------------------------------+
|  TIMELINE WITH REFCOUNT + RCU                                    |
+------------------------------------------------------------------+

    TIME ─────────────────────────────────────────────────────────>
    
    Reader A: [ rcu_lock ][ find ][ try_get_ref ][ rcu_unlock ][ use ]
                                       |
                                       +-- If refcount already 0, fail
                                           If refcount > 0, succeed
    
    Remover:  [ remove ][ sync_rcu ===============][ put_ref ][ free ]
                  |             |                        |
                  |             +--- Waits for Reader A  |
                  |                  to leave RCU        |
                  +--------------------------------------+
                  Object freed AFTER all readers done
```

### The Safe Pattern

```c
/* CORRECT: RCU + refcount */
struct obj *find_and_get_object(int id)
{
    struct obj *obj;
    
    rcu_read_lock();
    list_for_each_entry_rcu(obj, &object_list, list) {
        if (obj->id == id) {
            /* Try to get reference - may fail if being freed */
            if (kref_get_unless_zero(&obj->refcount)) {
                rcu_read_unlock();
                return obj;  /* Success - we own a reference */
            }
            /* Refcount was 0 - object being freed, keep looking */
        }
    }
    rcu_read_unlock();
    return NULL;
}

void remove_object(struct obj *obj)
{
    spin_lock(&list_lock);
    list_del_rcu(&obj->list);
    spin_unlock(&list_lock);
    
    synchronize_rcu();  /* Wait for RCU readers */
    
    obj_put(obj);       /* Release our reference */
    /* If refcount→0, object is freed */
}
```

**中文解释：**
- 组合模式：RCU + 引用计数
  1. RCU 保护查找过程（读者无锁）
  2. 引用计数保护长期使用
  3. `kref_get_unless_zero` 原子地检查并增加引用
  4. `synchronize_rcu` 确保所有读者完成后才释放

---

## Real Kernel Examples

### Example 1: struct file (VFS)

From `fs/file_table.c`:

```c
/* RCU-protected lookup + refcount */
struct file *fget(unsigned int fd)
{
    struct file *file;
    struct files_struct *files = current->files;
    
    rcu_read_lock();
    file = fcheck_files(files, fd);  /* RCU-protected lookup */
    if (file) {
        /* Try to get reference */
        if (!atomic_long_inc_not_zero(&file->f_count)) {
            /* Being freed - return NULL */
            file = NULL;
        }
    }
    rcu_read_unlock();
    return file;
}
```

```
+------------------------------------------------------------------+
|  fget() PATTERN                                                  |
+------------------------------------------------------------------+

    +---------------+         +---------------+
    | fd table      |  RCU    | struct file   |
    | (protected)   |-------->| f_count = N   |
    +---------------+         +---------------+
           |                         |
           | rcu_read_lock           | inc_not_zero
           | fcheck_files            |
           v                         v
    [ Find pointer ]        [ Atomic try-increment ]
           |                         |
           | rcu_read_unlock         | If success: use file
           +-------------------------+ If fail: return NULL
```

### Example 2: Network Devices

From `net/core/dev.c`:

```c
/* RCU-protected device lookup */
struct net_device *dev_get_by_index_rcu(struct net *net, int ifindex)
{
    struct net_device *dev;
    struct hlist_head *head = dev_index_hash(net, ifindex);
    
    hlist_for_each_entry_rcu(dev, head, index_hlist)
        if (dev->ifindex == ifindex)
            return dev;
    return NULL;
}

/* Get reference for longer use */
struct net_device *dev_get_by_index(struct net *net, int ifindex)
{
    struct net_device *dev;
    
    rcu_read_lock();
    dev = dev_get_by_index_rcu(net, ifindex);
    if (dev)
        dev_hold(dev);  /* Increment refcount */
    rcu_read_unlock();
    return dev;
}
```

### Example 3: Task Lookup

```c
/* From kernel/pid.c */
struct task_struct *find_task_by_vpid(pid_t vnr)
{
    /* Must be called under rcu_read_lock() */
    return pid_task(find_vpid(vnr), PIDTYPE_PID);
}

/* Safe pattern for longer use */
struct task_struct *get_task_by_pid(pid_t pid)
{
    struct task_struct *task;
    
    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task)
        get_task_struct(task);  /* Increment usage count */
    rcu_read_unlock();
    
    return task;  /* Caller must call put_task_struct() */
}
```

### Example 4: Module References

```c
/* Safely get module reference */
bool try_module_get(struct module *mod)
{
    bool ret = true;
    
    if (mod) {
        preempt_disable();
        /* Check if module is going away */
        if (likely(module_is_live(mod)))
            __module_get(mod);
        else
            ret = false;
        preempt_enable();
    }
    return ret;
}
```

### Example 5: Dentry Cache

```c
/* RCU-walk for pathname lookup */
static struct dentry *lookup_dcache(struct qstr *name,
                                    struct dentry *dir,
                                    unsigned int flags)
{
    struct dentry *dentry;
    
    /* RCU protects the lookup */
    dentry = d_lookup(dir, name);
    if (dentry) {
        /* d_lookup returns with elevated refcount */
        /* Safe to use dentry */
    }
    return dentry;
}
```

**中文解释：**
- 内核实例：
  1. **fget()**：RCU 保护 fd 表查找，`inc_not_zero` 原子增加引用
  2. **dev_get_by_index()**：RCU 查找 + `dev_hold()` 增加引用
  3. **任务查找**：RCU 读侧 + `get_task_struct()` 获取引用
  4. **模块引用**：`try_module_get()` 检查模块是否存活
  5. **dentry 缓存**：RCU 保护路径名查找

---

## What Goes Wrong If Contracts Are Violated

### Violation 1: Sleeping in RCU Read Section

```c
/* BUG: Sleeping while holding RCU read lock */
void broken_rcu_sleep(void)
{
    rcu_read_lock();
    
    /* ... lookup ... */
    
    msleep(100);  /* BUG: Sleeping in RCU read section! */
    
    /*
     * Problem: This blocks grace period
     * synchronize_rcu() will hang forever
     * System may deadlock
     */
    
    rcu_read_unlock();
}

/* CORRECT: Get reference, release RCU, then sleep */
void correct_rcu_sleep(void)
{
    struct obj *obj;
    
    rcu_read_lock();
    obj = lookup_object();
    if (obj)
        obj_get(obj);  /* Get refcount */
    rcu_read_unlock();
    
    if (obj) {
        msleep(100);   /* Safe - not in RCU section */
        use(obj);
        obj_put(obj);
    }
}
```

### Violation 2: Missing synchronize_rcu()

```c
/* BUG: Free without waiting for readers */
void broken_remove(struct obj *obj)
{
    spin_lock(&list_lock);
    list_del_rcu(&obj->list);
    spin_unlock(&list_lock);
    
    kfree(obj);  /* BUG: Readers may still be accessing! */
}

/* CORRECT: Wait for grace period */
void correct_remove(struct obj *obj)
{
    spin_lock(&list_lock);
    list_del_rcu(&obj->list);
    spin_unlock(&list_lock);
    
    synchronize_rcu();  /* Wait for all readers */
    
    kfree(obj);         /* Now safe */
}
```

### Violation 3: Dereferencing Without RCU Protection

```c
/* BUG: Reading RCU pointer without protection */
void broken_read(void)
{
    struct obj *obj;
    
    /* BUG: No rcu_read_lock! */
    obj = rcu_dereference(global_ptr);  /* Warning from sparse */
    use(obj);  /* May see torn pointer or freed object */
}

/* CORRECT: Proper RCU read section */
void correct_read(void)
{
    struct obj *obj;
    
    rcu_read_lock();
    obj = rcu_dereference(global_ptr);  /* Safe */
    use(obj);  /* Object guaranteed to exist */
    rcu_read_unlock();
}
```

### Violation 4: Forgetting kref_get_unless_zero

```c
/* BUG: Getting ref without checking */
void broken_get_ref(void)
{
    struct obj *obj;
    
    rcu_read_lock();
    obj = lookup();
    kref_get(&obj->refcount);  /* BUG: May be 0! */
    rcu_read_unlock();
}

/* CORRECT: Use atomic try-get */
void correct_get_ref(void)
{
    struct obj *obj;
    
    rcu_read_lock();
    obj = lookup();
    if (!kref_get_unless_zero(&obj->refcount))
        obj = NULL;  /* Being freed, return NULL */
    rcu_read_unlock();
}
```

**中文解释：**
- 违规后果：
  1. **RCU 读侧睡眠**：阻塞 grace period，导致死锁
  2. **缺少 synchronize_rcu**：读者访问已释放内存
  3. **无 RCU 保护解引用**：可能看到部分指针或已释放对象
  4. **忘记 kref_get_unless_zero**：增加已为 0 的引用计数

---

## Timeline Diagrams

### Successful Lookup Pattern

```
+------------------------------------------------------------------+
|  SUCCESSFUL LOOKUP + REFERENCE ACQUISITION                       |
+------------------------------------------------------------------+

    TIME ─────────────────────────────────────────────────────────>
    
    Reader:
    
    [ rcu_read_lock ]
         |
         v
    [ find in list ]
         |
         v
    [ kref_get_unless_zero ] ─── Returns TRUE (refcount was > 0)
         |
         v
    [ rcu_read_unlock ]
         |
         v
    [ use object safely ] ─── We own a reference!
         |
         v
    [ kref_put ] ─── Release our reference
    
    
    Object refcount timeline:
    
         2      3         3         3         2
    ─────●──────●─────────●─────────●─────────●─────────>
         ^      ^                             ^
         |      |                             |
       initial  reader got ref           reader put ref
```

### Failed Lookup (Object Being Freed)

```
+------------------------------------------------------------------+
|  FAILED LOOKUP - OBJECT BEING FREED                              |
+------------------------------------------------------------------+

    TIME ─────────────────────────────────────────────────────────>
    
    Reader:                          Remover:
    
    [ rcu_read_lock ]
         |                           [ list_del_rcu ]
         |                                |
         v                                |
    [ find in list ]                      |
         |                                |
         v                           [ synchronize_rcu ] (waiting)
    [ kref_get_unless_zero ]              |
         |                                |
         +── Returns FALSE                |
         |   (refcount already 0)         |
         v                                |
    [ return NULL ]                       |
         |                                |
    [ rcu_read_unlock ]                   |
                                          |
                                     [ put → free ]
    
    
    Object refcount timeline:
    
         1      0 (freed!)
    ─────●──────●────────────────────────────>
         ^      ^
         |      |
       last   try_get fails
       put    (refcount is 0)
```

**中文解释：**
- **成功查找**：引用计数 > 0，`kref_get_unless_zero` 成功
- **失败查找**：引用计数已为 0，`kref_get_unless_zero` 失败，返回 NULL

---

## User-Space Design Transfer

```c
/* user_space_rcu_refcount.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

/*---------------------------------------------------------
 * Simplified RCU-like mechanism for user-space
 * (Real user-space RCU: liburcu)
 *---------------------------------------------------------*/
static atomic_int rcu_readers = 0;

void rcu_read_lock(void)
{
    atomic_fetch_add(&rcu_readers, 1);
    atomic_thread_fence(memory_order_acquire);
}

void rcu_read_unlock(void)
{
    atomic_thread_fence(memory_order_release);
    atomic_fetch_sub(&rcu_readers, 1);
}

void synchronize_rcu(void)
{
    /* Wait for all readers to finish */
    while (atomic_load(&rcu_readers) > 0) {
        usleep(1000);
    }
    atomic_thread_fence(memory_order_seq_cst);
}

/*---------------------------------------------------------
 * Refcounted object
 *---------------------------------------------------------*/
struct object {
    atomic_int refcount;
    int id;
    char data[32];
    struct object *next;  /* RCU-protected list link */
};

static struct object *object_list = NULL;
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;

struct object *object_create(int id, const char *data)
{
    struct object *obj = malloc(sizeof(*obj));
    if (!obj) return NULL;
    
    atomic_init(&obj->refcount, 1);
    obj->id = id;
    snprintf(obj->data, sizeof(obj->data), "%s", data);
    obj->next = NULL;
    return obj;
}

bool object_get(struct object *obj)
{
    int old = atomic_load(&obj->refcount);
    while (old > 0) {
        if (atomic_compare_exchange_weak(&obj->refcount, &old, old + 1))
            return true;  /* Success */
    }
    return false;  /* Refcount was 0 */
}

void object_put(struct object *obj)
{
    if (atomic_fetch_sub(&obj->refcount, 1) == 1) {
        printf("[FREE] Object %d freed\n", obj->id);
        free(obj);
    }
}

/*---------------------------------------------------------
 * List operations (RCU + refcount pattern)
 *---------------------------------------------------------*/
void list_add(struct object *obj)
{
    pthread_mutex_lock(&list_lock);
    obj->next = object_list;
    atomic_thread_fence(memory_order_release);
    object_list = obj;
    pthread_mutex_unlock(&list_lock);
    printf("[ADD] Object %d added\n", obj->id);
}

struct object *list_find(int id)
{
    struct object *obj;
    
    rcu_read_lock();
    for (obj = object_list; obj != NULL; obj = obj->next) {
        if (obj->id == id) {
            if (object_get(obj)) {
                rcu_read_unlock();
                return obj;  /* Got reference */
            }
            /* Being freed, continue search */
        }
    }
    rcu_read_unlock();
    return NULL;
}

void list_remove(struct object *obj)
{
    struct object **pp;
    
    pthread_mutex_lock(&list_lock);
    for (pp = &object_list; *pp != NULL; pp = &(*pp)->next) {
        if (*pp == obj) {
            *pp = obj->next;
            break;
        }
    }
    pthread_mutex_unlock(&list_lock);
    
    printf("[REMOVE] Object %d removed, waiting for readers\n", obj->id);
    synchronize_rcu();
    printf("[SYNC] Grace period complete\n");
    
    object_put(obj);  /* Release list's reference */
}

/*---------------------------------------------------------
 * Demo
 *---------------------------------------------------------*/
int main(void)
{
    /* Create and add objects */
    struct object *obj1 = object_create(1, "First");
    struct object *obj2 = object_create(2, "Second");
    list_add(obj1);
    list_add(obj2);
    
    /* Find with refcount */
    struct object *found = list_find(1);
    if (found) {
        printf("Found: id=%d, data=%s\n", found->id, found->data);
        
        /* Remove while we hold reference */
        list_remove(obj1);
        
        /* Still safe - we have reference */
        printf("Still valid: id=%d, data=%s\n", found->id, found->data);
        
        object_put(found);  /* Release our reference → frees */
    }
    
    /* Cleanup */
    list_remove(obj2);
    
    return 0;
}
```

**中文解释：**
- 用户态实现：
  1. 简化的 RCU 机制（真实实现使用 liburcu）
  2. 原子引用计数 + compare-exchange
  3. `object_get` 实现 `kref_get_unless_zero` 语义
  4. 移除时等待 grace period 后释放

---

## Summary

```
+------------------------------------------------------------------+
|  REFCOUNT + RCU SUMMARY                                          |
+------------------------------------------------------------------+

    WHY BOTH:
    +----------------------------------------------------------+
    | RCU alone: Can't hold reference after rcu_read_unlock    |
    | Refcount alone: Race between lookup and increment        |
    | COMBINED: Safe lookup + long-term reference              |
    +----------------------------------------------------------+
    
    PATTERN:
    +----------------------------------------------------------+
    | 1. rcu_read_lock()                                       |
    | 2. Find object (RCU protected)                           |
    | 3. kref_get_unless_zero() - may fail                     |
    | 4. rcu_read_unlock()                                     |
    | 5. Use object (refcount protected)                       |
    | 6. kref_put() when done                                  |
    +----------------------------------------------------------+
    
    REMOVAL:
    +----------------------------------------------------------+
    | 1. Remove from list (under spinlock)                     |
    | 2. synchronize_rcu() - wait for readers                  |
    | 3. kref_put() - may trigger free                         |
    +----------------------------------------------------------+
    
    INVARIANTS:
    +----------------------------------------------------------+
    | - Never sleep in rcu_read_lock section                   |
    | - Always use kref_get_unless_zero, not kref_get          |
    | - Always synchronize_rcu before freeing RCU-protected    |
    | - Use rcu_dereference to read RCU pointers               |
    | - Use rcu_assign_pointer to publish new pointers         |
    +----------------------------------------------------------+
```

**中文总结：**
Refcount + RCU 组合模式：
- **为什么需要两者**：RCU 单独不能持有长期引用，引用计数单独有查找竞争
- **模式**：RCU 读侧保护查找 → `get_unless_zero` 原子获取引用 → 使用 → 释放
- **移除**：从列表移除 → `synchronize_rcu` 等待读者 → 释放引用
- **不变量**：不能在 RCU 读侧睡眠，必须使用 `get_unless_zero`，释放前必须等待 grace period

