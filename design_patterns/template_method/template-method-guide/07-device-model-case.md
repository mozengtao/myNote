# Case 5: Device Model Lifecycle (Probe Path)

## Subsystem Context

```
+=============================================================================+
|                    DEVICE MODEL ARCHITECTURE                                 |
+=============================================================================+

                           DEVICE DISCOVERY
    +----------------------------------------------------------+
    |   Bus enumeration (PCI scan, platform devices, etc.)     |
    +----------------------------------------------------------+
                              |
                              v
    +----------------------------------------------------------+
    |                    DEVICE CORE                            |
    |  +--------------------------------------------------+    |
    |  |  device_add()  <-- TEMPLATE METHOD               |    |
    |  |                                                  |    |
    |  |  1. Initialize device structure                  |    |
    |  |  2. kobject_add() - add to sysfs                 |    |
    |  |  3. device_create_file() - create attrs         |    |
    |  |  4. bus_add_device() - add to bus               |    |
    |  |  5. bus_probe_device() ----------------+         |    |
    |  |     -> driver_probe_device()           |         |    |
    |  |        -> CALL drv->probe() -----------|--+      |    |
    |  |  6. kobject_uevent() - notify udev     |  |      |    |
    |  +----------------------------------------|--|------+    |
    +------------------------------------------------|----------+
                                                     |
                                                     v
    +----------------------------------------------------------+
    |                    DRIVER LAYER                           |
    |  +----------------+  +----------------+  +---------------+|
    |  |  PCI driver    |  | Platform driver|  |   USB driver  ||
    |  |  my_pci_probe  |  | my_plat_probe  |  |  my_usb_probe ||
    |  +----------------+  +----------------+  +---------------+|
    +----------------------------------------------------------+
                              |
                              v
                      [ Hardware Initialized ]
```

**中文说明：**

设备模型是Linux内核中管理设备生命周期的子系统。设备发现（如PCI扫描）后，`device_add()`被调用。`device_add()`是模板方法：它初始化设备结构、通过`kobject_add()`添加到sysfs、创建属性文件、添加到总线，然后调用`bus_probe_device()`来匹配和探测驱动。如果匹配成功，调用驱动的`probe()`钩子，最后通过uevent通知udev。驱动只需实现`probe()`函数。

---

## The Template Method: device_add() and driver_probe_device()

### Components

| Component | Role |
|-----------|------|
| **Template Method** | `device_add()` / `driver_probe_device()` |
| **Fixed Steps** | kobject setup, sysfs, bus registration, uevent |
| **Variation Point** | `drv->probe()` |
| **Ops Table** | `struct device_driver` |

### Control Flow Diagram

```
    device_add(dev)
    ===============

    +----------------------------------+
    |  1. VALIDATE DEVICE              |
    |     - Check dev is valid         |
    |     - Assign device name         |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  2. KOBJECT ADD                  |
    |     - kobject_add()              |
    |     - Creates /sys/devices/...   |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  3. CREATE SYSFS ATTRIBUTES      |
    |     - device_create_file()       |
    |     - Standard dev attributes    |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  4. ADD TO BUS                   |
    |     - bus_add_device()           |
    |     - Link to bus's device list  |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  5. PROBE DEVICE                 |
    |     - bus_probe_device()         |
    |     - Match drivers to device    |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |     driver_probe_device()        |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  6. PRE-PROBE SETUP              |
    |     - Get reference to driver    |
    |     - pm_runtime_get_sync()      |
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  7. VARIATION POINT                    ||
    ||     drv->probe(dev)                    ||
    ||     Driver initializes hardware        ||
    +==========================================+
                   |
                   v
    +----------------------------------+
    |  8. POST-PROBE                   |
    |     - Update driver binding      |
    |     - driver_bound()             |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  9. SEND UEVENT                  |
    |     - kobject_uevent(KOBJ_ADD)   |
    |     - Notify userspace (udev)    |
    +----------------------------------+
```

**中文说明：**

`device_add()`和`driver_probe_device()`的控制流：(1) 验证设备；(2) kobject添加到sysfs；(3) 创建sysfs属性；(4) 添加到总线设备列表；(5) 探测设备（匹配驱动）；(6) 探测前设置（获取驱动引用、运行时电源管理）；(7) **变化点**——调用驱动的`probe()`初始化硬件；(8) 探测后处理（更新绑定状态）；(9) 发送uevent通知用户空间。

---

## Why Template Method is Required Here

### 1. Binding Order Must Be Strict

```
    LIFECYCLE ORDERING:

    +----------------------------------------------------------+
    | CORRECT ORDER (Framework Enforced):                      |
    +----------------------------------------------------------+
    |                                                          |
    | 1. device exists in device tree                          |
    |         |                                                |
    |         v                                                |
    | 2. kobject registered (sysfs visible)                    |
    |         |                                                |
    |         v                                                |
    | 3. device on bus list                                    |
    |         |                                                |
    |         v                                                |
    | 4. driver matched and probe() called                     |
    |         |                                                |
    |         v                                                |
    | 5. uevent sent (udev creates /dev node)                  |
    |                                                          |
    +----------------------------------------------------------+

    IF DRIVER CONTROLLED ORDER:
    +----------------------------------------------------------+
    | BROKEN SCENARIOS:                                        |
    |                                                          |
    | - Probe before kobject: no sysfs                         |
    | - Probe before bus add: no power management              |
    | - Uevent before probe: device not ready for use          |
    |                                                          |
    +----------------------------------------------------------+
```

**中文说明：**

绑定顺序必须严格遵守。框架强制执行的正确顺序：设备存在于设备树、kobject注册（sysfs可见）、设备在总线列表上、驱动匹配并调用probe()、发送uevent。如果驱动控制顺序，可能出现：probe在kobject前（无sysfs）、probe在bus_add前（无电源管理）、uevent在probe前（设备未准备好使用）。

### 2. Reference Counting Must Be Managed

```
    REFERENCE COUNTING AROUND PROBE:

    driver_probe_device(drv, dev) {
        /* Framework gets references */
        get_device(dev);           <-- Prevent device removal
        get_driver(drv);           <-- Prevent driver unload

        ret = drv->probe(dev);     <-- SAFE: both objects stable

        if (ret) {
            put_driver(drv);       <-- Cleanup on failure
            put_device(dev);
        }
    }

    IF DRIVER DID THIS:
    - Might forget to get reference
    - Device/driver could disappear mid-probe
    - Crash or data corruption
```

### 3. Power Management Integration

```
    PM INTEGRATION IN PROBE PATH:

    driver_probe_device() {
        /* Power on device before probe */
        pm_runtime_get_sync(dev);

        ret = drv->probe(dev);

        if (ret < 0) {
            pm_runtime_put(dev);
        }
        /* Device stays powered while bound */
    }

    BENEFITS:
    - Device guaranteed powered during probe
    - PM state machine consistent
    - No driver PM bugs during init
```

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL DEVICE MODEL TEMPLATE METHOD SIMULATION
 * 
 * Demonstrates the Template Method pattern in device probe path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct device;
struct device_driver;
struct bus_type;

/* ==========================================================
 * DEVICE DRIVER STRUCTURE
 * ========================================================== */
struct device_driver {
    const char *name;
    struct bus_type *bus;
    
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
};

/* ==========================================================
 * BUS TYPE STRUCTURE
 * ========================================================== */
struct bus_type {
    const char *name;
    
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
};

/* ==========================================================
 * DEVICE STRUCTURE
 * ========================================================== */
struct device {
    const char *name;
    struct device *parent;
    struct bus_type *bus;
    struct device_driver *driver;  /* Bound driver */
    
    void *platform_data;           /* Device-specific data */
    
    /* Reference counting */
    int ref_count;
    
    /* Kobject simulation */
    int kobject_registered;
    int sysfs_created;
    int on_bus_list;
};

/* ==========================================================
 * FRAMEWORK FIXED STEPS (Device Core)
 * ========================================================== */

static void get_device(struct device *dev)
{
    dev->ref_count++;
    printf("  [CORE] get_device(%s): refcount=%d\n", 
           dev->name, dev->ref_count);
}

static void put_device(struct device *dev)
{
    dev->ref_count--;
    printf("  [CORE] put_device(%s): refcount=%d\n", 
           dev->name, dev->ref_count);
}

static int kobject_add(struct device *dev)
{
    printf("  [CORE] kobject_add: registering %s in sysfs\n", dev->name);
    printf("  [CORE]   -> /sys/devices/%s\n", dev->name);
    dev->kobject_registered = 1;
    return 0;
}

static void kobject_del(struct device *dev)
{
    printf("  [CORE] kobject_del: removing %s from sysfs\n", dev->name);
    dev->kobject_registered = 0;
}

static int device_create_file(struct device *dev, const char *attr)
{
    printf("  [CORE] device_create_file: %s/%s\n", dev->name, attr);
    dev->sysfs_created = 1;
    return 0;
}

static int bus_add_device(struct device *dev)
{
    printf("  [CORE] bus_add_device: adding %s to bus %s\n",
           dev->name, dev->bus->name);
    dev->on_bus_list = 1;
    return 0;
}

static void bus_remove_device(struct device *dev)
{
    printf("  [CORE] bus_remove_device: removing %s from bus\n", dev->name);
    dev->on_bus_list = 0;
}

static void kobject_uevent(struct device *dev, const char *action)
{
    printf("  [CORE] kobject_uevent: %s %s\n", action, dev->name);
    printf("  [CORE]   -> udev notified, /dev node may be created\n");
}

static void pm_runtime_get_sync(struct device *dev)
{
    printf("  [CORE] pm_runtime_get_sync: powering on %s\n", dev->name);
}

static void pm_runtime_put(struct device *dev)
{
    printf("  [CORE] pm_runtime_put: allowing %s to sleep\n", dev->name);
}

/* ==========================================================
 * FRAMEWORK: driver_probe_device()
 * 
 * Inner template method for probing a single driver.
 * ========================================================== */
static int driver_probe_device(struct device_driver *drv, 
                                struct device *dev)
{
    int ret = 0;

    printf("  [CORE] driver_probe_device: trying %s with %s\n",
           dev->name, drv->name);

    /* ========== FIXED STEP 1: Get references ========== */
    get_device(dev);

    /* ========== FIXED STEP 2: Power management ========== */
    pm_runtime_get_sync(dev);

    /* ========== VARIATION POINT: Call driver probe ========== */
    printf("  [CORE] >>> Calling driver->probe()\n");
    if (drv->probe) {
        ret = drv->probe(dev);
    }
    printf("  [CORE] <<< Driver probe returned: %d\n", ret);

    /* ========== FIXED STEP 3: Handle result ========== */
    if (ret) {
        printf("  [CORE] Probe failed, cleaning up\n");
        pm_runtime_put(dev);
        put_device(dev);
        return ret;
    }

    /* ========== FIXED STEP 4: Bind driver to device ========== */
    dev->driver = drv;
    printf("  [CORE] driver_bound: %s bound to %s\n", 
           dev->name, drv->name);

    return 0;
}

/* ==========================================================
 * FRAMEWORK: bus_probe_device()
 * 
 * Matches device against all drivers on the bus.
 * ========================================================== */
static int bus_probe_device(struct device *dev)
{
    struct bus_type *bus = dev->bus;

    printf("  [CORE] bus_probe_device: finding driver for %s\n", dev->name);

    /* In real kernel: iterate over all drivers on bus */
    /* For simulation: just call bus->probe if exists */
    if (bus && bus->probe) {
        return bus->probe(dev);
    }
    
    return 0;
}

/* ==========================================================
 * TEMPLATE METHOD: device_add()
 * 
 * Main entry point for adding a device to the system.
 * ========================================================== */
int device_add(struct device *dev)
{
    int error;

    printf("[device_add] TEMPLATE METHOD START: %s\n", dev->name);

    /* ========== FIXED STEP 1: Validate device ========== */
    if (!dev) {
        printf("  [CORE] ERROR: null device\n");
        return -1;
    }
    printf("  [CORE] Device validated: %s\n", dev->name);

    /* ========== FIXED STEP 2: Initialize refcount ========== */
    dev->ref_count = 1;
    printf("  [CORE] Initial refcount: %d\n", dev->ref_count);

    /* ========== FIXED STEP 3: Add to sysfs ========== */
    error = kobject_add(dev);
    if (error) {
        printf("  [CORE] ERROR: kobject_add failed\n");
        return error;
    }

    /* ========== FIXED STEP 4: Create standard attributes ========== */
    device_create_file(dev, "uevent");
    device_create_file(dev, "power/state");

    /* ========== FIXED STEP 5: Add to bus ========== */
    if (dev->bus) {
        error = bus_add_device(dev);
        if (error) {
            printf("  [CORE] ERROR: bus_add_device failed\n");
            goto err_kobject;
        }
    }

    /* ========== FIXED STEP 6: Probe for driver ========== */
    bus_probe_device(dev);

    /* ========== FIXED STEP 7: Send uevent ========== */
    kobject_uevent(dev, "add");

    printf("[device_add] TEMPLATE METHOD END: %s\n\n", dev->name);
    return 0;

err_kobject:
    kobject_del(dev);
    return error;
}

/* ==========================================================
 * TEMPLATE METHOD: device_del()
 * 
 * Remove device from system.
 * ========================================================== */
void device_del(struct device *dev)
{
    printf("[device_del] TEMPLATE METHOD START: %s\n", dev->name);

    /* ========== FIXED STEP 1: Send remove uevent ========== */
    kobject_uevent(dev, "remove");

    /* ========== FIXED STEP 2: Unbind driver ========== */
    if (dev->driver && dev->driver->remove) {
        printf("  [CORE] >>> Calling driver->remove()\n");
        dev->driver->remove(dev);
        printf("  [CORE] <<< Driver remove returned\n");
        dev->driver = NULL;
    }

    /* ========== FIXED STEP 3: Remove from bus ========== */
    if (dev->bus) {
        bus_remove_device(dev);
    }

    /* ========== FIXED STEP 4: Remove from sysfs ========== */
    kobject_del(dev);

    printf("[device_del] TEMPLATE METHOD END: %s\n\n", dev->name);
}

/* ==========================================================
 * DRIVER IMPLEMENTATIONS (Variation Points)
 * ========================================================== */

/* --- PCI-like driver implementation --- */
static int my_pci_probe(struct device *dev)
{
    printf("    [PCI] Probing PCI device\n");
    printf("    [PCI] Reading PCI config space\n");
    printf("    [PCI] Enabling bus mastering\n");
    printf("    [PCI] Mapping BAR regions\n");
    printf("    [PCI] Registering interrupt handler\n");
    printf("    [PCI] Hardware initialized\n");
    return 0;  /* Success */
}

static int my_pci_remove(struct device *dev)
{
    printf("    [PCI] Removing PCI device\n");
    printf("    [PCI] Unregistering interrupt\n");
    printf("    [PCI] Unmapping BARs\n");
    return 0;
}

/* --- Platform driver implementation --- */
static int my_platform_probe(struct device *dev)
{
    printf("    [PLAT] Probing platform device\n");
    printf("    [PLAT] Getting resources from device tree\n");
    printf("    [PLAT] Mapping I/O memory\n");
    printf("    [PLAT] Configuring clocks and resets\n");
    printf("    [PLAT] Hardware initialized\n");
    return 0;
}

static int my_platform_remove(struct device *dev)
{
    printf("    [PLAT] Removing platform device\n");
    printf("    [PLAT] Releasing resources\n");
    return 0;
}

/* --- USB-like driver (probe failure demo) --- */
static int my_usb_probe(struct device *dev)
{
    printf("    [USB] Probing USB device\n");
    printf("    [USB] Checking device descriptor\n");
    printf("    [USB] ERROR: Unsupported device class!\n");
    return -1;  /* Failure */
}

/* ==========================================================
 * BUS IMPLEMENTATIONS
 * ========================================================== */

/* Global drivers for demo */
static struct device_driver pci_driver = {
    .name = "my_pci_driver",
    .probe = my_pci_probe,
    .remove = my_pci_remove,
};

static struct device_driver platform_driver = {
    .name = "my_platform_driver",
    .probe = my_platform_probe,
    .remove = my_platform_remove,
};

static struct device_driver usb_driver = {
    .name = "my_usb_driver",
    .probe = my_usb_probe,
    .remove = NULL,
};

/* PCI bus probe implementation */
static int pci_bus_probe(struct device *dev)
{
    /* In real kernel: match by vendor/device ID */
    return driver_probe_device(&pci_driver, dev);
}

static struct bus_type pci_bus = {
    .name = "pci",
    .probe = pci_bus_probe,
};

/* Platform bus probe implementation */
static int platform_bus_probe(struct device *dev)
{
    /* In real kernel: match by compatible string */
    return driver_probe_device(&platform_driver, dev);
}

static struct bus_type platform_bus = {
    .name = "platform",
    .probe = platform_bus_probe,
};

/* USB bus probe implementation */
static int usb_bus_probe(struct device *dev)
{
    return driver_probe_device(&usb_driver, dev);
}

static struct bus_type usb_bus = {
    .name = "usb",
    .probe = usb_bus_probe,
};

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("==============================================\n");
    printf("DEVICE MODEL TEMPLATE METHOD DEMONSTRATION\n");
    printf("==============================================\n\n");

    /* Create devices */
    struct device pci_dev = {
        .name = "0000:01:00.0",  /* PCI BDF notation */
        .bus = &pci_bus,
        .platform_data = NULL,
    };

    struct device platform_dev = {
        .name = "my-device@10000000",
        .bus = &platform_bus,
        .platform_data = NULL,
    };

    struct device usb_dev = {
        .name = "usb1/1-1",
        .bus = &usb_bus,
        .platform_data = NULL,
    };

    /* Add devices - demonstrates the template method */
    printf("=== Adding PCI Device ===\n");
    device_add(&pci_dev);

    printf("=== Adding Platform Device ===\n");
    device_add(&platform_dev);

    printf("=== Adding USB Device (probe will fail) ===\n");
    device_add(&usb_dev);

    /* Remove devices */
    printf("=== Removing PCI Device ===\n");
    device_del(&pci_dev);

    printf("=== Removing Platform Device ===\n");
    device_del(&platform_dev);

    return 0;
}
```

---

## What the Implementation is NOT Allowed to Do

```
+=============================================================================+
|              DRIVER PROBE IMPLEMENTATION RESTRICTIONS                        |
+=============================================================================+

    DRIVER CANNOT:

    1. CHANGE BINDING ORDER
       Framework calls probe() at the right time
       Driver cannot probe other devices

    2. MODIFY SYSFS STRUCTURE
       Framework creates standard attributes
       Driver can only add driver-specific ones

    3. SKIP UEVENT
       Framework sends uevent after probe success
       Driver cannot prevent or modify it

    4. HOLD REFERENCES INCORRECTLY
       Framework manages device/driver references
       Driver gets stable references during probe

    5. UNREGISTER DURING PROBE
       Cannot call device_del() from probe()
       Would corrupt framework state

    6. SLEEP INDEFINITELY
       Probe should complete in reasonable time
       Long delays block device enumeration

    7. FAIL SILENTLY
       Must return error code on failure
       Framework handles cleanup based on return

    +-----------------------------------------------------------------+
    |  PROBE IS A CALLBACK, NOT A CONTROL POINT:                      |
    |  - Device already exists in system                              |
    |  - Sysfs already visible                                        |
    |  - Power management already active                              |
    |  - Driver just initializes hardware                             |
    +-----------------------------------------------------------------+
```

**中文说明：**

驱动probe()实现的限制：(1) 不能改变绑定顺序——框架在正确时间调用probe；(2) 不能修改sysfs结构——框架创建标准属性；(3) 不能跳过uevent——框架在probe成功后发送；(4) 不能错误持有引用——框架管理引用计数；(5) 不能在probe期间注销——会破坏框架状态；(6) 不能无限睡眠——长延迟阻塞设备枚举；(7) 不能静默失败——必须返回错误码。probe是回调而非控制点：设备已存在于系统、sysfs已可见、电源管理已激活，驱动只是初始化硬件。

---

## Real Kernel Code Reference (v3.2)

### device_add() in drivers/base/core.c

```c
/* Simplified from actual kernel code */
int device_add(struct device *dev)
{
    struct device *parent = NULL;
    int error;

    dev = get_device(dev);
    if (!dev)
        return -EINVAL;

    /* Setup parent relationship */
    parent = get_device(dev->parent);

    /* Add kobject to sysfs */
    error = kobject_add(&dev->kobj, ...);
    if (error)
        goto Error;

    /* Create standard attributes */
    error = device_create_file(dev, &uevent_attr);
    if (error)
        goto attrError;

    /* Add to bus */
    error = bus_add_device(dev);
    if (error)
        goto BusError;

    /* Probe for driver */
    bus_probe_device(dev);

    /* Notify userspace */
    kobject_uevent(&dev->kobj, KOBJ_ADD);

    return 0;
    /* ... error handling ... */
}
```

### driver_probe_device() in drivers/base/dd.c

```c
int driver_probe_device(struct device_driver *drv, struct device *dev)
{
    int ret = 0;

    if (!device_is_registered(dev))
        return -ENODEV;

    pm_runtime_get_sync(dev);

    if (drv->probe)
        ret = drv->probe(dev);

    if (ret) {
        pm_runtime_put(dev);
        return ret;
    }

    driver_bound(dev);
    return 0;
}
```

---

## Key Takeaways

1. **Framework owns lifecycle**: `device_add()` controls all phases
2. **Order is mandatory**: kobject -> sysfs -> bus -> probe -> uevent
3. **References are managed**: Framework handles ref counting
4. **PM is automatic**: Power state managed around probe
5. **Drivers are simple**: Just initialize hardware in probe
