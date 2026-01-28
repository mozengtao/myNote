# Case 2: IP Routing Table

The routing table demonstrates RCU for network packet forwarding.

---

## Subsystem Context

```
+=============================================================================+
|                    ROUTING TABLE RCU                                         |
+=============================================================================+

    PROBLEM:
    ========
    
    Every IP packet needs routing decision:
    - Source -> Destination lookup
    - Happens for EVERY packet
    - High-speed links: millions of packets/second
    
    Route updates are rare:
    - Admin changes routes occasionally
    - Routing protocols update infrequently


    RCU SOLUTION:
    =============
    
    Packet processing (fast path):
    - rcu_read_lock()
    - Lookup route (no locks!)
    - Forward packet
    - rcu_read_unlock()
    
    Route updates (slow path):
    - Take appropriate locks
    - Create new route entry
    - rcu_assign_pointer()
    - synchronize_rcu()
    - Free old entry
```

**中文说明：**

路由表问题：每个IP包需要路由决策，高速链路上每秒数百万包。路由更新很少。RCU解决方案：包处理（快速路径）使用RCU无锁查找，路由更新（慢速路径）加锁后更新。

---

## Key Functions

```c
/* Simplified route lookup (net/ipv4/route.c) */
struct rtable *ip_route_output(struct net *net, __be32 daddr, __be32 saddr)
{
    struct rtable *rt;
    
    rcu_read_lock();
    
    /* RCU-protected hash lookup */
    rt = __ip_route_output_key(net, &fl);
    
    rcu_read_unlock();
    return rt;
}

/* Route cache lookup */
static struct rtable *rt_cache_lookup(unsigned hash)
{
    struct rtable *rt;
    
    /* RCU-protected list traversal */
    for (rt = rcu_dereference(rt_hash_table[hash].chain);
         rt != NULL;
         rt = rcu_dereference(rt->dst.rt_next)) {
        if (rt_match(rt))
            return rt;
    }
    return NULL;
}
```

---

## Minimal Simulation

```c
/* Simplified routing table with RCU */

#include <stdio.h>
#include <stdlib.h>

struct route {
    unsigned int dest_ip;
    unsigned int gateway;
    struct route *next;
};

static struct route *route_table = NULL;

/* RCU-protected route lookup */
struct route *route_lookup_rcu(unsigned int dest)
{
    struct route *rt;
    
    printf("[ROUTE] Looking up %u.%u.%u.%u\n",
           (dest >> 24) & 0xff, (dest >> 16) & 0xff,
           (dest >> 8) & 0xff, dest & 0xff);
    
    /* rcu_read_lock() */
    for (rt = route_table; rt; rt = rt->next) {
        if (rt->dest_ip == dest) {
            printf("[ROUTE] Found gateway %u.%u.%u.%u\n",
                   (rt->gateway >> 24) & 0xff, (rt->gateway >> 16) & 0xff,
                   (rt->gateway >> 8) & 0xff, rt->gateway & 0xff);
            return rt;
        }
    }
    /* rcu_read_unlock() */
    
    printf("[ROUTE] No route found\n");
    return NULL;
}

/* Add route (with RCU publish) */
void route_add(unsigned int dest, unsigned int gw)
{
    struct route *new = malloc(sizeof(*new));
    new->dest_ip = dest;
    new->gateway = gw;
    
    /* rcu_assign_pointer equivalent */
    new->next = route_table;
    route_table = new;
    
    printf("[ROUTE] Added route to %u.%u.%u.%u via %u.%u.%u.%u\n",
           (dest >> 24) & 0xff, (dest >> 16) & 0xff,
           (dest >> 8) & 0xff, dest & 0xff,
           (gw >> 24) & 0xff, (gw >> 16) & 0xff,
           (gw >> 8) & 0xff, gw & 0xff);
}

int main(void)
{
    printf("=== ROUTING TABLE RCU SIMULATION ===\n\n");
    
    /* Add some routes */
    route_add(0x0A000000, 0xC0A80001);  /* 10.0.0.0 via 192.168.0.1 */
    route_add(0xAC100000, 0xC0A80002);  /* 172.16.0.0 via 192.168.0.2 */
    
    printf("\n");
    
    /* Lookup routes (RCU-protected) */
    route_lookup_rcu(0x0A000000);
    route_lookup_rcu(0x08080808);
    
    return 0;
}
```

---

## Why RCU Here

```
    WITHOUT RCU:
    ============
    - Lock on every packet
    - Single lock = bottleneck
    - Per-CPU locks = complexity
    
    WITH RCU:
    =========
    - No locks for lookup
    - Massive parallelism
    - Simple code
```

---

## Version

Based on **Linux kernel v3.2** net/ipv4/route.c.
