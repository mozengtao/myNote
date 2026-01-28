# Identification Rules for Container-of Pattern

## Structural Signals

```
+=============================================================================+
|                    CONTAINER_OF PATTERN ANATOMY                              |
+=============================================================================+

    STRUCTURE DEFINITION:
    =====================

    struct specific_type {
        /* ... specific fields ... */
        
        struct generic_type member;    <-- EMBEDDED (not pointer!)
        
        /* ... more specific fields ... */
    };


    USAGE PATTERN:
    ==============

    struct generic_type *ptr = ...;    // Have pointer to member
    
    struct specific_type *container =
        container_of(ptr, struct specific_type, member);
                     ^^^  ^^^^^^^^^^^^^^^^^^^  ^^^^^^
                     |    |                    |
                     |    |                    +-- Member name in container
                     |    +-- Container type
                     +-- Pointer to the member


    COMMON IN:
    ==========
    
    - list_for_each_entry()    - uses container_of internally
    - hlist_for_each_entry()   - uses container_of internally
    - rb_entry()               - alias for container_of
    - list_entry()             - alias for container_of
```

**中文说明：**

container_of模式的结构特征：(1) 结构体定义中有嵌入的成员（不是指针）；(2) 使用时从成员指针通过`container_of(ptr, type, member)`恢复到容器指针。常见于`list_for_each_entry`、`hlist_for_each_entry`、`rb_entry`、`list_entry`等宏的内部实现。

---

## The Five Identification Rules

### Rule 1: Look for Embedded Structures (Not Pointers)

```c
/* CONTAINER_OF PATTERN: Structure is EMBEDDED */

struct my_device {
    char name[32];
    struct list_head list;     /* <-- EMBEDDED, not a pointer */
    struct kobject kobj;       /* <-- EMBEDDED */
};

/* NOT CONTAINER_OF: Structure is POINTED TO */

struct other_thing {
    char name[32];
    struct list_head *list;    /* <-- POINTER, not embedded */
    struct kobject *kobj;      /* <-- POINTER */
};

/* RULE: If the member is embedded (no *), container_of applies */
```

**中文说明：**

规则1：寻找嵌入的结构体（不是指针）。如果成员是嵌入的（没有*），container_of适用；如果成员是指针，则不是container_of模式。

### Rule 2: Check for Generic Structure Types

```c
/* Common generic types that indicate container_of usage: */

struct list_head { };      /* Linked list node */
struct hlist_node { };     /* Hash list node */
struct rb_node { };        /* Red-black tree node */
struct kobject { };        /* Kernel object base */
struct work_struct { };    /* Work queue item */
struct timer_list { };     /* Timer */
struct kref { };           /* Reference counter */
struct completion { };     /* Completion */

/* When you see these embedded, expect container_of nearby */

struct my_struct {
    struct list_head list;     /* Will use container_of */
    struct work_struct work;   /* Will use container_of */
    struct kref ref;           /* Will use container_of */
};
```

### Rule 3: Look for Iterator Macros

```c
/* These macros USE container_of internally: */

/* List iteration */
list_for_each_entry(pos, head, member)
list_for_each_entry_safe(pos, n, head, member)

/* Hash list iteration */
hlist_for_each_entry(pos, head, member)

/* RB-tree */
rb_entry(ptr, type, member)

/* Example usage reveals container_of pattern: */
struct my_device *dev;
list_for_each_entry(dev, &device_list, list) {
    /* dev is recovered via container_of(ptr, struct my_device, list) */
    printk("Device: %s\n", dev->name);
}
```

### Rule 4: Check Function Signatures for Generic Types

```c
/* Functions that take generic type but need specific type: */

/* This callback receives work_struct pointer */
void my_work_func(struct work_struct *work)
{
    /* Need to get back to my_device */
    struct my_device *dev = container_of(work, struct my_device, work);
    
    /* Now can access dev->name, dev->id, etc. */
}

/* This callback receives list_head pointer */
void process_list_node(struct list_head *node)
{
    struct my_device *dev = container_of(node, struct my_device, list);
}

/* RULE: When function receives generic type but needs specific,
         container_of is the way */
```

**中文说明：**

规则4：检查接收通用类型但需要特定类型的函数。当回调函数接收通用类型指针（如`work_struct*`、`list_head*`）但需要访问特定结构体时，container_of是标准方式。

### Rule 5: Look for Offset Calculation

```c
/* Direct use of offsetof indicates container_of thinking */

/* The container_of macro itself uses offsetof */
#define container_of(ptr, type, member) ({                      \
    (type *)((char *)(ptr) - offsetof(type, member));           \
})

/* Manual offset calculation (same pattern) */
struct my_device *dev = (struct my_device *)
    ((char *)list_ptr - offsetof(struct my_device, list));

/* Some code uses explicit offset for performance */
static const size_t list_offset = offsetof(struct my_device, list);
```

---

## Summary Checklist

```
+=============================================================================+
|                    CONTAINER_OF IDENTIFICATION CHECKLIST                     |
+=============================================================================+

    When examining code, check:

    [ ] 1. EMBEDDED STRUCTURE
        Is there a struct member that is embedded (not a pointer)?
        struct foo { struct list_head list; }  <-- YES
        struct foo { struct list_head *list; } <-- NO

    [ ] 2. GENERIC TYPE EMBEDDED
        Is the embedded type a generic kernel type?
        list_head, hlist_node, rb_node, kobject, work_struct, kref...

    [ ] 3. ITERATOR MACROS
        Are there list_for_each_entry or similar macros?
        These use container_of internally.

    [ ] 4. CALLBACK WITH GENERIC PARAMETER
        Does a callback receive generic type and need specific type?
        void func(struct work_struct *work) { container_of(...) }

    [ ] 5. OFFSET CALCULATION
        Is there offsetof() or manual offset subtraction?

    SCORING:
    2+ indicators = Definitely container_of pattern
    1 indicator   = Likely container_of pattern
    0 indicators  = Not container_of pattern
```

**中文说明：**

识别清单：(1) 是否有嵌入的结构体成员（不是指针）？(2) 嵌入的是否是通用内核类型？(3) 是否使用了迭代宏？(4) 回调函数是否接收通用类型但需要特定类型？(5) 是否有偏移量计算？2个以上指标则必定是container_of模式。

---

## Red Flags: NOT Container-of

```
    NOT CONTAINER_OF:

    1. POINTER TO GENERIC TYPE (not embedded)
       struct foo {
           struct list_head *list;  // Pointer, not embedded
       };

    2. VOID POINTER CASTING
       void *data = node->data;
       struct my_device *dev = (struct my_device *)data;
       // This is NOT container_of - it's unsafe casting

    3. UNION-BASED TYPE PUNNING
       union {
           struct type_a a;
           struct type_b b;
       };
       // This is union, not embedding

    4. SIMPLE FIELD ACCESS
       struct foo { int x; };
       struct foo *f = ...;
       int val = f->x;
       // Normal field access, not container_of
```

---

## Quick Reference: Common Aliases

| Macro | Equivalent to | Used for |
|-------|---------------|----------|
| `list_entry(ptr, type, member)` | `container_of(ptr, type, member)` | Lists |
| `rb_entry(ptr, type, member)` | `container_of(ptr, type, member)` | RB-trees |
| `hlist_entry(ptr, type, member)` | `container_of(ptr, type, member)` | Hash lists |

All these are just aliases for `container_of()`.
