# Linux Kernel Error Handling Patterns (v3.2)

## Overview

This document explains **failure-handling patterns** in Linux kernel v3.2, focusing on failure-first design and structured cleanup.

---

## Probe Error Path Analysis

```
+------------------------------------------------------------------+
|  DRIVER PROBE PATTERN                                            |
+------------------------------------------------------------------+

    PROBE FUNCTION STRUCTURE:
    
    static int my_probe(struct platform_device *pdev)
    {
        struct my_device *dev;
        int ret;
        
        /* Step 1: Allocate device structure */
        dev = kzalloc(sizeof(*dev), GFP_KERNEL);
        if (!dev)
            return -ENOMEM;  /* Nothing to cleanup yet */
        
        /* Step 2: Get resources */
        dev->clk = clk_get(&pdev->dev, NULL);
        if (IS_ERR(dev->clk)) {
            ret = PTR_ERR(dev->clk);
            goto err_free_dev;  /* Must free dev */
        }
        
        /* Step 3: Enable clock */
        ret = clk_enable(dev->clk);
        if (ret)
            goto err_put_clk;  /* Must put clk, free dev */
        
        /* Step 4: Map I/O memory */
        dev->regs = ioremap(res->start, size);
        if (!dev->regs) {
            ret = -ENOMEM;
            goto err_disable_clk;  /* Must disable clk, put, free */
        }
        
        /* Step 5: Request IRQ */
        ret = request_irq(irq, my_handler, 0, "my_dev", dev);
        if (ret)
            goto err_unmap;  /* Must unmap, disable, put, free */
        
        /* Step 6: Register with subsystem */
        ret = register_device(dev);
        if (ret)
            goto err_free_irq;  /* Must free irq, unmap, ... */
        
        platform_set_drvdata(pdev, dev);
        return 0;  /* SUCCESS */
        
        /* Error cleanup - REVERSE ORDER */
    err_free_irq:
        free_irq(irq, dev);
    err_unmap:
        iounmap(dev->regs);
    err_disable_clk:
        clk_disable(dev->clk);
    err_put_clk:
        clk_put(dev->clk);
    err_free_dev:
        kfree(dev);
        return ret;
    }
```

```
+------------------------------------------------------------------+
|  ACQUISITION vs CLEANUP ORDER                                    |
+------------------------------------------------------------------+

    ACQUISITION (forward)         CLEANUP (reverse)
    ─────────────────────         ────────────────────
    
    1. kzalloc(dev)          ──▶  6. kfree(dev)
           │                            ▲
           ▼                            │
    2. clk_get()             ──▶  5. clk_put()
           │                            ▲
           ▼                            │
    3. clk_enable()          ──▶  4. clk_disable()
           │                            ▲
           ▼                            │
    4. ioremap()             ──▶  3. iounmap()
           │                            ▲
           ▼                            │
    5. request_irq()         ──▶  2. free_irq()
           │                            ▲
           ▼                            │
    6. register_device()     ──▶  1. unregister_device()
    
    WHY REVERSE ORDER:
    +----------------------------------------------------------+
    | Later resources may depend on earlier ones               |
    | - IRQ handler uses mapped registers                      |
    | - Must free IRQ before unmapping registers               |
    +----------------------------------------------------------+
```

**中文解释：**
- probe 函数结构：顺序获取资源，失败时跳转到对应清理标签
- 获取 vs 清理顺序：清理与获取顺序相反
- 原因：后获取的资源可能依赖先获取的

---

## goto Cleanup Structure

```
+------------------------------------------------------------------+
|  GOTO-BASED CLEANUP PATTERN                                      |
+------------------------------------------------------------------+

    WHY GOTO:
    +----------------------------------------------------------+
    | - Single exit point for error handling                    |
    | - No code duplication                                     |
    | - Clear cleanup order                                     |
    | - Compiler optimizes well                                 |
    +----------------------------------------------------------+

    CORRECT PATTERN:
    
    int my_function(void)
    {
        int ret;
        struct resource_a *a = NULL;
        struct resource_b *b = NULL;
        struct resource_c *c = NULL;
        
        a = alloc_a();
        if (!a) {
            ret = -ENOMEM;
            goto out;  /* Nothing to cleanup */
        }
        
        b = alloc_b();
        if (!b) {
            ret = -ENOMEM;
            goto out_free_a;
        }
        
        c = alloc_c();
        if (!c) {
            ret = -ENOMEM;
            goto out_free_b;
        }
        
        /* Use resources... */
        ret = 0;
        
        /* Normal cleanup path (same as error path) */
    out_free_c:
        free_c(c);
    out_free_b:
        free_b(b);
    out_free_a:
        free_a(a);
    out:
        return ret;
    }

    BAD PATTERNS:
    
    /* BAD: Duplicate cleanup code */
    if (!a) {
        return -ENOMEM;
    }
    if (!b) {
        free_a(a);      /* Duplicated! */
        return -ENOMEM;
    }
    if (!c) {
        free_b(b);      /* Duplicated! */
        free_a(a);      /* Duplicated! */
        return -ENOMEM;
    }
    
    /* BAD: Deeply nested if-else */
    if (a) {
        if (b) {
            if (c) {
                /* 10 levels deep... */
            } else { /* cleanup */ }
        } else { /* cleanup */ }
    }
```

**中文解释：**
- 为什么用 goto：单一退出点、无代码重复、清晰清理顺序
- 正确模式：失败时跳转到对应清理标签，标签按逆序排列
- 错误模式：重复清理代码、深度嵌套 if-else

---

## Partial Initialization Rollback

```
+------------------------------------------------------------------+
|  PARTIAL INIT ROLLBACK                                           |
+------------------------------------------------------------------+

    SCENARIO: Initialize array of objects
    
    int init_objects(struct object **objs, int count)
    {
        int i, ret;
        
        for (i = 0; i < count; i++) {
            objs[i] = alloc_object(i);
            if (!objs[i]) {
                ret = -ENOMEM;
                goto rollback;
            }
            
            ret = init_object(objs[i]);
            if (ret) {
                free_object(objs[i]);
                goto rollback;
            }
        }
        return 0;
        
    rollback:
        /* Free only what was successfully allocated */
        while (--i >= 0) {
            cleanup_object(objs[i]);
            free_object(objs[i]);
        }
        return ret;
    }

    TIMELINE:
    
    i=0: alloc ✓ → init ✓
    i=1: alloc ✓ → init ✓
    i=2: alloc ✓ → init ✗  ← FAILURE
         │
         │ rollback (i starts at 2)
         ▼
    --i → i=1: cleanup ✓, free ✓
    --i → i=0: cleanup ✓, free ✓
    --i → i=-1: loop exits
    
    return error

    INVARIANT:
    +----------------------------------------------------------+
    | At any failure point:                                     |
    | - Objects 0..(i-1) are fully initialized                  |
    | - Object i may be partially initialized                   |
    | - Objects (i+1)..n are untouched                          |
    +----------------------------------------------------------+
```

**中文解释：**
- 部分初始化回滚：初始化数组时失败，只释放已成功分配的
- 回滚循环：while (--i >= 0) 逆序释放
- 不变量：0..(i-1) 完全初始化，i 部分初始化，(i+1)..n 未触及

---

## Error Handling Invariants

```
+------------------------------------------------------------------+
|  ERROR HANDLING INVARIANTS                                       |
+------------------------------------------------------------------+

    INVARIANT 1: Check Every Return Value
    +----------------------------------------------------------+
    | if (ret) { handle error }                                 |
    | if (!ptr) { handle null }                                 |
    | if (IS_ERR(ptr)) { handle error pointer }                 |
    +----------------------------------------------------------+

    INVARIANT 2: Clean Up What You Acquire
    +----------------------------------------------------------+
    | Every alloc has a matching free                           |
    | Every get has a matching put                              |
    | Every lock has a matching unlock                          |
    +----------------------------------------------------------+

    INVARIANT 3: Cleanup in Reverse Order
    +----------------------------------------------------------+
    | Last acquired = first released                            |
    | Respects dependencies                                     |
    +----------------------------------------------------------+

    INVARIANT 4: Don't Use After Error
    +----------------------------------------------------------+
    | If operation fails, treat result as invalid               |
    | Don't try to "fix" failed state                           |
    +----------------------------------------------------------+

    INVARIANT 5: Propagate Errors
    +----------------------------------------------------------+
    | If sub-function fails, caller should fail                 |
    | Don't swallow errors silently                             |
    +----------------------------------------------------------+

    ERROR RETURN VALUES:
    +----------------------------------------------------------+
    | 0         = success                                       |
    | negative  = errno (e.g., -ENOMEM, -EINVAL)                |
    | NULL      = allocation failure                            |
    | IS_ERR()  = error encoded in pointer                      |
    +----------------------------------------------------------+
```

**中文解释：**
- 不变量1：检查每个返回值
- 不变量2：获取什么就清理什么
- 不变量3：逆序清理
- 不变量4：错误后不使用
- 不变量5：传播错误

---

## User-Space Robust Library

```c
/* User-space error handling patterns */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*=================================================================
 * PATTERN 1: Error codes (not exceptions)
 *================================================================*/
typedef enum {
    ERR_OK = 0,
    ERR_NOMEM = -1,
    ERR_INVALID = -2,
    ERR_IO = -3,
    ERR_BUSY = -4,
} error_t;

const char *error_string(error_t err)
{
    switch (err) {
        case ERR_OK:      return "Success";
        case ERR_NOMEM:   return "Out of memory";
        case ERR_INVALID: return "Invalid argument";
        case ERR_IO:      return "I/O error";
        case ERR_BUSY:    return "Resource busy";
        default:          return "Unknown error";
    }
}

/*=================================================================
 * PATTERN 2: goto-based cleanup
 *================================================================*/
struct my_context {
    int fd;
    char *buffer;
    void *mapping;
    int initialized;
};

error_t context_init(struct my_context *ctx, const char *path, size_t size)
{
    error_t ret;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->fd = -1;
    
    /* Step 1: Open file */
    ctx->fd = open(path, O_RDWR);
    if (ctx->fd < 0) {
        ret = ERR_IO;
        goto err_out;
    }
    
    /* Step 2: Allocate buffer */
    ctx->buffer = malloc(size);
    if (!ctx->buffer) {
        ret = ERR_NOMEM;
        goto err_close;
    }
    
    /* Step 3: Map memory */
    ctx->mapping = mmap(NULL, size, PROT_READ, MAP_PRIVATE, ctx->fd, 0);
    if (ctx->mapping == MAP_FAILED) {
        ctx->mapping = NULL;
        ret = ERR_IO;
        goto err_free;
    }
    
    ctx->initialized = 1;
    return ERR_OK;
    
    /* Cleanup path */
err_free:
    free(ctx->buffer);
    ctx->buffer = NULL;
err_close:
    close(ctx->fd);
    ctx->fd = -1;
err_out:
    return ret;
}

void context_cleanup(struct my_context *ctx)
{
    if (!ctx->initialized)
        return;
    
    /* Reverse order cleanup */
    if (ctx->mapping) {
        munmap(ctx->mapping, /* size */);
        ctx->mapping = NULL;
    }
    
    if (ctx->buffer) {
        free(ctx->buffer);
        ctx->buffer = NULL;
    }
    
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
    
    ctx->initialized = 0;
}

/*=================================================================
 * PATTERN 3: Cleanup attribute (GCC extension)
 *================================================================*/
#define CLEANUP(func) __attribute__((cleanup(func)))

void auto_free(void **ptr)
{
    if (*ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

void auto_close(int *fd)
{
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

error_t process_file(const char *path)
{
    CLEANUP(auto_close) int fd = -1;
    CLEANUP(auto_free) char *buffer = NULL;
    
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return ERR_IO;  /* fd auto-closed */
    
    buffer = malloc(1024);
    if (!buffer)
        return ERR_NOMEM;  /* fd auto-closed, buffer auto-freed */
    
    /* Use fd and buffer... */
    
    return ERR_OK;  /* Auto cleanup on return */
}

/*=================================================================
 * PATTERN 4: Result type (success + error in one)
 *================================================================*/
struct result_ptr {
    void *value;
    error_t error;
};

#define RESULT_OK(val)  ((struct result_ptr){ .value = (val), .error = ERR_OK })
#define RESULT_ERR(err) ((struct result_ptr){ .value = NULL, .error = (err) })
#define IS_OK(r)        ((r).error == ERR_OK)
#define IS_ERR(r)       ((r).error != ERR_OK)

struct result_ptr create_buffer(size_t size)
{
    void *buf = malloc(size);
    if (!buf)
        return RESULT_ERR(ERR_NOMEM);
    return RESULT_OK(buf);
}

void use_result(void)
{
    struct result_ptr r = create_buffer(1024);
    if (IS_ERR(r)) {
        fprintf(stderr, "Error: %s\n", error_string(r.error));
        return;
    }
    
    /* Use r.value... */
    
    free(r.value);
}

/*=================================================================
 * PATTERN 5: Transaction-style operations
 *================================================================*/
struct transaction {
    struct operation {
        void (*rollback)(void *arg);
        void *arg;
    } ops[16];
    int count;
};

void tx_init(struct transaction *tx)
{
    tx->count = 0;
}

void tx_add(struct transaction *tx, void (*rollback)(void *), void *arg)
{
    if (tx->count < 16) {
        tx->ops[tx->count].rollback = rollback;
        tx->ops[tx->count].arg = arg;
        tx->count++;
    }
}

void tx_commit(struct transaction *tx)
{
    /* Nothing to do - operations are already done */
    tx->count = 0;
}

void tx_rollback(struct transaction *tx)
{
    /* Rollback in reverse order */
    while (tx->count > 0) {
        tx->count--;
        tx->ops[tx->count].rollback(tx->ops[tx->count].arg);
    }
}

/* Usage example */
error_t complex_operation(void)
{
    struct transaction tx;
    tx_init(&tx);
    
    void *a = malloc(100);
    if (!a) return ERR_NOMEM;
    tx_add(&tx, free, a);
    
    void *b = malloc(200);
    if (!b) {
        tx_rollback(&tx);  /* Frees a */
        return ERR_NOMEM;
    }
    tx_add(&tx, free, b);
    
    if (some_operation_fails()) {
        tx_rollback(&tx);  /* Frees b, then a */
        return ERR_IO;
    }
    
    tx_commit(&tx);  /* Success - nothing to rollback */
    return ERR_OK;
}
```

**中文解释：**
- 模式1：错误码（不用异常）
- 模式2：goto-based 清理
- 模式3：cleanup 属性（GCC 扩展，自动清理）
- 模式4：结果类型（值+错误合一）
- 模式5：事务式操作（rollback/commit）

