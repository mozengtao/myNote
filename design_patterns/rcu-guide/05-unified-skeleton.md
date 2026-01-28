# Unified RCU Skeleton

A generic C skeleton capturing RCU patterns as used in the Linux kernel.

---

## Complete Skeleton

```c
/*
 * Generic RCU Pattern Skeleton
 * Based on Linux kernel RCU implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * PART 1: DATA STRUCTURE
 * ================================================================ */

struct my_data {
    int key;
    int value;
    struct my_data *next;
    /* In kernel: struct rcu_head rcu; for call_rcu */
};

/* Global RCU-protected pointer */
static struct my_data *global_list = NULL;

/* ================================================================
 * PART 2: SIMULATED RCU PRIMITIVES
 * ================================================================ */

/* In kernel: these are real RCU operations */
#define rcu_read_lock()     do { } while(0)
#define rcu_read_unlock()   do { } while(0)
#define rcu_dereference(p)  (p)
#define rcu_assign_pointer(p, v) do { (p) = (v); } while(0)
#define synchronize_rcu()   do { } while(0)

/* ================================================================
 * PART 3: READER (LOCK-FREE)
 * ================================================================ */

/*
 * RCU-protected lookup - NO LOCKS
 * Can be called concurrently by many readers
 */
struct my_data *lookup_rcu(int key)
{
    struct my_data *p;
    
    rcu_read_lock();
    
    /* Walk list - RCU protects against concurrent update */
    for (p = rcu_dereference(global_list); p != NULL;
         p = rcu_dereference(p->next)) {
        if (p->key == key) {
            rcu_read_unlock();
            return p;
        }
    }
    
    rcu_read_unlock();
    return NULL;
}

/*
 * Iterate all entries (reader)
 */
void iterate_rcu(void (*callback)(struct my_data *))
{
    struct my_data *p;
    
    rcu_read_lock();
    
    for (p = rcu_dereference(global_list); p != NULL;
         p = rcu_dereference(p->next)) {
        callback(p);
    }
    
    rcu_read_unlock();
}

/* ================================================================
 * PART 4: WRITER (COPY-UPDATE)
 * ================================================================ */

/*
 * Add entry (writer)
 * In kernel: typically protected by separate mutex
 */
void add_entry(int key, int value)
{
    struct my_data *new;
    
    /* Allocate new entry */
    new = malloc(sizeof(*new));
    new->key = key;
    new->value = value;
    
    /* Publish: add to front of list */
    new->next = global_list;
    rcu_assign_pointer(global_list, new);
    
    /* No synchronize_rcu needed for add - readers see old or new */
}

/*
 * Update entry (writer) - copy-on-write
 */
int update_entry(int key, int new_value)
{
    struct my_data *old, *new, **pp;
    
    /* Find entry to update */
    for (pp = &global_list; *pp != NULL; pp = &(*pp)->next) {
        if ((*pp)->key == key) {
            old = *pp;
            
            /* Create copy with new value */
            new = malloc(sizeof(*new));
            memcpy(new, old, sizeof(*new));
            new->value = new_value;
            
            /* Atomic pointer swap */
            rcu_assign_pointer(*pp, new);
            
            /* Wait for all readers to finish with old */
            synchronize_rcu();
            
            /* Now safe to free old */
            free(old);
            return 0;
        }
    }
    return -1;  /* Not found */
}

/*
 * Delete entry (writer)
 */
int delete_entry(int key)
{
    struct my_data *old, **pp;
    
    /* Find and unlink entry */
    for (pp = &global_list; *pp != NULL; pp = &(*pp)->next) {
        if ((*pp)->key == key) {
            old = *pp;
            
            /* Unlink from list */
            rcu_assign_pointer(*pp, old->next);
            
            /* Wait for readers */
            synchronize_rcu();
            
            /* Safe to free */
            free(old);
            return 0;
        }
    }
    return -1;
}

/* ================================================================
 * PART 5: EXAMPLE USAGE
 * ================================================================ */

void print_entry(struct my_data *d)
{
    printf("  key=%d value=%d\n", d->key, d->value);
}

int main(void)
{
    struct my_data *found;
    
    printf("=== RCU SKELETON DEMO ===\n\n");
    
    /* Add entries */
    printf("Adding entries:\n");
    add_entry(1, 100);
    add_entry(2, 200);
    add_entry(3, 300);
    
    /* Read (concurrent readers safe) */
    printf("\nIterating:\n");
    iterate_rcu(print_entry);
    
    /* Lookup */
    printf("\nLookup key=2:\n");
    found = lookup_rcu(2);
    if (found)
        printf("  Found: value=%d\n", found->value);
    
    /* Update */
    printf("\nUpdating key=2 to value=999:\n");
    update_entry(2, 999);
    
    printf("\nAfter update:\n");
    iterate_rcu(print_entry);
    
    /* Delete */
    printf("\nDeleting key=1:\n");
    delete_entry(1);
    
    printf("\nAfter delete:\n");
    iterate_rcu(print_entry);
    
    return 0;
}
```

---

## Mapping to Kernel

```
    SKELETON                KERNEL
    ========                ======
    
    rcu_read_lock()         rcu_read_lock()
    rcu_dereference(p)      rcu_dereference(p)
    rcu_assign_pointer()    rcu_assign_pointer()
    synchronize_rcu()       synchronize_rcu()
    free()                  kfree_rcu() or call_rcu()
```

---

## Key Implementation Points

```
    1. READERS
       - rcu_read_lock/unlock bracket access
       - Use rcu_dereference for pointers
       - No sleeping in critical section
    
    2. WRITERS
       - Copy data before modifying
       - Use rcu_assign_pointer to publish
       - Wait (synchronize_rcu) before free
    
    3. MEMORY ORDERING
       - rcu_dereference ensures load ordering
       - rcu_assign_pointer ensures store ordering
       - Critical for correctness on weak memory models
```

---

## Version

Based on **Linux kernel v3.2** RCU patterns.
