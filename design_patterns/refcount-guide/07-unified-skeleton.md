# Unified Skeleton: Reference Counting Pattern

## Generic C Skeleton

```c
/*
 * REFERENCE COUNTING PATTERN - UNIFIED SKELETON
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* ==========================================================
 * PART 1: KREF - THE REFERENCE COUNTER
 * ========================================================== */

struct kref {
    atomic_int refcount;
};

/**
 * kref_init - Initialize reference counter to 1
 * 
 * Called when object is created.
 * Creator implicitly has first reference.
 */
static inline void kref_init(struct kref *kref)
{
    atomic_store(&kref->refcount, 1);
}

/**
 * kref_get - Acquire a reference
 * 
 * Caller must already have a valid reference.
 * Increments the counter.
 */
static inline void kref_get(struct kref *kref)
{
    atomic_fetch_add(&kref->refcount, 1);
}

/**
 * kref_put - Release a reference
 * 
 * Decrements counter. If it reaches 0, calls release function.
 * Returns 1 if object was released, 0 otherwise.
 */
static inline int kref_put(struct kref *kref, 
                           void (*release)(struct kref *))
{
    if (atomic_fetch_sub(&kref->refcount, 1) == 1) {
        release(kref);
        return 1;
    }
    return 0;
}


/* ==========================================================
 * PART 2: USER STRUCTURE WITH EMBEDDED KREF
 * ========================================================== */

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * struct my_object - User-defined refcounted object
 *
 * Embeds kref for reference counting.
 */
struct my_object {
    /* User data fields */
    int id;
    char name[32];
    void *private_data;
    
    /* EMBEDDED reference counter */
    struct kref ref;
};

/**
 * my_object_release - Called when refcount hits 0
 *
 * Uses container_of to recover object from kref pointer.
 * Cleans up and frees the object.
 */
static void my_object_release(struct kref *ref)
{
    struct my_object *obj = container_of(ref, struct my_object, ref);
    
    /* Clean up any allocated resources */
    if (obj->private_data)
        free(obj->private_data);
    
    /* Free the object itself */
    free(obj);
}


/* ==========================================================
 * PART 3: WRAPPER FUNCTIONS
 * ========================================================== */

/**
 * my_object_create - Create a new refcounted object
 *
 * Returns object with refcount = 1 (caller has reference).
 */
struct my_object *my_object_create(int id, const char *name)
{
    struct my_object *obj = malloc(sizeof(*obj));
    if (!obj)
        return NULL;
    
    obj->id = id;
    snprintf(obj->name, sizeof(obj->name), "%s", name);
    obj->private_data = NULL;
    
    /* Initialize refcount to 1 */
    kref_init(&obj->ref);
    
    return obj;
}

/**
 * my_object_get - Acquire a reference to object
 *
 * Returns the object for convenience.
 * Caller must already have a reference (or object from trusted source).
 */
struct my_object *my_object_get(struct my_object *obj)
{
    if (obj)
        kref_get(&obj->ref);
    return obj;
}

/**
 * my_object_put - Release a reference to object
 *
 * If this was the last reference, object is freed.
 * After calling, don't access obj anymore!
 */
void my_object_put(struct my_object *obj)
{
    if (obj)
        kref_put(&obj->ref, my_object_release);
}


/* ==========================================================
 * PART 4: USAGE PATTERNS
 * ========================================================== */

/**
 * Pattern 1: Create and use
 */
void pattern_create_use(void)
{
    /* Create object - refcount = 1 */
    struct my_object *obj = my_object_create(1, "example");
    
    /* Use object */
    use_object(obj);
    
    /* Done - release our reference */
    my_object_put(obj);
    /* obj may be freed now - don't touch! */
}

/**
 * Pattern 2: Pass to another user
 */
void pattern_pass_object(void)
{
    struct my_object *obj = my_object_create(2, "shared");
    
    /* Option A: Pass and keep our reference */
    my_object_get(obj);  /* Get reference for other_user */
    pass_to_other_user(obj);  /* They now have a reference */
    /* We still have our reference */
    
    /* Option B: Transfer our reference */
    transfer_to_other_user(obj);  /* They take our reference */
    /* We must NOT use obj anymore - we gave up our reference */
}

/**
 * Pattern 3: Callback with reference
 */
void callback_function(struct kref *ref)
{
    /* Callback receives kref pointer */
    struct my_object *obj = container_of(ref, struct my_object, ref);
    
    /* Use object via recovered pointer */
    process(obj);
}

/**
 * Pattern 4: Lookup with reference
 */
struct my_object *lookup_object(int id)
{
    struct my_object *obj = find_in_hash(id);
    if (obj) {
        /* Get reference before returning */
        my_object_get(obj);
    }
    return obj;
}


/* ==========================================================
 * PART 5: COMPLETE EXAMPLE
 * ========================================================== */

int main(void)
{
    /* Create object (refcount = 1) */
    struct my_object *obj = my_object_create(42, "test");
    
    /* Share with another user */
    my_object_get(obj);  /* refcount = 2 */
    
    /* Original creator done */
    my_object_put(obj);  /* refcount = 1 */
    
    /* Last user done */
    my_object_put(obj);  /* refcount = 0, object freed */
    
    return 0;
}
```

---

## Pattern Mapping

```
+=============================================================================+
|              REFERENCE COUNTING COMPONENTS                                   |
+=============================================================================+

    KERNEL STRUCTURE         |  PATTERN ELEMENT
    =================        |  ===============
    struct kref              |  Reference counter
    kref_init()              |  Initialize to 1
    kref_get()               |  Acquire reference
    kref_put(ref, release)   |  Release reference, call release if 0
    
    USER STRUCTURE           |
    ==============           |
    struct my_object         |  User-defined object
      struct kref ref;       |  Embedded counter
    
    WRAPPER FUNCTIONS        |
    =================        |
    my_object_create()       |  Allocate + kref_init
    my_object_get()          |  kref_get wrapper
    my_object_put()          |  kref_put wrapper
    my_object_release()      |  container_of + cleanup + free


    MAPPING TO KERNEL EXAMPLES:
    ===========================

    Pattern         | Counter   | Get           | Put          | Release
    ----------------+-----------+---------------+--------------+------------
    kobject         | kref      | kobject_get() | kobject_put()| ktype->release
    struct file     | f_count   | get_file()    | fput()       | __fput()
    struct inode    | i_count   | ihold()       | iput()       | destroy_inode()
    struct module   | refcnt    | try_module_get| module_put() | free_module()
```

---

## Rules Summary

```
+=============================================================================+
|              REFERENCE COUNTING RULES                                        |
+=============================================================================+

    1. BALANCED GET/PUT
       Every get must have matching put.
       
    2. CREATOR HAS FIRST REFERENCE
       Object created with refcount = 1.
       Creator must eventually put.
       
    3. NO ACCESS AFTER PUT
       After your put, don't access object.
       
    4. GET BEFORE RETURN
       Lookup functions get before returning.
       Caller must put when done.
       
    5. DOCUMENT OWNERSHIP
       Make clear who owns references.
       "This function takes/returns a reference"
       
    6. RELEASE FREES RESOURCES
       Release callback must clean up everything.
       Use container_of to get object.
```
