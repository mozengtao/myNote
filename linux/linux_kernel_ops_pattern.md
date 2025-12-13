# Linux Kernel "Operations Structure" Design Pattern

## The Object-Oriented C Pattern for Polymorphism and Extensibility

This document explains one of the most fundamental and pervasive design patterns in the Linux kernel - the "Operations Structure" pattern (also known as "Virtual Function Table" or "Object-Oriented C" pattern).

---

## Table of Contents

1. [Pattern Overview](#1-pattern-overview)
2. [Core Idea and Design Philosophy](#2-core-idea-and-design-philosophy)
3. [Implementation Approach](#3-implementation-approach)
4. [Real Kernel Examples](#4-real-kernel-examples)
5. [Complete Code Example](#5-complete-code-example)
6. [Advanced Techniques](#6-advanced-techniques)
7. [Summary and Best Practices](#7-summary-and-best-practices)

---

## 1. Pattern Overview

### 1.1 The Pattern Structure

```
+============================================================================+
|                    OPERATIONS STRUCTURE PATTERN                             |
+============================================================================+
|                                                                             |
|   The pattern you observed in Linux kernel:                                 |
|                                                                             |
|   struct xxx {                                                              |
|       const struct xxx_ops *ops;    // <-- Operations (virtual methods)     |
|       u32 handle;                   // <-- Identity/reference               |
|       struct yyy *yyy_ref;          // <-- Related object reference         |
|       void *private_data;           // <-- Implementation-specific data     |
|       ...                           // <-- Other state/attributes           |
|   };                                                                        |
|                                                                             |
|   struct xxx_ops {                                                          |
|       int (*open)(struct xxx *);    // <-- "Virtual" methods                |
|       int (*close)(struct xxx *);   //     (function pointers)              |
|       ssize_t (*read)(struct xxx *, void *, size_t);                        |
|       ssize_t (*write)(struct xxx *, const void *, size_t);                 |
|       ...                                                                   |
|   };                                                                        |
|                                                                             |
|   This is "Object-Oriented Programming in C" - implementing:                |
|   - Polymorphism (same interface, different implementations)                |
|   - Encapsulation (data + operations bundled)                               |
|   - Inheritance-like behavior (delegation/composition)                      |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 这个模式是 Linux 内核中最基础的设计模式之一
- 核心是将"操作"（函数指针）与"数据"分离
- 实现了 C 语言中的多态性：相同接口，不同实现
- 类似于 C++ 中的虚函数表（vtable）

### 1.2 Comparison with C++ Classes

```
+============================================================================+
|                    C++ CLASS vs LINUX KERNEL PATTERN                        |
+============================================================================+
|                                                                             |
|   C++ Class:                          Linux Kernel C:                       |
|   ==================                  ===================                   |
|                                                                             |
|   class Device {                      struct device {                       |
|   public:                                 const struct device_ops *ops;     |
|       virtual int open() = 0;             void *private_data;               |
|       virtual void close() = 0;           /* ... other fields */            |
|       virtual int read(...) = 0;      };                                    |
|   protected:                                                                |
|       void *private_data;             struct device_ops {                   |
|   };                                      int (*open)(struct device *);     |
|                                           void (*close)(struct device *);   |
|   class USBDevice : public Device {       int (*read)(struct device *,      |
|       int open() override;                           void *, size_t);       |
|       void close() override;          };                                    |
|       int read(...) override;                                               |
|   };                                  /* "Subclass" via different ops */    |
|                                       static const struct device_ops        |
|                                           usb_device_ops = {                |
|                                           .open = usb_open,                 |
|                                           .close = usb_close,               |
|                                           .read = usb_read,                 |
|                                       };                                    |
|                                                                             |
|   Calling a virtual method:           Calling an "ops" function:            |
|   =========================           ==========================            |
|                                                                             |
|   device->open();                     device->ops->open(device);            |
|                                                                             |
+============================================================================+
```

**中文说明**：
- C++ 使用虚函数和继承实现多态
- Linux 内核使用函数指针结构体实现相同效果
- 调用方式：`obj->ops->method(obj, args)` 替代 `obj->method(args)`
- C 版本需要显式传递 `this` 指针（即结构体指针）

---

## 2. Core Idea and Design Philosophy

### 2.1 Why This Pattern?

```
+============================================================================+
|                         WHY USE THIS PATTERN?                               |
+============================================================================+
|                                                                             |
|   1. POLYMORPHISM WITHOUT C++                                               |
|   +---------------------------------------------------------------+        |
|   |                                                                |        |
|   |   The Linux kernel is written in C, not C++.                  |        |
|   |   This pattern provides polymorphism in pure C:               |        |
|   |                                                                |        |
|   |   - Same interface (struct xxx_ops)                           |        |
|   |   - Different implementations (different ops instances)       |        |
|   |   - Runtime dispatch (call via function pointer)              |        |
|   |                                                                |        |
|   +---------------------------------------------------------------+        |
|                                                                             |
|   2. SEPARATION OF INTERFACE AND IMPLEMENTATION                             |
|   +---------------------------------------------------------------+        |
|   |                                                                |        |
|   |   Core subsystem defines:     Implementation provides:        |        |
|   |   - struct xxx               - Specific xxx_ops instance      |        |
|   |   - struct xxx_ops           - Actual function bodies         |        |
|   |   - Generic helper functions - Private data interpretation    |        |
|   |                                                                |        |
|   +---------------------------------------------------------------+        |
|                                                                             |
|   3. EXTENSIBILITY                                                          |
|   +---------------------------------------------------------------+        |
|   |                                                                |        |
|   |   Adding new "subclasses" requires:                           |        |
|   |   - Writing new ops function implementations                  |        |
|   |   - Creating a new ops structure instance                     |        |
|   |   - NO changes to core infrastructure code!                   |        |
|   |                                                                |        |
|   +---------------------------------------------------------------+        |
|                                                                             |
|   4. OPTIONAL OPERATIONS                                                    |
|   +---------------------------------------------------------------+        |
|   |                                                                |        |
|   |   Function pointers can be NULL = optional operation          |        |
|   |   Caller checks: if (ops->foo) ops->foo(obj);                 |        |
|   |   Or: if (ops->foo) return ops->foo(obj); return -ENOSYS;     |        |
|   |                                                                |        |
|   +---------------------------------------------------------------+        |
|                                                                             |
|   5. MEMORY EFFICIENCY                                                      |
|   +---------------------------------------------------------------+        |
|   |                                                                |        |
|   |   Multiple instances share the SAME ops structure:            |        |
|   |                                                                |        |
|   |   struct xxx obj1 = { .ops = &common_ops, ... };             |        |
|   |   struct xxx obj2 = { .ops = &common_ops, ... };             |        |
|   |   struct xxx obj3 = { .ops = &common_ops, ... };             |        |
|   |                                                                |        |
|   |   ops structure is typically "const" (read-only, in .rodata)  |        |
|   |                                                                |        |
|   +---------------------------------------------------------------+        |
|                                                                             |
+============================================================================+
```

**中文说明**：
1. **多态性**：在纯 C 中实现相同接口、不同实现
2. **接口与实现分离**：核心代码定义接口，具体实现可独立开发
3. **可扩展性**：添加新"子类"无需修改核心代码
4. **可选操作**：函数指针为 NULL 表示不支持该操作
5. **内存效率**：多个对象共享同一个 ops 结构体

### 2.2 The Three Key Components

```
+============================================================================+
|                       THREE KEY COMPONENTS                                  |
+============================================================================+
|                                                                             |
|   +-------------------------------------------------------------------+    |
|   |                                                                    |    |
|   |   COMPONENT 1: The "Object" Structure (Data + ops pointer)       |    |
|   |   =======================================================         |    |
|   |                                                                    |    |
|   |   struct net_device {                                             |    |
|   |       char name[IFNAMSIZ];           // Object identity           |    |
|   |       const struct net_device_ops *netdev_ops;  // <-- ops ptr   |    |
|   |       struct net *nd_net;            // Reference to network ns   |    |
|   |       void *priv;                    // Driver-specific data      |    |
|   |       unsigned int flags;            // Object state              |    |
|   |       /* ... hundreds more fields */                              |    |
|   |   };                                                               |    |
|   |                                                                    |    |
|   +-------------------------------------------------------------------+    |
|                                                                             |
|   +-------------------------------------------------------------------+    |
|   |                                                                    |    |
|   |   COMPONENT 2: The "Operations" Structure (Virtual Methods)      |    |
|   |   ==========================================================      |    |
|   |                                                                    |    |
|   |   struct net_device_ops {                                         |    |
|   |       int  (*ndo_open)(struct net_device *dev);                  |    |
|   |       int  (*ndo_stop)(struct net_device *dev);                  |    |
|   |       netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,         |    |
|   |                                      struct net_device *dev);    |    |
|   |       void (*ndo_set_rx_mode)(struct net_device *dev);           |    |
|   |       int  (*ndo_set_mac_address)(struct net_device *dev,        |    |
|   |                                    void *addr);                   |    |
|   |       /* ... many more operations */                              |    |
|   |   };                                                               |    |
|   |                                                                    |    |
|   +-------------------------------------------------------------------+    |
|                                                                             |
|   +-------------------------------------------------------------------+    |
|   |                                                                    |    |
|   |   COMPONENT 3: The Implementation (Specific ops instance)        |    |
|   |   ========================================================        |    |
|   |                                                                    |    |
|   |   /* virtio_net driver provides these implementations */          |    |
|   |   static const struct net_device_ops virtnet_netdev = {          |    |
|   |       .ndo_open           = virtnet_open,                        |    |
|   |       .ndo_stop           = virtnet_close,                       |    |
|   |       .ndo_start_xmit     = start_xmit,                          |    |
|   |       .ndo_set_rx_mode    = virtnet_set_rx_mode,                 |    |
|   |       .ndo_set_mac_address = virtnet_set_mac_address,            |    |
|   |   };                                                               |    |
|   |                                                                    |    |
|   |   /* e1000 driver provides different implementations */           |    |
|   |   static const struct net_device_ops e1000_netdev_ops = {        |    |
|   |       .ndo_open           = e1000_open,                          |    |
|   |       .ndo_stop           = e1000_close,                         |    |
|   |       .ndo_start_xmit     = e1000_xmit_frame,                    |    |
|   |       .ndo_set_rx_mode    = e1000_set_rx_mode,                   |    |
|   |       .ndo_set_mac_address = e1000_set_mac,                      |    |
|   |   };                                                               |    |
|   |                                                                    |    |
|   +-------------------------------------------------------------------+    |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **组件 1（对象结构体）**：包含数据字段和指向 ops 的指针
- **组件 2（操作结构体）**：纯函数指针的集合，定义接口
- **组件 3（具体实现）**：每个"子类"提供自己的 ops 实例

---

## 3. Implementation Approach

### 3.1 Step-by-Step Implementation

```
+============================================================================+
|                     IMPLEMENTATION STEPS                                    |
+============================================================================+
|                                                                             |
|   STEP 1: Define the Operations Structure (Interface)                      |
|   ====================================================                      |
|                                                                             |
|   /* Forward declaration */                                                 |
|   struct my_device;                                                         |
|                                                                             |
|   /* Define the "interface" - all function pointers */                     |
|   struct my_device_ops {                                                    |
|       /* Required operations */                                             |
|       int (*open)(struct my_device *dev);                                  |
|       void (*close)(struct my_device *dev);                                |
|                                                                             |
|       /* Optional operations (can be NULL) */                              |
|       int (*ioctl)(struct my_device *dev, unsigned int cmd, void *arg);   |
|       int (*suspend)(struct my_device *dev);                               |
|       int (*resume)(struct my_device *dev);                                |
|   };                                                                        |
|                                                                             |
|   STEP 2: Define the Object Structure                                       |
|   ====================================                                      |
|                                                                             |
|   struct my_device {                                                        |
|       /* Identity */                                                        |
|       char name[32];                                                        |
|       unsigned int id;                                                      |
|                                                                             |
|       /* Operations - THE KEY! */                                          |
|       const struct my_device_ops *ops;                                     |
|                                                                             |
|       /* References to related objects */                                   |
|       struct my_bus *bus;                                                   |
|       struct device *parent;                                               |
|                                                                             |
|       /* State */                                                           |
|       unsigned int flags;                                                   |
|       atomic_t refcount;                                                    |
|                                                                             |
|       /* Private data for implementations */                               |
|       void *private_data;                                                   |
|   };                                                                        |
|                                                                             |
|   STEP 3: Provide Generic Wrapper Functions (Optional but recommended)     |
|   =====================================================================    |
|                                                                             |
|   static inline int my_device_open(struct my_device *dev)                  |
|   {                                                                         |
|       /* Check if operation is supported */                                |
|       if (!dev->ops->open)                                                 |
|           return -ENOSYS;                                                   |
|       return dev->ops->open(dev);                                          |
|   }                                                                         |
|                                                                             |
|   static inline int my_device_ioctl(struct my_device *dev,                 |
|                                      unsigned int cmd, void *arg)          |
|   {                                                                         |
|       if (!dev->ops->ioctl)                                                |
|           return -ENOTSUPP;                                                 |
|       return dev->ops->ioctl(dev, cmd, arg);                               |
|   }                                                                         |
|                                                                             |
|   STEP 4: Create Specific Implementations                                   |
|   ========================================                                  |
|                                                                             |
|   /* Implementation A */                                                    |
|   static int impl_a_open(struct my_device *dev) { ... }                    |
|   static void impl_a_close(struct my_device *dev) { ... }                  |
|   static int impl_a_ioctl(struct my_device *dev, ...) { ... }              |
|                                                                             |
|   static const struct my_device_ops impl_a_ops = {                         |
|       .open  = impl_a_open,                                                |
|       .close = impl_a_close,                                               |
|       .ioctl = impl_a_ioctl,                                               |
|       /* .suspend = NULL - not supported */                                |
|       /* .resume  = NULL - not supported */                                |
|   };                                                                        |
|                                                                             |
|   /* Implementation B */                                                    |
|   static int impl_b_open(struct my_device *dev) { ... }                    |
|   static void impl_b_close(struct my_device *dev) { ... }                  |
|   static int impl_b_suspend(struct my_device *dev) { ... }                 |
|   static int impl_b_resume(struct my_device *dev) { ... }                  |
|                                                                             |
|   static const struct my_device_ops impl_b_ops = {                         |
|       .open    = impl_b_open,                                              |
|       .close   = impl_b_close,                                             |
|       /* .ioctl = NULL - not supported */                                  |
|       .suspend = impl_b_suspend,                                           |
|       .resume  = impl_b_resume,                                            |
|   };                                                                        |
|                                                                             |
|   STEP 5: Instantiate Objects with Appropriate ops                         |
|   =================================================                        |
|                                                                             |
|   struct my_device *create_device_a(void)                                  |
|   {                                                                         |
|       struct my_device *dev = kzalloc(sizeof(*dev), GFP_KERNEL);           |
|       dev->ops = &impl_a_ops;   /* <-- Assign implementation A */          |
|       return dev;                                                           |
|   }                                                                         |
|                                                                             |
|   struct my_device *create_device_b(void)                                  |
|   {                                                                         |
|       struct my_device *dev = kzalloc(sizeof(*dev), GFP_KERNEL);           |
|       dev->ops = &impl_b_ops;   /* <-- Assign implementation B */          |
|       return dev;                                                           |
|   }                                                                         |
|                                                                             |
+============================================================================+
```

**中文说明**：
1. **定义操作结构体**：声明所有可能的操作作为函数指针
2. **定义对象结构体**：包含 ops 指针和其他数据字段
3. **提供包装函数**：简化调用，处理 NULL 检查
4. **创建具体实现**：每个"子类"实现自己的函数，填充 ops
5. **实例化对象**：创建对象时，设置正确的 ops 指针

---

## 4. Real Kernel Examples

### 4.1 File Operations (VFS)

```c
/* include/linux/fs.h */

struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    /* ... many more */
};

struct file {
    struct path             f_path;
    const struct file_operations    *f_op;  /* <-- ops pointer */
    spinlock_t              f_lock;
    atomic_long_t           f_count;
    unsigned int            f_flags;
    fmode_t                 f_mode;
    void                    *private_data;
    /* ... */
};

/* Example: /dev/null implementation */
static ssize_t null_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    return count;  /* Just discard everything */
}

static ssize_t null_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    return 0;  /* EOF immediately */
}

static const struct file_operations null_fops = {
    .read  = null_read,
    .write = null_write,
    .llseek = noop_llseek,
};
```

### 4.2 Network Device Operations

```c
/* include/linux/netdevice.h */

struct net_device_ops {
    int  (*ndo_open)(struct net_device *dev);
    int  (*ndo_stop)(struct net_device *dev);
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    struct rtnl_link_stats64* (*ndo_get_stats64)(struct net_device *dev,
                                    struct rtnl_link_stats64 *storage);
    /* ... 50+ more operations */
};

struct net_device {
    char                    name[IFNAMSIZ];
    const struct net_device_ops *netdev_ops;  /* <-- ops pointer */
    const struct ethtool_ops    *ethtool_ops; /* Another ops! */
    struct net              *nd_net;
    unsigned int            flags;
    void                    *priv;
    /* ... hundreds more fields */
};

/* Example: virtio_net implementation */
static const struct net_device_ops virtnet_netdev = {
    .ndo_open            = virtnet_open,
    .ndo_stop            = virtnet_close,
    .ndo_start_xmit      = start_xmit,
    .ndo_set_mac_address = virtnet_set_mac_address,
    .ndo_set_rx_mode     = virtnet_set_rx_mode,
    .ndo_change_mtu      = virtnet_change_mtu,
    .ndo_get_stats64     = virtnet_stats,
};
```

### 4.3 Block Device Operations

```c
/* include/linux/blkdev.h */

struct block_device_operations {
    int (*open) (struct block_device *, fmode_t);
    int (*release) (struct gendisk *, fmode_t);
    int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    int (*compat_ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    int (*media_changed) (struct gendisk *);
    int (*revalidate_disk) (struct gendisk *);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
    struct module *owner;
};

struct gendisk {
    int major;
    int first_minor;
    char disk_name[DISK_NAME_LEN];
    const struct block_device_operations *fops;  /* <-- ops pointer */
    struct request_queue *queue;
    void *private_data;
    /* ... */
};
```

---

## 5. Complete Code Example

Here's a complete, compilable example demonstrating the pattern:

```c
/*
 * linux_kernel_ops_pattern_example.c
 * 
 * A complete example demonstrating the Linux kernel "Operations Structure"
 * design pattern. This creates a simple "sensor" abstraction with multiple
 * implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*============================================================================
 * STEP 1: Define the Operations Structure (Interface)
 *============================================================================
 * This defines the "interface" - what operations a sensor can perform.
 * All function pointers take the object as their first argument (like 'this').
 */

/* Forward declaration - needed because ops refers to struct sensor */
struct sensor;

/* The operations structure - defines the interface */
struct sensor_ops {
    /*
     * Initialize the sensor hardware.
     * Returns 0 on success, negative error code on failure.
     */
    int (*init)(struct sensor *s);
    
    /*
     * Read a value from the sensor.
     * Returns 0 on success, negative error code on failure.
     * Value is stored in *value.
     */
    int (*read)(struct sensor *s, int32_t *value);
    
    /*
     * Set sensor configuration parameter.
     * Optional - can be NULL if not supported.
     */
    int (*set_config)(struct sensor *s, unsigned int param, int value);
    
    /*
     * Get sensor configuration parameter.
     * Optional - can be NULL if not supported.
     */
    int (*get_config)(struct sensor *s, unsigned int param, int *value);
    
    /*
     * Put sensor into low-power mode.
     * Optional - can be NULL if not supported.
     */
    int (*suspend)(struct sensor *s);
    
    /*
     * Wake sensor from low-power mode.
     * Optional - can be NULL if not supported.
     */
    int (*resume)(struct sensor *s);
    
    /*
     * Cleanup and release resources.
     */
    void (*destroy)(struct sensor *s);
};

/*============================================================================
 * STEP 2: Define the Object Structure
 *============================================================================
 * The "base class" structure containing common data and the ops pointer.
 */

struct sensor {
    /* Identity */
    char                    name[32];       /* Human-readable name */
    unsigned int            id;             /* Unique identifier */
    
    /* THE KEY: pointer to operations */
    const struct sensor_ops *ops;           /* Implementation-specific ops */
    
    /* References to related objects (like in kernel patterns) */
    struct sensor_bus       *bus;           /* Parent bus (could be I2C, SPI, etc) */
    
    /* State */
    unsigned int            flags;          /* Status flags */
    #define SENSOR_FLAG_INITIALIZED  (1 << 0)
    #define SENSOR_FLAG_SUSPENDED    (1 << 1)
    
    /* Private data - interpretation depends on implementation */
    void                    *private_data;  /* Implementation-specific data */
};

/* Simulated "bus" structure (like i2c_adapter, spi_master in kernel) */
struct sensor_bus {
    char name[32];
    int bus_id;
};

/*============================================================================
 * STEP 3: Provide Generic Wrapper Functions
 *============================================================================
 * These provide a clean API and handle NULL checks for optional operations.
 */

/* Error codes (like kernel error codes) */
#define SENSOR_OK           0
#define SENSOR_ERR_NOTSUP  -1   /* Operation not supported */
#define SENSOR_ERR_INVAL   -2   /* Invalid argument */
#define SENSOR_ERR_IO      -3   /* I/O error */

/* Wrapper for init - required operation */
static inline int sensor_init(struct sensor *s)
{
    if (!s || !s->ops || !s->ops->init)
        return SENSOR_ERR_INVAL;
    return s->ops->init(s);
}

/* Wrapper for read - required operation */
static inline int sensor_read(struct sensor *s, int32_t *value)
{
    if (!s || !s->ops || !s->ops->read)
        return SENSOR_ERR_INVAL;
    return s->ops->read(s, value);
}

/* Wrapper for set_config - optional operation */
static inline int sensor_set_config(struct sensor *s, unsigned int param, int value)
{
    if (!s || !s->ops)
        return SENSOR_ERR_INVAL;
    if (!s->ops->set_config)         /* Optional: check if supported */
        return SENSOR_ERR_NOTSUP;
    return s->ops->set_config(s, param, value);
}

/* Wrapper for suspend - optional operation */
static inline int sensor_suspend(struct sensor *s)
{
    if (!s || !s->ops)
        return SENSOR_ERR_INVAL;
    if (!s->ops->suspend)            /* Optional: just succeed if not supported */
        return SENSOR_OK;
    return s->ops->suspend(s);
}

/* Wrapper for resume - optional operation */
static inline int sensor_resume(struct sensor *s)
{
    if (!s || !s->ops)
        return SENSOR_ERR_INVAL;
    if (!s->ops->resume)
        return SENSOR_OK;
    return s->ops->resume(s);
}

/* Wrapper for destroy - cleanup */
static inline void sensor_destroy(struct sensor *s)
{
    if (s && s->ops && s->ops->destroy)
        s->ops->destroy(s);
}

/*============================================================================
 * STEP 4: Implementation A - "Temperature Sensor" (e.g., DS18B20)
 *============================================================================
 * This is like a specific driver implementing the interface.
 */

/* Private data structure for temperature sensor */
struct temp_sensor_priv {
    int calibration_offset;
    int current_temp;  /* Simulated reading */
};

/* Init for temperature sensor */
static int temp_sensor_init(struct sensor *s)
{
    struct temp_sensor_priv *priv;
    
    printf("[TEMP] Initializing temperature sensor '%s'\n", s->name);
    
    /* Allocate private data */
    priv = malloc(sizeof(*priv));
    if (!priv)
        return SENSOR_ERR_IO;
    
    priv->calibration_offset = 0;
    priv->current_temp = 2500;  /* 25.00 degrees (fixed point) */
    
    s->private_data = priv;     /* Store in object */
    s->flags |= SENSOR_FLAG_INITIALIZED;
    
    printf("[TEMP] Initialization complete\n");
    return SENSOR_OK;
}

/* Read for temperature sensor */
static int temp_sensor_read(struct sensor *s, int32_t *value)
{
    struct temp_sensor_priv *priv = s->private_data;
    
    if (!(s->flags & SENSOR_FLAG_INITIALIZED))
        return SENSOR_ERR_INVAL;
    
    /* Simulate reading (in real driver, would access hardware) */
    priv->current_temp += (rand() % 100) - 50;  /* Random fluctuation */
    *value = priv->current_temp + priv->calibration_offset;
    
    printf("[TEMP] Read temperature: %d.%02d C\n", 
           *value / 100, abs(*value % 100));
    return SENSOR_OK;
}

/* Set config for temperature sensor */
static int temp_sensor_set_config(struct sensor *s, unsigned int param, int value)
{
    struct temp_sensor_priv *priv = s->private_data;
    
    switch (param) {
    case 0:  /* Calibration offset */
        printf("[TEMP] Setting calibration offset to %d\n", value);
        priv->calibration_offset = value;
        return SENSOR_OK;
    default:
        return SENSOR_ERR_INVAL;
    }
}

/* Destroy for temperature sensor */
static void temp_sensor_destroy(struct sensor *s)
{
    printf("[TEMP] Destroying temperature sensor '%s'\n", s->name);
    free(s->private_data);
    s->private_data = NULL;
}

/* THE OPERATIONS STRUCTURE INSTANCE for temperature sensor */
static const struct sensor_ops temp_sensor_ops = {
    .init       = temp_sensor_init,
    .read       = temp_sensor_read,
    .set_config = temp_sensor_set_config,
    .get_config = NULL,     /* Not implemented */
    .suspend    = NULL,     /* Not implemented */
    .resume     = NULL,     /* Not implemented */
    .destroy    = temp_sensor_destroy,
};

/*============================================================================
 * STEP 4: Implementation B - "Accelerometer Sensor" (e.g., ADXL345)
 *============================================================================
 * A different implementation with different capabilities.
 */

/* Private data structure for accelerometer */
struct accel_sensor_priv {
    int sample_rate;    /* Hz */
    int sensitivity;    /* G range */
    int x, y, z;        /* Current readings */
};

/* Init for accelerometer */
static int accel_sensor_init(struct sensor *s)
{
    struct accel_sensor_priv *priv;
    
    printf("[ACCEL] Initializing accelerometer '%s'\n", s->name);
    
    priv = malloc(sizeof(*priv));
    if (!priv)
        return SENSOR_ERR_IO;
    
    priv->sample_rate = 100;  /* 100 Hz default */
    priv->sensitivity = 2;    /* +/- 2G default */
    priv->x = priv->y = priv->z = 0;
    
    s->private_data = priv;
    s->flags |= SENSOR_FLAG_INITIALIZED;
    
    printf("[ACCEL] Initialized at %d Hz, +/-%dG\n", 
           priv->sample_rate, priv->sensitivity);
    return SENSOR_OK;
}

/* Read for accelerometer - returns magnitude */
static int accel_sensor_read(struct sensor *s, int32_t *value)
{
    struct accel_sensor_priv *priv = s->private_data;
    
    if (s->flags & SENSOR_FLAG_SUSPENDED) {
        printf("[ACCEL] Cannot read while suspended!\n");
        return SENSOR_ERR_INVAL;
    }
    
    /* Simulate reading */
    priv->x = (rand() % 2000) - 1000;
    priv->y = (rand() % 2000) - 1000;
    priv->z = (rand() % 2000) - 1000 + 1000; /* Gravity on Z */
    
    /* Return magnitude (simplified) */
    *value = abs(priv->x) + abs(priv->y) + abs(priv->z);
    
    printf("[ACCEL] Read: X=%d, Y=%d, Z=%d, magnitude=%d\n",
           priv->x, priv->y, priv->z, *value);
    return SENSOR_OK;
}

/* Set config for accelerometer */
static int accel_sensor_set_config(struct sensor *s, unsigned int param, int value)
{
    struct accel_sensor_priv *priv = s->private_data;
    
    switch (param) {
    case 0:  /* Sample rate */
        if (value < 1 || value > 1000)
            return SENSOR_ERR_INVAL;
        printf("[ACCEL] Setting sample rate to %d Hz\n", value);
        priv->sample_rate = value;
        return SENSOR_OK;
    case 1:  /* Sensitivity */
        if (value != 2 && value != 4 && value != 8 && value != 16)
            return SENSOR_ERR_INVAL;
        printf("[ACCEL] Setting sensitivity to +/-%dG\n", value);
        priv->sensitivity = value;
        return SENSOR_OK;
    default:
        return SENSOR_ERR_INVAL;
    }
}

/* Suspend for accelerometer - puts into low power mode */
static int accel_sensor_suspend(struct sensor *s)
{
    printf("[ACCEL] Entering low-power suspend mode\n");
    s->flags |= SENSOR_FLAG_SUSPENDED;
    return SENSOR_OK;
}

/* Resume for accelerometer */
static int accel_sensor_resume(struct sensor *s)
{
    printf("[ACCEL] Resuming from suspend\n");
    s->flags &= ~SENSOR_FLAG_SUSPENDED;
    return SENSOR_OK;
}

/* Destroy for accelerometer */
static void accel_sensor_destroy(struct sensor *s)
{
    printf("[ACCEL] Destroying accelerometer '%s'\n", s->name);
    free(s->private_data);
    s->private_data = NULL;
}

/* THE OPERATIONS STRUCTURE INSTANCE for accelerometer */
static const struct sensor_ops accel_sensor_ops = {
    .init       = accel_sensor_init,
    .read       = accel_sensor_read,
    .set_config = accel_sensor_set_config,
    .get_config = NULL,     /* Not implemented */
    .suspend    = accel_sensor_suspend,    /* Implemented! */
    .resume     = accel_sensor_resume,     /* Implemented! */
    .destroy    = accel_sensor_destroy,
};

/*============================================================================
 * STEP 5: Factory Functions to Create Sensor Objects
 *============================================================================
 */

struct sensor *create_temp_sensor(const char *name, unsigned int id,
                                   struct sensor_bus *bus)
{
    struct sensor *s = malloc(sizeof(*s));
    if (!s)
        return NULL;
    
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->id = id;
    s->bus = bus;
    s->ops = &temp_sensor_ops;   /* <-- Assign temperature sensor ops */
    
    return s;
}

struct sensor *create_accel_sensor(const char *name, unsigned int id,
                                    struct sensor_bus *bus)
{
    struct sensor *s = malloc(sizeof(*s));
    if (!s)
        return NULL;
    
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->id = id;
    s->bus = bus;
    s->ops = &accel_sensor_ops;  /* <-- Assign accelerometer ops */
    
    return s;
}

/*============================================================================
 * STEP 6: Generic Code That Works with ANY Sensor
 *============================================================================
 * This demonstrates polymorphism - same code works with different sensors.
 */

void test_sensor(struct sensor *s)
{
    int32_t value;
    int ret;
    
    printf("\n=== Testing sensor: %s (id=%u) ===\n", s->name, s->id);
    
    /* Initialize */
    ret = sensor_init(s);
    if (ret != SENSOR_OK) {
        printf("Failed to init sensor: %d\n", ret);
        return;
    }
    
    /* Read some values */
    for (int i = 0; i < 3; i++) {
        ret = sensor_read(s, &value);
        if (ret != SENSOR_OK) {
            printf("Failed to read: %d\n", ret);
        }
    }
    
    /* Try to configure (may or may not be supported) */
    ret = sensor_set_config(s, 0, 42);
    if (ret == SENSOR_ERR_NOTSUP) {
        printf("Configuration not supported for this sensor\n");
    }
    
    /* Try suspend/resume (may or may not be supported) */
    printf("Attempting suspend...\n");
    sensor_suspend(s);  /* Wrapper handles NULL ops gracefully */
    
    printf("Attempting read while possibly suspended...\n");
    sensor_read(s, &value);
    
    printf("Attempting resume...\n");
    sensor_resume(s);
    
    /* Final read */
    sensor_read(s, &value);
}

/*============================================================================
 * MAIN - Demonstrate the Pattern
 *============================================================================
 */

int main()
{
    /* Create a simulated bus */
    struct sensor_bus i2c_bus = { .name = "I2C-1", .bus_id = 1 };
    
    printf("========================================\n");
    printf("Linux Kernel Ops Pattern Demonstration\n");
    printf("========================================\n");
    
    /* Create different types of sensors - SAME interface, DIFFERENT behavior */
    struct sensor *temp = create_temp_sensor("TempSensor0", 1, &i2c_bus);
    struct sensor *accel = create_accel_sensor("Accelerometer0", 2, &i2c_bus);
    
    /* Test both using IDENTICAL code path */
    test_sensor(temp);
    test_sensor(accel);
    
    /* Cleanup */
    sensor_destroy(temp);
    sensor_destroy(accel);
    
    free(temp);
    free(accel);
    
    printf("\n========================================\n");
    printf("Demonstration complete!\n");
    printf("========================================\n");
    
    return 0;
}

/*
 * COMPILATION:
 *   gcc -o sensor_demo linux_kernel_ops_pattern_example.c
 *
 * SAMPLE OUTPUT:
 * ========================================
 * Linux Kernel Ops Pattern Demonstration
 * ========================================
 * 
 * === Testing sensor: TempSensor0 (id=1) ===
 * [TEMP] Initializing temperature sensor 'TempSensor0'
 * [TEMP] Initialization complete
 * [TEMP] Read temperature: 25.23 C
 * [TEMP] Read temperature: 24.89 C
 * [TEMP] Read temperature: 25.12 C
 * [TEMP] Setting calibration offset to 42
 * Attempting suspend...
 * Attempting read while possibly suspended...
 * [TEMP] Read temperature: 25.54 C
 * Attempting resume...
 * [TEMP] Read temperature: 25.01 C
 * 
 * === Testing sensor: Accelerometer0 (id=2) ===
 * [ACCEL] Initializing accelerometer 'Accelerometer0'
 * [ACCEL] Initialized at 100 Hz, +/-2G
 * [ACCEL] Read: X=123, Y=-456, Z=1789, magnitude=2368
 * [ACCEL] Read: X=-234, Y=567, Z=1234, magnitude=2035
 * [ACCEL] Read: X=345, Y=-678, Z=1456, magnitude=2479
 * [ACCEL] Setting sample rate to 42 Hz
 * Attempting suspend...
 * [ACCEL] Entering low-power suspend mode
 * Attempting read while possibly suspended...
 * [ACCEL] Cannot read while suspended!
 * Attempting resume...
 * [ACCEL] Resuming from suspend
 * [ACCEL] Read: X=456, Y=789, Z=1567, magnitude=2812
 * [TEMP] Destroying temperature sensor 'TempSensor0'
 * [ACCEL] Destroying accelerometer 'Accelerometer0'
 * 
 * ========================================
 * Demonstration complete!
 * ========================================
 */
```

---

## 6. Advanced Techniques

### 6.1 Multiple ops Structures in One Object

```c
/* Real kernel example: net_device has MULTIPLE ops pointers */
struct net_device {
    const struct net_device_ops  *netdev_ops;    /* Core operations */
    const struct ethtool_ops     *ethtool_ops;   /* Ethtool operations */
    const struct header_ops      *header_ops;    /* Header operations */
    const struct dcbnl_rtnl_ops  *dcbnl_ops;     /* DCB operations */
};

/* This allows different "aspects" of behavior to be customized independently */
```

### 6.2 Default Operations

```c
/* Provide defaults that implementations can override */
static int default_open(struct my_device *dev)
{
    return 0;  /* Default: just succeed */
}

static int default_ioctl(struct my_device *dev, unsigned int cmd, void *arg)
{
    return -ENOTSUPP;  /* Default: not supported */
}

static const struct my_device_ops default_ops = {
    .open  = default_open,
    .ioctl = default_ioctl,
    /* ... */
};

/* Implementation can use defaults for some, override others */
static const struct my_device_ops my_impl_ops = {
    .open  = default_open,        /* Use default */
    .close = my_impl_close,       /* Custom */
    .ioctl = my_impl_ioctl,       /* Custom */
};
```

### 6.3 Container-Of Pattern (Getting Private Data)

```c
/*
 * The "container_of" pattern is often used with ops pattern
 * to get from the base structure to the derived structure.
 */

/* "Derived" structure embeds the "base" */
struct my_specific_device {
    struct my_device base;    /* Base at offset 0 (or use container_of) */
    int specific_field;
    char specific_data[256];
};

/* In ops implementation, get the specific device */
static int my_specific_read(struct my_device *dev)
{
    struct my_specific_device *specific = 
        container_of(dev, struct my_specific_device, base);
    
    /* Now can access specific->specific_field, etc. */
    return 0;
}
```

---

## 7. Summary and Best Practices

```
+============================================================================+
|                         SUMMARY AND BEST PRACTICES                          |
+============================================================================+
|                                                                             |
|   PATTERN NAME:                                                             |
|   - "Operations Structure" Pattern                                          |
|   - Also called: Virtual Function Table, Object-Oriented C, Callbacks      |
|                                                                             |
|   CORE IDEA:                                                                |
|   - Separate interface (ops structure) from implementation                  |
|   - Use function pointers for polymorphic dispatch                         |
|   - Object carries pointer to its ops, allowing runtime behavior change    |
|                                                                             |
|   BEST PRACTICES:                                                           |
|                                                                             |
|   1. Mark ops structures as "const"                                        |
|      - Prevents accidental modification                                    |
|      - Allows placement in read-only memory                                |
|      - Example: const struct xxx_ops my_ops = { ... };                     |
|                                                                             |
|   2. First argument should always be the object itself                     |
|      - Like 'this' pointer in C++                                          |
|      - Example: int (*read)(struct xxx *obj, void *buf, size_t len);       |
|                                                                             |
|   3. Use wrapper functions for cleaner API                                 |
|      - Handle NULL checks                                                   |
|      - Provide default behavior for optional operations                    |
|      - Example: static inline int xxx_read(struct xxx *obj, ...) { ... }   |
|                                                                             |
|   4. Document which operations are required vs optional                    |
|      - Required: must not be NULL                                          |
|      - Optional: can be NULL, caller must check                            |
|                                                                             |
|   5. Use designated initializers for clarity                               |
|      - Example: .read = my_read, .write = my_write,                        |
|      - Unspecified members default to NULL/0                               |
|                                                                             |
|   TYPICAL USE CASES:                                                        |
|                                                                             |
|   - Device drivers (file_operations, net_device_ops, block_device_ops)     |
|   - Filesystems (file_operations, inode_operations, super_operations)      |
|   - Network protocols (proto_ops, packet_type)                             |
|   - Schedulers (sched_class)                                               |
|   - Memory allocators (kmem_cache operations)                              |
|   - Any subsystem with pluggable implementations                           |
|                                                                             |
+============================================================================+
```

**中文说明**：

**模式名称**：
- 操作结构体模式（Operations Structure Pattern）
- 也称为：虚函数表、面向对象 C、回调模式

**核心思想**：
- 将接口（ops 结构体）与实现分离
- 使用函数指针实现多态分发
- 对象携带指向其 ops 的指针，允许运行时行为变化

**最佳实践**：
1. 将 ops 结构体标记为 `const`
2. 第一个参数始终是对象本身（类似 `this` 指针）
3. 使用包装函数提供更清晰的 API
4. 记录哪些操作是必需的，哪些是可选的
5. 使用指定初始化器提高可读性

**典型用例**：
- 设备驱动（file_operations, net_device_ops）
- 文件系统（inode_operations, super_operations）
- 网络协议（proto_ops）
- 调度器（sched_class）
- 任何需要可插拔实现的子系统

---

## 8. Deep Dive: Understanding `s->ops->oper_xxx(s)`

This section explains exactly what happens when you call `s->ops->oper_xxx(s)` - the fundamental calling convention of the operations structure pattern.

### 8.1 The Three-Step Dereference

```
+============================================================================+
|                    ANATOMY OF s->ops->oper_xxx(s)                           |
+============================================================================+
|                                                                             |
|   The expression:  s->ops->read(s, buf, len)                               |
|                                                                             |
|   Breaks down into THREE steps:                                             |
|                                                                             |
|   STEP 1: s->ops                                                            |
|   ================                                                          |
|   Follow the 'ops' pointer inside struct 's' to get the ops table          |
|                                                                             |
|   STEP 2: s->ops->read                                                      |
|   ====================                                                      |
|   Follow the 'read' function pointer inside the ops table                  |
|                                                                             |
|   STEP 3: s->ops->read(s, buf, len)                                        |
|   ==================================                                        |
|   Call the function, passing 's' as the first argument                     |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 表达式 `s->ops->read(s, buf, len)` 分解为三个步骤
- 步骤 1：访问对象 s 中的 ops 指针
- 步骤 2：访问 ops 表中的 read 函数指针
- 步骤 3：调用该函数，将 s 作为第一个参数传递

### 8.2 Memory Layout Visualization

```
+============================================================================+
|                         MEMORY LAYOUT                                       |
+============================================================================+
|                                                                             |
|   Suppose we have:                                                          |
|                                                                             |
|       struct sensor *s = ...;                                               |
|       s->ops->read(s, &value);                                              |
|                                                                             |
|                                                                             |
|   MEMORY:                                                                   |
|   =======                                                                   |
|                                                                             |
|   Address 0x1000: struct sensor (object 's')                                |
|   +--------------------------------------------------+                      |
|   | name:    "TempSensor0"                           | offset 0             |
|   | id:      1                                       | offset 32            |
|   | ops:     0x3000  ----------------------+         | offset 36  <--+      |
|   | bus:     0x2000                        |         | offset 44     |      |
|   | flags:   0x01                          |         | offset 52     |      |
|   | private_data: 0x4000                   |         | offset 56     |      |
|   +--------------------------------------------------+               |      |
|                                            |                         |      |
|                                            |  s->ops (Step 1)        |      |
|                                            v                         |      |
|   Address 0x3000: struct sensor_ops (ops table)                      |      |
|   +--------------------------------------------------+               |      |
|   | init:       0x5100                               | offset 0      |      |
|   | read:       0x5200  ---------------+             | offset 8      |      |
|   | set_config: 0x5300                 |             | offset 16     |      |
|   | get_config: NULL (0x0000)          |             | offset 24     |      |
|   | suspend:    NULL (0x0000)          |             | offset 32     |      |
|   | resume:     NULL (0x0000)          |             | offset 40     |      |
|   | destroy:    0x5400                 |             | offset 48     |      |
|   +--------------------------------------------------+               |      |
|                                        |                             |      |
|                                        |  s->ops->read (Step 2)      |      |
|                                        v                             |      |
|   Address 0x5200: Function temp_sensor_read                          |      |
|   +--------------------------------------------------+               |      |
|   | int temp_sensor_read(struct sensor *s,           |               |      |
|   |                      int32_t *value)             |               |      |
|   | {                                                |               |      |
|   |     /* 's' points to 0x1000 */  <----------------|--------------+       |
|   |     struct temp_priv *priv = s->private_data;    |                      |
|   |     *value = priv->current_temp;                 |                      |
|   |     return 0;                                    |                      |
|   | }                                                |                      |
|   +--------------------------------------------------+                      |
|                                                                             |
|   Step 3: Call function with 's' (0x1000) as first argument                 |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 对象 `s` 位于内存地址 0x1000，包含指向 ops 表的指针（0x3000）
- ops 表位于 0x3000，包含多个函数指针
- `read` 函数指针指向实际函数 `temp_sensor_read`（0x5200）
- 调用时，将 `s`（0x1000）作为第一个参数传递给函数

### 8.3 Step-by-Step Execution

```c
/*
 * Let's trace through: s->ops->read(s, &value)
 * 
 * Assume:
 *   - s is at address 0x1000
 *   - s->ops is 0x3000
 *   - s->ops->read is 0x5200 (temp_sensor_read function)
 */

/* STEP 1: Access s->ops */
/* 
 * The compiler generates:
 *   Load the pointer at (s + offsetof(struct sensor, ops))
 *   = Load pointer at (0x1000 + 36) 
 *   = Load pointer at 0x1024
 *   = 0x3000  (address of ops table)
 */

/* STEP 2: Access s->ops->read */
/*
 * The compiler generates:
 *   Load the function pointer at (ops + offsetof(struct sensor_ops, read))
 *   = Load pointer at (0x3000 + 8)
 *   = Load pointer at 0x3008
 *   = 0x5200  (address of temp_sensor_read function)
 */

/* STEP 3: Call the function */
/*
 * The compiler generates a function call:
 *   - Push arguments onto stack (or load into registers):
 *       arg1 (s)     = 0x1000
 *       arg2 (&value) = address of value variable
 *   - CALL 0x5200
 *   
 * Inside temp_sensor_read:
 *   - First parameter 's' has value 0x1000
 *   - Function can access s->name, s->private_data, etc.
 */
```

### 8.4 Why Pass `s` Explicitly?

```
+============================================================================+
|                    WHY PASS 's' TO THE FUNCTION?                            |
+============================================================================+
|                                                                             |
|   In C++:                              In C (ops pattern):                  |
|   ========                             ====================                 |
|                                                                             |
|   obj->read(buf, len);                 s->ops->read(s, buf, len);          |
|                                                   ^                         |
|                                                   |                         |
|   'this' is implicit,                  's' must be passed explicitly!      |
|   compiler handles it                                                       |
|                                                                             |
|   REASON:                                                                   |
|   =======                                                                   |
|   In C, function pointers don't "remember" which object they belong to.    |
|   The ops table is SHARED by all objects of the same type.                 |
|                                                                             |
|   Example:                                                                  |
|   +----------------+          +------------------------+                    |
|   | sensor_a       |          |                        |                    |
|   | ops: --------->|--------->| temp_sensor_ops (shared)|                   |
|   +----------------+          |   .read = func_ptr     |                    |
|                               +------------------------+                    |
|   +----------------+                    ^                                   |
|   | sensor_b       |                    |                                   |
|   | ops: --------->|--------------------+                                   |
|   +----------------+                                                        |
|                                                                             |
|   Both sensor_a and sensor_b share the SAME ops table!                     |
|   When read() is called, it needs to know WHICH sensor is being read.     |
|   That's why we pass 's' explicitly.                                       |
|                                                                             |
+============================================================================+
```

**中文说明**：
- C++ 中 `this` 指针是隐式的，编译器自动处理
- C 中函数指针不"记住"它属于哪个对象
- ops 表被同类型的所有对象共享
- 因此必须显式传递 `s`，让函数知道操作的是哪个对象

### 8.5 Assembly Code Example

```
+============================================================================+
|              WHAT THE COMPILER GENERATES (x86-64)                           |
+============================================================================+
|                                                                             |
|   C Code:                                                                   |
|       int ret = s->ops->read(s, &value);                                   |
|                                                                             |
|   Generated Assembly (simplified, x86-64 calling convention):              |
|   =========================================================                |
|                                                                             |
|       ; Assume s is in register %rdi                                       |
|                                                                             |
|       ; STEP 1: Load s->ops into %rax                                      |
|       mov    0x24(%rdi), %rax     ; offset 36 = 0x24 is ops field         |
|                                                                             |
|       ; STEP 2: Load s->ops->read into %r11                                |
|       mov    0x8(%rax), %r11      ; offset 8 is read function ptr         |
|                                                                             |
|       ; STEP 3: Setup arguments and call                                   |
|       ; arg1 (s) is already in %rdi                                        |
|       lea    -0x10(%rbp), %rsi    ; arg2: &value                          |
|       call   *%r11                ; indirect call through function ptr     |
|                                                                             |
|       ; Return value is in %eax                                            |
|       mov    %eax, ret            ; store return value                     |
|                                                                             |
|   Note: The 'call *%r11' is an INDIRECT call - the target is not known    |
|   at compile time. This is the "virtual dispatch" mechanism.               |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 编译器生成的汇编代码展示了三个步骤
- `mov 0x24(%rdi), %rax`：加载 ops 指针
- `mov 0x8(%rax), %r11`：加载 read 函数指针
- `call *%r11`：间接调用（目标地址在运行时确定）
- 这就是"虚拟派发"机制的实现

### 8.6 Complete Trace Example

```c
/*
 * COMPLETE TRACE EXAMPLE
 * ======================
 * 
 * Given these definitions:
 */

struct sensor {
    char name[32];                    /* offset 0 */
    unsigned int id;                  /* offset 32 */
    const struct sensor_ops *ops;     /* offset 36 (or 40 on 64-bit) */
    void *private_data;               /* offset 44 (or 48 on 64-bit) */
};

struct sensor_ops {
    int (*init)(struct sensor *s);              /* offset 0 */
    int (*read)(struct sensor *s, int *value);  /* offset 8 */
    void (*destroy)(struct sensor *s);          /* offset 16 */
};

/* Implementation */
static int temp_read(struct sensor *s, int *value) {
    printf("Reading sensor: %s\n", s->name);  /* Access via s */
    *value = 2500;
    return 0;
}

static const struct sensor_ops temp_ops = {
    .init = temp_init,
    .read = temp_read,      /* <-- This is what gets called */
    .destroy = temp_destroy,
};

/* Object instance */
struct sensor my_sensor = {
    .name = "TempSensor0",
    .id = 1,
    .ops = &temp_ops,       /* <-- Points to ops table */
    .private_data = NULL,
};

/*
 * Now trace: my_sensor.ops->read(&my_sensor, &value)
 * 
 * STEP 1: my_sensor.ops
 *         = &temp_ops
 *         = 0x3000 (for example)
 * 
 * STEP 2: my_sensor.ops->read
 *         = temp_ops.read
 *         = &temp_read
 *         = 0x5200 (for example)
 * 
 * STEP 3: my_sensor.ops->read(&my_sensor, &value)
 *         = temp_read(&my_sensor, &value)
 *         = calls function at 0x5200 with:
 *             s     = &my_sensor (0x1000)
 *             value = &value
 * 
 * Inside temp_read:
 *         s->name is accessible because s = &my_sensor
 *         printf prints "Reading sensor: TempSensor0"
 *         *value = 2500
 *         return 0
 * 
 * Result: value = 2500, function returns 0
 */

int main() {
    int value;
    struct sensor *s = &my_sensor;
    
    /* The magic call */
    int ret = s->ops->read(s, &value);
    /*        ^^^^^^^^^^^
     *        |
     *        Evaluates to: temp_read(&my_sensor, &value)
     */
    
    printf("ret=%d, value=%d\n", ret, value);
    /* Output: ret=0, value=2500 */
}
```

### 8.7 The "this" Pointer Pattern

```
+============================================================================+
|                   THE "THIS" POINTER IN C                                   |
+============================================================================+
|                                                                             |
|   In object-oriented terms, s->ops->read(s, ...) can be understood as:      |
|                                                                             |
|   +------------------------------------------------------------------+      |
|   |                                                                   |     |
|   |   s->ops->read(s, buf, len)                                       |     |
|   |   ^     ^    ^  ^                                                 |     |
|   |   |     |    |  |                                                 |     |
|   |   |     |    |  +-- "this" pointer (the object itself)            |     |
|   |   |     |    |                                                    |     |
|   |   |     |    +-- method name                                      |     |
|   |   |     |                                                         |     |
|   |   |     +-- vtable (virtual function table)                       |     |
|   |   |                                                               |     |
|   |   +-- object instance                                             |     |
|   |                                                                   |     |
|   +------------------------------------------------------------------+      |
|                                                                             |
|   Equivalent C++ would be:                                                  |
|                                                                             |
|       s->read(buf, len);    // 'this' is implicit                           |
|                                                                             |
|   But in C, we must write:                                                  |
|                                                                             |
|       s->ops->read(s, buf, len);    // 's' passed explicitly                |
|                                                                             |
|   The function signature always has the object as first parameter:          |
|                                                                             |
|       int (*read)(struct sensor *s, void *buf, size_t len);                 |
|                   ^^^^^^^^^^^^^^^^                                          |
|                   This is "this"                                            |
|                                                                             |
+============================================================================+
```

**中文说明**：
- `s` 就是 C 语言版本的 `this` 指针
- C++ 中 `this` 是隐式的，C 中必须显式传递
- 函数签名的第一个参数总是对象指针（相当于 `this`）
- 这使得函数能够访问调用它的对象的所有数据

### 8.8 Common Patterns in Kernel Code

```c
/* Pattern 1: Direct call with NULL check */
if (s->ops->read)
    ret = s->ops->read(s, buf, len);
else
    ret = -ENOSYS;

/* Pattern 2: Wrapper function (preferred) */
static inline int sensor_read(struct sensor *s, void *buf, size_t len)
{
    if (!s || !s->ops || !s->ops->read)
        return -EINVAL;
    return s->ops->read(s, buf, len);
}

/* Pattern 3: Kernel's real examples */

/* File operations (fs/read_write.c) */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    if (file->f_op->read)
        return file->f_op->read(file, buf, count, pos);
    //                         ^^^^
    //                         'file' is passed to its own read method
}

/* Network operations (net/core/dev.c) */
static int __dev_open(struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    
    if (ops->ndo_open)
        ret = ops->ndo_open(dev);
    //                      ^^^
    //                      'dev' is passed to its own open method
}
```

---

## References

1. Linux Kernel Source - `include/linux/fs.h` (file_operations)
2. Linux Kernel Source - `include/linux/netdevice.h` (net_device_ops)
3. Linux Kernel Source - `include/linux/blkdev.h` (block_device_operations)
4. "Linux Device Drivers, 3rd Edition" - O'Reilly
5. "Understanding the Linux Kernel, 3rd Edition" - O'Reilly

