# Case 3: Return Value Conventions

## The Pattern in Detail

```
+=============================================================================+
|                    KERNEL RETURN VALUE CONVENTIONS                           |
+=============================================================================+

    CONVENTION 1: Integer Functions
    ===============================

    Signature: int function(args...);

    Return values:
    - Negative: Error code (-ENOMEM, -EINVAL, etc.)
    - Zero: Success
    - Positive: Success with additional info (e.g., bytes read)


    CONVENTION 2: Pointer Functions (Simple)
    ========================================

    Signature: void *function(args...);

    Return values:
    - NULL: Error (usually implies -ENOMEM)
    - Non-NULL: Success (valid pointer)


    CONVENTION 3: Pointer Functions (ERR_PTR)
    =========================================

    Signature: void *function(args...);

    Return values:
    - ERR_PTR(-errno): Error with specific code
    - Valid pointer: Success


    CONVENTION 4: Boolean Functions
    ===============================

    Signature: bool function(args...);

    Return values:
    - true: Condition satisfied
    - false: Condition not satisfied (not necessarily error)
```

**中文说明：**

内核返回值约定：(1) 整数函数——负数是错误码，零是成功，正数可能有特殊含义；(2) 简单指针函数——NULL是错误（通常意味着ENOMEM），非NULL是成功；(3) ERR_PTR指针函数——ERR_PTR是特定错误码，有效指针是成功；(4) 布尔函数——true是条件满足，false是条件不满足。

---

## Convention 1: Integer Functions

```c
/*
 * Integer function returns: negative=error, 0=success, positive=data
 */

/* Example: Read operation */
ssize_t my_read(struct file *file, char *buf, size_t count)
{
    /* Error cases */
    if (!file)
        return -EINVAL;
    
    if (!access_ok(buf, count))
        return -EFAULT;
    
    if (device_not_ready())
        return -EAGAIN;
    
    /* Success: return bytes read (positive) */
    return bytes_actually_read;
}

/* Caller */
ssize_t ret = my_read(file, buf, count);
if (ret < 0) {
    /* Error occurred */
    handle_error(-ret);  /* Convert to positive errno */
} else if (ret == 0) {
    /* EOF or no data */
} else {
    /* Got 'ret' bytes */
}


/* Example: Simple success/failure */
int my_init(void)
{
    if (setup_failed)
        return -ENOMEM;
    
    return 0;  /* Success */
}

/* Caller */
int ret = my_init();
if (ret) {  /* Shorthand: non-zero is error */
    pr_err("init failed: %d\n", ret);
    return ret;
}
```

---

## Convention 2: Simple Pointer (NULL on error)

```c
/*
 * Simple allocation - NULL means error
 */

void *kmalloc(size_t size, gfp_t flags)
{
    void *ptr = allocate_memory(size, flags);
    if (!ptr)
        return NULL;  /* Out of memory */
    return ptr;
}

/* Caller */
ptr = kmalloc(size, GFP_KERNEL);
if (!ptr) {
    /* Assume ENOMEM */
    return -ENOMEM;
}


/*
 * When to use NULL pattern:
 * - Memory allocation
 * - Simple get/find functions
 * - When only one error type is possible
 */
```

---

## Convention 3: ERR_PTR (Specific Error Codes)

```c
/*
 * ERR_PTR when multiple error types possible
 */

struct dentry *lookup_path(const char *name)
{
    if (!name)
        return ERR_PTR(-EINVAL);
    
    if (name[0] != '/')
        return ERR_PTR(-ENOTDIR);
    
    dentry = find_dentry(name);
    if (!dentry)
        return ERR_PTR(-ENOENT);
    
    if (!check_permission(dentry))
        return ERR_PTR(-EACCES);
    
    return dentry;
}

/* Caller */
dentry = lookup_path("/etc/passwd");
if (IS_ERR(dentry)) {
    switch (PTR_ERR(dentry)) {
    case -ENOENT:
        /* File not found */
        break;
    case -EACCES:
        /* Permission denied */
        break;
    default:
        /* Other error */
        break;
    }
    return PTR_ERR(dentry);
}


/*
 * When to use ERR_PTR:
 * - Multiple possible error types
 * - Caller needs to distinguish errors
 * - Used throughout VFS, device model
 */
```

---

## Combining Conventions

```c
/*
 * Real kernel pattern: combining conventions
 */

int driver_probe(struct device *dev)
{
    struct resource *res;
    void __iomem *base;
    int ret;
    
    /* Integer return (Convention 1) */
    ret = pci_enable_device(pci_dev);
    if (ret)
        return ret;
    
    /* Simple pointer (Convention 2) */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        ret = -EINVAL;
        goto err_disable;
    }
    
    /* ERR_PTR return (Convention 3) */
    base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base)) {
        ret = PTR_ERR(base);
        goto err_disable;
    }
    
    return 0;
    
err_disable:
    pci_disable_device(pci_dev);
    return ret;
}
```

---

## Error Code Selection Guide

```
+=============================================================================+
|                    CHOOSING THE RIGHT ERROR CODE                             |
+=============================================================================+

    Error Code     | When to Use
    ---------------+------------------------------------------------
    -ENOMEM        | Memory allocation failed
    -EINVAL        | Invalid argument passed to function
    -ENOENT        | Requested item not found
    -EEXIST        | Item already exists
    -EBUSY         | Resource is busy/in use
    -EAGAIN        | Try again later (temporary failure)
    -EIO           | I/O error (hardware issue)
    -EPERM         | Operation not permitted (security)
    -EACCES        | Access denied (permissions)
    -ENODEV        | No such device
    -EFAULT        | Bad memory address
    -ETIMEDOUT     | Operation timed out
    -ENOSPC        | No space left
    -ENOTSUPP      | Operation not supported

    GUIDELINES:
    ===========
    
    1. Be specific - use the most appropriate code
    2. Don't invent new codes - use existing ones
    3. Propagate original error when possible
    4. Document unusual error usage
```

---

## Minimal C Simulation

```c
/*
 * RETURN CONVENTION DEMONSTRATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ENOMEM 12
#define EINVAL 22
#define ENOENT  2
#define EAGAIN 11

#define MAX_ERRNO 4095
#define ERR_PTR(err) ((void *)(long)(err))
#define PTR_ERR(ptr) ((long)(ptr))
#define IS_ERR(ptr) ((unsigned long)(ptr) >= (unsigned long)-MAX_ERRNO)

/* ==========================================================
 * Convention 1: Integer return
 * ========================================================== */

int do_operation(int value)
{
    if (value < 0)
        return -EINVAL;
    
    if (value == 0)
        return -EAGAIN;
    
    return value * 2;  /* Success: return processed value */
}

void test_integer_convention(void)
{
    int results[] = {-5, 0, 10};
    
    printf("\n--- Integer Convention ---\n");
    for (int i = 0; i < 3; i++) {
        int ret = do_operation(results[i]);
        printf("do_operation(%d) = %d ", results[i], ret);
        if (ret < 0)
            printf("[ERROR]\n");
        else
            printf("[SUCCESS]\n");
    }
}

/* ==========================================================
 * Convention 2: Pointer return (NULL)
 * ========================================================== */

void *simple_alloc(size_t size, bool should_fail)
{
    if (should_fail)
        return NULL;
    return malloc(size);
}

void test_null_convention(void)
{
    printf("\n--- NULL Pointer Convention ---\n");
    
    void *p1 = simple_alloc(100, false);
    printf("simple_alloc(success): %p %s\n", 
           p1, p1 ? "[SUCCESS]" : "[ERROR]");
    free(p1);
    
    void *p2 = simple_alloc(100, true);
    printf("simple_alloc(fail):    %p %s\n", 
           p2, p2 ? "[SUCCESS]" : "[ERROR]");
}

/* ==========================================================
 * Convention 3: ERR_PTR return
 * ========================================================== */

void *lookup_item(int id)
{
    if (id < 0)
        return ERR_PTR(-EINVAL);
    
    if (id == 0)
        return ERR_PTR(-ENOENT);
    
    /* Simulate finding an item */
    static int found = 42;
    return &found;
}

void test_errptr_convention(void)
{
    printf("\n--- ERR_PTR Convention ---\n");
    
    int test_ids[] = {-1, 0, 5};
    for (int i = 0; i < 3; i++) {
        void *result = lookup_item(test_ids[i]);
        printf("lookup_item(%d): ", test_ids[i]);
        
        if (IS_ERR(result)) {
            printf("ERR_PTR(%ld) [ERROR]\n", PTR_ERR(result));
        } else {
            printf("%p [SUCCESS]\n", result);
        }
    }
}

int main(void)
{
    printf("=================================================\n");
    printf("RETURN VALUE CONVENTIONS DEMONSTRATION\n");
    printf("=================================================\n");
    
    test_integer_convention();
    test_null_convention();
    test_errptr_convention();
    
    printf("\n=================================================\n");
    return 0;
}
```

---

## Key Takeaways

1. **Be consistent**: Use the same convention throughout a subsystem
2. **Match caller expectations**: Check documentation for each function
3. **Propagate errors**: Pass original error codes up the stack
4. **Use specific codes**: Choose the most appropriate error code
5. **Check both NULL and ERR_PTR**: Know which convention a function uses
