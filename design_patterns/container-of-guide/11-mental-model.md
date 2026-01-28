# Final Mental Model: Container-of Pattern

## One-Paragraph Summary

The container_of pattern is C's equivalent of "upcasting" in object-oriented programming. When you embed a generic structure (like `list_head`, `work_struct`, or `kobject`) inside a specific structure, you can navigate from a pointer to the generic member back to the containing structure using simple pointer arithmetic: `container = member_ptr - offset_of_member`. This enables generic algorithms to work with any user-defined type while maintaining type safety and zero runtime overhead. The pattern is foundational to all Linux kernel data structures and underlies the kernel's approach to polymorphism and code reuse.

**中文总结：**

container_of模式是C语言中"向上转型"的等价物。当你将通用结构体（如list_head、work_struct、kobject）嵌入到特定结构体中时，可以通过简单的指针算术从通用成员的指针导航回包含它的结构体：`容器 = 成员指针 - 成员偏移量`。这使得通用算法能够与任何用户定义的类型配合工作，同时保持类型安全和零运行时开销。此模式是所有Linux内核数据结构的基础，也是内核实现多态和代码复用的核心方式。

---

## Decision Flowchart

```
+=============================================================================+
|              CONTAINER_OF DECISION FLOWCHART                                 |
+=============================================================================+

    START: Do I need container_of?
    ==============================

                    +-------------------+
                    | Need generic data |
                    | structure (list,  |
                    | hash, tree)?      |
                    +--------+----------+
                             |
              +--------------+--------------+
              |                             |
              v                             v
            [YES]                         [NO]
              |                             |
              v                             v
    +-------------------+          Just use normal
    | Will callbacks/   |          struct fields
    | iterators receive |
    | generic type ptr? |
    +--------+----------+
             |
    +--------+---------+
    |                  |
    v                  v
  [YES]              [NO]
    |                  |
    v                  v
EMBED the         Consider if
generic type      embedding still
in your struct    helps (code reuse)
    |
    v
    +-------------------------+
    | Use container_of in:    |
    | - Callbacks             |
    | - Custom iteration      |
    | - Type recovery         |
    +-------------------------+


    CHOOSING WHAT TO EMBED:
    =======================

    Need...                      Embed...
    -------                      --------
    Linked list membership  -->  struct list_head
    Hash table membership   -->  struct hlist_node
    Red-black tree node     -->  struct rb_node
    Reference counting      -->  struct kref
    Deferred work           -->  struct work_struct
    Timer                   -->  struct timer_list
    Device model            -->  struct device / struct kobject
    Completion              -->  struct completion
```

---

## The Three Rules

```
+=============================================================================+
|              THREE RULES OF CONTAINER_OF                                     |
+=============================================================================+

    RULE 1: EMBED, DON'T POINT
    ==========================
    
        WRONG:                          RIGHT:
        struct x {                      struct x {
            struct list_head *list;         struct list_head list;
        };                              };
        
        Pointer: container_of fails     Embedded: container_of works
    

    RULE 2: MATCH NAMES EXACTLY
    ===========================
    
        struct x {
            struct list_head alpha;
            struct list_head beta;
        };
        
        If ptr came from &obj->alpha:
            container_of(ptr, struct x, alpha)  <-- CORRECT
            container_of(ptr, struct x, beta)   <-- WRONG!
    

    RULE 3: LIFETIME MUST MATCH
    ===========================
    
        Container must live as long as generic member is in use.
        
        WRONG: Stack object added to persistent list
        RIGHT: Heap object, or remove before stack returns
```

---

## Visual Memory Model

```
+=============================================================================+
|              CONTAINER_OF MEMORY MODEL                                       |
+=============================================================================+

    STRUCTURE IN MEMORY:

    Address    Content
    -------    -------
    0x1000     +---------------------------+
               | struct my_object          |
    0x1000     |   int id;          (4B)   |
    0x1004     |   char name[28];   (28B)  |
    0x1020     |   struct list_head list;  |  <-- member_ptr points here
               |     .next          (8B)   |
               |     .prev          (8B)   |
    0x1030     |   int status;      (4B)   |
               +---------------------------+


    CONTAINER_OF CALCULATION:

    Given: member_ptr = 0x1020 (points to 'list' member)
    
    container_of(member_ptr, struct my_object, list)
    
    Step 1: offsetof(struct my_object, list) = 0x20 (32 bytes)
    
    Step 2: (char*)member_ptr - offset = 0x1020 - 0x20 = 0x1000
    
    Step 3: Cast to (struct my_object*) = 0x1000
    
    Result: Pointer to the whole my_object!


    ASCII VISUALIZATION:

    member_ptr                      container_of result
        |                                   |
        v                                   v
        +-----------------------------------+
        |  id  |       name       |  list  |  status  |
        +-----------------------------------+
        0x1000                     0x1020
                                     ^
                        offset = 0x20 = 32
```

**中文说明：**

container_of的内存模型：给定指向成员的指针（如0x1020），通过减去成员在结构体中的偏移量（如32字节），得到结构体的起始地址（0x1000）。这是编译时计算的常量偏移，运行时只需一次减法。

---

## Quick Reference Card

```
+=============================================================================+
|              CONTAINER_OF QUICK REFERENCE                                    |
+=============================================================================+

    MACRO:
    ------
    container_of(ptr, type, member)
        ptr    = pointer to the embedded member
        type   = container structure type
        member = name of the member in container
    

    COMMON ALIASES:
    ---------------
    list_entry(ptr, type, member)   = container_of(...)
    hlist_entry(ptr, type, member)  = container_of(...)
    rb_entry(ptr, type, member)     = container_of(...)
    

    ITERATOR MACROS (use container_of internally):
    -----------------------------------------------
    list_for_each_entry(pos, head, member)
    hlist_for_each_entry(pos, head, member)
    

    COMMON EMBEDDED TYPES:
    ----------------------
    struct list_head     - doubly linked list
    struct hlist_node    - hash list (space efficient)
    struct rb_node       - red-black tree
    struct kobject       - kernel object base
    struct kref          - reference counter
    struct work_struct   - deferred work
    struct timer_list    - kernel timer
    

    TYPICAL PATTERN:
    ----------------
    struct my_object {
        /* ... your fields ... */
        struct list_head link;    /* embedded */
        /* ... more fields ... */
    };

    /* In callback or iteration: */
    struct list_head *ptr = ...;
    struct my_object *obj = container_of(ptr, struct my_object, link);
```

---

## Common Use Cases Summary

| Use Case | Embedded Type | container_of Usage |
|----------|---------------|-------------------|
| Linked list | `list_head` | `list_entry()` / `list_for_each_entry()` |
| Hash table | `hlist_node` | `hlist_entry()` / `hlist_for_each_entry()` |
| Device model | `kobject` / `device` | `to_dev()` / `to_xxx()` |
| Workqueue | `work_struct` | In work handler callback |
| Timers | `timer_list` | In timer callback |
| Reference counting | `kref` | In release callback |
| Red-black trees | `rb_node` | `rb_entry()` |

---

## Final Checklist

Before using container_of:

- [ ] Is the member EMBEDDED (not a pointer)?
- [ ] Does the member name in container_of match the actual member?
- [ ] Is the container type correct?
- [ ] Is the pointer non-NULL?
- [ ] Does the container outlive the member's usage?
- [ ] Am I using the original member (not a copy)?

If all yes, container_of will work correctly.
