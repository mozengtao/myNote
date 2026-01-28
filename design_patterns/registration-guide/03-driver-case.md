# Case 1: PCI Driver Registration

PCI driver registration demonstrates the core registration pattern for device drivers.

---

## Subsystem Context

```
+=============================================================================+
|                    PCI DRIVER REGISTRATION                                   |
+=============================================================================+

    PCI SUBSYSTEM ROLE:
    ===================

    - Discovers PCI devices on bus
    - Maintains list of registered drivers
    - Matches devices to drivers by ID
    - Calls driver probe() when match found
    - Calls driver remove() when device removed


    REGISTRATION FLOW:
    ==================

    Module Load                      Device Discovery
    ===========                      ================

    1. module_init()                 1. PCI bus scan
           |                                |
           v                                v
    2. pci_register_driver()         2. For each device:
           |                                |
           v                                v
    3. Add to driver list            3. Match against drivers
                                           |
                                           v
                                     4. If match: probe()


    DRIVER STRUCTURE:
    =================

    struct pci_driver {
        const char *name;
        const struct pci_device_id *id_table;  /* Matching */
        int (*probe)(dev, id);                  /* Bind */
        void (*remove)(dev);                    /* Unbind */
        int (*suspend)(dev, state);             /* Power mgmt */
        int (*resume)(dev);
        /* ... */
    };
```

**中文说明：**

PCI驱动注册：PCI子系统发现设备、维护驱动列表、按ID匹配设备和驱动、匹配时调用probe、移除时调用remove。模块加载时调用pci_register_driver添加到驱动列表，设备发现时进行匹配。

---

## Minimal C Simulation

```c
/* Simplified PCI driver registration simulation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Device ID structure */
struct pci_device_id {
    unsigned int vendor;
    unsigned int device;
};

/* PCI device */
struct pci_dev {
    unsigned int vendor;
    unsigned int device;
    char name[32];
};

/* PCI driver */
struct pci_driver {
    const char *name;
    const struct pci_device_id *id_table;
    int (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
    void (*remove)(struct pci_dev *dev);
    struct pci_driver *next;
};

/* ====== PCI SUBSYSTEM (CORE) ====== */

static struct pci_driver *driver_list = NULL;
static struct pci_dev *device_list[10];
static int device_count = 0;

/* Check if driver matches device */
const struct pci_device_id *pci_match_device(struct pci_driver *drv,
                                              struct pci_dev *dev)
{
    const struct pci_device_id *id;
    
    for (id = drv->id_table; id->vendor || id->device; id++) {
        if (id->vendor == dev->vendor && id->device == dev->device)
            return id;
    }
    return NULL;
}

/* Register driver with PCI subsystem */
int pci_register_driver(struct pci_driver *drv)
{
    const struct pci_device_id *id;
    int i;
    
    printf("[PCI] Registering driver: %s\n", drv->name);
    
    /* Add to driver list */
    drv->next = driver_list;
    driver_list = drv;
    
    /* Try to match with existing devices */
    for (i = 0; i < device_count; i++) {
        struct pci_dev *dev = device_list[i];
        id = pci_match_device(drv, dev);
        if (id) {
            printf("[PCI] Match: %s <-> %s\n", drv->name, dev->name);
            drv->probe(dev, id);
        }
    }
    
    return 0;
}

/* Unregister driver */
void pci_unregister_driver(struct pci_driver *drv)
{
    struct pci_driver **pp;
    int i;
    
    printf("[PCI] Unregistering driver: %s\n", drv->name);
    
    /* Call remove for bound devices */
    for (i = 0; i < device_count; i++) {
        struct pci_dev *dev = device_list[i];
        if (pci_match_device(drv, dev)) {
            drv->remove(dev);
        }
    }
    
    /* Remove from driver list */
    for (pp = &driver_list; *pp; pp = &(*pp)->next) {
        if (*pp == drv) {
            *pp = drv->next;
            break;
        }
    }
}

/* Add device (simulates hot-plug) */
void pci_add_device(struct pci_dev *dev)
{
    struct pci_driver *drv;
    const struct pci_device_id *id;
    
    printf("[PCI] Device added: %s (vendor=0x%04x device=0x%04x)\n",
           dev->name, dev->vendor, dev->device);
    
    device_list[device_count++] = dev;
    
    /* Find matching driver */
    for (drv = driver_list; drv; drv = drv->next) {
        id = pci_match_device(drv, dev);
        if (id) {
            printf("[PCI] Match: %s <-> %s\n", drv->name, dev->name);
            drv->probe(dev, id);
            break;
        }
    }
}

/* ====== EXAMPLE DRIVER ====== */

/* Device IDs this driver supports */
static const struct pci_device_id e1000_ids[] = {
    { 0x8086, 0x1234 },  /* Intel E1000 variant 1 */
    { 0x8086, 0x5678 },  /* Intel E1000 variant 2 */
    { 0, 0 }             /* Terminator */
};

/* Probe callback - device found */
int e1000_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    printf("  [E1000] Probe: Initializing %s\n", dev->name);
    printf("  [E1000] Allocating resources...\n");
    printf("  [E1000] Device ready\n");
    return 0;
}

/* Remove callback - device removed */
void e1000_remove(struct pci_dev *dev)
{
    printf("  [E1000] Remove: Cleaning up %s\n", dev->name);
    printf("  [E1000] Releasing resources...\n");
}

/* Driver structure */
static struct pci_driver e1000_driver = {
    .name = "e1000",
    .id_table = e1000_ids,
    .probe = e1000_probe,
    .remove = e1000_remove,
};

/* Module init/exit simulation */
void e1000_module_init(void)
{
    printf("\n=== E1000 Module Loading ===\n");
    pci_register_driver(&e1000_driver);
}

void e1000_module_exit(void)
{
    printf("\n=== E1000 Module Unloading ===\n");
    pci_unregister_driver(&e1000_driver);
}

/* ====== SIMULATION ====== */

int main(void)
{
    /* Create some devices */
    static struct pci_dev dev1 = { 0x8086, 0x1234, "eth0" };
    static struct pci_dev dev2 = { 0x10ec, 0x8139, "eth1" };
    static struct pci_dev dev3 = { 0x8086, 0x5678, "eth2" };
    
    printf("=== PCI REGISTRATION SIMULATION ===\n\n");
    
    /* Boot: discover devices first */
    printf("--- Boot: Discovering devices ---\n");
    pci_add_device(&dev1);
    pci_add_device(&dev2);  /* No driver for this */
    
    /* Load driver module */
    e1000_module_init();
    
    /* Hot-plug: new device */
    printf("\n--- Hot-plug: New device ---\n");
    pci_add_device(&dev3);
    
    /* Unload driver module */
    e1000_module_exit();
    
    return 0;
}

/*
 * Output:
 *
 * === PCI REGISTRATION SIMULATION ===
 *
 * --- Boot: Discovering devices ---
 * [PCI] Device added: eth0 (vendor=0x8086 device=0x1234)
 * [PCI] Device added: eth1 (vendor=0x10ec device=0x8139)
 *
 * === E1000 Module Loading ===
 * [PCI] Registering driver: e1000
 * [PCI] Match: e1000 <-> eth0
 *   [E1000] Probe: Initializing eth0
 *   [E1000] Allocating resources...
 *   [E1000] Device ready
 *
 * --- Hot-plug: New device ---
 * [PCI] Device added: eth2 (vendor=0x8086 device=0x5678)
 * [PCI] Match: e1000 <-> eth2
 *   [E1000] Probe: Initializing eth2
 *   [E1000] Allocating resources...
 *   [E1000] Device ready
 *
 * === E1000 Module Unloading ===
 * [PCI] Unregistering driver: e1000
 *   [E1000] Remove: Cleaning up eth0
 *   [E1000] Releasing resources...
 *   [E1000] Remove: Cleaning up eth2
 *   [E1000] Releasing resources...
 */
```

---

## What Core Does NOT Control

```
    PCI Subsystem Controls:
    -----------------------
    [X] Device discovery
    [X] Driver matching
    [X] Calling probe/remove
    [X] Power management coordination

    Driver Controls:
    ----------------
    [X] Which devices to support (id_table)
    [X] How to initialize device (probe)
    [X] How to cleanup (remove)
    [X] Device-specific logic
```

---

## Version

Based on **Linux kernel v3.2** drivers/pci/.
