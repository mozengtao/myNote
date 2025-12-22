# Linux Kernel Debug Infrastructure (v3.2)

## Overview

This document explains **kernel debugging and tracing architecture** in v3.2, focusing on tracepoints, debugfs, and non-intrusive observability.

---

## Tracepoints Architecture

```
+------------------------------------------------------------------+
|  TRACEPOINT DESIGN                                               |
+------------------------------------------------------------------+

    WHAT ARE TRACEPOINTS:
    +----------------------------------------------------------+
    | - Static instrumentation points in kernel code            |
    | - Zero overhead when disabled                             |
    | - Can be enabled at runtime                               |
    | - Used by ftrace, perf, SystemTap                         |
    +----------------------------------------------------------+

    DEFINITION:
    
    /* In include/trace/events/sched.h */
    TRACE_EVENT(sched_switch,
        TP_PROTO(struct task_struct *prev, struct task_struct *next),
        
        TP_ARGS(prev, next),
        
        TP_STRUCT__entry(
            __array(char, prev_comm, TASK_COMM_LEN)
            __field(pid_t, prev_pid)
            __field(int, prev_prio)
            __field(long, prev_state)
            __array(char, next_comm, TASK_COMM_LEN)
            __field(pid_t, next_pid)
            __field(int, next_prio)
        ),
        
        TP_fast_assign(
            memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
            __entry->prev_pid   = prev->pid;
            __entry->prev_prio  = prev->prio;
            __entry->prev_state = prev->state;
            memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
            __entry->next_pid   = next->pid;
            __entry->next_prio  = next->prio;
        ),
        
        TP_printk("prev_comm=%s prev_pid=%d ... => next_comm=%s ...",
            __entry->prev_comm, __entry->prev_pid, ...)
    );

    USAGE IN KERNEL CODE:
    
    /* In kernel/sched/core.c */
    static void __sched __schedule(void)
    {
        ...
        trace_sched_switch(prev, next);  /* Tracepoint call */
        ...
    }

    EXPANSION (simplified):
    
    static inline void trace_sched_switch(...)
    {
        if (unlikely(tracepoint_enabled))   /* Branch prediction */
            __trace_sched_switch(...);      /* Actual tracing */
    }
```

```
+------------------------------------------------------------------+
|  TRACEPOINT PERFORMANCE MODEL                                    |
+------------------------------------------------------------------+

    DISABLED TRACEPOINT:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  if (unlikely(static_key)) {  /* Patched to NOP */          │
    │      /* Never executed */                                   │
    │  }                                                          │
    │                                                              │
    │  Overhead: ~0 (jump patched to NOP at runtime)              │
    └─────────────────────────────────────────────────────────────┘

    ENABLED TRACEPOINT:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  if (likely(1)) {                                           │
    │      copy_args_to_buffer();                                 │
    │      call_registered_probes();                              │
    │  }                                                          │
    │                                                             │
    │  Overhead: ~100-500 ns (depends on probe complexity)        │
    └─────────────────────────────────────────────────────────────┘

    ZERO-OVERHEAD WHEN DISABLED:
    +----------------------------------------------------------+
    | Uses "static keys" (jump label patching)                  |
    | Disabled: JMP instruction patched to NOP                  |
    | Enabled: NOP patched back to JMP                          |
    | No branch prediction overhead!                            |
    +----------------------------------------------------------+
```

**中文解释：**
- Tracepoint：内核代码中的静态插桩点
- 禁用时零开销：使用静态键，JMP 被补丁为 NOP
- 启用时：复制参数到缓冲区，调用注册的探针
- 用途：ftrace、perf、SystemTap

---

## debugfs Architecture

```
+------------------------------------------------------------------+
|  DEBUGFS: DEBUG FILESYSTEM                                       |
+------------------------------------------------------------------+

    PURPOSE:
    +----------------------------------------------------------+
    | - Expose kernel internals to user space                   |
    | - For debugging only (not stable ABI)                     |
    | - Simple file interface for read/write                    |
    | - Mount at /sys/kernel/debug                              |
    +----------------------------------------------------------+

    CREATING DEBUGFS FILES:
    
    /* In driver initialization */
    static struct dentry *my_debugfs_dir;
    static int my_debug_value;
    
    static int __init my_init(void)
    {
        /* Create directory */
        my_debugfs_dir = debugfs_create_dir("my_driver", NULL);
        if (!my_debugfs_dir)
            return -ENOMEM;
        
        /* Create integer file (read/write) */
        debugfs_create_u32("debug_level", 0644, my_debugfs_dir,
                           &my_debug_value);
        
        /* Create blob file */
        debugfs_create_blob("firmware", 0444, my_debugfs_dir,
                            &my_firmware_blob);
        
        /* Create custom file */
        debugfs_create_file("status", 0444, my_debugfs_dir,
                            NULL, &my_status_fops);
        
        return 0;
    }

    RESULT IN FILESYSTEM:
    
    /sys/kernel/debug/
    └── my_driver/
        ├── debug_level     ← echo 3 > debug_level
        ├── firmware        ← cat firmware
        └── status          ← cat status

    CUSTOM FILE OPERATIONS:
    
    static ssize_t status_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
    {
        char tmp[256];
        int len;
        
        len = snprintf(tmp, sizeof(tmp),
                       "State: %s\nErrors: %d\n",
                       state_string, error_count);
        
        return simple_read_from_buffer(buf, count, ppos, tmp, len);
    }
    
    static const struct file_operations my_status_fops = {
        .read = status_read,
        .llseek = default_llseek,
    };
```

**中文解释：**
- debugfs：调试文件系统，暴露内核内部给用户空间
- 挂载于 /sys/kernel/debug
- 创建：目录、整数文件、blob、自定义文件
- 不是稳定 ABI，仅用于调试

---

## Performance Constraints

```
+------------------------------------------------------------------+
|  DEBUG INFRASTRUCTURE PERFORMANCE RULES                          |
+------------------------------------------------------------------+

    RULE 1: Zero Cost When Disabled
    +----------------------------------------------------------+
    | Tracepoints, debug prints must have ZERO overhead         |
    | when not actively used.                                   |
    |                                                           |
    | Implementation:                                           |
    | - Static keys (jump label patching)                       |
    | - CONFIG options                                          |
    | - unlikely() hints                                        |
    +----------------------------------------------------------+

    RULE 2: Bounded Impact When Enabled
    +----------------------------------------------------------+
    | Even when enabled, impact must be bounded and             |
    | predictable.                                              |
    |                                                           |
    | Bad: Unbounded string formatting in hot path              |
    | Good: Copy fixed-size struct to ring buffer               |
    +----------------------------------------------------------+

    RULE 3: No Locking in Fast Path
    +----------------------------------------------------------+
    | Tracepoints must not acquire locks that could block       |
    | the traced code path.                                     |
    |                                                           |
    | Solution: Per-CPU ring buffers, lock-free operations      |
    +----------------------------------------------------------+

    RULE 4: Safe in All Contexts
    +----------------------------------------------------------+
    | Tracepoints may be called in:                             |
    | - IRQ context                                             |
    | - NMI context                                             |
    | - With locks held                                         |
    |                                                           |
    | Must not: sleep, allocate memory, cause recursion         |
    +----------------------------------------------------------+

    FTRACE RING BUFFER:
    
    ┌─────────────────────────────────────────────────────────────┐
    │                   Per-CPU Ring Buffer                        │
    │                                                              │
    │  CPU 0              CPU 1              CPU 2                │
    │  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐       │
    │  │ event       │   │ event       │   │ event       │       │
    │  │ event       │   │ event       │   │ event       │       │
    │  │ event       │   │ (empty)     │   │ event       │       │
    │  │ ↑ write     │   │ ↑ write     │   │ ↑ write     │       │
    │  └─────────────┘   └─────────────┘   └─────────────┘       │
    │                                                              │
    │  Benefits:                                                   │
    │  - No cross-CPU contention                                  │
    │  - Lock-free writes                                         │
    │  - Bounded memory usage                                     │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 规则1：禁用时零开销
- 规则2：启用时影响有界
- 规则3：快速路径无锁
- 规则4：所有上下文安全
- ftrace 环形缓冲区：每 CPU 独立，无锁写入

---

## Real Kernel Use Cases

```
+------------------------------------------------------------------+
|  KERNEL DEBUG USE CASES                                          |
+------------------------------------------------------------------+

    1. SCHEDULER TRACING
    +----------------------------------------------------------+
    | Tracepoints: sched_switch, sched_wakeup, sched_migrate    |
    |                                                           |
    | Use: Analyze scheduling latency, CPU affinity             |
    |                                                           |
    | # echo 1 > /sys/kernel/debug/tracing/events/sched/enable  |
    | # cat /sys/kernel/debug/tracing/trace                     |
    +----------------------------------------------------------+

    2. BLOCK I/O TRACING
    +----------------------------------------------------------+
    | Tracepoints: block_rq_issue, block_rq_complete            |
    |                                                           |
    | Use: Analyze I/O latency, queue depth                     |
    +----------------------------------------------------------+

    3. NETWORK TRACING
    +----------------------------------------------------------+
    | Tracepoints: net_dev_xmit, netif_receive_skb              |
    |                                                           |
    | Use: Debug packet drops, latency analysis                 |
    +----------------------------------------------------------+

    4. MEMORY ALLOCATION DEBUGGING
    +----------------------------------------------------------+
    | debugfs: /sys/kernel/debug/slab/                          |
    | Tracepoints: kmalloc, kfree                               |
    |                                                           |
    | Use: Find memory leaks, allocation patterns               |
    +----------------------------------------------------------+

    5. DRIVER DEBUGGING
    +----------------------------------------------------------+
    | debugfs: Driver-specific files                            |
    | Dynamic debug: pr_debug() with runtime enable             |
    |                                                           |
    | # echo 'module my_driver +p' > \                          |
    |     /sys/kernel/debug/dynamic_debug/control               |
    +----------------------------------------------------------+
```

**中文解释：**
- 调度器追踪：分析调度延迟、CPU 亲和性
- 块 I/O 追踪：分析 I/O 延迟、队列深度
- 网络追踪：调试丢包、延迟分析
- 内存分配调试：查找内存泄漏
- 驱动调试：动态调试 pr_debug

---

## User-Space Diagnostics

```c
/* User-space observability patterns */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/*=================================================================
 * PATTERN 1: Zero-overhead tracepoints
 *================================================================*/
#define TRACEPOINT_ENABLED 0

#if TRACEPOINT_ENABLED
#define TRACE(fmt, ...) trace_event(__func__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...) ((void)0)  /* Compiles to nothing */
#endif

void trace_event(const char *func, int line, const char *fmt, ...)
{
    /* Implementation when enabled */
}

/*=================================================================
 * PATTERN 2: Ring buffer for events
 *================================================================*/
#define RING_SIZE 1024

struct trace_entry {
    uint64_t timestamp;
    int cpu;
    int pid;
    char event[64];
};

struct ring_buffer {
    struct trace_entry entries[RING_SIZE];
    volatile int write_idx;
    pthread_mutex_t read_lock;
};

static __thread struct ring_buffer *local_ring;

void ring_init(void)
{
    local_ring = malloc(sizeof(*local_ring));
    local_ring->write_idx = 0;
    pthread_mutex_init(&local_ring->read_lock, NULL);
}

/* Lock-free write (single producer) */
void ring_write(const char *event)
{
    int idx = local_ring->write_idx % RING_SIZE;
    struct trace_entry *e = &local_ring->entries[idx];
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    e->timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    e->cpu = sched_getcpu();
    e->pid = getpid();
    strncpy(e->event, event, sizeof(e->event) - 1);
    
    __atomic_fetch_add(&local_ring->write_idx, 1, __ATOMIC_RELEASE);
}

/*=================================================================
 * PATTERN 3: Debug file interface
 *================================================================*/
struct debug_file {
    const char *name;
    int (*show)(char *buf, size_t size);
    int (*store)(const char *buf, size_t size);
};

static int debug_level = 0;
static int error_count = 0;

int show_status(char *buf, size_t size)
{
    return snprintf(buf, size, 
                    "Debug level: %d\n"
                    "Error count: %d\n",
                    debug_level, error_count);
}

int store_level(const char *buf, size_t size)
{
    debug_level = atoi(buf);
    return size;
}

struct debug_file debug_files[] = {
    { "status", show_status, NULL },
    { "level", NULL, store_level },
    { NULL, NULL, NULL }
};

/* Simple debug server (like debugfs) */
void *debug_server(void *arg)
{
    /* Could expose via Unix socket, HTTP, etc. */
    return NULL;
}

/*=================================================================
 * PATTERN 4: Metrics collection
 *================================================================*/
struct metrics {
    /* Counters (monotonic) */
    uint64_t requests_total;
    uint64_t errors_total;
    
    /* Gauges (current value) */
    int active_connections;
    int queue_depth;
    
    /* Histograms (distribution) */
    uint64_t latency_buckets[10];  /* <1ms, <2ms, <5ms, ... */
};

static __thread struct metrics local_metrics;
static struct metrics global_metrics;

void metrics_inc_requests(void)
{
    __atomic_fetch_add(&local_metrics.requests_total, 1, 
                       __ATOMIC_RELAXED);
}

void metrics_record_latency(uint64_t latency_us)
{
    int bucket;
    if (latency_us < 1000) bucket = 0;
    else if (latency_us < 2000) bucket = 1;
    else if (latency_us < 5000) bucket = 2;
    /* ... */
    else bucket = 9;
    
    __atomic_fetch_add(&local_metrics.latency_buckets[bucket], 1,
                       __ATOMIC_RELAXED);
}

void metrics_aggregate(void)
{
    /* Periodically aggregate per-thread metrics */
    __atomic_fetch_add(&global_metrics.requests_total,
                       local_metrics.requests_total,
                       __ATOMIC_RELAXED);
    local_metrics.requests_total = 0;
}

/*=================================================================
 * SUMMARY: Key principles
 *================================================================*/
/*
    1. ZERO OVERHEAD WHEN OFF
       - Use macros that compile to nothing
       - Or function pointers that are NULL-checked
    
    2. BOUNDED OVERHEAD WHEN ON
       - Fixed-size entries
       - Lock-free where possible
       - Per-CPU/thread buffers
    
    3. SIMPLE INTERFACES
       - Text files for humans
       - Binary protocols for tools
    
    4. SAFE IN ALL CONTEXTS
       - No allocation in hot path
       - No blocking operations
*/
```

**中文解释：**
- 模式1：零开销 tracepoint（禁用时编译为空）
- 模式2：环形缓冲区（无锁写入，每线程独立）
- 模式3：调试文件接口（类似 debugfs）
- 模式4：指标收集（计数器、仪表、直方图）
- 原则：禁用零开销、启用有界、简单接口、安全上下文

