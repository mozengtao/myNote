# Final Mental Model: Error Handling Patterns

## One-Paragraph Summary

Linux kernel error handling uses three main patterns: (1) **goto cleanup** for resource management - allocate forward, cleanup backward via labeled jumps; (2) **ERR_PTR/IS_ERR/PTR_ERR** for pointer functions that need to return specific error codes - encode negative errno in high address bits; (3) **return conventions** - negative values indicate errors for integer functions, NULL or ERR_PTR for pointer functions. These patterns replace exceptions, providing explicit control flow, zero overhead on success paths, and deterministic resource cleanup. The key discipline: always check return values, clean up in reverse allocation order, and propagate errors up the call stack.

**中文总结：**

Linux内核错误处理使用三种主要模式：(1) **goto cleanup**用于资源管理——正向分配，通过标签跳转反向清理；(2) **ERR_PTR/IS_ERR/PTR_ERR**用于需要返回特定错误码的指针函数——将负的errno编码到高地址位；(3) **返回约定**——整数函数用负值表示错误，指针函数用NULL或ERR_PTR。这些模式替代异常，提供显式的控制流、成功路径零开销、确定性的资源清理。核心纪律：始终检查返回值，按分配逆序清理，向上传播错误。

---

## Decision Flowchart

```
+=============================================================================+
|              ERROR HANDLING DECISION FLOWCHART                               |
+=============================================================================+

    CHOOSING RETURN CONVENTION:
    ===========================

                  +------------------------+
                  | What does function     |
                  | return?                |
                  +------------+-----------+
                               |
          +--------------------+--------------------+
          |                    |                    |
          v                    v                    v
      [Integer]           [Pointer]            [Boolean]
          |                    |                    |
          v                    |                    v
    Return -ERRNO              |              Return true/false
    Return 0 on success        |
                               |
          +--------------------+--------------------+
          |                                         |
          v                                         v
    [Only one error           [Multiple error types
     type possible]            or need specific code]
          |                                         |
          v                                         v
    Return NULL               Return ERR_PTR(-ERRNO)
    on error                  Check with IS_ERR()


    CLEANUP PATTERN DECISION:
    =========================

                  +------------------------+
                  | How many resources     |
                  | allocated?             |
                  +------------+-----------+
                               |
          +--------------------+--------------------+
          |                                         |
          v                                         v
     [One resource]                         [Multiple resources]
          |                                         |
          v                                         v
    Simple if-else                          goto cleanup pattern
    cleanup
```

---

## The Three Patterns

```
+=============================================================================+
|              THE THREE ERROR HANDLING PATTERNS                               |
+=============================================================================+

    PATTERN 1: GOTO CLEANUP
    =======================
    
    When to use: Multiple resource allocations
    
    int init(void)
    {
        a = alloc_a();
        if (!a) goto err_a;
        
        b = alloc_b();
        if (!b) goto err_b;
        
        return 0;
        
    err_b:
        free_a(a);
    err_a:
        return -ENOMEM;
    }


    PATTERN 2: ERR_PTR
    ==================
    
    When to use: Pointer function needs specific error codes
    
    void *get_item(int id)
    {
        if (id < 0)
            return ERR_PTR(-EINVAL);
        item = find(id);
        if (!item)
            return ERR_PTR(-ENOENT);
        return item;
    }
    
    /* Caller */
    item = get_item(id);
    if (IS_ERR(item))
        return PTR_ERR(item);


    PATTERN 3: ERROR PROPAGATION
    ============================
    
    When to use: Passing errors up call stack
    
    int high_level(void)
    {
        ret = mid_level();
        if (ret)
            return ret;  /* Propagate */
        return 0;
    }
```

---

## Visual Memory Model

```
+=============================================================================+
|              ERR_PTR MEMORY MODEL                                            |
+=============================================================================+

    Address Space (simplified, 32-bit):
    ===================================

    0x00000000  +------------------+
                |                  |
                |  User Space      |
                |                  |
    0x80000000  +------------------+
                |                  |
                |  Kernel Space    |
                |                  |
                |  Valid pointers  |
                |  are here        |
                |                  |
    0xFFFFF000  +------------------+
                |  ERROR RANGE     |  <-- ERR_PTR values
    0xFFFFFFFF  +------------------+


    ERR_PTR(-ENOMEM) = ERR_PTR(-12) = 0xFFFFFFF4
    ERR_PTR(-EINVAL) = ERR_PTR(-22) = 0xFFFFFFEA
    ERR_PTR(-ENOENT) = ERR_PTR(-2)  = 0xFFFFFFFE
    ERR_PTR(-1)                     = 0xFFFFFFFF

    IS_ERR checks: ptr >= 0xFFFFF001 (MAX_ERRNO = 4095)
    Valid pointers are ALWAYS below this range.
```

---

## Quick Reference Card

```
+=============================================================================+
|              ERROR HANDLING QUICK REFERENCE                                  |
+=============================================================================+

    GOTO CLEANUP:
    -------------
    goto err_X;     Jump to cleanup label
    err_X:          Cleanup entry point (labels fall through)
    return -ERRNO;  Return error code

    ERR_PTR:
    --------
    ERR_PTR(-ERRNO)     Encode error in pointer
    IS_ERR(ptr)         Check if error (true if error)
    PTR_ERR(ptr)        Extract error code
    IS_ERR_OR_NULL(ptr) Check both NULL and error
    ERR_CAST(ptr)       Cast error pointer type

    COMMON ERROR CODES:
    -------------------
    -ENOMEM    Out of memory
    -EINVAL    Invalid argument
    -ENOENT    No such entry
    -EIO       I/O error
    -EPERM     Not permitted
    -EACCES    Access denied
    -EBUSY     Resource busy
    -EAGAIN    Try again
    -EFAULT    Bad address

    RETURN CONVENTIONS:
    -------------------
    int func()    -> Negative=error, 0=success, positive=data
    void *func()  -> NULL=error OR ERR_PTR(-errno)
    bool func()   -> true=condition met, false=not met
```

---

## Common Patterns Summary

| Situation | Pattern | Example |
|-----------|---------|---------|
| **Multiple allocations** | goto cleanup | Driver probe |
| **Pointer with error info** | ERR_PTR | VFS lookup |
| **Simple alloc** | NULL check | kmalloc |
| **Pass error up** | return ret | Most functions |
| **Integer operation** | return -ERRNO | syscalls |

---

## Final Checklist

When writing a function:

- [ ] Do I allocate multiple resources? Use goto cleanup
- [ ] Should pointer convey error type? Use ERR_PTR
- [ ] Am I checking all return values?
- [ ] Is cleanup in reverse allocation order?
- [ ] Am I propagating errors to caller?

When calling a function:

- [ ] Does it return NULL or ERR_PTR? Use correct check
- [ ] Am I handling the error appropriately?
- [ ] Am I cleaning up on error?
- [ ] Am I propagating the error code?
