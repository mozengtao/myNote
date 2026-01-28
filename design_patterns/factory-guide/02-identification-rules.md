# Identification Rules: Factory Pattern

Five concrete rules to identify Factory pattern in kernel code.

---

## Rule 1: Look for alloc_* Functions

```c
/* Factory functions typically named alloc_* */
struct net_device *alloc_netdev(...);
struct sk_buff *alloc_skb(...);
struct gendisk *alloc_disk(...);
struct inode *alloc_inode(...);

/* SIGNAL: alloc_xxx() that returns struct xxx * */
```

**中文说明：**

规则1：查找alloc_*函数——工厂函数通常命名为alloc_xxx，返回struct xxx *。

---

## Rule 2: Look for Matching free_* Function

```c
/* Factory functions come with matching destructors */
alloc_netdev()  <-->  free_netdev()
alloc_skb()     <-->  kfree_skb()
alloc_disk()    <-->  put_disk()

/* SIGNAL: Paired alloc/free functions */
```

---

## Rule 3: Look for Internal Initialization

```c
/* Factory function initializes structure internally */
struct net_device *alloc_netdev(...)
{
    dev = kzalloc(...);  /* Allocate */
    
    /* Initialize many fields */
    dev->reg_state = NETREG_UNINITIALIZED;
    INIT_LIST_HEAD(&dev->napi_list);
    dev_net_set(dev, &init_net);
    /* ... more initialization ... */
    
    if (setup)
        setup(dev);  /* Optional callback */
    
    return dev;
}

/* SIGNAL: Function does allocation + initialization */
```

**中文说明：**

规则3：查找内部初始化——工厂函数在内部分配内存并初始化多个字段。

---

## Rule 4: Look for Setup Callbacks

```c
/* Optional customization callback */
struct net_device *alloc_netdev(int priv_size,
                                const char *name,
                                void (*setup)(struct net_device *))
{
    /* ... */
    if (setup)
        setup(dev);
    /* ... */
}

/* Usage: */
alloc_netdev(sizeof(priv), "eth%d", ether_setup);
alloc_netdev(sizeof(priv), "lo", loopback_setup);

/* SIGNAL: setup callback parameter for customization */
```

---

## Rule 5: Look for Size/Type Parameters

```c
/* Factory often takes size or type parameters */
alloc_netdev(int sizeof_priv, ...);  /* Extra private data */
alloc_skb(unsigned int size, ...);   /* Data size */
alloc_disk(int minors);              /* Number of partitions */

/* SIGNAL: Parameters that affect allocation size */
```

---

## Summary Checklist

```
+=============================================================================+
|                    FACTORY IDENTIFICATION CHECKLIST                          |
+=============================================================================+

    [ ] 1. ALLOC_* NAMING
        alloc_xxx() function name
    
    [ ] 2. MATCHING FREE
        Paired free_xxx() or put_xxx()
    
    [ ] 3. INTERNAL INIT
        Allocation + initialization inside
    
    [ ] 4. SETUP CALLBACK
        Optional customization function
    
    [ ] 5. SIZE PARAMETERS
        Parameters affecting allocation

    SCORING:
    3+ indicators = Factory pattern
```

---

## Red Flags: NOT Factory

```
    THESE ARE NOT FACTORY:
    ======================

    1. Just kmalloc wrapper
       Only allocates, no initialization
    
    2. No corresponding free
       Caller manages lifetime
    
    3. Returns allocated memory, not object
       void * instead of struct xxx *
```

---

## Version

Based on **Linux kernel v3.2**.
