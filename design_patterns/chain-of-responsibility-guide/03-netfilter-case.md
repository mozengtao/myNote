# Case 1: Netfilter Hooks

Netfilter implements Chain of Responsibility for packet filtering.

---

## Subsystem Context

```
+=============================================================================+
|                    NETFILTER CHAIN                                           |
+=============================================================================+

    NETFILTER ARCHITECTURE:
    =======================

    Packets pass through "hook points" in the network stack.
    At each hook, a chain of filters is checked.


    HOOK POINTS:
    ============

    Incoming Packet
          |
          v
    NF_INET_PRE_ROUTING -----> Routing Decision
          |                          |
          |                    +-----+-----+
          |                    |           |
          v                    v           v
    NF_INET_LOCAL_IN     NF_INET_FORWARD  |
          |                    |           |
          v                    v           |
    Local Process        NF_INET_POST_ROUTING
                               |
                               v
                         Outgoing Packet


    AT EACH HOOK POINT:
    ===================

    Packet --> Hook1 --> Hook2 --> Hook3 --> ...
                |          |          |
            ACCEPT?    ACCEPT?    ACCEPT?
                |          |          |
            no DROP      DROP!     (not reached)
```

**中文说明：**

Netfilter架构：数据包经过网络栈中的"钩子点"。在每个钩子点，检查过滤器链。钩子点包括PRE_ROUTING、LOCAL_IN、FORWARD、POST_ROUTING等。每个钩子点的链按顺序检查规则。

---

## Return Values

```c
/* include/linux/netfilter.h */

#define NF_DROP   0   /* Drop packet - stop chain */
#define NF_ACCEPT 1   /* Accept packet - continue to next hook */
#define NF_STOLEN 2   /* Packet stolen - stop, don't free */
#define NF_QUEUE  3   /* Queue to userspace */
#define NF_REPEAT 4   /* Call this hook again */
#define NF_STOP   5   /* Stop chain, accept packet */

/*
 * Chain processing:
 * - NF_DROP: Packet dropped, chain stops
 * - NF_ACCEPT: Continue to next hook in chain
 * - NF_STOLEN: Handler took ownership, stop
 */
```

---

## Key Structures

```c
/* Hook registration structure */
struct nf_hook_ops {
    struct list_head list;
    nf_hookfn *hook;              /* The filter function */
    struct module *owner;
    u_int8_t pf;                  /* Protocol family */
    unsigned int hooknum;         /* Which hook point */
    int priority;                 /* Order in chain */
};

/* Hook function signature */
typedef unsigned int nf_hookfn(unsigned int hooknum,
                               struct sk_buff *skb,
                               const struct net_device *in,
                               const struct net_device *out,
                               int (*okfn)(struct sk_buff *));
```

---

## Minimal C Simulation

```c
/* Netfilter chain simulation */

#include <stdio.h>
#include <stdlib.h>

#define NF_DROP   0
#define NF_ACCEPT 1

struct packet {
    int src_port;
    int dst_port;
    char data[64];
};

typedef int (*hook_fn)(struct packet *pkt);

struct nf_hook_ops {
    hook_fn hook;
    int priority;
    const char *name;
    struct nf_hook_ops *next;
};

/* Hook chain */
static struct nf_hook_ops *hooks = NULL;

/* Register hook by priority */
int nf_register_hook(struct nf_hook_ops *ops)
{
    struct nf_hook_ops **p;

    printf("[NF] Registering hook: %s (priority %d)\n",
           ops->name, ops->priority);

    for (p = &hooks; *p; p = &(*p)->next) {
        if (ops->priority < (*p)->priority)
            break;
    }
    ops->next = *p;
    *p = ops;
    return 0;
}

/* Process chain */
int nf_hook(struct packet *pkt)
{
    struct nf_hook_ops *h;
    int verdict;

    printf("[NF] Processing packet (dst_port=%d)\n", pkt->dst_port);

    for (h = hooks; h; h = h->next) {
        verdict = h->hook(pkt);

        printf("  [%s] verdict=%s\n", h->name,
               verdict == NF_DROP ? "DROP" : "ACCEPT");

        if (verdict == NF_DROP) {
            printf("[NF] Packet DROPPED by %s\n", h->name);
            return NF_DROP;  /* Stop chain */
        }
    }

    printf("[NF] Packet ACCEPTED\n");
    return NF_ACCEPT;
}

/* Example hooks */
int logging_hook(struct packet *pkt)
{
    printf("  [LOG] Packet to port %d\n", pkt->dst_port);
    return NF_ACCEPT;  /* Just log, continue */
}

int firewall_hook(struct packet *pkt)
{
    /* Block port 22 */
    if (pkt->dst_port == 22) {
        return NF_DROP;
    }
    return NF_ACCEPT;
}

int rate_limit_hook(struct packet *pkt)
{
    /* Simplified - always accept */
    return NF_ACCEPT;
}

static struct nf_hook_ops log_ops = {
    .hook = logging_hook,
    .priority = 100,
    .name = "logging"
};

static struct nf_hook_ops fw_ops = {
    .hook = firewall_hook,
    .priority = 200,
    .name = "firewall"
};

static struct nf_hook_ops rl_ops = {
    .hook = rate_limit_hook,
    .priority = 300,
    .name = "ratelimit"
};

int main(void)
{
    struct packet pkt1 = { .src_port = 12345, .dst_port = 80 };
    struct packet pkt2 = { .src_port = 12345, .dst_port = 22 };

    printf("=== NETFILTER CHAIN SIMULATION ===\n\n");

    /* Register hooks */
    nf_register_hook(&log_ops);
    nf_register_hook(&fw_ops);
    nf_register_hook(&rl_ops);

    /* Process packets */
    printf("\n--- Packet to port 80 ---\n");
    nf_hook(&pkt1);

    printf("\n--- Packet to port 22 ---\n");
    nf_hook(&pkt2);

    return 0;
}

/*
 * Output:
 *
 * === NETFILTER CHAIN SIMULATION ===
 *
 * [NF] Registering hook: logging (priority 100)
 * [NF] Registering hook: firewall (priority 200)
 * [NF] Registering hook: ratelimit (priority 300)
 *
 * --- Packet to port 80 ---
 * [NF] Processing packet (dst_port=80)
 *   [LOG] Packet to port 80
 *   [logging] verdict=ACCEPT
 *   [firewall] verdict=ACCEPT
 *   [ratelimit] verdict=ACCEPT
 * [NF] Packet ACCEPTED
 *
 * --- Packet to port 22 ---
 * [NF] Processing packet (dst_port=22)
 *   [LOG] Packet to port 22
 *   [logging] verdict=ACCEPT
 *   [firewall] verdict=DROP
 * [NF] Packet DROPPED by firewall
 */
```

---

## What Core Does NOT Control

```
    Netfilter Core Controls:
    ------------------------
    [X] Hook point definitions
    [X] Chain traversal
    [X] Verdict handling (DROP/ACCEPT)

    Hooks Control:
    --------------
    [X] What to filter
    [X] Whether to DROP or ACCEPT
    [X] Packet modification
```

---

## Version

Based on **Linux kernel v3.2** net/netfilter/.
