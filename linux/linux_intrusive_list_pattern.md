# Linux Kernel Intrusive List Pattern (v3.2)

## Overview

This document explains the **embedded-node / intrusive list pattern** used throughout the Linux kernel, focusing on:
- Why this pattern exists and what problems it solves
- How it is used safely with proper ownership and lifetime contracts
- Real kernel examples from multiple subsystems
- How to apply this pattern in user-space C systems

---

## Phase 1 — What Problem This Pattern Solves

### 1.1 What is an "Intrusive List"?

An **intrusive list** is a data structure where the list node is embedded directly inside the object being stored, rather than allocating a separate container node that points to the object.

```
+------------------------------------------+
|    NON-INTRUSIVE LIST                    |
+------------------------------------------+

    +-------------+     +-------------+
    | list_node   |---->| list_node   |---->...
    |   *data ----|     |   *data ----|
    +------+------+     +------+------+
           |                   |
           v                   v
    +-------------+     +-------------+
    |  my_object  |     |  my_object  |
    +-------------+     +-------------+

    - List node is SEPARATE from the object
    - Requires additional memory allocation
    - Extra pointer indirection
    - Object doesn't know it's in a list

+------------------------------------------+
|    INTRUSIVE LIST (Linux kernel style)   |
+------------------------------------------+

    +-----------------------+     +-----------------------+
    |      my_object        |     |      my_object        |
    |   +---------------+   |     |   +---------------+   |
    |   | list_head     |<--+---->|   | list_head     |<--+--->...
    |   | (next, prev)  |   |     |   | (next, prev)  |   |
    |   +---------------+   |     |   +---------------+   |
    |     other_field_1     |     |     other_field_1     |
    |     other_field_2     |     |     other_field_2     |
    +-----------------------+     +-----------------------+

    - List node is EMBEDDED inside the object
    - No separate allocation needed
    - Object can be on multiple lists simultaneously
    - container_of() recovers the enclosing object
```

**中文解释：**
- 侵入式链表（Intrusive List）将链表节点直接嵌入到对象内部，而非使用独立的容器节点
- 非侵入式链表需要为每个节点分配额外内存，而侵入式链表无需额外分配
- 侵入式链表的优势在于：零分配开销、一个对象可同时存在于多个链表中

### 1.2 How It Differs from Other Containers

| Aspect | Array-based | Pointer-to-node | Intrusive (Linux) |
|--------|-------------|-----------------|-------------------|
| Allocation | Bulk pre-alloc | Per-node malloc | Embedded, zero alloc |
| Cache locality | Excellent | Poor | Excellent |
| Multiple lists | Impossible | Impossible | Yes (embed multiple list_head) |
| Type safety | Homogeneous | Void* or templates | container_of() |
| Ownership | Container owns | Container owns node | Object owns node |

### 1.3 Why the Kernel Avoids Generic Containers

The kernel avoids generic containers (like C++ STL) for critical reasons:

**1. Memory Allocation Control:**
```c
/* Generic container approach (AVOIDED in kernel) */
struct node {
    void *data;              /* Hidden allocation */
    struct node *next;       /* Hidden allocation */
};

/* Intrusive approach (USED in kernel) */
struct task_struct {
    /* ... */
    struct list_head tasks;  /* NO allocation - embedded */
    /* ... */
};
```

**2. Type Information at Compile Time:**
- Generic void* containers lose type information
- `container_of()` recovers exact type without runtime overhead

**3. No Hidden Behavior:**
- No hidden malloc/free in hot paths
- Deterministic memory usage
- Explicit lifetime management

**中文解释：**
- 内核避免使用泛型容器的核心原因是：内存分配必须显式可控
- 在热路径（hot path）中绝不能有隐藏的内存分配，这会破坏实时性和可预测性
- `container_of()` 宏在编译期恢复类型信息，无运行时开销

### 1.4 Why Memory Allocation is Avoided in Hot Paths

```
+-----------------------------------------------+
|  KERNEL HOT PATH REQUIREMENTS                 |
+-----------------------------------------------+
|                                               |
|  1. LATENCY: Microsecond response required    |
|     - malloc() has unpredictable latency      |
|     - May trigger page reclaim                |
|     - May block on memory pressure            |
|                                               |
|  2. ATOMICITY: May run in atomic context      |
|     - Interrupt handlers                      |
|     - Spinlock-protected sections             |
|     - Cannot sleep, cannot allocate           |
|                                               |
|  3. MEMORY PRESSURE: No room for failure      |
|     - Hot paths must succeed                  |
|     - Cannot return -ENOMEM                   |
|     - Pre-allocated structures only           |
+-----------------------------------------------+
```

Example: Timer list (from `include/linux/timer.h`):

```c
struct timer_list {
    struct list_head entry;      /* Embedded list node */
    unsigned long expires;
    void (*function)(unsigned long);
    unsigned long data;
    /* ... */
};
```

When adding a timer, NO allocation occurs - the timer object already contains its list node.

**中文解释：**
- 内核热路径（如中断处理、定时器）对延迟要求极高，malloc 延迟不可预测
- 在原子上下文（持有自旋锁、中断处理中）不能睡眠，因此不能调用可能睡眠的分配器
- 侵入式链表允许对象预先包含链表节点，添加/删除操作无需任何内存分配

---

## Phase 2 — `struct list_head` Fundamentals

### 2.1 The list_head Structure

From `include/linux/types.h`:

```c
struct list_head {
    struct list_head *next, *prev;
};
```

This is a **circular doubly-linked list** with these properties:
- Only 16 bytes (two pointers on 64-bit)
- Symmetric: head and entries have the same structure
- Empty list: `next` and `prev` both point to head itself

```
+------------------------------------------+
|  CIRCULAR DOUBLY-LINKED LIST             |
+------------------------------------------+

              +----------+
              |   HEAD   |
              | next|prev|
              +--+----+--+
                 |    ^
                 v    |
              +--+----+--+
              | entry A  |
              | next|prev|
              +--+----+--+
                 |    ^
                 v    |
              +--+----+--+
              | entry B  |
              | next|prev|
              +--+----+--+
                 |    ^
                 |    |
                 +----+ (back to HEAD)

    Empty list: head->next == head->prev == &head
```

**中文解释：**
- `list_head` 仅包含两个指针：`next` 和 `prev`，形成循环双向链表
- 空链表的特征是：头节点的 next 和 prev 都指向自己
- 链表头和链表元素使用相同的结构，简化了代码实现

### 2.2 Why list_head is Embedded, Not Allocated

```
+------------------------------------------+
|  EMBEDDED vs ALLOCATED                   |
+------------------------------------------+

    ALLOCATED (BAD for kernel):
    
    malloc for         malloc for
    container node     actual object
          |                 |
          v                 v
    +----------+      +------------+
    | list_ptr |----->| my_struct  |
    | next/prev|      +------------+
    +----------+
    
    Problems:
    - Two allocations per object
    - Extra cache miss (indirection)
    - Memory fragmentation
    - Must track two lifetimes

    EMBEDDED (Linux kernel way):
    
          Single allocation
                 |
                 v
    +------------------------+
    |      my_struct         |
    |  +-----------------+   |
    |  | list_head       |   |  <- Embedded, not allocated
    |  | (next, prev)    |   |
    |  +-----------------+   |
    |    other_fields        |
    +------------------------+
    
    Benefits:
    - One allocation per object
    - No extra indirection
    - Better cache locality
    - Single lifetime to manage
```

### 2.3 One Object, Multiple Lists

A critical feature: one object can be on MULTIPLE lists simultaneously by embedding multiple `list_head` fields:

From `include/linux/fs.h` - inode structure:

```c
struct inode {
    /* ... */
    struct list_head    i_wb_list;   /* backing dev writeback list */
    struct list_head    i_lru;       /* inode LRU list */
    struct list_head    i_sb_list;   /* superblock's inode list */
    struct list_head    i_dentry;    /* alias list (dentries) */
    /* ... */
};
```

```
+------------------------------------------+
|  INODE ON MULTIPLE LISTS                 |
+------------------------------------------+

                   +-------------------+
                   |      inode        |
                   | +---------------+ |
      wb_list ---->| | i_wb_list     |<-----> other inodes
                   | +---------------+ |
     lru_list ---->| | i_lru         |<-----> other inodes
                   | +---------------+ |
      sb_list ---->| | i_sb_list     |<-----> other inodes
                   | +---------------+ |
    dentry_list -->| | i_dentry      |<-----> dentries
                   | +---------------+ |
                   |  other fields     |
                   +-------------------+

    - Same inode object participates in 4 different lists
    - Each list_head links to different peer objects
    - Impossible with non-intrusive containers!
```

**中文解释：**
- 一个对象可以同时属于多个链表，只需嵌入多个 `list_head` 字段
- 例如 inode 同时在 writeback 列表、LRU 缓存、超级块列表、dentry 别名列表中
- 非侵入式容器无法实现这种"一对多"的关系，除非为每个关系创建包装对象

### 2.4 Why This is Impossible with Non-Intrusive Containers

With non-intrusive containers, each list node "owns" a pointer to the object:

```c
/* Non-intrusive approach */
struct list_node {
    void *object;
    struct list_node *next, *prev;
};

/* To put object in two lists: */
struct list_node *node1 = malloc(sizeof(*node1));  /* Allocation! */
struct list_node *node2 = malloc(sizeof(*node2));  /* Allocation! */
node1->object = my_inode;
node2->object = my_inode;  /* Same object, two allocations */
```

Problems:
1. N allocations for N lists
2. Extra indirection to reach object
3. Lifetime coupling between node and object is unclear
4. Memory fragmentation

**中文解释：**
- 非侵入式方式需要为每个列表关系分配节点，N 个列表需要 N 次分配
- 导致内存碎片化、缓存不友好、生命周期管理复杂

---

## Phase 3 — `container_of` and Type Recovery

### 3.1 What container_of() Does

From `include/linux/kernel.h`:

```c
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);        \
    (type *)( (char *)__mptr - offsetof(type,member) );})
```

```
+------------------------------------------+
|  container_of() POINTER ARITHMETIC       |
+------------------------------------------+

    Given: pointer to list_head member
    Goal:  pointer to containing struct

                 offsetof(type, member)
                 <--------------------->
    
    +-----------------------------------+
    |         struct my_object          |
    +-----------------------------------+
    | field_a                           |
    | field_b                           |
    | +------------------+              |
    | | list_head member | <-- ptr      |
    | +------------------+              |
    | field_c                           |
    +-----------------------------------+
    ^
    |
    container_of(ptr, struct my_object, member)
    
    = (struct my_object *)((char *)ptr - offsetof(struct my_object, member))
```

### 3.2 How Pointer Arithmetic Works

```c
/* Example: task_struct contains children list_head */
struct task_struct {
    /* offset 0 */    int field1;
    /* offset 4 */    int field2;
    /* ... */
    /* offset 1320 */ struct list_head children;  /* Example offset */
    /* ... */
};

/* Given pointer to children list_head */
struct list_head *child_ptr = /* some pointer */;

/* Recover the task_struct pointer */
struct task_struct *task = container_of(child_ptr, 
                                        struct task_struct, 
                                        children);

/* Internally: */
/* task = (struct task_struct *)((char *)child_ptr - 1320); */
```

`offsetof()` from `include/linux/stddef.h`:

```c
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
```

**中文解释：**
- `container_of()` 通过指针算术从成员指针反推出包含结构体的指针
- 工作原理：成员指针减去成员在结构体中的偏移量 = 结构体起始地址
- `offsetof()` 使用技巧：将 0 强制转换为结构体指针，取成员地址即为偏移量

### 3.3 Strict Aliasing and Safety Assumptions

The `container_of()` macro is safe ONLY under these contracts:

**Contract 1: Pointer Validity**
```c
/* CORRECT: ptr actually points to embedded member */
struct list_head *pos;
list_for_each(pos, &some_list) {
    struct my_object *obj = container_of(pos, struct my_object, list);
    /* Safe: pos is guaranteed to be embedded in my_object */
}

/* WRONG: ptr doesn't point to the right type of embedding */
struct list_head unrelated;
struct my_object *obj = container_of(&unrelated, struct my_object, list);
/* UNDEFINED BEHAVIOR: unrelated is not embedded in my_object */
```

**Contract 2: Type Correctness**
```c
/* All entries in a list MUST be of the same containing type */

LIST_HEAD(my_list);

struct type_a { struct list_head link; };
struct type_b { struct list_head link; };

struct type_a a;
struct type_b b;

list_add(&a.link, &my_list);  /* OK */
list_add(&b.link, &my_list);  /* WRONG! Mixing types */
```

**Contract 3: Member Name Accuracy**
```c
struct my_object {
    struct list_head list_a;
    struct list_head list_b;
};

/* Using wrong member name = WRONG offset = CRASH */
list_for_each_entry(obj, &head_for_list_b, list_a)  /* BUG! */
```

**中文解释：**
- `container_of()` 安全使用的前提条件：
  1. 指针必须真正指向嵌入的成员（不能是独立变量）
  2. 链表中所有元素必须是相同的容器类型
  3. 成员名称必须正确匹配（否则偏移量错误导致崩溃）

### 3.4 Why This is Safe Only Under Specific Contracts

```
+------------------------------------------+
|  SAFETY CONTRACTS FOR container_of()     |
+------------------------------------------+

    +-----------------+     +-----------------+
    |  CALLER MUST    |     |  GUARANTEE      |
    +-----------------+     +-----------------+
    |                 |     |                 |
    | 1. ptr validity |---->| Points to real  |
    |                 |     | embedded member |
    |                 |     |                 |
    | 2. Type matches |---->| All list entries|
    |                 |     | same container  |
    |                 |     |                 |
    | 3. Member name  |---->| Correct offset  |
    |    correct      |     | calculation     |
    +-----------------+     +-----------------+
    
    VIOLATION = undefined behavior (crash, corruption)
```

The type-checking line in `container_of`:
```c
const typeof( ((type *)0)->member ) *__mptr = (ptr);
```
This provides a compile-time check that `ptr` is compatible with `member` type.

**中文解释：**
- `container_of()` 宏中的类型检查行提供编译期验证
- 如果 ptr 的类型与 member 不兼容，编译器会发出警告
- 但这只是类型兼容检查，无法检查 ptr 是否真正嵌入在结构体中

---

## Phase 4 — Ownership & Lifetime Contracts

### 4.1 Core Ownership Rules

```
+------------------------------------------+
|  OWNERSHIP MODEL                         |
+------------------------------------------+

    +-------------------+     +-------------------+
    | ENCLOSING STRUCT  |     |    LIST HEAD      |
    | (e.g. inode)      |     | (e.g. sb->inodes) |
    +-------------------+     +-------------------+
            |                         |
            | OWNS                    | ORGANIZES
            v                         v
    +-------------------+     +-------------------+
    | Embedded list_head|<--->|  Entry linkage    |
    | (i_sb_list)       |     |                   |
    +-------------------+     +-------------------+

    KEY RULES:
    
    1. The ENCLOSING STRUCT owns its embedded list_head
       - list_head has no independent lifetime
       - list_head is freed when enclosing struct is freed
    
    2. The LIST HEAD does not own the entries
       - List head only organizes references
       - Removing from list ≠ freeing the object
    
    3. The list_head is just linkage
       - No reference counting on list_head itself
       - Reference counting is on the enclosing struct
```

### 4.2 What Must Happen Before Operations

**Before Removing from a List:**

```c
/* CORRECT PATTERN */
spin_lock(&list_lock);
list_del_init(&obj->list);      /* 1. Remove from list while locked */
spin_unlock(&list_lock);
/* Object still exists, just not in list */

/* If object was last reference: */
kfree(obj);                      /* 2. Free AFTER removal */

/* WRONG PATTERN */
kfree(obj);                      /* BUG: Free first */
list_del(&obj->list);            /* Accessing freed memory! */
```

**Before Freeing the Object:**

```c
/* INVARIANT: Object must be removed from ALL lists before freeing */

struct inode {
    struct list_head i_wb_list;
    struct list_head i_lru;
    struct list_head i_sb_list;
    struct list_head i_dentry;
};

void destroy_inode(struct inode *inode) {
    /* Must remove from all lists first */
    list_del(&inode->i_wb_list);     /* Remove from writeback list */
    list_del(&inode->i_lru);         /* Remove from LRU */
    list_del(&inode->i_sb_list);     /* Remove from superblock list */
    /* i_dentry handled separately */
    
    /* NOW safe to free */
    kmem_cache_free(inode_cachep, inode);
}
```

**中文解释：**
- 所有权规则：包含结构体拥有其嵌入的 list_head，链表头不拥有列表元素
- 删除顺序：必须先从链表移除，再释放对象（否则访问已释放内存）
- 释放前检查：对象必须从所有链表中移除后才能释放

### 4.3 Invariants That Must Always Hold

```
+------------------------------------------+
|  LIST INVARIANTS                         |
+------------------------------------------+

    INVARIANT 1: Circular integrity
    
        For any node N in the list:
        N->next->prev == N
        N->prev->next == N
        
        Violation = list corruption
    
    INVARIANT 2: Initialization before use
    
        INIT_LIST_HEAD() or LIST_HEAD_INIT() required
        before any operation
        
        struct list_head uninit;
        list_add(&entry, &uninit);  /* BUG: uninit garbage */
    
    INVARIANT 3: Single-list membership (per list_head)
    
        One list_head can only be in ONE list at a time
        (That's why objects embed MULTIPLE list_heads
         to be in multiple lists)
    
    INVARIANT 4: Removal before reuse
    
        list_del(&entry);
        /* entry is now "poisoned" (debug builds) */
        /* or pointing to invalid locations */
        
        list_add(&entry, &new_list); /* BUG without reinit */
        
        Correct:
        list_del_init(&entry);       /* Reinitialize after removal */
        list_add(&entry, &new_list); /* Now safe */
```

### 4.4 Common Lifetime Rules Enforced by Convention

**Rule 1: Reference counting on container, not list_head**

```c
/* From fs/inode.c */
void ihold(struct inode *inode)
{
    WARN_ON(atomic_inc_return(&inode->i_count) < 2);
}

/* Note: i_count is on inode, not on i_sb_list */
```

**Rule 2: Lock protects list AND lifetimes**

```c
/* List operations under lock guarantee object lifetime */
spin_lock(&sb->s_inode_list_lock);
list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
    /* While holding lock, inode cannot be freed */
    /* (delete must also take this lock) */
}
spin_unlock(&sb->s_inode_list_lock);
```

**Rule 3: RCU read-side guarantees during iteration**

```c
/* From kernel/fork.c - task list iteration */
rcu_read_lock();
list_for_each_entry_rcu(p, &init_task.tasks, tasks) {
    /* RCU guarantees p won't be freed during this section */
}
rcu_read_unlock();
```

**中文解释：**
- 约定规则1：引用计数在容器对象上，而非 list_head 上
- 约定规则2：保护链表的锁同时保护对象生命周期（删除也需要获取锁）
- 约定规则3：RCU 读侧保证迭代期间对象不会被释放

---

## Phase 5 — Real Kernel Examples (8 Subsystems)

### Example 1: Task Lists (`task_struct`)

From `include/linux/sched.h`:

```c
struct task_struct {
    /* ... */
    struct list_head tasks;      /* All tasks list */
    struct list_head children;   /* My children */
    struct list_head sibling;    /* Linkage in parent's children */
    struct list_head ptraced;    /* Ptraced children */
    struct list_head thread_group;
    /* ... */
};
```

**Usage in kernel/fork.c:**

```c
/* Adding new task to global task list */
list_add_tail_rcu(&p->tasks, &init_task.tasks);

/* Adding child to parent's children list */
list_add_tail(&p->sibling, &p->real_parent->children);
```

```
+------------------------------------------+
|  TASK RELATIONSHIPS VIA LISTS            |
+------------------------------------------+

    init_task.tasks (global list head)
         |
         v
    +----------+    +----------+    +----------+
    | task A   |--->| task B   |--->| task C   |
    | .tasks   |<---|  .tasks  |<---|  .tasks  |
    +----------+    +----------+    +----------+
    
    parent.children (parent's children list head)
         |
         v
    +----------+    +----------+
    | child 1  |--->| child 2  |
    | .sibling |<---| .sibling |
    +----------+    +----------+
```

**中文解释：**
- `task_struct` 包含多个 list_head：全局任务列表、子进程列表、兄弟链接等
- 一个任务同时在多个链表中：全局任务列表 + 父进程的子进程列表
- 使用 RCU 保护的 `list_add_tail_rcu` 允许无锁读取

### Example 2: VFS Dentries

From `include/linux/dcache.h`:

```c
struct dentry {
    /* ... */
    struct list_head d_lru;      /* LRU list */
    struct list_head d_child;    /* Child of parent */
    struct list_head d_subdirs;  /* Our children */
    struct list_head d_alias;    /* Inode alias list */
};
```

**Manipulation in fs/dcache.c:**

```c
/* Adding dentry to LRU */
static void dentry_lru_add(struct dentry *dentry)
{
    spin_lock(&dcache_lru_lock);
    list_add(&dentry->d_lru, &sb->s_dentry_lru);
    spin_unlock(&dcache_lru_lock);
}
```

```
+------------------------------------------+
|  DENTRY HIERARCHY VIA LISTS              |
+------------------------------------------+

    parent_dentry.d_subdirs (children list head)
         |
         v
    +------------+    +------------+
    | child_a    |--->| child_b    |
    | .d_child   |<---| .d_child   |
    +------------+    +------------+
         |                  |
         | .d_subdirs       | .d_subdirs
         v                  v
    grandchildren      grandchildren
```

**中文解释：**
- dentry 通过 `d_child` 链接到父目录的 `d_subdirs` 列表
- LRU 缓存通过 `d_lru` 实现，回收内存时从 LRU 列表删除最旧项
- 多个 dentry 可以是同一 inode 的别名（硬链接），通过 `d_alias` 链接

### Example 3: Network Devices

From `include/linux/netdevice.h`:

```c
struct net_device {
    /* ... */
    struct list_head    dev_list;     /* Device list */
    struct list_head    napi_list;    /* NAPI list */
    struct list_head    unreg_list;   /* Unregister list */
    struct list_head    todo_list;    /* Todo list */
    struct list_head    link_watch_list;
    /* ... */
};
```

**Usage in net/core/dev.c:**

```c
/* Iterating all network devices */
struct net_device *next_net_device(struct net_device *dev)
{
    struct list_head *lh;
    struct net *net;
    
    net = dev_net(dev);
    lh = rcu_dereference(list_next_rcu(&dev->dev_list));
    return lh == &net->dev_base_head ? NULL : net_device_entry(lh);
}
```

**中文解释：**
- 每个 `net_device` 同时在多个列表中：全局设备列表、NAPI 轮询列表等
- 使用 RCU 保护设备列表遍历，允许无锁读取设备列表
- 设备注销时先加入 todo_list，异步完成清理

### Example 4: TCP Socket Listen Queues

From `include/net/inet_connection_sock.h`:

```c
struct inet_connection_sock {
    struct inet_sock icsk_inet;
    struct request_sock_queue icsk_accept_queue;
    /* ... */
};

/* Request sockets in accept queue */
struct request_sock {
    struct request_sock *dl_next;  /* Different pattern: singly-linked */
    /* ... */
};
```

From `include/net/sock.h`:

```c
struct proto {
    /* ... */
    struct list_head    node;  /* Protocol list */
    /* ... */
};
```

**中文解释：**
- TCP 连接管理中，协议结构通过 list_head 链接到协议列表
- accept 队列使用单链表优化（只需 FIFO 操作）
- 套接字可以在多个列表中：绑定哈希、连接列表等

### Example 5: Workqueue Items

From `include/linux/workqueue.h`:

```c
struct work_struct {
    atomic_long_t data;
    struct list_head entry;     /* Embedded list node */
    work_func_t func;
};

struct delayed_work {
    struct work_struct work;
    struct timer_list timer;
};
```

**Usage:**

```c
/* Work items added to workqueue's pending list */
bool queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
    /* work->entry is added to internal worklist */
}

/* Recovering work_struct from list */
list_for_each_entry_safe(work, n, &cwq->worklist, entry) {
    work_func_t f = work->func;
    list_del_init(&work->entry);  /* Remove before execute */
    f(work);
}
```

**中文解释：**
- `work_struct` 包含嵌入的 `entry` 用于链接到工作队列
- 执行工作项前必须先从列表移除（`list_del_init`）
- 延迟工作包含定时器和工作结构，定时器到期时将工作加入队列

### Example 6: Timer Lists

From `include/linux/timer.h`:

```c
struct timer_list {
    struct list_head entry;      /* Timer wheel linkage */
    unsigned long expires;
    void (*function)(unsigned long);
    unsigned long data;
    /* ... */
};
```

**Checking if timer is pending:**

```c
static inline int timer_pending(const struct timer_list *timer)
{
    return timer->entry.next != NULL;  /* In a list = pending */
}
```

**中文解释：**
- 定时器通过 `entry` 链接到定时器轮（timer wheel）
- 判断定时器是否待处理：检查 entry.next 是否为 NULL
- 添加/删除定时器无需内存分配，直接操作链表

### Example 7: Block Request Queues

From `include/linux/blkdev.h`:

```c
struct request {
    struct list_head queuelist;      /* Queue linkage */
    /* ... */
    struct list_head timeout_list;   /* Timeout tracking */
    /* ... */
};

struct request_queue {
    struct list_head queue_head;     /* Request list */
    struct list_head tag_busy_list;  /* Tagged requests */
    struct list_head timeout_list;   /* Timeout tracking */
    struct list_head flush_queue[2]; /* Flush handling */
    /* ... */
};
```

**中文解释：**
- 块设备请求同时在多个列表中：请求队列、超时跟踪、刷新队列
- 请求处理完成后从队列移除，但可能仍在超时列表中
- 块层使用 `queuelist` 进行请求排序和调度

### Example 8: Device Driver Resource Lists

From `include/linux/device.h`:

```c
struct device {
    /* ... */
    struct list_head    dma_pools;   /* DMA pools */
    struct list_head    devres_head; /* Device resources */
    /* ... */
};
```

**Device resource management (devres):**

```c
struct devres {
    struct list_head entry;
    void (*release)(struct device *dev, void *res);
    /* resource data follows */
};

/* Adding resource to device */
void devres_add(struct device *dev, void *res)
{
    struct devres *dr = container_of(res, struct devres, data);
    spin_lock(&dev->devres_lock);
    list_add_tail(&dr->entry, &dev->devres_head);
    spin_unlock(&dev->devres_lock);
}
```

**中文解释：**
- 设备资源管理使用侵入式链表跟踪分配的资源
- 设备移除时遍历 `devres_head`，自动释放所有关联资源
- 每个 devres 包含释放回调，实现 RAII 模式

---

## Phase 6 — Concurrency & Locking

### 6.1 How List Operations Interact with Spinlocks

```
+------------------------------------------+
|  SPINLOCK-PROTECTED LIST OPERATIONS      |
+------------------------------------------+

    WRITER (Add/Remove):
    
    spin_lock(&list_lock);
    +--------------------------+
    | list_add(&entry, &head); |  <- Atomic under lock
    | or                       |
    | list_del(&entry);        |
    +--------------------------+
    spin_unlock(&list_lock);
    
    READER (Iterate):
    
    spin_lock(&list_lock);
    +----------------------------------+
    | list_for_each_entry(e, &head, m) |
    |     process(e);                  |
    +----------------------------------+
    spin_unlock(&list_lock);
    
    RULE: Same lock protects ALL operations on the list
```

**中文解释：**
- 标准模式：使用自旋锁保护链表的所有操作（读/写/遍历）
- 持锁期间对象生命周期得到保证（删除也需要获取相同的锁）
- 简单但可能成为并发瓶颈

### 6.2 How List Operations Interact with RCU

```
+------------------------------------------+
|  RCU-PROTECTED LIST OPERATIONS           |
+------------------------------------------+

    WRITER (must hold lock + use RCU variants):
    
    spin_lock(&list_lock);
    +--------------------------------+
    | list_add_rcu(&entry, &head);   |  <- Memory barriers
    | or                             |
    | list_del_rcu(&entry);          |
    +--------------------------------+
    spin_unlock(&list_lock);
    
    /* After list_del_rcu: */
    synchronize_rcu();     /* Wait for readers */
    kfree(entry_container);
    
    READER (no lock needed):
    
    rcu_read_lock();
    +-------------------------------------+
    | list_for_each_entry_rcu(e, &head, m)|  <- No lock!
    |     process(e);                      |
    +-------------------------------------+
    rcu_read_unlock();
    
    ADVANTAGE: Readers don't block each other or writers
```

Example from kernel/fork.c:

```c
/* Adding to task list */
write_lock(&tasklist_lock);
list_add_tail_rcu(&p->tasks, &init_task.tasks);
write_unlock(&tasklist_lock);

/* Reading task list */
rcu_read_lock();
list_for_each_entry_rcu(p, &init_task.tasks, tasks) {
    /* Safe to read p, no lock needed */
}
rcu_read_unlock();
```

**中文解释：**
- RCU 允许读者无锁访问链表，极大提高读性能
- 写者仍需持锁，但使用 `_rcu` 后缀的函数（包含内存屏障）
- 删除后必须等待 grace period（`synchronize_rcu()`）才能释放对象

### 6.3 What Happens If Locking is Violated

```
+------------------------------------------+
|  LOCKING VIOLATION CONSEQUENCES          |
+------------------------------------------+

    1. TORN READS (Reader sees partial update)
    
       Writer:         Reader (no lock):
       list_add()      list_for_each()
          |                |
          |  next=new      |  read next (old)
          |  prev=new      |  read prev (new) 
          |                |
          +----------------+--> INCONSISTENT STATE
    
    2. LOST UPDATES (Two writers race)
    
       Writer 1:         Writer 2:
       list_del(A)       list_del(B)
          |                 |
          | A->prev->next = A->next
          |                 | B->prev->next = B->next
          |                 |
          +--> Both may corrupt if A->next == B
    
    3. USE-AFTER-FREE (Freed during iteration)
    
       Reader:               Writer:
       list_for_each(pos)    list_del(pos)
          |                     |
          |  pos = pos->next    |  kfree(container_of(pos))
          |                     |
          +--> CRASH: dereferencing freed memory
```

**中文解释：**
- 锁违规后果1：读者看到部分更新的状态（不一致）
- 锁违规后果2：两个写者竞争，链表结构被破坏
- 锁违规后果3：迭代期间对象被释放，访问已释放内存导致崩溃

### 6.4 Why Lists Themselves are Not Thread-Safe

The `list_head` structure and operations are **intentionally** not thread-safe:

**Design Philosophy:**

```c
/* list_add is NOT atomic */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;    /* 1. Update next->prev */
    new->next = next;    /* 2. Update new->next */
    new->prev = prev;    /* 3. Update new->prev */
    prev->next = new;    /* 4. Update prev->next */
    
    /* 4 separate memory writes - NOT atomic! */
}
```

**Why this is intentional:**

1. **Different contexts need different locks:**
   - Some lists use spinlocks
   - Some use mutexes
   - Some use RCU
   - One-size-fits-all would be wrong

2. **Lock granularity is context-dependent:**
   - Lock per list
   - Lock per subsystem
   - Global lock
   - The list code cannot know

3. **Performance:**
   - Built-in locking adds overhead even when not needed
   - Caller knows when locking is required

**中文解释：**
- 链表操作不是原子的，这是有意为之的设计选择
- 原因1：不同场景需要不同类型的锁（自旋锁/互斥锁/RCU）
- 原因2：锁粒度因场景而异，链表代码无法知道
- 原因3：内置锁定会增加不需要时的开销

---

## Phase 7 — Common Bugs & Failure Modes

### 7.1 Use-After-Free

```c
/* BUG: Freeing while still in list */
struct my_object *obj = find_object();
kfree(obj);                        /* Free first - BUG! */
list_del(&obj->list);              /* Accessing freed memory */

/* CORRECT */
struct my_object *obj = find_object();
spin_lock(&list_lock);
list_del(&obj->list);              /* Remove from list first */
spin_unlock(&list_lock);
kfree(obj);                        /* Now safe to free */
```

**Detection:** DEBUG_LIST poisons deleted entries:

```c
/* From lib/list_debug.c */
void list_del(struct list_head *entry)
{
    __list_del_entry(entry);
    entry->next = LIST_POISON1;    /* 0x00100100 */
    entry->prev = LIST_POISON2;    /* 0x00200200 */
}
```

### 7.2 Double Deletion

```c
/* BUG: Deleting twice */
list_del(&obj->list);
/* ... later ... */
list_del(&obj->list);   /* BUG: Already deleted! */

/* CORRECT: Use list_del_init */
list_del_init(&obj->list);   /* Reinitialize after delete */
/* Now can check: */
if (!list_empty(&obj->list))
    list_del_init(&obj->list);
```

**Detection in lib/list_debug.c:**

```c
void __list_del_entry(struct list_head *entry)
{
    if (WARN(next == LIST_POISON1,
        "list_del corruption, %p->next is LIST_POISON1\n",
        entry))
        return;   /* Catch double-delete */
}
```

### 7.3 Iterating While Modifying

```c
/* BUG: Deleting current element */
list_for_each_entry(obj, &head, list) {
    if (should_delete(obj))
        list_del(&obj->list);   /* BUG: corrupts iterator */
}

/* CORRECT: Use _safe variant */
list_for_each_entry_safe(obj, tmp, &head, list) {
    if (should_delete(obj)) {
        list_del(&obj->list);   /* Safe: tmp holds next */
        kfree(obj);
    }
}
```

### 7.4 Missing Initialization

```c
/* BUG: Using uninitialized list_head */
struct my_object *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
list_add(&obj->list, &head);   /* BUG: obj->list is garbage! */

/* CORRECT */
struct my_object *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
INIT_LIST_HEAD(&obj->list);     /* Initialize first */
list_add(&obj->list, &head);    /* Now safe */
```

### 7.5 Freeing Without Delisting

```c
/* BUG: Freeing without removing from list */
struct my_object *obj = find_object();
kfree(obj);   /* BUG: obj->list still in some list! */

/* Later: list iteration hits freed memory */
list_for_each_entry(o, &head, list) {
    /* CRASH when reaching freed obj's position */
}

/* CORRECT */
struct my_object *obj = find_object();
list_del(&obj->list);   /* Remove from ALL lists first */
kfree(obj);
```

### 7.6 How the Kernel Detects These Bugs

```
+------------------------------------------+
|  KERNEL DEBUG MECHANISMS                 |
+------------------------------------------+

    1. CONFIG_DEBUG_LIST
       - Validates list integrity on every operation
       - Checks prev/next consistency
       - Poisons deleted entries
    
    2. LIST_POISON values
       - 0x00100100, 0x00200200
       - Not NULL (would mask bugs)
       - Not valid address (causes fault on access)
    
    3. WARN_ON / BUG_ON
       - Assertions in critical paths
       - Catches contract violations
    
    4. KASAN (Kernel Address Sanitizer)
       - Detects use-after-free
       - Detects out-of-bounds access
```

**中文解释：**
- 常见错误1：释放前未从链表移除（use-after-free）
- 常见错误2：重复删除（通过 poison 值检测）
- 常见错误3：遍历时修改（使用 `_safe` 变体避免）
- 常见错误4：未初始化（`INIT_LIST_HEAD` 必须先调用）
- 常见错误5：释放时未从所有链表移除
- 内核通过 DEBUG_LIST、POISON 值、KASAN 等机制检测这些错误

---

## Phase 8 — Why This Pattern Works So Well

### 8.1 Advantages

**1. Cache Locality:**
```
+------------------------------------------+
|  CACHE BEHAVIOR                          |
+------------------------------------------+

    Non-intrusive (poor cache):
    
    [container][container]...[container]  <- malloc'd separately
         |          |
         v          v
    [object A] [object B]                  <- also malloc'd
    
    Access pattern: container -> pointer -> object
    Cache misses: 2 per element
    
    Intrusive (excellent cache):
    
    [object A with list_head][object B with list_head]...
    
    Access pattern: direct
    Cache misses: 0-1 per element (data already loaded)
```

**2. Zero Allocation Overhead:**
```c
/* Adding to list = just pointer manipulation */
void add_to_queue(struct work_struct *work, struct list_head *queue)
{
    list_add(&work->entry, queue);
    /* No malloc, no failure possible, O(1) */
}
```

**3. Flexible Membership:**
```c
struct my_object {
    struct list_head queue1;
    struct list_head queue2;
    struct list_head queue3;
};

/* Object can be in 3 lists simultaneously */
list_add(&obj->queue1, &active_queue);
list_add(&obj->queue2, &priority_queue);
list_add(&obj->queue3, &aging_queue);
```

**4. No Hidden Ownership:**
```
+------------------------------------------+
|  OWNERSHIP TRANSPARENCY                  |
+------------------------------------------+

    Non-intrusive container:
    
    Q: Who owns this object?
    A: Unclear - container? object allocator? both?
    
    Intrusive pattern:
    
    Q: Who owns this object?
    A: Whoever allocated it. The list is just linkage.
    
    The list_head is unambiguously part of the object,
    not a separate container entity with its own lifetime.
```

### 8.2 Tradeoffs

**1. Harder Correctness:**
- Must manually track all lists an object is in
- Must remove from all lists before freeing
- No automatic cleanup

**2. Tighter Coupling:**
- Object type must include list_head
- Changes to listing requirements = structure changes
- Compile-time decision, not runtime

**3. Less Abstraction:**
- No "add any type to a list" genericity
- Caller must know member name for `container_of`
- Type-specific iteration

**中文解释：**
- 优势1：缓存局部性极佳，对象数据和链表节点在同一缓存行
- 优势2：零分配开销，添加/删除是纯粹的指针操作
- 优势3：一个对象可同时属于多个链表
- 优势4：所有权清晰，链表只是链接，不拥有对象
- 代价1：正确性更难保证，需手动跟踪所有列表成员关系
- 代价2：耦合更紧，列表需求变化需要修改结构体
- 代价3：抽象较少，无泛型支持

---

## Phase 9 — User-Space Design Transfer

### 9.1 When to Use Intrusive Lists in User-Space

**Good use cases:**

1. **Job Schedulers:**
   - Jobs pre-allocated or pooled
   - Multiple queues (ready, running, waiting)
   - Performance critical

2. **Event Loops:**
   - Timer lists
   - Callback lists
   - No allocation in hot path

3. **Resource Tracking:**
   - Connection pools
   - Memory regions
   - Object registries

4. **Plugin Registries:**
   - Static registration
   - Runtime discovery

### 9.2 Minimal User-Space Implementation

```c
/* intrusive_list.h - Minimal user-space implementation */

#ifndef INTRUSIVE_LIST_H
#define INTRUSIVE_LIST_H

#include <stddef.h>

/*---------------------------------------------------------
 * container_of - Recover enclosing struct from member pointer
 *---------------------------------------------------------*/
#define container_of(ptr, type, member) ({                     \
    const typeof(((type *)0)->member) *__mptr = (ptr);         \
    (type *)((char *)__mptr - offsetof(type, member));         \
})

/*---------------------------------------------------------
 * struct list_head - Embedded list node
 *---------------------------------------------------------*/
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/*---------------------------------------------------------
 * List operations
 *---------------------------------------------------------*/
static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;  /* Poison for safety */
    entry->prev = NULL;
}

static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/*---------------------------------------------------------
 * Iteration macros
 *---------------------------------------------------------*/
#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member), \
         n = list_entry(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif /* INTRUSIVE_LIST_H */
```

### 9.3 Complete User-Space Example: Job Scheduler

```c
/* job_scheduler.c - User-space intrusive list example */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "intrusive_list.h"

/*---------------------------------------------------------
 * Job structure with embedded list nodes
 *---------------------------------------------------------*/
struct job {
    int id;
    int priority;
    char name[32];
    
    /* Embedded list heads for multiple queues */
    struct list_head pending_link;   /* In pending queue */
    struct list_head priority_link;  /* In priority queue */
    
    void (*execute)(struct job *);
};

/*---------------------------------------------------------
 * Queue heads and lock
 *---------------------------------------------------------*/
static LIST_HEAD(pending_queue);      /* Jobs waiting to run */
static LIST_HEAD(priority_queue);     /* High priority jobs */
static LIST_HEAD(free_pool);          /* Recycled job objects */
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

#define JOB_POOL_SIZE 100
static struct job job_pool[JOB_POOL_SIZE];

/*---------------------------------------------------------
 * Initialize job pool
 *---------------------------------------------------------*/
void init_job_pool(void)
{
    for (int i = 0; i < JOB_POOL_SIZE; i++) {
        INIT_LIST_HEAD(&job_pool[i].pending_link);
        INIT_LIST_HEAD(&job_pool[i].priority_link);
        list_add(&job_pool[i].pending_link, &free_pool);
    }
}

/*---------------------------------------------------------
 * Allocate job from pool (no malloc!)
 *---------------------------------------------------------*/
struct job *alloc_job(void)
{
    struct job *job = NULL;
    
    pthread_mutex_lock(&queue_lock);
    if (!list_empty(&free_pool)) {
        /* Get job from pool */
        struct list_head *first = free_pool.next;
        list_del_init(first);  /* Remove from free pool */
        job = container_of(first, struct job, pending_link);
        
        /* Reinitialize for reuse */
        INIT_LIST_HEAD(&job->priority_link);
    }
    pthread_mutex_unlock(&queue_lock);
    
    return job;
}

/*---------------------------------------------------------
 * Return job to pool
 *---------------------------------------------------------*/
void free_job(struct job *job)
{
    pthread_mutex_lock(&queue_lock);
    
    /* Must not be in any queue! */
    if (!list_empty(&job->pending_link)) {
        fprintf(stderr, "BUG: freeing job still in pending queue!\n");
    }
    if (!list_empty(&job->priority_link)) {
        fprintf(stderr, "BUG: freeing job still in priority queue!\n");
    }
    
    /* Return to free pool */
    list_add(&job->pending_link, &free_pool);
    
    pthread_mutex_unlock(&queue_lock);
}

/*---------------------------------------------------------
 * Submit job to appropriate queue(s)
 *---------------------------------------------------------*/
void submit_job(struct job *job)
{
    pthread_mutex_lock(&queue_lock);
    
    /* Add to pending queue */
    list_add_tail(&job->pending_link, &pending_queue);
    
    /* High priority jobs also go to priority queue */
    if (job->priority > 5) {
        list_add(&job->priority_link, &priority_queue);
    }
    
    pthread_mutex_unlock(&queue_lock);
}

/*---------------------------------------------------------
 * Get next job to run (priority first, then pending)
 *---------------------------------------------------------*/
struct job *get_next_job(void)
{
    struct job *job = NULL;
    
    pthread_mutex_lock(&queue_lock);
    
    /* Check priority queue first */
    if (!list_empty(&priority_queue)) {
        struct list_head *first = priority_queue.next;
        job = container_of(first, struct job, priority_link);
        
        /* Remove from both queues */
        list_del_init(&job->priority_link);
        list_del_init(&job->pending_link);
    }
    /* Fall back to pending queue */
    else if (!list_empty(&pending_queue)) {
        struct list_head *first = pending_queue.next;
        job = container_of(first, struct job, pending_link);
        
        /* Remove from pending queue */
        list_del_init(&job->pending_link);
        /* priority_link should already be empty */
    }
    
    pthread_mutex_unlock(&queue_lock);
    
    return job;
}

/*---------------------------------------------------------
 * Cancel all jobs with given priority
 *---------------------------------------------------------*/
void cancel_jobs_by_priority(int priority)
{
    struct job *job, *tmp;
    
    pthread_mutex_lock(&queue_lock);
    
    /* Safe iteration - we're removing while iterating */
    list_for_each_entry_safe(job, tmp, &pending_queue, pending_link) {
        if (job->priority == priority) {
            printf("Canceling job %d: %s\n", job->id, job->name);
            
            /* Remove from both queues */
            list_del_init(&job->pending_link);
            if (!list_empty(&job->priority_link)) {
                list_del_init(&job->priority_link);
            }
            
            /* Return to pool */
            list_add(&job->pending_link, &free_pool);
        }
    }
    
    pthread_mutex_unlock(&queue_lock);
}

/*---------------------------------------------------------
 * Example job function
 *---------------------------------------------------------*/
void example_job_func(struct job *job)
{
    printf("Executing job %d: %s (priority %d)\n", 
           job->id, job->name, job->priority);
}

/*---------------------------------------------------------
 * Main - demonstrate the pattern
 *---------------------------------------------------------*/
int main(void)
{
    /* Initialize pool */
    init_job_pool();
    
    /* Submit some jobs */
    for (int i = 0; i < 10; i++) {
        struct job *job = alloc_job();
        if (!job) {
            fprintf(stderr, "Pool exhausted!\n");
            break;
        }
        
        job->id = i;
        job->priority = i % 10;
        snprintf(job->name, sizeof(job->name), "Job_%d", i);
        job->execute = example_job_func;
        
        submit_job(job);
        printf("Submitted: %s (priority %d)\n", job->name, job->priority);
    }
    
    printf("\n--- Processing jobs (priority first) ---\n");
    
    /* Process all jobs */
    struct job *job;
    while ((job = get_next_job()) != NULL) {
        job->execute(job);
        free_job(job);
    }
    
    printf("\n--- All jobs processed ---\n");
    
    return 0;
}
```

**Compile and run:**
```bash
gcc -o job_scheduler job_scheduler.c -lpthread
./job_scheduler
```

**Expected output:**
```
Submitted: Job_0 (priority 0)
Submitted: Job_1 (priority 1)
...
--- Processing jobs (priority first) ---
Executing job 9: Job_9 (priority 9)
Executing job 8: Job_8 (priority 8)
Executing job 7: Job_7 (priority 7)
Executing job 6: Job_6 (priority 6)
Executing job 5: Job_5 (priority 5)
Executing job 4: Job_4 (priority 4)
...
--- All jobs processed ---
```

**中文解释：**
- 用户态实现完全复制内核的侵入式链表模式
- 作业调度器示例展示：对象池、多队列成员、安全遍历、生命周期管理
- 关键点：`alloc_job()` 从池获取，`free_job()` 归还到池，无 malloc/free 调用

---

## Phase 10 — Design Rules & Checklist

### 10.1 When to Use Embedded List Nodes

Use intrusive lists when:

| Criteria | Use Intrusive Lists |
|----------|---------------------|
| Allocation in hot path? | Must avoid |
| Object in multiple lists? | Yes |
| Cache performance critical? | Yes |
| Fixed object type? | Yes |
| Lifetime well-defined? | Yes |
| Lock-free reads needed? | RCU variant available |

### 10.2 When NOT to Use Embedded List Nodes

Avoid intrusive lists when:

| Criteria | Avoid Intrusive Lists |
|----------|----------------------|
| Heterogeneous types in one list? | Yes (use void* or union) |
| Object type can't be modified? | Yes (use wrapper) |
| List membership changes dynamically? | Maybe (multiple list_heads) |
| Language with generics/templates? | Consider type-safe containers |
| Correctness > Performance? | Consider simpler alternatives |

### 10.3 Safe Usage Checklist

```
+------------------------------------------+
|  INTRUSIVE LIST SAFETY CHECKLIST         |
+------------------------------------------+

    INITIALIZATION:
    [ ] All list_head members initialized (INIT_LIST_HEAD)
    [ ] List head initialized (LIST_HEAD or INIT_LIST_HEAD)
    
    OPERATIONS:
    [ ] Lock held for all add/remove/iterate operations
    [ ] Using _safe variant when removing during iteration
    [ ] Using _rcu variant when RCU protection needed
    
    LIFETIME:
    [ ] Object removed from ALL lists before freeing
    [ ] Reference counting on container, not list_head
    [ ] Lock held (or RCU grace period) during iteration
    
    TYPING:
    [ ] All entries in list are same container type
    [ ] container_of uses correct member name
    [ ] Not mixing different list_heads in same list
    
    DEBUGGING:
    [ ] CONFIG_DEBUG_LIST enabled in development
    [ ] Assertions for list state (empty check before use)
    [ ] Poison values for freed entries
```

### 10.4 Architectural Lessons from the Kernel

**Lesson 1: Embed, Don't Allocate**
- Allocation introduces failure modes
- Embedding eliminates indirection
- Lifetime becomes obvious

**Lesson 2: Separate Linkage from Ownership**
- Lists organize, they don't own
- Reference counting on the object
- Clear responsibility boundaries

**Lesson 3: Type Safety via Convention**
- `container_of` recovers type
- All entries must be same type
- Document the member name

**Lesson 4: Lock the List, Not the Entries**
- Single lock protects list structure
- Same lock guards lifetime
- RCU for read-heavy access

**Lesson 5: Initialize Before Use, Remove Before Free**
- No implicit initialization
- No implicit cleanup
- Explicit is better than implicit

**中文解释：**
- 使用场景：热路径零分配、多队列成员、性能关键
- 避免场景：异构类型、无法修改对象类型、正确性优先于性能
- 安全检查清单：初始化、操作时持锁、删除前从所有列表移除、类型一致性
- 架构教训：嵌入而非分配、链接与所有权分离、通过约定保证类型安全、锁保护列表而非元素、显式初始化和清理

---

## Summary

The intrusive list pattern is a fundamental building block of the Linux kernel, enabling:

1. **Zero-allocation data structure operations**
2. **Multiple list membership for single objects**
3. **Excellent cache locality**
4. **Clear ownership semantics**

The key contracts are:

- **Initialize before use** (`INIT_LIST_HEAD`)
- **Remove before free** (`list_del` before `kfree`)
- **Lock during operations** (spinlock, mutex, or RCU)
- **Type homogeneity** (all entries same type)
- **Correct member names** (for `container_of`)

This pattern transfers directly to user-space systems where:
- Performance is critical
- Allocation in hot paths must be avoided
- Objects participate in multiple collections

**中文总结：**
侵入式链表是 Linux 内核的基础构建模块，其核心价值在于：

1. 零分配开销的数据结构操作
2. 单个对象可同时属于多个链表
3. 优秀的缓存局部性
4. 清晰的所有权语义

关键契约包括：使用前初始化、释放前移除、操作时加锁、类型同质性、成员名正确。

此模式可直接应用于用户态系统，特别是需要高性能、避免热路径分配、对象参与多个集合的场景。

