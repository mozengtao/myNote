# Error Handling vs Exceptions

## Fundamental Comparison

```
+=============================================================================+
|              KERNEL ERROR HANDLING vs EXCEPTIONS                             |
+=============================================================================+

    EXCEPTIONS (C++/Java):                 ERROR HANDLING (Kernel):
    ======================                 =======================

    try {                                  int init(void) {
        a = allocA();                          a = alloc_a();
        b = allocB();                          if (!a) goto err_a;
        c = allocC();                          b = alloc_b();
    } catch (exception& e) {                   if (!b) goto err_b;
        // Automatic cleanup                   ...
        // of a, b, c                      err_b:
    }                                          free_a(a);
                                           err_a:
    Stack automatically                        return -ENOMEM;
    unwound                                }

                                           Manual cleanup via goto


    EXCEPTION FEATURES:                    KERNEL ERROR FEATURES:
    ===================                    =====================

    - Automatic stack unwinding            - Explicit cleanup code
    - Object destructors called            - Manual resource release
    - Type-based catch                     - Error code checking
    - Can throw from anywhere              - Return at defined points
    - Runtime type information             - No RTTI overhead
```

**中文说明：**

异常与内核错误处理的对比：异常有自动栈展开、自动调用析构函数、基于类型的catch、可以从任何地方抛出、需要运行时类型信息。内核错误处理有显式的清理代码、手动资源释放、错误码检查、在定义点返回、无RTTI开销。

---

## Why Kernel Doesn't Use Exceptions

### Reason 1: C Language

```c
/*
 * Linux kernel is written in C, not C++.
 * C has no built-in exception support.
 * 
 * Would need:
 * - Compiler support for try/catch
 * - Runtime library for exception handling
 * - Changes to all existing code
 */

/* This is NOT valid C: */
try {
    do_something();
} catch (int error) {
    handle_error();
}

/* Kernel uses standard C: */
int ret = do_something();
if (ret < 0) {
    handle_error(ret);
}
```

### Reason 2: Performance

```
    EXCEPTION OVERHEAD:
    ===================

    Every function needs:
    - Exception tables (for unwinding)
    - Stack frame metadata
    - Cleanup handlers registered
    
    On throw:
    - Walk up the stack
    - Find matching handler
    - Execute cleanup handlers
    - May involve memory allocation
    
    Even when no exception:
    - Code size increased
    - Potential cache impact


    GOTO CLEANUP OVERHEAD:
    ======================

    Every function needs:
    - Nothing extra

    On error:
    - Single jump instruction
    - Execute cleanup code
    - Return error code

    No error:
    - No overhead at all


    IN KERNEL (millions of ops/sec):
    Exception handling would be too expensive.
```

**中文说明：**

性能原因：异常需要异常表、栈帧元数据、注册清理处理器；抛出时需要遍历栈、查找处理器、执行清理、可能涉及内存分配。即使没有异常，代码大小也会增加。goto cleanup几乎没有开销：错误时只是一条跳转指令，没有错误时完全没有开销。在内核中（每秒数百万次操作），异常处理太昂贵了。

### Reason 3: Predictability

```c
/*
 * Kernel needs predictable behavior.
 * Exceptions can come from anywhere.
 */

/* With exceptions (C++): */
void function() {
    lock.acquire();
    
    do_something();  /* Might throw! */
    
    lock.release();  /* Never reached if exception thrown! */
}

/* Kernel code must be explicit: */
int function(void) {
    spin_lock(&lock);
    
    ret = do_something();
    if (ret < 0) {
        spin_unlock(&lock);  /* Explicit unlock */
        return ret;
    }
    
    spin_unlock(&lock);
    return 0;
}

/* Or with goto: */
int function(void) {
    spin_lock(&lock);
    
    ret = do_something();
    if (ret < 0)
        goto out;
    
    ret = 0;
out:
    spin_unlock(&lock);  /* Always executed */
    return ret;
}
```

### Reason 4: No RTTI

```
    Exception handling often requires:
    - Runtime Type Information (RTTI)
    - Type checking at catch site
    - Dynamic_cast-like operations
    
    Kernel:
    - No RTTI (too much overhead)
    - No dynamic_cast
    - All types known at compile time
    - Error codes are simple integers
```

---

## What Kernel DOES Have

### setjmp/longjmp (Not Used in Normal Code)

```c
/*
 * C provides setjmp/longjmp for non-local jumps.
 * But kernel DOES NOT use them for error handling.
 * 
 * Problems:
 * - No automatic cleanup
 * - Easy to misuse
 * - Hard to follow control flow
 * - Not interrupt-safe
 */

/* This is NOT kernel style: */
jmp_buf env;

int outer_func(void) {
    if (setjmp(env) != 0) {
        /* Error recovery */
        return -1;
    }
    inner_func();
    return 0;
}

void inner_func(void) {
    if (error)
        longjmp(env, 1);  /* Jump back to setjmp */
    /* Resources leaked! */
}
```

### Oops/Panic (Not for Normal Errors)

```c
/*
 * Kernel has panic() and BUG() for unrecoverable errors.
 * These are NOT exception handlers.
 * They halt the system.
 */

void panic(const char *fmt, ...);  /* System dies */
BUG();                             /* Oops, usually fatal */
BUG_ON(condition);                 /* Assert + BUG */
WARN_ON(condition);                /* Warning, continues */

/*
 * Use for:
 * - Kernel bugs (should never happen)
 * - Unrecoverable hardware errors
 * - Consistency check failures
 * 
 * NOT for:
 * - Normal errors (use return codes)
 * - User-triggered conditions
 * - Recoverable situations
 */
```

---

## Comparison Summary

| Aspect | Exceptions | Kernel Error Handling |
|--------|------------|----------------------|
| **Language** | C++, Java, etc. | C |
| **Cleanup** | Automatic (RAII) | Manual (goto) |
| **Overhead** | Always present | Only on error path |
| **Control flow** | Non-local jumps | Local returns |
| **Type info** | Required (RTTI) | Not needed |
| **Predictability** | Can throw anywhere | Explicit error points |
| **Learning curve** | Complex | Simple |

---

## When Each Makes Sense

```
+=============================================================================+
|              CHOOSING ERROR HANDLING APPROACH                                |
+=============================================================================+

    USE EXCEPTIONS WHEN:
    ====================
    
    - Writing application code
    - Using C++/Java/Python
    - RAII can handle cleanup
    - Performance is not critical
    - Want simpler error propagation


    USE KERNEL-STYLE WHEN:
    ======================
    
    - Writing in C
    - Performance critical
    - Need explicit control
    - Writing system/kernel code
    - Want predictable behavior
    - Resources are non-memory (locks, I/O)


    HYBRID APPROACHES:
    ==================
    
    C++ Kernel (not Linux):
    - Can use RAII for cleanup
    - Don't throw in critical paths
    - Use error codes for performance
    
    User-space C with cleanup:
    - Use goto cleanup pattern
    - Or cleanup stack with macros
    - Consider pthread_cleanup_push
```

---

## Key Takeaways

1. **C has no exceptions**: Kernel is written in C
2. **Performance matters**: Exceptions have runtime overhead
3. **Explicit is better**: Kernel prefers explicit error handling
4. **goto is not evil**: In cleanup context, it's clean and efficient
5. **Error codes are sufficient**: Simple, fast, no RTTI needed
