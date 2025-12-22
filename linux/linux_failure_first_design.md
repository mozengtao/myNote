# Linux Kernel Failure-First Design (v3.2)

## Overview

This document explains how the Linux kernel is designed for **failure-first behavior**, ensuring explicit error handling and robust cleanup patterns.

---

## Why Failure-First Design?

```
+------------------------------------------------------------------+
|  FAILURE-FIRST PHILOSOPHY                                        |
+------------------------------------------------------------------+

    "In systems programming, success is the exception.
     Most code paths must handle failure."
    
    +----------------------------------------------------------+
    | Kernel code faces:                                        |
    | - Memory allocation failures                              |
    | - Hardware errors                                         |
    | - Invalid user input                                      |
    | - Resource exhaustion                                     |
    | - Race conditions                                         |
    | - Permission denials                                      |
    +----------------------------------------------------------+
    
    DESIGN CONSEQUENCE:
    +----------------------------------------------------------+
    | 1. Every function assumes it will fail                    |
    | 2. Every allocation is checked                            |
    | 3. Every resource has explicit cleanup                    |
    | 4. Error paths are first-class citizens                   |
    +----------------------------------------------------------+
```

**中文解释：**
- 故障优先哲学：成功是例外，大多数代码路径必须处理失败
- 内核面临的故障：内存分配失败、硬件错误、无效输入、资源耗尽
- 设计后果：每个函数假设会失败，每个分配都检查，每个资源有显式清理

---

## Probe Error Paths Analysis

### Typical Probe Pattern

```c
static int my_driver_probe(struct platform_device *pdev)
{
    struct my_device *dev;
    struct resource *res;
    int irq, ret;
    
    /* Step 1: Allocate device structure */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;  /* Early return - nothing to clean */
    
    /* Step 2: Get resources */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        ret = -ENODEV;
        goto err_free_dev;  /* Must free dev */
    }
    
    /* Step 3: Map I/O memory */
    dev->regs = ioremap(res->start, resource_size(res));
    if (!dev->regs) {
        ret = -ENOMEM;
        goto err_free_dev;  /* Must free dev */
    }
    
    /* Step 4: Get and request IRQ */
    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        ret = irq;
        goto err_unmap;  /* Must unmap + free dev */
    }
    
    ret = request_irq(irq, my_irq_handler, 0, "my_device", dev);
    if (ret)
        goto err_unmap;  /* Must unmap + free dev */
    
    dev->irq = irq;
    
    /* Step 5: Register with subsystem */
    ret = register_my_device(dev);
    if (ret)
        goto err_free_irq;  /* Must free IRQ + unmap + free dev */
    
    platform_set_drvdata(pdev, dev);
    return 0;  /* SUCCESS */
    
    /* Error cleanup - reverse order of acquisition */
err_free_irq:
    free_irq(irq, dev);
err_unmap:
    iounmap(dev->regs);
err_free_dev:
    kfree(dev);
    return ret;
}
```

```
+------------------------------------------------------------------+
|  PROBE CLEANUP ORDER                                             |
+------------------------------------------------------------------+

    ACQUISITION ORDER:                CLEANUP ORDER (reverse):
    
    1. kzalloc(dev)                   4. kfree(dev)
           |                                 ^
           v                                 |
    2. ioremap(regs)                  3. iounmap(regs)
           |                                 ^
           v                                 |
    3. request_irq(irq)               2. free_irq(irq)
           |                                 ^
           v                                 |
    4. register_device()              1. unregister_device()
    
    
    GOTO LABELS:
    
    probe() {
        alloc dev    ─────────────────────────┐
        if fail: return -ENOMEM               │
                                              │
        map regs     ───────────────────┐     │
        if fail: goto err_free_dev      │     │
                                        │     │
        request irq  ─────────────┐     │     │
        if fail: goto err_unmap   │     │     │
                                  │     │     │
        register     ───────┐     │     │     │
        if fail: goto       │     │     │     │
           err_free_irq     │     │     │     │
                            │     │     │     │
        return 0            │     │     │     │
                            │     │     │     │
    err_free_irq:           │     │     │     │
        free_irq() ←────────┘     │     │     │
    err_unmap:                    │     │     │
        iounmap() ←───────────────┘     │     │
    err_free_dev:                       │     │
        kfree(dev) ←────────────────────┘     │
        return ret                            │
    }                                         │
```

**中文解释：**
- probe 函数获取资源的顺序：alloc → map → IRQ → register
- 清理顺序相反：unregister → free IRQ → unmap → free
- goto 标签按逆序排列，每个跳转到需要清理的位置

---

## goto-Based Cleanup Pattern

### Why goto is Correct Here

```
+------------------------------------------------------------------+
|  WHY goto FOR ERROR HANDLING                                     |
+------------------------------------------------------------------+

    ALTERNATIVE 1: Nested if-else (BAD)
    +----------------------------------------------------------+
    | if (alloc_a()) {                                         |
    |     if (alloc_b()) {                                     |
    |         if (alloc_c()) {                                 |
    |             /* success */                                |
    |         } else {                                         |
    |             free_b();                                    |
    |             free_a();                                    |
    |         }                                                |
    |     } else {                                             |
    |         free_a();                                        |
    |     }                                                    |
    | }                                                        |
    |                                                          |
    | Problems:                                                |
    | - Deep nesting                                           |
    | - Duplicated cleanup code                                |
    | - Hard to read                                           |
    +----------------------------------------------------------+
    
    ALTERNATIVE 2: Flag-based cleanup (BAD)
    +----------------------------------------------------------+
    | bool a_done = false, b_done = false, c_done = false;     |
    | if (alloc_a()) a_done = true; else goto cleanup;         |
    | if (alloc_b()) b_done = true; else goto cleanup;         |
    | if (alloc_c()) c_done = true; else goto cleanup;         |
    | return 0;                                                |
    | cleanup:                                                 |
    | if (c_done) free_c();                                    |
    | if (b_done) free_b();                                    |
    | if (a_done) free_a();                                    |
    |                                                          |
    | Problems:                                                |
    | - Extra variables                                        |
    | - Extra branches at cleanup                              |
    +----------------------------------------------------------+
    
    CORRECT: goto-based (Linux style)
    +----------------------------------------------------------+
    | if (!alloc_a()) goto err_a;                              |
    | if (!alloc_b()) goto err_b;                              |
    | if (!alloc_c()) goto err_c;                              |
    | return 0;                                                |
    | err_c: free_b();                                         |
    | err_b: free_a();                                         |
    | err_a: return error;                                     |
    |                                                          |
    | Benefits:                                                |
    | - Linear code flow                                       |
    | - No duplication                                         |
    | - Clear cleanup order                                    |
    +----------------------------------------------------------+
```

### Real Kernel Example

From `drivers/base/dd.c`:

```c
static int really_probe(struct device *dev, struct device_driver *drv)
{
    int ret = 0;
    
    atomic_inc(&probe_count);
    
    dev->driver = drv;
    
    /* Step 1: Add to sysfs */
    if (driver_sysfs_add(dev)) {
        printk(KERN_ERR "%s: driver_sysfs_add failed\n", __func__);
        goto probe_failed;
    }
    
    /* Step 2: Call bus probe or driver probe */
    if (dev->bus->probe) {
        ret = dev->bus->probe(dev);
        if (ret)
            goto probe_failed;
    } else if (drv->probe) {
        ret = drv->probe(dev);
        if (ret)
            goto probe_failed;
    }
    
    /* Success path */
    driver_bound(dev);
    ret = 1;
    goto done;
    
probe_failed:
    devres_release_all(dev);      /* Release managed resources */
    driver_sysfs_remove(dev);      /* Remove sysfs entries */
    dev->driver = NULL;            /* Clear driver reference */
    ret = 0;
    
done:
    atomic_dec(&probe_count);
    wake_up(&probe_waitqueue);
    return ret;
}
```

**中文解释：**
- goto 用于错误处理的优势：
  1. 线性代码流，无深度嵌套
  2. 无重复清理代码
  3. 清理顺序清晰
- 内核约定：goto 标签按逆序排列，每个标签清理一部分资源

---

## Why Exceptions are Avoided

```
+------------------------------------------------------------------+
|  WHY NO EXCEPTIONS IN KERNEL                                     |
+------------------------------------------------------------------+

    C++ EXCEPTIONS PROBLEMS:
    +----------------------------------------------------------+
    | 1. HIDDEN CONTROL FLOW                                    |
    |    - throw can jump anywhere                             |
    |    - Hard to audit cleanup paths                         |
    |                                                          |
    | 2. STACK UNWINDING COST                                   |
    |    - Exception tables in binary                          |
    |    - Runtime unwinding code                              |
    |    - Memory for exception state                          |
    |                                                          |
    | 3. ATOMIC CONTEXT                                         |
    |    - Cannot throw in IRQ handlers                        |
    |    - Cannot allocate during unwind                       |
    |                                                          |
    | 4. RESOURCE MANAGEMENT                                    |
    |    - No RAII in C                                        |
    |    - Cannot rely on destructors                          |
    +----------------------------------------------------------+
    
    EXPLICIT ERROR HANDLING BENEFITS:
    +----------------------------------------------------------+
    | 1. VISIBLE CONTROL FLOW                                   |
    |    - Every error path explicit in code                   |
    |    - Can audit with grep                                 |
    |                                                          |
    | 2. ZERO OVERHEAD                                          |
    |    - No exception tables                                 |
    |    - No unwinding runtime                                |
    |                                                          |
    | 3. WORKS EVERYWHERE                                       |
    |    - IRQ context                                         |
    |    - Atomic context                                      |
    |    - Early boot                                          |
    |                                                          |
    | 4. EXPLICIT CLEANUP                                       |
    |    - goto labels show exactly what's cleaned             |
    |    - No hidden destructor calls                          |
    +----------------------------------------------------------+
```

**中文解释：**
- 内核不使用异常的原因：
  1. 隐藏控制流，难以审计
  2. 栈展开开销（异常表、运行时代码）
  3. 原子上下文无法抛出异常
  4. C 语言无 RAII，不能依赖析构函数
- 显式错误处理的优势：可见控制流、零开销、任何上下文都可用

---

## Partial Failure Handling

### The Problem

```
+------------------------------------------------------------------+
|  PARTIAL FAILURE SCENARIO                                        |
+------------------------------------------------------------------+

    Operation: Create 5 objects, register all
    
    create(obj1) ✓
    create(obj2) ✓
    create(obj3) ✓
    create(obj4) ✗  FAILURE!
    
    QUESTION: What to do with obj1, obj2, obj3?
    
    WRONG: Leave them allocated (resource leak)
    WRONG: Ignore the failure (inconsistent state)
    
    CORRECT: Rollback - free obj1, obj2, obj3, return error
```

### Rollback Pattern

```c
/* Partial failure with rollback */
int init_subsystem(int count)
{
    struct object **objs;
    int i, ret;
    
    objs = kcalloc(count, sizeof(*objs), GFP_KERNEL);
    if (!objs)
        return -ENOMEM;
    
    /* Create objects */
    for (i = 0; i < count; i++) {
        objs[i] = create_object(i);
        if (!objs[i]) {
            ret = -ENOMEM;
            goto rollback;
        }
    }
    
    /* Register all - only after all created successfully */
    for (i = 0; i < count; i++) {
        ret = register_object(objs[i]);
        if (ret)
            goto unregister_rollback;
    }
    
    subsystem_objs = objs;
    return 0;
    
unregister_rollback:
    /* Unregister what we registered */
    while (--i >= 0)
        unregister_object(objs[i]);
    i = count;  /* Fall through to destroy all */
    
rollback:
    /* Destroy what we created */
    while (--i >= 0)
        destroy_object(objs[i]);
    kfree(objs);
    return ret;
}
```

```
+------------------------------------------------------------------+
|  ROLLBACK VISUALIZATION                                          |
+------------------------------------------------------------------+

    FORWARD PROGRESS:
    
    [create 0] ✓ → [create 1] ✓ → [create 2] ✓ → [create 3] ✗
    
    ROLLBACK ON FAILURE:
    
    [create 3] ✗
         |
         | goto rollback (i = 3)
         v
    [destroy 2] ← [destroy 1] ← [destroy 0]
    
    REGISTRATION ROLLBACK:
    
    [reg 0] ✓ → [reg 1] ✓ → [reg 2] ✗
         |
         | goto unregister_rollback (i = 2)
         v
    [unreg 1] ← [unreg 0]
         |
         v (fall through to rollback)
    [destroy 2] ← [destroy 1] ← [destroy 0]
```

**中文解释：**
- 部分失败场景：创建多个对象时中途失败
- 错误做法：留下已创建的对象（资源泄漏）或忽略失败（状态不一致）
- 正确做法：回滚 — 释放已创建的对象，返回错误
- 回滚使用逆序循环，从失败点向前清理

---

## Cleanup Invariants

```
+------------------------------------------------------------------+
|  CLEANUP INVARIANTS                                              |
+------------------------------------------------------------------+

    INVARIANT 1: Reverse Order
    +----------------------------------------------------------+
    | Cleanup in reverse order of acquisition                  |
    | Why: Dependencies - later resources may depend on earlier|
    |                                                          |
    | Example:                                                 |
    | - IRQ handler uses mapped registers                      |
    | - Must free_irq before iounmap                           |
    +----------------------------------------------------------+
    
    INVARIANT 2: Check Before Free
    +----------------------------------------------------------+
    | Only cleanup what was successfully acquired              |
    | Use NULL checks or flags                                 |
    |                                                          |
    | Example:                                                 |
    | if (dev->regs)                                           |
    |     iounmap(dev->regs);                                  |
    +----------------------------------------------------------+
    
    INVARIANT 3: Idempotent Cleanup
    +----------------------------------------------------------+
    | Cleanup should be safe to call multiple times            |
    | Set pointer to NULL after free                           |
    |                                                          |
    | Example:                                                 |
    | kfree(dev->buffer);                                      |
    | dev->buffer = NULL;  /* Safe if called again */          |
    +----------------------------------------------------------+
    
    INVARIANT 4: No Use After Free
    +----------------------------------------------------------+
    | After cleanup starts, don't access the resource          |
    | Cleanup implies end of lifetime                          |
    +----------------------------------------------------------+
```

### Remove Function Pattern

```c
static int my_driver_remove(struct platform_device *pdev)
{
    struct my_device *dev = platform_get_drvdata(pdev);
    
    /* Reverse order of probe */
    
    /* 1. Unregister from subsystem first (stops new operations) */
    unregister_my_device(dev);
    
    /* 2. Disable and free IRQ (stops async events) */
    free_irq(dev->irq, dev);
    
    /* 3. Unmap I/O memory */
    if (dev->regs)
        iounmap(dev->regs);
    
    /* 4. Free device structure last */
    kfree(dev);
    
    return 0;
}
```

**中文解释：**
- 清理不变量：
  1. 逆序清理（后获取的先释放）
  2. 释放前检查（只清理成功获取的）
  3. 幂等清理（可安全多次调用）
  4. 释放后不使用

---

## User-Space Failure-First Design

```c
/* user_space_failure_first.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/*---------------------------------------------------------
 * Error codes (like kernel -ENOMEM, -EINVAL)
 *---------------------------------------------------------*/
#define E_OK        0
#define E_NOMEM    -1
#define E_IO       -2
#define E_INVALID  -3

/*---------------------------------------------------------
 * Resource structure
 *---------------------------------------------------------*/
struct my_resource {
    int fd;
    char *buffer;
    size_t buf_size;
    void *mapped;
    size_t map_size;
};

/*---------------------------------------------------------
 * goto-based initialization with cleanup
 *---------------------------------------------------------*/
int init_resource(struct my_resource *res, const char *path, size_t size)
{
    int ret;
    
    /* Initialize to NULL/invalid for safe cleanup */
    memset(res, 0, sizeof(*res));
    res->fd = -1;
    
    /* Step 1: Open file */
    res->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (res->fd < 0) {
        ret = E_IO;
        goto err_out;  /* Nothing to clean yet */
    }
    
    /* Step 2: Allocate buffer */
    res->buffer = malloc(size);
    if (!res->buffer) {
        ret = E_NOMEM;
        goto err_close;  /* Must close fd */
    }
    res->buf_size = size;
    
    /* Step 3: Allocate another resource */
    res->mapped = malloc(size * 2);
    if (!res->mapped) {
        ret = E_NOMEM;
        goto err_free_buf;  /* Must free buffer + close fd */
    }
    res->map_size = size * 2;
    
    printf("Resource initialized: fd=%d, buf=%zu, map=%zu\n",
           res->fd, res->buf_size, res->map_size);
    return E_OK;
    
    /* Error cleanup - reverse order */
err_free_buf:
    free(res->buffer);
    res->buffer = NULL;
err_close:
    close(res->fd);
    res->fd = -1;
err_out:
    return ret;
}

/*---------------------------------------------------------
 * Cleanup function (safe for partial initialization)
 *---------------------------------------------------------*/
void cleanup_resource(struct my_resource *res)
{
    /* Check before free - handles partial init */
    if (res->mapped) {
        free(res->mapped);
        res->mapped = NULL;
    }
    
    if (res->buffer) {
        free(res->buffer);
        res->buffer = NULL;
    }
    
    if (res->fd >= 0) {
        close(res->fd);
        res->fd = -1;
    }
    
    printf("Resource cleaned up\n");
}

/*---------------------------------------------------------
 * Partial failure with rollback
 *---------------------------------------------------------*/
struct worker {
    int id;
    char *name;
    int initialized;
};

struct worker *workers = NULL;
int worker_count = 0;

int init_workers(int count)
{
    int i, ret;
    
    workers = calloc(count, sizeof(*workers));
    if (!workers)
        return E_NOMEM;
    
    for (i = 0; i < count; i++) {
        workers[i].id = i;
        workers[i].name = malloc(32);
        if (!workers[i].name) {
            ret = E_NOMEM;
            goto rollback;
        }
        snprintf(workers[i].name, 32, "worker-%d", i);
        workers[i].initialized = 1;
        
        /* Simulate occasional failure */
        if (i == 3) {
            ret = E_IO;
            goto rollback;
        }
    }
    
    worker_count = count;
    printf("Initialized %d workers\n", count);
    return E_OK;
    
rollback:
    /* Free what we allocated */
    printf("Rollback: freeing %d workers\n", i);
    while (--i >= 0) {
        free(workers[i].name);
    }
    free(workers);
    workers = NULL;
    return ret;
}

void cleanup_workers(void)
{
    if (!workers)
        return;
    
    for (int i = 0; i < worker_count; i++) {
        if (workers[i].name)
            free(workers[i].name);
    }
    free(workers);
    workers = NULL;
    worker_count = 0;
    printf("Workers cleaned up\n");
}

/*---------------------------------------------------------
 * Demo
 *---------------------------------------------------------*/
int main(void)
{
    struct my_resource res;
    int ret;
    
    printf("=== Resource Init (success) ===\n");
    ret = init_resource(&res, "/tmp/test_file", 1024);
    if (ret == E_OK) {
        cleanup_resource(&res);
    } else {
        printf("Init failed: %d\n", ret);
    }
    
    printf("\n=== Worker Init (partial failure) ===\n");
    ret = init_workers(5);
    if (ret != E_OK) {
        printf("Worker init failed: %d (rollback completed)\n", ret);
    } else {
        cleanup_workers();
    }
    
    printf("\n=== Cleanup idempotent test ===\n");
    cleanup_resource(&res);  /* Safe to call again */
    cleanup_workers();        /* Safe to call again */
    
    return 0;
}
```

**中文解释：**
- 用户态故障优先设计：
  1. goto-based 初始化和清理
  2. 部分失败回滚
  3. 幂等清理（安全多次调用）
  4. 初始化为 NULL/无效值便于清理

---

## Summary

```
+------------------------------------------------------------------+
|  FAILURE-FIRST DESIGN SUMMARY                                    |
+------------------------------------------------------------------+

    PRINCIPLES:
    +----------------------------------------------------------+
    | 1. ASSUME FAILURE                                         |
    |    Every allocation can fail                              |
    |    Every operation can error                              |
    |    Plan for it from the start                             |
    +----------------------------------------------------------+
    | 2. EXPLICIT CLEANUP                                       |
    |    goto labels for error paths                            |
    |    Reverse order of acquisition                           |
    |    Check before free                                      |
    +----------------------------------------------------------+
    | 3. NO EXCEPTIONS                                          |
    |    Visible control flow                                   |
    |    Works in any context                                   |
    |    Zero hidden overhead                                   |
    +----------------------------------------------------------+
    | 4. ROLLBACK ON PARTIAL FAILURE                            |
    |    Don't leave inconsistent state                         |
    |    Free what was acquired                                 |
    |    Return error code                                      |
    +----------------------------------------------------------+
    
    CLEANUP INVARIANTS:
    +----------------------------------------------------------+
    | - Reverse order (last acquired, first freed)              |
    | - Check before free (handle partial init)                 |
    | - Idempotent (safe to call multiple times)                |
    | - No use after free                                       |
    +----------------------------------------------------------+
    
    CODE PATTERNS:
    +----------------------------------------------------------+
    | Pattern              | When to Use                       |
    |----------------------|-----------------------------------|
    | Early return         | Single allocation, no cleanup     |
    | goto cleanup         | Multiple resources to cleanup     |
    | Rollback loop        | Array of resources                |
    | devres (managed)     | Driver resources with auto-free   |
    +----------------------------------------------------------+
    
    APPLYING TO USER-SPACE:
    +----------------------------------------------------------+
    | 1. Initialize resources to NULL/invalid                   |
    | 2. Use goto for multi-resource cleanup                    |
    | 3. Check pointers before free                             |
    | 4. Set pointers to NULL after free                        |
    | 5. Test error paths as thoroughly as success paths        |
    +----------------------------------------------------------+
```

**中文总结：**
故障优先设计原则：
1. **假设失败**：每个分配和操作都可能失败
2. **显式清理**：goto 标签、逆序释放、释放前检查
3. **不用异常**：可见控制流、任何上下文可用
4. **部分失败回滚**：不留不一致状态

清理不变量：逆序释放、释放前检查、幂等安全、释放后不使用

用户态应用：初始化为无效值、goto 多资源清理、检查后释放、释放后置 NULL

