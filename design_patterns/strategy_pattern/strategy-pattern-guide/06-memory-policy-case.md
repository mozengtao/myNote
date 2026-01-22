# Case 4: Memory Allocation Policies (High-level)

## Subsystem Background

```
+=============================================================================+
|                    MEMORY ALLOCATION ARCHITECTURE                            |
+=============================================================================+

                          ALLOCATOR CORE
                          ==============

    +------------------------------------------------------------------+
    |                     mm/*.c                                        |
    |                                                                   |
    |   MECHANISM (Fixed):                                              |
    |   - Page frame management (page allocator)                        |
    |   - Buddy system for contiguous allocation                        |
    |   - Slab/SLUB for small objects                                   |
    |   - Physical memory mapping                                       |
    |   - Memory accounting                                             |
    |                                                                   |
    +------------------------------------------------------------------+
                                |
                                | delegates POLICY to
                                v
    +------------------------------------------------------------------+
    |                   ALLOCATION POLICY STRATEGIES                    |
    |                   (Strategy Pattern)                              |
    |                                                                   |
    |   +------------------+  +------------------+  +------------------+|
    |   |    Default       |  |      Bind        |  |   Interleave     ||
    |   | (local first)    |  | (specific nodes) |  | (round-robin)    ||
    |   +------------------+  +------------------+  +------------------+|
    |                                                                   |
    |   +------------------+  +------------------+                      |
    |   |    Preferred     |  | (custom policies |                      |
    |   | (prefer node X)  |  |  possible)       |                      |
    |   +------------------+  +------------------+                      |
    |                                                                   |
    +------------------------------------------------------------------+

    KEY INSIGHT:
    - Memory allocator knows HOW to allocate physical pages
    - Memory policy knows WHERE to allocate (which NUMA node)
```

**中文说明：**

内存分配架构：分配器核心（`mm/*.c`）负责机制——页帧管理、伙伴系统、slab/slub、物理内存映射、内存统计。核心将策略委托给分配策略：Default（本地优先）、Bind（绑定到特定节点）、Interleave（轮询多节点）、Preferred（优先某节点）。关键洞察：内存分配器知道如何分配物理页，内存策略知道在哪里分配（哪个NUMA节点）。

---

## The Strategy Interface: Memory Policies

### Components

| Component | Role |
|-----------|------|
| **Strategy Interface** | `struct mempolicy` with `mode` |
| **Replaceable Algorithm** | MPOL_DEFAULT, MPOL_BIND, MPOL_INTERLEAVE, MPOL_PREFERRED |
| **Selection Mechanism** | Per-process via `set_mempolicy()`, per-VMA via `mbind()` |

### The Conceptual Interface

```c
/* Memory policy modes (from include/linux/mempolicy.h) */
enum {
    MPOL_DEFAULT,      /* Use system default allocation */
    MPOL_PREFERRED,    /* Prefer allocation on specific node */
    MPOL_BIND,         /* Allocate only from specific nodes */
    MPOL_INTERLEAVE,   /* Interleave allocations across nodes */
    MPOL_MAX,
};

struct mempolicy {
    unsigned short mode;      /* Policy mode */
    unsigned short flags;
    nodemask_t v.nodes;      /* Nodes to use */
    /* or */
    short v.preferred_node;  /* Preferred node for MPOL_PREFERRED */
};
```

### Control Flow: How Core Uses Policy

```
    alloc_pages_vma() - Allocate Pages for VMA
    ==========================================

    +----------------------------------+
    |  Page fault or allocation        |
    |  request arrives                 |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  Get memory policy               |  MECHANISM
    |  (from task or VMA)              |  (Core)
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  policy_zonelist()                     ||  STRATEGY
    ||  (Get list of nodes based on policy)   ||
    +==========================================+
                   |
                   v
           +-------+-------+
           |       |       |
           v       v       v
    +--------+ +--------+ +--------+
    |DEFAULT | |BIND    | |INTERL. |
    |local   | |specific| |round-  |
    |node    | |nodes   | |robin   |
    +--------+ +--------+ +--------+
                   |
                   v
    +----------------------------------+
    |  __alloc_pages_nodemask()        |  MECHANISM
    |  (Actually allocate from nodes)  |  (Core)
    +----------------------------------+
```

**中文说明：**

`alloc_pages_vma()`的控制流：页错误或分配请求到达，获取内存策略（从任务或VMA），然后调用策略的`policy_zonelist()`根据策略获取节点列表。不同策略返回不同的节点列表：DEFAULT返回本地节点、BIND返回指定节点、INTERLEAVE轮询返回。最后核心从这些节点实际分配页。

---

## Why Strategy is Required Here

### 1. NUMA Systems Have Non-Uniform Access

```
    NUMA ARCHITECTURE:

    +--------------------+        +--------------------+
    |      NODE 0        |        |      NODE 1        |
    |  +------+  +----+  |        |  +------+  +----+  |
    |  | CPU  |  | RAM|  |  slow  |  | CPU  |  | RAM|  |
    |  | 0-3  |  | 32G|  |<======>|  | 4-7  |  | 32G|  |
    |  +------+  +----+  |        |  +------+  +----+  |
    +--------------------+        +--------------------+
          ^                              ^
          |                              |
      local access                   local access
      ~100 cycles                    ~100 cycles
          |                              |
          +--------- remote access ------+
                     ~300 cycles

    PROBLEM: Allocating memory on wrong node = 3x latency
```

### 2. Different Workloads Need Different Policies

```
    WORKLOAD                BEST MEMORY POLICY
    ========                ==================

    Single-threaded         MPOL_DEFAULT
    +-------------------+   - Allocate local to running CPU
    | Runs on one CPU   |   - Minimum latency
    +-------------------+

    Database Buffer Pool    MPOL_BIND
    +-------------------+   - Pin to specific nodes
    | Large, static     |   - Predictable performance
    | memory region     |   - Controlled placement
    +-------------------+

    Parallel/HPC            MPOL_INTERLEAVE
    +-------------------+   - Spread across all nodes
    | Threads on all    |   - Balance bandwidth
    | CPUs access data  |   - Avoid hotspots
    +-------------------+

    Fail-soft Priority      MPOL_PREFERRED
    +-------------------+   - Try preferred node first
    | Prefer local but  |   - Fall back if unavailable
    | accept remote     |
    +-------------------+
```

**中文说明：**

为什么需要策略：(1) NUMA系统有非均匀访问——访问本地内存约100周期，远程内存约300周期，分配在错误节点意味着3倍延迟。(2) 不同工作负载需要不同策略——单线程适合DEFAULT（本地分配）、数据库缓冲池适合BIND（固定节点）、并行HPC适合INTERLEAVE（分散带宽）、容错优先适合PREFERRED（首选某节点但可回退）。

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL MEMORY POLICY STRATEGY SIMULATION
 * 
 * Demonstrates how memory policies work as strategies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct mempolicy;

/* ==========================================================
 * NUMA SIMULATION
 * ========================================================== */
#define MAX_NODES 4

struct numa_node {
    int id;
    unsigned long free_pages;
    unsigned long used_pages;
};

static struct numa_node nodes[MAX_NODES] = {
    { .id = 0, .free_pages = 1000, .used_pages = 0 },
    { .id = 1, .free_pages = 1000, .used_pages = 0 },
    { .id = 2, .free_pages = 1000, .used_pages = 0 },
    { .id = 3, .free_pages = 1000, .used_pages = 0 },
};

static int current_cpu = 0;  /* Simulated current CPU */
static int cpu_to_node[8] = { 0, 0, 1, 1, 2, 2, 3, 3 };  /* CPU to node mapping */

static int get_local_node(void)
{
    return cpu_to_node[current_cpu];
}

/* ==========================================================
 * MEMORY POLICY: Strategy Interface
 * ========================================================== */

/* Policy modes */
#define MPOL_DEFAULT    0
#define MPOL_PREFERRED  1
#define MPOL_BIND       2
#define MPOL_INTERLEAVE 3

struct mempolicy {
    int mode;
    unsigned int nodemask;     /* Bitmask of allowed nodes */
    int preferred_node;        /* For MPOL_PREFERRED */
    int interleave_next;       /* For MPOL_INTERLEAVE round-robin */
};

/* ==========================================================
 * POLICY STRATEGY IMPLEMENTATIONS
 * ========================================================== */

/* DEFAULT: Allocate from local node */
static int policy_default_get_node(struct mempolicy *pol)
{
    int node = get_local_node();
    printf("  [DEFAULT] Selecting local node %d (CPU %d)\n", 
           node, current_cpu);
    return node;
}

/* PREFERRED: Try preferred node, fall back to others */
static int policy_preferred_get_node(struct mempolicy *pol)
{
    int node = pol->preferred_node;
    
    if (nodes[node].free_pages > 0) {
        printf("  [PREFERRED] Selecting preferred node %d\n", node);
        return node;
    }
    
    /* Fall back to any node with memory */
    for (int i = 0; i < MAX_NODES; i++) {
        if (nodes[i].free_pages > 0) {
            printf("  [PREFERRED] Preferred node %d full, falling back to %d\n",
                   pol->preferred_node, i);
            return i;
        }
    }
    
    return -1;  /* Out of memory */
}

/* BIND: Only allocate from specified nodes */
static int policy_bind_get_node(struct mempolicy *pol)
{
    /* Find node in allowed set with free memory */
    for (int i = 0; i < MAX_NODES; i++) {
        if ((pol->nodemask & (1 << i)) && nodes[i].free_pages > 0) {
            printf("  [BIND] Selecting allowed node %d (mask=0x%x)\n",
                   i, pol->nodemask);
            return i;
        }
    }
    
    printf("  [BIND] No memory in allowed nodes!\n");
    return -1;  /* No memory in allowed nodes */
}

/* INTERLEAVE: Round-robin across nodes */
static int policy_interleave_get_node(struct mempolicy *pol)
{
    int start = pol->interleave_next;
    
    /* Find next node in allowed set */
    for (int i = 0; i < MAX_NODES; i++) {
        int node = (start + i) % MAX_NODES;
        if ((pol->nodemask & (1 << node)) && nodes[node].free_pages > 0) {
            pol->interleave_next = (node + 1) % MAX_NODES;
            printf("  [INTERLEAVE] Selecting node %d (round-robin)\n", node);
            return node;
        }
    }
    
    return -1;
}

/* Strategy dispatcher */
static int policy_get_node(struct mempolicy *pol)
{
    switch (pol->mode) {
    case MPOL_DEFAULT:
        return policy_default_get_node(pol);
    case MPOL_PREFERRED:
        return policy_preferred_get_node(pol);
    case MPOL_BIND:
        return policy_bind_get_node(pol);
    case MPOL_INTERLEAVE:
        return policy_interleave_get_node(pol);
    default:
        return policy_default_get_node(pol);
    }
}

/* ==========================================================
 * MEMORY ALLOCATOR CORE (MECHANISM)
 * ========================================================== */

/* Core: Allocate a page using policy */
static void *alloc_pages_policy(struct mempolicy *pol, unsigned int count)
{
    printf("[MM CORE] alloc_pages_policy: requesting %u pages\n", count);
    
    /* STRATEGY: Get target node from policy */
    int node = policy_get_node(pol);
    if (node < 0) {
        printf("[MM CORE] Allocation failed - no memory\n");
        return NULL;
    }
    
    /* MECHANISM: Actually allocate from the node */
    if (nodes[node].free_pages < count) {
        printf("[MM CORE] Node %d has insufficient memory\n", node);
        return NULL;
    }
    
    nodes[node].free_pages -= count;
    nodes[node].used_pages += count;
    
    printf("[MM CORE] Allocated %u pages from node %d "
           "(free=%lu, used=%lu)\n",
           count, node, nodes[node].free_pages, nodes[node].used_pages);
    
    return (void *)(long)(node * 1000 + nodes[node].used_pages);  /* Fake address */
}

/* ==========================================================
 * PROCESS SIMULATION
 * ========================================================== */

struct task_struct {
    const char *name;
    struct mempolicy *policy;
};

/* Set memory policy for task (like set_mempolicy syscall) */
static void set_mempolicy(struct task_struct *task, int mode, 
                          unsigned int nodemask, int preferred)
{
    if (!task->policy)
        task->policy = calloc(1, sizeof(struct mempolicy));
    
    task->policy->mode = mode;
    task->policy->nodemask = nodemask;
    task->policy->preferred_node = preferred;
    task->policy->interleave_next = 0;
    
    const char *mode_names[] = {"DEFAULT", "PREFERRED", "BIND", "INTERLEAVE"};
    printf("[SYSCALL] set_mempolicy(%s, %s, nodes=0x%x, pref=%d)\n",
           task->name, mode_names[mode], nodemask, preferred);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("================================================\n");
    printf("MEMORY POLICY STRATEGY PATTERN DEMONSTRATION\n");
    printf("================================================\n");
    printf("\nNUMA Topology: 4 nodes, 8 CPUs\n");
    printf("CPUs 0-1 -> Node 0, CPUs 2-3 -> Node 1\n");
    printf("CPUs 4-5 -> Node 2, CPUs 6-7 -> Node 3\n");

    /* Create tasks with different policies */
    struct task_struct task_default = { .name = "app_default" };
    struct task_struct task_bind = { .name = "database" };
    struct task_struct task_interleave = { .name = "hpc_job" };
    struct task_struct task_preferred = { .name = "web_server" };

    /* === Configure policies === */
    printf("\n=== CONFIGURING POLICIES ===\n\n");
    
    set_mempolicy(&task_default, MPOL_DEFAULT, 0xF, 0);
    set_mempolicy(&task_bind, MPOL_BIND, 0x3, 0);      /* Only nodes 0,1 */
    set_mempolicy(&task_interleave, MPOL_INTERLEAVE, 0xF, 0);  /* All nodes */
    set_mempolicy(&task_preferred, MPOL_PREFERRED, 0xF, 1);    /* Prefer node 1 */

    /* === DEFAULT policy === */
    printf("\n=== DEFAULT POLICY (Local Allocation) ===\n");
    current_cpu = 0;  /* Running on CPU 0 (Node 0) */
    printf("Task running on CPU %d:\n", current_cpu);
    alloc_pages_policy(task_default.policy, 10);
    
    current_cpu = 5;  /* Running on CPU 5 (Node 2) */
    printf("\nTask migrated to CPU %d:\n", current_cpu);
    alloc_pages_policy(task_default.policy, 10);

    /* === BIND policy === */
    printf("\n=== BIND POLICY (Database: Nodes 0,1 only) ===\n");
    printf("Allocating from bound nodes only:\n");
    for (int i = 0; i < 3; i++) {
        alloc_pages_policy(task_bind.policy, 10);
    }

    /* === INTERLEAVE policy === */
    printf("\n=== INTERLEAVE POLICY (HPC: Round-robin) ===\n");
    printf("Allocating across all nodes:\n");
    for (int i = 0; i < 8; i++) {
        alloc_pages_policy(task_interleave.policy, 10);
    }

    /* === PREFERRED policy === */
    printf("\n=== PREFERRED POLICY (Web server: Prefer node 1) ===\n");
    printf("Allocating with preference:\n");
    for (int i = 0; i < 3; i++) {
        alloc_pages_policy(task_preferred.policy, 10);
    }

    /* === Final state === */
    printf("\n=== FINAL NODE STATE ===\n");
    for (int i = 0; i < MAX_NODES; i++) {
        printf("Node %d: free=%lu, used=%lu\n",
               i, nodes[i].free_pages, nodes[i].used_pages);
    }

    printf("\n================================================\n");
    printf("KEY OBSERVATIONS:\n");
    printf("1. DEFAULT follows CPU location\n");
    printf("2. BIND restricts to specific nodes\n");
    printf("3. INTERLEAVE spreads load across nodes\n");
    printf("4. PREFERRED tries preferred, falls back if needed\n");
    printf("5. Allocator core doesn't know policy details\n");
    printf("================================================\n");

    return 0;
}
```

---

## What the Kernel Core Does NOT Control

```
+=============================================================================+
|              WHAT CORE DOES NOT CONTROL (Strategy Owns)                      |
+=============================================================================+

    THE CORE DOES NOT DECIDE:

    1. WHICH NUMA NODE TO USE
       +-------------------------------------------------------+
       | DEFAULT: local to current CPU                         |
       | BIND: only from specified set                         |
       | INTERLEAVE: round-robin across nodes                  |
       | PREFERRED: try preferred, fallback allowed            |
       +-------------------------------------------------------+

    2. FALLBACK BEHAVIOR
       +-------------------------------------------------------+
       | BIND: fail if no memory in allowed nodes              |
       | PREFERRED: fall back to any node                      |
       | Different policies have different failure modes       |
       +-------------------------------------------------------+

    3. DISTRIBUTION PATTERN
       +-------------------------------------------------------+
       | INTERLEAVE: how to distribute across nodes            |
       | Application-specific spread patterns                  |
       +-------------------------------------------------------+

    THE CORE ONLY PROVIDES:
    - Physical page allocation from specific node
    - Free page tracking per node
    - Zone/node lookup infrastructure
    - Page frame number management
```

**中文说明：**

核心不控制的内容：(1) 使用哪个NUMA节点——DEFAULT用本地、BIND用指定集合、INTERLEAVE轮询、PREFERRED先尝试首选；(2) 回退行为——BIND在允许节点无内存时失败、PREFERRED回退到任意节点；(3) 分布模式——INTERLEAVE如何分布。核心只提供：从特定节点分配物理页、每节点空闲页跟踪、zone/节点查找。

---

## Real Kernel Code Reference (v3.2)

### struct mempolicy in include/linux/mempolicy.h

```c
struct mempolicy {
    atomic_t refcnt;
    unsigned short mode;
    unsigned short flags;
    union {
        short preferred_node;
        nodemask_t nodes;
    } v;
    /* ... */
};
```

### Policy application in mm/mempolicy.c

```c
/* Get zonelist for allocation based on policy */
struct zonelist *policy_zonelist(gfp_t gfp, struct mempolicy *policy)
{
    int nd = numa_node_id();

    switch (policy->mode) {
    case MPOL_PREFERRED:
        nd = policy->v.preferred_node;
        break;
    case MPOL_BIND:
        /* Return zonelist restricted to allowed nodes */
        ...
    case MPOL_INTERLEAVE:
        nd = interleave_nodes(policy);
        break;
    }
    
    return node_zonelist(nd, gfp);
}
```

---

## Key Takeaways

1. **Policy determines WHERE**: Allocator mechanism handles HOW
2. **Per-process and per-VMA policies**: Fine-grained control
3. **Multiple policies coexist**: Different tasks use different strategies
4. **Runtime changeable**: `set_mempolicy()` and `mbind()` syscalls
5. **Fallback behaviors vary by policy**: Part of the strategy definition
