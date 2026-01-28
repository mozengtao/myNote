# Identification Rules for Error Handling Patterns

## Structural Signals

```
+=============================================================================+
|                    ERROR HANDLING PATTERN ANATOMY                            |
+=============================================================================+

    GOTO CLEANUP PATTERN:
    =====================

    int function(void)
    {
        /* Allocation section */
        res1 = alloc1();
        if (!res1)
            goto err1;            <-- Jump to cleanup
        
        res2 = alloc2();
        if (!res2)
            goto err2;            <-- Jump to cleanup
        
        return 0;                 <-- Success path
        
        /* Cleanup section */
    err2:                         <-- Labels in reverse order
        free1(res1);
    err1:
        return -ENOMEM;           <-- Return error code
    }


    ERR_PTR PATTERN:
    ================

    void *alloc_func(void)
    {
        if (error_condition)
            return ERR_PTR(-ENOMEM);  <-- Error encoded in pointer
        return valid_pointer;
    }

    /* Caller */
    ptr = alloc_func();
    if (IS_ERR(ptr)) {               <-- Check for error
        err = PTR_ERR(ptr);          <-- Extract error code
        return err;
    }
```

**中文说明：**

错误处理模式的结构特征：(1) goto cleanup模式——分配部分使用goto跳转到清理标签，清理标签按逆序排列，返回错误码；(2) ERR_PTR模式——用ERR_PTR将错误码编码到指针中，调用者用IS_ERR检查、用PTR_ERR提取。

---

## The Five Identification Rules

### Rule 1: Look for goto with err_ Labels

```c
/* GOTO CLEANUP PATTERN: err_xxx labels */

int my_function(void)
{
    /* ... */
    if (error)
        goto err_something;      /* <-- GOTO CLEANUP */
    /* ... */

err_something:                   /* <-- CLEANUP LABEL */
    cleanup();
    return -ERRNO;
}

/* Common label naming: */
err_xxx          /* Generic error */
fail_xxx         /* Failure cleanup */
out_xxx          /* Exit/cleanup point */
error_xxx        /* Error cleanup */
```

### Rule 2: Look for IS_ERR / PTR_ERR / ERR_PTR

```c
/* ERR_PTR PATTERN: Error encoding in pointers */

/* Creating error pointer */
return ERR_PTR(-ENOMEM);         /* <-- ERR_PTR */
return ERR_PTR(-EINVAL);
return ERR_PTR(ret);             /* ret is negative */

/* Checking error pointer */
if (IS_ERR(ptr))                 /* <-- IS_ERR */
    return PTR_ERR(ptr);         /* <-- PTR_ERR */

/* Also: IS_ERR_OR_NULL for both NULL and error check */
if (IS_ERR_OR_NULL(ptr))
    /* Handle NULL or error */
```

**中文说明：**

规则2：寻找IS_ERR/PTR_ERR/ERR_PTR。这些宏用于将错误码编码到指针中。`ERR_PTR`创建错误指针，`IS_ERR`检查是否为错误指针，`PTR_ERR`提取错误码。还有`IS_ERR_OR_NULL`同时检查NULL和错误。

### Rule 3: Check for Negative Return Values

```c
/* RETURN CONVENTION: Negative means error */

/* Function that can fail */
int do_operation(void);

/* Checking for error */
ret = do_operation();
if (ret < 0)                     /* <-- Negative check */
    return ret;                  /* Propagate error */

if (ret)                         /* Shorthand: non-zero is error */
    goto err;

/* Common error codes */
-ENOMEM    /* Out of memory */
-EINVAL    /* Invalid argument */
-ENOENT    /* No such entry */
-EBUSY     /* Resource busy */
-EIO       /* I/O error */
-EPERM     /* Permission denied */
```

### Rule 4: Look for Cleanup in Reverse Order

```c
/* REVERSE ORDER CLEANUP */

int init(void)
{
    /* Forward allocation */
    a = alloc_a();  /* Step 1 */
    b = alloc_b();  /* Step 2 */
    c = alloc_c();  /* Step 3 */
    
    return 0;
    
    /* Reverse cleanup */
err_c:
    free_b(b);      /* Undo step 2 */
err_b:
    free_a(a);      /* Undo step 1 */
err_a:
    return -ENOMEM;
}

/* Labels fall through in reverse order of allocation */
```

### Rule 5: Look for Single Exit Point Pattern

```c
/* SINGLE EXIT POINT (variant of goto cleanup) */

int function(void)
{
    int ret = 0;
    
    res = alloc();
    if (!res) {
        ret = -ENOMEM;
        goto out;                /* <-- Single exit */
    }
    
    /* More operations... */
    
out:                             /* <-- Single cleanup point */
    if (res)
        free(res);
    return ret;
}
```

---

## Summary Checklist

```
+=============================================================================+
|                    ERROR HANDLING IDENTIFICATION CHECKLIST                   |
+=============================================================================+

    When examining code, check:

    [ ] 1. GOTO WITH ERR_ LABELS
        goto err_xxx; with err_xxx: label later in function
        Fall-through cleanup in reverse order

    [ ] 2. ERR_PTR / IS_ERR / PTR_ERR
        return ERR_PTR(-EXXX);
        if (IS_ERR(ptr)) { PTR_ERR(ptr); }

    [ ] 3. NEGATIVE RETURN CHECK
        if (ret < 0) or if (ret)
        Return value is error code

    [ ] 4. REVERSE ORDER CLEANUP
        Allocations go forward, cleanups go backward
        Labels cascade through cleanup

    [ ] 5. SINGLE EXIT POINT
        One 'out:' or 'exit:' label
        All paths converge to cleanup

    SCORING:
    2+ indicators = Definitely error handling pattern
    1 indicator   = Likely error handling pattern
    0 indicators  = Not using standard patterns
```

---

## Red Flags: Anti-Patterns

```
    NOT GOOD ERROR HANDLING:

    1. MISSING CLEANUP
       if (!alloc())
           return -ENOMEM;    /* No cleanup of prior allocations! */

    2. WRONG ORDER CLEANUP
       err2:
           free_b(b);         /* Should be after err3 */
       err3:
           free_a(a);         /* Wrong order! */

    3. NULL CHECK AFTER ERR_PTR
       ptr = func_returning_err_ptr();
       if (!ptr)              /* Wrong! Should be IS_ERR(ptr) */
           return -ENOMEM;

    4. USING PTR AFTER IS_ERR TRUE
       if (IS_ERR(ptr))
           use(ptr);          /* BUG! ptr is not valid */

    5. IGNORING RETURN VALUE
       do_something();        /* Error ignored! */
```

---

## Quick Reference: Error Macros

| Macro | Usage | Purpose |
|-------|-------|---------|
| `ERR_PTR(err)` | `return ERR_PTR(-ENOMEM);` | Encode error in pointer |
| `PTR_ERR(ptr)` | `err = PTR_ERR(ptr);` | Extract error from pointer |
| `IS_ERR(ptr)` | `if (IS_ERR(ptr))` | Check if pointer is error |
| `IS_ERR_OR_NULL(ptr)` | `if (IS_ERR_OR_NULL(ptr))` | Check NULL or error |
| `ERR_CAST(ptr)` | `return ERR_CAST(ptr);` | Cast error pointer type |
