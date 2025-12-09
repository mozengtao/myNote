# Singleton Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                    SINGLETON PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    +------------------+                                           |
|    |    Client A      |                                           |
|    +--------+---------+                                           |
|             |                                                     |
|             |  get_instance()                                     |
|             v                                                     |
|    +--------+---------+                                           |
|    |    Singleton     |<------ Only ONE instance exists           |
|    +------------------+                                           |
|    | - static instance|        in the entire system               |
|    | - private data   |                                           |
|    +------------------+                                           |
|    | + get_instance() |                                           |
|    | + operation()    |                                           |
|    +--------+---------+                                           |
|             ^                                                     |
|             |  get_instance()                                     |
|             |                                                     |
|    +--------+---------+                                           |
|    |    Client B      |                                           |
|    +------------------+                                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 单例模式确保一个类只有一个实例，并提供一个全局访问点。在Linux内核中，许多全局子系统（如调度器、内存管理器、VFS）都采用这种模式，通过静态变量和初始化函数来保证全局唯一性。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: SEP Device Singleton

```c
/* From: drivers/staging/sep/sep_driver.c */

/* Global singleton device pointer */
static struct sep_device *sep_dev;

/**
 * sep_singleton_open - open function for the singleton driver
 * @inode_ptr: struct inode *
 * @file_ptr: struct file *
 *
 * Called when the user opens the singleton device interface.
 * Uses test_and_set_bit() to ensure only one access at a time.
 */
static int sep_singleton_open(struct inode *inode_ptr, struct file *file_ptr)
{
    struct sep_device *sep;

    /* Get the singleton SEP device structure */
    sep = sep_dev;
    file_ptr->private_data = sep;

    /* Atomic test-and-set ensures single access */
    if (test_and_set_bit(0, &sep->singleton_access_flag))
        return -EBUSY;  /* Already in use */
    return 0;
}

static int sep_singleton_release(struct inode *inode, struct file *filp)
{
    struct sep_device *sep = filp->private_data;
    /* Release the singleton lock */
    clear_bit(0, &sep->singleton_access_flag);
    return 0;
}
```

### 2.2 Kernel Example: Module Initialization (init_once pattern)

```c
/* From: init/main.c - start_kernel() is called exactly once */

asmlinkage void __init start_kernel(void)
{
    /* This function initializes the entire kernel - runs ONCE */
    smp_setup_processor_id();
    lockdep_init();
    debug_objects_early_init();
    boot_init_stack_canary();
    cgroup_init_early();
    /* ... many more one-time initializations ... */
    mm_init();
    sched_init();
    /* Kernel singleton subsystems are initialized here */
}
```

### 2.3 Architecture Diagram

```
+------------------------------------------------------------------+
|                 LINUX KERNEL SINGLETON ARCHITECTURE               |
+------------------------------------------------------------------+
|                                                                   |
|    Boot Process                                                   |
|    +-----------+                                                  |
|    | BIOS/UEFI |                                                  |
|    +-----+-----+                                                  |
|          |                                                        |
|          v                                                        |
|    +-----+-----+                                                  |
|    | Bootloader|                                                  |
|    +-----+-----+                                                  |
|          |                                                        |
|          v                                                        |
|    +-----+------------+     +--------------------------+          |
|    | start_kernel()   |---->| Initialize Singletons   |           |
|    | (called ONCE)    |     +--------------------------+          |
|    +---------+--------+     | - mm_init() (memory)    |           |
|              |              | - sched_init() (sched)  |           |
|              |              | - vfs_caches_init()     |           |
|              v              | - init_timers()         |           |
|    +---------+--------+     +--------------------------+          |
|    | Global Singleton |                                           |
|    | Variables        |                                           |
|    +------------------+                                           |
|    | static init_mm   |  <-- Only ONE memory descriptor           |
|    | static init_task |  <-- Only ONE init task                   |
|    +------------------+                                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 内核启动时，`start_kernel()` 函数只被调用一次，它初始化所有的单例子系统。这些子系统通过静态变量（如 `init_mm`、`init_task`）保证全局唯一性，并通过 `__init` 宏标记函数在初始化后可以被回收。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Resource Control** | Ensures only one instance of critical resources (hardware devices, kernel subsystems) |
| **Memory Efficiency** | Avoids duplicate allocation of large global structures |
| **Global Access Point** | Provides consistent interface for all kernel code to access shared resources |
| **Initialization Control** | Guarantees proper initialization order through explicit init functions |
| **Thread Safety** | Uses atomic operations (test_and_set_bit) for concurrent access control |

**中文说明：** 单例模式的优势在于资源控制（确保关键资源只有一个实例）、内存效率（避免重复分配）、全局访问点（统一接口）、初始化控制（保证初始化顺序）和线程安全（使用原子操作）。

---

## 4. User-Space Implementation Example

```c
/*
 * Singleton Pattern - User Space Implementation
 * Mimics Linux Kernel style with static variable + accessor function
 * 
 * Compile: gcc -o singleton singleton.c -pthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

/* ============================================================
 * Singleton Structure Definition
 * ============================================================ */

/* The singleton structure holding system configuration */
struct system_config {
    int max_connections;        /* Maximum allowed connections */
    int timeout_seconds;        /* Connection timeout value */
    char server_name[64];       /* Server identification name */
    bool initialized;           /* Initialization flag */
    pthread_mutex_t lock;       /* Mutex for thread safety */
};

/* ============================================================
 * Static Instance - The actual singleton (like kernel's sep_dev)
 * ============================================================ */

/* Static singleton instance - only ONE exists in the entire program */
static struct system_config *g_config = NULL;

/* Mutex for initialization synchronization */
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================
 * Singleton Accessor Functions
 * ============================================================ */

/**
 * get_system_config - Get the singleton instance
 * 
 * This function implements lazy initialization with double-checked
 * locking pattern for thread safety. Similar to kernel's module_init().
 *
 * Returns: Pointer to the singleton configuration structure
 */
struct system_config *get_system_config(void)
{
    /* First check without lock (fast path) */
    if (g_config != NULL && g_config->initialized) {
        return g_config;
    }

    /* Lock for initialization (slow path) */
    pthread_mutex_lock(&init_mutex);
    
    /* Double-check after acquiring lock */
    if (g_config == NULL) {
        /* Allocate the singleton instance */
        g_config = (struct system_config *)malloc(sizeof(struct system_config));
        if (g_config == NULL) {
            pthread_mutex_unlock(&init_mutex);
            return NULL;
        }

        /* Initialize with default values (like kernel's __init functions) */
        g_config->max_connections = 100;
        g_config->timeout_seconds = 30;
        snprintf(g_config->server_name, sizeof(g_config->server_name), 
                 "DefaultServer");
        pthread_mutex_init(&g_config->lock, NULL);
        
        /* Mark as initialized - memory barrier implied by mutex */
        g_config->initialized = true;
        
        printf("[INIT] Singleton instance created at %p\n", (void *)g_config);
    }
    
    pthread_mutex_unlock(&init_mutex);
    return g_config;
}

/**
 * destroy_system_config - Clean up the singleton
 * 
 * Similar to kernel's module_exit() cleanup functions.
 * Should only be called at program termination.
 */
void destroy_system_config(void)
{
    pthread_mutex_lock(&init_mutex);
    
    if (g_config != NULL) {
        pthread_mutex_destroy(&g_config->lock);
        free(g_config);
        g_config = NULL;
        printf("[EXIT] Singleton instance destroyed\n");
    }
    
    pthread_mutex_unlock(&init_mutex);
}

/* ============================================================
 * Thread-Safe Operations on Singleton
 * ============================================================ */

/**
 * config_set_max_connections - Thread-safe setter
 * @max_conn: New maximum connections value
 *
 * Uses internal mutex to protect concurrent modifications.
 */
void config_set_max_connections(int max_conn)
{
    struct system_config *cfg = get_system_config();
    if (cfg == NULL) return;
    
    pthread_mutex_lock(&cfg->lock);
    cfg->max_connections = max_conn;
    pthread_mutex_unlock(&cfg->lock);
}

/**
 * config_get_max_connections - Thread-safe getter
 *
 * Returns: Current max_connections value
 */
int config_get_max_connections(void)
{
    struct system_config *cfg = get_system_config();
    int val;
    
    if (cfg == NULL) return -1;
    
    pthread_mutex_lock(&cfg->lock);
    val = cfg->max_connections;
    pthread_mutex_unlock(&cfg->lock);
    
    return val;
}

/* ============================================================
 * Test Functions - Demonstrate Singleton Behavior
 * ============================================================ */

/* Thread function to test singleton access */
void *thread_func(void *arg)
{
    int thread_id = *(int *)arg;
    
    /* All threads get the SAME instance */
    struct system_config *cfg = get_system_config();
    
    printf("Thread %d: Got config at %p, max_conn=%d\n",
           thread_id, (void *)cfg, config_get_max_connections());
    
    /* Modify shared state */
    config_set_max_connections(config_get_max_connections() + 1);
    
    return NULL;
}

int main(void)
{
    pthread_t threads[5];
    int thread_ids[5];
    int i;

    printf("=== Singleton Pattern Demo ===\n\n");

    /* Multiple threads accessing the singleton */
    printf("[TEST] Creating 5 threads to access singleton...\n");
    
    for (i = 0; i < 5; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
    }

    for (i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify all threads modified the same instance */
    printf("\n[RESULT] Final max_connections = %d (started at 100)\n",
           config_get_max_connections());
    
    /* Verify same instance returned multiple times */
    struct system_config *cfg1 = get_system_config();
    struct system_config *cfg2 = get_system_config();
    printf("[VERIFY] cfg1=%p, cfg2=%p, same=%s\n",
           (void *)cfg1, (void *)cfg2,
           (cfg1 == cfg2) ? "YES" : "NO");

    /* Cleanup */
    destroy_system_config();

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Execution Flow Diagram

```
+------------------------------------------------------------------+
|                    SINGLETON EXECUTION FLOW                       |
+------------------------------------------------------------------+
|                                                                   |
|    Thread 1              Thread 2              Thread 3           |
|       |                     |                     |               |
|       v                     v                     v               |
|  get_system_config()   get_system_config()   get_system_config() |
|       |                     |                     |               |
|       v                     v                     v               |
|  +----+----+           +----+----+           +----+----+          |
|  | g_config|           | g_config|           | g_config|          |
|  | == NULL?|           | == NULL?|           | == NULL?|          |
|  +----+----+           +----+----+           +----+----+          |
|       | YES                 | YES                 | NO            |
|       v                     v                     |               |
|  +----+--------+       +----+--------+            |               |
|  | Lock mutex  |       | Lock mutex  |            |               |
|  | (blocked)   |<------| (waiting)   |            |               |
|  +----+--------+       +-------------+            |               |
|       |                     |                     |               |
|       v                     |                     |               |
|  +----+--------+            |                     |               |
|  | Create      |            |                     |               |
|  | Instance    |            |                     |               |
|  +----+--------+            |                     |               |
|       |                     |                     |               |
|       v                     v                     v               |
|  +----+--------+       +----+--------+       +----+--------+      |
|  | Return      |       | Return      |       | Return      |      |
|  | g_config    |       | g_config    |       | g_config    |      |
|  +----+--------+       +----+--------+       +----+--------+      |
|       |                     |                     |               |
|       +----------+----------+----------+----------+               |
|                  |                                                |
|                  v                                                |
|         +-------+--------+                                        |
|         | SAME INSTANCE  |                                        |
|         | (0x7f8a3c00)   |                                        |
|         +----------------+                                        |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 上图展示了多线程访问单例时的执行流程。第一个线程获取锁并创建实例，其他线程等待；所有线程最终都返回同一个实例地址，保证了全局唯一性。

---

## 6. Key Implementation Points

1. **Static Variable**: Use `static` to limit visibility and ensure single instance
2. **Lazy Initialization**: Create instance only when first needed
3. **Thread Safety**: Use mutex or atomic operations for concurrent access
4. **Double-Checked Locking**: Optimize for common case (already initialized)
5. **Cleanup Function**: Provide explicit cleanup for resource management

**中文说明：** 实现单例模式的关键点包括：使用静态变量限制可见性、延迟初始化节省资源、使用互斥锁保证线程安全、双重检查锁优化性能、提供清理函数管理资源。

