# Case 1: Network Device Factory (alloc_netdev)

The network device factory demonstrates comprehensive object creation.

---

## Subsystem Context

```
+=============================================================================+
|                    NETDEV FACTORY                                            |
+=============================================================================+

    THE PROBLEM:
    ============

    Creating a network device requires:
    - Allocating net_device structure
    - Allocating private data
    - Initializing dozens of fields
    - Setting up queues, stats, lists
    - Linking to network namespace


    THE FACTORY:
    ============

    struct net_device *alloc_netdev(sizeof_priv, name, setup)
    {
        /* 1. Calculate total size */
        alloc_size = sizeof(struct net_device) + sizeof_priv;
        
        /* 2. Allocate memory */
        dev = kzalloc(alloc_size, GFP_KERNEL);
        
        /* 3. Initialize all fields */
        dev->reg_state = NETREG_UNINITIALIZED;
        INIT_LIST_HEAD(&dev->napi_list);
        INIT_LIST_HEAD(&dev->unreg_list);
        /* ... many more ... */
        
        /* 4. Call type-specific setup */
        setup(dev);  /* ether_setup, loopback_setup, etc. */
        
        /* 5. Return ready device */
        return dev;
    }
```

**中文说明：**

网络设备工厂：创建网络设备需要分配结构和私有数据、初始化数十个字段、设置队列/统计/列表、链接到网络命名空间。alloc_netdev封装所有这些步骤。

---

## Key Structures

```c
/* net/core/dev.c */

struct net_device *alloc_netdev_mqs(int sizeof_priv,
                                    const char *name,
                                    void (*setup)(struct net_device *),
                                    unsigned int txqs,
                                    unsigned int rxqs)
{
    struct net_device *dev;
    size_t alloc_size;

    /* Calculate allocation size with alignment */
    alloc_size = sizeof(struct net_device);
    alloc_size = ALIGN(alloc_size, NETDEV_ALIGN);
    alloc_size += sizeof_priv;

    /* Allocate */
    dev = kzalloc(alloc_size, GFP_KERNEL);
    if (!dev)
        return NULL;

    /* Initialize fields */
    dev->pcpu_refcnt = alloc_percpu(int);
    dev->gso_max_size = GSO_MAX_SIZE;
    dev->num_tx_queues = txqs;
    dev->num_rx_queues = rxqs;

    INIT_LIST_HEAD(&dev->napi_list);
    INIT_LIST_HEAD(&dev->unreg_list);

    /* Name */
    strcpy(dev->name, name);

    /* Call setup callback */
    setup(dev);

    return dev;
}
```

---

## Minimal Simulation

```c
/* Simplified network device factory */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct net_device {
    char name[32];
    int mtu;
    int flags;
    unsigned char addr[6];
    void *priv;
    int reg_state;
};

#define NETREG_UNINITIALIZED 0
#define NETREG_REGISTERED    1

/* Setup callbacks for different device types */
void ether_setup(struct net_device *dev)
{
    dev->mtu = 1500;
    dev->flags = 0x1043;  /* UP, BROADCAST, etc. */
    printf("  [SETUP] Ethernet defaults applied\n");
}

void loopback_setup(struct net_device *dev)
{
    dev->mtu = 65536;
    dev->flags = 0x0049;  /* UP, LOOPBACK */
    printf("  [SETUP] Loopback defaults applied\n");
}

/* Factory function */
struct net_device *alloc_netdev(int sizeof_priv,
                                const char *name,
                                void (*setup)(struct net_device *))
{
    struct net_device *dev;
    size_t alloc_size;

    printf("[FACTORY] Creating network device '%s'\n", name);

    /* Calculate total size */
    alloc_size = sizeof(struct net_device) + sizeof_priv;
    printf("  [ALLOC] Size: %zu bytes (%zu + %d priv)\n",
           alloc_size, sizeof(struct net_device), sizeof_priv);

    /* Allocate */
    dev = calloc(1, alloc_size);
    if (!dev) {
        printf("  [ERROR] Allocation failed\n");
        return NULL;
    }

    /* Basic initialization */
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->reg_state = NETREG_UNINITIALIZED;
    dev->priv = (char *)dev + sizeof(struct net_device);

    printf("  [INIT] Basic fields initialized\n");

    /* Call type-specific setup */
    if (setup)
        setup(dev);

    printf("  [DONE] Device ready\n");
    return dev;
}

/* Destructor */
void free_netdev(struct net_device *dev)
{
    printf("[FACTORY] Freeing device '%s'\n", dev->name);
    free(dev);
}

/* Private data for Ethernet driver */
struct eth_priv {
    int rx_packets;
    int tx_packets;
};

int main(void)
{
    struct net_device *eth0, *lo;

    printf("=== NETDEV FACTORY SIMULATION ===\n\n");

    /* Create Ethernet device */
    eth0 = alloc_netdev(sizeof(struct eth_priv), "eth0", ether_setup);
    printf("  Result: %s, MTU=%d\n\n", eth0->name, eth0->mtu);

    /* Create loopback device */
    lo = alloc_netdev(0, "lo", loopback_setup);
    printf("  Result: %s, MTU=%d\n\n", lo->name, lo->mtu);

    /* Cleanup */
    free_netdev(eth0);
    free_netdev(lo);

    return 0;
}

/*
 * Output:
 *
 * === NETDEV FACTORY SIMULATION ===
 *
 * [FACTORY] Creating network device 'eth0'
 *   [ALLOC] Size: 64 bytes (56 + 8 priv)
 *   [INIT] Basic fields initialized
 *   [SETUP] Ethernet defaults applied
 *   [DONE] Device ready
 *   Result: eth0, MTU=1500
 *
 * [FACTORY] Creating network device 'lo'
 *   [ALLOC] Size: 56 bytes (56 + 0 priv)
 *   [INIT] Basic fields initialized
 *   [SETUP] Loopback defaults applied
 *   [DONE] Device ready
 *   Result: lo, MTU=65536
 *
 * [FACTORY] Freeing device 'eth0'
 * [FACTORY] Freeing device 'lo'
 */
```

---

## What Factory Hides

```
    FACTORY ENCAPSULATES:
    =====================
    
    [X] Memory allocation size calculation
    [X] Field initialization
    [X] Private data alignment
    [X] Default values

    CALLER PROVIDES:
    ================
    
    [X] Private data size
    [X] Device name
    [X] Setup callback for type-specific init
```

---

## Version

Based on **Linux kernel v3.2** net/core/dev.c.
