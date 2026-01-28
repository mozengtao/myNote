# Case 2: Device Hierarchy

The device model demonstrates Composite pattern for hardware hierarchy.

---

## Subsystem Context

```
+=============================================================================+
|                    DEVICE HIERARCHY                                          |
+=============================================================================+

    DEVICE MODEL HIERARCHY:
    =======================

    Physical hardware forms natural hierarchy:
    
    PCI Bus
        |
        +-- PCI Device (network controller)
                |
                +-- Network Interface (eth0)

    The kernel models this with struct device hierarchy.


    DEVICE STRUCTURE:
    =================

    struct device {
        struct device *parent;        /* Parent device */
        struct kobject kobj;          /* Embedded kobject */
        struct bus_type *bus;
        struct device_driver *driver;
        struct klist_node knode_parent;
        struct klist klist_children;  /* Child devices */
        /* ... */
    };


    HIERARCHY EXAMPLE:
    ==================

    device: platform_bus
        |
        +-- device: pci0000:00 (host bridge)
                |
                +-- device: 0000:00:1f.0 (PCI device)
                        |
                        +-- device: eth0 (network)
```

**中文说明：**

设备模型层次：物理硬件形成自然层次（PCI总线->PCI设备->网络接口）。内核用struct device层次建模，每个device有parent指针和children列表。

---

## Key Functions

```c
/* Add device to hierarchy */
int device_add(struct device *dev)
{
    /* Set parent */
    if (dev->parent)
        klist_add_tail(&dev->knode_parent,
                       &parent->klist_children);
    
    /* Add kobject */
    kobject_add(&dev->kobj, &parent->kobj, ...);
    
    /* Create sysfs entries */
    /* Notify bus */
    /* Trigger uevent */
}

/* Remove device from hierarchy */
void device_del(struct device *dev)
{
    /* Remove from parent's children */
    /* Remove kobject */
    /* Remove sysfs entries */
}

/* Get/put reference */
struct device *get_device(struct device *dev);
void put_device(struct device *dev);
```

---

## Minimal Simulation

```c
/* Simplified device hierarchy */

#include <stdio.h>
#include <string.h>

struct device {
    char name[32];
    struct device *parent;
    struct device *children;
    struct device *sibling;
    int refcount;
};

void device_initialize(struct device *dev, const char *name)
{
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->parent = NULL;
    dev->children = NULL;
    dev->sibling = NULL;
    dev->refcount = 1;
}

int device_add(struct device *dev, struct device *parent)
{
    dev->parent = parent;
    
    if (parent) {
        dev->sibling = parent->children;
        parent->children = dev;
    }
    
    printf("[DEV] Added '%s' under '%s'\n",
           dev->name, parent ? parent->name : "(root)");
    return 0;
}

void print_device_tree(struct device *dev, int depth)
{
    struct device *child;
    int i;
    
    for (i = 0; i < depth; i++)
        printf("    ");
    printf("%s\n", dev->name);
    
    for (child = dev->children; child; child = child->sibling) {
        print_device_tree(child, depth + 1);
    }
}

int main(void)
{
    struct device platform_bus = {};
    struct device pci_bus = {};
    struct device pci_dev = {};
    struct device eth0 = {};
    
    printf("=== DEVICE HIERARCHY ===\n\n");
    
    device_initialize(&platform_bus, "platform_bus");
    device_initialize(&pci_bus, "pci0000:00");
    device_initialize(&pci_dev, "0000:00:1f.0");
    device_initialize(&eth0, "eth0");
    
    device_add(&platform_bus, NULL);
    device_add(&pci_bus, &platform_bus);
    device_add(&pci_dev, &pci_bus);
    device_add(&eth0, &pci_dev);
    
    printf("\n--- Device Tree ---\n");
    print_device_tree(&platform_bus, 0);
    
    return 0;
}
```

---

## Why Hierarchy Matters

```
    DEVICE HIERARCHY ENABLES:
    =========================
    
    1. Power Management
       - Suspend children before parent
       - Resume parent before children
    
    2. Hot-plug
       - Parent device controls child discovery
       - Child removed when parent removed
    
    3. Resource Management
       - Children inherit parent's resources
       - DMA coherent memory from parent
    
    4. Sysfs Representation
       - /sys/devices/ reflects hierarchy
       - User space sees relationships
```

---

## Version

Based on **Linux kernel v3.2** drivers/base/.
