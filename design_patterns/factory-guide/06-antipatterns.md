# Factory Anti-Patterns

Common mistakes to avoid when implementing Factory pattern.

---

## Anti-Pattern 1: Incomplete Initialization

```c
/* BAD: Factory doesn't initialize all fields */
struct my_obj *bad_alloc(void)
{
    struct my_obj *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    
    obj->field1 = 0;
    /* Forgot field2, field3, list heads, etc. */
    
    return obj;  /* Partially initialized! */
}

/* CORRECT: Initialize everything */
struct my_obj *good_alloc(void)
{
    struct my_obj *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    
    obj->field1 = DEFAULT1;
    obj->field2 = DEFAULT2;
    INIT_LIST_HEAD(&obj->list);
    kref_init(&obj->kref);
    
    return obj;
}
```

**Chinese Explanation:**

Anti-pattern 1: Incomplete initialization - Factory must initialize all fields. Use kzalloc for zero initialization, then set specific defaults.

---

## Anti-Pattern 2: No Matching Free Function

```c
/* BAD: Factory without destructor */
struct my_obj *alloc_my_obj(void)
{
    /* Creates object but no way to properly free */
}
/* Caller must know internal details to free! */

/* CORRECT: Provide matching free */
struct my_obj *alloc_my_obj(void);
void free_my_obj(struct my_obj *obj);
```

---

## Anti-Pattern 3: Leaking on Error

```c
/* BAD: Memory leak on partial failure */
struct my_obj *bad_alloc(void)
{
    struct my_obj *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    
    obj->buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!obj->buffer)
        return NULL;  /* obj leaked! */
    
    return obj;
}

/* CORRECT: Cleanup on error */
struct my_obj *good_alloc(void)
{
    struct my_obj *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    
    obj->buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!obj->buffer) {
        kfree(obj);  /* Cleanup! */
        return NULL;
    }
    
    return obj;
}
```

**Chinese Explanation:**

Anti-pattern 3: Memory leak on partial failure - If allocation fails partway, must free already allocated memory before returning NULL.

---

## Anti-Pattern 4: Exposing Internal Details

```c
/* BAD: Caller must know size */
void bad_usage(void)
{
    struct my_obj *obj = kmalloc(sizeof(struct my_obj) + priv, GFP_KERNEL);
    /* Caller knows too much about internals */
}

/* CORRECT: Factory hides details */
void good_usage(void)
{
    struct my_obj *obj = alloc_my_obj(priv_size);
    /* Factory handles size calculation */
}
```

---

## Anti-Pattern 5: Ignoring Return Value

```c
/* BAD: Not checking factory return */
void bad_caller(void)
{
    struct my_obj *obj = alloc_my_obj();
    obj->field = 1;  /* Crash if NULL! */
}

/* CORRECT: Always check return */
void good_caller(void)
{
    struct my_obj *obj = alloc_my_obj();
    if (!obj)
        return -ENOMEM;
    obj->field = 1;
}
```

---

## Summary Checklist

```
+=============================================================================+
|                    FACTORY SAFE USAGE                                        |
+=============================================================================+

    [X] Initialize all fields (use kzalloc)
    [X] Provide matching free function
    [X] Cleanup on partial failure
    [X] Hide internal details
    [X] Always check return value
    [X] Document ownership/refcount
```

---

## Version

Based on **Linux kernel v3.2** factory patterns.
