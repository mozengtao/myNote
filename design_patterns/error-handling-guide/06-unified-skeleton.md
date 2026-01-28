# Unified Skeleton: Error Handling Patterns

## Generic C Skeleton

```c
/*
 * ERROR HANDLING PATTERN - UNIFIED SKELETON
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ==========================================================
 * PART 1: ERROR CODE DEFINITIONS
 * ========================================================== */

/* Common kernel error codes (negated when returned) */
#define EPERM            1      /* Operation not permitted */
#define ENOENT           2      /* No such file or directory */
#define EIO              5      /* I/O error */
#define EAGAIN          11      /* Try again */
#define ENOMEM          12      /* Out of memory */
#define EACCES          13      /* Permission denied */
#define EFAULT          14      /* Bad address */
#define EBUSY           16      /* Device or resource busy */
#define EEXIST          17      /* File exists */
#define ENODEV          19      /* No such device */
#define EINVAL          22      /* Invalid argument */
#define ENOSPC          28      /* No space left on device */
#define ETIMEDOUT      110      /* Connection timed out */


/* ==========================================================
 * PART 2: ERR_PTR MACROS
 * ========================================================== */

#define MAX_ERRNO 4095

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
 * PART 3: GOTO CLEANUP SKELETON
 * ========================================================== */

/**
 * Template for function with multiple resource allocations.
 * 
 * Pattern:
 * 1. Allocate resources in order
 * 2. Check each allocation, goto errN on failure
 * 3. Return success (0)
 * 4. Cleanup labels in reverse order, fall through
 * 5. Return error code
 */
int function_with_cleanup(void /* args */)
{
    int ret;
    void *resource1 = NULL;
    void *resource2 = NULL;
    void *resource3 = NULL;
    
    /* Step 1: First allocation */
    resource1 = allocate_first();
    if (!resource1) {
        ret = -ENOMEM;
        goto err1;
    }
    
    /* Step 2: Second allocation */
    resource2 = allocate_second();
    if (!resource2) {
        ret = -ENOMEM;
        goto err2;
    }
    
    /* Step 3: Third allocation */
    resource3 = allocate_third();
    if (!resource3) {
        ret = -ENOMEM;
        goto err3;
    }
    
    /* Success path */
    return 0;
    
    /* Cleanup path (reverse order) */
err3:
    free_second(resource2);
err2:
    free_first(resource1);
err1:
    return ret;
}


/* ==========================================================
 * PART 4: ERR_PTR FUNCTION SKELETON
 * ========================================================== */

/**
 * Template for pointer-returning function with error codes.
 * 
 * Returns: Valid pointer on success, ERR_PTR(-errno) on error.
 */
void *function_returning_ptr(int arg)
{
    void *result;
    
    /* Validate arguments */
    if (arg < 0)
        return ERR_PTR(-EINVAL);
    
    /* Try operation that might fail */
    result = try_operation(arg);
    if (!result)
        return ERR_PTR(-ENOMEM);
    
    /* More checks */
    if (check_permission_failed())
        return ERR_PTR(-EPERM);
    
    /* Success */
    return result;
}

/**
 * Template for caller of ERR_PTR function.
 */
int caller_of_ptr_function(int arg)
{
    void *ptr;
    
    ptr = function_returning_ptr(arg);
    if (IS_ERR(ptr)) {
        /* Handle specific errors if needed */
        switch (PTR_ERR(ptr)) {
        case -EINVAL:
            /* Handle invalid argument */
            break;
        case -ENOMEM:
            /* Handle out of memory */
            break;
        case -EPERM:
            /* Handle permission denied */
            break;
        }
        return PTR_ERR(ptr);  /* Propagate error */
    }
    
    /* Use ptr */
    use_pointer(ptr);
    
    return 0;
}


/* ==========================================================
 * PART 5: ERROR PROPAGATION SKELETON
 * ========================================================== */

/**
 * Low-level function that can fail.
 */
int low_level_operation(void)
{
    if (hardware_error())
        return -EIO;
    return 0;
}

/**
 * Mid-level function: propagate or handle errors.
 */
int mid_level_operation(void)
{
    int ret;
    
    ret = low_level_operation();
    if (ret) {
        /* Option 1: Just propagate */
        return ret;
        
        /* Option 2: Log and propagate */
        pr_err("low_level failed: %d\n", ret);
        return ret;
        
        /* Option 3: Convert error */
        return -EFAULT;  /* More generic error */
    }
    
    return 0;
}

/**
 * High-level function: final error handling.
 */
int high_level_operation(void)
{
    int ret;
    
    ret = mid_level_operation();
    if (ret) {
        /* Handle error at top level */
        if (ret == -EIO)
            reset_hardware();
        return ret;
    }
    
    return 0;
}


/* ==========================================================
 * PART 6: COMBINED PATTERN EXAMPLE
 * ========================================================== */

/**
 * Complete example combining all patterns.
 */
struct my_device *my_device_create(const char *name)
{
    struct my_device *dev;
    int ret;
    
    /* Validate argument */
    if (!name)
        return ERR_PTR(-EINVAL);
    
    /* Allocate device */
    dev = malloc(sizeof(*dev));
    if (!dev)
        return ERR_PTR(-ENOMEM);
    
    /* Initialize resources */
    dev->buffer = malloc(4096);
    if (!dev->buffer) {
        ret = -ENOMEM;
        goto err_buffer;
    }
    
    dev->lock = create_lock();
    if (!dev->lock) {
        ret = -ENOMEM;
        goto err_lock;
    }
    
    /* Register device */
    ret = register_device(dev);
    if (ret)
        goto err_register;
    
    return dev;
    
err_register:
    destroy_lock(dev->lock);
err_lock:
    free(dev->buffer);
err_buffer:
    free(dev);
    return ERR_PTR(ret);
}
```

---

## Pattern Mapping

```
+=============================================================================+
|              ERROR HANDLING COMPONENTS                                       |
+=============================================================================+

    COMPONENT               |  USAGE
    ========================|===============================================
    goto err_xxx            |  Jump to cleanup on error
    err_xxx: label          |  Cleanup entry point
    return -ERRNO           |  Return negative error code
    ERR_PTR(-ERRNO)         |  Encode error in pointer
    IS_ERR(ptr)             |  Check if pointer is error
    PTR_ERR(ptr)            |  Extract error code from pointer
    
    
    MAPPING TO KERNEL CODE:
    =======================
    
    Pattern              | Where Used
    ---------------------+----------------------------------------------
    goto cleanup         | Driver init, module init, most kernel funcs
    ERR_PTR              | VFS, device model, many pointer-returning APIs
    Negative return      | All syscalls, most kernel APIs
    Boolean return       | Predicate functions (is_xxx, has_xxx)
```

---

## Rules Summary

```
+=============================================================================+
|              ERROR HANDLING RULES                                            |
+=============================================================================+

    1. ALWAYS CHECK RETURN VALUES
       ret = some_function();
       if (ret < 0)
           handle_error();

    2. CLEAN UP IN REVERSE ORDER
       Allocations: A, B, C
       Cleanup: C, B, A (via goto or explicit)

    3. PROPAGATE ERRORS
       Don't swallow errors silently
       Return the error code to caller

    4. USE APPROPRIATE CONVENTION
       - Integer function: return -ERRNO
       - Simple alloc: return NULL
       - Complex alloc: return ERR_PTR(-ERRNO)

    5. CHOOSE SPECIFIC ERROR CODES
       -EINVAL for bad arguments
       -ENOMEM for allocation failure
       -EIO for hardware errors
       etc.
```
