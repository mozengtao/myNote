# Linux Kernel kobject/sysfs Object Model (v3.2)

## Overview

This document explains **kobject and sysfs architecture** in Linux kernel v3.2, focusing on hierarchical object lifetimes and introspection.

---

## kobject Purpose

From `include/linux/kobject.h`:

```c
struct kobject {
    const char *name;
    struct list_head entry;
    struct kobject *parent;
    struct kset *kset;
    struct kobj_type *ktype;
    struct sysfs_dirent *sd;
    struct kref kref;
    unsigned int state_initialized:1;
    unsigned int state_in_sysfs:1;
    unsigned int state_add_uevent_sent:1;
    unsigned int state_remove_uevent_sent:1;
    unsigned int uevent_suppress:1;
};
```

```
+------------------------------------------------------------------+
|  KOBJECT: THE KERNEL OBJECT ABSTRACTION                          |
+------------------------------------------------------------------+

    PURPOSE:
    +----------------------------------------------------------+
    | 1. Reference counting (kref)                              |
    | 2. Parent-child relationships                             |
    | 3. Sysfs representation                                   |
    | 4. Hotplug event notification                             |
    +----------------------------------------------------------+

    WHAT EMBEDS KOBJECT:
    +----------------------------------------------------------+
    | struct device       → /sys/devices/...                    |
    | struct device_driver → /sys/bus/.../drivers/...           |
    | struct bus_type     → /sys/bus/...                        |
    | struct class        → /sys/class/...                      |
    | struct module       → /sys/module/...                     |
    +----------------------------------------------------------+

    EMBEDDING PATTERN:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  struct device {                                             │
    │      struct kobject kobj;   ← Embedded, not pointer         │
    │      const char *name;                                       │
    │      struct bus_type *bus;                                   │
    │      struct device *parent;                                  │
    │      /* ... */                                               │
    │  };                                                          │
    │                                                              │
    │  Recovery: container_of(&dev->kobj, struct device, kobj)    │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- kobject 目的：引用计数、父子关系、sysfs 表示、热插拔事件
- 嵌入 kobject 的结构：device、device_driver、bus_type、class、module
- 嵌入模式：直接嵌入而非指针，用 container_of 恢复

---

## Reference Counting

```
+------------------------------------------------------------------+
|  KOBJECT LIFECYCLE                                               |
+------------------------------------------------------------------+

    1. INITIALIZE
    ┌─────────────────────────────────────────────────────────────┐
    │ kobject_init(kobj, ktype);                                   │
    │   - Sets refcount to 1                                       │
    │   - Sets ktype (operations table)                            │
    │   - NOT visible in sysfs yet                                 │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    2. ADD TO SYSFS
    ┌─────────────────────────────────────────────────────────────┐
    │ kobject_add(kobj, parent, "name");                           │
    │   - Creates sysfs directory                                  │
    │   - Links to parent                                          │
    │   - Visible in /sys/...                                      │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    3. GET REFERENCE
    ┌─────────────────────────────────────────────────────────────┐
    │ kobject_get(kobj);                                           │
    │   - Increments refcount                                      │
    │   - Object guaranteed to exist                               │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    4. PUT REFERENCE
    ┌─────────────────────────────────────────────────────────────┐
    │ kobject_put(kobj);                                           │
    │   - Decrements refcount                                      │
    │   - If refcount == 0, calls ktype->release()                 │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    5. DELETE FROM SYSFS
    ┌─────────────────────────────────────────────────────────────┐
    │ kobject_del(kobj);                                           │
    │   - Removes from sysfs                                       │
    │   - Does NOT free object                                     │
    │   - Object may still have references                         │
    └─────────────────────────────────────────────────────────────┘

    RELEASE CALLBACK:
    
    struct kobj_type {
        void (*release)(struct kobject *kobj);  ← Called when refcnt=0
        const struct sysfs_ops *sysfs_ops;
        struct attribute **default_attrs;
    };
```

**中文解释：**
- 生命周期：初始化 → 添加到sysfs → 获取引用 → 释放引用 → 从sysfs删除
- 引用计数：refcount=0 时调用 ktype->release()
- kobject_del 只从 sysfs 移除，不释放对象

---

## Parent-Child Lifetimes

```
+------------------------------------------------------------------+
|  PARENT-CHILD OWNERSHIP                                          |
+------------------------------------------------------------------+

    SYSFS HIERARCHY:
    
    /sys/
    └── devices/
        └── pci0000:00/           ← Parent device
            └── 0000:00:1f.0/     ← Child device
                └── usb1/         ← Grandchild
                    └── 1-1/      ← Great-grandchild

    KOBJECT TREE:
    
    ┌────────────────┐
    │ devices_kset   │
    │   (root)       │
    └───────┬────────┘
            │ parent
            ▼
    ┌────────────────┐       ┌────────────────┐
    │ pci0000:00     │──────▶│   children     │
    │   kobject      │       │   list         │
    └───────┬────────┘       └────────────────┘
            │ parent
            ▼
    ┌────────────────┐       ┌────────────────┐
    │ 0000:00:1f.0   │──────▶│   children     │
    │   kobject      │       │   list         │
    └───────┬────────┘       └────────────────┘
            │ parent
            ▼
    ┌────────────────┐
    │    usb1        │
    │   kobject      │
    └────────────────┘

    LIFETIME RULES:
    +----------------------------------------------------------+
    | 1. Parent holds reference to child (implicit)             |
    | 2. Child holds reference to parent (kobject->parent)      |
    | 3. Parent cannot be freed while children exist            |
    | 4. Removing parent removes children first                 |
    +----------------------------------------------------------+

    SAFE REMOVAL SEQUENCE:
    
    remove_child_devices();    /* Remove children first */
    kobject_del(&parent->kobj); /* Then remove parent */
    kobject_put(&parent->kobj); /* Finally put reference */
```

**中文解释：**
- sysfs 层次结构：devices → pci → usb → ...
- 生命周期规则：
  1. 父持有子的引用
  2. 子持有父的引用
  3. 有子存在时父不能释放
  4. 移除父先移除子

---

## Sysfs Exposure

```
+------------------------------------------------------------------+
|  SYSFS: KERNEL OBJECT INTROSPECTION                              |
+------------------------------------------------------------------+

    SYSFS STRUCTURE:
    
    /sys/
    ├── block/          ← Block devices
    ├── bus/            ← Bus types (pci, usb, ...)
    ├── class/          ← Device classes (net, tty, ...)
    ├── devices/        ← Device hierarchy
    ├── firmware/       ← Firmware objects
    ├── fs/             ← Filesystems
    ├── kernel/         ← Kernel parameters
    ├── module/         ← Loaded modules
    └── power/          ← Power management

    ATTRIBUTE FILES:
    
    /sys/devices/pci0000:00/0000:00:1f.0/
    ├── class           ← Read-only attribute
    ├── config          ← Read-write binary attribute
    ├── device          ← Symlink
    ├── enable          ← Read-write attribute
    ├── irq             ← Read-only attribute
    ├── resource        ← Read-only attribute
    └── vendor          ← Read-only attribute

    ATTRIBUTE DEFINITION:
    
    struct attribute {
        const char *name;
        mode_t mode;        /* Permissions */
    };
    
    struct kobj_attribute {
        struct attribute attr;
        ssize_t (*show)(struct kobject *kobj, 
                        struct kobj_attribute *attr, char *buf);
        ssize_t (*store)(struct kobject *kobj,
                         struct kobj_attribute *attr,
                         const char *buf, size_t count);
    };

    EXAMPLE ATTRIBUTE:
    
    static ssize_t my_show(struct kobject *kobj,
                           struct kobj_attribute *attr, char *buf)
    {
        struct my_device *dev = container_of(kobj, 
                                             struct my_device, kobj);
        return sprintf(buf, "%d\n", dev->value);
    }
    
    static ssize_t my_store(struct kobject *kobj,
                            struct kobj_attribute *attr,
                            const char *buf, size_t count)
    {
        struct my_device *dev = container_of(kobj,
                                             struct my_device, kobj);
        sscanf(buf, "%d", &dev->value);
        return count;
    }
    
    static struct kobj_attribute my_attr =
        __ATTR(my_value, 0644, my_show, my_store);
```

**中文解释：**
- sysfs 结构：block、bus、class、devices、module 等目录
- 属性文件：只读、读写、二进制
- 属性定义：show（读）和 store（写）回调
- container_of 从 kobject 恢复对象

---

## User-Space Object Tree

```c
/* User-space object tree inspired by kobject */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Equivalent to kref */
struct kref {
    int count;
};

/* Equivalent to kobject */
struct kobject {
    char *name;
    struct kref kref;
    struct kobject *parent;
    struct list_head children;
    struct list_head sibling;
    void (*release)(struct kobject *kobj);
};

/* List implementation */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

void list_init(struct list_head *list)
{
    list->next = list->prev = list;
}

void list_add(struct list_head *new, struct list_head *head)
{
    new->next = head->next;
    new->prev = head;
    head->next->prev = new;
    head->next = new;
}

void list_del(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

int list_empty(struct list_head *head)
{
    return head->next == head;
}

/* kobject operations */
void kobject_init(struct kobject *kobj, const char *name,
                  void (*release)(struct kobject *))
{
    kobj->name = strdup(name);
    kobj->kref.count = 1;
    kobj->parent = NULL;
    kobj->release = release;
    list_init(&kobj->children);
    list_init(&kobj->sibling);
}

struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj)
        kobj->kref.count++;
    return kobj;
}

void kobject_put(struct kobject *kobj)
{
    if (!kobj) return;
    
    if (--kobj->kref.count == 0) {
        /* Release parent reference */
        if (kobj->parent)
            kobject_put(kobj->parent);
        
        /* Call release callback */
        if (kobj->release)
            kobj->release(kobj);
        
        free(kobj->name);
    }
}

int kobject_add(struct kobject *kobj, struct kobject *parent)
{
    kobj->parent = kobject_get(parent);
    
    if (parent) {
        list_add(&kobj->sibling, &parent->children);
    }
    
    return 0;
}

void kobject_del(struct kobject *kobj)
{
    /* Remove from parent's children list */
    if (!list_empty(&kobj->sibling)) {
        list_del(&kobj->sibling);
        list_init(&kobj->sibling);
    }
}

/* Print tree (like sysfs ls) */
void print_tree(struct kobject *kobj, int depth)
{
    for (int i = 0; i < depth; i++)
        printf("  ");
    printf("%s (ref=%d)\n", kobj->name, kobj->kref.count);
    
    struct list_head *pos;
    for (pos = kobj->children.next; pos != &kobj->children; 
         pos = pos->next) {
        struct kobject *child = container_of(pos, struct kobject, 
                                             sibling);
        print_tree(child, depth + 1);
    }
}

/* Example: Device hierarchy */
struct my_device {
    struct kobject kobj;
    int device_id;
};

void my_device_release(struct kobject *kobj)
{
    struct my_device *dev = container_of(kobj, struct my_device, kobj);
    printf("Releasing device %d\n", dev->device_id);
    free(dev);
}

struct my_device *my_device_create(const char *name, int id,
                                    struct kobject *parent)
{
    struct my_device *dev = malloc(sizeof(*dev));
    dev->device_id = id;
    kobject_init(&dev->kobj, name, my_device_release);
    kobject_add(&dev->kobj, parent);
    return dev;
}

void my_device_destroy(struct my_device *dev)
{
    kobject_del(&dev->kobj);
    kobject_put(&dev->kobj);
}

int main(void)
{
    /* Create root */
    struct kobject root;
    kobject_init(&root, "devices", NULL);
    
    /* Create hierarchy */
    struct my_device *pci = my_device_create("pci0000:00", 1, &root);
    struct my_device *eth = my_device_create("eth0", 2, &pci->kobj);
    struct my_device *lo = my_device_create("lo", 3, &pci->kobj);
    
    /* Print tree */
    printf("Object tree:\n");
    print_tree(&root, 0);
    
    /* Cleanup (children first) */
    my_device_destroy(eth);
    my_device_destroy(lo);
    my_device_destroy(pci);
    
    return 0;
}
```

**中文解释：**
- 用户态对象树：模拟内核 kobject
- 引用计数：get 增加、put 减少、为0时调用 release
- 父子关系：add 添加到父的子列表，del 从列表移除
- 层次打印：类似 sysfs 的 ls

