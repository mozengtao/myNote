# Linux Kernel IDR/Radix Tree Design (v3.2)

## Overview

This document explains **IDR (ID Radix) and radix tree design** in Linux kernel v3.2, focusing on ID allocation, handle-based access, and registry patterns.

---

## Why Pointer Exposure is Avoided

```
+------------------------------------------------------------------+
|  THE PROBLEM WITH RAW POINTERS                                   |
+------------------------------------------------------------------+

    EXPOSING POINTERS TO USERSPACE:
    +----------------------------------------------------------+
    | fd = open("/dev/foo");  /* What does fd contain? */       |
    |                                                           |
    | WRONG: fd = (int)(void *)file;  /* Pointer as int! */     |
    |                                                           |
    | Problems:                                                 |
    | 1. Security: Leaks kernel addresses (KASLR bypass)        |
    | 2. Safety: User can forge arbitrary pointers              |
    | 3. ABI: Pointer size changes between 32/64-bit            |
    | 4. Lifetime: Pointer may become invalid                   |
    +----------------------------------------------------------+

    SOLUTION: HANDLES/IDs
    
    ┌─────────────────────────────────────────────────────────────┐
    │  User Space                        Kernel Space             │
    │                                                              │
    │  fd = 3 ─────────────────────────▶ fd_table[3] ──▶ *file   │
    │  (opaque integer)                  (lookup)      (object)   │
    │                                                              │
    │  Benefits:                                                   │
    │  - No pointer leak                                          │
    │  - Validated on every access                                │
    │  - Kernel controls lifetime                                 │
    │  - Stable ABI                                               │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 暴露指针问题：泄漏内核地址（KASLR绕过）、用户可伪造指针、ABI不稳定、生命周期问题
- 解决方案：句柄/ID — 不透明整数，每次访问验证，内核控制生命周期

---

## IDR Design

From `include/linux/idr.h`:

```c
struct idr {
    struct idr_layer *top;
    struct idr_layer *id_free;
    int layers;
    int id_free_cnt;
    spinlock_t lock;
};
```

```
+------------------------------------------------------------------+
|  IDR ARCHITECTURE                                                |
+------------------------------------------------------------------+

    IDR = Radix tree optimized for small integer ID allocation
    
    ┌─────────────────────────────────────────────────────────────┐
    │                      IDR Structure                           │
    │                                                              │
    │  ID = 42 ──▶ ?                                              │
    │                                                              │
    │        Layer 0 (top)                                        │
    │        ┌────┬────┬────┬────┬────┐                           │
    │        │ 0  │ 1  │ 2  │ ...│ 63 │  ← 6 bits per level       │
    │        └────┴────┴──┬─┴────┴────┘                           │
    │                     │                                        │
    │                     ▼ slot[1]                                │
    │        Layer 1                                               │
    │        ┌────┬────┬────┬────┬────┐                           │
    │        │ 0  │ ...│ 42 │ ...│ 63 │                           │
    │        └────┴────┴──┬─┴────┴────┘                           │
    │                     │                                        │
    │                     ▼                                        │
    │                 ┌────────┐                                   │
    │                 │ Object │  ← Pointer to actual object      │
    │                 │ ptr    │                                   │
    │                 └────────┘                                   │
    └─────────────────────────────────────────────────────────────┘

    ID ALLOCATION:
    +----------------------------------------------------------+
    | idr_alloc() - Allocates unused ID, stores pointer         |
    | idr_find()  - Returns pointer for given ID                |
    | idr_remove()- Removes ID, returns pointer                 |
    +----------------------------------------------------------+
```

**中文解释：**
- IDR：为小整数 ID 分配优化的基数树
- 每层 6 位（64 槽），多层支持大 ID
- 操作：idr_alloc（分配ID）、idr_find（查找）、idr_remove（删除）

---

## ID Allocation and Lookup

```c
/* Allocate new ID */
int id;
struct my_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

idr_preload(GFP_KERNEL);          /* Pre-allocate tree nodes */
spin_lock(&my_lock);
id = idr_alloc(&my_idr, obj, 0, 0, GFP_NOWAIT);  /* 0 = auto ID */
spin_unlock(&my_lock);
idr_preload_end();

if (id < 0)
    return id;  /* Error */

/* Lookup by ID */
spin_lock(&my_lock);
obj = idr_find(&my_idr, id);
spin_unlock(&my_lock);

if (!obj)
    return -ENOENT;

/* Remove by ID */
spin_lock(&my_lock);
obj = idr_remove(&my_idr, id);
spin_unlock(&my_lock);
/* Now free obj */
```

```
+------------------------------------------------------------------+
|  IDR OPERATIONS TIMELINE                                         |
+------------------------------------------------------------------+

    ALLOCATION:
    
    idr_preload(GFP_KERNEL)     Allocate tree nodes (may sleep)
           │
           ▼
    spin_lock(&lock)           Acquire lock
           │
           ▼
    id = idr_alloc(...)        Get ID (fast, atomic)
           │                    Returns: 0, 1, 2, 3, ...
           ▼
    spin_unlock(&lock)         Release lock
           │
           ▼
    idr_preload_end()          Done with preload

    LOOKUP:
    
    spin_lock(&lock)
           │
           ▼
    obj = idr_find(&idr, id)   O(log N) lookup
           │
           ▼
    spin_unlock(&lock)
           │
           ▼
    use(obj)                   Use the object
```

**中文解释：**
- idr_preload：预分配树节点（可睡眠）
- idr_alloc：在锁内分配 ID（快速，原子）
- idr_find：O(log N) 查找
- 模式：预分配 → 加锁 → 分配/查找 → 解锁 → 结束预分配

---

## Lifetime Coupling

```
+------------------------------------------------------------------+
|  ID LIFECYCLE AND OBJECT LIFETIME                                |
+------------------------------------------------------------------+

    RULE: ID lifetime ≤ Object lifetime
    
    CORRECT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Object created                                              │
    │       │                                                      │
    │       ▼                                                      │
    │  id = idr_alloc(&idr, obj)  ← ID assigned                   │
    │       │                                                      │
    │       │  Object valid, ID valid                              │
    │       │                                                      │
    │       ▼                                                      │
    │  idr_remove(&idr, id)       ← ID removed                    │
    │       │                                                      │
    │       │  Object valid, ID invalid                            │
    │       │                                                      │
    │       ▼                                                      │
    │  kfree(obj)                 ← Object freed                  │
    └─────────────────────────────────────────────────────────────┘

    WRONG (dangling ID):
    ┌─────────────────────────────────────────────────────────────┐
    │  id = idr_alloc(&idr, obj)                                  │
    │       │                                                      │
    │       ▼                                                      │
    │  kfree(obj)                 ← Object freed first!           │
    │       │                                                      │
    │       │  ID still valid, but points to freed memory!        │
    │       │                                                      │
    │       ▼                                                      │
    │  idr_find(&idr, id)         ← Returns dangling pointer!     │
    │                             ← USE-AFTER-FREE BUG            │
    └─────────────────────────────────────────────────────────────┘

    INVARIANT:
    +----------------------------------------------------------+
    | Always remove ID before freeing object                    |
    | Or: Use reference counting with idr_find                  |
    +----------------------------------------------------------+
```

**中文解释：**
- 规则：ID 生命周期 ≤ 对象生命周期
- 正确顺序：创建对象 → 分配ID → 移除ID → 释放对象
- 错误：先释放对象，ID 指向已释放内存（use-after-free）
- 不变量：总是先移除 ID 再释放对象

---

## Kernel Usage Examples

```
+------------------------------------------------------------------+
|  KERNEL IDR USAGE EXAMPLES                                       |
+------------------------------------------------------------------+

    1. FILE DESCRIPTORS
    +----------------------------------------------------------+
    | Purpose: Map fd (0, 1, 2, ...) to struct file             |
    | Location: fs/file.c                                       |
    |                                                           |
    | fd = alloc_fd();  /* Get unused fd number */              |
    | fd_install(fd, file);  /* Map fd → file */                |
    +----------------------------------------------------------+
    
    2. PROCESS IDs (PIDs)
    +----------------------------------------------------------+
    | Purpose: Map pid to task_struct                           |
    | Location: kernel/pid.c                                    |
    |                                                           |
    | struct pid uses IDR-like allocation                       |
    | find_task_by_vpid() looks up by ID                        |
    +----------------------------------------------------------+
    
    3. IPC IDs
    +----------------------------------------------------------+
    | Purpose: shmid, semid, msgid → IPC objects                |
    | Location: ipc/util.c                                      |
    |                                                           |
    | int id = ipc_addid(&shm_ids, &shp->shm_perm, ...);        |
    +----------------------------------------------------------+
    
    4. DRM (Graphics) Handles
    +----------------------------------------------------------+
    | Purpose: GEM object handles for userspace                 |
    | Location: drivers/gpu/drm/                                |
    |                                                           |
    | handle = idr_alloc(&file_priv->object_idr, obj, ...);     |
    | obj = idr_find(&file_priv->object_idr, handle);           |
    +----------------------------------------------------------+
```

**中文解释：**
- 文件描述符：fd → struct file
- 进程 ID：pid → task_struct
- IPC ID：shmid/semid → IPC 对象
- DRM 句柄：GEM 对象句柄 → GPU 对象

---

## User-Space Resource Handle

```c
/* User-space handle registry inspired by kernel IDR */

#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_HANDLES 4096

struct handle_registry {
    void *objects[MAX_HANDLES];
    uint32_t bitmap[MAX_HANDLES / 32];
    pthread_mutex_t lock;
    int next_hint;
};

/* Initialize registry */
void registry_init(struct handle_registry *reg)
{
    memset(reg, 0, sizeof(*reg));
    pthread_mutex_init(&reg->lock, NULL);
}

/* Find free slot */
static int find_free_slot(struct handle_registry *reg)
{
    for (int i = reg->next_hint; i < MAX_HANDLES; i++) {
        int word = i / 32;
        int bit = i % 32;
        if (!(reg->bitmap[word] & (1u << bit))) {
            return i;
        }
    }
    /* Wrap around */
    for (int i = 0; i < reg->next_hint; i++) {
        int word = i / 32;
        int bit = i % 32;
        if (!(reg->bitmap[word] & (1u << bit))) {
            return i;
        }
    }
    return -1;  /* Full */
}

/* Allocate handle for object */
int registry_alloc(struct handle_registry *reg, void *obj)
{
    pthread_mutex_lock(&reg->lock);
    
    int slot = find_free_slot(reg);
    if (slot < 0) {
        pthread_mutex_unlock(&reg->lock);
        return -1;
    }
    
    reg->objects[slot] = obj;
    reg->bitmap[slot / 32] |= (1u << (slot % 32));
    reg->next_hint = slot + 1;
    
    pthread_mutex_unlock(&reg->lock);
    return slot;  /* This is the handle */
}

/* Lookup object by handle */
void *registry_find(struct handle_registry *reg, int handle)
{
    if (handle < 0 || handle >= MAX_HANDLES)
        return NULL;
    
    pthread_mutex_lock(&reg->lock);
    
    void *obj = NULL;
    int word = handle / 32;
    int bit = handle % 32;
    
    if (reg->bitmap[word] & (1u << bit)) {
        obj = reg->objects[handle];
    }
    
    pthread_mutex_unlock(&reg->lock);
    return obj;
}

/* Remove handle, return object */
void *registry_remove(struct handle_registry *reg, int handle)
{
    if (handle < 0 || handle >= MAX_HANDLES)
        return NULL;
    
    pthread_mutex_lock(&reg->lock);
    
    void *obj = NULL;
    int word = handle / 32;
    int bit = handle % 32;
    
    if (reg->bitmap[word] & (1u << bit)) {
        obj = reg->objects[handle];
        reg->objects[handle] = NULL;
        reg->bitmap[word] &= ~(1u << bit);
    }
    
    pthread_mutex_unlock(&reg->lock);
    return obj;
}

/* Example usage */
struct my_resource {
    int data;
    /* ... */
};

int create_resource(struct handle_registry *reg)
{
    struct my_resource *res = malloc(sizeof(*res));
    if (!res) return -1;
    
    res->data = 42;
    
    int handle = registry_alloc(reg, res);
    if (handle < 0) {
        free(res);
        return -1;
    }
    
    return handle;  /* Return opaque handle to caller */
}

int use_resource(struct handle_registry *reg, int handle)
{
    struct my_resource *res = registry_find(reg, handle);
    if (!res) return -1;
    
    /* Use resource */
    return res->data;
}

void destroy_resource(struct handle_registry *reg, int handle)
{
    struct my_resource *res = registry_remove(reg, handle);
    if (res) {
        free(res);
    }
}
```

**中文解释：**
- 用户态句柄注册表：模拟内核 IDR
- 分配：找到空闲槽，存储对象指针，返回句柄
- 查找：验证句柄有效性，返回对象指针
- 移除：清除句柄，返回对象指针供调用者释放
- 不暴露指针给外部，只暴露整数句柄

