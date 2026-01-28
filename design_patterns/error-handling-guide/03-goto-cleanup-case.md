# Case 1: goto Cleanup Pattern

## The Pattern in Detail

```
+=============================================================================+
|                    GOTO CLEANUP PATTERN                                      |
+=============================================================================+

    STRUCTURE:
    ==========

    int function(void)
    {
        /* INITIALIZATION PHASE (forward order) */
        
        resource1 = acquire1();
        if (!resource1)
            goto err1;
        
        resource2 = acquire2();
        if (!resource2)
            goto err2;
        
        resource3 = acquire3();
        if (!resource3)
            goto err3;
        
        /* SUCCESS PATH */
        return 0;
        
        /* CLEANUP PHASE (reverse order) */
    err3:
        release2(resource2);   /* Fall through */
    err2:
        release1(resource1);   /* Fall through */
    err1:
        return -ENOMEM;        /* Or appropriate error */
    }


    EXECUTION FLOW ON ERROR:
    ========================

    If acquire1() fails:
        Jump to err1
        Return -ENOMEM
        
    If acquire2() fails:
        Jump to err2
        Fall through: release1(resource1)
        Return -ENOMEM
        
    If acquire3() fails:
        Jump to err3
        Fall through: release2(resource2)
        Fall through: release1(resource1)
        Return -ENOMEM
```

**中文说明：**

goto cleanup模式的结构：初始化阶段按顺序获取资源，每次获取失败跳转到对应的错误标签；成功时返回0；清理阶段按逆序释放资源，利用fall-through特性。错误时执行流程：跳转到对应标签，然后依次fall through释放所有之前获取的资源。

---

## Minimal C Code Simulation

```c
/*
 * GOTO CLEANUP PATTERN SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simulated kernel error codes */
#define ENOMEM 12
#define EIO    5

/* ==========================================================
 * SIMULATED RESOURCE ACQUISITION
 * ========================================================== */

struct resource_a {
    char name[32];
    void *data;
};

struct resource_b {
    int irq;
    void *handler;
};

struct resource_c {
    void *io_base;
    size_t size;
};

/* Simulate allocation that can fail */
struct resource_a *alloc_resource_a(const char *name, int fail)
{
    printf("[ALLOC] Allocating resource A (%s)...\n", name);
    if (fail) {
        printf("[ALLOC] Resource A allocation FAILED\n");
        return NULL;
    }
    struct resource_a *a = malloc(sizeof(*a));
    strncpy(a->name, name, sizeof(a->name) - 1);
    a->data = malloc(1024);
    printf("[ALLOC] Resource A allocated successfully\n");
    return a;
}

struct resource_b *alloc_resource_b(int irq, int fail)
{
    printf("[ALLOC] Allocating resource B (IRQ %d)...\n", irq);
    if (fail) {
        printf("[ALLOC] Resource B allocation FAILED\n");
        return NULL;
    }
    struct resource_b *b = malloc(sizeof(*b));
    b->irq = irq;
    b->handler = malloc(256);
    printf("[ALLOC] Resource B allocated successfully\n");
    return b;
}

struct resource_c *alloc_resource_c(size_t size, int fail)
{
    printf("[ALLOC] Allocating resource C (size %zu)...\n", size);
    if (fail) {
        printf("[ALLOC] Resource C allocation FAILED\n");
        return NULL;
    }
    struct resource_c *c = malloc(sizeof(*c));
    c->size = size;
    c->io_base = malloc(size);
    printf("[ALLOC] Resource C allocated successfully\n");
    return c;
}

void free_resource_a(struct resource_a *a)
{
    printf("[FREE] Freeing resource A (%s)\n", a->name);
    free(a->data);
    free(a);
}

void free_resource_b(struct resource_b *b)
{
    printf("[FREE] Freeing resource B (IRQ %d)\n", b->irq);
    free(b->handler);
    free(b);
}

void free_resource_c(struct resource_c *c)
{
    printf("[FREE] Freeing resource C (size %zu)\n", c->size);
    free(c->io_base);
    free(c);
}

/* ==========================================================
 * DEVICE INITIALIZATION WITH GOTO CLEANUP
 * ========================================================== */

struct my_device {
    struct resource_a *a;
    struct resource_b *b;
    struct resource_c *c;
};

/*
 * Initialize device using goto cleanup pattern
 * 
 * fail_at: 0 = success, 1 = fail at A, 2 = fail at B, 3 = fail at C
 */
int my_device_init(struct my_device *dev, int fail_at)
{
    printf("\n=== my_device_init (fail_at=%d) ===\n", fail_at);
    
    /* Step 1: Allocate resource A */
    dev->a = alloc_resource_a("my_device", fail_at == 1);
    if (!dev->a)
        goto err_a;
    
    /* Step 2: Allocate resource B */
    dev->b = alloc_resource_b(42, fail_at == 2);
    if (!dev->b)
        goto err_b;
    
    /* Step 3: Allocate resource C */
    dev->c = alloc_resource_c(4096, fail_at == 3);
    if (!dev->c)
        goto err_c;
    
    printf("[INIT] Device initialization SUCCESS\n");
    return 0;
    
    /* Cleanup in reverse order */
err_c:
    printf("[CLEANUP] At err_c: freeing B\n");
    free_resource_b(dev->b);
err_b:
    printf("[CLEANUP] At err_b: freeing A\n");
    free_resource_a(dev->a);
err_a:
    printf("[CLEANUP] At err_a: returning error\n");
    return -ENOMEM;
}

/*
 * Cleanup on normal exit (all resources allocated)
 */
void my_device_exit(struct my_device *dev)
{
    printf("\n=== my_device_exit ===\n");
    free_resource_c(dev->c);
    free_resource_b(dev->b);
    free_resource_a(dev->a);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    struct my_device dev;
    int ret;
    
    printf("=================================================\n");
    printf("GOTO CLEANUP PATTERN DEMONSTRATION\n");
    printf("=================================================\n");
    
    /* Test 1: All succeed */
    printf("\n--- Test 1: All allocations succeed ---\n");
    ret = my_device_init(&dev, 0);
    printf("Return value: %d\n", ret);
    if (ret == 0) {
        my_device_exit(&dev);
    }
    
    /* Test 2: Fail at step 1 */
    printf("\n--- Test 2: Fail at resource A ---\n");
    ret = my_device_init(&dev, 1);
    printf("Return value: %d\n", ret);
    
    /* Test 3: Fail at step 2 */
    printf("\n--- Test 3: Fail at resource B ---\n");
    ret = my_device_init(&dev, 2);
    printf("Return value: %d\n", ret);
    
    /* Test 4: Fail at step 3 */
    printf("\n--- Test 4: Fail at resource C ---\n");
    ret = my_device_init(&dev, 3);
    printf("Return value: %d\n", ret);
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- goto jumps to cleanup point\n");
    printf("- Labels fall through in reverse order\n");
    printf("- Each error cleans up only allocated resources\n");
    printf("- No duplicate cleanup code\n");
    printf("- Clear, linear code structure\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## Real Kernel Example

```c
/* drivers/pci/pci-driver.c (simplified) */

static int pci_device_probe(struct pci_dev *dev, 
                            const struct pci_device_id *id)
{
    int error;
    
    /* Enable device */
    error = pci_enable_device(dev);
    if (error)
        goto err;
    
    /* Request regions */
    error = pci_request_regions(dev, "driver_name");
    if (error)
        goto err_disable;
    
    /* Map BAR */
    dev->bar = pci_iomap(dev, 0, 0);
    if (!dev->bar) {
        error = -ENOMEM;
        goto err_release;
    }
    
    /* Initialize device */
    error = driver_init_device(dev);
    if (error)
        goto err_unmap;
    
    return 0;
    
err_unmap:
    pci_iounmap(dev, dev->bar);
err_release:
    pci_release_regions(dev);
err_disable:
    pci_disable_device(dev);
err:
    return error;
}
```

---

## Variants of the Pattern

### Variant 1: Single Out Label

```c
int function(void)
{
    int ret = 0;
    void *a = NULL, *b = NULL;
    
    a = alloc_a();
    if (!a) {
        ret = -ENOMEM;
        goto out;
    }
    
    b = alloc_b();
    if (!b) {
        ret = -ENOMEM;
        goto out;
    }
    
    /* Success */
    return 0;
    
out:
    if (b) free_b(b);
    if (a) free_a(a);
    return ret;
}
```

### Variant 2: Error Variable

```c
int function(void)
{
    int error = -ENOMEM;
    
    a = alloc_a();
    if (!a)
        goto err_a;
    
    b = alloc_b();
    if (!b)
        goto err_b;
    
    return 0;
    
err_b:
    free_a(a);
err_a:
    return error;
}
```

---

## Key Takeaways

1. **Forward allocation, backward cleanup**: Natural ordering
2. **Fall-through**: Labels cascade through cleanup code
3. **No duplication**: Each cleanup action appears once
4. **Clear structure**: Easy to verify correctness
5. **Efficient**: Single jump instruction per error path
