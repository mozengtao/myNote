# Case 1: list_head and Linked Lists

## Subsystem Background

```
+=============================================================================+
|                    LINUX KERNEL LINKED LIST                                  |
+=============================================================================+

    THE KERNEL'S UNIVERSAL LINKED LIST:
    ===================================

    +------------------------------------------------------------------+
    |                        list_head                                  |
    |                                                                   |
    |   struct list_head {                                              |
    |       struct list_head *next;                                     |
    |       struct list_head *prev;                                     |
    |   };                                                              |
    |                                                                   |
    |   - Doubly linked                                                 |
    |   - Circular (head.prev points to tail, tail.next to head)        |
    |   - Intrusive (embedded in user structures)                       |
    |   - Used EVERYWHERE in kernel                                     |
    |                                                                   |
    +------------------------------------------------------------------+

    CIRCULAR STRUCTURE:
    
          +------+     +------+     +------+     +------+
    +---->| head |<--->| node |<--->| node |<--->| node |<----+
    |     +------+     +------+     +------+     +------+     |
    |                                                         |
    +---------------------------------------------------------+
```

**中文说明：**

Linux内核的通用链表：`list_head`是一个双向循环链表，只包含`next`和`prev`两个指针。它是侵入式的（嵌入在用户结构体中），在内核中到处使用。循环结构意味着头的prev指向尾，尾的next指向头。

---

## How container_of Enables Generic Lists

```
    WITHOUT container_of (Bad):           WITH container_of (Kernel Way):
    ===========================           =============================

    struct node {                         struct list_head {
        void *data;  /* Type lost! */         struct list_head *next, *prev;
        struct node *next, *prev;         };
    };
                                          struct task_struct {
    /* Must cast everywhere */                pid_t pid;
    struct task *t = (struct task *)          struct list_head tasks;  // EMBED
        node->data;                       };

    /* Type errors at runtime */          /* Type safe recovery */
                                          struct task_struct *t = 
                                              container_of(ptr, 
                                                  struct task_struct, tasks);


    Memory Layout:

    +--------------------+
    | struct task_struct |
    |  pid               |     offset 0
    |  tasks.next  ------+---> points to another tasks member
    |  tasks.prev  ------+---> points to another tasks member
    |  other_field       |
    +--------------------+
    
    The list only links tasks members together.
    container_of recovers the full task_struct.
```

**中文说明：**

对比：没有container_of时使用void指针会丢失类型信息，到处需要强制转换，类型错误只能在运行时发现。使用container_of时，list_head嵌入在用户结构体中，通过container_of可以类型安全地恢复完整结构体。链表只连接嵌入的list_head成员，container_of恢复外层结构体。

---

## Minimal C Code Simulation

```c
/*
 * CONTAINER_OF WITH LIST_HEAD SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* ==========================================================
 * KERNEL-STYLE DEFINITIONS
 * ========================================================== */

/* The container_of macro */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* Doubly linked list head */
struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

/* Initialize a list head */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/* Add new entry after head */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    struct list_head *next = head->next;
    next->prev = new;
    new->next = next;
    new->prev = head;
    head->next = new;
}

/* Add new entry at the end (before head in circular list) */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    struct list_head *prev = head->prev;
    prev->next = new;
    new->prev = prev;
    new->next = head;
    head->prev = new;
}

/* Delete entry from list */
static inline void list_del(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

/* Check if list is empty */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/* Get the container structure - THIS IS THE KEY */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/* Iterate over list */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* Iterate over list, getting container structure */
#define list_for_each_entry(pos, head, member)                          \
    for (pos = list_entry((head)->next, typeof(*pos), member);          \
         &pos->member != (head);                                        \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* ==========================================================
 * USER STRUCTURE: Example Device
 * ========================================================== */

struct my_device {
    int id;
    char name[32];
    struct list_head list;    /* EMBEDDED list node */
    int status;
};

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

/* Global list of devices */
static LIST_HEAD(device_list);

/* Create and add a device */
struct my_device *create_device(int id, const char *name)
{
    struct my_device *dev = malloc(sizeof(*dev));
    if (!dev) return NULL;
    
    dev->id = id;
    snprintf(dev->name, sizeof(dev->name), "%s", name);
    dev->status = 1;
    INIT_LIST_HEAD(&dev->list);
    
    /* Add to global list using embedded list_head */
    list_add_tail(&dev->list, &device_list);
    
    printf("[CREATE] Device %d: %s (at %p, list at %p)\n", 
           id, name, (void *)dev, (void *)&dev->list);
    
    return dev;
}

/* Find device by name - demonstrates container_of */
struct my_device *find_device(const char *name)
{
    struct list_head *pos;
    
    printf("[SEARCH] Looking for '%s'...\n", name);
    
    /* Iterate through list_head pointers */
    list_for_each(pos, &device_list) {
        /* 
         * pos points to the 'list' member inside some my_device.
         * Use container_of to get the my_device pointer.
         */
        struct my_device *dev = container_of(pos, struct my_device, list);
        
        printf("  [CHECK] At list_head %p -> device '%s' at %p\n",
               (void *)pos, dev->name, (void *)dev);
        
        if (strcmp(dev->name, name) == 0) {
            printf("  [FOUND] Device '%s'\n", name);
            return dev;
        }
    }
    
    printf("  [NOT FOUND]\n");
    return NULL;
}

/* Print all devices - using list_for_each_entry */
void print_all_devices(void)
{
    struct my_device *dev;
    
    printf("\n[ALL DEVICES]\n");
    
    /* list_for_each_entry uses container_of internally */
    list_for_each_entry(dev, &device_list, list) {
        printf("  Device %d: %s (status=%d)\n", 
               dev->id, dev->name, dev->status);
    }
    printf("\n");
}

/* Remove a device */
void remove_device(struct my_device *dev)
{
    printf("[REMOVE] Device %d: %s\n", dev->id, dev->name);
    list_del(&dev->list);
    free(dev);
}

int main(void)
{
    printf("=================================================\n");
    printf("CONTAINER_OF WITH LIST_HEAD DEMONSTRATION\n");
    printf("=================================================\n\n");

    /* Create several devices */
    printf("--- Creating devices ---\n");
    struct my_device *dev1 = create_device(1, "eth0");
    struct my_device *dev2 = create_device(2, "eth1");
    struct my_device *dev3 = create_device(3, "lo");
    
    /* Show the list */
    print_all_devices();
    
    /* Find a device (demonstrates container_of) */
    printf("--- Finding device ---\n");
    struct my_device *found = find_device("eth1");
    if (found) {
        printf("Found device with id=%d, status=%d\n", 
               found->id, found->status);
    }
    
    printf("\n--- Demonstrating container_of calculation ---\n");
    printf("dev1 address:      %p\n", (void *)dev1);
    printf("dev1->list address: %p\n", (void *)&dev1->list);
    printf("offsetof(struct my_device, list) = %zu\n", 
           offsetof(struct my_device, list));
    printf("container_of(&dev1->list) = %p - %zu = %p\n",
           (void *)&dev1->list,
           offsetof(struct my_device, list),
           (void *)container_of(&dev1->list, struct my_device, list));
    
    /* Cleanup */
    printf("\n--- Removing devices ---\n");
    remove_device(dev1);
    remove_device(dev2);
    remove_device(dev3);
    
    printf("\n--- Final state ---\n");
    print_all_devices();
    
    printf("=================================================\n");
    printf("KEY INSIGHT:\n");
    printf("- list_head contains NO user data\n");
    printf("- list_head is EMBEDDED in user structure\n");
    printf("- container_of RECOVERS user structure from list_head\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## What container_of Enables Here

```
+=============================================================================+
|              WHAT container_of ENABLES FOR LINKED LISTS                      |
+=============================================================================+

    1. SINGLE LIST IMPLEMENTATION FOR ALL TYPES
       ========================================
       
       The kernel has ONE list_add(), ONE list_del().
       Works for task_struct, inode, net_device, everything.
       container_of recovers the specific type.

    2. TYPE SAFETY
       ===========
       
       list_for_each_entry(dev, &device_list, list)
       
       'dev' is typed as struct my_device*.
       Compiler checks all accesses to dev->field.

    3. ZERO RUNTIME TYPE INFORMATION
       =============================
       
       No need for type tags or runtime checks.
       container_of is pure compile-time calculation.

    4. MULTIPLE LIST MEMBERSHIP
       ========================
       
       struct my_device {
           struct list_head by_name;    /* On name-sorted list */
           struct list_head by_id;      /* On id-sorted list */
           struct list_head by_status;  /* On status list */
       };
       
       Same device on THREE different lists simultaneously!
       Each list_head recovers the same my_device.
```

**中文说明：**

container_of为链表实现了什么：(1) 单一链表实现适用于所有类型——内核只有一个`list_add`、一个`list_del`；(2) 类型安全——编译器检查所有字段访问；(3) 零运行时类型信息——无需类型标签；(4) 多链表成员——同一结构体可以通过多个list_head同时在多个链表中。

---

## Real Kernel Examples

### Task List (kernel/sched.c)

```c
/* All tasks are on a doubly linked list */
struct task_struct {
    /* ... many fields ... */
    struct list_head tasks;  /* Links all tasks */
    /* ... many fields ... */
};

/* Iterate all tasks */
#define for_each_process(p) \
    list_for_each_entry(p, &init_task.tasks, tasks)
```

### Inode List (fs/inode.c)

```c
struct inode {
    struct list_head i_list;     /* On superblock's inode list */
    struct list_head i_sb_list;  /* On global inode list */
    /* ... */
};
```

### Network Devices (include/linux/netdevice.h)

```c
struct net_device {
    struct list_head dev_list;   /* Global device list */
    struct list_head napi_list;  /* NAPI instances */
    /* ... */
};
```

---

## Key Takeaways

1. **list_head is generic**: Contains only next/prev pointers
2. **Embedding is key**: list_head goes inside your structure
3. **container_of recovers context**: From list_head back to your structure
4. **list_for_each_entry hides it**: Uses container_of internally
5. **Multiple memberships**: One struct can be on multiple lists
