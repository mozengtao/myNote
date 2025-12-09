# Bridge Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                       BRIDGE PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|         Abstraction                    Implementation             |
|    +------------------+           +------------------+            |
|    |  Abstraction     |           | Implementor     |            |
|    +------------------+   uses    +------------------+            |
|    | - impl*          |---------->| + impl_op()     |            |
|    | + operation()    |           +--------+--------+            |
|    +--------+---------+                    ^                     |
|             ^                              |                     |
|             |                    +---------+---------+           |
|    +--------+---------+          |                   |           |
|    |                  |          v                   v           |
|    v                  v     +----------+       +----------+      |
| +--------+      +--------+  | Concrete |       | Concrete |      |
| |Refined |      |Refined |  | Impl A   |       | Impl B   |      |
| |Abstr.A |      |Abstr.B |  +----------+       +----------+      |
| +--------+      +--------+                                       |
|                                                                   |
|    Abstraction and Implementation vary independently              |
|    Connected by composition, not inheritance                      |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 桥接模式分离抽象与实现，通过组合替代继承，支持两者独立扩展。在Linux内核中，驱动模型就是桥接模式的典型应用：设备（抽象）与驱动（实现）分离，通过总线作为桥接连接。这样新设备和新驱动可以独立开发，只要符合接口规范就能匹配工作。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Device-Driver Bridge

```c
/* From: include/linux/device.h */

/**
 * struct device - Device abstraction
 *
 * Represents the "abstraction" side of the bridge.
 * Devices are hardware entities independent of drivers.
 */
struct device {
    struct device       *parent;
    struct kobject kobj;
    
    /* BRIDGE: Reference to implementation (driver) */
    struct device_driver *driver;   /* Which driver is bound */
    
    /* BRIDGE: Connection point (bus) */
    struct bus_type     *bus;       /* Type of bus */
    
    void    *platform_data;         /* Platform specific data */
    void    *driver_data;           /* Driver specific data */
    /* ... */
};

/**
 * struct device_driver - Driver implementation
 *
 * Represents the "implementor" side of the bridge.
 * Drivers are software implementations independent of specific devices.
 */
struct device_driver {
    const char      *name;
    
    /* BRIDGE: Connection point (bus) */
    struct bus_type *bus;
    
    /* Operations - different for each driver */
    int (*probe)(struct device *dev);   /* Bind driver to device */
    int (*remove)(struct device *dev);  /* Unbind driver */
    void (*shutdown)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    
    const struct of_device_id *of_match_table;
    /* ... */
};

/**
 * struct bus_type - The BRIDGE connecting devices and drivers
 *
 * Bus type defines how devices and drivers are matched.
 */
struct bus_type {
    const char      *name;
    
    /* Match function - determines if driver can handle device */
    int (*match)(struct device *dev, struct device_driver *drv);
    
    /* Events during binding */
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    
    /* Power management bridge */
    const struct dev_pm_ops *pm;
    /* ... */
};

/* The bridge in action - matching devices with drivers */
static int driver_match_device(struct device_driver *drv, struct device *dev)
{
    /* Bus determines matching criteria */
    return drv->bus->match ? drv->bus->match(dev, drv) : 1;
}
```

### 2.2 Kernel Example: Platform Device Bridge

```c
/* From: drivers/base/platform.c */

/**
 * Platform bus - Bridges platform devices and platform drivers
 */
struct bus_type platform_bus_type = {
    .name       = "platform",
    .dev_groups = platform_dev_groups,
    .match      = platform_match,    /* Bridge matching logic */
    .uevent     = platform_uevent,
    .pm         = &platform_dev_pm_ops,
};

/**
 * platform_match - Bridge matching function
 * @dev: Platform device (abstraction)
 * @drv: Platform driver (implementation)
 *
 * The bridge decides if this driver can handle this device.
 */
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    /* Match by device tree */
    if (of_driver_match_device(dev, drv))
        return 1;

    /* Match by ACPI */
    if (acpi_driver_match_device(dev, drv))
        return 1;

    /* Match by ID table */
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    /* Match by name */
    return (strcmp(pdev->name, drv->name) == 0);
}
```

### 2.3 Kernel Example: Block Device Bridge

```c
/* From: include/linux/blkdev.h */

/**
 * Block device abstraction bridged to specific implementations
 */
struct gendisk {
    int major;                  /* Device major number */
    int first_minor;
    int minors;
    
    char disk_name[DISK_NAME_LEN];
    
    /* BRIDGE: Operations provided by implementation */
    const struct block_device_operations *fops;
    
    struct request_queue *queue;
    void *private_data;
    /* ... */
};

/**
 * block_device_operations - Implementation interface
 *
 * Each block device type provides its own implementation.
 */
struct block_device_operations {
    int (*open)(struct block_device *, fmode_t);
    void (*release)(struct gendisk *, fmode_t);
    int (*ioctl)(struct block_device *, fmode_t, unsigned, unsigned long);
    int (*media_changed)(struct gendisk *);
    int (*revalidate_disk)(struct gendisk *);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
    struct module *owner;
};

/* Different implementations for different device types */
/* SCSI disk operations */
static const struct block_device_operations sd_fops = {
    .owner          = THIS_MODULE,
    .open           = sd_open,
    .release        = sd_release,
    .ioctl          = sd_ioctl,
    /* ... */
};

/* RAM disk operations */
static const struct block_device_operations brd_fops = {
    .owner          = THIS_MODULE,
    .open           = brd_open,
    .ioctl          = brd_ioctl,
    /* ... */
};
```

### 2.4 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL BRIDGE PATTERN                         |
|                  (Device-Driver Model)                            |
+------------------------------------------------------------------+
|                                                                   |
|    Abstraction Side              Bridge              Impl. Side   |
|    (Devices)                     (Bus)               (Drivers)    |
|                                                                   |
|    +-------------+                                +-------------+ |
|    | USB Device  |                                | USB Driver  | |
|    | (mouse)     |----+                    +------| (usbhid)    | |
|    +-------------+    |                    |      +-------------+ |
|                       |                    |                      |
|    +-------------+    |    +-----------+   |      +-------------+ |
|    | USB Device  |----+--->| USB Bus   |<--+------| USB Driver  | |
|    | (keyboard)  |    |    |  .match() |   |      | (usb-kbd)   | |
|    +-------------+    |    +-----------+   |      +-------------+ |
|                       |         ^          |                      |
|    +-------------+    |         |          |      +-------------+ |
|    | USB Device  |----+         |          +------| USB Driver  | |
|    | (storage)   |              |                 | (usb-stor)  | |
|    +-------------+              |                 +-------------+ |
|                                 |                                 |
|                         +-------+-------+                         |
|                         | Match Logic:  |                         |
|                         | - Vendor ID   |                         |
|                         | - Product ID  |                         |
|                         | - Class       |                         |
|                         +---------------+                         |
|                                                                   |
|    New devices and new drivers can be added independently         |
|    as long as they follow the bus interface                       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux的设备驱动模型是桥接模式的典型应用。设备（抽象端）和驱动（实现端）通过总线（桥）连接。新设备可以独立于驱动添加，新驱动也可以独立于设备添加。总线定义匹配逻辑，决定哪个驱动适合哪个设备。这种分离使得设备和驱动可以独立演进。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Independent Evolution** | Abstraction and implementation can change independently |
| **Reduced Coupling** | Abstraction doesn't depend on specific implementation |
| **Runtime Binding** | Implementation can be selected/changed at runtime |
| **Single Responsibility** | Clear separation of concerns |
| **Scalability** | Add new abstractions without new implementations and vice versa |
| **Testing** | Each side can be tested independently |

**中文说明：** 桥接模式的优势包括：独立演进（抽象和实现可以独立变化）、降低耦合（抽象不依赖具体实现）、运行时绑定（可以在运行时选择/更换实现）、单一职责（关注点清晰分离）、可扩展性（添加新抽象无需新实现，反之亦然）、可测试性（两端可以独立测试）。

---

## 4. User-Space Implementation Example

```c
/*
 * Bridge Pattern - User Space Implementation
 * Mimics Linux Kernel's Device-Driver model
 * 
 * Compile: gcc -o bridge bridge.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Implementor Interface - Driver Operations
 * Similar to device_driver in kernel
 * ============================================================ */

/* Forward declarations */
struct device;
struct driver;

/* Driver operations - implementation interface */
struct driver_ops {
    int (*probe)(struct driver *drv, struct device *dev);
    void (*remove)(struct driver *drv, struct device *dev);
    int (*read)(struct driver *drv, struct device *dev, char *buf, int len);
    int (*write)(struct driver *drv, struct device *dev, const char *buf, int len);
};

/* Driver structure - Implementor */
struct driver {
    const char *name;
    const char *bus_type;           /* Which bus this driver works with */
    const struct driver_ops *ops;
    void *private_data;
    
    /* Supported device IDs */
    const char **supported_ids;
    int num_supported;
};

/* ============================================================
 * Abstraction - Device
 * Similar to struct device in kernel
 * ============================================================ */

/* Device structure - Abstraction */
struct device {
    const char *name;
    const char *device_id;          /* Unique identifier */
    const char *bus_type;           /* Which bus this device is on */
    
    /* BRIDGE: Reference to bound driver */
    struct driver *driver;
    
    void *platform_data;
    void *driver_data;
};

/* ============================================================
 * Bridge - Bus Type
 * Similar to bus_type in kernel
 * ============================================================ */

struct bus;

/* Bus operations */
struct bus_ops {
    int (*match)(struct bus *bus, struct device *dev, struct driver *drv);
    int (*probe)(struct bus *bus, struct device *dev);
    void (*remove)(struct bus *bus, struct device *dev);
};

/* Bus structure - The Bridge */
struct bus {
    const char *name;
    const struct bus_ops *ops;
    
    /* Registered devices and drivers */
    struct device **devices;
    int device_count;
    int device_capacity;
    
    struct driver **drivers;
    int driver_count;
    int driver_capacity;
};

/* ============================================================
 * Concrete Implementor 1: Network Driver
 * ============================================================ */

struct net_driver_data {
    int mtu;
    int speed_mbps;
};

static int net_probe(struct driver *drv, struct device *dev)
{
    struct net_driver_data *data = drv->private_data;
    printf("[NetDriver] Probing device '%s', MTU=%d, Speed=%dMbps\n",
           dev->name, data->mtu, data->speed_mbps);
    return 0;
}

static void net_remove(struct driver *drv, struct device *dev)
{
    printf("[NetDriver] Removing device '%s'\n", dev->name);
}

static int net_read(struct driver *drv, struct device *dev, char *buf, int len)
{
    snprintf(buf, len, "Network packet from %s", dev->name);
    printf("[NetDriver] Read %lu bytes from '%s'\n", strlen(buf), dev->name);
    return strlen(buf);
}

static int net_write(struct driver *drv, struct device *dev, const char *buf, int len)
{
    printf("[NetDriver] Sending %d bytes via '%s': %s\n", len, dev->name, buf);
    return len;
}

static const struct driver_ops net_driver_ops = {
    .probe = net_probe,
    .remove = net_remove,
    .read = net_read,
    .write = net_write
};

static const char *net_supported[] = {"eth-realtek", "eth-intel", "eth-broadcom"};

/* ============================================================
 * Concrete Implementor 2: Storage Driver
 * ============================================================ */

struct storage_driver_data {
    int sector_size;
    int cache_size;
};

static int storage_probe(struct driver *drv, struct device *dev)
{
    struct storage_driver_data *data = drv->private_data;
    printf("[StorageDriver] Probing device '%s', Sector=%d, Cache=%dKB\n",
           dev->name, data->sector_size, data->cache_size);
    return 0;
}

static void storage_remove(struct driver *drv, struct device *dev)
{
    printf("[StorageDriver] Removing device '%s'\n", dev->name);
}

static int storage_read(struct driver *drv, struct device *dev, char *buf, int len)
{
    snprintf(buf, len, "Block data from %s", dev->name);
    printf("[StorageDriver] Read %lu bytes from '%s'\n", strlen(buf), dev->name);
    return strlen(buf);
}

static int storage_write(struct driver *drv, struct device *dev, const char *buf, int len)
{
    printf("[StorageDriver] Writing %d bytes to '%s': %s\n", len, dev->name, buf);
    return len;
}

static const struct driver_ops storage_driver_ops = {
    .probe = storage_probe,
    .remove = storage_remove,
    .read = storage_read,
    .write = storage_write
};

static const char *storage_supported[] = {"scsi-disk", "nvme-drive", "sata-disk"};

/* ============================================================
 * Bus Implementation - The Bridge Logic
 * ============================================================ */

/* Match function - determines if driver can handle device */
static int pci_match(struct bus *bus, struct device *dev, struct driver *drv)
{
    /* Check if bus types match */
    if (strcmp(dev->bus_type, drv->bus_type) != 0) {
        return 0;
    }
    
    /* Check if driver supports this device ID */
    for (int i = 0; i < drv->num_supported; i++) {
        if (strcmp(dev->device_id, drv->supported_ids[i]) == 0) {
            printf("[Bus] Match found: device '%s' <-> driver '%s'\n",
                   dev->name, drv->name);
            return 1;
        }
    }
    
    return 0;
}

/* Probe - bind driver to device */
static int pci_probe(struct bus *bus, struct device *dev)
{
    if (dev->driver && dev->driver->ops->probe) {
        return dev->driver->ops->probe(dev->driver, dev);
    }
    return -1;
}

/* Remove - unbind driver from device */
static void pci_remove(struct bus *bus, struct device *dev)
{
    if (dev->driver && dev->driver->ops->remove) {
        dev->driver->ops->remove(dev->driver, dev);
    }
    dev->driver = NULL;
}

static const struct bus_ops pci_bus_ops = {
    .match = pci_match,
    .probe = pci_probe,
    .remove = pci_remove
};

/* ============================================================
 * Bus Management Functions
 * ============================================================ */

struct bus *create_bus(const char *name, const struct bus_ops *ops)
{
    struct bus *bus = malloc(sizeof(struct bus));
    if (!bus) return NULL;
    
    bus->name = name;
    bus->ops = ops;
    bus->device_count = 0;
    bus->device_capacity = 16;
    bus->devices = malloc(sizeof(struct device *) * bus->device_capacity);
    bus->driver_count = 0;
    bus->driver_capacity = 16;
    bus->drivers = malloc(sizeof(struct driver *) * bus->driver_capacity);
    
    printf("[Bus] Created bus '%s'\n", name);
    return bus;
}

/* Register a driver with the bus */
int bus_register_driver(struct bus *bus, struct driver *drv)
{
    if (bus->driver_count >= bus->driver_capacity) return -1;
    
    bus->drivers[bus->driver_count++] = drv;
    printf("[Bus] Registered driver '%s' on bus '%s'\n", drv->name, bus->name);
    
    /* Try to match with existing devices */
    for (int i = 0; i < bus->device_count; i++) {
        struct device *dev = bus->devices[i];
        if (dev->driver == NULL && bus->ops->match(bus, dev, drv)) {
            dev->driver = drv;
            bus->ops->probe(bus, dev);
        }
    }
    
    return 0;
}

/* Register a device with the bus */
int bus_register_device(struct bus *bus, struct device *dev)
{
    if (bus->device_count >= bus->device_capacity) return -1;
    
    bus->devices[bus->device_count++] = dev;
    printf("[Bus] Registered device '%s' on bus '%s'\n", dev->name, bus->name);
    
    /* Try to match with existing drivers */
    for (int i = 0; i < bus->driver_count; i++) {
        struct driver *drv = bus->drivers[i];
        if (bus->ops->match(bus, dev, drv)) {
            dev->driver = drv;
            bus->ops->probe(bus, dev);
            break;
        }
    }
    
    return 0;
}

/* ============================================================
 * Device Operations (use bridge to reach implementation)
 * ============================================================ */

int device_read(struct device *dev, char *buf, int len)
{
    if (dev->driver && dev->driver->ops->read) {
        return dev->driver->ops->read(dev->driver, dev, buf, len);
    }
    return -1;
}

int device_write(struct device *dev, const char *buf, int len)
{
    if (dev->driver && dev->driver->ops->write) {
        return dev->driver->ops->write(dev->driver, dev, buf, len);
    }
    return -1;
}

/* ============================================================
 * Main - Demonstrate Bridge Pattern
 * ============================================================ */

int main(void)
{
    struct bus *pci_bus;
    char buffer[256];

    printf("=== Bridge Pattern Demo (Device-Driver Model) ===\n\n");

    /* Create the PCI bus (the bridge) */
    printf("--- Creating Bus ---\n");
    pci_bus = create_bus("pci", &pci_bus_ops);

    /* Create drivers (implementations) */
    printf("\n--- Creating Drivers ---\n");
    
    struct net_driver_data net_data = { .mtu = 1500, .speed_mbps = 1000 };
    struct driver net_driver = {
        .name = "realtek-net",
        .bus_type = "pci",
        .ops = &net_driver_ops,
        .private_data = &net_data,
        .supported_ids = net_supported,
        .num_supported = 3
    };
    
    struct storage_driver_data storage_data = { .sector_size = 512, .cache_size = 8192 };
    struct driver storage_driver = {
        .name = "ahci-storage",
        .bus_type = "pci",
        .ops = &storage_driver_ops,
        .private_data = &storage_data,
        .supported_ids = storage_supported,
        .num_supported = 3
    };

    /* Create devices (abstractions) */
    printf("\n--- Creating Devices ---\n");
    
    struct device eth0 = {
        .name = "eth0",
        .device_id = "eth-realtek",
        .bus_type = "pci",
        .driver = NULL
    };
    
    struct device sda = {
        .name = "sda",
        .device_id = "sata-disk",
        .bus_type = "pci",
        .driver = NULL
    };
    
    struct device eth1 = {
        .name = "eth1",
        .device_id = "eth-intel",
        .bus_type = "pci",
        .driver = NULL
    };

    /* Register drivers first */
    printf("\n--- Registering Drivers ---\n");
    bus_register_driver(pci_bus, &net_driver);
    bus_register_driver(pci_bus, &storage_driver);

    /* Register devices - should auto-bind to matching drivers */
    printf("\n--- Registering Devices (auto-binding) ---\n");
    bus_register_device(pci_bus, &eth0);
    bus_register_device(pci_bus, &sda);
    bus_register_device(pci_bus, &eth1);

    /* Use devices through the bridge */
    printf("\n--- Using Devices (via bridge) ---\n");
    
    device_write(&eth0, "Hello Network!", 14);
    device_read(&eth0, buffer, sizeof(buffer));
    printf("Received: %s\n\n", buffer);
    
    device_write(&sda, "Block Data", 10);
    device_read(&sda, buffer, sizeof(buffer));
    printf("Received: %s\n", buffer);

    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    free(pci_bus->devices);
    free(pci_bus->drivers);
    free(pci_bus);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Bridge Binding Flow

```
+------------------------------------------------------------------+
|                     BRIDGE BINDING FLOW                           |
+------------------------------------------------------------------+
|                                                                   |
|    1. Register Driver                                             |
|    +-------------------+                                          |
|    | bus_register_     |                                          |
|    | driver(bus, drv)  |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|             v                                                     |
|    +--------+----------+                                          |
|    | Add to bus->      |                                          |
|    | drivers[]         |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|             v                                                     |
|    +--------+----------+                                          |
|    | For each device:  |                                          |
|    | try match()       |                                          |
|    +-------------------+                                          |
|                                                                   |
|    2. Register Device                                             |
|    +-------------------+                                          |
|    | bus_register_     |                                          |
|    | device(bus, dev)  |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|             v                                                     |
|    +--------+----------+     +-------------------+                |
|    | For each driver:  |---->| bus->ops->match() |                |
|    | try match()       |     | Compare IDs       |                |
|    +--------+----------+     +--------+----------+                |
|             |                         |                           |
|             | (match found)           | returns 1                 |
|             v                         v                           |
|    +--------+----------+     +--------+----------+                |
|    | dev->driver = drv |     | bus->ops->probe() |                |
|    +-------------------+     | drv->ops->probe() |                |
|                              +-------------------+                |
|                                                                   |
|    3. Device Operation (through bridge)                           |
|    +-------------------+                                          |
|    | device_read(dev)  |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|             v                                                     |
|    +--------+----------+                                          |
|    | dev->driver->ops  |                                          |
|    | ->read(drv, dev)  |                                          |
|    +-------------------+                                          |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 桥接绑定流程：1）注册驱动时，驱动被添加到总线的驱动列表，并尝试与已有设备匹配；2）注册设备时，遍历所有驱动尝试匹配，匹配成功则绑定驱动并调用probe；3）设备操作时，通过dev->driver访问驱动的操作函数。这种设计使设备和驱动可以独立开发和注册。

---

## 6. Key Implementation Points

1. **Composition over Inheritance**: Device contains driver pointer, not extends it
2. **Match Function**: Bridge determines if abstraction and implementation fit
3. **Late Binding**: Driver bound to device at runtime, not compile time
4. **Common Interface**: Both sides implement interfaces defined by the bridge
5. **Registration Mechanism**: Devices and drivers register with the bus
6. **Auto-matching**: New registrations automatically try to match

**中文说明：** 实现桥接模式的关键点：使用组合而非继承（设备包含驱动指针）、匹配函数（桥判断抽象和实现是否匹配）、延迟绑定（运行时而非编译时绑定驱动）、共同接口（两端实现桥定义的接口）、注册机制（设备和驱动向总线注册）、自动匹配（新注册时自动尝试匹配）。

