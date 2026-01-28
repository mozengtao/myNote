# Case 2: IS_ERR / PTR_ERR / ERR_PTR

## The Pattern in Detail

```
+=============================================================================+
|                    ERR_PTR PATTERN                                           |
+=============================================================================+

    THE PROBLEM:
    ============

    void *alloc_something(void)
    {
        if (no_memory)
            return NULL;           /* Error, but which one? */
        if (no_permission)
            return NULL;           /* Same return, different error */
        return ptr;
    }

    Caller cannot distinguish error types!


    THE SOLUTION:
    =============

    void *alloc_something(void)
    {
        if (no_memory)
            return ERR_PTR(-ENOMEM);    /* Specific error */
        if (no_permission)
            return ERR_PTR(-EPERM);     /* Different error */
        return ptr;                      /* Valid pointer */
    }

    /* Caller */
    ptr = alloc_something();
    if (IS_ERR(ptr)) {
        err = PTR_ERR(ptr);
        switch (err) {
        case -ENOMEM:
            /* Handle OOM */
            break;
        case -EPERM:
            /* Handle permission error */
            break;
        }
        return err;
    }
    /* Use ptr normally */
```

**中文说明：**

ERR_PTR模式解决的问题：返回NULL无法区分不同的错误类型。解决方案：使用`ERR_PTR(-errno)`将具体的错误码编码到指针中。调用者用`IS_ERR()`检查是否为错误，用`PTR_ERR()`提取错误码，可以根据不同错误做不同处理。

---

## How It Works

```c
/* include/linux/err.h */

/*
 * Kernel pointers have a few bits that are always 0 or 1.
 * Error codes are small negative numbers (-1 to -4095).
 * When cast to pointer, these become addresses at the TOP of memory:
 *   -1    = 0xFFFFFFFF (32-bit) or 0xFFFFFFFFFFFFFFFF (64-bit)
 *   -4095 = 0xFFFFF001 (32-bit) or 0xFFFFFFFFFFFFF001 (64-bit)
 * 
 * Valid kernel pointers are NEVER in this range.
 */

#define MAX_ERRNO   4095

/* Check if pointer is in error range */
#define IS_ERR_VALUE(x) unlikely((unsigned long)(void *)(x) >= \
                                 (unsigned long)-MAX_ERRNO)

static inline void *ERR_PTR(long error)
{
    return (void *)error;
}

static inline long PTR_ERR(const void *ptr)
{
    return (long)ptr;
}

static inline bool IS_ERR(const void *ptr)
{
    return IS_ERR_VALUE((unsigned long)ptr);
}

/* Also check for NULL */
static inline bool IS_ERR_OR_NULL(const void *ptr)
{
    return unlikely(!ptr) || IS_ERR_VALUE((unsigned long)ptr);
}

/* Cast error pointer to different type */
static inline void *ERR_CAST(const void *ptr)
{
    return (void *)ptr;
}
```

---

## Minimal C Code Simulation

```c
/*
 * ERR_PTR PATTERN SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Error codes */
#define ENOMEM  12
#define EINVAL  22
#define EPERM    1
#define ENOENT   2
#define EIO      5

#define MAX_ERRNO 4095

/* ==========================================================
 * ERR_PTR IMPLEMENTATION
 * ========================================================== */

static inline void *ERR_PTR(long error)
{
    return (void *)error;
}

static inline long PTR_ERR(const void *ptr)
{
    return (long)ptr;
}

static inline bool IS_ERR(const void *ptr)
{
    return (unsigned long)ptr >= (unsigned long)-MAX_ERRNO;
}

static inline bool IS_ERR_OR_NULL(const void *ptr)
{
    return !ptr || IS_ERR(ptr);
}

/* ==========================================================
 * EXAMPLE STRUCTURES
 * ========================================================== */

struct my_object {
    int id;
    char name[32];
    void *data;
};

/* ==========================================================
 * FUNCTIONS RETURNING ERR_PTR
 * ========================================================== */

/*
 * Allocate object - demonstrates ERR_PTR usage
 */
struct my_object *my_object_create(const char *name, int flags)
{
    struct my_object *obj;
    
    printf("[CREATE] Creating object '%s' with flags 0x%x\n", name, flags);
    
    /* Check arguments */
    if (!name || strlen(name) == 0) {
        printf("[CREATE] Error: invalid name\n");
        return ERR_PTR(-EINVAL);
    }
    
    /* Check permissions (simulated) */
    if (flags & 0x100) {
        printf("[CREATE] Error: permission denied\n");
        return ERR_PTR(-EPERM);
    }
    
    /* Allocate memory (simulated failure) */
    if (flags & 0x200) {
        printf("[CREATE] Error: out of memory\n");
        return ERR_PTR(-ENOMEM);
    }
    
    /* I/O error (simulated) */
    if (flags & 0x400) {
        printf("[CREATE] Error: I/O error\n");
        return ERR_PTR(-EIO);
    }
    
    /* Success path */
    obj = malloc(sizeof(*obj));
    if (!obj) {
        return ERR_PTR(-ENOMEM);
    }
    
    obj->id = rand() % 1000;
    strncpy(obj->name, name, sizeof(obj->name) - 1);
    obj->data = malloc(256);
    
    printf("[CREATE] Success: object '%s' (id=%d) at %p\n", 
           name, obj->id, (void *)obj);
    return obj;
}

/*
 * Lookup object - returns ERR_PTR on not found
 */
struct my_object *my_object_lookup(int id, struct my_object *list[], int count)
{
    printf("[LOOKUP] Looking for object with id=%d\n", id);
    
    for (int i = 0; i < count; i++) {
        if (list[i] && list[i]->id == id) {
            printf("[LOOKUP] Found object '%s'\n", list[i]->name);
            return list[i];
        }
    }
    
    printf("[LOOKUP] Object not found\n");
    return ERR_PTR(-ENOENT);
}

/* ==========================================================
 * CALLER CODE DEMONSTRATING PATTERN
 * ========================================================== */

void demonstrate_create(const char *name, int flags)
{
    struct my_object *obj;
    long err;
    
    printf("\n--- Creating '%s' with flags 0x%x ---\n", 
           name ? name : "(null)", flags);
    
    obj = my_object_create(name, flags);
    
    if (IS_ERR(obj)) {
        err = PTR_ERR(obj);
        printf("Creation failed with error: %ld\n", err);
        
        /* Handle specific errors */
        switch ((int)err) {
        case -EINVAL:
            printf("  -> Invalid argument provided\n");
            break;
        case -EPERM:
            printf("  -> Permission denied\n");
            break;
        case -ENOMEM:
            printf("  -> Out of memory\n");
            break;
        case -EIO:
            printf("  -> I/O error\n");
            break;
        default:
            printf("  -> Unknown error\n");
        }
        return;
    }
    
    /* Success - use object */
    printf("Created successfully: %s (id=%d)\n", obj->name, obj->id);
    
    /* Cleanup */
    free(obj->data);
    free(obj);
}

void demonstrate_lookup(void)
{
    struct my_object obj1 = { .id = 42, .name = "test1" };
    struct my_object obj2 = { .id = 99, .name = "test2" };
    struct my_object *list[] = { &obj1, &obj2 };
    struct my_object *found;
    
    printf("\n--- Lookup demonstration ---\n");
    
    /* Successful lookup */
    found = my_object_lookup(42, list, 2);
    if (IS_ERR(found)) {
        printf("Lookup failed: %ld\n", PTR_ERR(found));
    } else {
        printf("Found: %s\n", found->name);
    }
    
    /* Failed lookup */
    found = my_object_lookup(123, list, 2);
    if (IS_ERR(found)) {
        printf("Lookup failed: %ld (ENOENT expected)\n", PTR_ERR(found));
    }
}

/* ==========================================================
 * MAIN DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("ERR_PTR PATTERN DEMONSTRATION\n");
    printf("=================================================\n");
    
    /* Show ERR_PTR values */
    printf("\nERR_PTR values:\n");
    printf("ERR_PTR(-EINVAL) = %p\n", ERR_PTR(-EINVAL));
    printf("ERR_PTR(-ENOMEM) = %p\n", ERR_PTR(-ENOMEM));
    printf("ERR_PTR(-EPERM)  = %p\n", ERR_PTR(-EPERM));
    printf("ERR_PTR(-EIO)    = %p\n", ERR_PTR(-EIO));
    
    printf("\nIS_ERR checks:\n");
    printf("IS_ERR(ERR_PTR(-EINVAL)) = %d\n", IS_ERR(ERR_PTR(-EINVAL)));
    printf("IS_ERR((void*)0x1000)    = %d\n", IS_ERR((void*)0x1000));
    printf("IS_ERR(NULL)             = %d\n", IS_ERR(NULL));
    
    /* Demonstrate various error cases */
    demonstrate_create("valid_name", 0);      /* Success */
    demonstrate_create(NULL, 0);               /* EINVAL */
    demonstrate_create("", 0);                 /* EINVAL */
    demonstrate_create("test", 0x100);         /* EPERM */
    demonstrate_create("test", 0x200);         /* ENOMEM */
    demonstrate_create("test", 0x400);         /* EIO */
    
    /* Demonstrate lookup */
    demonstrate_lookup();
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- ERR_PTR encodes error in pointer value\n");
    printf("- IS_ERR checks if pointer is in error range\n");
    printf("- PTR_ERR extracts the error code\n");
    printf("- Caller can distinguish different errors\n");
    printf("- Error pointers are high addresses (near max)\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## Real Kernel Examples

### File Operations

```c
/* fs/namei.c */
struct dentry *lookup_one_len(const char *name, 
                               struct dentry *base, int len)
{
    /* ... */
    if (!len)
        return ERR_PTR(-EACCES);
    
    if (name[0] == '.') {
        if (len == 1)
            return ERR_PTR(-EINVAL);
        if (len == 2 && name[1] == '.')
            return ERR_PTR(-EINVAL);
    }
    /* ... */
}

/* Caller */
dentry = lookup_one_len(name, base, namelen);
if (IS_ERR(dentry))
    return PTR_ERR(dentry);
```

### Device Registration

```c
/* Device creation returns ERR_PTR on failure */
struct device *device_create(struct class *class, ...);

/* Usage */
dev = device_create(my_class, NULL, devno, NULL, "mydev");
if (IS_ERR(dev)) {
    pr_err("Failed to create device: %ld\n", PTR_ERR(dev));
    return PTR_ERR(dev);
}
```

---

## Key Takeaways

1. **Error encoding**: Negative error codes fit in pointer's high bits
2. **Type preservation**: Can return specific error OR valid pointer
3. **No ambiguity**: IS_ERR clearly distinguishes error from valid pointer
4. **Error propagation**: Easy to pass error up the call stack
5. **NULL is different**: IS_ERR(NULL) is false; use IS_ERR_OR_NULL if needed
