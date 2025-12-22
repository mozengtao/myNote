# Linux Kernel Ops Tables — Manual Polymorphism (v3.2)

## Overview

This document explains how the Linux kernel implements **object-oriented design using ops tables** (function pointer tables), focusing on architectural intent rather than syntax.

---

## What Ops Tables Represent Conceptually

```
+------------------------------------------------------------------+
|  OPS TABLE = CONTRACT BETWEEN FRAMEWORK AND IMPLEMENTATION       |
+------------------------------------------------------------------+

    +-------------------+          +-------------------+
    |    FRAMEWORK      |          |  IMPLEMENTATION   |
    |  (VFS, netdev,    |          |  (ext4, e1000,    |
    |   serial core)    |          |   pl011 uart)     |
    +-------------------+          +-------------------+
            |                              |
            |  "I will call these          |  "I promise to
            |   functions when..."         |   implement these..."
            |                              |
            v                              v
    +--------------------------------------------------+
    |              struct xxx_operations               |
    |  +--------------------------------------------+  |
    |  | .open    = impl_open,                      |  |
    |  | .read    = impl_read,                      |  |
    |  | .write   = impl_write,                     |  |
    |  | .close   = impl_close,                     |  |
    |  +--------------------------------------------+  |
    +--------------------------------------------------+
                          |
                          |  THE CONTRACT
                          v
    +--------------------------------------------------+
    |  - Framework owns the call timing                |
    |  - Implementation owns the behavior              |
    |  - Neither knows the other's internals           |
    +--------------------------------------------------+
```

**中文解释：**
- Ops 表是框架与实现之间的**契约**
- 框架（如 VFS、netdev）决定**何时**调用函数
- 实现（如 ext4、e1000 驱动）决定**如何**执行
- 双方互不了解对方内部实现，仅通过契约交互

---

## Why the Kernel Avoids Inheritance and Virtual Functions

### The C++ Way (NOT used in kernel)

```cpp
// C++ approach - AVOIDED in kernel
class FileSystem {
    virtual int read(File *f, char *buf, size_t len) = 0;
    virtual int write(File *f, const char *buf, size_t len) = 0;
};

class Ext4 : public FileSystem {
    int read(...) override { ... }
    int write(...) override { ... }
};
```

### The Kernel Way

```c
/* Kernel approach - Explicit ops table */
struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    /* ... */
};

static const struct file_operations ext4_file_ops = {
    .read   = ext4_file_read,
    .write  = ext4_file_write,
    /* ... */
};
```

```
+------------------------------------------------------------------+
|  WHY KERNEL AVOIDS C++ INHERITANCE                               |
+------------------------------------------------------------------+

    +-------------------+     +-------------------+
    |  INHERITANCE      |     |  OPS TABLES       |
    +-------------------+     +-------------------+
    |                   |     |                   |
    | Hidden vtable     |     | Explicit table    |
    | Runtime type info |     | No RTTI needed    |
    | Name mangling     |     | C ABI compatible  |
    | Exception support |     | No exceptions     |
    | Constructor magic |     | Explicit init     |
    | Compiler-specific |     | Portable C        |
    |                   |     |                   |
    +-------------------+     +-------------------+
            |                         |
            v                         v
    Unpredictable behavior    Predictable, auditable
```

**Reasons for ops tables:**

1. **Transparency**: Every function pointer is visible in source
2. **No hidden costs**: No vtable lookup overhead beyond explicit pointer
3. **Partial implementation**: NULL pointers = optional callbacks
4. **Multiple interfaces**: One object can have multiple ops tables
5. **Hot-patching**: Ops can be changed at runtime
6. **ABI stability**: No C++ ABI concerns

**中文解释：**
- 内核避免 C++ 继承的原因：
  1. 隐藏的虚表和 RTTI 增加不可控开销
  2. C++ ABI 不稳定，编译器间不兼容
  3. 异常处理在内核中被禁止
  4. Ops 表完全透明、可审计、可部分实现

---

## The `xxx->ops->yyy()` Pattern

```
+------------------------------------------------------------------+
|  THE CANONICAL KERNEL DISPATCH PATTERN                           |
+------------------------------------------------------------------+

    object->ops->callback(object, args...)
    
    Example:
    
    file->f_op->read(file, buf, count, &pos)
    ~~~~  ~~~~  ~~~~
      |     |     |
      |     |     +-- The actual function pointer
      |     +-------- The ops table
      +-------------- The context object

+------------------------------------------------------------------+
|  CALL FLOW                                                       |
+------------------------------------------------------------------+

    User calls:     sys_read(fd, buf, count)
                           |
                           v
    VFS layer:      file = fget(fd)
                    file->f_op->read(file, buf, count, &file->f_pos)
                           |
                           v
    Dispatch:       ext4_file_read(file, buf, count, ppos)
                           |
                           v
    Hardware:       ... actual disk I/O ...
```

**Pattern structure:**

```c
/* 1. Define the ops table type */
struct xxx_operations {
    int (*op1)(struct xxx *obj, args...);
    int (*op2)(struct xxx *obj, args...);
    /* ... */
};

/* 2. Object contains pointer to ops */
struct xxx {
    const struct xxx_operations *ops;
    /* other fields */
};

/* 3. Implementation provides ops */
static const struct xxx_operations my_ops = {
    .op1 = my_op1_impl,
    .op2 = my_op2_impl,
};

/* 4. Framework dispatches through ops */
int framework_do_op1(struct xxx *obj, args...)
{
    if (obj->ops->op1)
        return obj->ops->op1(obj, args...);
    return -ENOSYS;  /* or default behavior */
}
```

**中文解释：**
- 标准模式：`对象->ops表->回调函数(对象, 参数...)`
- 对象携带指向 ops 表的指针
- 框架通过 ops 表分发调用到具体实现
- 空指针表示可选回调，框架提供默认行为

---

## Real Kernel Examples Analysis

### Example 1: VFS `file_operations`

From `include/linux/fs.h`:

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    /* ... 20+ more callbacks ... */
};
```

```
+------------------------------------------------------------------+
|  file_operations OWNERSHIP MODEL                                 |
+------------------------------------------------------------------+

    WHO OWNS:       Filesystem implementation (ext4, NFS, etc.)
                    - Statically allocated
                    - const qualified
                    - Registered at mount time
    
    WHO CALLS:      VFS layer
                    - sys_read() → vfs_read() → f_op->read()
                    - sys_write() → vfs_write() → f_op->write()
    
    INVARIANTS:
    +---------------------------------------------------------+
    | 1. read/write must handle partial transfers             |
    | 2. Must respect O_NONBLOCK if set                       |
    | 3. Must update *ppos correctly                          |
    | 4. Cannot sleep with spinlocks held                     |
    | 5. Must validate user pointers                          |
    +---------------------------------------------------------+
    
    VIOLATIONS LOOK LIKE:
    +---------------------------------------------------------+
    | - Returning garbage instead of bytes read               |
    | - Ignoring file position                                |
    | - Blocking forever on O_NONBLOCK files                  |
    | - Writing to user space without copy_to_user()          |
    +---------------------------------------------------------+
```

**中文解释：**
- **所有者**：文件系统实现（ext4、NFS 等），静态分配，const 限定
- **调用者**：VFS 层，系统调用通过 VFS 分发到具体实现
- **不变量**：处理部分传输、尊重 O_NONBLOCK、正确更新文件位置
- **违规表现**：返回错误的字节数、忽略文件位置、用户空间访问未验证

### Example 2: `net_device_ops`

From `include/linux/netdevice.h`:

```c
struct net_device_ops {
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    /* ... 30+ more callbacks ... */
};
```

```
+------------------------------------------------------------------+
|  net_device_ops OWNERSHIP MODEL                                  |
+------------------------------------------------------------------+

    WHO OWNS:       Network driver (e1000, ixgbe, etc.)
                    - Statically allocated per driver type
                    - Assigned during driver probe
    
    WHO CALLS:      Network core / protocol stack
                    - dev_open() → ndo_open()
                    - dev_queue_xmit() → ndo_start_xmit()
    
    INVARIANTS:
    +---------------------------------------------------------+
    | ndo_start_xmit():                                       |
    | 1. MUST be atomic (softirq context)                     |
    | 2. MUST NOT sleep                                       |
    | 3. MUST consume or free the skb                         |
    | 4. Return NETDEV_TX_OK or NETDEV_TX_BUSY               |
    +---------------------------------------------------------+
    
    VIOLATIONS LOOK LIKE:
    +---------------------------------------------------------+
    | - Sleeping in ndo_start_xmit() → deadlock               |
    | - Leaking skb → memory exhaustion                       |
    | - Returning wrong status → packet loss                  |
    +---------------------------------------------------------+
```

**中文解释：**
- **所有者**：网络驱动（e1000、ixgbe 等）
- **调用者**：网络核心和协议栈
- **关键约束**：`ndo_start_xmit` 在软中断上下文执行，不能睡眠，必须消费或释放 skb
- **违规后果**：睡眠导致死锁、泄漏 skb 导致内存耗尽

### Example 3: `uart_ops`

From `include/linux/serial_core.h`:

```c
struct uart_ops {
    unsigned int (*tx_empty)(struct uart_port *);
    void (*set_mctrl)(struct uart_port *, unsigned int mctrl);
    unsigned int (*get_mctrl)(struct uart_port *);
    void (*stop_tx)(struct uart_port *);
    void (*start_tx)(struct uart_port *);
    void (*stop_rx)(struct uart_port *);
    void (*startup)(struct uart_port *);
    void (*shutdown)(struct uart_port *);
    /* ... */
};
```

```
+------------------------------------------------------------------+
|  uart_ops OWNERSHIP MODEL                                        |
+------------------------------------------------------------------+

    WHO OWNS:       UART hardware driver (pl011, 8250, etc.)
    
    WHO CALLS:      Serial core layer
                    - uart_startup() → ops->startup()
                    - uart_write() → ops->start_tx()
    
    INVARIANTS:
    +---------------------------------------------------------+
    | 1. tx_empty() must not sleep                            |
    | 2. start_tx() may be called in interrupt context        |
    | 3. startup() can sleep (process context)                |
    | 4. Must coordinate with IRQ handler                     |
    +---------------------------------------------------------+
    
    CONTEXT RULES:
    +---------------------------------------------------------+
    | Callback        | Context         | Can Sleep?          |
    |-----------------+-----------------|---------------------|
    | startup         | process         | YES                 |
    | shutdown        | process         | YES                 |
    | start_tx        | any (spinlock)  | NO                  |
    | stop_tx         | any (spinlock)  | NO                  |
    | tx_empty        | any             | NO                  |
    +---------------------------------------------------------+
```

**中文解释：**
- **所有者**：UART 硬件驱动（pl011、8250 等）
- **调用者**：串口核心层
- **上下文规则**：startup/shutdown 可睡眠，start_tx/stop_tx 在自旋锁下调用不可睡眠

### Example 4: `block_device_operations`

From `include/linux/blkdev.h`:

```c
struct block_device_operations {
    int (*open) (struct block_device *, fmode_t);
    int (*release) (struct gendisk *, fmode_t);
    int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
    /* ... */
    struct module *owner;
};
```

```
+------------------------------------------------------------------+
|  block_device_operations OWNERSHIP MODEL                         |
+------------------------------------------------------------------+

    WHO OWNS:       Block device driver (SCSI, NVMe, etc.)
    
    WHO CALLS:      Block layer
                    - blkdev_open() → bdev->bd_disk->fops->open()
    
    INVARIANTS:
    +---------------------------------------------------------+
    | 1. open() must handle concurrent opens                   |
    | 2. ioctl() must validate user arguments                  |
    | 3. owner field MUST be set for module refcounting        |
    +---------------------------------------------------------+
```

### Example 5: `usb_driver` ops

From `include/linux/usb.h`:

```c
struct usb_driver {
    const char *name;
    int (*probe) (struct usb_interface *intf, const struct usb_device_id *id);
    void (*disconnect) (struct usb_interface *intf);
    int (*suspend) (struct usb_interface *intf, pm_message_t message);
    int (*resume) (struct usb_interface *intf);
    /* ... */
    const struct usb_device_id *id_table;
};
```

```
+------------------------------------------------------------------+
|  usb_driver OWNERSHIP MODEL                                      |
+------------------------------------------------------------------+

    WHO OWNS:       USB device driver (usb-storage, usbhid, etc.)
    
    WHO CALLS:      USB core
                    - usb_probe_interface() → drv->probe()
                    - usb_unbind_interface() → drv->disconnect()
    
    INVARIANTS:
    +---------------------------------------------------------+
    | 1. probe() returns 0 on success, negative on failure    |
    | 2. disconnect() called before device removed            |
    | 3. id_table MUST be set for hotplug matching            |
    | 4. probe() may sleep                                    |
    +---------------------------------------------------------+
```

### Example 6: `tty_operations`

From `include/linux/tty_driver.h`:

```c
struct tty_operations {
    int (*open)(struct tty_struct *tty, struct file *filp);
    void (*close)(struct tty_struct *tty, struct file *filp);
    int (*write)(struct tty_struct *tty, const unsigned char *buf, int count);
    unsigned int (*write_room)(struct tty_struct *tty);
    void (*flush_buffer)(struct tty_struct *tty);
    /* ... */
};
```

```
+------------------------------------------------------------------+
|  tty_operations OWNERSHIP MODEL                                  |
+------------------------------------------------------------------+

    WHO OWNS:       TTY driver (serial, pty, console, etc.)
    
    WHO CALLS:      TTY core layer
                    - tty_open() → ops->open()
                    - tty_write() → ops->write()
    
    INVARIANTS:
    +---------------------------------------------------------+
    | 1. write() returns bytes written, may be partial        |
    | 2. write_room() returns available buffer space          |
    | 3. Must handle hangup during I/O                        |
    +---------------------------------------------------------+
```

---

## Invariants and Violations Summary

```
+------------------------------------------------------------------+
|  UNIVERSAL OPS TABLE INVARIANTS                                  |
+------------------------------------------------------------------+

    1. OWNERSHIP CLARITY
       - Ops table is statically allocated
       - Object points to ops, not vice versa
       - const qualifier prevents modification
    
    2. NULL HANDLING
       - NULL callback = feature not supported
       - Framework MUST check before calling
       - Or provide default implementation
    
    3. CONTEXT AWARENESS
       - Each callback has defined context rules
       - Atomic context: cannot sleep
       - Process context: may sleep
    
    4. OBJECT PASSING
       - First argument is always the context object
       - Enables implementation to access private data
       - via container_of()
    
    5. ERROR SEMANTICS
       - Negative return = error code
       - Zero = success (usually)
       - Positive = bytes/count (for I/O)

+------------------------------------------------------------------+
|  COMMON VIOLATIONS                                               |
+------------------------------------------------------------------+

    VIOLATION                     | CONSEQUENCE
    ------------------------------|-----------------------------
    Sleep in atomic callback      | Deadlock, system hang
    Forget NULL check             | Kernel oops
    Wrong return semantics        | Data corruption, hangs
    Modify const ops              | Undefined behavior
    Ignore context object         | Wrong private data
    Skip validation               | Security vulnerability
```

**中文解释：**
- **通用不变量**：
  1. 所有权明确：ops 表静态分配，const 限定
  2. NULL 处理：NULL 回调表示功能不支持
  3. 上下文感知：每个回调有明确的上下文规则
  4. 对象传递：第一个参数始终是上下文对象
  5. 错误语义：负数返回表示错误码

---

## Reusable User-Space Design Pattern

```c
/* user_space_ops_pattern.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------
 * Step 1: Define the ops table (contract)
 *---------------------------------------------------------*/
struct storage_ops {
    int (*open)(void *ctx);
    int (*close)(void *ctx);
    ssize_t (*read)(void *ctx, void *buf, size_t count);
    ssize_t (*write)(void *ctx, const void *buf, size_t count);
};

/*---------------------------------------------------------
 * Step 2: Define the context object
 *---------------------------------------------------------*/
struct storage_device {
    const char *name;
    const struct storage_ops *ops;  /* Points to ops table */
    void *private_data;              /* Implementation-specific */
};

/*---------------------------------------------------------
 * Step 3: Framework functions (dispatch layer)
 *---------------------------------------------------------*/
int storage_open(struct storage_device *dev)
{
    if (!dev || !dev->ops)
        return -1;
    if (dev->ops->open)
        return dev->ops->open(dev->private_data);
    return 0;  /* Default: success if not implemented */
}

ssize_t storage_read(struct storage_device *dev, void *buf, size_t count)
{
    if (!dev || !dev->ops || !dev->ops->read)
        return -1;
    return dev->ops->read(dev->private_data, buf, count);
}

ssize_t storage_write(struct storage_device *dev, const void *buf, size_t count)
{
    if (!dev || !dev->ops || !dev->ops->write)
        return -1;
    return dev->ops->write(dev->private_data, buf, count);
}

int storage_close(struct storage_device *dev)
{
    if (!dev || !dev->ops)
        return -1;
    if (dev->ops->close)
        return dev->ops->close(dev->private_data);
    return 0;
}

/*---------------------------------------------------------
 * Step 4: Implementation A - Memory storage
 *---------------------------------------------------------*/
struct mem_storage {
    char buffer[1024];
    size_t size;
    size_t pos;
};

static int mem_open(void *ctx)
{
    struct mem_storage *ms = ctx;
    ms->pos = 0;
    printf("[mem] opened\n");
    return 0;
}

static ssize_t mem_read(void *ctx, void *buf, size_t count)
{
    struct mem_storage *ms = ctx;
    size_t avail = ms->size - ms->pos;
    size_t to_read = (count < avail) ? count : avail;
    memcpy(buf, ms->buffer + ms->pos, to_read);
    ms->pos += to_read;
    return to_read;
}

static ssize_t mem_write(void *ctx, const void *buf, size_t count)
{
    struct mem_storage *ms = ctx;
    size_t avail = sizeof(ms->buffer) - ms->size;
    size_t to_write = (count < avail) ? count : avail;
    memcpy(ms->buffer + ms->size, buf, to_write);
    ms->size += to_write;
    return to_write;
}

static int mem_close(void *ctx)
{
    printf("[mem] closed\n");
    return 0;
}

static const struct storage_ops mem_storage_ops = {
    .open  = mem_open,
    .close = mem_close,
    .read  = mem_read,
    .write = mem_write,
};

/*---------------------------------------------------------
 * Step 5: Implementation B - Null storage
 *---------------------------------------------------------*/
static ssize_t null_write(void *ctx, const void *buf, size_t count)
{
    printf("[null] discarded %zu bytes\n", count);
    return count;  /* Pretend we wrote everything */
}

static ssize_t null_read(void *ctx, void *buf, size_t count)
{
    return 0;  /* EOF immediately */
}

static const struct storage_ops null_storage_ops = {
    .open  = NULL,  /* Optional - not implemented */
    .close = NULL,
    .read  = null_read,
    .write = null_write,
};

/*---------------------------------------------------------
 * Step 6: Usage - Framework is independent of implementation
 *---------------------------------------------------------*/
int main(void)
{
    /* Create memory storage */
    struct mem_storage ms = {0};
    struct storage_device mem_dev = {
        .name = "memory",
        .ops = &mem_storage_ops,
        .private_data = &ms,
    };
    
    /* Create null storage */
    struct storage_device null_dev = {
        .name = "null",
        .ops = &null_storage_ops,
        .private_data = NULL,
    };
    
    /* Same framework code works with both */
    storage_open(&mem_dev);
    storage_write(&mem_dev, "Hello, ops tables!", 18);
    
    char buf[32] = {0};
    storage_open(&mem_dev);  /* Reset position */
    storage_read(&mem_dev, buf, sizeof(buf));
    printf("Read from mem: %s\n", buf);
    storage_close(&mem_dev);
    
    /* Null storage */
    storage_open(&null_dev);
    storage_write(&null_dev, "This goes nowhere", 17);
    storage_close(&null_dev);
    
    return 0;
}
```

**中文解释：**
- 用户态复用模式：
  1. 定义 ops 表结构（契约）
  2. 定义上下文对象（包含 ops 指针和私有数据）
  3. 框架函数进行分发（检查 NULL、调用 ops）
  4. 实现提供具体的 ops 表
  5. 使用时框架代码与实现解耦

---

## Summary

```
+------------------------------------------------------------------+
|  OPS TABLE DESIGN PRINCIPLES                                     |
+------------------------------------------------------------------+

    1. CONTRACT FIRST
       - Define ops table = define the interface
       - Document invariants for each callback
    
    2. SEPARATION OF CONCERNS
       - Framework: timing and coordination
       - Implementation: actual behavior
    
    3. NULL AS FEATURE FLAG
       - NULL callback = optional feature
       - Framework provides defaults
    
    4. CONTEXT PASSING
       - First argument = context object
       - Enables access to private data
    
    5. STATIC ALLOCATION
       - Ops tables are compile-time constants
       - No runtime registration overhead
    
    6. EXPLICIT IS BETTER
       - No hidden vtables
       - No magic constructors
       - Every dispatch is visible in code
```

**中文总结：**
Ops 表是 Linux 内核实现多态的核心机制，其设计原则包括：
1. 契约优先：ops 表即接口定义
2. 关注点分离：框架负责时序，实现负责行为
3. NULL 作为特性标志
4. 上下文传递模式
5. 静态分配，编译期常量
6. 显式优于隐式，无隐藏开销

