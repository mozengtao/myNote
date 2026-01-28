# Anti-Patterns: What NOT to Do with Reference Counting

## Anti-Pattern 1: Unbalanced Get/Put

```c
/*
 * ANTI-PATTERN: Missing put
 * =========================
 */

void memory_leak(void)
{
    struct my_object *obj = my_object_create();  /* refcount = 1 */
    my_object_get(obj);  /* refcount = 2 */
    
    use_object(obj);
    
    my_object_put(obj);  /* refcount = 1 */
    
    /* LEAK! Forgot to put the second reference */
    /* refcount never reaches 0 */
    /* Object never freed */
}


/*
 * CORRECT: Balanced get/put
 */
void no_leak(void)
{
    struct my_object *obj = my_object_create();  /* refcount = 1 */
    my_object_get(obj);  /* refcount = 2 */
    
    use_object(obj);
    
    my_object_put(obj);  /* refcount = 1 */
    my_object_put(obj);  /* refcount = 0, freed */
}
```

**中文说明：**

反模式1：不平衡的get/put。如果get了两次但只put了一次，引用计数永远不会变为0，对象永远不会被释放，造成内存泄漏。必须保证每个get都有对应的put。

---

## Anti-Pattern 2: Use After Put

```c
/*
 * ANTI-PATTERN: Accessing object after releasing reference
 * ========================================================
 */

void use_after_free(void)
{
    struct my_object *obj = my_object_create();
    
    /* Release our only reference */
    my_object_put(obj);  /* refcount = 0, FREED! */
    
    /* BUG: Object is freed, but we still access it */
    printf("ID: %d\n", obj->id);  /* CRASH or corruption */
}


/*
 * CORRECT: Don't access after put
 */
void safe_usage(void)
{
    struct my_object *obj = my_object_create();
    
    /* Access before put */
    printf("ID: %d\n", obj->id);  /* OK */
    
    /* Release reference */
    my_object_put(obj);
    
    /* Don't touch obj anymore! */
    obj = NULL;  /* Good practice: NULL the pointer */
}
```

---

## Anti-Pattern 3: Get Without Valid Reference

```c
/*
 * ANTI-PATTERN: Getting reference when you don't have one
 * =======================================================
 */

void get_from_nowhere(struct my_object *obj)
{
    /*
     * Who says obj is valid?
     * If caller didn't have a reference, obj might be freed!
     */
    my_object_get(obj);  /* May be operating on freed memory! */
}


/*
 * CORRECT: Get only when you already have a reference
 */
void safe_get(struct my_object *obj)
{
    /*
     * Caller guarantees they hold a reference.
     * obj is guaranteed valid.
     */
    my_object_get(obj);  /* Safe - caller has reference */
    
    /* Now WE have a reference too */
    use_object(obj);
    my_object_put(obj);  /* Release OUR reference */
}
```

---

## Anti-Pattern 4: Not Incrementing on Return

```c
/*
 * ANTI-PATTERN: Returning object without incrementing
 * ====================================================
 */

struct my_object *bad_lookup(int id)
{
    struct my_object *obj = find_in_list(id);
    return obj;  /* Returning without get! */
    
    /* PROBLEM: 
     * - Caller assumes they have a reference
     * - But object could be freed by another thread!
     */
}


/*
 * CORRECT: Get before returning
 */
struct my_object *good_lookup(int id)
{
    struct my_object *obj = find_in_list(id);
    if (obj) {
        my_object_get(obj);  /* Increment before return */
    }
    return obj;
    
    /* Caller must call my_object_put() when done */
}
```

**中文说明：**

反模式4：返回对象时不增加引用计数。如果查找函数返回对象但不增加引用计数，调用者可能在使用时对象被其他线程释放。正确做法：返回前增加引用计数，调用者用完后负责释放。

---

## Anti-Pattern 5: Double Free

```c
/*
 * ANTI-PATTERN: Putting more times than getting
 * =============================================
 */

void double_free(void)
{
    struct my_object *obj = my_object_create();  /* refcount = 1 */
    
    my_object_put(obj);  /* refcount = 0, FREED */
    my_object_put(obj);  /* DOUBLE FREE! Undefined behavior */
}


/*
 * ANOTHER FORM: Two owners both think they should free
 */
void double_free_two_owners(struct my_object *obj)
{
    /* Thread A and Thread B both have obj pointer */
    /* But only ONE reference exists */
    
    /* Thread A: "I'm done" */
    my_object_put(obj);  /* OK, freed */
    
    /* Thread B: "I'm done too" */
    my_object_put(obj);  /* DOUBLE FREE! */
}


/*
 * CORRECT: Each owner has their own reference
 */
void correct_two_owners(struct my_object *obj)
{
    my_object_get(obj);  /* Thread B gets own reference */
    
    /* Thread A: "I'm done" */
    my_object_put(obj);  /* refcount 2 -> 1, not freed */
    
    /* Thread B: "I'm done" */
    my_object_put(obj);  /* refcount 1 -> 0, freed */
}
```

---

## Anti-Pattern 6: Circular References

```c
/*
 * ANTI-PATTERN: Circular references without handling
 * ==================================================
 */

struct parent {
    struct kref ref;
    struct child *child;
};

struct child {
    struct kref ref;
    struct parent *parent;  /* Points back to parent */
};

void create_cycle(void)
{
    struct parent *p = create_parent();    /* p.ref = 1 */
    struct child *c = create_child();      /* c.ref = 1 */
    
    p->child = c;
    kref_get(&c->ref);    /* c.ref = 2 (owned by p) */
    
    c->parent = p;
    kref_get(&p->ref);    /* p.ref = 2 (owned by c) */
    
    /* Now release creator's references */
    parent_put(p);        /* p.ref = 1 (held by c) */
    child_put(c);         /* c.ref = 1 (held by p) */
    
    /* LEAK! Both refcounts stuck at 1 forever */
}


/*
 * CORRECT: Use weak reference for back-pointer
 */
struct child_fixed {
    struct kref ref;
    struct parent *parent;  /* WEAK - no kref_get */
};

void create_no_cycle(void)
{
    struct parent *p = create_parent();
    struct child_fixed *c = create_child_fixed();
    
    p->child = c;
    kref_get(&c->ref);    /* c.ref = 2 */
    
    c->parent = p;        /* NO kref_get - weak reference */
    
    /* Release creator's references */
    parent_put(p);        /* p.ref = 0, freed, c->parent invalid */
    child_put(c);         /* c.ref = 1, then 0, freed */
}
```

---

## Anti-Pattern 7: Non-Atomic Counter

```c
/*
 * ANTI-PATTERN: Using non-atomic counter
 * ======================================
 */

struct bad_object {
    int refcount;  /* NOT atomic! */
};

void bad_get(struct bad_object *obj)
{
    obj->refcount++;  /* NOT THREAD SAFE */
    
    /* Race condition:
     * Thread A reads refcount (1)
     * Thread B reads refcount (1)
     * Thread A writes refcount (2)
     * Thread B writes refcount (2)
     * Should be 3, but it's 2!
     */
}


/*
 * CORRECT: Use atomic counter
 */
struct good_object {
    atomic_int refcount;  /* Atomic */
};

void good_get(struct good_object *obj)
{
    atomic_fetch_add(&obj->refcount, 1);  /* Thread safe */
}
```

---

## Summary: Reference Counting Rules

```
+=============================================================================+
|              REFERENCE COUNTING RULES                                        |
+=============================================================================+

    1. BALANCE GET AND PUT
       Every get needs matching put.
       Track ownership carefully.

    2. NO ACCESS AFTER PUT
       Once you put, treat pointer as invalid.
       Set to NULL to catch bugs.

    3. GET REQUIRES VALID REFERENCE
       Only get when you already have reference.
       Can't get from nothing.

    4. GET BEFORE RETURN
       Lookup functions must get before returning.
       Caller must put when done.

    5. AVOID CYCLES
       Use weak references for back-pointers.
       Or break cycles explicitly before release.

    6. USE ATOMIC COUNTER
       Multi-threaded code needs atomic operations.
       Use kref or atomic_t.

    7. DOCUMENT OWNERSHIP
       Comments should clarify who owns what.
       "Takes a reference" / "Returns with reference"
```
