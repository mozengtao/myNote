# Core Concept: Composite/Hierarchy Pattern

What the Composite pattern means in kernel architecture for managing hierarchical object relationships.

---

## What Problem Does Composite Solve?

```
+=============================================================================+
|                    THE HIERARCHY PROBLEM                                     |
+=============================================================================+

    KERNEL OBJECTS FORM HIERARCHIES:
    ================================

    Device Hierarchy:
    /sys/devices/
        pci0000:00/                    <- PCI bus
            0000:00:1f.0/              <- PCI device
                net/                   <- subsystem
                    eth0/              <- network device

    Directory Hierarchy:
    /
        home/
            user/
                file.txt

    Kobject Hierarchy:
    - Every kobject can have a parent
    - ksets contain multiple kobjects
    - Forms sysfs structure


    WITHOUT COMPOSITE:
    ==================

    struct device {
        struct pci_device *pci_parent;
        struct usb_device *usb_parent;
        struct platform_device *platform_parent;
        /* Different parent types = messy */
    };


    WITH COMPOSITE (KOBJECT):
    =========================

    struct kobject {
        struct kobject *parent;   /* Uniform parent type */
        struct kset *kset;        /* Container */
        struct list_head entry;   /* Sibling list */
        struct kref kref;         /* Lifecycle */
    };

    /* All devices use same kobject mechanism */
    struct device {
        struct kobject kobj;      /* Embedded kobject */
        struct device *parent;    /* Points to parent device */
    };
```

**中文说明：**

组合模式解决的问题：内核对象形成层次结构（设备层次、目录层次、kobject层次）。没有组合模式，不同父类型会导致混乱。有了kobject，所有对象使用统一的parent指针机制。

---

## How Composite Works in Kernel

```
+=============================================================================+
|                    KOBJECT HIERARCHY                                         |
+=============================================================================+

    KOBJECT STRUCTURE:
    ==================

    struct kobject {
        const char *name;           /* Object name */
        struct kobject *parent;     /* Parent in tree */
        struct kset *kset;          /* Container set */
        struct kobj_type *ktype;    /* Type operations */
        struct sysfs_dirent *sd;    /* Sysfs representation */
        struct kref kref;           /* Reference count */
    };


    KSET (CONTAINER):
    =================

    struct kset {
        struct list_head list;      /* List of children */
        struct kobject kobj;        /* kset IS a kobject */
        /* ... */
    };


    TREE FORMATION:
    ===============

                    kset (root)
                        |
            +-----------+-----------+
            |           |           |
         kobject     kobject      kset
                                    |
                              +-----+-----+
                              |           |
                           kobject     kobject


    KEY OPERATIONS:
    ===============

    kobject_init_and_add(kobj, ktype, parent, name)
        - Initialize kobject
        - Set parent
        - Create sysfs entry

    kobject_del(kobj)
        - Remove from parent
        - Remove sysfs entry

    kobject_get(kobj) / kobject_put(kobj)
        - Reference counting
        - Prevents premature deletion
```

**中文说明：**

Kobject层次机制：kobject有parent指针指向父节点，kset是kobject的容器（本身也是kobject）。关键操作：kobject_init_and_add初始化并添加到父节点，kobject_del移除，kobject_get/put管理引用计数。

---

## Sysfs Integration

```
    SYSFS REFLECTS KOBJECT HIERARCHY:
    =================================

    Kernel Objects:              Sysfs Directory:
    
    kset: devices                /sys/devices/
        |
        +-- kobj: pci0000:00     /sys/devices/pci0000:00/
                |
                +-- kobj: eth0   /sys/devices/pci0000:00/net/eth0/


    ATTRIBUTES:
    ===========

    Each kobject can have attributes -> sysfs files
    
    /sys/devices/pci0000:00/net/eth0/
        address          <- attribute file
        mtu              <- attribute file
        statistics/      <- attribute group
```

---

## Why Kernel Uses Composite

```
    1. UNIFORM INTERFACE
       - All objects use same kobject
       - Same reference counting
       - Same sysfs integration
    
    2. HIERARCHICAL NAMING
       - Path reflects hierarchy
       - /sys/devices/pci0000:00/0000:00:1f.0/...
    
    3. LIFETIME MANAGEMENT
       - Parents hold references to children
       - Children released before parents
    
    4. SYSFS AUTOMATIC
       - Hierarchy automatically exposed
       - User space sees object relationships
```

---

## Composite vs Other Patterns

```
    COMPOSITE:                       SIMPLE POINTERS:
    ==========                       ================
    
    Uniform parent type              Ad-hoc parent pointers
    Reference counted                Manual lifetime
    Sysfs integration                No visibility
    Tree operations                  Manual traversal
```

---

## Version

Based on **Linux kernel v3.2** kobject implementation.
