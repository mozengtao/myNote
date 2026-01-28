# Core Concept: Error Handling in Kernel Context

## The Resource Cleanup Problem

```
+=============================================================================+
|                    THE CLEANUP PROBLEM                                       |
+=============================================================================+

    SCENARIO: Multi-step initialization
    ====================================

    int my_device_init(void)
    {
        /* Step 1: Allocate memory */
        dev = kmalloc(sizeof(*dev), GFP_KERNEL);
        
        /* Step 2: Allocate IRQ */
        err = request_irq(irq, handler, 0, "mydev", dev);
        
        /* Step 3: Map I/O memory */
        dev->regs = ioremap(base, size);
        
        /* Step 4: Create sysfs entries */
        err = device_register(&dev->dev);
        
        return 0;
    }


    WHAT IF STEP 3 FAILS?
    =====================

    We have:
    - dev (kmalloc'd)     --> Must kfree
    - IRQ (requested)     --> Must free_irq
    - regs (NOT mapped)   --> Nothing to do
    - sysfs (NOT created) --> Nothing to do


    WITHOUT PROPER HANDLING:
    ========================

    if (!dev->regs) {
        return -ENOMEM;    /* LEAK! dev and IRQ not cleaned up! */
    }
```

**中文说明：**

资源清理问题：多步骤初始化中，如果第3步失败，必须清理第1、2步已分配的资源，但不需要清理第3、4步（因为它们还没执行或已失败）。没有proper handling，直接返回会导致资源泄漏。

---

## Why goto is the Solution

```
+=============================================================================+
|                    WHY GOTO FOR CLEANUP                                      |
+=============================================================================+

    ALTERNATIVE 1: Nested if-else (Bad)
    ====================================

    int init(void)
    {
        a = alloc_a();
        if (a) {
            b = alloc_b();
            if (b) {
                c = alloc_c();
                if (c) {
                    return 0;  /* Success */
                }
                free_b(b);
            }
            free_a(a);
        }
        return -ENOMEM;
    }

    PROBLEMS:
    - Deep nesting (hard to read)
    - Error path mixed with success path
    - Gets worse with more resources


    ALTERNATIVE 2: Multiple returns (Bad)
    =====================================

    int init(void)
    {
        a = alloc_a();
        if (!a) return -ENOMEM;
        
        b = alloc_b();
        if (!b) {
            free_a(a);
            return -ENOMEM;
        }
        
        c = alloc_c();
        if (!c) {
            free_b(b);  /* Must remember ALL prior allocations */
            free_a(a);
            return -ENOMEM;
        }
        
        return 0;
    }

    PROBLEMS:
    - Duplicate cleanup code
    - Easy to miss a cleanup
    - Hard to maintain


    THE SOLUTION: goto cleanup
    ==========================

    int init(void)
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
        
        return 0;
        
    err_c:
        free_b(b);
    err_b:
        free_a(a);
    err_a:
        return -ENOMEM;
    }

    BENEFITS:
    - Linear, readable code
    - One cleanup path
    - Cleanup in reverse order (natural)
    - Easy to maintain
```

**中文说明：**

为什么用goto做清理：(1) 嵌套if-else——深度嵌套难以阅读，错误路径与成功路径混合；(2) 多个return——重复的清理代码，容易遗漏；(3) goto cleanup——线性可读的代码，一个清理路径，按逆序清理（自然），易于维护。Linux内核选择goto cleanup因为它最清晰、最不容易出错。

---

## The ERR_PTR Pattern

```
+=============================================================================+
|                    ERR_PTR / IS_ERR / PTR_ERR                                |
+=============================================================================+

    THE PROBLEM:
    ============

    void *my_alloc(void)
    {
        if (out_of_memory)
            return NULL;        /* Error */
        
        if (permission_denied)
            return NULL;        /* Also error, but different! */
        
        return ptr;             /* Success */
    }

    /* Caller can't distinguish error types! */
    ptr = my_alloc();
    if (!ptr) {
        /* Was it ENOMEM? EPERM? Something else? */
    }


    THE SOLUTION:
    =============

    void *my_alloc(void)
    {
        if (out_of_memory)
            return ERR_PTR(-ENOMEM);    /* Encoded error */
        
        if (permission_denied)
            return ERR_PTR(-EPERM);     /* Different error */
        
        return ptr;                      /* Success */
    }

    ptr = my_alloc();
    if (IS_ERR(ptr)) {
        int err = PTR_ERR(ptr);
        /* err is -ENOMEM, -EPERM, etc. */
    }


    HOW IT WORKS:
    =============

    /* Error codes are small negative numbers (-1 to -4095) */
    /* These map to high addresses: 0xFFFF...FFFF to 0xFFFF...F001 */
    /* Valid kernel pointers are never in this range */

    ERR_PTR(-ENOMEM)  -->  (void *)(-12)  -->  0xFFFFFFF4 (32-bit)
    IS_ERR(ptr)       -->  (unsigned long)ptr > (unsigned long)-4096
    PTR_ERR(ptr)      -->  (long)ptr
```

**中文说明：**

ERR_PTR/IS_ERR/PTR_ERR模式解决的问题：返回NULL无法区分不同的错误类型。解决方案：将错误码编码到指针中。错误码是小的负数（-1到-4095），这些映射到高地址，有效的内核指针永远不会在这个范围内。`ERR_PTR`将错误码转换为指针，`IS_ERR`检查指针是否为错误，`PTR_ERR`从指针提取错误码。

---

## Return Value Conventions

```
+=============================================================================+
|                    KERNEL RETURN VALUE CONVENTIONS                           |
+=============================================================================+

    CONVENTION 1: Integer Functions
    ===============================

    int do_something(void);
    
    Return value:
    - Negative: Error code (-ENOMEM, -EINVAL, etc.)
    - Zero: Success
    - Positive: May have special meaning (bytes written, etc.)
    
    if (do_something() < 0) {
        /* Error */
    }


    CONVENTION 2: Pointer Functions (NULL)
    ======================================

    void *simple_alloc(void);
    
    Return value:
    - NULL: Error (usually ENOMEM implied)
    - Non-NULL: Success
    
    ptr = simple_alloc();
    if (!ptr) {
        /* Error - assume ENOMEM */
    }


    CONVENTION 3: Pointer Functions (ERR_PTR)
    =========================================

    void *complex_alloc(void);
    
    Return value:
    - ERR_PTR(-errno): Error with specific code
    - Valid pointer: Success
    
    ptr = complex_alloc();
    if (IS_ERR(ptr)) {
        err = PTR_ERR(ptr);
        /* err tells us what went wrong */
    }


    CONVENTION 4: Boolean (rare)
    ============================

    bool is_valid(x);
    
    Return value:
    - true: Condition met
    - false: Condition not met
```

---

## Error Propagation

```c
/*
 * Pattern: Propagate errors up the call stack
 */

int low_level_func(void)
{
    if (error)
        return -EIO;
    return 0;
}

int mid_level_func(void)
{
    int ret;
    
    ret = low_level_func();
    if (ret)
        return ret;  /* Propagate error */
    
    /* Continue on success */
    return 0;
}

int high_level_func(void)
{
    int ret;
    
    ret = mid_level_func();
    if (ret) {
        pr_err("mid_level_func failed: %d\n", ret);
        return ret;  /* Propagate again */
    }
    
    return 0;
}
```

---

## Summary

The kernel uses three main error handling patterns:

1. **goto cleanup**: For resource cleanup on failure
2. **ERR_PTR/IS_ERR/PTR_ERR**: For pointer functions with error codes
3. **Return conventions**: Negative=error, zero=success for integers

These patterns work together to provide robust, efficient error handling without exceptions.
