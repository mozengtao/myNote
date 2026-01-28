# Unified Factory Skeleton

A generic C skeleton capturing the Factory pattern.

---

## Complete Skeleton

```c
/*
 * Generic Factory Pattern Skeleton
 * Based on Linux kernel factory functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * PART 1: OBJECT STRUCTURE
 * ================================================================ */

struct my_object {
    char name[32];
    int id;
    int state;
    int refcount;
    void *private;
    
    /* List linkage, etc. */
};

#define STATE_UNINITIALIZED 0
#define STATE_INITIALIZED   1

/* ================================================================
 * PART 2: FACTORY FUNCTION
 * ================================================================ */

/*
 * Factory: Allocate and initialize object
 *
 * @sizeof_priv: Size of private data to allocate
 * @name: Object name
 * @setup: Optional setup callback
 *
 * Returns: Initialized object or NULL on failure
 */
struct my_object *alloc_my_object(int sizeof_priv,
                                   const char *name,
                                   void (*setup)(struct my_object *))
{
    struct my_object *obj;
    size_t alloc_size;
    static int next_id = 1;

    /* 1. Calculate allocation size */
    alloc_size = sizeof(struct my_object) + sizeof_priv;

    /* 2. Allocate memory */
    obj = calloc(1, alloc_size);
    if (!obj)
        return NULL;

    /* 3. Initialize fields */
    strncpy(obj->name, name, sizeof(obj->name) - 1);
    obj->id = next_id++;
    obj->state = STATE_INITIALIZED;
    obj->refcount = 1;
    
    /* Private data follows object */
    if (sizeof_priv > 0)
        obj->private = (char *)obj + sizeof(struct my_object);

    /* 4. Call optional setup callback */
    if (setup)
        setup(obj);

    return obj;
}

/* ================================================================
 * PART 3: DESTRUCTOR
 * ================================================================ */

void free_my_object(struct my_object *obj)
{
    if (!obj)
        return;
    
    obj->refcount--;
    if (obj->refcount == 0) {
        /* Cleanup before free */
        obj->state = STATE_UNINITIALIZED;
        free(obj);
    }
}

/* ================================================================
 * PART 4: ACCESSOR FOR PRIVATE DATA
 * ================================================================ */

void *my_object_priv(struct my_object *obj)
{
    return obj->private;
}

/* ================================================================
 * PART 5: EXAMPLE SETUP CALLBACKS
 * ================================================================ */

void type_a_setup(struct my_object *obj)
{
    printf("  [SETUP] Type A configuration\n");
    /* Type A specific initialization */
}

void type_b_setup(struct my_object *obj)
{
    printf("  [SETUP] Type B configuration\n");
    /* Type B specific initialization */
}

/* ================================================================
 * PART 6: USAGE
 * ================================================================ */

struct my_priv_data {
    int counter;
    char data[64];
};

int main(void)
{
    struct my_object *obj1, *obj2;
    struct my_priv_data *priv;

    printf("=== FACTORY SKELETON ===\n\n");

    /* Create object with private data */
    printf("[CREATE] Object with private data:\n");
    obj1 = alloc_my_object(sizeof(struct my_priv_data),
                           "object1", type_a_setup);
    printf("  Created: id=%d, name=%s\n", obj1->id, obj1->name);

    /* Access private data */
    priv = my_object_priv(obj1);
    priv->counter = 42;

    /* Create object without private data */
    printf("\n[CREATE] Object without private data:\n");
    obj2 = alloc_my_object(0, "object2", type_b_setup);
    printf("  Created: id=%d, name=%s\n", obj2->id, obj2->name);

    /* Cleanup */
    printf("\n[CLEANUP]\n");
    free_my_object(obj1);
    free_my_object(obj2);

    return 0;
}
```

---

## Mapping to Kernel

```
    SKELETON                KERNEL
    ========                ======
    
    alloc_my_object         alloc_netdev
                            alloc_skb
                            alloc_disk
    
    free_my_object          free_netdev
                            kfree_skb
                            put_disk
    
    setup callback          ether_setup
                            loopback_setup
    
    private pointer         netdev_priv()
                            skb->data
```

---

## Key Implementation Points

```
    1. ALLOCATION
       - Calculate total size including private data
       - Use calloc/kzalloc for zero initialization
    
    2. INITIALIZATION
       - Set all required fields
       - Initialize refcount to 1
       - Set state to initialized
    
    3. SETUP CALLBACK
       - Allow type-specific customization
       - Called after basic initialization
    
    4. DESTRUCTOR
       - Decrement refcount
       - Free only when refcount reaches 0
```

---

## Version

Based on **Linux kernel v3.2** factory patterns.
