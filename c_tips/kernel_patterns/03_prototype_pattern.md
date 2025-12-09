# Prototype Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                    PROTOTYPE PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    Original Object (Prototype)                                    |
|    +------------------+                                           |
|    |    Prototype     |                                           |
|    +------------------+                                           |
|    | + clone()        |-------+                                   |
|    | + data           |       |                                   |
|    +------------------+       |                                   |
|                               |                                   |
|                               | clone()                           |
|                               |                                   |
|         +---------------------+---------------------+             |
|         |                     |                     |             |
|         v                     v                     v             |
|    +----+-----+         +-----+----+         +-----+----+         |
|    |  Clone 1 |         |  Clone 2 |         |  Clone 3 |         |
|    +----------+         +----------+         +----------+         |
|    | data copy|         | data copy|         | data copy|         |
|    +----------+         +----------+         +----------+         |
|                                                                   |
|    All clones start with same state as prototype                  |
|    but can be independently modified                              |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 原型模式通过克隆已有对象来创建新对象，避免重复初始化逻辑。在Linux内核中，这种模式广泛用于复制进程（fork）、复制套接字（sk_clone）、复制内存映射（dup_mm）等场景。克隆操作比从头创建更高效，因为可以复用原型对象的大部分初始化结果。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Task Structure Clone (fork)

```c
/* From: kernel/fork.c */

/**
 * dup_task_struct - Duplicate a task_struct
 * @orig: Original task to clone
 *
 * Creates a copy of the task structure for fork().
 * This is the kernel's primary example of the Prototype pattern.
 */
static struct task_struct *dup_task_struct(struct task_struct *orig)
{
    struct task_struct *tsk;
    struct thread_info *ti;
    int node = tsk_fork_get_node(orig);
    int err;

    /* Prepare the original for copying */
    prepare_to_copy(orig);

    /* Allocate new task structure */
    tsk = alloc_task_struct_node(node);
    if (!tsk)
        return NULL;

    /* Allocate thread info */
    ti = alloc_thread_info_node(tsk, node);
    if (!ti) {
        free_task_struct(tsk);
        return NULL;
    }

    /* Copy the task structure - this is the "clone" operation */
    err = arch_dup_task_struct(tsk, orig);
    if (err)
        goto out;

    /* Set up the new task's stack */
    tsk->stack = ti;
    setup_thread_stack(tsk, orig);
    
    /* Clear fields that shouldn't be inherited */
    clear_user_return_notifier(tsk);
    clear_tsk_need_resched(tsk);
    
    /* Initialize reference count */
    atomic_set(&tsk->usage, 2);

    return tsk;
out:
    free_thread_info(ti);
    free_task_struct(tsk);
    return NULL;
}

/**
 * arch_dup_task_struct - Architecture-specific task copy
 * @dst: Destination task
 * @src: Source task (prototype)
 *
 * Default implementation just does a structure copy.
 */
int __weak arch_dup_task_struct(struct task_struct *dst,
                                struct task_struct *src)
{
    *dst = *src;  /* Direct structure copy - clone the prototype */
    return 0;
}
```

### 2.2 Kernel Example: Memory Map Clone

```c
/* From: kernel/fork.c */

/**
 * dup_mm - Duplicate memory descriptor
 * @tsk: Task to associate with new mm
 *
 * Creates a copy of the current process's memory map.
 * The new mm_struct starts as a clone of the original.
 */
struct mm_struct *dup_mm(struct task_struct *tsk)
{
    struct mm_struct *mm, *oldmm = current->mm;
    int err;

    if (!oldmm)
        return NULL;

    /* Allocate new mm_struct */
    mm = allocate_mm();
    if (!mm)
        goto fail_nomem;

    /* Clone: Copy the entire structure from prototype */
    memcpy(mm, oldmm, sizeof(*mm));
    mm_init_cpumask(mm);

    /* Initialize fields specific to new instance */
    mm->token_priority = 0;
    mm->last_interval = 0;

    /* Initialize the new mm */
    if (!mm_init(mm, tsk))
        goto fail_nomem;

    /* Clone the memory mappings */
    err = dup_mmap(mm, oldmm);
    if (err)
        goto free_pt;

    /* Update statistics */
    mm->hiwater_rss = get_mm_rss(mm);
    mm->hiwater_vm = mm->total_vm;

    return mm;

free_pt:
    mm->binfmt = NULL;
    mmput(mm);
fail_nomem:
    return NULL;
}
```

### 2.3 Kernel Example: Socket Clone

```c
/* From: net/core/sock.c */

/**
 * sk_clone - Clone a socket
 * @sk: Original socket (prototype)
 * @priority: Allocation priority
 *
 * Duplicates a socket structure. Used in accept() to create
 * a new socket for each incoming connection.
 */
struct sock *sk_clone(const struct sock *sk, const gfp_t priority)
{
    struct sock *newsk;

    /* Allocate new socket using same protocol */
    newsk = sk_prot_alloc(sk->sk_prot, priority, sk->sk_family);
    if (newsk != NULL) {
        struct sk_filter *filter;

        /* Clone: Copy socket data from prototype */
        sock_copy(newsk, sk);

        /* Initialize fields specific to new instance */
        get_net(sock_net(newsk));
        sk_node_init(&newsk->sk_node);
        sock_lock_init(newsk);
        bh_lock_sock(newsk);
        
        /* Clear/reset queues */
        newsk->sk_backlog.head = newsk->sk_backlog.tail = NULL;
        newsk->sk_backlog.len = 0;
        atomic_set(&newsk->sk_rmem_alloc, 0);
        atomic_set(&newsk->sk_wmem_alloc, 1);
        
        skb_queue_head_init(&newsk->sk_receive_queue);
        skb_queue_head_init(&newsk->sk_write_queue);

        /* Clone filter if present */
        filter = rcu_dereference_protected(newsk->sk_filter, 1);
        if (filter != NULL)
            sk_filter_charge(newsk, filter);

        /* Clone security policy */
        if (unlikely(xfrm_sk_clone_policy(newsk))) {
            newsk->sk_destruct = NULL;
            bh_unlock_sock(newsk);
            sk_free(newsk);
            newsk = NULL;
            goto out;
        }

        /* Reset error and priority */
        newsk->sk_err = 0;
        newsk->sk_priority = 0;
        
        /* Set reference count */
        atomic_set(&newsk->sk_refcnt, 2);
    }
out:
    return newsk;
}
```

### 2.4 Kernel Example: SKB Clone

```c
/* From: net/core/skbuff.c */

/**
 * skb_clone - Duplicate an sk_buff
 * @skb: Buffer to clone (prototype)
 * @gfp_mask: Allocation priority
 *
 * Creates a clone of a network buffer. The clone shares
 * the same data but has its own header structure.
 */
struct sk_buff *skb_clone(struct sk_buff *skb, gfp_t gfp_mask)
{
    struct sk_buff *n;

    /* Handle zero-copy buffers */
    if (skb_shinfo(skb)->tx_flags & SKBTX_DEV_ZEROCOPY) {
        if (skb_copy_ubufs(skb, gfp_mask))
            return NULL;
    }

    /* Try to use pre-allocated clone space */
    n = skb + 1;
    if (skb->fclone == SKB_FCLONE_ORIG &&
        n->fclone == SKB_FCLONE_UNAVAILABLE) {
        n->fclone = SKB_FCLONE_CLONE;
        atomic_inc(fclone_ref);
    } else {
        /* Allocate new skb */
        n = kmem_cache_alloc(skbuff_head_cache, gfp_mask);
        if (!n)
            return NULL;
        n->fclone = SKB_FCLONE_UNAVAILABLE;
    }

    /* Clone: Copy header from prototype */
    return __skb_clone(n, skb);
}
```

### 2.5 Architecture Diagram

```
+------------------------------------------------------------------+
|                 LINUX KERNEL PROTOTYPE PATTERN                    |
+------------------------------------------------------------------+
|                                                                   |
|    fork() System Call                                             |
|    +------------------+                                           |
|    | Parent Process   | (Prototype)                               |
|    +------------------+                                           |
|    | task_struct      |                                           |
|    | mm_struct        |                                           |
|    | files_struct     |                                           |
|    | signal_struct    |                                           |
|    +--------+---------+                                           |
|             |                                                     |
|             | do_fork() -> copy_process()                         |
|             |                                                     |
|    +--------v---------+                                           |
|    |  Clone Process   |                                           |
|    +------------------+                                           |
|    | dup_task_struct()|-----> Copy task_struct                    |
|    | dup_mm()         |-----> Copy/share memory                   |
|    | copy_files()     |-----> Copy/share file descriptors         |
|    | copy_sighand()   |-----> Copy/share signal handlers          |
|    +--------+---------+                                           |
|             |                                                     |
|             v                                                     |
|    +--------+---------+                                           |
|    | Child Process    | (Clone)                                   |
|    +------------------+                                           |
|    | Copied fields    | <-- Independent copy                      |
|    | Shared resources | <-- Reference counted sharing             |
|    | New PID          | <-- Unique identity                       |
|    +------------------+                                           |
|                                                                   |
|    COW (Copy-On-Write): Pages shared until modified               |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux的fork()系统调用是原型模式的典型应用。父进程作为原型，通过copy_process()函数克隆出子进程。克隆过程包括复制任务结构、内存映射、文件描述符等。为提高效率，内核使用写时复制(COW)技术，页面在修改前共享，修改时才真正复制。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Performance** | Copying existing object faster than full initialization |
| **Copy-On-Write** | Delayed copying reduces unnecessary memory allocation |
| **State Preservation** | New objects inherit prototype's configuration |
| **Simplified Creation** | Avoid complex initialization code duplication |
| **Runtime Configuration** | Clone objects configured at runtime |
| **Resource Sharing** | Reference counting allows safe resource sharing |

**中文说明：** 原型模式的优势包括：性能提升（复制比完整初始化更快）、写时复制（延迟复制减少内存分配）、状态保留（新对象继承原型配置）、简化创建（避免重复初始化代码）、运行时配置（克隆运行时配置的对象）、资源共享（引用计数实现安全共享）。

---

## 4. User-Space Implementation Example

```c
/*
 * Prototype Pattern - User Space Implementation
 * Mimics Linux Kernel's fork/clone mechanism
 * 
 * Compile: gcc -o prototype prototype.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Prototype Interface
 * Similar to kernel's task_struct clone mechanism
 * ============================================================ */

/* Forward declaration */
struct process;

/* Clone function type - the prototype pattern's core operation */
typedef struct process *(*clone_fn)(const struct process *);

/* ============================================================
 * Process Structure - Our "Prototype" object
 * Similar to kernel's task_struct
 * ============================================================ */

/* Memory region structure - like kernel's vm_area_struct */
struct memory_region {
    unsigned long start_addr;    /* Start address */
    unsigned long end_addr;      /* End address */
    int permissions;             /* rwx permissions */
    char name[64];               /* Region name */
    struct memory_region *next;  /* Linked list */
};

/* File descriptor entry */
struct fd_entry {
    int fd;                      /* File descriptor number */
    char filename[256];          /* File name */
    int flags;                   /* Open flags */
};

/* Process structure - our prototype object */
struct process {
    /* Identity */
    int pid;                     /* Process ID */
    int ppid;                    /* Parent process ID */
    char name[64];               /* Process name */
    
    /* State */
    int state;                   /* Running, sleeping, etc. */
    int exit_code;               /* Exit code */
    
    /* Resources */
    struct memory_region *memory;/* Memory map (linked list) */
    struct fd_entry *fds;        /* File descriptors */
    int fd_count;                /* Number of open files */
    int max_fds;                 /* Maximum file descriptors */
    
    /* Scheduling */
    int priority;                /* Scheduling priority */
    unsigned long runtime;       /* Total runtime */
    
    /* Clone function - enables prototype pattern */
    clone_fn clone;
    
    /* Reference counting for shared resources */
    int *ref_count;
};

/* Global PID counter */
static int next_pid = 1000;

/* ============================================================
 * Memory Region Operations
 * ============================================================ */

/**
 * create_memory_region - Create a new memory region
 */
struct memory_region *create_memory_region(unsigned long start, 
                                            unsigned long end,
                                            int perms, 
                                            const char *name)
{
    struct memory_region *region = malloc(sizeof(struct memory_region));
    if (!region) return NULL;
    
    region->start_addr = start;
    region->end_addr = end;
    region->permissions = perms;
    strncpy(region->name, name, sizeof(region->name) - 1);
    region->next = NULL;
    
    return region;
}

/**
 * clone_memory_regions - Deep copy of memory region list
 * 
 * This simulates Copy-On-Write behavior by creating new
 * region structures but could share underlying pages.
 */
struct memory_region *clone_memory_regions(const struct memory_region *src)
{
    struct memory_region *head = NULL;
    struct memory_region *tail = NULL;
    
    while (src) {
        /* Create copy of each region */
        struct memory_region *copy = create_memory_region(
            src->start_addr,
            src->end_addr,
            src->permissions,
            src->name
        );
        
        if (!copy) {
            /* Cleanup on failure */
            while (head) {
                struct memory_region *tmp = head;
                head = head->next;
                free(tmp);
            }
            return NULL;
        }
        
        /* Link into list */
        if (!head) {
            head = tail = copy;
        } else {
            tail->next = copy;
            tail = copy;
        }
        
        src = src->next;
    }
    
    return head;
}

/**
 * free_memory_regions - Free memory region list
 */
void free_memory_regions(struct memory_region *region)
{
    while (region) {
        struct memory_region *tmp = region;
        region = region->next;
        free(tmp);
    }
}

/* ============================================================
 * File Descriptor Operations
 * ============================================================ */

/**
 * clone_file_descriptors - Copy file descriptor table
 * 
 * In kernel, this would share struct file with ref counting.
 */
struct fd_entry *clone_file_descriptors(const struct fd_entry *src, 
                                         int count, 
                                         int max)
{
    struct fd_entry *fds = malloc(sizeof(struct fd_entry) * max);
    if (!fds) return NULL;
    
    /* Copy existing entries */
    memcpy(fds, src, sizeof(struct fd_entry) * count);
    
    return fds;
}

/* ============================================================
 * Process Clone Implementation
 * This is the core of the Prototype Pattern
 * ============================================================ */

/**
 * process_clone - Clone a process (Prototype Pattern core)
 * @prototype: The process to clone
 *
 * Creates a new process as a copy of the prototype.
 * Similar to kernel's copy_process() in fork.c
 *
 * Returns: New process or NULL on failure
 */
struct process *process_clone(const struct process *prototype)
{
    struct process *clone;
    
    if (!prototype) return NULL;
    
    /* Allocate new process structure */
    clone = malloc(sizeof(struct process));
    if (!clone) return NULL;
    
    printf("[CLONE] Cloning process '%s' (PID %d)\n", 
           prototype->name, prototype->pid);
    
    /* Copy basic fields from prototype */
    memcpy(clone, prototype, sizeof(struct process));
    
    /* Assign new unique PID */
    clone->pid = next_pid++;
    clone->ppid = prototype->pid;  /* Parent is the prototype */
    
    /* Reset runtime statistics */
    clone->runtime = 0;
    clone->exit_code = 0;
    clone->state = 0;  /* Running */
    
    /* Clone memory regions (deep copy) */
    clone->memory = clone_memory_regions(prototype->memory);
    if (prototype->memory && !clone->memory) {
        free(clone);
        return NULL;
    }
    
    /* Clone file descriptors */
    clone->fds = clone_file_descriptors(prototype->fds, 
                                         prototype->fd_count,
                                         prototype->max_fds);
    if (prototype->fds && !clone->fds) {
        free_memory_regions(clone->memory);
        free(clone);
        return NULL;
    }
    
    /* Allocate new reference counter */
    clone->ref_count = malloc(sizeof(int));
    if (!clone->ref_count) {
        free(clone->fds);
        free_memory_regions(clone->memory);
        free(clone);
        return NULL;
    }
    *clone->ref_count = 1;
    
    printf("[CLONE] Created process '%s' (PID %d) from prototype (PID %d)\n",
           clone->name, clone->pid, clone->ppid);
    
    return clone;
}

/* ============================================================
 * Process Creation and Destruction
 * ============================================================ */

/**
 * process_create - Create a new process from scratch
 * @name: Process name
 *
 * Creates a prototype process that can be cloned later.
 */
struct process *process_create(const char *name)
{
    struct process *proc = malloc(sizeof(struct process));
    if (!proc) return NULL;
    
    /* Initialize identity */
    proc->pid = next_pid++;
    proc->ppid = 1;  /* Init is parent */
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    
    /* Initialize state */
    proc->state = 0;
    proc->exit_code = 0;
    proc->priority = 20;  /* Default priority */
    proc->runtime = 0;
    
    /* Initialize resources */
    proc->memory = NULL;
    proc->fds = malloc(sizeof(struct fd_entry) * 256);
    proc->fd_count = 0;
    proc->max_fds = 256;
    
    /* Set clone function - enables prototype pattern */
    proc->clone = process_clone;
    
    /* Initialize reference counter */
    proc->ref_count = malloc(sizeof(int));
    *proc->ref_count = 1;
    
    printf("[CREATE] Created new process '%s' (PID %d)\n", name, proc->pid);
    
    return proc;
}

/**
 * process_destroy - Free process resources
 */
void process_destroy(struct process *proc)
{
    if (!proc) return;
    
    printf("[DESTROY] Destroying process '%s' (PID %d)\n", 
           proc->name, proc->pid);
    
    /* Decrement reference count */
    (*proc->ref_count)--;
    
    /* Only free when no more references */
    if (*proc->ref_count <= 0) {
        free(proc->ref_count);
        free_memory_regions(proc->memory);
        free(proc->fds);
    }
    
    free(proc);
}

/* ============================================================
 * Process Operations
 * ============================================================ */

/**
 * process_add_memory_region - Add memory region to process
 */
void process_add_memory_region(struct process *proc,
                               unsigned long start,
                               unsigned long end,
                               int perms,
                               const char *name)
{
    struct memory_region *region = create_memory_region(start, end, perms, name);
    if (!region) return;
    
    /* Add to head of list */
    region->next = proc->memory;
    proc->memory = region;
    
    printf("[MEMORY] Added region '%s' [0x%lx-0x%lx] to PID %d\n",
           name, start, end, proc->pid);
}

/**
 * process_add_fd - Add file descriptor to process
 */
void process_add_fd(struct process *proc, const char *filename, int flags)
{
    if (proc->fd_count >= proc->max_fds) return;
    
    struct fd_entry *entry = &proc->fds[proc->fd_count];
    entry->fd = proc->fd_count + 3;  /* Start after stdin/out/err */
    strncpy(entry->filename, filename, sizeof(entry->filename) - 1);
    entry->flags = flags;
    proc->fd_count++;
    
    printf("[FD] Added fd %d (%s) to PID %d\n", 
           entry->fd, filename, proc->pid);
}

/**
 * process_print - Print process information
 */
void process_print(const struct process *proc)
{
    struct memory_region *region;
    int i;
    
    printf("\n========================================\n");
    printf("Process: %s\n", proc->name);
    printf("========================================\n");
    printf("PID: %d, PPID: %d\n", proc->pid, proc->ppid);
    printf("State: %d, Priority: %d\n", proc->state, proc->priority);
    printf("Runtime: %lu\n", proc->runtime);
    
    printf("\nMemory Regions:\n");
    region = proc->memory;
    while (region) {
        printf("  [0x%08lx-0x%08lx] %s (perms: %d)\n",
               region->start_addr, region->end_addr,
               region->name, region->permissions);
        region = region->next;
    }
    
    printf("\nFile Descriptors (%d):\n", proc->fd_count);
    for (i = 0; i < proc->fd_count; i++) {
        printf("  fd %d: %s (flags: %d)\n",
               proc->fds[i].fd, proc->fds[i].filename, proc->fds[i].flags);
    }
    printf("========================================\n\n");
}

/* ============================================================
 * Main - Demonstrate Prototype Pattern
 * ============================================================ */

int main(void)
{
    struct process *parent;
    struct process *child1;
    struct process *child2;

    printf("=== Prototype Pattern Demo (fork simulation) ===\n\n");

    /* Create prototype (parent) process */
    parent = process_create("parent_app");
    
    /* Set up prototype with resources */
    process_add_memory_region(parent, 0x400000, 0x401000, 5, "code");
    process_add_memory_region(parent, 0x600000, 0x601000, 6, "data");
    process_add_memory_region(parent, 0x7fff0000, 0x80000000, 7, "stack");
    
    process_add_fd(parent, "/etc/passwd", 0);
    process_add_fd(parent, "/var/log/app.log", 1);
    
    parent->priority = 10;
    parent->runtime = 12345;
    
    printf("\n--- Original Process (Prototype) ---\n");
    process_print(parent);

    /* Clone the process - this is the Prototype Pattern in action */
    printf("--- Cloning Process (like fork) ---\n\n");
    child1 = parent->clone(parent);
    
    printf("\n--- First Clone ---\n");
    process_print(child1);
    
    /* Modify the child - demonstrates independence */
    printf("--- Modifying child1 ---\n");
    strncpy(child1->name, "child1_app", sizeof(child1->name));
    process_add_memory_region(child1, 0x700000, 0x701000, 6, "heap");
    process_add_fd(child1, "/tmp/child1.tmp", 2);
    
    process_print(child1);
    
    /* Clone again from parent - another prototype copy */
    printf("--- Creating second clone from original ---\n\n");
    child2 = parent->clone(parent);
    strncpy(child2->name, "child2_app", sizeof(child2->name));
    
    process_print(child2);
    
    /* Show that parent is unchanged */
    printf("--- Original Parent (unchanged) ---\n");
    process_print(parent);

    /* Cleanup */
    process_destroy(child2);
    process_destroy(child1);
    process_destroy(parent);

    printf("=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Clone Flow Diagram

```
+------------------------------------------------------------------+
|                    PROTOTYPE CLONE FLOW                           |
+------------------------------------------------------------------+
|                                                                   |
|    Parent Process (Prototype)                                     |
|    +---------------------------+                                  |
|    | PID: 1000                 |                                  |
|    | Name: "parent_app"        |                                  |
|    | Priority: 10              |                                  |
|    +---------------------------+                                  |
|    | Memory Regions:           |                                  |
|    |   [code]  0x400000        |                                  |
|    |   [data]  0x600000        |                                  |
|    |   [stack] 0x7fff0000      |                                  |
|    +---------------------------+                                  |
|    | File Descriptors:         |                                  |
|    |   fd 3: /etc/passwd       |                                  |
|    |   fd 4: /var/log/app.log  |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  | clone()                                        |
|                  v                                                |
|    +-------------+-------------+                                  |
|    |     Clone Operation       |                                  |
|    +---------------------------+                                  |
|    | 1. Allocate new struct    |                                  |
|    | 2. memcpy(clone, proto)   |                                  |
|    | 3. Assign new PID         |                                  |
|    | 4. Deep copy memory list  |                                  |
|    | 5. Copy fd table          |                                  |
|    | 6. Reset statistics       |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  v                                                |
|    +-------------+-------------+                                  |
|    | Child Process (Clone)     |                                  |
|    +---------------------------+                                  |
|    | PID: 1001 (NEW)           |                                  |
|    | PPID: 1000 (parent)       |                                  |
|    | Name: "parent_app" (copy) |                                  |
|    | Priority: 10 (inherited)  |                                  |
|    +---------------------------+                                  |
|    | Memory: COPIED            |                                  |
|    | FDs: COPIED               |                                  |
|    | Runtime: 0 (reset)        |                                  |
|    +---------------------------+                                  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 克隆流程图展示了从原型进程复制出子进程的过程：首先分配新结构，然后使用memcpy复制原型数据，接着分配新PID，深度复制内存区域列表，复制文件描述符表，最后重置统计数据。子进程继承父进程的配置但有独立的资源副本。

---

## 6. Key Implementation Points

1. **memcpy for Structure Copy**: Fast bulk copy of prototype fields
2. **Deep Copy for Pointers**: Linked structures must be independently copied
3. **Unique Identity**: Clone gets new PID/identity while inheriting configuration
4. **Reset Mutable State**: Statistics and runtime data should be reset
5. **Reference Counting**: Shared resources need proper ref counting
6. **Clone Function Pointer**: Object carries its own clone method

**中文说明：** 实现原型模式的关键点：使用memcpy快速复制结构体、指针指向的结构需要深度复制、克隆对象获得新身份但继承配置、重置可变状态（如统计数据）、共享资源需要引用计数、对象携带自己的clone方法指针。

