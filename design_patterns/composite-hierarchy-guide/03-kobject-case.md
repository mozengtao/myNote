# Case 1: Kobject/Kset Hierarchy

The kobject system demonstrates the core Composite pattern in the Linux kernel.

---

## Subsystem Context

```
+=============================================================================+
|                    KOBJECT HIERARCHY                                         |
+=============================================================================+

    KOBJECT IS THE FOUNDATION:
    ==========================

    Almost all kernel objects that appear in sysfs
    use kobject for hierarchy management.

    - Devices (struct device)
    - Drivers (struct device_driver)
    - Buses (struct bus_type)
    - Classes (struct class)


    HIERARCHY STRUCTURE:
    ====================

    kset: "devices"
        |
        +-- kobject: "pci0000:00"
        |       |
        |       +-- kobject: "0000:00:1f.0"
        |               |
        |               +-- kset: "net"
        |                       |
        |                       +-- kobject: "eth0"
        |
        +-- kobject: "platform"
                |
                +-- kobject: "serial8250"


    SYSFS MAPPING:
    ==============

    /sys/devices/
        pci0000:00/
            0000:00:1f.0/
                net/
                    eth0/
        platform/
            serial8250/
```

**中文说明：**

Kobject是基础：几乎所有出现在sysfs中的内核对象都使用kobject管理层次结构。层次结构：kset包含kobject，kobject可以有子kobject。Sysfs直接反映kobject层次。

---

## Key Structures

```c
/* include/linux/kobject.h */

struct kobject {
    const char          *name;
    struct list_head    entry;      /* Sibling list */
    struct kobject      *parent;    /* Parent kobject */
    struct kset         *kset;      /* Container kset */
    struct kobj_type    *ktype;     /* Type operations */
    struct sysfs_dirent *sd;        /* Sysfs directory */
    struct kref         kref;       /* Reference count */
    /* ... */
};

struct kset {
    struct list_head list;          /* Children kobjects */
    spinlock_t list_lock;
    struct kobject kobj;            /* kset IS a kobject */
    const struct kset_uevent_ops *uevent_ops;
};

struct kobj_type {
    void (*release)(struct kobject *kobj);  /* Destructor */
    const struct sysfs_ops *sysfs_ops;
    struct attribute **default_attrs;
};
```

---

## Minimal C Simulation

```c
/* Simplified kobject hierarchy simulation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simplified kobject */
struct kobject {
    char name[64];
    struct kobject *parent;
    int refcount;
    struct kobject *children;
    struct kobject *sibling;
};

/* Initialize kobject */
void kobject_init(struct kobject *kobj, const char *name)
{
    strncpy(kobj->name, name, sizeof(kobj->name) - 1);
    kobj->parent = NULL;
    kobj->refcount = 1;
    kobj->children = NULL;
    kobj->sibling = NULL;
}

/* Add to parent */
int kobject_add(struct kobject *kobj, struct kobject *parent)
{
    kobj->parent = parent;
    
    if (parent) {
        /* Add to parent's children list */
        kobj->sibling = parent->children;
        parent->children = kobj;
        parent->refcount++;  /* Parent holds reference */
    }
    
    printf("[KOBJ] Added '%s' under '%s'\n",
           kobj->name, parent ? parent->name : "(root)");
    return 0;
}

/* Get reference */
struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj) {
        kobj->refcount++;
        printf("[KOBJ] Get '%s' (ref=%d)\n", kobj->name, kobj->refcount);
    }
    return kobj;
}

/* Put reference */
void kobject_put(struct kobject *kobj)
{
    if (kobj) {
        kobj->refcount--;
        printf("[KOBJ] Put '%s' (ref=%d)\n", kobj->name, kobj->refcount);
        if (kobj->refcount == 0) {
            printf("[KOBJ] Releasing '%s'\n", kobj->name);
            /* Would call ktype->release() here */
        }
    }
}

/* Print hierarchy */
void print_hierarchy(struct kobject *kobj, int depth)
{
    struct kobject *child;
    int i;
    
    for (i = 0; i < depth; i++)
        printf("  ");
    printf("+-- %s (ref=%d)\n", kobj->name, kobj->refcount);
    
    for (child = kobj->children; child; child = child->sibling) {
        print_hierarchy(child, depth + 1);
    }
}

/* Build path (like sysfs path) */
void get_path(struct kobject *kobj, char *buf, int size)
{
    if (kobj->parent) {
        get_path(kobj->parent, buf, size);
        strncat(buf, "/", size - strlen(buf) - 1);
    }
    strncat(buf, kobj->name, size - strlen(buf) - 1);
}

int main(void)
{
    struct kobject devices, pci, net, eth0;
    char path[256];
    
    printf("=== KOBJECT HIERARCHY SIMULATION ===\n\n");
    
    /* Build hierarchy */
    printf("--- Building hierarchy ---\n");
    kobject_init(&devices, "devices");
    kobject_init(&pci, "pci0000:00");
    kobject_init(&net, "net");
    kobject_init(&eth0, "eth0");
    
    kobject_add(&devices, NULL);
    kobject_add(&pci, &devices);
    kobject_add(&net, &pci);
    kobject_add(&eth0, &net);
    
    /* Print hierarchy */
    printf("\n--- Hierarchy ---\n");
    print_hierarchy(&devices, 0);
    
    /* Get path */
    printf("\n--- Path for eth0 ---\n");
    path[0] = '\0';
    get_path(&eth0, path, sizeof(path));
    printf("/sys/%s\n", path);
    
    /* Reference counting */
    printf("\n--- Reference counting ---\n");
    kobject_get(&eth0);
    kobject_put(&eth0);
    
    return 0;
}

/*
 * Output:
 *
 * === KOBJECT HIERARCHY SIMULATION ===
 *
 * --- Building hierarchy ---
 * [KOBJ] Added 'devices' under '(root)'
 * [KOBJ] Added 'pci0000:00' under 'devices'
 * [KOBJ] Added 'net' under 'pci0000:00'
 * [KOBJ] Added 'eth0' under 'net'
 *
 * --- Hierarchy ---
 * +-- devices (ref=2)
 *   +-- pci0000:00 (ref=2)
 *     +-- net (ref=2)
 *       +-- eth0 (ref=1)
 *
 * --- Path for eth0 ---
 * /sys/devices/pci0000:00/net/eth0
 *
 * --- Reference counting ---
 * [KOBJ] Get 'eth0' (ref=2)
 * [KOBJ] Put 'eth0' (ref=1)
 */
```

---

## What Core Does NOT Control

```
    Kobject Core Controls:
    ----------------------
    [X] Parent-child relationships
    [X] Reference counting
    [X] Sysfs representation
    [X] Naming

    Embedding Object Controls:
    --------------------------
    [X] When to create/destroy
    [X] Object-specific attributes
    [X] Object-specific behavior
```

---

## Version

Based on **Linux kernel v3.2** lib/kobject.c.
