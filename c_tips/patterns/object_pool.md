# Object Pool Pattern (对象池模式)

## 1. Core Concept and Use Cases

### Core Concept
Pre-create and manage a pool of reusable objects. When an object is needed, acquire it from the pool; when done, return it to the pool instead of destroying it. This **avoids expensive creation/destruction overhead**.

### Typical Use Cases
- Database connection pooling
- Thread pool management
- Memory buffer pools
- Network socket pools
- Game object management (bullets, particles)

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                    Object Pool Pattern                                            |
+--------------------------------------------------------------------------------------------------+

                              +---------------------------+
                              |       Object Pool         |
                              +---------------------------+
                              | - objects[MAX_SIZE]       |
                              | - free_list[]             |
                              | - in_use_count            |
                              +---------------------------+
                              | + acquire()               |
                              | + release(obj)            |
                              | + get_available_count()   |
                              +-------------+-------------+
                                            |
                      +---------------------+---------------------+
                      |                     |                     |
                      v                     v                     v
               +-------------+       +-------------+       +-------------+
               | Object 0    |       | Object 1    |       | Object N    |
               | [IN_USE]    |       | [FREE]      |       | [FREE]      |
               +-------------+       +-------------+       +-------------+


    Pool Operations:
    
    acquire():
    +-------+-------+-------+-------+-------+
    | Obj 0 | Obj 1 | Obj 2 | Obj 3 | ...   |   Pool
    | USED  | FREE  | FREE  | USED  |       |
    +-------+-------+-------+-------+-------+
                ^
                |
          Return Obj 1, mark as USED
          
    release(Obj 0):
    +-------+-------+-------+-------+-------+
    | Obj 0 | Obj 1 | Obj 2 | Obj 3 | ...   |   Pool
    | FREE  | USED  | FREE  | USED  |       |   <-- Obj 0 returned to pool
    +-------+-------+-------+-------+-------+
```

**中文说明：**

对象池模式的核心流程：

1. **初始化**：
   - 预先创建一定数量的对象
   - 所有对象初始状态为 FREE

2. **获取对象（acquire）**：
   - 从空闲列表中取出一个对象
   - 标记为 IN_USE
   - 返回给调用者

3. **释放对象（release）**：
   - 重置对象状态
   - 标记为 FREE
   - 放回空闲列表

---

## 3. Code Skeleton

```c
/* Pool object state */
typedef enum {
    OBJ_STATE_FREE,
    OBJ_STATE_IN_USE
} ObjectState;

/* Pooled object wrapper */
typedef struct {
    ObjectState state;
    void* data;
} PooledObject;

/* Object pool */
typedef struct {
    PooledObject objects[POOL_SIZE];
    int free_count;
    int total_count;
} ObjectPool;

/* Pool operations */
void pool_init(ObjectPool* pool);
void* pool_acquire(ObjectPool* pool);
void pool_release(ObjectPool* pool, void* obj);
void pool_destroy(ObjectPool* pool);
```

**中文说明：**

代码骨架包含：
- `ObjectState`：对象状态枚举
- `PooledObject`：池化对象包装
- `ObjectPool`：对象池管理结构
- 核心操作：`init`、`acquire`、`release`、`destroy`

---

## 4. Complete Example Code

```c
/*
 * Object Pool Pattern - Connection Pool Example
 * 
 * This example demonstrates a database connection pool
 * that reuses connection objects to avoid creation overhead.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define POOL_SIZE 5
#define MAX_HOST_LEN 64

/* ============================================
 * Pooled Object: Database Connection
 * ============================================ */
typedef enum {
    CONN_STATE_FREE,        /* Available in pool */
    CONN_STATE_IN_USE,      /* Currently acquired */
    CONN_STATE_INVALID      /* Needs reconnection */
} ConnectionState;

typedef struct {
    int id;                          /* Connection ID */
    ConnectionState state;           /* Current state */
    char host[MAX_HOST_LEN];         /* Database host */
    int port;                        /* Database port */
    int socket_fd;                   /* Simulated socket */
    time_t created_at;               /* Creation timestamp */
    time_t last_used;                /* Last usage timestamp */
    int query_count;                 /* Queries executed */
} DbConnection;

/* Connection operations */
void connection_init(DbConnection* conn, int id, const char* host, int port)
{
    conn->id = id;
    conn->state = CONN_STATE_FREE;
    strncpy(conn->host, host, MAX_HOST_LEN - 1);
    conn->port = port;
    conn->socket_fd = 1000 + id;  /* Simulated socket FD */
    conn->created_at = time(NULL);
    conn->last_used = 0;
    conn->query_count = 0;
    
    printf("    [Connection %d] Initialized (host=%s:%d, fd=%d)\n",
           id, host, port, conn->socket_fd);
}

void connection_reset(DbConnection* conn)
{
    conn->query_count = 0;
    printf("    [Connection %d] Reset for reuse\n", conn->id);
}

int connection_execute_query(DbConnection* conn, const char* query)
{
    if (conn->state != CONN_STATE_IN_USE) {
        printf("    [Connection %d] Error: Not in use!\n", conn->id);
        return -1;
    }
    
    conn->query_count++;
    conn->last_used = time(NULL);
    printf("    [Connection %d] Executing query #%d: %s\n",
           conn->id, conn->query_count, query);
    return 0;
}

void connection_print_stats(DbConnection* conn)
{
    const char* state_str = (conn->state == CONN_STATE_FREE) ? "FREE" :
                            (conn->state == CONN_STATE_IN_USE) ? "IN_USE" : "INVALID";
    printf("    Connection %d: state=%s, queries=%d\n",
           conn->id, state_str, conn->query_count);
}

/* ============================================
 * Object Pool: Connection Pool
 * ============================================ */
typedef struct {
    DbConnection connections[POOL_SIZE];  /* Pre-allocated connections */
    int free_count;                       /* Number of free connections */
    int total_acquired;                   /* Statistics: total acquires */
    int total_released;                   /* Statistics: total releases */
    char host[MAX_HOST_LEN];              /* Database host */
    int port;                             /* Database port */
} ConnectionPool;

/* Initialize the connection pool */
void pool_init(ConnectionPool* pool, const char* host, int port)
{
    printf("\n[Pool] Initializing connection pool (%d connections)\n", POOL_SIZE);
    
    strncpy(pool->host, host, MAX_HOST_LEN - 1);
    pool->port = port;
    pool->free_count = POOL_SIZE;
    pool->total_acquired = 0;
    pool->total_released = 0;
    
    /* Pre-create all connections */
    for (int i = 0; i < POOL_SIZE; i++) {
        connection_init(&pool->connections[i], i, host, port);
    }
    
    printf("[Pool] Initialization complete. %d connections available.\n\n", pool->free_count);
}

/* Acquire a connection from the pool */
DbConnection* pool_acquire(ConnectionPool* pool)
{
    printf("[Pool] Acquire requested (free: %d)\n", pool->free_count);
    
    /* Find a free connection */
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool->connections[i].state == CONN_STATE_FREE) {
            DbConnection* conn = &pool->connections[i];
            conn->state = CONN_STATE_IN_USE;
            conn->last_used = time(NULL);
            pool->free_count--;
            pool->total_acquired++;
            
            printf("[Pool] Acquired connection %d (free: %d)\n", 
                   conn->id, pool->free_count);
            return conn;
        }
    }
    
    /* No free connections */
    printf("[Pool] ERROR: No connections available!\n");
    return NULL;
}

/* Release a connection back to the pool */
void pool_release(ConnectionPool* pool, DbConnection* conn)
{
    if (conn == NULL) return;
    
    printf("[Pool] Releasing connection %d\n", conn->id);
    
    /* Verify the connection belongs to this pool */
    int found = 0;
    for (int i = 0; i < POOL_SIZE; i++) {
        if (&pool->connections[i] == conn) {
            found = 1;
            break;
        }
    }
    
    if (!found) {
        printf("[Pool] ERROR: Connection does not belong to this pool!\n");
        return;
    }
    
    if (conn->state != CONN_STATE_IN_USE) {
        printf("[Pool] WARNING: Connection %d was not in use!\n", conn->id);
        return;
    }
    
    /* Reset and return to pool */
    connection_reset(conn);
    conn->state = CONN_STATE_FREE;
    pool->free_count++;
    pool->total_released++;
    
    printf("[Pool] Connection %d released (free: %d)\n", 
           conn->id, pool->free_count);
}

/* Get number of available connections */
int pool_available(ConnectionPool* pool)
{
    return pool->free_count;
}

/* Print pool statistics */
void pool_print_stats(ConnectionPool* pool)
{
    printf("\n=== Connection Pool Statistics ===\n");
    printf("Host: %s:%d\n", pool->host, pool->port);
    printf("Total Size: %d\n", POOL_SIZE);
    printf("Available: %d\n", pool->free_count);
    printf("In Use: %d\n", POOL_SIZE - pool->free_count);
    printf("Total Acquired: %d\n", pool->total_acquired);
    printf("Total Released: %d\n", pool->total_released);
    printf("\nConnection Details:\n");
    for (int i = 0; i < POOL_SIZE; i++) {
        connection_print_stats(&pool->connections[i]);
    }
    printf("==================================\n\n");
}

/* Destroy the pool (cleanup) */
void pool_destroy(ConnectionPool* pool)
{
    printf("[Pool] Destroying connection pool\n");
    /* In a real implementation, close all connections here */
    for (int i = 0; i < POOL_SIZE; i++) {
        printf("    [Connection %d] Closed\n", i);
    }
}

/* ============================================
 * Client Code - Using the Pool
 * ============================================ */
void simulate_database_work(ConnectionPool* pool, const char* work_name)
{
    printf("\n--- %s ---\n", work_name);
    
    /* Acquire connection */
    DbConnection* conn = pool_acquire(pool);
    if (conn == NULL) {
        printf("Failed to acquire connection for %s\n", work_name);
        return;
    }
    
    /* Use connection */
    connection_execute_query(conn, "SELECT * FROM users");
    connection_execute_query(conn, "UPDATE users SET active=1");
    
    /* Release connection back to pool */
    pool_release(pool, conn);
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    ConnectionPool pool;
    
    printf("=== Object Pool Pattern Demo ===\n");
    
    /* Initialize pool */
    pool_init(&pool, "db.example.com", 5432);
    pool_print_stats(&pool);
    
    /* Scenario 1: Normal usage */
    printf("--- Scenario 1: Normal Usage ---\n");
    simulate_database_work(&pool, "Task A");
    simulate_database_work(&pool, "Task B");
    pool_print_stats(&pool);
    
    /* Scenario 2: Multiple concurrent connections */
    printf("--- Scenario 2: Multiple Concurrent Connections ---\n");
    DbConnection* conn1 = pool_acquire(&pool);
    DbConnection* conn2 = pool_acquire(&pool);
    DbConnection* conn3 = pool_acquire(&pool);
    
    printf("\nUsing multiple connections simultaneously:\n");
    if (conn1) connection_execute_query(conn1, "Query from conn1");
    if (conn2) connection_execute_query(conn2, "Query from conn2");
    if (conn3) connection_execute_query(conn3, "Query from conn3");
    
    pool_print_stats(&pool);
    
    /* Release connections */
    printf("\nReleasing connections:\n");
    pool_release(&pool, conn1);
    pool_release(&pool, conn2);
    pool_release(&pool, conn3);
    
    pool_print_stats(&pool);
    
    /* Scenario 3: Pool exhaustion */
    printf("--- Scenario 3: Pool Exhaustion ---\n");
    DbConnection* conns[POOL_SIZE + 2];
    
    printf("Acquiring all connections...\n");
    for (int i = 0; i < POOL_SIZE + 2; i++) {
        conns[i] = pool_acquire(&pool);
        if (conns[i] == NULL) {
            printf("  Could not acquire connection #%d (pool exhausted)\n", i);
        }
    }
    
    pool_print_stats(&pool);
    
    printf("Releasing all connections...\n");
    for (int i = 0; i < POOL_SIZE + 2; i++) {
        if (conns[i] != NULL) {
            pool_release(&pool, conns[i]);
        }
    }
    
    pool_print_stats(&pool);
    
    /* Cleanup */
    pool_destroy(&pool);
    
    printf("=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了数据库连接池：

1. **连接对象（DbConnection）**：
   - 包含连接 ID、状态、主机信息、统计数据
   - 提供 `init`、`reset`、`execute_query` 等方法

2. **连接池（ConnectionPool）**：
   - 预分配固定数量的连接对象
   - `pool_acquire()`：获取空闲连接
   - `pool_release()`：归还连接到池中

3. **演示场景**：
   - **正常使用**：获取连接、执行查询、释放连接
   - **并发使用**：同时获取多个连接
   - **池耗尽**：所有连接都在使用中时的处理

4. **优势体现**：
   - 避免重复创建/销毁连接的开销
   - 限制最大连接数
   - 连接复用，提高性能

