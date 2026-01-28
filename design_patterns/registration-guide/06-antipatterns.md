# Registration Anti-Patterns

Common mistakes to avoid when implementing registration patterns.

---

## Anti-Pattern 1: Missing Unregister

```c
/* BAD: No unregister in module_exit */
static void __exit my_exit(void)
{
    /* Forgot to unregister! */
    /* Driver stays in list after module unload */
    /* Dangling pointer -> kernel crash */
}

/* CORRECT: Always unregister */
static void __exit my_exit(void)
{
    pci_unregister_driver(&my_driver);
}
```

**中文说明：**

反模式1：忘记注销——模块卸载时必须调用unregister，否则留下悬空指针导致内核崩溃。

---

## Anti-Pattern 2: Register Without Error Check

```c
/* BAD: Ignoring register return value */
static int __init my_init(void)
{
    pci_register_driver(&my_driver);  /* What if this fails? */
    other_init();
    return 0;
}

/* CORRECT: Check return value */
static int __init my_init(void)
{
    int ret;
    
    ret = pci_register_driver(&my_driver);
    if (ret) {
        pr_err("Failed to register driver\n");
        return ret;
    }
    
    ret = other_init();
    if (ret) {
        pci_unregister_driver(&my_driver);
        return ret;
    }
    
    return 0;
}
```

---

## Anti-Pattern 3: Missing MODULE_DEVICE_TABLE

```c
/* BAD: No MODULE_DEVICE_TABLE */
static const struct pci_device_id my_ids[] = {
    { PCI_DEVICE(0x8086, 0x1234) },
    { 0, }
};

/* Module auto-loading won't work! */

/* CORRECT: Include MODULE_DEVICE_TABLE */
static const struct pci_device_id my_ids[] = {
    { PCI_DEVICE(0x8086, 0x1234) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, my_ids);  /* Enables auto-loading */
```

**中文说明：**

反模式3：缺少MODULE_DEVICE_TABLE——没有这个宏，模块自动加载不工作。

---

## Anti-Pattern 4: Probe Failure Without Cleanup

```c
/* BAD: Partial init, then fail */
int my_probe(struct pci_dev *dev, ...)
{
    resource1 = alloc();
    resource2 = alloc();
    
    if (error)
        return -EINVAL;  /* resource1, resource2 leaked! */
}

/* CORRECT: Cleanup on error */
int my_probe(struct pci_dev *dev, ...)
{
    resource1 = alloc();
    if (!resource1)
        return -ENOMEM;
    
    resource2 = alloc();
    if (!resource2) {
        free(resource1);
        return -ENOMEM;
    }
    
    if (error) {
        free(resource2);
        free(resource1);
        return -EINVAL;
    }
    
    return 0;
}
```

---

## Anti-Pattern 5: Using Driver After Unregister

```c
/* BAD: Accessing driver data after unregister */
void my_exit(void)
{
    pci_unregister_driver(&my_driver);
    
    /* BAD: Driver may still be in use! */
    printk("Driver stats: %d\n", my_driver.stats);
}

/* CORRECT: Unregister handles cleanup */
void my_exit(void)
{
    /* Save any stats before unregister */
    int stats = my_driver.stats;
    
    pci_unregister_driver(&my_driver);
    /* After this, don't touch my_driver */
    
    printk("Final stats: %d\n", stats);
}
```

---

## Summary Checklist

```
+=============================================================================+
|                    REGISTRATION SAFE USAGE                                   |
+=============================================================================+

    [X] Always unregister in module_exit
    [X] Check register return values
    [X] Include MODULE_DEVICE_TABLE for auto-loading
    [X] Clean up resources on probe failure
    [X] Don't use driver after unregister
    [X] Handle remove properly
```

---

## Version

Based on **Linux kernel v3.2** registration patterns.
