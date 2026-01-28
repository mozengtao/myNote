# Identification Rules: Composite/Hierarchy Pattern

Five concrete rules to identify Composite pattern in Linux kernel source code.

---

## Rule 1: Look for Parent Pointer in Structure

```c
/* Parent pointer indicates hierarchy */
struct kobject {
    struct kobject *parent;  /* <-- Parent pointer */
};

struct device {
    struct device *parent;   /* <-- Parent device */
    struct kobject kobj;
};

struct dentry {
    struct dentry *d_parent; /* <-- Parent directory */
};

/* SIGNAL: struct X contains struct X *parent */
```

**中文说明：**

规则1：查找结构体中的parent指针——层次结构的标志。

---

## Rule 2: Look for Children List

```c
/* Children list for tree structure */
struct kset {
    struct list_head list;   /* <-- Children list */
    struct kobject kobj;
};

struct device {
    struct klist_node knode_parent;
    struct klist klist_children;  /* <-- Children */
};

/* SIGNAL: list_head or klist for children */
```

---

## Rule 3: Look for Embedded kobject

```c
/* Embedded kobject indicates sysfs-visible object */
struct device {
    struct kobject kobj;     /* <-- Embedded */
};

struct bus_type {
    struct subsys_private *p;  /* Contains kobject */
};

struct device_driver {
    struct driver_private *p;  /* Contains kobject */
};

/* SIGNAL: struct kobject as member (not pointer) */
```

**中文说明：**

规则3：查找嵌入的kobject——表示sysfs可见的对象，使用container_of访问。

---

## Rule 4: Look for kobject_* Functions

```c
/* Kobject lifecycle functions */
kobject_init_and_add(&kobj, ktype, parent, "name");
kobject_del(&kobj);
kobject_get(&kobj);
kobject_put(&kobj);
kobject_create_and_add("name", parent);

/* SIGNAL: kobject_* function calls */
```

---

## Rule 5: Look for Sysfs Directory Structure

```c
/* Sysfs path reflects hierarchy */
/* /sys/devices/pci0000:00/0000:00:1f.0/net/eth0 */

/* Create attribute */
sysfs_create_file(&kobj, &attr);
sysfs_create_group(&kobj, &attr_group);

/* SIGNAL: sysfs_* operations */
```

---

## Summary Checklist

```
+=============================================================================+
|                    COMPOSITE IDENTIFICATION CHECKLIST                        |
+=============================================================================+

    [ ] 1. PARENT POINTER
        struct X *parent in structure
    
    [ ] 2. CHILDREN LIST
        list_head or klist for children
    
    [ ] 3. EMBEDDED KOBJECT
        struct kobject as member
    
    [ ] 4. KOBJECT FUNCTIONS
        kobject_init_and_add, kobject_put, etc.
    
    [ ] 5. SYSFS INTEGRATION
        sysfs_create_file, visible in /sys/

    SCORING:
    3+ indicators = Composite/Hierarchy pattern
```

---

## Version

Based on **Linux kernel v3.2**.
