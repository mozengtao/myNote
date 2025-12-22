# Linux Kernel Fast Path vs Slow Path Separation (v3.2)

## Overview

This document explains how the Linux kernel **separates fast paths from slow paths** to achieve high performance while maintaining functionality.

---

## Fast Path vs Slow Path Definition

```
+------------------------------------------------------------------+
|  FAST PATH vs SLOW PATH                                          |
+------------------------------------------------------------------+

    FAST PATH:
    +----------------------------------------------------------+
    | - Executed frequently (hot code)                         |
    | - Optimized for common case                              |
    | - Minimal branches, no locks if possible                 |
    | - No sleeping, no allocation                             |
    | - Measured in nanoseconds                                |
    +----------------------------------------------------------+
    
    SLOW PATH:
    +----------------------------------------------------------+
    | - Executed rarely (cold code)                            |
    | - Handles edge cases, errors, setup                      |
    | - May acquire locks, allocate memory                     |
    | - May sleep, may block                                   |
    | - Measured in microseconds to milliseconds               |
    +----------------------------------------------------------+

+------------------------------------------------------------------+
|  THE SEPARATION PRINCIPLE                                        |
+------------------------------------------------------------------+

    +-------------------+
    |   Entry Point     |
    +---------+---------+
              |
              v
        +-----+-----+
        | Fast Path |  <-- 99% of calls
        | Check     |
        +-----+-----+
              |
        +-----+-----+
        |           |
        v           v
    SUCCESS     SLOW PATH   <-- 1% of calls
    (return)    (handle edge case)
```

**中文解释：**
- **快路径**：频繁执行、优化常见情况、最小化分支、无锁无分配
- **慢路径**：罕见执行、处理边缘情况、可获取锁和分配内存
- **分离原则**：入口点快速检查，99% 走快路径，1% 走慢路径

---

## Networking Examples

### Packet Reception Fast Path

```
+------------------------------------------------------------------+
|  NETWORK RX: FAST vs SLOW PATH                                   |
+------------------------------------------------------------------+

    FAST PATH (NAPI polling, 99%+ of packets):
    
    +-------------------+
    | Hardware IRQ      |  Minimal: just schedule NAPI
    +---------+---------+
              |
              v
    +-------------------+
    | NAPI Poll         |  Process packets in batch
    | (softirq context) |
    +---------+---------+
              |
              v
    +-------------------+
    | Protocol handlers |  Direct function calls
    | (IP, TCP, UDP)    |  No queuing overhead
    +---------+---------+
              |
              v
    +-------------------+
    | Socket buffer     |  Wake up waiting process
    +-------------------+
    
    
    SLOW PATH (1% - setup, errors):
    
    +-------------------+
    | Socket creation   |  Locks, allocation
    +-------------------+
              |
    +-------------------+
    | Connection setup  |  TCP handshake
    +-------------------+
              |
    +-------------------+
    | Buffer allocation |  May fail, retry
    +-------------------+
```

### TCP Fast Path

From `net/ipv4/tcp_input.c`:

```c
/* Fast path check in tcp_rcv_established() */
static void tcp_rcv_established(struct sock *sk, struct sk_buff *skb,
                                const struct tcphdr *th, unsigned int len)
{
    struct tcp_sock *tp = tcp_sk(sk);
    
    /*
     * FAST PATH: Header prediction
     * Check if this is a predictable, common-case packet
     */
    if ((tcp_flag_word(th) & TCP_HP_BITS) == tp->pred_flags &&
        TCP_SKB_CB(skb)->seq == tp->rcv_nxt &&
        !after(TCP_SKB_CB(skb)->ack_seq, tp->snd_nxt)) {
        
        /* FAST PATH: Process without state machine */
        __skb_pull(skb, tcp_header_len);
        tcp_rcv_rtt_measure_ts(sk, skb);
        tcp_event_data_recv(sk, skb);
        
        /* Direct to receive queue */
        __skb_queue_tail(&sk->sk_receive_queue, skb);
        
        /* Wake up reader */
        sk->sk_data_ready(sk, 0);
        return;
    }
    
    /* SLOW PATH: Full TCP state machine processing */
    tcp_rcv_slow_path(sk, skb, th, len);
}
```

```
+------------------------------------------------------------------+
|  TCP HEADER PREDICTION (Fast Path Optimization)                  |
+------------------------------------------------------------------+

    Expected packet:
    +----------------------------------------------------------+
    | - No special flags (no SYN, FIN, RST, URG)               |
    | - Sequence number is what we expect (rcv_nxt)            |
    | - ACK is valid (not beyond what we've sent)              |
    | - No options that need processing                        |
    +----------------------------------------------------------+
    
    If ALL conditions match → FAST PATH
    - Skip TCP state machine
    - Direct queue to socket
    
    If ANY condition fails → SLOW PATH
    - Full state machine processing
    - Handle edge cases
```

**中文解释：**
- TCP 快路径使用"头部预测"优化：
  - 检查包是否符合预期（无特殊标志、正确序列号、有效 ACK）
  - 所有条件满足 → 跳过状态机，直接入队
  - 任一条件不满足 → 完整状态机处理

### Transmit Path

```
+------------------------------------------------------------------+
|  NETWORK TX: FAST vs SLOW PATH                                   |
+------------------------------------------------------------------+

    FAST PATH (sendmsg on established connection):
    
    User:           write(fd, data, len)
                           |
                           v
    Socket:         tcp_sendmsg()
                           |
                           +-- Already connected
                           |   Already have buffers
                           |   No memory pressure
                           v
    TCP:            tcp_write_xmit()
                           |
                           v
    IP:             ip_queue_xmit()
                           |
                           v
    Driver:         ndo_start_xmit()
                           |
                           v
    Hardware:       DMA to NIC
    
    
    SLOW PATH (connection setup, buffer allocation):
    
    connect():      TCP 3-way handshake
                    - SYN/SYN-ACK/ACK
                    - May timeout, retry
                    - Route lookup
    
    write() under   Memory pressure:
    pressure:       - Wait for buffer space
                    - May block
                    - May trigger reclaim
```

---

## Block I/O Examples

### Block Request Fast Path

```
+------------------------------------------------------------------+
|  BLOCK I/O: FAST vs SLOW PATH                                    |
+------------------------------------------------------------------+

    FAST PATH (cached read):
    
    read():         sys_read()
                           |
                           v
    Page Cache:     find_get_page()
                           |
                           +-- Page found in cache!
                           |
                           v
    Return:         Copy to user buffer
                    (no disk I/O!)
    
    
    SLOW PATH (cache miss):
    
    read():         sys_read()
                           |
                           v
    Page Cache:     find_get_page()
                           |
                           +-- Page NOT in cache
                           |
                           v
    Allocate:       alloc_page()
                           |
                           v
    Block Layer:    submit_bio()
                           |
                           v
    I/O Scheduler:  Merge, sort requests
                           |
                           v
    Driver:         Block device I/O
                           |
                           v
    Wait:           Process sleeps until I/O complete
```

### Request Merging

```
+------------------------------------------------------------------+
|  BLOCK LAYER: MERGING FOR FAST PATH                              |
+------------------------------------------------------------------+

    Incoming requests:
    
    [read sector 100]
    [read sector 101]   <- Can merge with above!
    [read sector 102]   <- Can merge!
    [read sector 200]   <- Different range, separate
    
    After merging:
    
    [read sectors 100-102]  <- Single request
    [read sector 200]
    
    Result:
    +----------------------------------------------------------+
    | - Fewer requests to hardware                             |
    | - Better utilization of disk bandwidth                   |
    | - Reduced interrupt overhead                             |
    +----------------------------------------------------------+
```

---

## Scheduler Examples

### Scheduler Fast Path

```
+------------------------------------------------------------------+
|  SCHEDULER: FAST vs SLOW PATH                                    |
+------------------------------------------------------------------+

    FAST PATH (pick_next_task):
    
    Context switch:     schedule()
                              |
                              v
    Check fair class:   pick_next_task_fair()
                              |
                              +-- Most tasks are in fair class
                              |
                              v
    Pick from RB-tree:  __pick_first_entity()
                              |
                              v
    Switch:             context_switch()
    
    
    SLOW PATH (load balancing):
    
    Timer tick:         scheduler_tick()
                              |
                              v
    Balance check:      trigger_load_balance()
                              |
                              +-- CPU imbalanced
                              |
                              v
    Migration:          move_tasks()
                              |
                              +-- Lock multiple runqueues
                              |
                              +-- Find movable tasks
                              |
                              +-- Transfer tasks
```

### Pick Next Task Optimization

From `kernel/sched_fair.c`:

```c
/*
 * The fast path for picking next task
 */
static struct task_struct *pick_next_task_fair(struct rq *rq)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se;
    
    if (!cfs_rq->nr_running)
        return NULL;
    
    /* FAST PATH: Just pick leftmost in RB-tree */
    se = __pick_first_entity(cfs_rq);
    
    /* Navigate to task */
    while (se->my_q)
        se = __pick_first_entity(se->my_q);
    
    return task_of(se);
}
```

```
+------------------------------------------------------------------+
|  CFS RB-TREE: O(1) PICK NEXT                                     |
+------------------------------------------------------------------+

              (root)
             /      \
           /          \
        (se1)         (se5)
        /   \         /   \
    (se0)  (se2)   (se4)  (se6)
      ^
      |
    LEFTMOST - Always pick this one!
    (Lowest vruntime = most deserving)
    
    Leftmost cached → O(1) access
```

**中文解释：**
- 调度器快路径：
  - 大多数任务在公平调度类
  - 从 RB 树取最左节点 = O(1)
  - 最左节点缓存，无需遍历
- 慢路径：负载均衡、任务迁移（需要锁多个运行队列）

---

## How Slow Paths are Isolated

### Technique 1: unlikely() Macro

```c
/* Compiler hint: this branch is rarely taken */
if (unlikely(error_condition)) {
    /* Slow path - handle error */
    goto slow_path;
}
/* Fast path continues */

#define unlikely(x) __builtin_expect(!!(x), 0)
```

### Technique 2: Separate Functions

```c
/* Fast path inline */
static inline int fast_operation(struct obj *obj)
{
    if (likely(common_case))
        return quick_result;
    
    return slow_operation(obj);  /* Outlined */
}

/* Slow path NOT inline - saves icache */
static noinline int slow_operation(struct obj *obj)
{
    /* Complex handling */
    /* May sleep */
    /* Error paths */
}
```

### Technique 3: Deferred Work

```
+------------------------------------------------------------------+
|  DEFER SLOW WORK TO BACKGROUND                                   |
+------------------------------------------------------------------+

    Fast path:          Mark for later
                              |
                              v
    Return immediately  <-----+
    
    Background:
    
    +-------------------+
    | Workqueue         |  Process deferred work
    | (process context) |  - Can sleep
    +-------------------+  - Can allocate
              |            - No latency impact
              v
    +-------------------+
    | Complete work     |
    +-------------------+
```

Example: Dentry cache shrinking

```c
/* Fast path: Just mark for shrinking */
static void dentry_lru_add(struct dentry *dentry)
{
    list_add(&dentry->d_lru, &sb->s_dentry_lru);
    sb->s_nr_dentry_unused++;
    /* Don't shrink here - let shrinker do it later */
}

/* Slow path: Background shrinker */
static int shrink_dcache_memory(struct shrinker *shrink,
                                struct shrink_control *sc)
{
    /* Called by memory pressure handler */
    /* Can do complex LRU scanning */
    prune_dcache(sc->nr_to_scan);
}
```

**中文解释：**
- **隔离技术1**：`unlikely()` 宏提示编译器分支预测
- **隔离技术2**：分离函数，慢路径不内联（节省指令缓存）
- **隔离技术3**：延迟工作到后台处理（工作队列）

---

## How Mixing Paths Causes Regressions

### Anti-Pattern 1: Lock in Fast Path

```c
/* BAD: Taking lock on every packet */
netdev_tx_t broken_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    
    spin_lock(&priv->lock);  /* BOTTLENECK! */
    
    /* ... transmit ... */
    
    spin_unlock(&priv->lock);
    return NETDEV_TX_OK;
}

/* BETTER: Lock-free fast path with per-CPU queues */
netdev_tx_t better_xmit(struct sk_buff *skb, struct net_device *dev)
{
    int cpu = smp_processor_id();
    struct tx_queue *q = &priv->tx_queues[cpu];
    
    /* No lock for per-CPU queue */
    enqueue(q, skb);
    trigger_tx(dev);
    return NETDEV_TX_OK;
}
```

### Anti-Pattern 2: Allocation in Fast Path

```c
/* BAD: Allocating on every operation */
int broken_send(struct connection *conn, void *data, size_t len)
{
    struct buffer *buf = kmalloc(sizeof(*buf) + len, GFP_KERNEL);
    
    /* Problems:
     * 1. kmalloc may fail
     * 2. May trigger memory reclaim
     * 3. Unpredictable latency
     */
    
    memcpy(buf->data, data, len);
    send_buffer(conn, buf);
    return 0;
}

/* BETTER: Pre-allocated buffer pool */
int better_send(struct connection *conn, void *data, size_t len)
{
    struct buffer *buf = get_buffer_from_pool(conn);
    
    if (unlikely(!buf))
        return -ENOBUFS;  /* Fast fail */
    
    memcpy(buf->data, data, len);
    send_buffer(conn, buf);
    return 0;
}
```

### Anti-Pattern 3: Complex Logic in Fast Path

```c
/* BAD: Complex validation on every packet */
int broken_process(struct packet *pkt)
{
    /* All this runs on every packet! */
    if (validate_header(pkt)) {
        if (check_routing(pkt)) {
            if (apply_firewall(pkt)) {
                if (update_statistics(pkt)) {
                    /* ... */
                }
            }
        }
    }
}

/* BETTER: Early return for common case */
int better_process(struct packet *pkt)
{
    /* Quick header check */
    if (unlikely(!basic_sanity(pkt)))
        goto slow_path;
    
    /* Fast path: Direct processing */
    return fast_forward(pkt);
    
slow_path:
    /* Complex handling only when needed */
    return slow_process(pkt);
}
```

```
+------------------------------------------------------------------+
|  PERFORMANCE IMPACT OF PATH MIXING                               |
+------------------------------------------------------------------+

    SCENARIO: 1 million packets/second
    
    Fast path only (10ns per packet):
    Total: 10ms CPU time/second = 1% CPU
    
    Mixed path (100ns per packet due to lock):
    Total: 100ms CPU time/second = 10% CPU
    
    With allocation (1000ns per packet):
    Total: 1000ms CPU time/second = 100% CPU (saturated!)
    
    RULE: Every nanosecond matters in fast path
```

**中文解释：**
- **反模式1**：快路径加锁 → 成为瓶颈
- **反模式2**：快路径分配 → 延迟不可预测
- **反模式3**：快路径复杂逻辑 → 累积开销
- **影响**：每纳秒在快路径都很重要，百万次操作放大效应巨大

---

## User-Space Application

### Event Loop Fast Path

```c
/* user_space_fast_slow_path.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_EVENTS 64
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/*---------------------------------------------------------
 * Fast path: Pre-allocated connection pool
 *---------------------------------------------------------*/
#define CONN_POOL_SIZE 1024

struct connection {
    int fd;
    char read_buf[4096];    /* Pre-allocated */
    char write_buf[4096];   /* Pre-allocated */
    int read_pos;
    int write_pos;
    bool in_use;
};

static struct connection conn_pool[CONN_POOL_SIZE];
static int pool_next = 0;

/* Fast path: O(1) allocation from pool */
struct connection *conn_alloc_fast(void)
{
    if (likely(pool_next < CONN_POOL_SIZE)) {
        struct connection *c = &conn_pool[pool_next++];
        c->in_use = true;
        return c;
    }
    return NULL;  /* Pool exhausted - slow path */
}

/* Slow path: Dynamic allocation (only when pool exhausted) */
struct connection *conn_alloc_slow(void)
{
    struct connection *c = malloc(sizeof(*c));
    if (c) {
        memset(c, 0, sizeof(*c));
        c->in_use = true;
    }
    return c;
}

struct connection *conn_alloc(void)
{
    struct connection *c = conn_alloc_fast();
    if (unlikely(!c))
        c = conn_alloc_slow();
    return c;
}

/*---------------------------------------------------------
 * Fast path: Common case processing
 *---------------------------------------------------------*/
typedef enum {
    RESULT_OK,
    RESULT_NEED_MORE,
    RESULT_ERROR,
    RESULT_CLOSE
} process_result_t;

/* Fast path: Simple echo */
process_result_t process_fast(struct connection *c)
{
    /* Assume: complete message in buffer */
    int len = c->read_pos;
    
    if (likely(len > 0 && len <= sizeof(c->write_buf))) {
        memcpy(c->write_buf, c->read_buf, len);
        c->write_pos = len;
        c->read_pos = 0;
        return RESULT_OK;
    }
    
    return RESULT_NEED_MORE;
}

/* Slow path: Complex protocol handling */
process_result_t process_slow(struct connection *c)
{
    /* Handle:
     * - Fragmented messages
     * - Protocol errors
     * - Connection management
     * - Logging
     */
    
    if (c->read_pos == 0)
        return RESULT_NEED_MORE;
    
    /* Complex parsing... */
    printf("[SLOW] Processing complex case\n");
    
    return process_fast(c);  /* Try fast path after fixup */
}

/*---------------------------------------------------------
 * Main event loop
 *---------------------------------------------------------*/
void event_loop_demo(void)
{
    printf("Fast/Slow path demo:\n\n");
    
    /* Simulate connections */
    struct connection *c1 = conn_alloc();
    struct connection *c2 = conn_alloc();
    
    /* Fast path case */
    strcpy(c1->read_buf, "Hello");
    c1->read_pos = 5;
    
    process_result_t r1 = process_fast(c1);
    printf("Fast path result: %s\n", 
           r1 == RESULT_OK ? "OK (fast)" : "needed slow");
    
    /* Slow path case - empty buffer */
    c2->read_pos = 0;
    
    process_result_t r2 = process_fast(c2);
    if (r2 != RESULT_OK) {
        printf("Fast path failed, trying slow...\n");
        r2 = process_slow(c2);
    }
    printf("Slow path result: %s\n",
           r2 == RESULT_NEED_MORE ? "need more data" : "processed");
}

int main(void)
{
    event_loop_demo();
    return 0;
}
```

**中文解释：**
- 用户态示例：
  1. 连接池预分配 → 快路径 O(1) 分配
  2. 简单情况快速处理
  3. 复杂情况走慢路径
  4. `likely()`/`unlikely()` 提示编译器

---

## Summary

```
+------------------------------------------------------------------+
|  FAST/SLOW PATH SUMMARY                                          |
+------------------------------------------------------------------+

    DESIGN PRINCIPLES:
    +----------------------------------------------------------+
    | 1. Identify the common case (99% of operations)           |
    | 2. Optimize relentlessly for that case                    |
    | 3. Isolate slow path into separate functions              |
    | 4. Use unlikely() to hint compiler                        |
    | 5. Pre-allocate resources for fast path                   |
    | 6. Defer complex work to background                       |
    +----------------------------------------------------------+
    
    FAST PATH REQUIREMENTS:
    +----------------------------------------------------------+
    | - No locks (or lock-free algorithms)                      |
    | - No allocation (use pools)                               |
    | - No sleeping                                             |
    | - Minimal branches                                        |
    | - Cache-friendly access patterns                          |
    +----------------------------------------------------------+
    
    SLOW PATH CHARACTERISTICS:
    +----------------------------------------------------------+
    | - Can acquire locks                                       |
    | - Can allocate memory                                     |
    | - Can sleep/block                                         |
    | - Handle errors and edge cases                            |
    | - Not performance critical                                |
    +----------------------------------------------------------+
    
    MEASUREMENT:
    +----------------------------------------------------------+
    | - Profile first to identify hot paths                     |
    | - Measure latency in nanoseconds for fast path            |
    | - Count instructions, cache misses                        |
    | - Test under load to find bottlenecks                     |
    +----------------------------------------------------------+
```

**中文总结：**
快路径/慢路径分离是内核性能的关键：
1. 识别常见情况（99% 的操作）并极致优化
2. 隔离慢路径到单独函数
3. 快路径要求：无锁、无分配、不睡眠、最小分支
4. 慢路径可以：获取锁、分配内存、睡眠阻塞
5. 性能测量：分析热路径、纳秒级延迟、缓存命中率

