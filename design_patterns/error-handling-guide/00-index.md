# Error Handling Patterns in Linux Kernel (v3.2)

Kernel-specific error handling idioms for robust resource management.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept: Error Handling in Kernel Context |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-goto-cleanup-case.md](03-goto-cleanup-case.md) | Case 1: goto Cleanup Pattern |
| [04-err-ptr-case.md](04-err-ptr-case.md) | Case 2: IS_ERR/PTR_ERR/ERR_PTR |
| [05-return-conventions-case.md](05-return-conventions-case.md) | Case 3: Return Value Conventions |
| [06-unified-skeleton.md](06-unified-skeleton.md) | Unified Skeleton |
| [07-vs-exceptions.md](07-vs-exceptions.md) | Error Handling vs Exceptions |
| [08-antipatterns.md](08-antipatterns.md) | Anti-Patterns |
| [09-reading-guide.md](09-reading-guide.md) | Source Reading Guide |
| [10-mental-model.md](10-mental-model.md) | Final Mental Model |

---

## Overview Diagram

```
+=============================================================================+
|                    KERNEL ERROR HANDLING PATTERNS                            |
+=============================================================================+

    THE PROBLEM:
    ============

    Kernel functions often:
    1. Allocate multiple resources
    2. Any step can fail
    3. On failure, must clean up ALL previous allocations
    4. Cannot use exceptions (C language, kernel context)

    int my_init(void)
    {
        a = alloc_a();      /* Success */
        b = alloc_b();      /* Success */
        c = alloc_c();      /* FAILS! */
        
        /* Must free a and b before returning error */
        /* How to do this cleanly? */
    }


    THE SOLUTION: GOTO CLEANUP
    ==========================

    int my_init(void)
    {
        a = alloc_a();
        if (!a) goto err_a;
        
        b = alloc_b();
        if (!b) goto err_b;
        
        c = alloc_c();
        if (!c) goto err_c;
        
        return 0;       /* Success */
        
    err_c:
        free_b(b);
    err_b:
        free_a(a);
    err_a:
        return -ENOMEM;
    }

    Clean, readable, efficient!
```

**中文说明：**

内核错误处理的问题：内核函数经常需要分配多个资源，任何步骤都可能失败，失败时必须清理所有之前的分配，但C语言和内核上下文不能使用异常。解决方案：goto cleanup模式——使用goto跳转到清理标签，按照分配的逆序释放资源。这种方式清晰、可读、高效。

---

## Key Error Handling Patterns

### Pattern 1: goto Cleanup

```c
/* Most common pattern for resource cleanup */
int init(void)
{
    res1 = alloc1();
    if (!res1)
        goto err1;
    
    res2 = alloc2();
    if (!res2)
        goto err2;
    
    return 0;
    
err2:
    free1(res1);
err1:
    return -ENOMEM;
}
```

### Pattern 2: IS_ERR / PTR_ERR / ERR_PTR

```c
/* Encode error code in pointer */
void *my_alloc(void)
{
    if (error_condition)
        return ERR_PTR(-ENOMEM);
    return valid_pointer;
}

/* Check and decode */
void *ptr = my_alloc();
if (IS_ERR(ptr)) {
    int err = PTR_ERR(ptr);
    /* Handle error */
}
```

### Pattern 3: Return Conventions

```c
/* Negative = error, 0 = success, positive = special */
int do_something(void);  /* Returns -errno or 0 */

/* NULL = error for pointers */
void *get_something(void);  /* Returns ptr or NULL */

/* ERR_PTR for pointers with error codes */
void *get_something_v2(void);  /* Returns ptr or ERR_PTR(-errno) */
```

---

## Why These Patterns

### No Exceptions in C

```
    C++ / Java:                       C (Kernel):
    ===========                       ===========

    try {                             int func(void) {
        a = alloc_a();                    a = alloc_a();
        b = alloc_b();                    if (!a) goto err_a;
        c = alloc_c();                    b = alloc_b();
    } catch (...) {                       if (!b) goto err_b;
        /* cleanup */                     ...
    }                                 err_b:
                                          free_a(a);
    Exception unwinds stack           err_a:
    automatically                         return -ENOMEM;
                                      }
                                      
                                      Manual cleanup via goto
```

### Performance

```
    Exceptions require:               goto requires:
    - Stack unwinding                 - Single jump instruction
    - Runtime type info               - No runtime overhead
    - Exception tables                - Predictable branches
    
    In kernel (millions of calls/sec):
    - goto cleanup is nearly free
    - Exceptions would be too expensive
```

---

## Key Terminology

| Term | Meaning |
|------|---------|
| **goto cleanup** | Jump to cleanup code on error |
| **ERR_PTR** | Encode error code as pointer |
| **IS_ERR** | Check if pointer is error |
| **PTR_ERR** | Extract error code from pointer |
| **-ENOMEM** | Error code: out of memory |
| **-EINVAL** | Error code: invalid argument |
| **-ENOENT** | Error code: no such entry |

---

## Error Code Convention

```c
/* include/linux/errno.h and include/asm-generic/errno.h */

/* Common error codes (negated in return values) */
#define EPERM            1      /* Operation not permitted */
#define ENOENT           2      /* No such file or directory */
#define ESRCH            3      /* No such process */
#define EINTR            4      /* Interrupted system call */
#define EIO              5      /* I/O error */
#define ENOMEM          12      /* Out of memory */
#define EACCES          13      /* Permission denied */
#define EFAULT          14      /* Bad address */
#define EBUSY           16      /* Device or resource busy */
#define EEXIST          17      /* File exists */
#define ENODEV          19      /* No such device */
#define EINVAL          22      /* Invalid argument */
#define ENOSPC          28      /* No space left on device */
#define EAGAIN          11      /* Try again */

/* Kernel returns -ENOMEM, -EINVAL, etc. */
```

---

## Version

This guide targets **Linux kernel v3.2**.
