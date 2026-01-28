# Anti-Patterns: What NOT to Do with Error Handling

## Anti-Pattern 1: Missing Cleanup

```c
/*
 * ANTI-PATTERN: Not cleaning up on error
 * ======================================
 */

int bad_init(void)
{
    a = alloc_a();
    if (!a)
        return -ENOMEM;
    
    b = alloc_b();
    if (!b)
        return -ENOMEM;  /* LEAK! 'a' not freed */
    
    c = alloc_c();
    if (!c)
        return -ENOMEM;  /* LEAK! 'a' and 'b' not freed */
    
    return 0;
}


/*
 * CORRECT: Clean up all prior allocations
 */
int good_init(void)
{
    a = alloc_a();
    if (!a)
        return -ENOMEM;
    
    b = alloc_b();
    if (!b)
        goto err_b;
    
    c = alloc_c();
    if (!c)
        goto err_c;
    
    return 0;
    
err_c:
    free_b(b);
err_b:
    free_a(a);
    return -ENOMEM;
}
```

**中文说明：**

反模式1：错误时不清理。如果分配b失败后直接返回而不释放a，会导致内存泄漏。正确做法：使用goto cleanup模式，按逆序释放所有之前分配的资源。

---

## Anti-Pattern 2: Wrong Cleanup Order

```c
/*
 * ANTI-PATTERN: Cleanup in wrong order
 * ====================================
 */

int bad_order(void)
{
    a = alloc_a();
    if (!a)
        goto err_a;
    
    b = alloc_b_using_a(a);  /* b depends on a */
    if (!b)
        goto err_b;
    
    return 0;
    
err_a:
    free_b(b);  /* WRONG: b not allocated yet! */
err_b:
    free_a(a);  /* WRONG: Can't free a before b! */
    return -ENOMEM;
}


/*
 * CORRECT: Cleanup in reverse allocation order
 */
int good_order(void)
{
    a = alloc_a();
    if (!a)
        goto err_a;
    
    b = alloc_b_using_a(a);
    if (!b)
        goto err_b;
    
    return 0;
    
err_b:
    free_a(a);  /* Free a (b never allocated) */
err_a:
    return -ENOMEM;
}
```

---

## Anti-Pattern 3: Checking NULL After ERR_PTR

```c
/*
 * ANTI-PATTERN: Using NULL check for ERR_PTR function
 * ===================================================
 */

void *function_returns_err_ptr(void);

int bad_caller(void)
{
    void *ptr = function_returns_err_ptr();
    
    /* WRONG: ERR_PTR is not NULL! */
    if (!ptr) {
        return -ENOMEM;  /* This path never taken for ERR_PTR */
    }
    
    /* ptr might be ERR_PTR(-ENOMEM), not a valid pointer! */
    use(ptr);  /* CRASH or corruption */
}


/*
 * CORRECT: Use IS_ERR for ERR_PTR functions
 */
int good_caller(void)
{
    void *ptr = function_returns_err_ptr();
    
    if (IS_ERR(ptr)) {
        return PTR_ERR(ptr);
    }
    
    use(ptr);
    return 0;
}
```

**中文说明：**

反模式3：对返回ERR_PTR的函数使用NULL检查。ERR_PTR返回的是高地址指针，不是NULL，所以`if (!ptr)`永远不会为真。必须使用`IS_ERR(ptr)`检查。

---

## Anti-Pattern 4: Using Pointer After IS_ERR

```c
/*
 * ANTI-PATTERN: Using pointer when IS_ERR is true
 * ===============================================
 */

int very_bad(void)
{
    void *ptr = get_something();
    
    if (IS_ERR(ptr)) {
        /* ptr is NOT a valid pointer here! */
        log("Error with %s", ptr->name);  /* CRASH! */
        free(ptr);  /* CRASH! */
    }
}


/*
 * CORRECT: Only use pointer when NOT IS_ERR
 */
int correct(void)
{
    void *ptr = get_something();
    
    if (IS_ERR(ptr)) {
        /* Only use PTR_ERR to get error code */
        int err = PTR_ERR(ptr);
        log("Error: %d", err);
        return err;
    }
    
    /* Now safe to use ptr */
    use(ptr);
    return 0;
}
```

---

## Anti-Pattern 5: Ignoring Return Values

```c
/*
 * ANTI-PATTERN: Ignoring error returns
 * ====================================
 */

void careless_function(void)
{
    /* Error ignored! */
    do_something();
    
    /* Also ignored! */
    allocate_resource();
    
    /* Assuming success without checking */
    use_resource();
}


/*
 * CORRECT: Always check return values
 */
int careful_function(void)
{
    int ret;
    
    ret = do_something();
    if (ret)
        return ret;
    
    ret = allocate_resource();
    if (ret)
        return ret;
    
    use_resource();
    return 0;
}
```

---

## Anti-Pattern 6: Error Code Confusion

```c
/*
 * ANTI-PATTERN: Confusing error conventions
 * =========================================
 */

int confused_function(void)
{
    void *ptr = kmalloc(size, GFP_KERNEL);
    
    /* WRONG: kmalloc returns NULL on error, not ERR_PTR */
    if (IS_ERR(ptr))  /* Always false for kmalloc! */
        return PTR_ERR(ptr);
    
    /* ptr might be NULL here, causing crash later */
}


/*
 * CORRECT: Know which convention each function uses
 */
int correct_function(void)
{
    void *ptr = kmalloc(size, GFP_KERNEL);
    
    /* kmalloc returns NULL on error */
    if (!ptr)
        return -ENOMEM;
    
    /* ERR_PTR function */
    struct dentry *dentry = lookup_dentry(...);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);
}
```

**中文说明：**

反模式6：混淆错误约定。`kmalloc`返回NULL表示错误，而`lookup_dentry`返回ERR_PTR表示错误。必须知道每个函数使用哪种约定，使用正确的检查方式。

---

## Anti-Pattern 7: Creating Spaghetti Cleanup

```c
/*
 * ANTI-PATTERN: Duplicate cleanup code
 * ====================================
 */

int spaghetti(void)
{
    a = alloc_a();
    if (!a)
        return -ENOMEM;
    
    b = alloc_b();
    if (!b) {
        free_a(a);
        return -ENOMEM;
    }
    
    c = alloc_c();
    if (!c) {
        free_b(b);
        free_a(a);
        return -ENOMEM;
    }
    
    d = alloc_d();
    if (!d) {
        free_c(c);
        free_b(b);
        free_a(a);
        return -ENOMEM;
    }
    
    /* Lots of duplicate code! */
}


/*
 * CORRECT: Single cleanup path with goto
 */
int clean(void)
{
    a = alloc_a();
    if (!a)
        goto err_a;
    
    b = alloc_b();
    if (!b)
        goto err_b;
    
    c = alloc_c();
    if (!c)
        goto err_c;
    
    d = alloc_d();
    if (!d)
        goto err_d;
    
    return 0;
    
err_d:
    free_c(c);
err_c:
    free_b(b);
err_b:
    free_a(a);
err_a:
    return -ENOMEM;
}
```

---

## Summary: Error Handling Rules

```
+=============================================================================+
|              ERROR HANDLING RULES                                            |
+=============================================================================+

    1. ALWAYS CLEANUP ON ERROR
       Every allocation must have corresponding cleanup on error path.

    2. CLEANUP IN REVERSE ORDER
       Free resources in opposite order of allocation.

    3. KNOW YOUR CONVENTIONS
       - kmalloc: NULL = error
       - ERR_PTR functions: IS_ERR() = error
       - Integer functions: negative = error

    4. NEVER USE ERR_PTR AS VALID POINTER
       After IS_ERR() is true, only use PTR_ERR().

    5. CHECK ALL RETURN VALUES
       Don't assume success.

    6. USE GOTO FOR CLEANUP
       Single cleanup path, no duplicate code.

    7. PROPAGATE ERRORS
       Return original error code to caller.
```
