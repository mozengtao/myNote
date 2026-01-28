# Case 2: Socket Buffer Factory (alloc_skb)

The socket buffer factory demonstrates high-performance object creation.

---

## Subsystem Context

```
+=============================================================================+
|                    SKB FACTORY                                               |
+=============================================================================+

    THE PROBLEM:
    ============

    Socket buffers (sk_buff) are:
    - Allocated millions of times per second
    - Complex structure with many fields
    - Need proper initialization for network stack
    - Must be efficient (hot path)


    THE FACTORY:
    ============

    struct sk_buff *alloc_skb(size, gfp)
    {
        /* 1. Allocate sk_buff header from cache */
        skb = kmem_cache_alloc(skbuff_head_cache, gfp);
        
        /* 2. Allocate data buffer */
        data = kmalloc(size, gfp);
        
        /* 3. Initialize pointers */
        skb->head = data;
        skb->data = data;
        skb->tail = data;
        skb->end = data + size;
        
        /* 4. Initialize other fields */
        atomic_set(&skb->users, 1);
        skb->cloned = 0;
        /* ... */
        
        return skb;
    }
```

**中文说明：**

SKB工厂：套接字缓冲区每秒分配数百万次，结构复杂，需要正确初始化供网络栈使用。工厂封装从缓存分配skb头、分配数据缓冲区、初始化指针和其他字段。

---

## Key Structures

```c
/* include/linux/skbuff.h */

struct sk_buff {
    /* Packet data pointers */
    unsigned char *head;  /* Start of buffer */
    unsigned char *data;  /* Start of data */
    unsigned char *tail;  /* End of data */
    unsigned char *end;   /* End of buffer */
    
    /* Reference counting */
    atomic_t users;
    
    /* Networking metadata */
    struct net_device *dev;
    __u32 priority;
    /* ... many more fields ... */
};
```

---

## Minimal Simulation

```c
/* Simplified SKB factory */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sk_buff {
    /* Data pointers */
    unsigned char *head;
    unsigned char *data;
    unsigned char *tail;
    unsigned char *end;
    
    /* Metadata */
    unsigned int len;
    int users;
    int protocol;
};

/* Factory function */
struct sk_buff *alloc_skb(unsigned int size, int gfp)
{
    struct sk_buff *skb;
    unsigned char *data;

    printf("[SKB] Allocating skb (size=%u)\n", size);

    /* Allocate sk_buff structure */
    skb = calloc(1, sizeof(*skb));
    if (!skb) {
        printf("  [ERROR] skb alloc failed\n");
        return NULL;
    }

    /* Allocate data buffer */
    data = malloc(size);
    if (!data) {
        printf("  [ERROR] data alloc failed\n");
        free(skb);
        return NULL;
    }

    /* Initialize pointers */
    skb->head = data;
    skb->data = data;
    skb->tail = data;
    skb->end = data + size;
    skb->len = 0;

    /* Initialize reference count */
    skb->users = 1;

    printf("  [INIT] head=%p data=%p tail=%p end=%p\n",
           skb->head, skb->data, skb->tail, skb->end);

    return skb;
}

/* Add data to buffer */
unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
    unsigned char *tmp = skb->tail;
    skb->tail += len;
    skb->len += len;
    return tmp;
}

/* Reserve headroom */
void skb_reserve(struct sk_buff *skb, unsigned int len)
{
    skb->data += len;
    skb->tail += len;
}

/* Free sk_buff */
void kfree_skb(struct sk_buff *skb)
{
    skb->users--;
    if (skb->users == 0) {
        printf("[SKB] Freeing skb\n");
        free(skb->head);
        free(skb);
    }
}

int main(void)
{
    struct sk_buff *skb;
    unsigned char *p;

    printf("=== SKB FACTORY SIMULATION ===\n\n");

    /* Allocate packet buffer */
    skb = alloc_skb(1500, 0);

    /* Reserve for headers */
    printf("\n[USE] Reserve 14 bytes for Ethernet header\n");
    skb_reserve(skb, 14);

    /* Add data */
    printf("[USE] Add 100 bytes of data\n");
    p = skb_put(skb, 100);
    memset(p, 'X', 100);

    printf("[USE] len=%u, headroom=%ld, tailroom=%ld\n",
           skb->len,
           (long)(skb->data - skb->head),
           (long)(skb->end - skb->tail));

    /* Free */
    printf("\n");
    kfree_skb(skb);

    return 0;
}
```

---

## Why Factory for SKB

```
    SKB FACTORY ADVANTAGES:
    =======================
    
    1. PERFORMANCE
       Uses slab cache for sk_buff headers
       Fast allocation path
    
    2. CORRECTNESS
       Pointers properly initialized
       Reference count set to 1
    
    3. FLEXIBILITY
       Different allocation sizes
       GFP flags for context
    
    4. CLEANUP
       Matching kfree_skb handles both parts
```

---

## Version

Based on **Linux kernel v3.2** net/core/skbuff.c.
