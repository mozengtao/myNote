# Unified Skeleton: Container-of Pattern

## Generic C Skeleton

```c
/*
 * CONTAINER_OF PATTERN - UNIFIED SKELETON
 * 
 * This skeleton shows the fundamental structure that can be
 * applied to any use case: lists, hash tables, device models, etc.
 */

#include <stddef.h>

/* ==========================================================
 * PART 1: THE CONTAINER_OF MACRO
 * ========================================================== */

/**
 * container_of - Cast a member of a structure out to the containing structure
 * @ptr:    pointer to the member
 * @type:   type of the container struct
 * @member: name of the member within the container struct
 *
 * Return: pointer to the containing structure
 */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* Alternative: without typeof (for non-GNU compilers) */
#define container_of_simple(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))


/* ==========================================================
 * PART 2: GENERIC STRUCTURE (What Gets Embedded)
 * ========================================================== */

/**
 * struct generic_node - A generic structure to be embedded
 *
 * Examples in kernel:
 *   - struct list_head
 *   - struct hlist_node
 *   - struct rb_node
 *   - struct kobject
 *   - struct work_struct
 *   - struct kref
 */
struct generic_node {
    /* Linkage or infrastructure fields */
    struct generic_node *next;
    struct generic_node *prev;
    /* Possibly more fields */
};

/* Operations that work on generic_node */
void generic_add(struct generic_node *new, struct generic_node *head);
void generic_del(struct generic_node *node);


/* ==========================================================
 * PART 3: CONTAINER STRUCTURE (What Embeds the Generic)
 * ========================================================== */

/**
 * struct container_object - User's specific structure
 *
 * Contains the generic_node as a member, NOT a pointer.
 * This is the "intrusive" pattern.
 */
struct container_object {
    /* User-specific fields */
    int id;
    char name[32];
    void *private_data;
    
    /* EMBEDDED generic structure - NOT A POINTER */
    struct generic_node node;
    
    /* More user-specific fields */
    int status;
};

/**
 * to_container - Recover container from embedded node
 * @ptr: pointer to the embedded generic_node
 *
 * This is often defined as a convenient macro/inline.
 */
static inline struct container_object *
to_container(struct generic_node *ptr)
{
    return container_of(ptr, struct container_object, node);
}


/* ==========================================================
 * PART 4: CALLBACK PATTERN
 * ========================================================== */

/**
 * Callbacks receive generic type, recover specific type
 * via container_of.
 */

/* Function type that receives generic pointer */
typedef void (*node_callback_t)(struct generic_node *node);

/* Implementation recovers container */
void my_callback(struct generic_node *node)
{
    /* Recover the container structure */
    struct container_object *obj = to_container(node);
    
    /* Now can access obj->id, obj->name, obj->status */
    process_object(obj);
}


/* ==========================================================
 * PART 5: ITERATION PATTERN
 * ========================================================== */

/**
 * Iterate over nodes, recovering container for each
 */
#define for_each_container(pos, head, member)                   \
    for (pos = container_of((head)->next, typeof(*pos), member);\
         &pos->member != (head);                                \
         pos = container_of(pos->member.next, typeof(*pos), member))

/* Usage: */
void process_all(struct generic_node *head)
{
    struct container_object *obj;
    
    for_each_container(obj, head, node) {
        /* obj is automatically recovered via container_of */
        printf("Object: %s (id=%d)\n", obj->name, obj->id);
    }
}


/* ==========================================================
 * PART 6: MULTIPLE EMBEDDING
 * ========================================================== */

/**
 * A structure can embed multiple generic nodes
 * for membership in multiple collections
 */
struct multi_member {
    int id;
    
    /* On list sorted by ID */
    struct generic_node by_id;
    
    /* On list sorted by name */
    struct generic_node by_name;
    
    /* In hash table */
    struct generic_node hash;
    
    char name[32];
};

/* Different recovery macros for each membership */
#define to_multi_by_id(ptr) \
    container_of(ptr, struct multi_member, by_id)

#define to_multi_by_name(ptr) \
    container_of(ptr, struct multi_member, by_name)

#define to_multi_by_hash(ptr) \
    container_of(ptr, struct multi_member, hash)


/* ==========================================================
 * PART 7: COMPLETE EXAMPLE
 * ========================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* List head definition */
struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_add_tail(struct list_head *new, 
                                  struct list_head *head)
{
    struct list_head *prev = head->prev;
    prev->next = new;
    new->prev = prev;
    new->next = head;
    head->prev = new;
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_for_each_entry(pos, head, member)                          \
    for (pos = list_entry((head)->next, typeof(*pos), member);          \
         &pos->member != (head);                                        \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* User structure */
struct my_object {
    int value;
    char label[16];
    struct list_head link;  /* EMBEDDED */
};

/* Global list */
static LIST_HEAD(object_list);

/* Create and add object */
struct my_object *create_object(int value, const char *label)
{
    struct my_object *obj = malloc(sizeof(*obj));
    if (!obj) return NULL;
    
    obj->value = value;
    strncpy(obj->label, label, sizeof(obj->label) - 1);
    INIT_LIST_HEAD(&obj->link);
    
    list_add_tail(&obj->link, &object_list);
    return obj;
}

/* Process callback - demonstrates container_of */
void process_node(struct list_head *node)
{
    struct my_object *obj = container_of(node, struct my_object, link);
    printf("Processing: %s = %d\n", obj->label, obj->value);
}

/* Print all - demonstrates iteration with container_of */
void print_all(void)
{
    struct my_object *obj;
    list_for_each_entry(obj, &object_list, link) {
        printf("  %s: %d\n", obj->label, obj->value);
    }
}

int main(void)
{
    create_object(10, "alpha");
    create_object(20, "beta");
    create_object(30, "gamma");
    
    printf("All objects:\n");
    print_all();
    
    return 0;
}
```

---

## Pattern Mapping

```
+=============================================================================+
|              CONTAINER_OF PATTERN COMPONENTS                                 |
+=============================================================================+

    GENERIC STRUCTURE          |  KERNEL EXAMPLES
    =================          |  ===============
    struct generic_node        |  list_head, hlist_node, rb_node,
                               |  kobject, work_struct, kref
                               |
    CONTAINER STRUCTURE        |  
    ===================        |  
    struct container_object    |  task_struct, inode, net_device,
    {                          |  sk_buff, file, dentry
        struct generic_node;   |
    }                          |
                               |
    RECOVERY MACRO             |
    ==============             |
    to_container()             |  list_entry(), rb_entry(), to_dev(),
                               |  to_net_dev(), container_of()
                               |
    ITERATION MACRO            |
    ===============            |
    for_each_container()       |  list_for_each_entry(),
                               |  hlist_for_each_entry(),
                               |  for_each_process()
```

---

## Key Structural Elements

| Element | Purpose | Example |
|---------|---------|---------|
| **Generic struct** | Linkage/infrastructure | `struct list_head` |
| **Container struct** | User data + embedded generic | `struct task_struct` |
| **container_of** | Recover container from member | `container_of(ptr, type, member)` |
| **to_X macro** | Convenient type-specific recovery | `to_dev(kobj)` |
| **for_each macro** | Iterate with automatic recovery | `list_for_each_entry()` |
