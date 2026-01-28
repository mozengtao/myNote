# Identification Rules for Reference Counting Pattern

## Structural Signals

```
+=============================================================================+
|                    REFERENCE COUNTING ANATOMY                                |
+=============================================================================+

    STRUCTURE DEFINITION:
    =====================

    struct refcounted_object {
        /* ... object data ... */
        
        struct kref ref;           <-- EMBEDDED reference counter
        /* OR */
        atomic_t refcount;         <-- Direct atomic counter
        /* OR */
        int count;                 <-- Simple counter (must be protected)
        
        /* ... more data ... */
    };


    OPERATIONS PATTERN:
    ===================

    /* Initialize */
    kref_init(&obj->ref);
    /* OR */
    atomic_set(&obj->refcount, 1);

    /* Get reference */
    kref_get(&obj->ref);
    /* OR */
    atomic_inc(&obj->refcount);
    /* OR */
    get_object(obj);              <-- Wrapper function

    /* Put reference */
    kref_put(&obj->ref, release_func);
    /* OR */
    if (atomic_dec_and_test(&obj->refcount))
        free_object(obj);
    /* OR */
    put_object(obj);              <-- Wrapper function
```

**中文说明：**

引用计数模式的结构特征：结构体中有嵌入的kref或atomic_t计数器，有初始化、获取（get）、释放（put）三种操作。常见的包装函数模式：`get_xxx()`增加引用，`put_xxx()`减少引用。

---

## The Five Identification Rules

### Rule 1: Look for kref or atomic_t Member

```c
/* REFERENCE COUNTING: Has kref or atomic counter */

struct kobject {
    /* ... */
    struct kref kref;         /* <-- REFERENCE COUNTING */
};

struct file {
    /* ... */
    atomic_long_t f_count;    /* <-- REFERENCE COUNTING */
};

struct module {
    /* ... */
    struct module_ref ref;    /* <-- REFERENCE COUNTING */
};

/* NOT REFERENCE COUNTING: Simple int without protection */
struct not_refcounted {
    int use_count;            /* Might be, but need to verify */
    /* Look for atomic operations or locks around it */
};
```

### Rule 2: Look for get/put Function Pairs

```c
/* Common naming patterns for reference counting: */

/* Pattern: xxx_get / xxx_put */
struct kobject *kobject_get(struct kobject *kobj);
void kobject_put(struct kobject *kobj);

/* Pattern: get_xxx / put_xxx */
struct file *get_file(struct file *f);
void fput(struct file *f);

/* Pattern: xxx_hold / xxx_release */
void dev_hold(struct net_device *dev);
void dev_put(struct net_device *dev);

/* Pattern: xxx_ref / xxx_unref */
void skb_get(struct sk_buff *skb);
void kfree_skb(struct sk_buff *skb);  /* Implicit unref + free */

/* If you see these pairs, it's reference counting! */
```

**中文说明：**

规则2：寻找get/put函数对。常见命名模式包括：`xxx_get/xxx_put`、`get_xxx/put_xxx`、`xxx_hold/xxx_release`、`xxx_ref/xxx_unref`。如果看到这些配对的函数，就是引用计数模式。

### Rule 3: Look for Release/Destroy Callbacks

```c
/* Reference counting often has a release callback */

/* kref pattern */
void my_release(struct kref *ref)
{
    struct my_object *obj = container_of(ref, struct my_object, ref);
    kfree(obj);
}

kref_put(&obj->ref, my_release);

/* kobject pattern */
struct kobj_type my_ktype = {
    .release = my_kobject_release,  /* <-- Release callback */
};

/* If there's a release callback invoked on "put", it's refcounting */
```

### Rule 4: Check for Atomic Operations on Counter

```c
/* Reference counting uses atomic operations */

/* Direct atomic usage */
atomic_inc(&obj->count);           /* get */
if (atomic_dec_and_test(&obj->count))  /* put + check */
    free_obj(obj);

/* Read current count (for debugging/checking) */
atomic_read(&obj->count);

/* Look for these atomic operations on a counter field */
```

### Rule 5: Look for Documentation Comments

```c
/* Kernel code often documents refcount semantics */

/**
 * xxx_get - acquire a reference to xxx
 * @obj: the object to reference
 *
 * Caller must already hold a reference.
 * ...
 */

/**
 * xxx_put - release a reference to xxx
 * @obj: the object to release
 *
 * If this was the last reference, obj will be freed.
 * ...
 */

/* Comments mentioning "reference", "refcount", "hold", "release" */
```

---

## Summary Checklist

```
+=============================================================================+
|                    REFERENCE COUNTING CHECKLIST                              |
+=============================================================================+

    When examining code, check:

    [ ] 1. COUNTER MEMBER
        Is there a kref, atomic_t, or atomic_long_t member?
        struct foo { struct kref ref; }
        struct foo { atomic_t count; }

    [ ] 2. GET/PUT FUNCTIONS
        Are there paired functions for acquire/release?
        xxx_get() / xxx_put()
        get_xxx() / put_xxx()

    [ ] 3. RELEASE CALLBACK
        Is there a function called when count hits zero?
        void release(struct kref *ref) { ... kfree(...) }

    [ ] 4. ATOMIC OPERATIONS
        Are increment/decrement atomic?
        atomic_inc(), atomic_dec_and_test()

    [ ] 5. DOCUMENTATION
        Do comments mention "reference" or "refcount"?

    SCORING:
    3+ indicators = Definitely reference counting
    2 indicators  = Likely reference counting
    1 indicator   = Investigate further
```

---

## Red Flags: NOT Reference Counting

```
    NOT REFERENCE COUNTING:

    1. SIMPLE INT COUNTER WITHOUT ATOMIC
       struct foo {
           int count;  /* NOT refcount without atomic ops */
       };
       count++;         /* Not atomic - probably not refcount */

    2. USE COUNT FOR STATISTICS
       struct foo {
           int usage_count;  /* Just tracking how many times used */
       };
       /* Not for lifecycle - just statistics */

    3. LIMIT COUNTER
       struct foo {
           int max_uses;
           int current_uses;
       };
       /* Limiting concurrent access, not lifecycle management */

    4. STATE FLAGS
       struct foo {
           int state;  /* 0=idle, 1=active, 2=error */
       };
       /* State machine, not reference counting */
```

---

## Quick Reference: Common Refcounted Types

| Type | Counter | Get | Put |
|------|---------|-----|-----|
| `struct kobject` | `kref` | `kobject_get()` | `kobject_put()` |
| `struct file` | `f_count` | `get_file()` | `fput()` |
| `struct inode` | `i_count` | `ihold()` | `iput()` |
| `struct dentry` | `d_count` | `dget()` | `dput()` |
| `struct module` | `refcnt` | `try_module_get()` | `module_put()` |
| `struct net_device` | `refcnt` | `dev_hold()` | `dev_put()` |
| `struct sk_buff` | `users` | `skb_get()` | `kfree_skb()` |
| `struct page` | `_count` | `get_page()` | `put_page()` |
