# Source Reading Guide: Container-of Pattern in Linux v3.2

## Key Files

### The container_of Definition

```
include/linux/kernel.h
~~~~~~~~~~~~~~~~~~~~~~
Line ~700: The container_of macro definition

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

EXERCISE: Trace how typeof and offsetof work together.
```

### List Implementation

```
include/linux/list.h
~~~~~~~~~~~~~~~~~~~~
Core linked list implementation.

Key sections to read:
- struct list_head definition (~line 20)
- LIST_HEAD_INIT, LIST_HEAD, INIT_LIST_HEAD macros
- list_add(), list_del() (~line 50-100)
- list_entry() macro - alias for container_of
- list_for_each_entry() macro - uses container_of internally

EXERCISE: Trace list_for_each_entry expansion to see container_of usage.
```

### Hash List Implementation

```
include/linux/list.h (same file)
~~~~~~~~~~~~~~~~~~~~
Lines ~600+: hlist implementation

Key sections:
- struct hlist_head, struct hlist_node definitions
- hlist_entry() macro
- hlist_for_each_entry() macro

EXERCISE: Compare memory size of hlist_head vs list_head.
```

---

## Subsystem Deep Dives

### 1. Process List

```
include/linux/sched.h
~~~~~~~~~~~~~~~~~~~~~
struct task_struct {
    ...
    struct list_head tasks;    /* Line ~1200 */
    ...
};

kernel/sched.c
~~~~~~~~~~~~~~
- for_each_process() macro uses list_for_each_entry
- init_task.tasks is the head of all tasks list

EXERCISE:
1. Find for_each_process macro definition
2. Trace what container_of it expands to
3. Find where tasks are added/removed from list
```

### 2. File System Inodes

```
include/linux/fs.h
~~~~~~~~~~~~~~~~~~
struct inode {
    struct hlist_node i_hash;     /* For inode hash table */
    struct list_head i_list;      /* For superblock inode list */
    struct list_head i_sb_list;   /* For global inode list */
    ...
};

fs/inode.c
~~~~~~~~~~
- inode_hashtable: The global inode hash table
- find_inode_fast(): Uses hlist_for_each_entry

EXERCISE:
1. Trace inode lookup in hash table
2. Find where container_of (via hlist_entry) recovers inode
3. Note how one inode is on multiple lists simultaneously
```

### 3. Network Devices

```
include/linux/netdevice.h
~~~~~~~~~~~~~~~~~~~~~~~~~
struct net_device {
    struct list_head dev_list;     /* Global device list */
    struct hlist_node name_hlist;  /* Device name hash */
    struct hlist_node index_hlist; /* Device index hash */
    ...
    struct device dev;             /* Embedded device (contains kobject) */
    ...
};

net/core/dev.c
~~~~~~~~~~~~~~
- dev_base_head: Global list of all network devices
- first_net_device(): Uses list_entry
- dev_get_by_name(): Uses hash table lookup

EXERCISE:
1. Trace dev_get_by_name() to see hlist_entry usage
2. Note how net_device is on multiple lists/hashes
3. Find container_of chain: kobject -> device -> net_device
```

### 4. Kobject and Device Model

```
include/linux/kobject.h
~~~~~~~~~~~~~~~~~~~~~~~
struct kobject {
    const char *name;
    struct list_head entry;        /* On kset's list */
    struct kobject *parent;
    struct kset *kset;
    struct kref kref;              /* Embedded reference count */
    ...
};

include/linux/device.h
~~~~~~~~~~~~~~~~~~~~~~
struct device {
    struct kobject kobj;           /* EMBEDDED kobject */
    ...
};

#define to_dev(kobj) container_of(kobj, struct device, kobj)

drivers/base/core.c
~~~~~~~~~~~~~~~~~~~
- device_add(): Adds device to hierarchy
- Sysfs attribute callbacks use to_dev() to recover device

EXERCISE:
1. Trace to_dev() macro
2. Find where sysfs callbacks receive kobject and recover device
3. Look for multi-level container_of chains
```

### 5. Workqueue

```
include/linux/workqueue.h
~~~~~~~~~~~~~~~~~~~~~~~~~
struct work_struct {
    atomic_long_t data;
    struct list_head entry;
    work_func_t func;
};

DECLARE_WORK(n, f)
INIT_WORK(_work, _func)

kernel/workqueue.c
~~~~~~~~~~~~~~~~~~
- Worker thread retrieves work_struct from queue
- Work handler uses container_of to get context

EXERCISE:
1. Find a driver that uses INIT_WORK
2. Trace the work handler function
3. Identify container_of usage in handler
```

---

## Reading Strategy

### Step 1: Understand the Macro

```bash
# In Linux source root:
grep -n "define container_of" include/linux/kernel.h
grep -n "define list_entry" include/linux/list.h
grep -n "define hlist_entry" include/linux/list.h
```

### Step 2: Find Usage Sites

```bash
# Find structures with embedded list_head:
grep -r "struct list_head" include/linux/*.h | head -50

# Find container_of usage:
grep -rn "container_of" kernel/ | head -50

# Find list_entry usage:
grep -rn "list_entry" fs/ | head -50
```

### Step 3: Trace a Complete Path

```
TRACE: Process iteration

1. include/linux/sched.h
   - Find struct task_struct
   - Locate the 'tasks' list_head member

2. include/linux/sched.h
   - Find for_each_process macro
   - Note it uses list_for_each_entry

3. include/linux/list.h
   - Expand list_for_each_entry
   - See container_of (via list_entry)

4. kernel/sched.c or any scheduler code
   - Find for_each_process usage
   - See how tasks are enumerated
```

---

## Key Patterns to Look For

```
+=============================================================================+
|              PATTERNS IN KERNEL CODE                                         |
+=============================================================================+

    PATTERN 1: RECOVERY MACRO
    ~~~~~~~~~~~~~~~~~~~~~~~~~
    #define to_xxx(ptr) container_of(ptr, struct xxx, member)
    
    Find: to_dev, to_net_dev, list_entry, hlist_entry, rb_entry
    

    PATTERN 2: ITERATION WITH RECOVERY
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    list_for_each_entry(pos, head, member)
    hlist_for_each_entry(pos, head, member)
    
    'pos' is the container type, container_of happens internally.
    

    PATTERN 3: CALLBACK RECOVERY
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void callback(struct generic_type *ptr)
    {
        struct specific_type *obj = container_of(ptr, ...);
        // use obj
    }
    
    Find in: timer handlers, work handlers, sysfs show/store
    

    PATTERN 4: MULTI-LIST MEMBERSHIP
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    struct xxx {
        struct list_head list_a;  // On one list
        struct list_head list_b;  // On another list
        struct hlist_node hash;   // In hash table
    };
    
    Same object on multiple data structures.
```

---

## Exercises

### Exercise 1: Timer Callback

```
Find: include/linux/timer.h
      kernel/timer.c

1. Locate struct timer_list definition
2. Find DEFINE_TIMER or setup_timer usage
3. Find a driver that uses timers
4. Trace how timer callback recovers device context
```

### Exercise 2: Block Device

```
Find: include/linux/genhd.h
      block/genhd.c

1. Locate struct gendisk definition
2. Find embedded kobject
3. Trace disk_to_dev() and container_of chain
```

### Exercise 3: Network Socket

```
Find: include/net/sock.h
      net/core/sock.c

1. Locate struct sock definition
2. Find various embedded structures (timers, lists)
3. Trace how callbacks recover socket context
```

---

## Summary: Files to Read

| File | Content |
|------|---------|
| `include/linux/kernel.h` | container_of definition |
| `include/linux/list.h` | list_head, hlist, iteration macros |
| `include/linux/sched.h` | task_struct with list_head |
| `include/linux/fs.h` | inode with multiple list memberships |
| `include/linux/netdevice.h` | net_device with embedded device |
| `include/linux/kobject.h` | kobject base class |
| `include/linux/device.h` | device with embedded kobject |
| `include/linux/workqueue.h` | work_struct |
