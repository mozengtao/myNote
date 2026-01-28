# Case 3: kobject Embedding

## Subsystem Background

```
+=============================================================================+
|                    KOBJECT AND SYSFS                                         |
+=============================================================================+

    KOBJECT: The Kernel Object Base Class
    ======================================

    Every object visible in /sys is backed by a kobject.
    kobject provides:
    - Reference counting
    - Sysfs representation
    - Hierarchy (parent/child relationships)
    - Hotplug/uevent notification

    EMBEDDING KOBJECT:

    struct device {
        struct kobject kobj;     /* EMBEDDED */
        const char *init_name;
        struct device_type *type;
        struct device *parent;
        /* ... */
    };

    struct net_device {
        struct device dev;       /* Which contains kobject */
        /* ... */
    };

    HIERARCHY IN SYSFS:

    /sys/devices/
        pci0000:00/           <- kobject for PCI bus
            0000:00:1f.0/     <- kobject for PCI device
                net/          <- kobject for subsystem
                    eth0/     <- kobject for net_device
```

**中文说明：**

kobject是内核对象的基类。每个在/sys中可见的对象都有一个kobject。kobject提供：引用计数、sysfs表示、层次结构、热插拔通知。kobject被嵌入到更具体的结构体中（如device），然后device又可能被嵌入到更具体的结构体中（如net_device）。

---

## container_of in Device Model

```
    FROM KOBJECT TO DEVICE:
    =======================

    struct device {
        struct kobject kobj;     /* offset 0 (usually) */
        /* ... more fields ... */
    };

    /* Kernel provides this macro */
    #define to_dev(kobj) container_of(kobj, struct device, kobj)

    /* Usage in sysfs attribute show function */
    ssize_t dev_attr_show(struct kobject *kobj, 
                          struct attribute *attr, 
                          char *buf)
    {
        struct device *dev = to_dev(kobj);  /* container_of */
        /* Now can access dev->driver, dev->type, etc. */
    }


    FROM DEVICE TO SPECIFIC TYPE:
    ============================

    struct net_device {
        struct device dev;       /* EMBEDDED */
        char name[IFNAMSIZ];
        /* ... */
    };

    #define to_net_dev(d) container_of(d, struct net_device, dev)

    /* Multiple levels of container_of */
    struct net_device *ndev = to_net_dev(to_dev(kobj));
```

**中文说明：**

在设备模型中使用container_of：从kobject到device使用`to_dev(kobj)`宏（内部是container_of），从device到具体设备类型（如net_device）再次使用container_of。可以链式调用container_of来从最通用的kobject恢复到最具体的类型。

---

## Minimal C Code Simulation

```c
/*
 * CONTAINER_OF WITH KOBJECT SIMULATION
 * Demonstrates the device model embedding pattern
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ==========================================================
 * KERNEL-STYLE DEFINITIONS
 * ========================================================== */

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* Reference count (simplified kref) */
struct kref {
    int refcount;
};

static inline void kref_init(struct kref *kref)
{
    kref->refcount = 1;
}

static inline void kref_get(struct kref *kref)
{
    kref->refcount++;
}

static inline int kref_put(struct kref *kref, void (*release)(struct kref *))
{
    if (--kref->refcount == 0) {
        release(kref);
        return 1;
    }
    return 0;
}

/* ==========================================================
 * KOBJECT (Simplified)
 * ========================================================== */

struct kobject {
    const char *name;
    struct kref kref;
    struct kobject *parent;
    void (*release)(struct kobject *kobj);
};

static inline void kobject_init(struct kobject *kobj, const char *name)
{
    kobj->name = name;
    kobj->parent = NULL;
    kobj->release = NULL;
    kref_init(&kobj->kref);
}

static inline struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj)
        kref_get(&kobj->kref);
    return kobj;
}

static void kobject_release(struct kref *kref)
{
    struct kobject *kobj = container_of(kref, struct kobject, kref);
    printf("[KOBJECT] Releasing kobject '%s'\n", kobj->name);
    if (kobj->release)
        kobj->release(kobj);
}

static inline void kobject_put(struct kobject *kobj)
{
    if (kobj)
        kref_put(&kobj->kref, kobject_release);
}

/* ==========================================================
 * DEVICE (Embeds kobject)
 * ========================================================== */

struct device {
    struct kobject kobj;          /* EMBEDDED */
    const char *init_name;
    struct device *parent;
    void *driver_data;
};

/* container_of macro for device */
#define to_dev(kobj) container_of(kobj, struct device, kobj)

static void device_release(struct kobject *kobj)
{
    struct device *dev = to_dev(kobj);
    printf("[DEVICE] Releasing device '%s'\n", dev->init_name);
    free(dev);
}

static inline void device_init(struct device *dev, const char *name)
{
    kobject_init(&dev->kobj, name);
    dev->kobj.release = device_release;
    dev->init_name = name;
    dev->parent = NULL;
    dev->driver_data = NULL;
}

static inline struct device *device_get(struct device *dev)
{
    if (dev)
        kobject_get(&dev->kobj);
    return dev;
}

static inline void device_put(struct device *dev)
{
    if (dev)
        kobject_put(&dev->kobj);
}

/* ==========================================================
 * NET_DEVICE (Embeds device which embeds kobject)
 * ========================================================== */

struct net_device {
    struct device dev;            /* EMBEDDED (which embeds kobject) */
    char name[16];
    int flags;
    unsigned char dev_addr[6];
    /* More network-specific fields */
};

/* container_of macro for net_device */
#define to_net_dev(d) container_of(d, struct net_device, dev)

/* Get net_device from kobject (two levels of container_of) */
static inline struct net_device *netdev_from_kobject(struct kobject *kobj)
{
    struct device *dev = to_dev(kobj);
    return to_net_dev(dev);
}

static void netdev_release(struct kobject *kobj)
{
    struct net_device *ndev = netdev_from_kobject(kobj);
    printf("[NET_DEVICE] Releasing net_device '%s'\n", ndev->name);
    free(ndev);
}

struct net_device *alloc_netdev(const char *name)
{
    struct net_device *ndev = malloc(sizeof(*ndev));
    if (!ndev) return NULL;
    
    device_init(&ndev->dev, name);
    ndev->dev.kobj.release = netdev_release;
    strncpy(ndev->name, name, sizeof(ndev->name) - 1);
    ndev->flags = 0;
    memset(ndev->dev_addr, 0, sizeof(ndev->dev_addr));
    
    return ndev;
}

/* ==========================================================
 * SYSFS ATTRIBUTE SIMULATION
 * ========================================================== */

struct attribute {
    const char *name;
    int mode;
};

/* Sysfs show function receives kobject, must recover specific type */
ssize_t netdev_show_name(struct kobject *kobj, 
                         struct attribute *attr,
                         char *buf)
{
    /* Use container_of to get from kobject to net_device */
    struct device *dev = to_dev(kobj);
    struct net_device *ndev = to_net_dev(dev);
    
    printf("[SYSFS] show called: kobj='%s'\n", kobj->name);
    printf("  kobject at:    %p\n", (void *)kobj);
    printf("  -> device at:  %p (via container_of)\n", (void *)dev);
    printf("  -> netdev at:  %p (via container_of)\n", (void *)ndev);
    printf("  netdev->name:  '%s'\n", ndev->name);
    
    return sprintf(buf, "%s\n", ndev->name);
}

ssize_t netdev_show_flags(struct kobject *kobj,
                          struct attribute *attr,
                          char *buf)
{
    struct net_device *ndev = netdev_from_kobject(kobj);
    return sprintf(buf, "0x%x\n", ndev->flags);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("CONTAINER_OF WITH KOBJECT DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    /* Create a net_device (three levels of embedding) */
    printf("--- Creating net_device ---\n");
    struct net_device *eth0 = alloc_netdev("eth0");
    if (!eth0) return 1;
    
    eth0->flags = 0x1043;  /* IFF_UP | IFF_BROADCAST | ... */
    eth0->dev_addr[0] = 0x00;
    eth0->dev_addr[1] = 0x11;
    eth0->dev_addr[2] = 0x22;
    
    /* Show the embedding structure */
    printf("\n--- Memory Layout ---\n");
    printf("net_device at:           %p\n", (void *)eth0);
    printf("  .dev (device) at:      %p\n", (void *)&eth0->dev);
    printf("  .dev.kobj (kobject) at: %p\n", (void *)&eth0->dev.kobj);
    printf("  .name at:              %p\n", (void *)eth0->name);
    
    printf("\n--- Offsets ---\n");
    printf("offsetof(net_device, dev) = %zu\n", 
           offsetof(struct net_device, dev));
    printf("offsetof(device, kobj) = %zu\n", 
           offsetof(struct device, kobj));
    printf("offsetof(net_device, name) = %zu\n", 
           offsetof(struct net_device, name));
    
    /* Simulate sysfs attribute access */
    printf("\n--- Simulating sysfs attribute access ---\n");
    char buf[64];
    struct attribute attr = { .name = "name", .mode = 0444 };
    
    /* sysfs would call this with just the kobject */
    struct kobject *kobj = &eth0->dev.kobj;
    printf("sysfs calls show() with kobject at %p\n", (void *)kobj);
    netdev_show_name(kobj, &attr, buf);
    printf("Result: %s", buf);
    
    /* Demonstrate container_of chain */
    printf("\n--- container_of chain demonstration ---\n");
    printf("Starting with kobject at %p\n", (void *)kobj);
    
    printf("\nStep 1: to_dev(kobj)\n");
    struct device *dev = to_dev(kobj);
    printf("  %p - offsetof(device, kobj)[%zu] = %p\n",
           (void *)kobj, 
           offsetof(struct device, kobj),
           (void *)dev);
    
    printf("\nStep 2: to_net_dev(dev)\n");
    struct net_device *ndev = to_net_dev(dev);
    printf("  %p - offsetof(net_device, dev)[%zu] = %p\n",
           (void *)dev,
           offsetof(struct net_device, dev),
           (void *)ndev);
    
    printf("\nFinal: netdev '%s' at %p\n", ndev->name, (void *)ndev);
    
    /* Verify */
    printf("\nVerification: eth0 == ndev? %s\n", 
           eth0 == ndev ? "YES" : "NO");
    
    /* Reference counting */
    printf("\n--- Reference counting ---\n");
    printf("Initial refcount: %d\n", eth0->dev.kobj.kref.refcount);
    
    device_get(&eth0->dev);
    printf("After device_get: %d\n", eth0->dev.kobj.kref.refcount);
    
    device_put(&eth0->dev);
    printf("After device_put: %d\n", eth0->dev.kobj.kref.refcount);
    
    /* Release */
    printf("\n--- Final release ---\n");
    device_put(&eth0->dev);  /* refcount -> 0, triggers release */
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- kobject is embedded in device\n");
    printf("- device is embedded in net_device\n");
    printf("- container_of chains allow recovery at any level\n");
    printf("- sysfs uses kobject, container_of recovers specific type\n");
    printf("- This is how Linux implements 'inheritance' in C\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## What container_of Enables Here

```
+=============================================================================+
|              container_of IN DEVICE MODEL                                    |
+=============================================================================+

    1. POLYMORPHISM
       =============
       
       sysfs only knows about kobject.
       Container_of recovers the actual device type.
       
       sysfs/   <- works with kobject*
         |
         v
       kobject -> container_of -> device -> container_of -> net_device
                                                            block_device
                                                            char_device
                                                            ...

    2. COMMON INFRASTRUCTURE
       =====================
       
       kobject provides:
       - Reference counting (kref embedded in kobject)
       - Sysfs representation
       - Parent/child relationships
       
       ALL device types get these for free by embedding kobject.

    3. LAYERED EMBEDDING
       ==================
       
       net_device embeds device embeds kobject
       
       From any layer, container_of can recover outer layer:
       - From kref to kobject (kref is IN kobject)
       - From kobject to device (kobject is IN device)
       - From device to net_device (device is IN net_device)
```

---

## Real Kernel Examples

### Device to kobject (include/linux/device.h)

```c
static inline struct kobject *get_device_kobject(struct device *dev)
{
    return &dev->kobj;
}

/* In sysfs callbacks */
#define to_dev(kobj) container_of(kobj, struct device, kobj)
```

### Net device (include/linux/netdevice.h)

```c
#define to_net_dev(d) container_of(d, struct net_device, dev)

/* Get net_device from kobject */
static inline struct net_device *to_net_device_from_kobject(struct kobject *kobj)
{
    return to_net_dev(to_dev(kobj));
}
```

---

## Key Takeaways

1. **kobject is the kernel's base class**: Provides refcount, sysfs, hierarchy
2. **Embedding creates type hierarchy**: device embeds kobject, specific devices embed device
3. **container_of chains**: Can traverse multiple levels of embedding
4. **sysfs uses kobject interface**: container_of recovers actual type in callbacks
5. **This IS C-style inheritance**: Without language support, using macros
