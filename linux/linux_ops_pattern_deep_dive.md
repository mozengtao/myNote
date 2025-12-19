# The `xxx->ops->yyy(xxx, ...)` Pattern in Linux Kernel

A deep architectural analysis of the operations table pattern in Linux kernel v3.2.

---

## Table of Contents

1. [Define the Pattern Precisely](#step-1--define-the-pattern-precisely)
2. [Why the Kernel Uses This Pattern](#step-2--why-the-kernel-uses-this-pattern)
3. [The `ops` Structure as a Contract](#step-3--the-ops-structure-as-a-contract)
4. [Private Data Handling](#step-4--private-data-handling-core-concept)
5. [VFS Subsystem Examples](#step-5--vfs-subsystem-examples)
6. [TCP/IP Network Subsystem Examples](#step-6--tcpip-network-subsystem-examples)
7. [Other Subsystem Examples](#step-65--other-subsystem-examples) — MM, TTY, Scheduler, Block
8. [container_of and Type Recovery](#step-7--container_of-and-type-recovery)
9. [Control Flow and Dependency Direction](#step-8--control-flow-and-dependency-direction)
10. [Common Misunderstandings & Pitfalls](#step-9--common-misunderstandings--pitfalls)
11. [Architecture Lessons](#step-10--architecture-lessons-i-should-internalize)

---

## Step 1 — Define the Pattern Precisely

### The Pattern Structure

```
    xxx->ops->yyy(xxx, ...)
    │    │    │   │
    │    │    │   └── The framework object passed BACK to implementation
    │    │    │
    │    │    └── Function pointer: the specific operation
    │    │
    │    └── Operations table: struct of function pointers
    │
    └── Framework object: generic handle known to framework
```

**说明:**
- `xxx`: 框架对象，是框架层定义和管理的结构体实例
- `ops`: 操作表，是一个包含函数指针的结构体
- `yyy`: 具体操作函数，由实现层提供
- 关键点：`xxx` 作为参数再次传回给操作函数

### What Each Component Represents

| Component | Role | Owned By | Example |
|-----------|------|----------|---------|
| `xxx` | Framework-defined object | Framework layer | `struct file`, `struct socket` |
| `ops` | Contract definition | Framework layer | `struct file_operations` |
| `yyy` | Operation implementation | Implementation layer | `ext4_file_read()` |
| `xxx` (as argument) | Context for implementation | Framework (passed to impl) | Enables private data recovery |

### Why This is NOT Just "Object-Oriented C"

```
TRADITIONAL OOP:                      KERNEL OPS PATTERN:

+----------------+                    +----------------+
|  Object        |                    |  Framework Obj |
|  (self-owned)  |                    |  (framework-   |
|                |                    |   owned)       |
|  +----------+  |                    |  +----------+  |
|  | data     |  |                    |  | generic  |  |
|  +----------+  |                    |  | fields   |  |
|  | methods  |--|-- vtable           |  +----------+  |
|  +----------+  |                    |  | ops -------|---> [Contract]
+----------------+                    +----------------+
                                              |
                                              | CRITICAL DIFFERENCE:
                                              | Implementation stores
                                              | private data OUTSIDE
                                              | the framework object
                                              v
                                      +----------------+
                                      | Implementation |
                                      | Private Data   |
                                      | (separate!)    |
                                      +----------------+
```

**说明:**
- 传统 OOP: 对象拥有自己的方法和数据
- 内核模式: 框架对象只包含通用字段，实现数据存储在别处
- 这不是 OOP 的 vtable，而是控制反转的架构边界

**Key Differences from OOP:**

| Aspect | OOP | Kernel Pattern |
|--------|-----|----------------|
| Data ownership | Object owns its data | Framework owns object, impl owns private data |
| Type knowledge | Object knows its full type | Framework knows only generic type |
| Lifecycle | Object manages itself | Framework manages lifecycle |
| Binding | Compile-time or runtime | Always runtime, loadable |
| Purpose | Code reuse | Architectural separation |

### The Boundary This Pattern Creates

```
+=================================================================+
|                      FRAMEWORK LAYER                             |
|                                                                  |
|   - Defines struct xxx (generic object)                          |
|   - Defines struct xxx_ops (contract)                            |
|   - Manages object lifecycle                                     |
|   - Knows NOTHING about implementation details                   |
|                                                                  |
|   Example: VFS defines struct file, struct file_operations       |
|            VFS manages file opens/closes                         |
|            VFS knows nothing about ext4                          |
|                                                                  |
+=================================================================+
                           │
                           │  ops->yyy(xxx, ...)
                           │
                           ▼
+=================================================================+
|                    IMPLEMENTATION LAYER                          |
|                                                                  |
|   - Implements operations defined in contract                    |
|   - Stores private data somewhere accessible                     |
|   - Recovers private data from xxx parameter                     |
|   - Has full knowledge of its own internals                      |
|                                                                  |
|   Example: ext4 implements file_operations                       |
|            ext4 stores private data in ext4_inode_info           |
|            ext4 recovers via container_of                        |
|                                                                  |
+=================================================================+
```

**说明:**
- 框架层: 定义对象和契约，管理生命周期，不了解实现
- 实现层: 实现操作，存储私有数据，需要了解框架契约

---

## Step 2 — Why the Kernel Uses This Pattern

### The Problem: Multiple Implementations, One Interface

```
KERNEL SUBSYSTEM REALITY:

                    VFS (One Interface)
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
      ┌──────┐         ┌──────┐         ┌──────┐
      │ ext4 │         │ xfs  │         │ nfs  │
      └──────┘         └──────┘         └──────┘
         │                 │                 │
    Local disk        Local disk        Network
    journaling        log-structured    remote server

    (30+ filesystems in kernel, each with different internals)
```

**说明:**
- VFS 需要支持 30+ 种文件系统
- 每种文件系统有完全不同的内部实现
- VFS 不能知道任何具体文件系统的细节

### Why Direct Function Calls Fail

```c
/* WRONG APPROACH: Direct function calls */

ssize_t vfs_read(struct file *file, char *buf, size_t count)
{
    /* This couples VFS to every filesystem! */
    if (file->type == FS_EXT4) {
        return ext4_read(file, buf, count);
    } else if (file->type == FS_XFS) {
        return xfs_read(file, buf, count);
    } else if (file->type == FS_NFS) {
        return nfs_read(file, buf, count);
    }
    /* Must modify VFS for every new filesystem! */
}
```

**Problems with this approach:**

| Problem | Impact |
|---------|--------|
| VFS must know all filesystems | Adding filesystem requires VFS change |
| VFS must link against all filesystems | Cannot load filesystems dynamically |
| Maintenance nightmare | Every fs change may affect VFS |
| Testing explosion | Must test all combinations |
| Circular dependencies | VFS → fs → VFS |

### Why Virtual Dispatch is Required

```c
/* CORRECT APPROACH: Operations table */

ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    /* VFS knows nothing about ext4, xfs, or nfs */
    /* It only knows the CONTRACT defined by file_operations */
    
    if (file->f_op->read)
        return file->f_op->read(file, buf, count, pos);
    else
        return do_sync_read(file, buf, count, pos);
}
```

**说明:**
- VFS 完全不知道 ext4, xfs, nfs 的存在
- VFS 只知道 `file_operations` 契约
- 新文件系统无需修改 VFS

### What This Enables

```
EXTENSIBILITY WITHOUT MODIFICATION:

Before:                             After adding new filesystem:

+--------+                          +--------+
|  VFS   |                          |  VFS   |  <- NO CHANGE
+---+----+                          +---+----+
    |                                   |
+---+----+----+----+               +---+----+----+----+----+
|ext4|xfs |nfs |... |             |ext4|xfs |nfs |... |btrfs|
+----+----+----+----+             +----+----+----+----+-----+
                                                         ^
                                                    Just implements
                                                    file_operations
```

### Dynamic Behavior (Hotplug/Modules)

```c
/* Filesystem can be loaded at runtime */
$ insmod ext4.ko

/* Module initialization registers with VFS */
static int __init ext4_init(void)
{
    /* Tell VFS: "I implement file_operations like this" */
    return register_filesystem(&ext4_fs_type);
}

/* VFS never linked against ext4 at compile time */
/* VFS discovers ext4's operations at runtime */
```

### Eliminating `if (type == ...)` Logic

| Approach | Scalability | Maintenance | Performance |
|----------|-------------|-------------|-------------|
| `if (type == A) ... else if (type == B) ...` | O(n) additions | Every type requires change | O(n) per call |
| Operations table | O(1) additions | Self-contained | O(1) per call |

---

## Step 3 — The `ops` Structure as a Contract

### Contract Definition

```c
/* include/linux/fs.h:1583-1611 */
/* This is a CONTRACT, not just a struct */

struct file_operations {
    struct module *owner;     /* Which module provides this? */
    
    /* === CORE CONTRACT OPERATIONS === */
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    
    /* === EXTENDED OPERATIONS === */
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    /* ... more operations ... */
};
```

### Guarantees the Caller (Framework) Assumes

| Guarantee | Description |
|-----------|-------------|
| Signature compliance | Function matches declared signature exactly |
| Semantic compliance | Function does what the name implies |
| Return value semantics | Returns follow kernel conventions (0 = success, negative = errno) |
| Locking compliance | Respects documented locking requirements |
| Memory compliance | Handles user pointers correctly (`copy_to_user`, etc.) |

```c
/* VFS assumes: if f_op->read exists, calling it with valid 
 * file/buf/count/pos will read data or return error properly */

ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    /* VFS ASSUMES: */
    /* 1. file is valid (VFS validated) */
    /* 2. If f_op->read exists, it follows the contract */
    /* 3. Return value follows kernel conventions */
    
    if (file->f_op->read)
        ret = file->f_op->read(file, buf, count, pos);
}
```

### Guarantees the Implementer Must Uphold

```c
/* ext4's implementation of the read contract */

static ssize_t ext4_file_read(struct file *file, char __user *buf,
                               size_t count, loff_t *pos)
{
    /* IMPLEMENTER MUST: */
    
    /* 1. Handle user pointer correctly */
    if (copy_to_user(buf, kernel_data, count))
        return -EFAULT;  /* Correct error return */
    
    /* 2. Update position if successful */
    *pos += bytes_read;
    
    /* 3. Return bytes read, or negative errno */
    return bytes_read;
    
    /* 4. Not corrupt framework state */
    /* 5. Respect locking documented in Documentation/filesystems/Locking */
}
```

### Handling NULL Operations

```c
/* Pattern 1: Check before call (common) */
if (file->f_op->read)
    ret = file->f_op->read(file, buf, count, pos);
else
    ret = do_sync_read(file, buf, count, pos);  /* fallback */

/* Pattern 2: Use default if NULL */
if (inode->i_op->permission)
    ret = inode->i_op->permission(inode, mask);
else
    ret = generic_permission(inode, mask);  /* default */

/* Pattern 3: Optional operation, skip if NULL */
if (dentry->d_op && dentry->d_op->d_release)
    dentry->d_op->d_release(dentry);
/* No fallback - simply don't call */
```

### Optional vs Mandatory Operations

```c
/* From include/net/tcp.h:689-717 */
struct tcp_congestion_ops {
    /* === REQUIRED OPERATIONS (marked in comments) === */
    u32 (*ssthresh)(struct sock *sk);           /* required */
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);  /* required */
    
    /* === OPTIONAL OPERATIONS === */
    void (*init)(struct sock *sk);              /* optional */
    void (*release)(struct sock *sk);           /* optional */
    u32 (*min_cwnd)(const struct sock *sk);     /* optional */
    void (*set_state)(struct sock *sk, u8 new_state);  /* optional */
    void (*pkts_acked)(struct sock *sk, u32 num_acked, s32 rtt_us);  /* optional */
    
    char name[TCP_CA_NAME_MAX];
    struct module *owner;
};

/* Caller code shows the pattern: */
static inline void tcp_ca_event(struct sock *sk, const enum tcp_ca_event event)
{
    const struct inet_connection_sock *icsk = inet_csk(sk);
    
    /* Optional: check before call */
    if (icsk->icsk_ca_ops->cwnd_event)
        icsk->icsk_ca_ops->cwnd_event(sk, event);
}
```

---

## Step 4 — Private Data Handling (Core Concept)

This section explains the three strategies for handling private data in the ops pattern, with detailed kernel examples, line-by-line explanations, and complete userspace C simulations.

### The Fundamental Challenge

```
+------------------------------------------------------------------+
|                    THE PRIVATE DATA PROBLEM                       |
+------------------------------------------------------------------+

FRAMEWORK LAYER (VFS):                IMPLEMENTATION LAYER (ext4):

  "I manage generic objects"            "I need ext4-specific data"

  struct file {                         struct ext4_file_info {
      struct file_operations *f_op;         int prealloc_block;
      struct inode *f_inode;                int journal_handle;
      loff_t f_pos;                         ext4_extent *cached_extent;
      /* I know nothing else! */            __u32 ext4_flags;
  };                                        /* VFS can't see these! */
                                        };
         │
         │  When VFS calls: file->f_op->read(file, buf, count, pos)
         │
         ▼
  ┌─────────────────────────────────────────────────────────────┐
  │  QUESTION: How does ext4_read() access ext4_file_info?      │
  │                                                             │
  │  ANSWER: Three strategies for storing/recovering private    │
  │          data from the framework object                     │
  └─────────────────────────────────────────────────────────────┘
```

**说明:**
- 框架层只定义通用字段，完全不知道实现的私有数据
- 实现层需要在每次操作调用时访问自己的私有数据
- 三种策略解决这个问题：嵌入、指针、内联存储

---

### Strategy 1: Embedding and `container_of`

**Concept:** Embed the framework object inside a larger implementation-specific structure. Use pointer arithmetic (`container_of`) to recover the full structure from the embedded member.

#### Memory Layout Diagram

```
MEMORY LAYOUT - EMBEDDING PATTERN:

Address
   │
   ▼
   ┌────────────────────────────────────────────────────────┐
   │              struct ext4_inode_info                     │
   │  ┌──────────────────────────────────────────────────┐  │
0x1000 │  __le32 i_data[15]         (60 bytes)            │  │ ← ext4-private
   │  │  __u32 i_dtime              (4 bytes)             │  │
   │  │  ext4_fsblk_t i_file_acl    (8 bytes)             │  │
   │  │  ext4_group_t i_block_group (4 bytes)             │  │
   │  │  unsigned long i_flags      (8 bytes)             │  │
   │  │  ... more ext4 fields ...                         │  │
   │  └──────────────────────────────────────────────────┘  │
   │                                                         │
   │  ┌──────────────────────────────────────────────────┐  │
0x1200 │  struct inode vfs_inode    (embedded)            │  │ ← VFS sees this
   │  │    i_mode, i_uid, i_gid                           │  │
   │  │    i_op, i_fop                                    │  │
   │  │    ... VFS fields ...                             │  │
   │  └──────────────────────────────────────────────────┘  │
   └────────────────────────────────────────────────────────┘

POINTER ARITHMETIC:

VFS has:     struct inode *inode = 0x1200
ext4 needs:  struct ext4_inode_info *ei = 0x1000

Calculation: ei = (void*)inode - offsetof(struct ext4_inode_info, vfs_inode)
           = 0x1200 - 0x200
           = 0x1000  ✓
```

**说明:**
- `ext4_inode_info` 结构体包含所有 ext4 私有字段
- `struct inode vfs_inode` 嵌入在 `ext4_inode_info` 内部
- VFS 只传递 `&ei->vfs_inode` 给操作函数
- ext4 用 `container_of` 从嵌入的 inode 恢复完整的 `ext4_inode_info`

#### Detailed Kernel Code Analysis

```c
/* ========== STEP 1: Define the embedding structure ========== */
/* fs/ext4/ext4.h:784-870 */
struct ext4_inode_info {
    /* [KEY] ext4-private fields BEFORE the embedded inode */
    __le32  i_data[15];           /* Direct/indirect block pointers */
    __u32   i_dtime;              /* Deletion time for undelete */
    ext4_fsblk_t i_file_acl;      /* File ACL block */
    ext4_group_t i_block_group;   /* Block group for this inode */
    unsigned long i_flags;        /* ext4-specific inode flags */
    
    /* [KEY] More ext4-specific fields */
    __u32   i_reserved_data_blocks;
    __u32   i_reserved_meta_blocks;
    
    /* ... many more ext4 fields ... */
    
    /* [KEY] THE EMBEDDED FRAMEWORK OBJECT - must be included */
    /* VFS will only ever see this member, not the surrounding struct */
    struct inode vfs_inode;
    /*           ^^^^^^^^^^
     *               |
     *               +-- This is what VFS passes to operations
     *                   VFS has no idea about the ext4 fields above
     */
};

/* ========== STEP 2: Define the recovery macro ========== */
/* fs/ext4/ext4.h:1259-1261 */

/* [KEY] container_of macro usage */
static inline struct ext4_inode_info *EXT4_I(struct inode *inode)
{
    /* container_of(ptr, type, member):
     * - ptr:    pointer to the embedded member (vfs_inode)
     * - type:   containing structure type (struct ext4_inode_info)
     * - member: name of embedded member (vfs_inode)
     * 
     * Returns: pointer to the containing structure
     */
    return container_of(inode, struct ext4_inode_info, vfs_inode);
    /*                   ^^^^^                         ^^^^^^^^^
     *                     |                               |
     *                     |                               +-- member name
     *                     +-- pointer to embedded inode
     */
}

/* [KEY] The container_of macro itself (include/linux/kernel.h) */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})
/*                           ^^^^^^^^^^^^^^^^^^^^^^^^^^
 *                                      |
 *                                      +-- Subtracts offset to get container start
 */

/* ========== STEP 3: Allocate with embedding in mind ========== */
/* fs/ext4/super.c */
static struct inode *ext4_alloc_inode(struct super_block *sb)
{
    struct ext4_inode_info *ei;
    
    /* [KEY] Allocate the FULL ext4_inode_info structure */
    /* This includes space for both ext4 fields AND embedded vfs_inode */
    ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);
    /*   ^^^^^^^^^^^^^^^^
     *          |
     *          +-- Allocates sizeof(struct ext4_inode_info) bytes
     */
    
    if (!ei)
        return NULL;
    
    /* [KEY] Initialize ext4-specific fields */
    ei->i_reserved_data_blocks = 0;
    ei->i_reserved_meta_blocks = 0;
    ei->i_block_group = 0;
    /* ... more initialization ... */
    
    /* [KEY] Return pointer to EMBEDDED inode, not the full struct */
    /* VFS will store and pass around this pointer */
    return &ei->vfs_inode;
    /*     ^^^^^^^^^^^^^^
     *           |
     *           +-- VFS only sees the embedded inode
     *               ext4 private data is "invisible" before this address
     */
}

/* ========== STEP 4: Use in operations ========== */
/* fs/ext4/file.c */
static int ext4_file_open(struct inode *inode, struct file *file)
{
    /* [KEY] VFS passed 'inode' which is actually &some_ei->vfs_inode */
    /* Recover the full ext4_inode_info structure */
    struct ext4_inode_info *ei = EXT4_I(inode);
    /*                          ^^^^^^^^
     *                              |
     *                              +-- container_of recovers full struct
     */
    
    struct super_block *sb = inode->i_sb;
    struct ext4_sb_info *sbi = EXT4_SB(sb);  /* Similar pattern for superblock */
    
    /* [KEY] Now we can access ext4-specific fields! */
    if (ei->i_flags & EXT4_ENCRYPT_FL) {
        /* Handle encrypted file - uses ext4-specific i_flags */
        return ext4_crypto_check_encryption_mode(inode);
    }
    
    if (ei->i_file_acl) {
        /* Handle extended attributes - uses ext4-specific i_file_acl */
        ext4_load_acl(inode);
    }
    
    /* [KEY] Can also access generic inode fields via same pointer */
    if (inode->i_mode & S_IFDIR) {
        /* It's a directory */
    }
    
    return 0;
}
```

#### Complete Userspace C Simulation: Strategy 1

```c
/*
 * strategy1_embedding.c - Userspace simulation of embedding + container_of
 * 
 * Compile: gcc -o strategy1 strategy1_embedding.c
 * Run: ./strategy1
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>  /* for offsetof */
#include <string.h>

/* ========== container_of macro (same as Linux kernel) ========== */

/* [KEY] This is the magic that enables private data recovery */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
/*          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *                         |
 *                         +-- Pointer arithmetic:
 *                             1. Cast ptr to char* for byte-level math
 *                             2. Subtract the offset of member within type
 *                             3. Cast result to type*
 */

/* ========== Framework Layer (like VFS) ========== */

/* [KEY] Framework only knows about this generic structure */
struct framework_object {
    int generic_field1;
    int generic_field2;
    const struct framework_ops *ops;  /* [KEY] Operations table pointer */
};

/* [KEY] Operations table - the contract */
struct framework_ops {
    int (*operation)(struct framework_object *obj, int param);
    void (*cleanup)(struct framework_object *obj);
};

/* [KEY] Framework function that calls operations */
int framework_do_operation(struct framework_object *obj, int param)
{
    printf("[Framework] Calling obj->ops->operation(obj, %d)\n", param);
    
    /* [KEY] THE OPS PATTERN: obj->ops->operation(obj, param) */
    /* Framework passes 'obj' BACK to implementation */
    return obj->ops->operation(obj, param);
}

void framework_cleanup(struct framework_object *obj)
{
    if (obj->ops && obj->ops->cleanup) {
        obj->ops->cleanup(obj);
    }
}

/* ========== Implementation Layer (like ext4) ========== */

/* [KEY] Implementation's private data structure with EMBEDDED framework object */
struct impl_private_data {
    /* Implementation-specific fields (framework doesn't know about these) */
    int private_counter;
    char private_name[32];
    double private_value;
    
    /* [KEY] EMBEDDED framework object - MUST be a member, not a pointer */
    struct framework_object base;  /* Framework will only see this part */
};
/*
 * MEMORY LAYOUT:
 * 
 * +----------------------------------+
 * | struct impl_private_data         |
 * |   int private_counter            |  ← offset 0
 * |   char private_name[32]          |  ← offset 4
 * |   double private_value           |  ← offset 36
 * |   ┌──────────────────────────┐   |
 * |   | struct framework_object  |   |  ← offset 48 (base)
 * |   |   generic_field1         |   |
 * |   |   generic_field2         |   |
 * |   |   ops                    |   |
 * |   └──────────────────────────┘   |
 * +----------------------------------+
 */

/* [KEY] Recovery macro - implementation defines this */
#define IMPL_PRIVATE(obj) \
    container_of(obj, struct impl_private_data, base)
/*                                              ^^^^
 *                                               |
 *                                               +-- name of embedded member
 */

/* [KEY] Implementation's operation function */
static int impl_operation(struct framework_object *obj, int param)
{
    /* [KEY] PRIVATE DATA RECOVERY using container_of */
    /* obj points to &some_impl_private_data->base */
    /* We recover the full impl_private_data structure */
    struct impl_private_data *priv = IMPL_PRIVATE(obj);
    
    printf("[Implementation] Recovered private data:\n");
    printf("  private_counter = %d\n", priv->private_counter);
    printf("  private_name = '%s'\n", priv->private_name);
    printf("  private_value = %.2f\n", priv->private_value);
    
    /* [KEY] Modify private data */
    priv->private_counter += param;
    printf("[Implementation] Updated counter to %d\n", priv->private_counter);
    
    /* [KEY] Can also access generic fields via obj */
    printf("[Implementation] generic_field1 = %d\n", obj->generic_field1);
    
    return priv->private_counter;
}

/* [KEY] Implementation's cleanup function */
static void impl_cleanup(struct framework_object *obj)
{
    struct impl_private_data *priv = IMPL_PRIVATE(obj);
    printf("[Implementation] Cleanup: freeing '%s'\n", priv->private_name);
    free(priv);  /* Free the FULL structure, not just obj */
}

/* [KEY] Implementation's operations table */
static const struct framework_ops impl_ops = {
    .operation = impl_operation,
    .cleanup = impl_cleanup,
};

/* [KEY] Implementation's allocator - called by framework */
struct framework_object *impl_alloc(const char *name)
{
    /* [KEY] Allocate the FULL implementation structure */
    struct impl_private_data *priv = malloc(sizeof(*priv));
    if (!priv) return NULL;
    
    /* [KEY] Initialize private fields (framework doesn't know about these) */
    priv->private_counter = 100;
    strncpy(priv->private_name, name, sizeof(priv->private_name) - 1);
    priv->private_value = 3.14159;
    
    /* [KEY] Initialize the embedded framework object */
    priv->base.generic_field1 = 1;
    priv->base.generic_field2 = 2;
    priv->base.ops = &impl_ops;  /* [KEY] Point to our operations */
    
    /* [KEY] Return pointer to EMBEDDED object, not the full struct */
    return &priv->base;
    /*     ^^^^^^^^^^^
     *          |
     *          +-- Framework receives pointer to embedded member
     *              It has no way to access the private fields directly
     *              Only impl_operation() can recover them via container_of
     */
}

/* ========== Demonstration ========== */

int main(void)
{
    printf("=== Strategy 1: Embedding + container_of Demo ===\n\n");
    
    /* [KEY] Show the memory layout and offsets */
    printf("Memory Layout Analysis:\n");
    printf("  sizeof(struct impl_private_data) = %zu bytes\n", 
           sizeof(struct impl_private_data));
    printf("  sizeof(struct framework_object) = %zu bytes\n", 
           sizeof(struct framework_object));
    printf("  offsetof(impl_private_data, base) = %zu bytes\n", 
           offsetof(struct impl_private_data, base));
    printf("\n");
    
    /* [KEY] Framework allocates via implementation */
    struct framework_object *obj = impl_alloc("MyObject");
    
    printf("Pointer Analysis:\n");
    printf("  obj (framework sees)     = %p\n", (void*)obj);
    printf("  IMPL_PRIVATE(obj)        = %p\n", (void*)IMPL_PRIVATE(obj));
    printf("  Difference               = %td bytes (= offset of 'base')\n",
           (char*)obj - (char*)IMPL_PRIVATE(obj));
    printf("\n");
    
    /* [KEY] Framework calls operation - implementation recovers private data */
    printf("First operation call:\n");
    int result = framework_do_operation(obj, 5);
    printf("Result: %d\n\n", result);
    
    printf("Second operation call:\n");
    result = framework_do_operation(obj, 10);
    printf("Result: %d\n\n", result);
    
    /* [KEY] Framework triggers cleanup */
    printf("Cleanup:\n");
    framework_cleanup(obj);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}

/*
 * EXPECTED OUTPUT:
 * 
 * === Strategy 1: Embedding + container_of Demo ===
 * 
 * Memory Layout Analysis:
 *   sizeof(struct impl_private_data) = 72 bytes
 *   sizeof(struct framework_object) = 24 bytes
 *   offsetof(impl_private_data, base) = 48 bytes
 * 
 * Pointer Analysis:
 *   obj (framework sees)     = 0x55a1234567a0
 *   IMPL_PRIVATE(obj)        = 0x55a123456770
 *   Difference               = 48 bytes (= offset of 'base')
 * 
 * First operation call:
 * [Framework] Calling obj->ops->operation(obj, 5)
 * [Implementation] Recovered private data:
 *   private_counter = 100
 *   private_name = 'MyObject'
 *   private_value = 3.14
 * [Implementation] Updated counter to 105
 * [Implementation] generic_field1 = 1
 * Result: 105
 * 
 * Second operation call:
 * [Framework] Calling obj->ops->operation(obj, 10)
 * [Implementation] Recovered private data:
 *   private_counter = 105
 *   private_name = 'MyObject'
 *   private_value = 3.14
 * [Implementation] Updated counter to 115
 * [Implementation] generic_field1 = 1
 * Result: 115
 * 
 * Cleanup:
 * [Implementation] Cleanup: freeing 'MyObject'
 * 
 * === Demo Complete ===
 */
```

**说明 (Strategy 1):**
- 嵌入策略是内核最常用的私有数据方法
- 实现结构体包含框架结构体作为成员（不是指针）
- `container_of` 宏使用编译时计算的偏移量恢复完整结构
- 优点：无额外内存分配，无指针追踪开销
- 缺点：框架必须调用实现的分配函数（如 `alloc_inode`）

---

### Strategy 2: Private Pointer Field

**Concept:** The framework object includes an explicit `void *` pointer field that implementations can use to store a pointer to their private data.

#### Memory Layout Diagram

```
MEMORY LAYOUT - PRIVATE POINTER PATTERN:

      Framework Object (dentry)              Separate Private Data
      ┌──────────────────────────┐           ┌─────────────────────┐
      │ struct dentry            │           │ struct nfs_dentry   │
      │   d_name                 │           │   nfs_fh fhandle    │
      │   d_inode                │           │   nfs_fattr fattr   │
      │   d_parent               │           │   unsigned long     │
      │   d_op ─────────────────────────┐    │     cache_validity  │
      │   void *d_fsdata ────────────┐  │    └─────────────────────┘
      │   ...                    │   │  │             ▲
      └──────────────────────────┘   │  │             │
           ▲                         │  │             │
           │                         │  │             │
   Framework allocates this          │  │    Implementation allocates
   during path lookup                │  │    this separately
                                     │  │
                                     │  └──► const struct dentry_operations
                                     │       { .d_revalidate = nfs_revalidate }
                                     │
                                     └──────► points to nfs_dentry (private data)

RECOVERY:
    struct nfs_dentry *nfs_data = dentry->d_fsdata;
    /* Simple pointer cast, no arithmetic needed */
```

**说明:**
- 框架对象有一个 `void *` 字段供实现使用
- 实现单独分配私有数据并存储指针
- 恢复只需简单的指针解引用和类型转换
- 两个独立的内存分配（框架对象 + 私有数据）

#### Detailed Kernel Code Analysis

```c
/* ========== STEP 1: Framework defines object with private pointer ========== */
/* include/linux/dcache.h:90-145 */
struct dentry {
    unsigned int d_flags;
    seqcount_t d_seq;
    struct hlist_bl_node d_hash;
    struct dentry *d_parent;
    struct qstr d_name;
    struct inode *d_inode;
    unsigned char d_iname[DNAME_INLINE_LEN];
    
    const struct dentry_operations *d_op;  /* [KEY] Operations table */
    struct super_block *d_sb;
    unsigned long d_time;
    
    /* [KEY] Private pointer for filesystem-specific data */
    void *d_fsdata;
    /*    ^^^^^^^^^
     *        |
     *        +-- Framework provides this void* for implementation's use
     *            Framework never dereferences it
     *            Only implementation knows the actual type
     */
    
    struct list_head d_lru;
    struct dentry *d_child;
    struct list_head d_subdirs;
    /* ... */
};

/* ========== STEP 2: Implementation defines its private data ========== */
/* fs/nfs/dir.c (simplified) */
struct nfs_dentry_data {
    /* [KEY] NFS-specific fields for this dentry */
    struct nfs_fh fhandle;        /* NFS file handle */
    struct nfs_fattr fattr;       /* File attributes from server */
    unsigned long cache_validity; /* Cache state */
    unsigned long flags;          /* NFS flags */
};

/* ========== STEP 3: Implementation allocates and stores private data ========== */
/* fs/nfs/dir.c - during lookup or create */
static struct dentry *nfs_lookup(struct inode *dir, struct dentry *dentry,
                                  struct nameidata *nd)
{
    struct nfs_dentry_data *nfs_data;
    
    /* [KEY] Allocate separate private data structure */
    nfs_data = kmalloc(sizeof(*nfs_data), GFP_KERNEL);
    /*         ^^^^^^
     *            |
     *            +-- Separate allocation, not part of dentry
     */
    
    if (!nfs_data)
        return ERR_PTR(-ENOMEM);
    
    /* [KEY] Initialize NFS-specific fields */
    nfs_data->cache_validity = 0;
    nfs_data->flags = 0;
    
    /* [KEY] Store pointer in framework object's private field */
    dentry->d_fsdata = nfs_data;
    /*                 ^^^^^^^^
     *                     |
     *                     +-- Store pointer to our private data
     *                         Framework will preserve this pointer
     *                         but never dereference it
     */
    
    /* [KEY] Set operations table */
    dentry->d_op = &nfs_dentry_operations;
    
    /* ... perform NFS lookup ... */
    
    return dentry;
}

/* ========== STEP 4: Recover private data in operations ========== */
/* fs/nfs/dir.c */
static int nfs_lookup_revalidate(struct dentry *dentry, struct nameidata *nd)
{
    /* [KEY] PRIVATE DATA RECOVERY via pointer field */
    /* Simple: just dereference d_fsdata and cast to correct type */
    struct nfs_dentry_data *nfs_data = dentry->d_fsdata;
    /*                                 ^^^^^^^^^^^^^^^^
     *                                        |
     *                                        +-- Direct pointer access
     *                                            No arithmetic needed
     */
    
    /* [KEY] Now we can use NFS-specific data */
    if (nfs_data->cache_validity & NFS_INO_INVALID_DATA) {
        /* Need to re-fetch from server */
        return nfs_do_revalidate(dentry);
    }
    
    if (time_after(jiffies, nfs_data->cache_validity)) {
        /* Cache expired */
        return 0;
    }
    
    return 1;  /* Still valid */
}

/* ========== STEP 5: Clean up private data ========== */
/* fs/nfs/dir.c */
static void nfs_d_release(struct dentry *dentry)
{
    /* [KEY] Called when dentry is being destroyed */
    struct nfs_dentry_data *nfs_data = dentry->d_fsdata;
    
    if (nfs_data) {
        /* [KEY] Free the separately allocated private data */
        kfree(nfs_data);
        /*     ^^^^^^^^
         *         |
         *         +-- Must free what we allocated in nfs_lookup
         */
        dentry->d_fsdata = NULL;
    }
}

/* [KEY] Operations table */
const struct dentry_operations nfs_dentry_operations = {
    .d_revalidate = nfs_lookup_revalidate,
    .d_release    = nfs_d_release,         /* [KEY] Cleanup hook */
    /* ... */
};
```

#### Complete Userspace C Simulation: Strategy 2

```c
/*
 * strategy2_private_pointer.c - Userspace simulation of private pointer field
 * 
 * Compile: gcc -o strategy2 strategy2_private_pointer.c
 * Run: ./strategy2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== Framework Layer ========== */

/* [KEY] Framework object with explicit private pointer */
struct framework_object {
    int id;
    const char *name;
    
    /* [KEY] Private data pointer for implementation's use */
    void *private_data;
    /*    ^^^^^^^^^^^^
     *         |
     *         +-- Framework provides this void* field
     *             Implementation can store anything here
     *             Framework preserves but never dereferences it
     */
    
    const struct framework_ops *ops;
};

struct framework_ops {
    int (*process)(struct framework_object *obj);
    int (*get_status)(struct framework_object *obj);
    void (*release)(struct framework_object *obj);
};

/* [KEY] Framework allocates the base object */
struct framework_object *framework_alloc(int id, const char *name)
{
    struct framework_object *obj = malloc(sizeof(*obj));
    if (!obj) return NULL;
    
    obj->id = id;
    obj->name = name;
    obj->private_data = NULL;  /* [KEY] Implementation will set this later */
    obj->ops = NULL;
    
    return obj;
}

/* Framework calls operation */
int framework_process(struct framework_object *obj)
{
    printf("[Framework] Processing object '%s' (id=%d)\n", obj->name, obj->id);
    
    if (obj->ops && obj->ops->process) {
        return obj->ops->process(obj);
    }
    return -1;
}

int framework_get_status(struct framework_object *obj)
{
    if (obj->ops && obj->ops->get_status) {
        return obj->ops->get_status(obj);
    }
    return -1;
}

void framework_release(struct framework_object *obj)
{
    if (obj->ops && obj->ops->release) {
        obj->ops->release(obj);
    }
    free(obj);  /* Framework frees the base object */
}

/* ========== Implementation A: Simple Counter ========== */

/* [KEY] Implementation A's private data */
struct impl_a_private {
    int counter;
    int max_value;
    const char *description;
};

static int impl_a_process(struct framework_object *obj)
{
    /* [KEY] PRIVATE DATA RECOVERY - just dereference and cast */
    struct impl_a_private *priv = obj->private_data;
    /*                           ^^^^^^^^^^^^^^^^
     *                                  |
     *                                  +-- Direct access to void* field
     *                                      Cast to our known type
     */
    
    printf("[Impl A] Processing with counter=%d (max=%d)\n", 
           priv->counter, priv->max_value);
    
    priv->counter++;
    if (priv->counter > priv->max_value) {
        printf("[Impl A] Counter exceeded max!\n");
        return -1;
    }
    
    return priv->counter;
}

static int impl_a_get_status(struct framework_object *obj)
{
    struct impl_a_private *priv = obj->private_data;
    return priv->counter;
}

static void impl_a_release(struct framework_object *obj)
{
    struct impl_a_private *priv = obj->private_data;
    printf("[Impl A] Releasing: '%s'\n", priv->description);
    
    /* [KEY] Free separately allocated private data */
    free(priv);
    /*   ^^^^
     *     |
     *     +-- Implementation must free what it allocated
     */
    obj->private_data = NULL;
}

static const struct framework_ops impl_a_ops = {
    .process = impl_a_process,
    .get_status = impl_a_get_status,
    .release = impl_a_release,
};

/* [KEY] Implementation A's initialization */
int impl_a_init(struct framework_object *obj, int max_value, const char *desc)
{
    /* [KEY] Allocate private data SEPARATELY */
    struct impl_a_private *priv = malloc(sizeof(*priv));
    if (!priv) return -1;
    
    priv->counter = 0;
    priv->max_value = max_value;
    priv->description = desc;
    
    /* [KEY] Store pointer in framework object's private field */
    obj->private_data = priv;
    /*                  ^^^^
     *                    |
     *                    +-- Store our pointer in framework's void* field
     */
    
    obj->ops = &impl_a_ops;
    
    return 0;
}

/* ========== Implementation B: State Machine ========== */

/* [KEY] Implementation B's private data (completely different structure) */
struct impl_b_private {
    enum { STATE_IDLE, STATE_RUNNING, STATE_PAUSED, STATE_STOPPED } state;
    int transitions;
    char last_event[64];
};

static int impl_b_process(struct framework_object *obj)
{
    /* [KEY] Same recovery pattern, different type */
    struct impl_b_private *priv = obj->private_data;
    
    printf("[Impl B] Current state: %d, transitions: %d\n", 
           priv->state, priv->transitions);
    
    /* State machine logic */
    switch (priv->state) {
        case STATE_IDLE:
            priv->state = STATE_RUNNING;
            strcpy(priv->last_event, "started");
            break;
        case STATE_RUNNING:
            priv->state = STATE_PAUSED;
            strcpy(priv->last_event, "paused");
            break;
        case STATE_PAUSED:
            priv->state = STATE_RUNNING;
            strcpy(priv->last_event, "resumed");
            break;
        default:
            return -1;
    }
    
    priv->transitions++;
    printf("[Impl B] Transition: %s\n", priv->last_event);
    
    return priv->state;
}

static int impl_b_get_status(struct framework_object *obj)
{
    struct impl_b_private *priv = obj->private_data;
    return priv->state;
}

static void impl_b_release(struct framework_object *obj)
{
    struct impl_b_private *priv = obj->private_data;
    printf("[Impl B] Releasing after %d transitions, last event: '%s'\n",
           priv->transitions, priv->last_event);
    free(priv);
    obj->private_data = NULL;
}

static const struct framework_ops impl_b_ops = {
    .process = impl_b_process,
    .get_status = impl_b_get_status,
    .release = impl_b_release,
};

int impl_b_init(struct framework_object *obj)
{
    struct impl_b_private *priv = malloc(sizeof(*priv));
    if (!priv) return -1;
    
    priv->state = STATE_IDLE;
    priv->transitions = 0;
    strcpy(priv->last_event, "initialized");
    
    obj->private_data = priv;
    obj->ops = &impl_b_ops;
    
    return 0;
}

/* ========== Demonstration ========== */

int main(void)
{
    printf("=== Strategy 2: Private Pointer Field Demo ===\n\n");
    
    /*
     * MEMORY LAYOUT VISUALIZATION:
     * 
     * Framework Object             Private Data (Impl A)
     * ┌─────────────────┐          ┌──────────────────┐
     * │ id = 1          │          │ counter = 0      │
     * │ name = "ObjA"   │          │ max_value = 5    │
     * │ private_data ───┼─────────►│ description = ...│
     * │ ops ────────────┼──┐       └──────────────────┘
     * └─────────────────┘  │
     *                      └──────► impl_a_ops {...}
     * 
     * Framework Object             Private Data (Impl B)
     * ┌─────────────────┐          ┌──────────────────┐
     * │ id = 2          │          │ state = IDLE     │
     * │ name = "ObjB"   │          │ transitions = 0  │
     * │ private_data ───┼─────────►│ last_event = ... │
     * │ ops ────────────┼──┐       └──────────────────┘
     *                      └──────► impl_b_ops {...}
     */
    
    printf("Creating two objects with DIFFERENT implementations:\n\n");
    
    /* Create object with Implementation A */
    struct framework_object *obj_a = framework_alloc(1, "ObjectA");
    impl_a_init(obj_a, 5, "Counter implementation");
    
    /* Create object with Implementation B */
    struct framework_object *obj_b = framework_alloc(2, "ObjectB");
    impl_b_init(obj_b);
    
    printf("Pointer Analysis:\n");
    printf("  obj_a             = %p\n", (void*)obj_a);
    printf("  obj_a->private    = %p (separate allocation)\n", obj_a->private_data);
    printf("  obj_b             = %p\n", (void*)obj_b);
    printf("  obj_b->private    = %p (separate allocation)\n", obj_b->private_data);
    printf("\n");
    
    /* Process both objects - each uses its own private data */
    printf("=== Processing Object A (counter) ===\n");
    for (int i = 0; i < 3; i++) {
        framework_process(obj_a);
    }
    printf("Status: %d\n\n", framework_get_status(obj_a));
    
    printf("=== Processing Object B (state machine) ===\n");
    for (int i = 0; i < 3; i++) {
        framework_process(obj_b);
    }
    printf("Status: %d\n\n", framework_get_status(obj_b));
    
    /* Cleanup */
    printf("=== Cleanup ===\n");
    framework_release(obj_a);
    framework_release(obj_b);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}

/*
 * EXPECTED OUTPUT:
 * 
 * === Strategy 2: Private Pointer Field Demo ===
 * 
 * Creating two objects with DIFFERENT implementations:
 * 
 * Pointer Analysis:
 *   obj_a             = 0x55a123456780
 *   obj_a->private    = 0x55a1234567b0 (separate allocation)
 *   obj_b             = 0x55a1234567e0
 *   obj_b->private    = 0x55a123456810 (separate allocation)
 * 
 * === Processing Object A (counter) ===
 * [Framework] Processing object 'ObjectA' (id=1)
 * [Impl A] Processing with counter=0 (max=5)
 * [Framework] Processing object 'ObjectA' (id=1)
 * [Impl A] Processing with counter=1 (max=5)
 * [Framework] Processing object 'ObjectA' (id=1)
 * [Impl A] Processing with counter=2 (max=5)
 * Status: 3
 * 
 * === Processing Object B (state machine) ===
 * [Framework] Processing object 'ObjectB' (id=2)
 * [Impl B] Current state: 0, transitions: 0
 * [Impl B] Transition: started
 * [Framework] Processing object 'ObjectB' (id=2)
 * [Impl B] Current state: 1, transitions: 1
 * [Impl B] Transition: paused
 * [Framework] Processing object 'ObjectB' (id=2)
 * [Impl B] Current state: 2, transitions: 2
 * [Impl B] Transition: resumed
 * Status: 1
 * 
 * === Cleanup ===
 * [Impl A] Releasing: 'Counter implementation'
 * [Impl B] Releasing after 3 transitions, last event: 'resumed'
 * 
 * === Demo Complete ===
 */
```

**说明 (Strategy 2):**
- 框架对象包含一个 `void *` 字段供实现使用
- 实现单独分配私有数据并存储指针
- 恢复简单：`priv = obj->private_data`（只是指针解引用）
- 优点：框架可以自己分配对象，实现后续初始化私有数据
- 缺点：额外的内存分配和指针追踪开销

---

### Strategy 3: Inline Fixed-Size Data

**Concept:** The framework object includes a fixed-size array or buffer that implementations can use directly, without separate allocation.

#### Memory Layout Diagram

```
MEMORY LAYOUT - INLINE DATA PATTERN:

      Framework Object (inet_connection_sock)
      ┌───────────────────────────────────────────────────────┐
      │ struct inet_connection_sock                           │
      │   struct inet_sock icsk_inet                          │
      │   ...                                                 │
      │   const struct tcp_congestion_ops *icsk_ca_ops ───────┼──► { .cong_avoid = cubic_cong_avoid }
      │   ...                                                 │
      │   ┌───────────────────────────────────────────────┐   │
      │   │ __u32 icsk_ca_priv[16]  (64 bytes inline)     │   │ ← Private storage
      │   │                                               │   │
      │   │   [0]: beg_snd_nxt                            │   │   Implementation
      │   │   [1]: beg_snd_una                            │   │   interprets these
      │   │   [2]: beg_snd_cwnd                           │   │   bytes as its own
      │   │   [3]: bic_origin_point                       │   │   structure
      │   │   ...                                         │   │
      │   └───────────────────────────────────────────────┘   │
      │   ...                                                 │
      └───────────────────────────────────────────────────────┘

NO SEPARATE ALLOCATION NEEDED!
Private data lives INSIDE the framework object.

RECOVERY:
    struct bictcp *ca = (struct bictcp *)icsk->icsk_ca_priv;
    /* Simple cast - no pointer arithmetic, no separate allocation */
    /* Implementation must ensure sizeof(bictcp) <= sizeof(icsk_ca_priv) */
```

**说明:**
- 框架对象内部有固定大小的存储空间
- 实现将自己的结构体叠加（overlay）在这个空间上
- 无额外分配，无指针追踪
- 实现必须确保私有数据适合框架提供的空间

#### Detailed Kernel Code Analysis

```c
/* ========== STEP 1: Framework provides inline storage ========== */
/* include/net/inet_connection_sock.h:86-140 */
struct inet_connection_sock {
    struct inet_sock icsk_inet;
    
    /* [KEY] Pointer to congestion control algorithm operations */
    const struct tcp_congestion_ops *icsk_ca_ops;
    /*                               ^^^^^^^^^^^
     *                                    |
     *                                    +-- Points to algorithm's ops table
     *                                        (cubic, reno, vegas, etc.)
     */
    
    /* Other connection-level fields... */
    __u8  icsk_ca_state;
    __u8  icsk_retransmits;
    
    /* [KEY] INLINE PRIVATE DATA STORAGE */
    /* Fixed-size array that congestion algorithms can use */
    __u32 icsk_ca_priv[16];
    /*    ^^^^^^^^^^^^^^^
     *          |
     *          +-- 16 * 4 = 64 bytes of inline storage
     *              Algorithms cast this to their private struct
     *              No separate allocation needed!
     */
};

/* ========== STEP 2: Helper macro to access inline storage ========== */
/* include/net/tcp.h */
static inline void *inet_csk_ca(const struct sock *sk)
{
    /* [KEY] Return pointer to inline storage, cast to void* */
    return (void *)inet_csk(sk)->icsk_ca_priv;
    /*             ^^^^^^^^^^^^^^^^^^^^^^^^^
     *                        |
     *                        +-- Returns &icsk->icsk_ca_priv[0]
     *                            Algorithm casts to its private struct type
     */
}

/* ========== STEP 3: Algorithm defines private data structure ========== */
/* net/ipv4/tcp_cubic.c */
struct bictcp {
    /* [KEY] All fields must fit in icsk_ca_priv (64 bytes) */
    u32 cnt;              /* 4 bytes - ACK counter */
    u32 last_max_cwnd;    /* 4 bytes - last maximum cwnd */
    u32 loss_cwnd;        /* 4 bytes - cwnd at last loss */
    u32 last_cwnd;        /* 4 bytes - last cwnd */
    u32 last_time;        /* 4 bytes - time of last update */
    u32 bic_origin_point; /* 4 bytes - origin point of bic function */
    u32 bic_K;            /* 4 bytes - time to origin from last_max */
    u32 delay_min;        /* 4 bytes - min RTT */
    u32 epoch_start;      /* 4 bytes - start of current epoch */
    u32 ack_cnt;          /* 4 bytes - ACKs in current epoch */
    u32 tcp_cwnd;         /* 4 bytes - estimated Reno cwnd */
    u16 delayed_ack;      /* 2 bytes - delayed ACK estimate */
    u8  sample_cnt;       /* 1 byte - RTT sample count */
    u8  found;            /* 1 byte - cubic root found flag */
    u32 round_start;      /* 4 bytes - round start time */
    u32 end_seq;          /* 4 bytes - end sequence number */
    u32 last_ack;         /* 4 bytes - last ACK time */
    u32 curr_rtt;         /* 4 bytes - current RTT */
    /* Total: approximately 60 bytes, fits in 64 bytes */
};
/*
 * BUILD-TIME CHECK (optional but recommended):
 * BUILD_BUG_ON(sizeof(struct bictcp) > sizeof(__u32) * 16);
 */

/* ========== STEP 4: Initialize private data in inline storage ========== */
/* net/ipv4/tcp_cubic.c */
static void bictcp_init(struct sock *sk)
{
    /* [KEY] Get pointer to inline storage */
    struct bictcp *ca = inet_csk_ca(sk);
    /*                  ^^^^^^^^^^^^^^
     *                        |
     *                        +-- Returns &icsk->icsk_ca_priv[0]
     *                            We treat it as struct bictcp*
     */
    
    /* [KEY] Initialize our private data in the inline storage */
    /* No malloc needed! Memory is already part of the socket. */
    ca->cnt = 0;
    ca->last_max_cwnd = 0;
    ca->loss_cwnd = 0;
    ca->last_cwnd = 0;
    ca->last_time = 0;
    ca->epoch_start = 0;
    ca->delay_min = 0;
    ca->ack_cnt = 0;
    ca->tcp_cwnd = 0;
    ca->found = 0;
    /* ... */
}

/* ========== STEP 5: Use private data in operations ========== */
/* net/ipv4/tcp_cubic.c */
static void bictcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    struct tcp_sock *tp = tcp_sk(sk);
    
    /* [KEY] PRIVATE DATA RECOVERY from inline storage */
    struct bictcp *ca = inet_csk_ca(sk);
    /*                  ^^^^^^^^^^^^^^
     *                        |
     *                        +-- Same inline storage, interpreted as bictcp
     */
    
    if (!tcp_is_cwnd_limited(sk, in_flight))
        return;
    
    if (tp->snd_cwnd <= tp->snd_ssthresh) {
        /* [KEY] Slow start - use tcp_slow_start */
        tcp_slow_start(tp);
    } else {
        /* [KEY] Congestion avoidance - CUBIC algorithm */
        bictcp_update(ca, tp->snd_cwnd);
        /*            ^^
         *             |
         *             +-- Uses private data for algorithm state
         */
        
        if (ca->cnt > tp->snd_cwnd) {
            tp->snd_cwnd++;
            ca->cnt = 0;
        }
    }
}

/* ========== STEP 6: No explicit cleanup needed ========== */
/* When socket is destroyed, inline storage is freed automatically
 * as part of the socket structure. No separate free() needed! */

/* [KEY] Operations table registration */
static struct tcp_congestion_ops cubictcp __read_mostly = {
    .init       = bictcp_init,       /* Initialize inline storage */
    .ssthresh   = bictcp_recalc_ssthresh,
    .cong_avoid = bictcp_cong_avoid, /* Use inline storage */
    .set_state  = bictcp_state,
    .undo_cwnd  = bictcp_undo_cwnd,
    .pkts_acked = bictcp_acked,
    .owner      = THIS_MODULE,
    .name       = "cubic",
    /* Note: no .release needed - inline storage freed with socket */
};
```

#### Complete Userspace C Simulation: Strategy 3

```c
/*
 * strategy3_inline_data.c - Userspace simulation of inline fixed-size storage
 * 
 * Compile: gcc -o strategy3 strategy3_inline_data.c
 * Run: ./strategy3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ========== Framework Layer ========== */

/* [KEY] Size of inline private storage (like icsk_ca_priv[16]) */
#define PRIVATE_STORAGE_SIZE  64  /* 64 bytes, same as 16 * sizeof(u32) */

struct framework_ops;

/* [KEY] Framework object with INLINE storage for private data */
struct framework_object {
    int id;
    const char *name;
    
    const struct framework_ops *ops;
    
    /* [KEY] INLINE PRIVATE STORAGE - fixed size array */
    /* Implementations overlay their structs on this storage */
    uint8_t private_storage[PRIVATE_STORAGE_SIZE];
    /*      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     *                     |
     *                     +-- 64 bytes of inline storage
     *                         No separate allocation needed
     *                         Implementation casts to its type
     */
};

/*
 * MEMORY LAYOUT:
 * 
 * struct framework_object
 * ┌────────────────────────────────────────┐
 * │ id                      (4 bytes)      │
 * │ name                    (8 bytes ptr)  │
 * │ ops                     (8 bytes ptr)  │
 * │ ┌──────────────────────────────────┐   │
 * │ │ private_storage[64]              │   │ ← Inline storage
 * │ │                                  │   │
 * │ │   [Implementation A might see:]  │   │
 * │ │     struct impl_a_data {...}     │   │
 * │ │                                  │   │
 * │ │   [Implementation B might see:]  │   │
 * │ │     struct impl_b_data {...}     │   │
 * │ │                                  │   │
 * │ └──────────────────────────────────┘   │
 * └────────────────────────────────────────┘
 * 
 * TOTAL: Single allocation, private data included
 */

struct framework_ops {
    void (*init)(struct framework_object *obj);
    int (*process)(struct framework_object *obj, int input);
    void (*get_info)(struct framework_object *obj);
    /* Note: no cleanup op needed - storage freed with object */
};

/* [KEY] Helper to access inline storage (like inet_csk_ca) */
static inline void *get_private_storage(struct framework_object *obj)
{
    return obj->private_storage;
    /*     ^^^^^^^^^^^^^^^^^^^
     *            |
     *            +-- Returns pointer to inline array
     *                Implementation casts to its specific type
     */
}

/* Framework allocation */
struct framework_object *framework_alloc(int id, const char *name)
{
    struct framework_object *obj = malloc(sizeof(*obj));
    if (!obj) return NULL;
    
    obj->id = id;
    obj->name = name;
    obj->ops = NULL;
    
    /* [KEY] Zero out inline storage */
    memset(obj->private_storage, 0, sizeof(obj->private_storage));
    
    return obj;
}

void framework_init(struct framework_object *obj)
{
    if (obj->ops && obj->ops->init) {
        obj->ops->init(obj);
    }
}

int framework_process(struct framework_object *obj, int input)
{
    printf("[Framework] Processing '%s' with input=%d\n", obj->name, input);
    if (obj->ops && obj->ops->process) {
        return obj->ops->process(obj, input);
    }
    return -1;
}

void framework_free(struct framework_object *obj)
{
    /* [KEY] No separate cleanup needed for inline storage! */
    /* When we free the framework object, inline storage goes with it */
    printf("[Framework] Freeing object '%s' (inline storage freed automatically)\n",
           obj->name);
    free(obj);
}

/* ========== Implementation A: Moving Average Calculator ========== */

/* [KEY] Implementation A's private data - must fit in 64 bytes */
struct impl_a_data {
    int samples[10];       /* 40 bytes - sample buffer */
    int sample_count;      /* 4 bytes - number of samples */
    int next_index;        /* 4 bytes - circular buffer index */
    int sum;               /* 4 bytes - running sum */
    int average;           /* 4 bytes - calculated average */
    /* Total: 56 bytes, fits in 64 bytes ✓ */
};

/* Compile-time check (optional) */
_Static_assert(sizeof(struct impl_a_data) <= PRIVATE_STORAGE_SIZE,
               "impl_a_data too large for inline storage");

static void impl_a_init(struct framework_object *obj)
{
    /* [KEY] Cast inline storage to our type */
    struct impl_a_data *data = get_private_storage(obj);
    /*                        ^^^^^^^^^^^^^^^^^^^^^^^^
     *                                   |
     *                                   +-- Returns &obj->private_storage[0]
     *                                       We treat it as struct impl_a_data*
     */
    
    printf("[Impl A] Initializing moving average calculator in inline storage\n");
    
    /* [KEY] Initialize our data in the inline storage */
    /* No malloc! We're writing directly into the framework object */
    memset(data->samples, 0, sizeof(data->samples));
    data->sample_count = 0;
    data->next_index = 0;
    data->sum = 0;
    data->average = 0;
}

static int impl_a_process(struct framework_object *obj, int input)
{
    /* [KEY] PRIVATE DATA RECOVERY from inline storage */
    struct impl_a_data *data = get_private_storage(obj);
    
    /* Circular buffer for moving average */
    if (data->sample_count < 10) {
        data->samples[data->sample_count++] = input;
        data->sum += input;
    } else {
        /* Remove oldest, add newest */
        data->sum -= data->samples[data->next_index];
        data->samples[data->next_index] = input;
        data->sum += input;
        data->next_index = (data->next_index + 1) % 10;
    }
    
    data->average = data->sum / data->sample_count;
    
    printf("[Impl A] Added sample %d, count=%d, sum=%d, avg=%d\n",
           input, data->sample_count, data->sum, data->average);
    
    return data->average;
}

static void impl_a_get_info(struct framework_object *obj)
{
    struct impl_a_data *data = get_private_storage(obj);
    printf("[Impl A] Moving average: samples=%d, average=%d\n",
           data->sample_count, data->average);
}

static const struct framework_ops impl_a_ops = {
    .init = impl_a_init,
    .process = impl_a_process,
    .get_info = impl_a_get_info,
    /* No cleanup needed - inline storage freed with object */
};

/* ========== Implementation B: Exponential Backoff Timer ========== */

/* [KEY] Implementation B's private data - completely different structure */
struct impl_b_data {
    int base_delay;        /* 4 bytes */
    int current_delay;     /* 4 bytes */
    int max_delay;         /* 4 bytes */
    int attempt_count;     /* 4 bytes */
    double multiplier;     /* 8 bytes */
    char status[32];       /* 32 bytes */
    /* Total: 56 bytes, fits in 64 bytes ✓ */
};

_Static_assert(sizeof(struct impl_b_data) <= PRIVATE_STORAGE_SIZE,
               "impl_b_data too large for inline storage");

static void impl_b_init(struct framework_object *obj)
{
    /* [KEY] Same inline storage, different interpretation */
    struct impl_b_data *data = get_private_storage(obj);
    
    printf("[Impl B] Initializing exponential backoff timer in inline storage\n");
    
    data->base_delay = 100;
    data->current_delay = 100;
    data->max_delay = 10000;
    data->attempt_count = 0;
    data->multiplier = 2.0;
    strcpy(data->status, "initialized");
}

static int impl_b_process(struct framework_object *obj, int input)
{
    struct impl_b_data *data = get_private_storage(obj);
    
    if (input == 0) {
        /* Success - reset backoff */
        data->current_delay = data->base_delay;
        data->attempt_count = 0;
        strcpy(data->status, "success-reset");
    } else {
        /* Failure - increase backoff */
        data->attempt_count++;
        int new_delay = (int)(data->current_delay * data->multiplier);
        if (new_delay > data->max_delay) {
            new_delay = data->max_delay;
        }
        data->current_delay = new_delay;
        snprintf(data->status, sizeof(data->status), 
                 "backoff-attempt-%d", data->attempt_count);
    }
    
    printf("[Impl B] %s: delay=%dms, attempts=%d\n",
           data->status, data->current_delay, data->attempt_count);
    
    return data->current_delay;
}

static void impl_b_get_info(struct framework_object *obj)
{
    struct impl_b_data *data = get_private_storage(obj);
    printf("[Impl B] Backoff timer: delay=%dms, max=%dms, attempts=%d\n",
           data->current_delay, data->max_delay, data->attempt_count);
}

static const struct framework_ops impl_b_ops = {
    .init = impl_b_init,
    .process = impl_b_process,
    .get_info = impl_b_get_info,
};

/* ========== Demonstration ========== */

int main(void)
{
    printf("=== Strategy 3: Inline Fixed-Size Storage Demo ===\n\n");
    
    printf("Size Analysis:\n");
    printf("  PRIVATE_STORAGE_SIZE = %d bytes\n", PRIVATE_STORAGE_SIZE);
    printf("  sizeof(struct impl_a_data) = %zu bytes (moving average)\n", 
           sizeof(struct impl_a_data));
    printf("  sizeof(struct impl_b_data) = %zu bytes (backoff timer)\n", 
           sizeof(struct impl_b_data));
    printf("  Both fit in inline storage: ✓\n\n");
    
    /*
     * VISUAL: How inline storage is reused:
     * 
     * Object A:                          Object B:
     * ┌──────────────────────────┐       ┌──────────────────────────┐
     * │ id=1, name="ObjA"        │       │ id=2, name="ObjB"        │
     * │ ops = &impl_a_ops        │       │ ops = &impl_b_ops        │
     * │ ┌────────────────────┐   │       │ ┌────────────────────┐   │
     * │ │ private_storage:   │   │       │ │ private_storage:   │   │
     * │ │   samples[10]      │   │       │ │   base_delay       │   │
     * │ │   sample_count     │   │       │ │   current_delay    │   │
     * │ │   next_index       │   │       │ │   max_delay        │   │
     * │ │   sum              │   │       │ │   attempt_count    │   │
     * │ │   average          │   │       │ │   multiplier       │   │
     * │ │                    │   │       │ │   status[32]       │   │
     * │ └────────────────────┘   │       │ └────────────────────┘   │
     * └──────────────────────────┘       └──────────────────────────┘
     * 
     * Same inline storage, different interpretations!
     */
    
    /* Create and initialize objects */
    struct framework_object *obj_a = framework_alloc(1, "MovingAvg");
    obj_a->ops = &impl_a_ops;
    framework_init(obj_a);
    
    struct framework_object *obj_b = framework_alloc(2, "BackoffTimer");
    obj_b->ops = &impl_b_ops;
    framework_init(obj_b);
    
    printf("\nPointer Analysis (inline storage is INSIDE object):\n");
    printf("  obj_a                    = %p\n", (void*)obj_a);
    printf("  &obj_a->private_storage  = %p\n", (void*)obj_a->private_storage);
    printf("  Offset from object start = %td bytes\n",
           (char*)obj_a->private_storage - (char*)obj_a);
    printf("  (Private storage is INSIDE the object, no separate alloc!)\n\n");
    
    /* Process Object A: moving average */
    printf("=== Processing Object A (moving average) ===\n");
    int samples[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        framework_process(obj_a, samples[i]);
    }
    obj_a->ops->get_info(obj_a);
    printf("\n");
    
    /* Process Object B: backoff timer */
    printf("=== Processing Object B (backoff timer) ===\n");
    framework_process(obj_b, 1);  /* Failure */
    framework_process(obj_b, 1);  /* Failure */
    framework_process(obj_b, 1);  /* Failure */
    framework_process(obj_b, 0);  /* Success - reset */
    framework_process(obj_b, 1);  /* Failure */
    obj_b->ops->get_info(obj_b);
    printf("\n");
    
    /* Cleanup */
    printf("=== Cleanup ===\n");
    framework_free(obj_a);  /* Inline storage freed automatically */
    framework_free(obj_b);  /* No separate free() needed */
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}

/*
 * EXPECTED OUTPUT:
 * 
 * === Strategy 3: Inline Fixed-Size Storage Demo ===
 * 
 * Size Analysis:
 *   PRIVATE_STORAGE_SIZE = 64 bytes
 *   sizeof(struct impl_a_data) = 56 bytes (moving average)
 *   sizeof(struct impl_b_data) = 56 bytes (backoff timer)
 *   Both fit in inline storage: ✓
 * 
 * [Impl A] Initializing moving average calculator in inline storage
 * [Impl B] Initializing exponential backoff timer in inline storage
 * 
 * Pointer Analysis (inline storage is INSIDE object):
 *   obj_a                    = 0x55a123456780
 *   &obj_a->private_storage  = 0x55a123456798
 *   Offset from object start = 24 bytes
 *   (Private storage is INSIDE the object, no separate alloc!)
 * 
 * === Processing Object A (moving average) ===
 * [Framework] Processing 'MovingAvg' with input=10
 * [Impl A] Added sample 10, count=1, sum=10, avg=10
 * [Framework] Processing 'MovingAvg' with input=20
 * [Impl A] Added sample 20, count=2, sum=30, avg=15
 * [Framework] Processing 'MovingAvg' with input=30
 * [Impl A] Added sample 30, count=3, sum=60, avg=20
 * [Framework] Processing 'MovingAvg' with input=40
 * [Impl A] Added sample 40, count=4, sum=100, avg=25
 * [Framework] Processing 'MovingAvg' with input=50
 * [Impl A] Added sample 50, count=5, sum=150, avg=30
 * [Impl A] Moving average: samples=5, average=30
 * 
 * === Processing Object B (backoff timer) ===
 * [Framework] Processing 'BackoffTimer' with input=1
 * [Impl B] backoff-attempt-1: delay=200ms, attempts=1
 * [Framework] Processing 'BackoffTimer' with input=1
 * [Impl B] backoff-attempt-2: delay=400ms, attempts=2
 * [Framework] Processing 'BackoffTimer' with input=1
 * [Impl B] backoff-attempt-3: delay=800ms, attempts=3
 * [Framework] Processing 'BackoffTimer' with input=0
 * [Impl B] success-reset: delay=100ms, attempts=0
 * [Framework] Processing 'BackoffTimer' with input=1
 * [Impl B] backoff-attempt-1: delay=200ms, attempts=1
 * [Impl B] Backoff timer: delay=200ms, max=10000ms, attempts=1
 * 
 * === Cleanup ===
 * [Framework] Freeing object 'MovingAvg' (inline storage freed automatically)
 * [Framework] Freeing object 'BackoffTimer' (inline storage freed automatically)
 * 
 * === Demo Complete ===
 */
```

**说明 (Strategy 3):**
- 框架对象内部包含固定大小的存储空间
- 实现将自己的结构体"叠加"在这个空间上
- 无额外内存分配，无指针追踪
- 优点：最高效率，无分配开销
- 缺点：私有数据大小受限，必须适合预定义的空间

---

### Strategy Comparison Summary

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     PRIVATE DATA STRATEGIES COMPARISON                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  STRATEGY 1: Embedding + container_of                                       │
│  ┌─────────────────────────────────────┐                                    │
│  │ struct impl_data                    │  Single allocation                 │
│  │   impl_field1                       │  container_of() to recover         │
│  │   impl_field2                       │  Framework calls impl's alloc      │
│  │   ┌───────────────────────────────┐ │                                    │
│  │   │ struct framework_obj (embed)  │ │ ← Framework sees this              │
│  │   └───────────────────────────────┘ │                                    │
│  └─────────────────────────────────────┘                                    │
│                                                                             │
│  STRATEGY 2: Private Pointer Field                                          │
│  ┌───────────────────────────┐    ┌───────────────────┐                     │
│  │ struct framework_obj      │    │ struct impl_data  │  Two allocations    │
│  │   ...                     │    │   impl_field1     │  Simple pointer     │
│  │   void *private ──────────┼───►│   impl_field2     │  dereference        │
│  │   ...                     │    └───────────────────┘                     │
│  └───────────────────────────┘                                              │
│                                                                             │
│  STRATEGY 3: Inline Fixed-Size Storage                                      │
│  ┌───────────────────────────────────────┐                                  │
│  │ struct framework_obj                  │  Single allocation               │
│  │   ...                                 │  Cast to impl type               │
│  │   ┌─────────────────────────────────┐ │  Size constrained                │
│  │   │ uint8_t storage[SIZE]           │ │ ← Cast as impl_data              │
│  │   └─────────────────────────────────┘ │                                  │
│  └───────────────────────────────────────┘                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

| Aspect | Strategy 1: Embedding | Strategy 2: Pointer | Strategy 3: Inline |
|--------|----------------------|--------------------|--------------------|
| **Memory allocations** | 1 (impl struct) | 2 (framework + impl) | 1 (framework) |
| **Recovery method** | `container_of` | Pointer dereference | Cast |
| **Framework allocator** | Must call impl's | Can use its own | Can use its own |
| **Private data size** | Unlimited | Unlimited | Fixed maximum |
| **Cache locality** | Excellent | Poor (two allocations) | Excellent |
| **Cleanup needed** | Yes (impl frees) | Yes (impl frees private) | No (auto-freed) |
| **Kernel examples** | `ext4_inode_info` | `dentry.d_fsdata` | `icsk_ca_priv[]` |

**说明:**
- **策略 1** 最常用，适合私有数据较大且框架愿意调用实现的分配函数
- **策略 2** 适合框架已经自己分配对象，实现需要后续附加私有数据
- **策略 3** 最高效，适合私有数据较小且大小可预测的场景

---

### Why Passing `xxx` Back is Critical

```c
/* WITHOUT passing xxx back: */
ssize_t bad_read(char __user *buf, size_t count)
{
    /* PROBLEM: No way to know WHICH file! */
    /* No way to access private data! */
    /* No way to update file position! */
}

/* WITH passing xxx back: */
ssize_t ext4_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    /* file parameter enables: */
    
    /* 1. Access to generic framework state */
    struct inode *inode = file->f_inode;
    
    /* 2. Recovery of private data */
    struct ext4_inode_info *ei = EXT4_I(inode);
    
    /* 3. Access to other framework objects */
    struct super_block *sb = inode->i_sb;
    struct ext4_sb_info *sbi = EXT4_SB(sb);
    
    /* 4. Modification of framework state */
    *pos += bytes_read;  /* Update position */
}
```

### Lifecycle and Ownership Rules

```
OWNERSHIP CHAIN:

Framework creates:          Implementation initializes:
      │                              │
      ▼                              ▼
struct file *f = alloc()    f->f_op = &ext4_fops
      │                     f->f_inode->i_private = ext4_data
      │                              │
      ▼                              ▼
Framework manages lifecycle  Implementation uses during ops
      │                              │
      ▼                              ▼
Framework destroys:          Implementation cleans up:
file_free(f)                 ext4_fops.release(inode, file)
```

**说明:**
- 框架拥有对象生命周期（创建和销毁）
- 实现拥有私有数据初始化和清理
- 实现通过回调（如 `.release`）获得清理机会

---

## Step 5 — VFS Subsystem Examples

This section presents five distinct VFS examples, each illustrating a different framework object and operations table combination. Each example includes detailed line-by-line analysis with comments on key lines related to the ops pattern.

### Example 1: `struct file` → `f_op` → `read()` — File I/O Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: User Space ========== */
/* User calls: read(fd, buf, count) */
/* System call entry point in fs/read_write.c */

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
    struct file *file;
    /* ... fd lookup ... */
    
    /* [KEY] Call into VFS layer with file object */
    ret = vfs_read(file, buf, count, &pos);
}

/* ========== LAYER 2: VFS Core ========== */
/* fs/read_write.c:378-393 */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    /* [KEY] Security check - VFS owns policy decisions */
    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        
        /* [KEY] Check if ops table has 'read' function pointer */
        if (file->f_op->read)
            /* [KEY] THE OPS PATTERN: xxx->ops->yyy(xxx, ...) */
            /* file->f_op points to filesystem's operations table */
            /* ->read is the function pointer for read operation */
            /* file is passed BACK to the implementation */
            ret = file->f_op->read(file, buf, count, pos);
        else
            /* [KEY] Fallback if read is NULL */
            ret = do_sync_read(file, buf, count, pos);
            
        /* [KEY] VFS owns post-operation handling */
        if (ret > 0) {
            fsnotify_access(file);  /* Notification - impl doesn't know */
            add_rchar(current, ret);/* Accounting - impl doesn't know */
        }
    }
    return ret;
}

/* ========== LAYER 3: Filesystem Implementation ========== */
/* fs/ext4/file.c (simplified) */
static ssize_t ext4_file_read(struct file *file, char __user *buf,
                               size_t count, loff_t *pos)
{
    /* [KEY] PRIVATE DATA RECOVERY - the critical step */
    /* file->f_inode is the generic inode */
    struct inode *inode = file->f_inode;
    
    /* [KEY] container_of magic - recover ext4-specific data */
    /* EXT4_I() = container_of(inode, struct ext4_inode_info, vfs_inode) */
    struct ext4_inode_info *ei = EXT4_I(inode);
    
    /* [KEY] Now we have access to ext4's private fields */
    /* ei->i_data[] - block pointers (ext4-specific) */
    /* ei->i_flags - ext4 inode flags */
    /* ei->i_disksize - on-disk size */
    
    /* ext4 can now perform its specific read logic */
    if (ei->i_flags & EXT4_EXTENTS_FL) {
        /* Handle extent-based file - ext4-specific algorithm */
    }
    
    /* [KEY] Return value follows contract: bytes read or -errno */
    return generic_file_aio_read(...);
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* fs/ext4/file.c - static definition at module level */
const struct file_operations ext4_file_operations = {
    .owner      = THIS_MODULE,  /* [KEY] Module reference counting */
    .llseek     = ext4_llseek,
    .read       = ext4_file_read,    /* [KEY] Points to implementation */
    .write      = ext4_file_write,
    .mmap       = generic_file_mmap,
    .open       = ext4_file_open,
    .release    = ext4_release_file,
    .fsync      = ext4_sync_file,
    /* ... */
};
```

**说明:**
- 第1层：用户空间调用 `read()` 系统调用
- 第2层：VFS 检查权限，通过 `f_op->read` 调用文件系统实现
- 第3层：ext4 通过 `EXT4_I()` 恢复私有数据，执行具体读取逻辑
- 关键点：`file` 作为参数传回实现，使实现能够恢复上下文

**Boundary enforced:** VFS (framework) ←→ Filesystem (implementation)

---

### Example 2: `struct inode` → `i_op` → `lookup()` — Directory Resolution Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Path Resolution Entry ========== */
/* fs/namei.c - Walking path "/home/user/file.txt" */

static int do_lookup(struct nameidata *nd, struct qstr *name,
                     struct path *path, struct inode **inode)
{
    struct dentry *parent = nd->path.dentry;
    struct inode *dir = parent->d_inode;  /* Directory inode */
    
    /* [KEY] First, try dcache lookup (no ops call needed) */
    struct dentry *dentry = d_lookup(parent, name);
    
    if (!dentry) {
        /* [KEY] Cache miss - must ask filesystem */
        dentry = real_lookup(parent, name, nd);
    }
}

/* ========== LAYER 2: VFS Lookup Delegation ========== */
/* fs/namei.c:437 */
static struct dentry *real_lookup(struct dentry *parent,
                                   struct qstr *name,
                                   struct nameidata *nd)
{
    struct inode *dir = parent->d_inode;
    struct dentry *dentry;
    
    /* [KEY] Allocate new dentry in VFS layer */
    dentry = d_alloc(parent, name);  /* Framework allocates */
    
    /* [KEY] THE OPS PATTERN: inode->i_op->lookup(inode, dentry, nd) */
    /* - inode is the framework object (directory inode) */
    /* - i_op points to filesystem's inode operations table */
    /* - lookup is the operation function pointer */
    /* - inode passed BACK so impl can recover private data */
    result = dir->i_op->lookup(dir, dentry, nd);
    /*     ^^^                 ^^^
     *      |                   |
     *      |                   +-- Same inode passed to implementation
     *      +-- Implementation function pointer
     */
    
    return dentry;
}

/* ========== LAYER 3: Filesystem Implementation ========== */
/* fs/ext4/namei.c (simplified) */
static struct dentry *ext4_lookup(struct inode *dir,    /* Framework obj */
                                   struct dentry *dentry,
                                   struct nameidata *nd)
{
    struct inode *inode = NULL;
    struct ext4_dir_entry_2 *de;
    struct buffer_head *bh;
    
    /* [KEY] PRIVATE DATA RECOVERY */
    /* dir is generic inode, recover ext4-specific structure */
    struct ext4_inode_info *ei = EXT4_I(dir);
    
    /* [KEY] Access ext4-specific directory format */
    /* ext4_find_entry uses ei->i_data[] for block pointers */
    /* This format is completely hidden from VFS */
    bh = ext4_find_entry(dir, &dentry->d_name, &de);
    
    if (bh) {
        /* [KEY] Found entry - get inode number from ext4 format */
        unsigned long ino = le32_to_cpu(de->inode);
        
        /* [KEY] Ask VFS to get/create inode (framework manages cache) */
        inode = ext4_iget(dir->i_sb, ino);
    }
    
    /* [KEY] d_splice_alias connects dentry to inode */
    /* This is VFS helper - impl doesn't manage dentry lifecycle */
    return d_splice_alias(inode, dentry);
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* fs/ext4/namei.c */
const struct inode_operations ext4_dir_inode_operations = {
    .create     = ext4_create,
    .lookup     = ext4_lookup,     /* [KEY] Directory lookup */
    .link       = ext4_link,
    .unlink     = ext4_unlink,
    .symlink    = ext4_symlink,
    .mkdir      = ext4_mkdir,
    .rmdir      = ext4_rmdir,
    .mknod      = ext4_mknod,
    .rename     = ext4_rename,
    /* ... */
};
```

**说明:**
- `lookup` 操作将路径分量（如 "file.txt"）转换为 inode
- VFS 管理 dentry 缓存，只在缓存未命中时调用文件系统
- ext4 通过 `EXT4_I()` 恢复私有数据，读取 ext4 特定的目录格式
- VFS 完全不知道 ext4 的目录项格式（`ext4_dir_entry_2`）

**Boundary enforced:** VFS path resolution ←→ Filesystem directory format

---

### Example 3: `struct super_block` → `s_op` → `alloc_inode()` — Inode Allocation Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: VFS Inode Request ========== */
/* fs/inode.c:200-225 - VFS needs a new inode */

struct inode *new_inode(struct super_block *sb)
{
    struct inode *inode;
    
    /* [KEY] VFS calls internal allocation function */
    inode = alloc_inode(sb);
    /* ... initialize generic fields ... */
    return inode;
}

/* ========== LAYER 2: Dispatch to Filesystem ========== */
/* fs/inode.c:209-225 */
static struct inode *alloc_inode(struct super_block *sb)
{
    struct inode *inode;

    /* [KEY] THE OPS PATTERN: sb->s_op->alloc_inode(sb) */
    /* Check if filesystem provides custom allocator */
    if (sb->s_op->alloc_inode)
        /* [KEY] Filesystem allocates larger structure with embedded inode */
        /* sb passed back so impl can access sb->s_fs_info (private data) */
        inode = sb->s_op->alloc_inode(sb);
    else
        /* [KEY] Fallback: allocate bare inode from VFS cache */
        /* Only used by simple filesystems without private inode data */
        inode = kmem_cache_alloc(inode_cachep, GFP_KERNEL);

    if (!inode)
        return NULL;

    /* [KEY] VFS initializes GENERIC fields - impl already set its private */
    if (unlikely(inode_init_always(sb, inode))) {
        /* [KEY] Cleanup on failure - respects same boundary */
        if (inode->i_sb->s_op->destroy_inode)
            inode->i_sb->s_op->destroy_inode(inode);
        else
            kmem_cache_free(inode_cachep, inode);
        return NULL;
    }
    return inode;
}

/* ========== LAYER 3: Filesystem Implementation ========== */
/* fs/ext4/super.c (simplified) */
static struct inode *ext4_alloc_inode(struct super_block *sb)
{
    struct ext4_inode_info *ei;
    
    /* [KEY] Allocate LARGER structure that embeds generic inode */
    /* ext4_inode_cachep was created with correct size at module init */
    ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);
    if (!ei)
        return NULL;

    /* [KEY] Initialize ext4-PRIVATE fields */
    /* VFS knows nothing about these fields */
    ei->i_block_group = 0;
    ei->i_flags = 0;
    ei->i_file_acl = 0;
    ei->i_dtime = 0;
    ei->i_reserved_data_blocks = 0;
    /* ... many more ext4-specific initializations ... */

    /* [KEY] Return pointer to EMBEDDED generic inode */
    /* VFS will work with &ei->vfs_inode */
    /* Later, ext4 recovers ei via container_of */
    return &ei->vfs_inode;
    /*     ^^^^^^^^^^^^^^
     *         |
     *         +-- VFS only sees this part
     *             ext4's private data is "invisible" before this address
     */
}

/* [KEY] Memory layout visualization */
/*
 * +----------------------------------------+
 * | struct ext4_inode_info                 |  <- ei points here
 * |   i_data[15]        (ext4 private)     |
 * |   i_flags           (ext4 private)     |
 * |   i_file_acl        (ext4 private)     |
 * |   i_block_group     (ext4 private)     |
 * |   ... more ext4 fields ...             |
 * |   +--------------------------------+   |
 * |   | struct inode vfs_inode         |   |  <- VFS sees this
 * |   |   i_mode, i_uid, i_gid, ...    |   |
 * |   +--------------------------------+   |
 * +----------------------------------------+
 */

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* fs/ext4/super.c */
static const struct super_operations ext4_sops = {
    .alloc_inode    = ext4_alloc_inode,  /* [KEY] Custom allocation */
    .destroy_inode  = ext4_destroy_inode,/* [KEY] Matching deallocation */
    .write_inode    = ext4_write_inode,
    .dirty_inode    = ext4_dirty_inode,
    .drop_inode     = ext4_drop_inode,
    .evict_inode    = ext4_evict_inode,
    .put_super      = ext4_put_super,
    .sync_fs        = ext4_sync_fs,
    /* ... */
};
```

**说明:**
- `alloc_inode` 是私有数据嵌入模式的关键
- 文件系统分配更大的结构体（`ext4_inode_info`），其中嵌入通用 `inode`
- VFS 只看到返回的 `&ei->vfs_inode`
- 之后通过 `EXT4_I(inode)` 即 `container_of` 恢复完整的 `ext4_inode_info`

**Boundary enforced:** VFS inode lifecycle ←→ Filesystem private data layout

---

### Example 4: `struct dentry` → `d_op` → `d_revalidate()` — Cache Validity Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Path Walk with Cached Dentry ========== */
/* fs/namei.c - Walking path, found dentry in cache */

static int do_lookup(struct nameidata *nd, struct qstr *name,
                     struct path *path, struct inode **inode)
{
    struct dentry *dentry;
    
    /* [KEY] Found dentry in dcache */
    dentry = d_lookup(nd->path.dentry, name);
    
    if (dentry && dentry->d_op && dentry->d_op->d_revalidate) {
        /* [KEY] Filesystem wants to verify cached entry is still valid */
        /* Critical for network filesystems where remote may have changed */
        
        /* Call wrapper function */
        status = d_revalidate(dentry, nd);
        if (!status) {
            /* [KEY] Cache invalid - must perform real lookup */
            d_invalidate(dentry);
            dentry = NULL;
        }
    }
}

/* ========== LAYER 2: VFS Revalidation Dispatch ========== */
/* fs/namei.c:496-499 */
static inline int d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
    /* [KEY] THE OPS PATTERN: dentry->d_op->d_revalidate(dentry, nd) */
    /* - dentry is the framework object (cached directory entry) */
    /* - d_op points to filesystem's dentry operations table */
    /* - d_revalidate is the validation function pointer */
    /* - dentry passed back for context and private data access */
    return dentry->d_op->d_revalidate(dentry, nd);
    /*           ^^^^^^              ^^^^^^^
     *              |                    |
     *              |                    +-- Same dentry passed to impl
     *              +-- Filesystem's validation function
     */
}

/* ========== LAYER 3: Network Filesystem Implementation ========== */
/* fs/nfs/dir.c (simplified) */
static int nfs_lookup_revalidate(struct dentry *dentry, struct nameidata *nd)
{
    struct inode *dir = dentry->d_parent->d_inode;
    struct inode *inode = dentry->d_inode;
    
    /* [KEY] PRIVATE DATA RECOVERY via d_fsdata */
    /* NFS stores file handle and attributes in dentry's private pointer */
    struct nfs_fh *fh = NFS_FH(inode);
    struct nfs_fattr *fattr = NFS_I(inode)->fattr;
    
    /* [KEY] Check if our cached version matches server */
    /* This is network-specific - VFS has no concept of "server version" */
    error = NFS_PROTO(dir)->lookup(dir, &dentry->d_name, fh, fattr);
    if (error) {
        /* Server says file doesn't exist or changed */
        return 0;  /* [KEY] Return 0 = invalid, VFS will re-lookup */
    }
    
    /* [KEY] Compare cached attributes with server response */
    if (nfs_compare_fh(NFS_FH(inode), fh) != 0) {
        /* File handle changed - different file now! */
        return 0;  /* Invalid */
    }
    
    if (nfs_fattr_differs(NFS_I(inode)->fattr, fattr)) {
        /* Attributes changed - update cache */
        nfs_refresh_inode(inode, fattr);
    }
    
    return 1;  /* [KEY] Return 1 = valid, VFS can use cached dentry */
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* fs/nfs/dir.c */
const struct dentry_operations nfs_dentry_operations = {
    .d_revalidate   = nfs_lookup_revalidate,  /* [KEY] Remote validation */
    .d_delete       = nfs_dentry_delete,
    .d_iput         = nfs_dentry_iput,
    .d_automount    = nfs_d_automount,
    .d_release      = nfs_d_release,
};

/* [KEY] Compare with local filesystem that doesn't need revalidation */
/* fs/ext4/dir.c - ext4 doesn't define d_revalidate */
/* Local disk doesn't change behind our back, so no need to revalidate */
```

**说明:**
- `d_revalidate` 是网络文件系统的关键操作
- NFS 必须验证缓存的 dentry 是否与远程服务器一致
- 本地文件系统（如 ext4）通常不定义此操作（磁盘不会"背后"改变）
- 返回值：1 = 有效，0 = 无效（VFS 将重新查找）

**Boundary enforced:** VFS dentry cache ←→ Remote filesystem consistency

---

### Example 5: `struct address_space` → `a_ops` → `readpage()` — Page Cache I/O Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Generic File Read Requesting Page ========== */
/* mm/filemap.c - Page fault or read() needing file data */

static int do_generic_file_read(struct file *filp, loff_t *ppos,
                                 read_descriptor_t *desc,
                                 read_actor_t actor)
{
    struct address_space *mapping = filp->f_mapping;
    struct page *page;
    pgoff_t index = *ppos >> PAGE_CACHE_SHIFT;
    
    /* [KEY] Try to find page in cache */
    page = find_get_page(mapping, index);
    
    if (!page) {
        /* [KEY] Page not in cache - need to read from filesystem */
        page = page_cache_alloc_cold(mapping);
        error = add_to_page_cache_lru(page, mapping, index, GFP_KERNEL);
        
        /* [KEY] Ask filesystem to fill the page */
        goto readpage;
    }

readpage:
    /* [KEY] Dispatch to filesystem's readpage implementation */
    error = mapping->a_ops->readpage(filp, page);
}

/* ========== LAYER 2: Address Space Operations Dispatch ========== */
/* mm/filemap.c:1100 */
/* The call: mapping->a_ops->readpage(filp, page) */
/*
 * [KEY] THE OPS PATTERN BREAKDOWN:
 * - mapping is the address_space (page cache for this file)
 * - a_ops points to filesystem's address_space_operations
 * - readpage is the function pointer to read one page
 * - filp and page are passed to implementation
 * 
 * Note: This slightly differs from typical pattern:
 * - mapping is not passed directly (it's page->mapping)
 * - file is passed for credentials/context
 * - page contains back-reference to mapping
 */

/* ========== LAYER 3: Filesystem Implementation ========== */
/* fs/ext4/inode.c (simplified) */
static int ext4_readpage(struct file *file, struct page *page)
{
    /* [KEY] Page contains reference back to mapping and inode */
    struct address_space *mapping = page->mapping;
    struct inode *inode = mapping->host;  /* inode that owns this mapping */
    
    /* [KEY] PRIVATE DATA RECOVERY */
    struct ext4_inode_info *ei = EXT4_I(inode);
    
    /* [KEY] Calculate which disk blocks to read */
    /* Uses ext4-specific block mapping (extent tree or indirect blocks) */
    sector_t block = page->index << (PAGE_CACHE_SHIFT - inode->i_blkbits);
    
    if (ei->i_flags & EXT4_EXTENTS_FL) {
        /* [KEY] ext4 extent tree lookup - completely hidden from VFS */
        ext4_map_blocks(handle, inode, &map, 0);
    } else {
        /* [KEY] Traditional indirect block lookup */
        ext4_get_block(inode, block, bh, 0);
    }
    
    /* [KEY] Submit I/O to block layer */
    /* Framework pattern continues: block layer has its own ops */
    return mpage_readpage(page, ext4_get_block);
}

/* [KEY] The get_block function - another ops-like callback */
static int ext4_get_block(struct inode *inode, sector_t iblock,
                           struct buffer_head *bh, int create)
{
    struct ext4_inode_info *ei = EXT4_I(inode);
    
    /* [KEY] Map logical block to physical block */
    /* Uses ei->i_data[] for indirect blocks or extent tree */
    ext4_map_blocks(NULL, inode, &map, flags);
    
    /* [KEY] Tell block layer where data is on disk */
    map_bh(bh, inode->i_sb, map.m_pblk);
    return 0;
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* fs/ext4/inode.c */
static const struct address_space_operations ext4_aops = {
    .readpage       = ext4_readpage,       /* [KEY] Read one page */
    .readpages      = ext4_readpages,      /* [KEY] Read multiple pages */
    .writepage      = ext4_writepage,      /* [KEY] Write one page */
    .writepages     = ext4_writepages,     /* [KEY] Write multiple pages */
    .write_begin    = ext4_write_begin,    /* [KEY] Prepare for write */
    .write_end      = ext4_write_end,      /* [KEY] Finish write */
    .bmap           = ext4_bmap,           /* [KEY] Logical->physical block */
    .invalidatepage = ext4_invalidatepage,
    .releasepage    = ext4_releasepage,
    .direct_IO      = ext4_direct_IO,      /* [KEY] Bypass page cache */
    /* ... */
};
```

**说明:**
- `readpage` 是页面缓存和文件系统之间的边界
- 页面缓存管理内存页，但不知道数据在磁盘上的位置
- 文件系统通过块映射（extent 树或间接块）将逻辑偏移转换为物理块
- 这展示了 ops 模式的层叠：VFS → Page Cache → Filesystem → Block Layer

**Boundary enforced:** Page cache memory management ←→ Filesystem block mapping

---

### VFS Examples Summary Table

| # | Framework Object | Ops Table | Operation | Private Data Recovery | Boundary Description |
|---|------------------|-----------|-----------|----------------------|---------------------|
| 1 | `struct file` | `f_op` | `read()` | `EXT4_I(file->f_inode)` | VFS I/O dispatch ←→ FS data access |
| 2 | `struct inode` | `i_op` | `lookup()` | `EXT4_I(dir)` | Path resolution ←→ Directory format |
| 3 | `struct super_block` | `s_op` | `alloc_inode()` | Embedded + `container_of` | Inode lifecycle ←→ Private data layout |
| 4 | `struct dentry` | `d_op` | `d_revalidate()` | `dentry->d_fsdata` | Cache policy ←→ Remote consistency |
| 5 | `struct address_space` | `a_ops` | `readpage()` | `EXT4_I(mapping->host)` | Page cache ←→ Block mapping |

---

## Step 6 — TCP/IP Network Subsystem Examples

This section presents five distinct networking examples, each illustrating a different framework object and operations table combination. Each example includes detailed line-by-line analysis with comments on key lines related to the ops pattern.

### Example 1: `struct socket` → `ops` → `sendmsg()` — BSD Socket/Protocol Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: System Call Entry ========== */
/* net/socket.c - User calls sendmsg() */

SYSCALL_DEFINE3(sendmsg, int, fd, struct msghdr __user *, msg, unsigned, flags)
{
    struct socket *sock;
    
    /* [KEY] Get socket from file descriptor */
    sock = sockfd_lookup_light(fd, &err, &fput_needed);
    
    /* [KEY] Call into socket layer */
    err = __sys_sendmsg(sock, msg, flags);
}

/* ========== LAYER 2: Socket Layer Dispatch ========== */
/* net/socket.c:552-558 */
static inline int __sock_sendmsg_nosec(struct kiocb *iocb, struct socket *sock,
                                        struct msghdr *msg, size_t size)
{
    struct sock_iocb *si = kiocb_to_siocb(iocb);

    /* [KEY] Store context for async I/O */
    si->sock = sock;
    si->scm = NULL;
    si->msg = msg;
    si->size = size;

    /* [KEY] THE OPS PATTERN: sock->ops->sendmsg(iocb, sock, msg, size) */
    /* - sock is the framework object (BSD-level socket) */
    /* - ops points to protocol family's operations table */
    /* - sendmsg is the operation to send a message */
    /* - sock passed back for protocol to access internal state */
    return sock->ops->sendmsg(iocb, sock, msg, size);
    /*        ^^^^                 ^^^^
     *         |                    |
     *         |                    +-- Same socket passed to protocol
     *         +-- Protocol-specific sendmsg (e.g., inet_sendmsg)
     */
}

/* ========== LAYER 3: Protocol Family Implementation ========== */
/* net/ipv4/af_inet.c */
int inet_sendmsg(struct kiocb *iocb, struct socket *sock,
                  struct msghdr *msg, size_t size)
{
    struct sock *sk = sock->sk;  /* [KEY] Get internal socket */
    
    /* [KEY] Validate socket is connected for SOCK_STREAM */
    if (!inet_sk(sk)->inet_num && 
        inet_autobind(sk))
        return -EAGAIN;

    /* [KEY] ANOTHER OPS DISPATCH - to transport protocol */
    /* This shows layering: proto_ops -> proto */
    return sk->sk_prot->sendmsg(iocb, sk, msg, size);
    /*         ^^^^^^^                ^^
     *            |                   |
     *            |                   +-- Internal socket passed down
     *            +-- TCP's or UDP's sendmsg
     */
}

/* ========== LAYER 4: Transport Protocol Implementation ========== */
/* net/ipv4/tcp.c */
int tcp_sendmsg(struct kiocb *iocb, struct sock *sk,
                struct msghdr *msg, size_t size)
{
    /* [KEY] PRIVATE DATA RECOVERY */
    /* tcp_sk() is a cast: sock at offset 0 in hierarchy */
    struct tcp_sock *tp = tcp_sk(sk);
    /*
     * Memory layout (first-member embedding):
     * struct tcp_sock {
     *     struct inet_connection_sock inet_conn;  // offset 0
     *         struct inet_sock icsk_inet;         // offset 0
     *             struct sock sk;                 // offset 0
     *     // TCP-specific fields follow
     *     u16 tcp_header_len;
     *     u32 rcv_nxt;
     *     u32 snd_nxt;
     *     u32 snd_una;
     *     ...
     * };
     */
    
    /* [KEY] Access TCP-specific state */
    int mss_now = tcp_current_mss(sk);  /* Uses tp->mss_cache */
    
    while (msg_data_left(msg)) {
        /* [KEY] Allocate sk_buff from socket's write queue */
        skb = sk_stream_alloc_skb(sk, select_size(sk, sg), 
                                   sk->sk_allocation);
        
        /* [KEY] Copy user data into kernel buffer */
        err = skb_copy_to_page_nocache(sk, msg, skb, page, off, copy);
        
        /* [KEY] Update TCP sequence numbers (TCP-specific) */
        tp->write_seq += copy;
        TCP_SKB_CB(skb)->end_seq += copy;
        
        /* [KEY] Push data if needed */
        if (forced_push(tp))
            tcp_mark_push(tp, skb);
    }
    
    /* [KEY] Trigger actual transmission */
    tcp_push(sk, flags, mss_now, tp->nonagle);
    
    return copied;
}

/* ========== OPERATIONS TABLE DEFINITIONS ========== */
/* net/ipv4/af_inet.c - BSD socket level */
const struct proto_ops inet_stream_ops = {
    .family        = PF_INET,
    .owner         = THIS_MODULE,
    .release       = inet_release,
    .bind          = inet_bind,
    .connect       = inet_stream_connect,
    .socketpair    = sock_no_socketpair,
    .accept        = inet_accept,
    .getname       = inet_getname,
    .poll          = tcp_poll,
    .ioctl         = inet_ioctl,
    .listen        = inet_listen,
    .shutdown      = inet_shutdown,
    .setsockopt    = sock_common_setsockopt,
    .getsockopt    = sock_common_getsockopt,
    .sendmsg       = inet_sendmsg,    /* [KEY] Points to inet layer */
    .recvmsg       = inet_recvmsg,
    .mmap          = sock_no_mmap,
    .sendpage      = inet_sendpage,
};

/* net/ipv4/tcp_ipv4.c - Transport protocol level */
struct proto tcp_prot = {
    .name           = "TCP",
    .owner          = THIS_MODULE,
    .close          = tcp_close,
    .connect        = tcp_v4_connect,
    .disconnect     = tcp_disconnect,
    .accept         = inet_csk_accept,
    .ioctl          = tcp_ioctl,
    .init           = tcp_v4_init_sock,
    .destroy        = tcp_v4_destroy_sock,
    .shutdown       = tcp_shutdown,
    .setsockopt     = tcp_setsockopt,
    .getsockopt     = tcp_getsockopt,
    .recvmsg        = tcp_recvmsg,
    .sendmsg        = tcp_sendmsg,    /* [KEY] Points to TCP impl */
    .hash           = inet_hash,
    .unhash         = inet_unhash,
    /* ... */
};
```

**说明:**
- 网络栈展示了多层 ops 模式：`proto_ops` → `proto`
- BSD socket 层（`struct socket`）通过 `ops` 调用协议族（如 `inet_sendmsg`）
- 协议族通过 `sk_prot` 再调用具体传输协议（如 `tcp_sendmsg`）
- 私有数据恢复使用首成员嵌入：`tcp_sk()` 是指针强制转换

**Boundary enforced:** BSD socket API ←→ Protocol family ←→ Transport protocol

---

### Example 2: `struct net_device` → `netdev_ops` → `ndo_start_xmit()` — Network Stack/Driver Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Network Core Transmission ========== */
/* net/core/dev.c - Sending packet from stack to hardware */

int dev_queue_xmit(struct sk_buff *skb)
{
    struct net_device *dev = skb->dev;
    struct netdev_queue *txq;
    
    /* [KEY] Select transmit queue (multi-queue NICs) */
    txq = netdev_pick_tx(dev, skb);
    
    /* [KEY] Enter driver's transmit path */
    return dev_hard_start_xmit(skb, dev, txq);
}

/* ========== LAYER 2: Transmission Dispatch ========== */
/* net/core/dev.c:2210-2260 */
int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev,
                        struct netdev_queue *txq)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    int rc = NETDEV_TX_OK;
    unsigned int skb_len;

    /* [KEY] Features check and GSO handling */
    if (likely(!skb->next)) {
        /* Single packet path */
        if (dev->priv_flags & IFF_XMIT_DST_RELEASE)
            skb_dst_drop(skb);

        /* [KEY] Check for hardware features (checksum offload, etc.) */
        features = netif_skb_features(skb);
        if (netif_needs_gso(skb, features)) {
            skb = dev_gso_segment(skb, features);
        }

        skb_len = skb->len;
        
        /* [KEY] THE OPS PATTERN: ops->ndo_start_xmit(skb, dev) */
        /* - dev is the framework object (network device) */
        /* - netdev_ops points to driver's operations table */
        /* - ndo_start_xmit is the transmit function pointer */
        /* - dev passed back for driver to access private data */
        rc = ops->ndo_start_xmit(skb, dev);
        /*        ^^^^^^^^^^^^^^^     ^^^
         *              |              |
         *              |              +-- Same device passed to driver
         *              +-- Driver's transmit function (e.g., e1000_xmit_frame)
         */
        
        /* [KEY] Framework handles tracing - driver doesn't know */
        trace_net_dev_xmit(skb, rc, dev, skb_len);
        
        if (rc == NETDEV_TX_OK)
            txq_trans_update(txq);  /* Update queue timestamp */
        
        return rc;
    }
    
    /* [KEY] GSO path - multiple packets */
gso:
    do {
        struct sk_buff *nskb = skb->next;
        skb->next = NULL;
        
        skb_len = skb->len;
        
        /* [KEY] Same ops call for each GSO segment */
        rc = ops->ndo_start_xmit(skb, dev);
        
        trace_net_dev_xmit(skb, rc, dev, skb_len);
        
        /* [KEY] Handle transmit busy - driver returns NETDEV_TX_BUSY */
        if (unlikely(rc != NETDEV_TX_OK)) {
            /* Re-queue packets if driver can't accept */
            nskb->next = skb->next;
            skb->next = nskb;
            return rc;
        }
        
        skb = nskb;
    } while (skb);
    
    return rc;
}

/* ========== LAYER 3: Network Driver Implementation ========== */
/* drivers/net/e1000/e1000_main.c (simplified) */
static netdev_tx_t e1000_xmit_frame(struct sk_buff *skb,
                                     struct net_device *netdev)
{
    /* [KEY] PRIVATE DATA RECOVERY */
    /* netdev_priv() returns pointer to driver-private area */
    struct e1000_adapter *adapter = netdev_priv(netdev);
    /*
     * Memory layout:
     * +--------------------------------+
     * | struct net_device              |  <- netdev points here
     * |   name[IFNAMSIZ]               |
     * |   const struct net_device_ops *|
     * |   ...many fields...            |
     * +--------------------------------+
     * | Driver private data            |  <- netdev_priv() returns this
     * | struct e1000_adapter {         |
     * |   struct e1000_hw hw;          |
     * |   struct e1000_tx_ring *tx_ring|
     * |   ...                          |
     * | }                              |
     * +--------------------------------+
     */
    
    struct e1000_hw *hw = &adapter->hw;
    struct e1000_tx_ring *tx_ring = adapter->tx_ring;
    unsigned int first, max_per_txd;
    unsigned int nr_frags;
    
    /* [KEY] Check if queue has space */
    if (unlikely(e1000_maybe_stop_tx(netdev, tx_ring, count))) {
        /* [KEY] Return BUSY - framework will retry later */
        return NETDEV_TX_BUSY;
    }
    
    /* [KEY] Hardware-specific VLAN handling */
    if (adapter->vlgrp && vlan_tx_tag_present(skb)) {
        tx_flags |= E1000_TX_FLAGS_VLAN;
        tx_flags |= vlan_tx_tag_get(skb) << E1000_TX_FLAGS_VLAN_SHIFT;
    }
    
    /* [KEY] Setup DMA mapping - driver knows hardware requirements */
    first = tx_ring->next_to_use;
    
    /* [KEY] Handle TCP Segmentation Offload if supported */
    if (skb_is_gso(skb)) {
        tso = e1000_tso(adapter, tx_ring, skb);
    }
    
    /* [KEY] Map skb fragments to hardware descriptors */
    /* This is e1000-specific descriptor format */
    count = e1000_tx_map(adapter, tx_ring, skb, first, max_per_txd);
    
    /* [KEY] Program hardware to start transmission */
    /* Write to hardware registers - completely hardware-specific */
    e1000_tx_queue(adapter, tx_ring, tx_flags, count);
    
    /* [KEY] Ring doorbell - tell hardware to process descriptors */
    writel(tx_ring->next_to_use, hw->hw_addr + tx_ring->tdt);
    
    return NETDEV_TX_OK;
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* drivers/net/e1000/e1000_main.c */
static const struct net_device_ops e1000_netdev_ops = {
    .ndo_open               = e1000_open,
    .ndo_stop               = e1000_close,
    .ndo_start_xmit         = e1000_xmit_frame,  /* [KEY] Transmit */
    .ndo_get_stats          = e1000_get_stats,
    .ndo_set_rx_mode        = e1000_set_rx_mode,
    .ndo_set_mac_address    = e1000_set_mac,
    .ndo_tx_timeout         = e1000_tx_timeout,
    .ndo_change_mtu         = e1000_change_mtu,
    .ndo_do_ioctl           = e1000_ioctl,
    .ndo_validate_addr      = eth_validate_addr,
    .ndo_vlan_rx_add_vid    = e1000_vlan_rx_add_vid,
    .ndo_vlan_rx_kill_vid   = e1000_vlan_rx_kill_vid,
    /* ... */
};

/* [KEY] Registration during probe */
static int e1000_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct net_device *netdev;
    struct e1000_adapter *adapter;
    
    /* [KEY] Allocate net_device + private data together */
    netdev = alloc_etherdev(sizeof(struct e1000_adapter));
    
    /* [KEY] Set operations table */
    netdev->netdev_ops = &e1000_netdev_ops;
    
    adapter = netdev_priv(netdev);
    adapter->netdev = netdev;
    adapter->pdev = pdev;
    /* ... initialize hardware ... */
    
    /* [KEY] Register with network stack */
    register_netdev(netdev);
}
```

**说明:**
- 网络核心完全不知道 e1000 硬件细节（寄存器、DMA 环、描述符格式）
- `netdev_priv()` 返回驱动私有数据（与 `container_of` 类似但使用偏移量）
- 返回值 `NETDEV_TX_OK` 或 `NETDEV_TX_BUSY` 是契约的一部分
- 驱动处理所有硬件特定细节：DMA 映射、VLAN 标签、TSO 卸载

**Boundary enforced:** Network stack (software) ←→ Network driver (hardware)

---

### Example 3: `struct inet_connection_sock` → `icsk_ca_ops` → `cong_avoid()` — TCP/Congestion Control Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: TCP ACK Processing ========== */
/* net/ipv4/tcp_input.c - Received ACK from peer */

static int tcp_ack(struct sock *sk, struct sk_buff *skb, int flag)
{
    struct tcp_sock *tp = tcp_sk(sk);
    u32 prior_snd_una = tp->snd_una;
    u32 ack = TCP_SKB_CB(skb)->ack_seq;
    u32 acked;
    
    /* [KEY] Update acknowledged bytes */
    acked = ack - prior_snd_una;
    
    /* [KEY] Check if this ACK acknowledges new data */
    if (after(ack, prior_snd_una)) {
        /* [KEY] Call congestion control algorithm */
        tcp_cong_avoid(sk, ack, prior_in_flight);
    }
}

/* ========== LAYER 2: Congestion Control Dispatch ========== */
/* net/ipv4/tcp_cong.c:373-388 */
void tcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    const struct inet_connection_sock *icsk = inet_csk(sk);
    
    /* [KEY] THE OPS PATTERN: icsk->icsk_ca_ops->cong_avoid(sk, ack, in_flight) */
    /* - icsk contains the congestion algorithm ops pointer */
    /* - icsk_ca_ops points to pluggable algorithm (cubic, reno, etc.) */
    /* - cong_avoid is the congestion avoidance function */
    /* - sk passed back for algorithm to access TCP state */
    icsk->icsk_ca_ops->cong_avoid(sk, ack, in_flight);
    /*                 ^^^^^^^^^^  ^^
     *                     |       |
     *                     |       +-- Same socket for private data access
     *                     +-- Algorithm's implementation (e.g., bictcp_cong_avoid)
     */
    
    /* [KEY] Ensure cwnd doesn't exceed maximum */
    tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_cwnd_clamp);
}

/* ========== LAYER 3: Congestion Algorithm Implementation ========== */
/* net/ipv4/tcp_cubic.c */
static void bictcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    struct tcp_sock *tp = tcp_sk(sk);
    
    /* [KEY] PRIVATE DATA RECOVERY - uses inline storage */
    /* Congestion algorithms store private data in icsk_ca_priv[] */
    struct bictcp *ca = inet_csk_ca(sk);
    /*
     * inet_csk_ca() returns pointer to:
     * struct inet_connection_sock {
     *     struct inet_sock icsk_inet;
     *     ...
     *     u32 icsk_ca_priv[16];  <- Private storage here
     *     ...
     * };
     * 
     * struct bictcp fits within icsk_ca_priv[16] (64 bytes)
     */
    
    /* [KEY] Check if we're still in slow start */
    if (tp->snd_cwnd <= tp->snd_ssthresh) {
        /* [KEY] Slow start - exponential growth */
        tcp_slow_start(tp);
    } else {
        /* [KEY] Congestion avoidance - CUBIC algorithm */
        bictcp_update(ca, tp->snd_cwnd);
        
        /* [KEY] CUBIC's window calculation */
        /* Uses cubic function of time since last congestion event */
        /* ca->cnt tracks when to increase cwnd */
        if (ca->cnt > tp->snd_cwnd) {
            /* Time to increase window */
            tp->snd_cwnd++;
            ca->cnt = 0;
        }
    }
    
    /* [KEY] Record RTT sample for CUBIC calculations */
    if (ca->delay_min == 0 || ca->delay_min > rtt)
        ca->delay_min = rtt;
}

/* [KEY] CUBIC private data structure */
struct bictcp {
    u32 cnt;           /* Increase cwnd by 1 after ACKs >= cnt */
    u32 last_max_cwnd; /* Last maximum cwnd */
    u32 loss_cwnd;     /* Cwnd right before last loss */
    u32 last_cwnd;     /* Last cwnd */
    u32 last_time;     /* Time of last cwnd update */
    u32 bic_origin_point; /* Origin point of cubic function */
    u32 bic_K;         /* Time to reach origin point from last max */
    u32 delay_min;     /* Minimum RTT seen */
    u32 epoch_start;   /* Start of current epoch */
    u32 ack_cnt;       /* ACK count in current epoch */
    u32 tcp_cwnd;      /* Estimated Reno cwnd */
    /* More fields fit within icsk_ca_priv[16] */
};

/* ========== ALGORITHM REGISTRATION ========== */
/* net/ipv4/tcp_cubic.c */
static struct tcp_congestion_ops cubictcp __read_mostly = {
    .init       = bictcp_init,
    .ssthresh   = bictcp_recalc_ssthresh,   /* [KEY] Required */
    .cong_avoid = bictcp_cong_avoid,        /* [KEY] Required */
    .set_state  = bictcp_state,
    .undo_cwnd  = bictcp_undo_cwnd,
    .pkts_acked = bictcp_acked,
    .owner      = THIS_MODULE,
    .name       = "cubic",
};

/* net/ipv4/tcp_cong.c:38-60 - Registration with validation */
int tcp_register_congestion_control(struct tcp_congestion_ops *ca)
{
    int ret = 0;

    /* [KEY] Contract enforcement: required operations */
    /* All algorithms MUST implement ssthresh and cong_avoid */
    if (!ca->ssthresh || !ca->cong_avoid) {
        printk(KERN_ERR "TCP %s does not implement required ops\n",
               ca->name);
        return -EINVAL;  /* [KEY] Reject incomplete implementation */
    }

    spin_lock(&tcp_cong_list_lock);
    if (tcp_ca_find(ca->name)) {
        ret = -EEXIST;
    } else {
        list_add_tail_rcu(&ca->list, &tcp_cong_list);
    }
    spin_unlock(&tcp_cong_list_lock);

    return ret;
}
EXPORT_SYMBOL_GPL(tcp_register_congestion_control);
```

**说明:**
- TCP 拥塞控制是经典的可插拔算法示例（cubic, reno, vegas 等）
- 私有数据使用内联存储（`icsk_ca_priv[16]`）而非嵌入结构
- 注册时强制检查必需操作（`ssthresh` 和 `cong_avoid`）
- 用户可通过 sysctl 或 socket 选项在运行时切换算法

**Boundary enforced:** TCP core logic ←→ Pluggable congestion control algorithms

---

### Example 4: `struct inet_connection_sock` → `icsk_af_ops` → `queue_xmit()` — Transport/Network Layer Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: TCP Sending Data Segment ========== */
/* net/ipv4/tcp_output.c - TCP has segment ready to send */

static int tcp_transmit_skb(struct sock *sk, struct sk_buff *skb,
                            int clone_it, gfp_t gfp_mask)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct tcp_sock *tp = tcp_sk(sk);
    struct tcp_skb_cb *tcb = TCP_SKB_CB(skb);
    struct tcphdr *th;
    int err;
    
    /* [KEY] Build TCP header - TCP layer responsibility */
    th = tcp_hdr(skb);
    th->source      = inet->inet_sport;
    th->dest        = inet->inet_dport;
    th->seq         = htonl(tcb->seq);
    th->ack_seq     = htonl(tp->rcv_nxt);
    th->doff        = (tcp_header_size >> 2);
    th->window      = htons(tp->rcv_wnd >> tp->rx_opt.rcv_wscale);
    
    /* [KEY] Calculate TCP checksum - may use hardware offload */
    tcp_options_write((__be32 *)(th + 1), tp, &opts);
    
    /* [KEY] Now hand off to IP layer */
    /* But which IP? IPv4 or IPv6? */
    /* icsk_af_ops abstracts this! */
    
    /* [KEY] THE OPS PATTERN: icsk->icsk_af_ops->queue_xmit(skb) */
    /* - icsk contains address family operations pointer */
    /* - icsk_af_ops points to IPv4 or IPv6 operations */
    /* - queue_xmit sends packet to IP layer */
    /* - skb contains all context (socket info via skb->sk) */
    err = icsk->icsk_af_ops->queue_xmit(skb);
    /*                       ^^^^^^^^^^
     *                           |
     *                           +-- ip_queue_xmit for IPv4
     *                               inet6_csk_xmit for IPv6
     */
    
    return err;
}

/* ========== LAYER 2: IP Layer Transmission ========== */
/* net/ipv4/ip_output.c */
int ip_queue_xmit(struct sk_buff *skb)
{
    struct sock *sk = skb->sk;
    struct inet_sock *inet = inet_sk(sk);
    struct ip_options *opt = inet->opt;
    struct rtable *rt;
    struct iphdr *iph;
    
    /* [KEY] Route lookup - IP layer responsibility */
    rt = ip_route_output_ports(sock_net(sk), &fl4, sk,
                               inet->inet_daddr, inet->inet_saddr,
                               inet->inet_dport, inet->inet_sport,
                               sk->sk_protocol, RT_CONN_FLAGS(sk),
                               sk->sk_bound_dev_if);
    
    /* [KEY] Reserve space for IP header */
    skb_push(skb, sizeof(struct iphdr) + (opt ? opt->optlen : 0));
    skb_reset_network_header(skb);
    
    /* [KEY] Build IP header - completely different from TCP header */
    iph = ip_hdr(skb);
    iph->version  = 4;
    iph->ihl      = 5;  /* Plus options if any */
    iph->tos      = inet->tos;
    iph->tot_len  = htons(skb->len);
    iph->id       = htons(inet->inet_id++);
    iph->frag_off = htons(IP_DF);  /* Don't fragment */
    iph->ttl      = ip_select_ttl(inet, &rt->dst);
    iph->protocol = sk->sk_protocol;  /* TCP = 6 */
    iph->saddr    = fl4.saddr;
    iph->daddr    = fl4.daddr;
    
    /* [KEY] Calculate IP checksum */
    ip_send_check(iph);
    
    /* [KEY] Output to netfilter and device */
    /* This continues the ops chain: IP -> Netfilter -> Device */
    return ip_local_out(skb);
}

/* ========== ADDRESS FAMILY OPS TABLES ========== */
/* net/ipv4/tcp_ipv4.c - IPv4 operations for TCP */
const struct inet_connection_sock_af_ops ipv4_specific = {
    .queue_xmit        = ip_queue_xmit,     /* [KEY] IPv4 transmit */
    .send_check        = tcp_v4_send_check, /* [KEY] IPv4 checksum */
    .rebuild_header    = inet_sk_rebuild_header,
    .conn_request      = tcp_v4_conn_request,
    .syn_recv_sock     = tcp_v4_syn_recv_sock,
    .get_peer          = tcp_v4_get_peer,
    .net_header_len    = sizeof(struct iphdr),
    .setsockopt        = ip_setsockopt,
    .getsockopt        = ip_getsockopt,
    .addr2sockaddr     = inet_csk_addr2sockaddr,
    .sockaddr_len      = sizeof(struct sockaddr_in),
};

/* net/ipv6/tcp_ipv6.c - IPv6 operations for TCP */
const struct inet_connection_sock_af_ops ipv6_specific = {
    .queue_xmit        = inet6_csk_xmit,    /* [KEY] IPv6 transmit */
    .send_check        = tcp_v6_send_check, /* [KEY] IPv6 checksum */
    .rebuild_header    = inet6_sk_rebuild_header,
    .conn_request      = tcp_v6_conn_request,
    .syn_recv_sock     = tcp_v6_syn_recv_sock,
    .get_peer          = tcp_v6_get_peer,
    .net_header_len    = sizeof(struct ipv6hdr),
    .setsockopt        = ipv6_setsockopt,
    .getsockopt        = ipv6_getsockopt,
    .addr2sockaddr     = inet6_csk_addr2sockaddr,
    .sockaddr_len      = sizeof(struct sockaddr_in6),
};

/* [KEY] Assignment during socket initialization */
/* net/ipv4/tcp_ipv4.c */
static int tcp_v4_init_sock(struct sock *sk)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    
    /* [KEY] Set IPv4-specific operations */
    icsk->icsk_af_ops = &ipv4_specific;
    
    /* ... other initialization ... */
}
```

**说明:**
- `icsk_af_ops` 允许 TCP 代码对 IPv4 和 IPv6 透明
- TCP 只调用 `queue_xmit()`，不知道是 `ip_queue_xmit` 还是 `inet6_csk_xmit`
- 同一个 TCP 代码路径支持两种 IP 版本
- 这是依赖反转：高层（TCP）不依赖低层具体实现（IPv4/IPv6）

**Boundary enforced:** Transport protocol (TCP) ←→ Network protocol (IPv4/IPv6)

---

### Example 5: `struct sock` → `sk_prot` → `init()` — Socket Protocol Initialization Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Socket Creation ========== */
/* net/socket.c - User calls socket(AF_INET, SOCK_STREAM, 0) */

SYSCALL_DEFINE3(socket, int, family, int, type, int, protocol)
{
    /* [KEY] Dispatch to protocol family */
    return __sys_socket(family, type, protocol);
}

int __sock_create(struct net *net, int family, int type, int protocol,
                  struct socket **res, int kern)
{
    struct socket *sock;
    const struct net_proto_family *pf;
    
    /* [KEY] Allocate socket structure */
    sock = sock_alloc();
    sock->type = type;
    
    /* [KEY] Find protocol family (AF_INET, AF_INET6, etc.) */
    pf = rcu_dereference(net_families[family]);
    
    /* [KEY] ANOTHER OPS PATTERN - protocol family creation */
    /* pf->create will eventually call protocol-specific init */
    err = pf->create(net, sock, protocol, kern);
    
    *res = sock;
    return 0;
}

/* ========== LAYER 2: INET Protocol Family ========== */
/* net/ipv4/af_inet.c */
static int inet_create(struct net *net, struct socket *sock, int protocol,
                       int kern)
{
    struct sock *sk;
    struct inet_protosw *answer;
    struct proto *answer_prot;
    
    /* [KEY] Find protocol (TCP, UDP, etc.) from protocol table */
    list_for_each_entry_rcu(answer, &inetsw[sock->type], list) {
        if (protocol == answer->protocol) {
            if (protocol != IPPROTO_IP)
                break;
        }
    }
    answer_prot = answer->prot;  /* [KEY] e.g., &tcp_prot */
    
    /* [KEY] Allocate internal socket with protocol's size */
    sk = sk_alloc(net, PF_INET, GFP_KERNEL, answer_prot);
    /*                                      ^^^^^^^^^^^
     *                                          |
     *                                          +-- answer_prot->obj_size
     *                                              determines allocation size
     */
    
    /* [KEY] Initialize inet-specific fields */
    sock_init_data(sock, sk);
    
    /* [KEY] THE OPS PATTERN: sk->sk_prot->init(sk) */
    /* - sk is the framework object (internal socket) */
    /* - sk_prot points to protocol's operations table */
    /* - init is the initialization function pointer */
    /* - sk passed for protocol to initialize its state */
    if (sk->sk_prot->init) {
        err = sk->sk_prot->init(sk);
        /*               ^^^^  ^^
         *                |    |
         *                |    +-- Same socket for state initialization
         *                +-- tcp_v4_init_sock for TCP
         */
        if (err)
            sk_common_release(sk);
    }
    
    return err;
}

/* ========== LAYER 3: TCP Protocol Initialization ========== */
/* net/ipv4/tcp_ipv4.c */
static int tcp_v4_init_sock(struct sock *sk)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct tcp_sock *tp = tcp_sk(sk);
    
    /* [KEY] PRIVATE DATA INITIALIZATION */
    /* tcp_sock was allocated (via sk_alloc using tcp_prot.obj_size) */
    /* Now initialize TCP-specific fields */
    
    /* [KEY] Initialize TCP state machine */
    tp->out_of_order_queue = RB_ROOT;
    tcp_init_xmit_timers(sk);
    tcp_prequeue_init(tp);
    
    /* [KEY] Set default TCP options */
    tp->rto = TCP_TIMEOUT_INIT;
    tp->mdev = TCP_TIMEOUT_INIT;
    tp->rcv_ssthresh = TCP_DEFAULT_INIT_RCVWND;
    
    /* [KEY] Initialize congestion control */
    icsk->icsk_ca_ops = &tcp_init_congestion_ops;
    icsk->icsk_rto = TCP_TIMEOUT_INIT;
    
    /* [KEY] Set address family operations (IPv4) */
    icsk->icsk_af_ops = &ipv4_specific;
    
    /* [KEY] Initialize sync cookie handling */
    sk->sk_state = TCP_CLOSE;
    sk->sk_write_space = sk_stream_write_space;
    
    /* [KEY] TCP memory management */
    tp->snd_cwnd = TCP_INIT_CWND;
    tp->snd_cwnd_clamp = ~0;
    tp->mss_cache = TCP_MSS_DEFAULT;
    
    return 0;
}

/* ========== PROTOCOL TABLE REGISTRATION ========== */
/* net/ipv4/af_inet.c */
static struct inet_protosw inetsw_array[] = {
    {
        .type       = SOCK_STREAM,
        .protocol   = IPPROTO_TCP,
        .prot       = &tcp_prot,         /* [KEY] TCP protocol ops */
        .ops        = &inet_stream_ops,   /* [KEY] BSD socket ops */
        .no_check   = 0,
        .flags      = INET_PROTOSW_PERMANENT | INET_PROTOSW_ICSK,
    },
    {
        .type       = SOCK_DGRAM,
        .protocol   = IPPROTO_UDP,
        .prot       = &udp_prot,         /* [KEY] UDP protocol ops */
        .ops        = &inet_dgram_ops,    /* [KEY] BSD socket ops */
        .no_check   = UDP_CSUM_DEFAULT,
        .flags      = INET_PROTOSW_PERMANENT,
    },
    /* ... ICMP, RAW, etc. ... */
};

/* net/ipv4/tcp_ipv4.c - TCP protocol definition */
struct proto tcp_prot = {
    .name           = "TCP",
    .owner          = THIS_MODULE,
    
    /* [KEY] Socket lifecycle operations */
    .init           = tcp_v4_init_sock,   /* Called during creation */
    .destroy        = tcp_v4_destroy_sock,/* Called during close */
    
    /* [KEY] Connection operations */
    .close          = tcp_close,
    .connect        = tcp_v4_connect,
    .disconnect     = tcp_disconnect,
    .accept         = inet_csk_accept,
    
    /* [KEY] Data transfer operations */
    .sendmsg        = tcp_sendmsg,
    .recvmsg        = tcp_recvmsg,
    
    /* [KEY] Memory size for allocation */
    .obj_size       = sizeof(struct tcp_sock),  /* [KEY] Full TCP socket size */
    .slab_flags     = SLAB_DESTROY_BY_RCU,
    
    .hash           = inet_hash,
    .unhash         = inet_unhash,
    /* ... */
};
```

**说明:**
- `sk->sk_prot->init` 在 socket 创建时初始化协议特定状态
- TCP 初始化包括：状态机、定时器、拥塞控制、选项
- `obj_size` 字段告诉分配器需要多大内存（`sizeof(struct tcp_sock)`）
- 这确保分配的内存足够容纳 TCP 私有数据

**Boundary enforced:** Socket creation framework ←→ Protocol-specific initialization

---

### Network Examples Summary Table

| # | Framework Object | Ops Table | Operation | Private Data Recovery | Boundary Description |
|---|------------------|-----------|-----------|----------------------|---------------------|
| 1 | `struct socket` | `proto_ops` | `sendmsg()` | `sock->sk` → `tcp_sk()` | BSD socket ←→ Protocol family ←→ Transport |
| 2 | `struct net_device` | `netdev_ops` | `ndo_start_xmit()` | `netdev_priv(dev)` | Network stack ←→ Hardware driver |
| 3 | `inet_connection_sock` | `icsk_ca_ops` | `cong_avoid()` | `inet_csk_ca(sk)` inline | TCP core ←→ Congestion algorithm |
| 4 | `inet_connection_sock` | `icsk_af_ops` | `queue_xmit()` | `inet_sk(sk)` | Transport (TCP) ←→ Network (IPv4/IPv6) |
| 5 | `struct sock` | `sk_prot` | `init()` | `tcp_sk(sk)` | Socket framework ←→ Protocol init |

---

## Step 6.5 — Other Subsystem Examples

This section presents additional examples from other kernel subsystems, demonstrating the universality of the ops pattern across different domains.

### Example A: `struct vm_area_struct` → `vm_ops` → `fault()` — Memory Management/Page Fault Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Page Fault Entry ========== */
/* mm/memory.c - Hardware page fault occurred */

int handle_mm_fault(struct mm_struct *mm, struct vm_area_struct *vma,
                    unsigned long address, unsigned int flags)
{
    /* [KEY] Determine fault type and dispatch */
    if (!(vma->vm_flags & VM_WRITE)) {
        /* Read fault */
    }
    
    /* [KEY] Handle the actual fault */
    return __handle_mm_fault(mm, vma, address, flags);
}

static int __handle_mm_fault(struct mm_struct *mm, struct vm_area_struct *vma,
                             unsigned long address, unsigned int flags)
{
    /* ... page table walking ... */
    
    /* [KEY] No page present - need to fault it in */
    return handle_pte_fault(mm, vma, address, pte, pmd, flags);
}

/* ========== LAYER 2: PTE Fault Dispatch ========== */
/* mm/memory.c:3324 */
static int handle_pte_fault(struct mm_struct *mm, struct vm_area_struct *vma,
                            unsigned long address, pte_t *pte, pmd_t *pmd,
                            unsigned int flags)
{
    pte_t entry = *pte;
    
    if (!pte_present(entry)) {
        if (pte_none(entry)) {
            /* [KEY] First access - call vma's fault handler */
            if (vma->vm_ops && vma->vm_ops->fault)
                /* [KEY] THE OPS PATTERN: vma->vm_ops->fault(vma, vmf) */
                return do_linear_fault(mm, vma, address, pte, pmd, flags, entry);
        }
    }
}

static int do_linear_fault(struct mm_struct *mm, struct vm_area_struct *vma,
                           unsigned long address, pte_t *pte, pmd_t *pmd,
                           unsigned int flags, pte_t orig_pte)
{
    struct vm_fault vmf;
    
    /* [KEY] Prepare fault context */
    vmf.virtual_address = (void __user *)(address & PAGE_MASK);
    vmf.pgoff = (((address & PAGE_MASK) - vma->vm_start) >> PAGE_SHIFT) 
                + vma->vm_pgoff;
    vmf.flags = flags;
    vmf.page = NULL;
    
    /* [KEY] THE OPS PATTERN: vma->vm_ops->fault(vma, &vmf) */
    /* - vma is the framework object (virtual memory area) */
    /* - vm_ops points to file/device-specific operations */
    /* - fault is the page fault handler function */
    /* - vma passed back for implementation context */
    ret = vma->vm_ops->fault(vma, &vmf);
    /*                 ^^^^^  ^^^
     *                   |     |
     *                   |     +-- Same VMA for file/context access
     *                   +-- filemap_fault for file mappings
     *                       shmem_fault for tmpfs
     *                       device_fault for device memory
     */
    
    if (vmf.page) {
        /* [KEY] Implementation returned a page - install it */
        page = vmf.page;
        /* ... install page in page table ... */
    }
    
    return ret;
}

/* ========== LAYER 3: File Mapping Implementation ========== */
/* mm/filemap.c */
int filemap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    /* [KEY] PRIVATE DATA RECOVERY via vma->vm_file */
    struct file *file = vma->vm_file;
    struct address_space *mapping = file->f_mapping;
    struct inode *inode = mapping->host;
    pgoff_t offset = vmf->pgoff;
    struct page *page;
    
    /* [KEY] Try to find page in page cache */
    page = find_get_page(mapping, offset);
    
    if (!page) {
        /* [KEY] Page not in cache - read from filesystem */
        /* This triggers ANOTHER ops call: a_ops->readpage */
        page = page_cache_alloc_cold(mapping);
        error = mapping->a_ops->readpage(file, page);
    }
    
    /* [KEY] Return page to MM framework */
    vmf->page = page;
    return 0;
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* mm/filemap.c */
const struct vm_operations_struct generic_file_vm_ops = {
    .fault      = filemap_fault,       /* [KEY] Page fault handler */
    .page_mkwrite = filemap_page_mkwrite, /* [KEY] Write fault */
};

/* [KEY] During mmap(), filesystem sets vm_ops */
int generic_file_mmap(struct file *file, struct vm_area_struct *vma)
{
    /* [KEY] Set VMA operations to generic file ops */
    vma->vm_ops = &generic_file_vm_ops;
    /* Now page faults on this VMA call filemap_fault */
    return 0;
}

/* [KEY] Device memory example - different implementation */
/* drivers/gpu/drm/drm_gem.c */
static const struct vm_operations_struct drm_gem_vm_ops = {
    .fault      = drm_gem_vm_fault,    /* [KEY] GPU memory fault */
    .open       = drm_gem_vm_open,
    .close      = drm_gem_vm_close,
};
/* GPU driver handles page faults completely differently */
```

**说明:**
- 页面错误处理展示 MM 子系统如何使用 ops 模式
- `vm_ops->fault` 允许文件映射、设备内存、共享内存有不同的页错误处理
- 文件映射通过页面缓存获取页面（`filemap_fault`）
- 设备驱动可能从 GPU 内存或其他设备获取页面

**Boundary enforced:** Memory management framework ←→ Backing store (file/device)

---

### Example B: `struct tty_struct` → `ops` → `write()` — TTY Core/Driver Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Write System Call ========== */
/* drivers/tty/tty_io.c - User writes to /dev/ttyXXX */

static ssize_t tty_write(struct file *file, const char __user *buf,
                         size_t count, loff_t *ppos)
{
    struct tty_struct *tty = file_tty(file);
    struct tty_ldisc *ld;
    
    /* [KEY] Get line discipline */
    ld = tty_ldisc_ref_wait(tty);
    
    /* [KEY] Dispatch through line discipline first */
    /* This is ANOTHER ops pattern: ld->ops->write */
    if (ld->ops->write)
        ret = ld->ops->write(tty, file, buf, count);
    
    tty_ldisc_deref(ld);
    return ret;
}

/* ========== LAYER 2: Line Discipline Processing ========== */
/* drivers/tty/n_tty.c - Normal TTY line discipline */
static ssize_t n_tty_write(struct tty_struct *tty, struct file *file,
                           const unsigned char *buf, size_t nr)
{
    /* [KEY] Process output through line discipline */
    /* Handle special characters, output processing, etc. */
    
    while (nr > 0) {
        /* [KEY] Eventually calls tty driver */
        c = tty->ops->write(tty, b, nr);
        /*         ^^^^^  ^^^
         *           |     |
         *           |     +-- Same tty for driver context
         *           +-- Driver's write function
         */
        if (c < 0) {
            retval = c;
            break;
        }
        nr -= c;
        b += c;
    }
    
    return retval;
}

/* ========== LAYER 3: TTY Driver Implementation ========== */
/* drivers/tty/serial/8250.c (simplified) */
static int serial8250_write(struct tty_struct *tty,
                            const unsigned char *buf, int count)
{
    /* [KEY] PRIVATE DATA RECOVERY */
    struct uart_state *state = tty->driver_data;
    struct uart_port *port = state->uart_port;
    struct uart_8250_port *up = container_of(port, struct uart_8250_port, port);
    
    unsigned long flags;
    int c, ret = 0;
    
    spin_lock_irqsave(&port->lock, flags);
    
    while (count > 0) {
        /* [KEY] Wait for transmit buffer space */
        while (!(serial_in(up, UART_LSR) & UART_LSR_THRE)) {
            /* Hardware FIFO full - wait */
        }
        
        /* [KEY] Write to hardware UART register */
        /* This is completely hardware-specific */
        c = min(count, (int)(port->fifosize - port->x_char_pending));
        serial_out(up, UART_TX, *buf++);
        /*         ^^
         *          |
         *          +-- Write to hardware I/O port
         */
        count--;
        ret++;
    }
    
    spin_unlock_irqrestore(&port->lock, flags);
    return ret;
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* include/linux/tty_driver.h:246-286 */
struct tty_operations {
    struct tty_struct *(*lookup)(struct tty_driver *, struct inode *, int);
    int  (*install)(struct tty_driver *, struct tty_struct *);
    void (*remove)(struct tty_driver *, struct tty_struct *);
    int  (*open)(struct tty_struct *, struct file *);
    void (*close)(struct tty_struct *, struct file *);
    void (*shutdown)(struct tty_struct *);
    void (*cleanup)(struct tty_struct *);
    int  (*write)(struct tty_struct *, const unsigned char *, int);  /* [KEY] */
    int  (*put_char)(struct tty_struct *, unsigned char);
    void (*flush_chars)(struct tty_struct *);
    int  (*write_room)(struct tty_struct *);  /* [KEY] Buffer space */
    int  (*chars_in_buffer)(struct tty_struct *);
    int  (*ioctl)(struct tty_struct *, unsigned int, unsigned long);
    void (*set_termios)(struct tty_struct *, struct ktermios *);
    void (*throttle)(struct tty_struct *);
    void (*unthrottle)(struct tty_struct *);
    void (*stop)(struct tty_struct *);
    void (*start)(struct tty_struct *);
    void (*hangup)(struct tty_struct *);
    /* ... */
};

/* drivers/tty/serial/8250.c */
static const struct tty_operations serial8250_ops = {
    .open       = serial8250_open,
    .close      = serial8250_close,
    .write      = serial8250_write,   /* [KEY] Points to implementation */
    .put_char   = serial8250_put_char,
    .write_room = serial8250_write_room,
    .chars_in_buffer = serial8250_chars_in_buffer,
    .ioctl      = serial8250_ioctl,
    .set_termios = serial8250_set_termios,
    .throttle   = serial8250_throttle,
    .unthrottle = serial8250_unthrottle,
    .stop       = serial8250_stop,
    .start      = serial8250_start,
    /* ... */
};
```

**说明:**
- TTY 子系统展示两层 ops 模式：行规则（line discipline）和驱动
- 行规则处理字符处理（回显、删除键等）
- 驱动处理硬件交互（UART 寄存器）
- 同一个 TTY 核心支持串口、伪终端、控制台等

**Boundary enforced:** TTY core ←→ Line discipline ←→ Hardware driver

---

### Example C: `struct seq_file` → `op` → `show()` — Proc/Seq File Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: User Reads /proc File ========== */
/* fs/seq_file.c - User reads /proc/meminfo, /proc/cpuinfo, etc. */

ssize_t seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    struct seq_file *m = file->private_data;
    size_t copied = 0;
    int err = 0;
    void *p;
    
    /* [KEY] Call iterator to get first element */
    p = m->op->start(m, &pos);
    /*       ^^^^^
     *         |
     *         +-- Start iteration (e.g., seq_list_start)
     */
    
    while (1) {
        /* [KEY] Render current element to buffer */
        err = m->op->show(m, p);
        /*          ^^^^
         *            |
         *            +-- Format and output one item
         */
        
        if (err < 0)
            break;
        
        if (m->count < m->size)
            goto Fill;
        
        /* [KEY] Get next element */
        p = m->op->next(m, p, &pos);
        /*          ^^^^
         *            |
         *            +-- Advance to next item
         */
        
        if (!p || IS_ERR(p))
            break;
    }
    
    /* [KEY] Cleanup iteration */
    m->op->stop(m, p);
    /*      ^^^^
     *        |
     *        +-- End iteration (release locks, etc.)
     */
    
    /* Copy data to user */
    if (copy_to_user(buf, m->buf + m->from, copied))
        return -EFAULT;
    
    return copied;
}

/* ========== LAYER 2: Specific /proc File Implementation ========== */
/* fs/proc/meminfo.c */
static int meminfo_proc_show(struct seq_file *m, void *v)
{
    struct sysinfo i;
    unsigned long pages[NR_LRU_LISTS];
    
    /* [KEY] Gather system memory information */
    si_meminfo(&i);
    
    /* [KEY] Format output using seq_file helpers */
    seq_printf(m,
        "MemTotal:       %8lu kB\n"
        "MemFree:        %8lu kB\n"
        "Buffers:        %8lu kB\n"
        "Cached:         %8lu kB\n",
        K(i.totalram),
        K(i.freeram),
        K(i.bufferram),
        K(cached));
    
    /* [KEY] More memory statistics... */
    seq_printf(m,
        "SwapCached:     %8lu kB\n"
        "Active:         %8lu kB\n"
        "Inactive:       %8lu kB\n",
        K(total_swapcache_pages()),
        K(pages[LRU_ACTIVE_ANON] + pages[LRU_ACTIVE_FILE]),
        K(pages[LRU_INACTIVE_ANON] + pages[LRU_INACTIVE_FILE]));
    
    return 0;
}

/* [KEY] Single-element seq_file (no iteration needed) */
static int meminfo_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, meminfo_proc_show, NULL);
}

/* ========== LAYER 3: Iterated /proc File Implementation ========== */
/* fs/proc/stat.c - /proc/stat with per-CPU data */
static void *stat_start(struct seq_file *m, loff_t *pos)
{
    /* [KEY] Start iteration - return first CPU */
    return cpumask_first(cpu_online_mask) < nr_cpu_ids ? 
           (void *)(long)(cpumask_first(cpu_online_mask) + 1) : NULL;
}

static void *stat_next(struct seq_file *m, void *v, loff_t *pos)
{
    /* [KEY] Get next online CPU */
    int cpu = (int)(long)v - 1;
    cpu = cpumask_next(cpu, cpu_online_mask);
    ++*pos;
    return cpu < nr_cpu_ids ? (void *)(long)(cpu + 1) : NULL;
}

static int stat_show(struct seq_file *m, void *v)
{
    int cpu = (int)(long)v - 1;
    struct cpu_usage_stat *cpustat;
    
    /* [KEY] Get per-CPU statistics */
    cpustat = &kstat_cpu(cpu).cpustat;
    
    /* [KEY] Format CPU line */
    seq_printf(m, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu\n",
               cpu,
               cputime64_to_clock_t(cpustat->user),
               cputime64_to_clock_t(cpustat->nice),
               cputime64_to_clock_t(cpustat->system),
               cputime64_to_clock_t(cpustat->idle),
               /* ... */);
    
    return 0;
}

static void stat_stop(struct seq_file *m, void *v)
{
    /* [KEY] Nothing to clean up for CPU iteration */
}

/* ========== OPERATIONS TABLE DEFINITIONS ========== */
/* include/linux/seq_file.h:30-35 */
struct seq_operations {
    void * (*start) (struct seq_file *m, loff_t *pos);
    void (*stop) (struct seq_file *m, void *v);
    void * (*next) (struct seq_file *m, void *v, loff_t *pos);
    int (*show) (struct seq_file *m, void *v);  /* [KEY] Render one item */
};

/* fs/proc/stat.c */
static const struct seq_operations stat_ops = {
    .start  = stat_start,
    .next   = stat_next,
    .stop   = stat_stop,
    .show   = stat_show,
};

static const struct file_operations proc_stat_operations = {
    .open       = stat_open,
    .read       = seq_read,      /* [KEY] Generic seq_read */
    .llseek     = seq_lseek,
    .release    = seq_release,
};

/* fs/proc/meminfo.c - Single value (no iteration) */
static const struct file_operations meminfo_proc_fops = {
    .open       = meminfo_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
```

**说明:**
- seq_file 是内核中最常用的 ops 模式之一
- 提供统一的迭代器接口：`start` → `show` → `next` → ... → `stop`
- `/proc` 文件只需实现 `show()`，seq_file 处理缓冲区管理和用户空间交互
- 支持单值文件（`single_open`）和迭代文件（`seq_open`）

**Boundary enforced:** seq_file framework ←→ Data source (memory info, CPU stats, etc.)

---

### Example D: `struct sched_class` → `pick_next_task()` — Scheduler/Policy Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Context Switch Entry ========== */
/* kernel/sched.c - Time to pick next task */

static void __sched __schedule(void)
{
    struct task_struct *prev, *next;
    struct rq *rq;
    int cpu;
    
    cpu = smp_processor_id();
    rq = cpu_rq(cpu);
    prev = rq->curr;
    
    /* [KEY] Put previous task back in its queue */
    put_prev_task(rq, prev);
    
    /* [KEY] Pick next task to run */
    next = pick_next_task(rq);
    
    if (likely(prev != next)) {
        rq->curr = next;
        /* [KEY] Perform context switch */
        context_switch(rq, prev, next);
    }
}

/* ========== LAYER 2: Scheduler Class Dispatch ========== */
/* kernel/sched.c:4365-4390 */
static inline struct task_struct *pick_next_task(struct rq *rq)
{
    const struct sched_class *class;
    struct task_struct *p;

    /* [KEY] Optimization: if all tasks are CFS, skip class iteration */
    if (likely(rq->nr_running == rq->cfs.h_nr_running)) {
        /* [KEY] THE OPS PATTERN: class->pick_next_task(rq) */
        p = fair_sched_class.pick_next_task(rq);
        /*                    ^^^^^^^^^^^^^^
         *                          |
         *                          +-- CFS scheduler's pick function
         */
        if (likely(p))
            return p;
    }

    /* [KEY] Walk scheduler classes in priority order */
    /* stop > rt > fair > idle */
    for_each_class(class) {
        /* [KEY] THE OPS PATTERN: class->pick_next_task(rq) */
        p = class->pick_next_task(rq);
        /*         ^^^^^^^^^^^^^^  ^^
         *               |          |
         *               |          +-- Run queue for this CPU
         *               +-- Each class implements its own policy
         */
        if (p)
            return p;
    }

    /* [KEY] Should never reach here - idle class always has a task */
    BUG();
}

/* ========== LAYER 3: CFS (Completely Fair Scheduler) ========== */
/* kernel/sched_fair.c */
static struct task_struct *pick_next_task_fair(struct rq *rq)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se;
    struct task_struct *p;
    
    if (!cfs_rq->nr_running)
        return NULL;
    
    /* [KEY] CFS uses red-black tree sorted by virtual runtime */
    /* Leftmost node has smallest vruntime = most deserving */
    do {
        se = pick_next_entity(cfs_rq);
        /*   ^^^^^^^^^^^^^^^^^
         *          |
         *          +-- Returns leftmost entity from rb-tree
         */
        set_next_entity(cfs_rq, se);
        cfs_rq = group_cfs_rq(se);
    } while (cfs_rq);
    
    p = task_of(se);
    return p;
}

/* [KEY] Pick leftmost (smallest vruntime) entity */
static struct sched_entity *pick_next_entity(struct cfs_rq *cfs_rq)
{
    /* [KEY] rb_leftmost points to task with smallest vruntime */
    struct rb_node *left = cfs_rq->rb_leftmost;
    
    if (!left)
        return NULL;
    
    return rb_entry(left, struct sched_entity, run_node);
}

/* ========== LAYER 4: Real-Time Scheduler ========== */
/* kernel/sched_rt.c - Completely different algorithm */
static struct task_struct *pick_next_task_rt(struct rq *rq)
{
    struct rt_rq *rt_rq = &rq->rt;
    struct sched_rt_entity *rt_se;
    struct task_struct *p;
    
    if (!rt_rq->rt_nr_running)
        return NULL;
    
    /* [KEY] RT uses priority bitmap + per-priority queues */
    /* Highest priority task runs (SCHED_FIFO/SCHED_RR) */
    idx = sched_find_first_bit(rt_rq->bitmap);
    /*    ^^^^^^^^^^^^^^^^^^^^^
     *            |
     *            +-- Find highest priority with runnable tasks
     */
    queue = &rt_rq->queue[idx];
    rt_se = list_entry(queue->next, struct sched_rt_entity, run_list);
    
    p = rt_task_of(rt_se);
    return p;
}

/* ========== SCHEDULER CLASS DEFINITIONS ========== */
/* include/linux/sched.h:1084-1120 */
struct sched_class {
    const struct sched_class *next;  /* [KEY] Priority chain */

    void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task) (struct rq *rq);

    void (*check_preempt_curr) (struct rq *rq, struct task_struct *p, int flags);

    struct task_struct * (*pick_next_task) (struct rq *rq);  /* [KEY] */
    void (*put_prev_task) (struct rq *rq, struct task_struct *p);

    void (*set_curr_task) (struct rq *rq);
    void (*task_tick) (struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork) (struct task_struct *p);
    /* ... */
};

/* kernel/sched_fair.c */
static const struct sched_class fair_sched_class = {
    .next           = &idle_sched_class,
    .enqueue_task   = enqueue_task_fair,
    .dequeue_task   = dequeue_task_fair,
    .yield_task     = yield_task_fair,
    .check_preempt_curr = check_preempt_wakeup,
    .pick_next_task = pick_next_task_fair,    /* [KEY] CFS algorithm */
    .put_prev_task  = put_prev_task_fair,
    .set_curr_task  = set_curr_task_fair,
    .task_tick      = task_tick_fair,
    .task_fork      = task_fork_fair,
    /* ... */
};

/* kernel/sched_rt.c */
static const struct sched_class rt_sched_class = {
    .next           = &fair_sched_class,      /* [KEY] RT > CFS priority */
    .enqueue_task   = enqueue_task_rt,
    .dequeue_task   = dequeue_task_rt,
    .yield_task     = yield_task_rt,
    .check_preempt_curr = check_preempt_curr_rt,
    .pick_next_task = pick_next_task_rt,      /* [KEY] RT algorithm */
    .put_prev_task  = put_prev_task_rt,
    .set_curr_task  = set_curr_task_rt,
    .task_tick      = task_tick_rt,
    /* ... */
};
```

**说明:**
- 调度器使用 ops 模式实现多种调度策略（CFS、RT、Deadline）
- `sched_class` 链表按优先级排序：stop > rt > fair > idle
- CFS 使用红黑树按虚拟运行时间排序
- RT 使用优先级位图 + 每优先级队列
- 框架遍历调度类，直到找到可运行任务

**Boundary enforced:** Core scheduler ←→ Scheduling policy (CFS, RT, Deadline)

---

### Example E: `struct block_device` → `bd_ops` → `open()` — Block Device/Driver Boundary

**Top-Down Call Chain Analysis:**

```c
/* ========== LAYER 1: Block Device Open ========== */
/* fs/block_dev.c - Opening /dev/sda, /dev/nvme0n1, etc. */

static int blkdev_open(struct inode *inode, struct file *filp)
{
    struct block_device *bdev;
    
    /* [KEY] Get block device from inode */
    bdev = bd_acquire(inode);
    
    /* [KEY] Open the block device */
    ret = blkdev_get(bdev, filp->f_mode, filp);
    
    filp->f_mapping = bdev->bd_inode->i_mapping;
    return ret;
}

int blkdev_get(struct block_device *bdev, fmode_t mode, void *holder)
{
    /* [KEY] Get the disk */
    struct gendisk *disk = get_gendisk(bdev->bd_dev, &partno);
    
    /* [KEY] Call driver's open function */
    ret = __blkdev_get(bdev, mode, 0);
    
    return ret;
}

/* ========== LAYER 2: Block Device Get ========== */
/* fs/block_dev.c:1127-1210 */
static int __blkdev_get(struct block_device *bdev, fmode_t mode, int for_part)
{
    struct gendisk *disk = bdev->bd_disk;
    int ret;
    
    if (!bdev->bd_openers) {
        /* [KEY] First open of this device */
        
        /* [KEY] THE OPS PATTERN: disk->fops->open(bdev, mode) */
        /* - disk is the generic disk structure */
        /* - fops points to driver's block_device_operations */
        /* - open is the device open function */
        /* - bdev passed for driver to access device state */
        if (disk->fops->open) {
            ret = disk->fops->open(bdev, mode);
            /*               ^^^^  ^^^^
             *                 |     |
             *                 |     +-- Block device for context
             *                 +-- Driver's open function (e.g., sd_open)
             */
            if (ret)
                goto out_clear;
        }
    }
    
    bdev->bd_openers++;
    return 0;
}

/* ========== LAYER 3: SCSI Disk Driver Implementation ========== */
/* drivers/scsi/sd.c */
static int sd_open(struct block_device *bdev, fmode_t mode)
{
    /* [KEY] PRIVATE DATA RECOVERY */
    struct scsi_disk *sdkp = scsi_disk_get(bdev->bd_disk);
    /*
     * scsi_disk_get uses disk->private_data to get scsi_disk
     * 
     * Memory layout:
     * struct gendisk {
     *     const struct block_device_operations *fops;
     *     void *private_data;  <- Points to struct scsi_disk
     *     ...
     * };
     */
    struct scsi_device *sdev;
    int retval;
    
    if (!sdkp)
        return -ENXIO;
    
    sdev = sdkp->device;
    
    /* [KEY] Check if device is online */
    if (!scsi_device_online(sdev)) {
        scsi_disk_put(sdkp);
        return -ENXIO;
    }
    
    /* [KEY] Check if media is present (for removable devices) */
    if (sdev->removable) {
        /* [KEY] Issue SCSI TEST_UNIT_READY command */
        check_disk_change(bdev);
        
        if (!sdkp->media_present) {
            scsi_disk_put(sdkp);
            return -ENOMEDIUM;
        }
    }
    
    /* [KEY] For write access, check write protection */
    if ((mode & FMODE_WRITE) && sdkp->write_prot) {
        scsi_disk_put(sdkp);
        return -EROFS;
    }
    
    return 0;
}

/* ========== OPERATIONS TABLE REGISTRATION ========== */
/* include/linux/blkdev.h:1298-1315 */
struct block_device_operations {
    int (*open) (struct block_device *, fmode_t);              /* [KEY] */
    int (*release) (struct gendisk *, fmode_t);
    int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    int (*compat_ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    unsigned int (*check_events) (struct gendisk *, unsigned int);
    int (*media_changed) (struct gendisk *);
    void (*unlock_native_capacity) (struct gendisk *);
    int (*revalidate_disk) (struct gendisk *);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
    void (*swap_slot_free_notify) (struct block_device *, unsigned long);
    struct module *owner;
};

/* drivers/scsi/sd.c */
static const struct block_device_operations sd_fops = {
    .owner          = THIS_MODULE,
    .open           = sd_open,            /* [KEY] SCSI disk open */
    .release        = sd_release,
    .ioctl          = sd_ioctl,
    .getgeo         = sd_getgeo,
    .check_events   = sd_check_events,
    .revalidate_disk = sd_revalidate_disk,
    .unlock_native_capacity = sd_unlock_native_capacity,
};

/* [KEY] Different driver - NVMe */
/* drivers/block/nvme-core.c */
static const struct block_device_operations nvme_fops = {
    .owner          = THIS_MODULE,
    .open           = nvme_open,          /* [KEY] NVMe-specific open */
    .release        = nvme_release,
    .ioctl          = nvme_ioctl,
    .compat_ioctl   = nvme_compat_ioctl,
    .getgeo         = nvme_getgeo,
};
```

**说明:**
- 块设备层使用 `block_device_operations` 抽象不同存储驱动
- SCSI 磁盘、NVMe、virtio-blk 都实现相同接口
- 私有数据通过 `disk->private_data` 存储和恢复
- 驱动处理特定硬件协议（SCSI 命令、NVMe 队列等）

**Boundary enforced:** Block layer ←→ Storage driver (SCSI, NVMe, etc.)

---

### Other Subsystems Summary Table

| # | Framework Object | Ops Table | Operation | Private Data | Boundary Description |
|---|------------------|-----------|-----------|--------------|---------------------|
| A | `vm_area_struct` | `vm_ops` | `fault()` | `vma->vm_file` | MM fault handling ←→ Backing store |
| B | `tty_struct` | `tty_ops` | `write()` | `tty->driver_data` | TTY core ←→ Serial driver |
| C | `seq_file` | `seq_ops` | `show()` | `m->private` | Proc framework ←→ Data source |
| D | `sched_class` | (direct) | `pick_next_task()` | Run queue structures | Core scheduler ←→ Policy |
| E | `block_device` | `bd_disk->fops` | `open()` | `disk->private_data` | Block layer ←→ Storage driver |

---

## Step 7 — `container_of` and Type Recovery

### What `container_of` Does

```c
/* include/linux/kernel.h */
#define container_of(ptr, type, member) ({                  \
    const typeof(((type *)0)->member) *__mptr = (ptr);      \
    (type *)((char *)__mptr - offsetof(type, member));      \
})

/* Visual explanation: */

/*
 Given:
   ptr = &(some_ext4_inode_info->vfs_inode)
   type = struct ext4_inode_info
   member = vfs_inode

 Memory layout:
 
 +-----------------------------------+
 |  struct ext4_inode_info           |
 |                                   |
 |  i_data[15]          <-- offset 0 |
 |  i_dtime                          |
 |  i_file_acl                       |
 |  ...                              |
 |                                   |
 |  +---------------------------+    |
 |  | struct inode (vfs_inode)  | <--+-- ptr points here
 |  | ...                       |    |
 |  +---------------------------+    |
 |                                   |
 +-----------------------------------+
   ^
   |
   container_of returns this (start of ext4_inode_info)
   
 Calculation:
   result = ptr - offsetof(struct ext4_inode_info, vfs_inode)
*/
```

**说明:**
- `container_of` 从成员指针计算出包含结构的指针
- 使用编译时计算的偏移量
- 完全是指针算术，无运行时开销

### Why It Is Safe

**Invariant 1: Member must actually be embedded**

```c
/* CORRECT: vfs_inode is embedded in ext4_inode_info */
struct ext4_inode_info {
    /* ext4 fields */
    struct inode vfs_inode;  /* EMBEDDED */
};

/* UNSAFE: If someone passed a standalone inode */
struct inode *standalone = kmalloc(sizeof(struct inode), GFP_KERNEL);
struct ext4_inode_info *WRONG = EXT4_I(standalone);  /* CRASH or corruption! */
```

**Invariant 2: Allocation must use correct allocator**

```c
/* ext4 allocates via its slab cache */
static struct inode *ext4_alloc_inode(struct super_block *sb)
{
    struct ext4_inode_info *ei;
    ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);
    /* ... */
    return &ei->vfs_inode;
}

/* VFS calls ext4's alloc_inode, guaranteeing embedded layout */
if (sb->s_op->alloc_inode)
    inode = sb->s_op->alloc_inode(sb);
```

**Invariant 3: Type correctness throughout lifecycle**

```c
/* VFS creates inode via filesystem's alloc_inode */
/* VFS stores pointer to embedded inode */
/* VFS passes inode to filesystem operations */
/* Filesystem recovers full object via container_of */
/* VFS destroys inode via filesystem's destroy_inode */

/* The TYPE NEVER CHANGES during lifecycle */
```

### What Breaks If Invariants Violated

```c
/* Violation 1: Wrong type */
struct inode *inode = some_xfs_inode;  /* Actually xfs_inode_info! */
struct ext4_inode_info *ei = EXT4_I(inode);  /* WRONG TYPE! */
/* ei points to garbage - xfs layout != ext4 layout */

/* Violation 2: Not embedded */
struct inode *inode = kmalloc(sizeof(*inode), GFP_KERNEL);
struct ext4_inode_info *ei = EXT4_I(inode);  /* OVERRUN! */
/* ei points BEFORE the allocation - memory corruption */

/* Violation 3: Use after transformation */
/* If inode type could change, container_of would break */
/* This is why inode type is fixed at allocation */
```

### VFS Example: `EXT4_I()`

```c
/* fs/ext4/ext4.h:1259-1261 */
static inline struct ext4_inode_info *EXT4_I(struct inode *inode)
{
    return container_of(inode, struct ext4_inode_info, vfs_inode);
}

/* Usage in ext4 code: */
static int ext4_file_open(struct inode *inode, struct file *filp)
{
    struct super_block *sb = inode->i_sb;
    struct ext4_sb_info *sbi = EXT4_SB(sb);
    struct ext4_inode_info *ei = EXT4_I(inode);  /* TYPE RECOVERY */
    
    /* Now we can access ext4-specific fields */
    if (ei->i_flags & EXT4_EXTENTS_FL) {
        /* Handle extent-based inode */
    }
}
```

### Networking Example: `tcp_sk()`

```c
/* include/linux/tcp.h */
static inline struct tcp_sock *tcp_sk(const struct sock *sk)
{
    return (struct tcp_sock *)sk;
}

/* This works because tcp_sock STARTS with inet_connection_sock,
 * which STARTS with inet_sock,
 * which STARTS with sock.
 */

struct tcp_sock {
    struct inet_connection_sock inet_conn;  /* FIRST MEMBER */
    u16 tcp_header_len;
    /* ... TCP-specific fields ... */
};

struct inet_connection_sock {
    struct inet_sock icsk_inet;  /* FIRST MEMBER */
    /* ... */
};

struct inet_sock {
    struct sock sk;  /* FIRST MEMBER */
    /* ... */
};

/* Because each is first member, pointers are interchangeable */
/* sk == &tcp_sock.inet_conn.icsk_inet.sk */
/* (struct tcp_sock *)sk works by casting, not offsetting */
```

---

## Step 8 — Control Flow and Dependency Direction

### Who Owns the Control Flow

```
CONTROL FLOW OWNERSHIP:

+------------------------------------------------------------------+
|                     FRAMEWORK (VFS, Socket Core)                  |
|                                                                   |
|   OWNS:                                                          |
|   - When to call operations                                      |
|   - In what order                                                 |
|   - With what parameters                                          |
|   - Error handling policy                                         |
|   - Lifecycle management                                          |
|                                                                   |
|   read(fd, buf, count)                                            |
|        │                                                          |
|        ▼                                                          |
|   vfs_read() {                                                    |
|       if (file->f_op->read)                                       |
|           ret = file->f_op->read(file, buf, count, pos);          |
|                         │                                         |
|                         │ FRAMEWORK CALLS IMPLEMENTATION          |
|                         ▼                                         |
+------------------------------------------------------------------+
                          │
                          ▼
+------------------------------------------------------------------+
|                    IMPLEMENTATION (ext4, TCP)                     |
|                                                                   |
|   OWNS:                                                          |
|   - HOW to perform the operation                                  |
|   - Internal data structures                                      |
|   - Hardware interaction                                          |
|   - Algorithm choice                                              |
|                                                                   |
|   DOES NOT OWN:                                                   |
|   - When it is called                                             |
|   - What happens before/after                                     |
|   - Error policy at upper layers                                  |
|                                                                   |
+------------------------------------------------------------------+
```

**说明:**
- 框架决定何时调用、以什么顺序、传什么参数
- 实现决定如何执行操作
- 实现不能决定何时被调用

### Why Lower-Level Code Never Calls Upward

```c
/* FORBIDDEN: Implementation calling framework internals */

/* In ext4 code - THIS IS WRONG */
static ssize_t ext4_file_read(struct file *file, ...)
{
    /* DON'T DO THIS: */
    vfs_notify_read_start(file);  /* Calling into VFS internals */
    
    /* Do read */
    
    vfs_notify_read_end(file);    /* More upward calls */
}

/* WHY THIS IS WRONG:
 * 1. Creates circular dependency: ext4 → VFS → ext4
 * 2. VFS can't change internal notification without breaking ext4
 * 3. Testing ext4 requires full VFS
 * 4. ext4 assumes VFS internal behavior
 */

/* CORRECT: Framework calls implementation, handles notifications */
ssize_t vfs_read(struct file *file, ...)
{
    /* VFS owns notification - implementation doesn't know */
    if (file->f_op->read)
        ret = file->f_op->read(file, buf, count, pos);
    if (ret > 0)
        fsnotify_access(file);  /* VFS handles this */
}
```

### Inversion of Control (IoC)

```
TRADITIONAL LIBRARY:                  KERNEL OPS PATTERN (IoC):

Application owns flow:                Framework owns flow:

main() {                              framework_init() {
    lib_init();                           register_impl(&my_ops);
    while (running) {                 }
        data = lib_read();
        process(data);                /* Later, framework calls: */
        lib_write(result);            framework_event() {
    }                                     impl->handler(ctx);
    lib_cleanup();                    }
}

Application decides                   Framework decides
when to call library                  when to call implementation
```

### Framework-Driven Execution

```c
/* Example: Network packet reception */

/* 1. Hardware receives packet, generates interrupt */
/* 2. Driver interrupt handler: */
static irqreturn_t e1000_intr(int irq, void *data)
{
    /* Driver handles hardware, queues packet */
    netif_rx(skb);  /* Hand to framework */
}

/* 3. Framework's softirq processes packet */
static int netif_receive_skb(struct sk_buff *skb)
{
    /* Framework decides what to do */
    /* Finds protocol handler, calls it */
    ret = deliver_skb(skb, pt_prev, orig_dev);
}

/* 4. Protocol handler (e.g., ip_rcv) called by framework */
int ip_rcv(struct sk_buff *skb, struct net_device *dev, ...)
{
    /* IP layer doesn't decide when it's called */
    /* It just processes the packet it receives */
}

/* Control flow: Hardware → Driver → Framework → Protocol
 * Never: Protocol → Framework internals
 */
```

### Driver/Module Behavior

```c
/* Module only implements callbacks, doesn't control flow */

static struct file_system_type ext4_fs_type = {
    .name       = "ext4",
    .mount      = ext4_mount,      /* Framework calls when mount requested */
    .kill_sb    = kill_block_super,/* Framework calls when unmount */
    .fs_flags   = FS_REQUIRES_DEV,
};

static int __init ext4_init_fs(void)
{
    /* Module says: "I exist, here are my callbacks" */
    return register_filesystem(&ext4_fs_type);
    /* Module does NOT mount anything itself */
    /* Module waits to be called */
}

/* Framework decides:
 * - When to mount (user request)
 * - When to read files (user request)
 * - When to unmount (user request)
 * - When to sync (timer, memory pressure)
 */
```

---

## Step 9 — Common Misunderstandings & Pitfalls

### Misunderstanding 1: "This is just OOP"

**The claim:** "This is just C implementing object-oriented programming with vtables."

**Why it's wrong:**

| Aspect | OOP vtable | Kernel ops pattern |
|--------|------------|-------------------|
| Purpose | Polymorphism for code reuse | Architectural separation |
| Data ownership | Object owns its data | Framework owns object, impl owns private data |
| Lifecycle | Object often self-managing | Framework manages lifecycle |
| Discovery | Compile-time or runtime type | Always runtime, module discovery |
| Coupling | Subclass knows superclass | Implementation ignores framework internals |

```c
/* OOP would look like: */
struct FileImpl {
    struct File base;  /* Inheritance */
    int my_data;       /* Child adds data */
};
void file_read(struct File *this) {
    /* Downcast to access child data */
    struct FileImpl *impl = (struct FileImpl *)this;
}

/* Kernel pattern is different: */
struct ext4_inode_info {
    /* ext4 data first */
    int my_ext4_data;
    /* Framework struct embedded, not inherited */
    struct inode vfs_inode;
};
/* Framework NEVER downcasts to ext4_inode_info */
/* Only ext4 code does, via EXT4_I() */
```

### Misunderstanding 2: "This is unnecessary indirection"

**The claim:** "We could just call functions directly, this indirection costs performance."

**Why it's wrong:**

| Factor | Direct calls | Ops pattern |
|--------|--------------|-------------|
| Adding new implementation | Modify caller | Just add implementation |
| Compile-time coupling | Tight | None |
| Module loading | Impossible | Trivial |
| Testing | Requires all impls | Mock ops table |
| Maintenance | O(n²) | O(n) |

```c
/* The "cost" of indirection: */
ret = file->f_op->read(file, buf, count, pos);
/* 1 pointer dereference (f_op) */
/* 1 indirect call */
/* Total: maybe 5 cycles on modern CPU */
/* Compared to I/O: microseconds to milliseconds */

/* The "cost" of direct calls: */
/* VFS code would be 10x larger */
/* Every new filesystem = VFS recompile */
/* No modules, no runtime extension */
/* Testing requires full kernel */
```

### Misunderstanding 3: "We can just switch on type"

**The claim:** "Just use `switch (type)` or `if (type == ...)` instead of function pointers."

**Why it's wrong:**

```c
/* The "switch on type" approach: */

ssize_t vfs_read(struct file *file, ...)
{
    switch (file->fs_type) {
    case FS_EXT4:
        return ext4_file_read(file, ...);
    case FS_XFS:
        return xfs_file_read(file, ...);
    case FS_NFS:
        return nfs_file_read(file, ...);
    /* ... 30+ cases ... */
    default:
        return -ENOSYS;
    }
}

/* PROBLEMS:
 * 1. VFS must know about every filesystem
 * 2. Adding filesystem requires VFS change
 * 3. VFS must link against all filesystems
 * 4. Cannot load filesystem as module
 * 5. Enum exhaustion (finite type values)
 * 6. No encapsulation - VFS sees all fs headers
 */
```

### Misunderstanding 4: "Private data should be in the framework object"

**The claim:** "Just add all fields to struct inode, simpler than container_of."

**Why it's wrong:**

```c
/* If private data were in framework object: */
struct inode {
    /* Generic fields */
    umode_t i_mode;
    uid_t i_uid;
    
    /* ext4 fields */
    __le32 ext4_i_data[15];
    ext4_group_t ext4_i_block_group;
    
    /* xfs fields */
    xfs_ino_t xfs_i_ino;
    struct xfs_inode_log_item *xfs_ili;
    
    /* nfs fields */
    struct nfs_fh nfs_fh;
    struct nfs_fattr nfs_fattr;
    
    /* ... 30+ filesystems' fields ... */
};

/* PROBLEMS:
 * 1. Every inode pays memory cost for all filesystems
 * 2. VFS must include all filesystem headers
 * 3. Name collision (everyone wants 'flags')
 * 4. Cannot add fields without kernel change
 * 5. Memory layout frozen across all filesystems
 */
```

### Pitfall 1: Assuming ops are always non-NULL

```c
/* WRONG: */
ret = file->f_op->read(file, buf, count, pos);  /* Crash if read is NULL! */

/* CORRECT: */
if (file->f_op->read)
    ret = file->f_op->read(file, buf, count, pos);
else
    ret = -EINVAL;  /* or use fallback */
```

### Pitfall 2: Storing implementation pointer in wrong structure

```c
/* WRONG: Storing module data in framework struct directly */
file->f_private = my_ext4_data;  /* Arbitrary use of f_private */
/* Different filesystems may conflict! */

/* CORRECT: Use well-defined private data mechanism */
struct ext4_file_info *efi = EXT4_I(inode)->file_info;
```

### Pitfall 3: Implementation calling back into framework

```c
/* WRONG: Implementation re-entering framework */
static ssize_t ext4_file_write(struct file *file, ...)
{
    /* DON'T DO THIS */
    vfs_write(file, buf, count, pos);  /* Re-entering VFS! */
}

/* This creates:
 * - Recursive locking issues
 * - Unexpected call stack
 * - Assumption about VFS behavior
 */
```

---

## Step 10 — Architecture Lessons I Should Internalize

### Lesson 1: Boundaries Are Defined by Contracts, Not Code

```
PRINCIPLE:
The operations table IS the boundary.
Everything on one side knows only the contract.
Everything on the other side can change freely.

APPLICATION:
- Define ops structures at subsystem boundaries
- Ops structure = stable interface
- Implementation details = free to change
- Framework = consumer of contract
- Implementation = provider of contract
```

### Lesson 2: Private Data Enables True Encapsulation

```
PRINCIPLE:
Framework objects should not contain implementation details.
Implementation stores private data separately.
Recovery mechanism (container_of) bridges the gap.

APPLICATION:
struct my_framework_obj {
    /* Generic fields only */
    const struct my_ops *ops;
    /* NO implementation-specific fields */
};

struct my_impl_obj {
    /* Implementation data */
    int impl_specific_field;
    /* Embedded framework object */
    struct my_framework_obj base;
};

#define MY_IMPL(obj) container_of(obj, struct my_impl_obj, base)
```

### Lesson 3: Passing Object Back Enables Stateful Operations

```
PRINCIPLE:
Always pass the framework object as first parameter.
This enables:
- Private data recovery
- Framework state access
- Context for the operation

APPLICATION:
/* Contract definition */
struct my_ops {
    int (*operation)(struct my_obj *obj, int param);
    /*                ^^^^^^^^^^^^^ CRITICAL */
};

/* Implementation */
static int impl_operation(struct my_obj *obj, int param)
{
    struct impl_private *priv = MY_PRIV(obj);  /* Recover private data */
    /* Now have full context */
}
```

### Lesson 4: Registration Pattern for Discovery

```
PRINCIPLE:
Framework should not know implementations at compile time.
Use registration pattern for runtime discovery.

APPLICATION:
/* Framework provides: */
int register_implementation(const struct impl_type *type);
void unregister_implementation(const struct impl_type *type);

/* Implementation provides: */
static struct impl_type my_impl = {
    .name = "my_impl",
    .ops = &my_impl_ops,
};

static int __init my_impl_init(void)
{
    return register_implementation(&my_impl);
}
```

### Lesson 5: Optional Operations with NULL Checks

```
PRINCIPLE:
Not all implementations need all operations.
Check for NULL before calling.
Provide sensible defaults or skip gracefully.

APPLICATION:
/* In contract: */
struct my_ops {
    int (*required_op)(struct obj *);     /* Must implement */
    void (*optional_op)(struct obj *);    /* May be NULL */
};

/* In framework: */
/* Required - no check needed if registration validates */
ret = obj->ops->required_op(obj);

/* Optional - always check */
if (obj->ops->optional_op)
    obj->ops->optional_op(obj);
```

### Lesson 6: Framework Controls Lifecycle

```
PRINCIPLE:
Framework owns:
- Object allocation timing
- Initialization sequence
- Operation invocation order
- Cleanup and destruction

Implementation owns:
- Private data content
- Algorithm choice
- Internal state

APPLICATION:
/* Framework: */
struct my_obj *my_create(const struct my_ops *ops)
{
    struct my_obj *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
    obj->ops = ops;
    if (ops->init)
        ops->init(obj);  /* Let impl initialize */
    return obj;
}

void my_destroy(struct my_obj *obj)
{
    if (obj->ops->cleanup)
        obj->ops->cleanup(obj);  /* Let impl cleanup */
    kfree(obj);
}
```

### Summary: Applying to My Own C Systems

| Lesson | Kernel Pattern | My Application |
|--------|----------------|----------------|
| Define boundary with ops struct | `struct file_operations` | Create `struct my_storage_ops` |
| Separate private data | `EXT4_I(inode)` | Use embedding + container_of |
| Pass object as first param | `f_op->read(file, ...)` | `ops->read(obj, ...)` |
| Use registration | `register_filesystem()` | `my_register_backend()` |
| Check optional ops | `if (f_op->mmap)` | Always check, provide default |
| Framework owns lifecycle | VFS creates/destroys inodes | My framework creates/destroys objects |
| Never call upward | ext4 never calls VFS internals | Implementation never calls framework internals |

### The Deep Insight

```
THE OPS PATTERN IS NOT ABOUT:
- Object-oriented programming
- Code reuse via inheritance
- Polymorphism for type hierarchy

THE OPS PATTERN IS ABOUT:
- Architectural separation between stable and volatile
- Runtime extensibility without recompilation
- Encapsulation that survives across module boundaries
- Inversion of control for framework-driven execution
- Enabling a 25+ million line codebase to remain maintainable
```

---

## Appendix: Quick Reference

### Defining an Ops Structure

```c
/* Contract definition */
struct my_ops {
    struct module *owner;  /* For module refcounting */
    
    /* Lifecycle */
    int (*init)(struct my_obj *obj);
    void (*cleanup)(struct my_obj *obj);
    
    /* Core operations */
    int (*read)(struct my_obj *obj, void *buf, size_t len);
    int (*write)(struct my_obj *obj, const void *buf, size_t len);
    
    /* Optional operations */
    void (*status)(struct my_obj *obj, struct status *st);
};
```

### Implementing Operations

```c
/* Implementation private data */
struct my_impl_data {
    int internal_state;
    struct my_obj base;  /* Embed framework object */
};

#define MY_DATA(obj) container_of(obj, struct my_impl_data, base)

/* Operations implementation */
static int my_impl_read(struct my_obj *obj, void *buf, size_t len)
{
    struct my_impl_data *data = MY_DATA(obj);
    /* Use data->internal_state */
    return len;
}

static const struct my_ops my_impl_ops = {
    .owner = THIS_MODULE,
    .init = my_impl_init,
    .cleanup = my_impl_cleanup,
    .read = my_impl_read,
    .write = my_impl_write,
    /* .status = NULL */  /* Optional, not implemented */
};
```

### Framework Invocation

```c
int my_framework_read(struct my_obj *obj, void *buf, size_t len)
{
    if (!obj->ops || !obj->ops->read)
        return -ENOSYS;
    
    return obj->ops->read(obj, buf, len);
}

void my_framework_status(struct my_obj *obj, struct status *st)
{
    if (obj->ops && obj->ops->status)
        obj->ops->status(obj, st);
    else
        memset(st, 0, sizeof(*st));  /* Default */
}
```

---

*This document analyzes patterns from Linux kernel v3.2. The architectural principles are timeless and apply to any large C system requiring extensibility and maintainability.*

