# Linux Kernel Ownership & Lifetime Discipline (v3.2)

## Overview

This document explains how **strict ownership and lifetime rules** are enforced in the Linux kernel, preventing memory leaks, use-after-free bugs, and resource corruption.

---

## Ownership vs Reference

```
+------------------------------------------------------------------+
|  OWNERSHIP vs REFERENCE                                          |
+------------------------------------------------------------------+

    OWNERSHIP:
    +----------------------------------------------------------+
    |  "I am responsible for this object's lifetime"           |
    |                                                          |
    |  Owner MUST:                                             |
    |  - Allocate the object                                   |
    |  - Free the object when done                             |
    |  - Ensure no dangling references before free             |
    +----------------------------------------------------------+
    
    REFERENCE:
    +----------------------------------------------------------+
    |  "I am using this object temporarily"                    |
    |                                                          |
    |  Reference holder MUST:                                  |
    |  - Acquire reference (refcount++)                        |
    |  - Release reference when done (refcount--)              |
    |  - Never free the object (not my responsibility)         |
    +----------------------------------------------------------+

+------------------------------------------------------------------+
|  KERNEL RULE: SINGLE OWNER, MULTIPLE REFERENCES                  |
+------------------------------------------------------------------+

    +-------------+
    |   OWNER     |  (created the object, will destroy it)
    +------+------+
           |
           | owns
           v
    +-------------+
    |   OBJECT    |  refcount = 3
    +-------------+
      ^    ^    ^
      |    |    |
    ref  ref  ref
      |    |    |
    +-+-++-+-++-+-+
    |   ||   ||   |
    | A || B || C |  (temporary users)
    +---++---++---+
    
    When A, B, C release refs → refcount = 0 → owner frees
```

**中文解释：**
- **所有权**：负责对象的生命周期（分配和释放）
- **引用**：临时使用对象，通过引用计数管理
- **内核规则**：单一所有者，多个引用持有者
- 当所有引用释放后，所有者负责释放对象

---

## Analysis: sk_buff Ownership

### sk_buff: The Packet Buffer

```
+------------------------------------------------------------------+
|  sk_buff OWNERSHIP MODEL                                         |
+------------------------------------------------------------------+

    ALLOCATION:
    +----------------------------------------------------------+
    | skb = alloc_skb(size, GFP_KERNEL);                       |
    | Allocator is the INITIAL OWNER                           |
    +----------------------------------------------------------+
    
    OWNERSHIP TRANSFER:
    +----------------------------------------------------------+
    | netif_receive_skb(skb);   /* Transfer to network stack */|
    | dev_queue_xmit(skb);      /* Transfer to TX path */      |
    | consume_skb(skb);         /* Transfer to "freed" state */|
    +----------------------------------------------------------+
    
    CRITICAL RULE:
    +----------------------------------------------------------+
    | After transferring ownership, you MUST NOT touch skb!    |
    | The new owner may have already freed it!                 |
    +----------------------------------------------------------+
```

```
+------------------------------------------------------------------+
|  sk_buff LIFECYCLE                                               |
+------------------------------------------------------------------+

    RX PATH:
    
    Driver:         skb = netdev_alloc_skb()
                         |
                         | OWNS
                         v
                    Fill skb with packet data
                         |
                         | TRANSFER OWNERSHIP
                         v
    Net Core:       netif_receive_skb(skb)
                         |
                         | OWNS (now net core)
                         v
    Protocol:       ip_rcv(skb) → tcp_rcv(skb)
                         |
                         | OWNS (passed up)
                         v
    Socket:         skb_queue_tail(&sk->receive_queue, skb)
                         |
                         | OWNS (queued for app)
                         v
    User read:      skb_dequeue() → copy to user → kfree_skb()
    
    
    TX PATH:
    
    Socket:         skb = sock_alloc_send_skb()
                         |
                         | OWNS
                         v
    Protocol:       tcp_transmit_skb(skb)
                         |
                         | TRANSFER OWNERSHIP
                         v
    Net Core:       dev_queue_xmit(skb)
                         |
                         | OWNS
                         v
    Driver:         ndo_start_xmit(skb)
                         |
                         | MUST consume or return BUSY
                         v
    Hardware:       DMA → consume_skb() or dev_kfree_skb()
```

### Ownership Rules for sk_buff

```c
/* RULE 1: Owner must consume or return */
netdev_tx_t my_xmit(struct sk_buff *skb, struct net_device *dev)
{
    /* Option A: Consume (transfer to hardware) */
    dma_map_and_send(skb);
    return NETDEV_TX_OK;  /* We took ownership, will free later */
    
    /* Option B: Return BUSY (don't take ownership) */
    if (queue_full)
        return NETDEV_TX_BUSY;  /* Caller still owns skb */
    
    /* WRONG: Ignore skb */
    return NETDEV_TX_OK;  /* BUG: Memory leak! */
}

/* RULE 2: Clone if you need to keep a copy */
void process_skb(struct sk_buff *skb)
{
    struct sk_buff *clone = skb_clone(skb, GFP_ATOMIC);
    if (clone) {
        /* clone is a new skb, we own it */
        queue_for_later(clone);
    }
    /* Original skb continues its journey */
}

/* RULE 3: Don't touch after transfer */
void broken_rx(struct sk_buff *skb)
{
    netif_receive_skb(skb);  /* Ownership transferred */
    
    printk("len = %d\n", skb->len);  /* BUG: skb may be freed! */
}
```

**中文解释：**
- sk_buff 所有权规则：
  1. 所有者必须消费或返回（不能忽略）
  2. 需要保留副本时使用 `skb_clone()`
  3. 转移所有权后不能再访问 skb

---

## Analysis: struct file Ownership

```
+------------------------------------------------------------------+
|  struct file OWNERSHIP MODEL                                     |
+------------------------------------------------------------------+

    CREATION:
    +----------------------------------------------------------+
    | file = get_empty_filp();                                 |
    | VFS creates file during open()                           |
    | Initial refcount = 1                                     |
    +----------------------------------------------------------+
    
    REFERENCE COUNTING:
    +----------------------------------------------------------+
    | fget(fd)        → Acquire reference, refcount++          |
    | fput(file)      → Release reference, refcount--          |
    | When refcount→0 → __fput() cleans up and frees          |
    +----------------------------------------------------------+
    
    OWNERSHIP:
    +----------------------------------------------------------+
    | The file descriptor table "owns" the file                |
    | Callers acquire references for temporary use             |
    | close(fd) removes from table, releases owner's ref       |
    +----------------------------------------------------------+
```

```
+------------------------------------------------------------------+
|  struct file LIFECYCLE                                           |
+------------------------------------------------------------------+

    sys_open():
                    +-------------------+
                    |   fdtable slot    |  refcount = 1
                    |   (OWNER)         |
                    +--------+----------+
                             |
                             v
                    +-------------------+
                    |   struct file     |
                    +-------------------+
    
    fget(fd):       fdtable still owner
                    refcount = 2
                             ^
                             |
                    +--------+----------+
                    |   kernel code     |  (reference holder)
                    +-------------------+
    
    fput(file):     refcount = 1
                    kernel code done
    
    sys_close(fd):  
                    fdtable releases → refcount = 0
                    → __fput() → file->f_op->release()
                    → free struct file
```

### Reference Pattern for struct file

```c
/* CORRECT: Acquire reference, use, release */
ssize_t my_syscall_that_uses_fd(int fd)
{
    struct file *file;
    ssize_t ret;
    
    file = fget(fd);          /* Acquire reference */
    if (!file)
        return -EBADF;
    
    ret = do_something(file); /* Use file */
    
    fput(file);               /* Release reference */
    return ret;
}

/* WRONG: Holding reference too long */
struct file *stored_file;

void broken_store(int fd)
{
    stored_file = fget(fd);   /* Acquire reference */
    /* Never release! → Memory leak */
    /* Also: stored_file may outlive the fd table entry */
}
```

**中文解释：**
- struct file 所有权模型：
  1. 文件描述符表"拥有"文件
  2. 调用者通过 `fget()` 获取临时引用
  3. 使用完毕后 `fput()` 释放引用
  4. `close(fd)` 释放所有者的引用，当 refcount=0 时释放

---

## Analysis: task_struct Ownership

```
+------------------------------------------------------------------+
|  task_struct OWNERSHIP MODEL                                     |
+------------------------------------------------------------------+

    CREATION:
    +----------------------------------------------------------+
    | fork() → copy_process() → alloc task_struct              |
    | Initial reference: task exists                            |
    +----------------------------------------------------------+
    
    REFERENCE OPERATIONS:
    +----------------------------------------------------------+
    | get_task_struct(task)  → Increment usage count           |
    | put_task_struct(task)  → Decrement, may free             |
    | task_rcu_dereference() → Safe read under RCU             |
    +----------------------------------------------------------+
    
    LIFECYCLE:
    +----------------------------------------------------------+
    | Running → Zombie → Freed                                  |
    | Parent reaps zombie → releases final reference           |
    +----------------------------------------------------------+
```

```
+------------------------------------------------------------------+
|  task_struct STATES AND OWNERSHIP                                |
+------------------------------------------------------------------+

    RUNNING/SLEEPING:
    +-------------------+
    |   task_struct     |  Referenced by:
    |                   |  - Scheduler (running)
    |   usage = N       |  - Wait queues (sleeping)
    +-------------------+  - Parent (always)
                           - Temporary lookups
    
    ZOMBIE (EXIT_ZOMBIE):
    +-------------------+
    |   task_struct     |  Referenced by:
    |                   |  - Parent (waiting to reap)
    |   usage = small   |
    +-------------------+
    
    DEAD (after wait()):
    +-------------------+
    |   freed           |  Parent reaped
    +-------------------+  → put_task_struct() → free
```

### Safe task_struct Access

```c
/* CORRECT: Using RCU for safe lookup */
void print_task_name(pid_t pid)
{
    struct task_struct *task;
    
    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task) {
        /* Safe under RCU */
        printk("Name: %s\n", task->comm);
    }
    rcu_read_unlock();
    /* Cannot access task here - RCU protection ended */
}

/* CORRECT: Acquiring reference for longer use */
int do_something_with_task(pid_t pid)
{
    struct task_struct *task;
    int ret;
    
    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task)
        get_task_struct(task);  /* Acquire reference */
    rcu_read_unlock();
    
    if (!task)
        return -ESRCH;
    
    ret = long_operation(task); /* Safe - we hold reference */
    
    put_task_struct(task);      /* Release reference */
    return ret;
}
```

**中文解释：**
- task_struct 所有权模型：
  1. fork() 创建，父进程是隐式所有者
  2. RCU 保护短期访问
  3. `get_task_struct()` 获取长期引用
  4. 僵尸进程由父进程回收后释放

---

## Refcounting Rules

```
+------------------------------------------------------------------+
|  KERNEL REFCOUNTING RULES                                        |
+------------------------------------------------------------------+

    RULE 1: ACQUIRE BEFORE USE
    +----------------------------------------------------------+
    | You MUST hold a reference before accessing an object     |
    | Either:                                                  |
    |   - You allocated it (initial reference)                 |
    |   - You explicitly acquired it (refcount++)              |
    |   - You're protected by RCU, spinlock, etc.              |
    +----------------------------------------------------------+
    
    RULE 2: RELEASE AFTER DONE
    +----------------------------------------------------------+
    | You MUST release references when done                    |
    | Failure = memory leak                                    |
    +----------------------------------------------------------+
    
    RULE 3: BALANCED ACQUIRE/RELEASE
    +----------------------------------------------------------+
    | Every get() must have matching put()                     |
    | Every refcount++ must have refcount--                    |
    | Code review: Count the pairs!                            |
    +----------------------------------------------------------+
    
    RULE 4: DON'T ACCESS AFTER RELEASE
    +----------------------------------------------------------+
    | After put(), the object may be freed                     |
    | Accessing it = use-after-free                            |
    +----------------------------------------------------------+
    
    RULE 5: TRANSFER SEMANTICS
    +----------------------------------------------------------+
    | Some functions consume the reference                      |
    | "steal" semantics: caller loses reference                |
    | Document clearly in function comments                    |
    +----------------------------------------------------------+
```

### Common Refcount Patterns

```c
/* Pattern 1: Get-Use-Put */
void pattern_get_use_put(void)
{
    struct foo *obj;
    
    obj = foo_get();      /* Acquire */
    if (!obj)
        return;
    
    use_foo(obj);         /* Use */
    
    foo_put(obj);         /* Release */
}

/* Pattern 2: Conditional Get */
void pattern_conditional_get(struct foo *maybe_null)
{
    if (maybe_null) {
        foo_get(maybe_null);
        /* ... */
        foo_put(maybe_null);
    }
}

/* Pattern 3: Transfer (caller loses ref) */
void takes_ownership(struct foo *obj)
{
    queue_add(obj);  /* Queue now owns obj */
    /* Caller should NOT call foo_put() */
}

void caller(void)
{
    struct foo *obj = foo_alloc();  /* I own it */
    takes_ownership(obj);            /* Transferred */
    /* Do NOT use obj anymore */
}

/* Pattern 4: Error Path */
int pattern_error_path(void)
{
    struct foo *obj;
    int err;
    
    obj = foo_alloc();
    if (!obj)
        return -ENOMEM;
    
    err = step1(obj);
    if (err)
        goto err_out;  /* Must release on error */
    
    err = step2(obj);
    if (err)
        goto err_out;
    
    return 0;
    
err_out:
    foo_put(obj);
    return err;
}
```

**中文解释：**
- 引用计数规则：
  1. 使用前必须持有引用
  2. 完成后必须释放引用
  3. 获取/释放必须配对
  4. 释放后不能访问
  5. 转移语义需明确文档

---

## Common Lifetime Bugs

### Bug 1: Use-After-Free

```c
/* BUG: Accessing freed object */
void use_after_free_bug(struct foo *obj)
{
    foo_put(obj);           /* Release reference */
    
    printk("%d\n", obj->x); /* BUG: obj may be freed! */
}

/* CORRECT */
void use_after_free_fixed(struct foo *obj)
{
    int x = obj->x;         /* Read first */
    foo_put(obj);           /* Then release */
    printk("%d\n", x);      /* Use local copy */
}
```

### Bug 2: Double Free

```c
/* BUG: Double release */
void double_free_bug(struct foo *obj)
{
    foo_put(obj);
    /* ... */
    foo_put(obj);           /* BUG: Already released! */
}

/* Pattern to prevent: NULL after put */
void double_free_prevention(struct foo **objp)
{
    if (*objp) {
        foo_put(*objp);
        *objp = NULL;       /* Prevent double free */
    }
}
```

### Bug 3: Reference Leak

```c
/* BUG: Missing put on error path */
int reference_leak_bug(void)
{
    struct foo *obj = foo_get();
    
    if (condition1)
        return -EINVAL;     /* BUG: Leaked obj! */
    
    if (condition2)
        return -EBUSY;      /* BUG: Leaked obj! */
    
    foo_put(obj);
    return 0;
}

/* CORRECT: All paths release */
int reference_leak_fixed(void)
{
    struct foo *obj = foo_get();
    int ret = 0;
    
    if (condition1) {
        ret = -EINVAL;
        goto out;
    }
    
    if (condition2) {
        ret = -EBUSY;
        goto out;
    }
    
out:
    foo_put(obj);
    return ret;
}
```

### Bug 4: Dangling Pointer

```c
/* BUG: Storing pointer after ownership transfer */
struct foo *global_ptr;

void dangling_pointer_bug(struct foo *obj)
{
    global_ptr = obj;       /* Store pointer */
    hand_to_framework(obj); /* Transfer ownership */
    
    /* global_ptr is now dangling! */
}

/* CORRECT: Get new reference if storing */
void dangling_pointer_fixed(struct foo *obj)
{
    global_ptr = foo_get_ref(obj);  /* New reference */
    hand_to_framework(obj);          /* Original transferred */
    
    /* global_ptr is valid - has its own reference */
}
```

**中文解释：**
- 常见生命周期错误：
  1. **Use-After-Free**：释放后访问
  2. **Double Free**：重复释放
  3. **Reference Leak**：错误路径忘记释放
  4. **Dangling Pointer**：所有权转移后仍持有指针

---

## User-Space Application

```c
/* user_space_refcount.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

/*---------------------------------------------------------
 * Reference-counted object
 *---------------------------------------------------------*/
struct refcounted_obj {
    atomic_int refcount;
    char data[64];
    void (*destructor)(struct refcounted_obj *);
};

struct refcounted_obj *obj_create(const char *data)
{
    struct refcounted_obj *obj = malloc(sizeof(*obj));
    if (!obj)
        return NULL;
    
    atomic_init(&obj->refcount, 1);  /* Initial reference */
    snprintf(obj->data, sizeof(obj->data), "%s", data);
    obj->destructor = NULL;
    
    printf("[ALLOC] Created object: %s (refcount=1)\n", obj->data);
    return obj;
}

void obj_get(struct refcounted_obj *obj)
{
    int old = atomic_fetch_add(&obj->refcount, 1);
    printf("[GET] %s: refcount %d -> %d\n", obj->data, old, old + 1);
}

void obj_put(struct refcounted_obj *obj)
{
    int old = atomic_fetch_sub(&obj->refcount, 1);
    printf("[PUT] %s: refcount %d -> %d\n", obj->data, old, old - 1);
    
    if (old == 1) {
        /* Last reference released */
        printf("[FREE] Destroying object: %s\n", obj->data);
        if (obj->destructor)
            obj->destructor(obj);
        free(obj);
    }
}

/*---------------------------------------------------------
 * Usage example
 *---------------------------------------------------------*/
void worker(struct refcounted_obj *obj)
{
    printf("Worker using: %s\n", obj->data);
    obj_put(obj);  /* Worker done */
}

int main(void)
{
    /* Owner creates object */
    struct refcounted_obj *obj = obj_create("shared_resource");
    
    /* Give reference to worker 1 */
    obj_get(obj);
    worker(obj);
    
    /* Give reference to worker 2 */
    obj_get(obj);
    worker(obj);
    
    /* Owner releases its reference */
    printf("Owner releasing...\n");
    obj_put(obj);  /* Object freed here (last ref) */
    
    return 0;
}
```

Output:
```
[ALLOC] Created object: shared_resource (refcount=1)
[GET] shared_resource: refcount 1 -> 2
Worker using: shared_resource
[PUT] shared_resource: refcount 2 -> 1
[GET] shared_resource: refcount 1 -> 2
Worker using: shared_resource
[PUT] shared_resource: refcount 2 -> 1
Owner releasing...
[PUT] shared_resource: refcount 1 -> 0
[FREE] Destroying object: shared_resource
```

**中文解释：**
- 用户态引用计数实现：
  1. 原子计数器保证线程安全
  2. 创建时初始引用为 1
  3. `obj_get()` 增加引用
  4. `obj_put()` 减少引用，为 0 时释放

---

## Summary

```
+------------------------------------------------------------------+
|  OWNERSHIP & LIFETIME SUMMARY                                    |
+------------------------------------------------------------------+

    1. SINGLE OWNER RULE
       - Every object has exactly one owner
       - Owner is responsible for lifecycle
    
    2. EXPLICIT TRANSFERS
       - Ownership transfer must be explicit
       - Document in function comments
       - Caller must not access after transfer
    
    3. REFERENCE COUNTING
       - For shared access
       - get() / put() must be paired
       - Last put() triggers destruction
    
    4. ACQUIRE-USE-RELEASE
       - Always acquire before use
       - Always release when done
       - All error paths must release
    
    5. PROTECTION MECHANISMS
       - RCU for lockless read
       - Spinlocks for exclusive access
       - Refcounts for shared ownership

+------------------------------------------------------------------+
|  CHECKLIST FOR CODE REVIEW                                       |
+------------------------------------------------------------------+

    [ ] Every allocation has a corresponding free
    [ ] Every get() has a matching put()
    [ ] Error paths release all acquired resources
    [ ] No access after put() or free()
    [ ] Ownership transfers are documented
    [ ] Stored pointers have their own references
```

**中文总结：**
内核的所有权和生命周期管理规则：
1. 单一所有者规则：每个对象有且只有一个所有者
2. 显式转移：所有权转移必须明确，并在函数注释中说明
3. 引用计数：用于共享访问，get/put 必须配对
4. 获取-使用-释放模式：始终在使用前获取，完成后释放
5. 保护机制：RCU 用于无锁读取，自旋锁用于独占访问，引用计数用于共享所有权

