# 内存池 (Memory Pool)

## 定义

内存池是一种内存管理技术，预先分配一大块连续内存，然后从中分配和回收固定大小的内存块。通过避免频繁的系统调用（malloc/free），内存池可以显著提高内存分配效率并消除内存碎片。

## 适用场景

- 游戏开发中频繁创建和销毁的实体对象
- 网络服务器的连接对象管理
- 实时系统中需要确定性分配时间的场景
- 嵌入式系统中内存资源受限的情况
- 需要避免内存碎片的长期运行程序
- 对象池（Object Pool）的底层实现
- 粒子系统、子弹等大量相同类型对象的管理

## ASCII 图解

```
+------------------------------------------------------------------------+
|                          MEMORY POOL                                    |
+------------------------------------------------------------------------+
|                                                                         |
|   Traditional malloc/free:           Memory Pool:                       |
|   +-------+                          +---------------------------+      |
|   |malloc |---> OS Kernel            | Pre-allocated Memory      |      |
|   +-------+     (syscall,            +---------------------------+      |
|       |         slow)                | Block | Block | Block |...|      |
|       v                              +---------------------------+      |
|   +-------+                                    |                        |
|   |malloc |---> OS Kernel                 pool_alloc()                  |
|   +-------+     (syscall,                  O(1) time!                   |
|       |         slow)                      No syscall!                  |
|       v                                                                 |
|   Fragmentation issues               No fragmentation!                  |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   FREE LIST STRUCTURE:                                                  |
|                                                                         |
|   Initial state (all blocks free):                                      |
|                                                                         |
|   free_list                                                             |
|      |                                                                  |
|      v                                                                  |
|   +------+    +------+    +------+    +------+    +------+             |
|   |  B0  |--->|  B1  |--->|  B2  |--->|  B3  |--->|  B4  |---> NULL    |
|   +------+    +------+    +------+    +------+    +------+             |
|                                                                         |
|   After allocating B0 and B1:                                           |
|                                                                         |
|   free_list                                                             |
|      |                                                                  |
|      v                                                                  |
|   +------+    +------+    +------+                                      |
|   |  B2  |--->|  B3  |--->|  B4  |---> NULL                            |
|   +------+    +------+    +------+                                      |
|                                                                         |
|   After freeing B0:                                                     |
|                                                                         |
|   free_list                                                             |
|      |                                                                  |
|      v                                                                  |
|   +------+    +------+    +------+    +------+                          |
|   |  B0  |--->|  B2  |--->|  B3  |--->|  B4  |---> NULL                |
|   +------+    +------+    +------+    +------+                          |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   MEMORY LAYOUT:                                                        |
|                                                                         |
|   +----+----+----+----+----+----+----+----+----+----+----+----+        |
|   | B0 | B1 | B2 | B3 | B4 | B5 | B6 | B7 | B8 | B9 |B10 |B11 |        |
|   +----+----+----+----+----+----+----+----+----+----+----+----+        |
|   |<-- block_size -->|                                                  |
|   |                                                                     |
|   |<------------------ total allocated memory ------------------->|    |
|                                                                         |
|   Each block stores pointer to next free block when not in use.        |
|   No additional metadata overhead!                                      |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图对比了传统malloc与内存池的区别。传统方式每次分配都需要系统调用，速度慢且会产生碎片。内存池预先分配连续内存块，通过空闲链表管理。每个空闲块内部存储指向下一个空闲块的指针，分配时从链表头部取出一个块（O(1)），释放时将块加入链表头部（O(1)）。这种设计无需额外的元数据开销，且保证分配时间确定性。

## 实现方法

1. 预分配一大块连续内存
2. 将内存划分为固定大小的块
3. 用空闲链表将所有块串联
4. 分配时从链表头取块，释放时加入链表头
5. 可选：支持池扩展或多池管理

## C语言代码示例

### 基础内存池实现

```c
// memory_pool.h
#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>

typedef struct MemoryPool MemoryPool;

// 创建内存池
// block_size: 每个块的大小
// block_count: 块的数量
MemoryPool* pool_create(size_t block_size, size_t block_count);

// 从池中分配一个块
void* pool_alloc(MemoryPool* pool);

// 将块释放回池中
void pool_free(MemoryPool* pool, void* ptr);

// 重置池（释放所有块）
void pool_reset(MemoryPool* pool);

// 获取统计信息
size_t pool_get_used_count(MemoryPool* pool);
size_t pool_get_free_count(MemoryPool* pool);
size_t pool_get_capacity(MemoryPool* pool);
void pool_print_stats(MemoryPool* pool);

// 销毁内存池
void pool_destroy(MemoryPool* pool);

#endif // MEMORY_POOL_H
```

```c
// memory_pool.c
#include "memory_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 内存池结构
struct MemoryPool {
    void* memory;           // 内存块起始地址
    void* free_list;        // 空闲链表头
    size_t block_size;      // 每个块的大小
    size_t block_count;     // 块总数
    size_t used_count;      // 已使用块数
    size_t alloc_count;     // 总分配次数（统计用）
    size_t free_count_stat; // 总释放次数（统计用）
};

// 确保块大小至少能存放一个指针
static size_t align_block_size(size_t size) {
    size_t min_size = sizeof(void*);
    if (size < min_size) {
        return min_size;
    }
    // 对齐到指针大小的倍数
    return (size + min_size - 1) & ~(min_size - 1);
}

MemoryPool* pool_create(size_t block_size, size_t block_count) {
    if (block_count == 0) {
        return NULL;
    }
    
    MemoryPool* pool = (MemoryPool*)malloc(sizeof(MemoryPool));
    if (!pool) {
        return NULL;
    }
    
    pool->block_size = align_block_size(block_size);
    pool->block_count = block_count;
    
    // 分配内存块
    size_t total_size = pool->block_size * block_count;
    pool->memory = malloc(total_size);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }
    
    // 初始化空闲链表
    pool->free_list = pool->memory;
    char* current = (char*)pool->memory;
    
    for (size_t i = 0; i < block_count - 1; i++) {
        char* next = current + pool->block_size;
        *(void**)current = next;
        current = next;
    }
    *(void**)current = NULL;  // 最后一块
    
    pool->used_count = 0;
    pool->alloc_count = 0;
    pool->free_count_stat = 0;
    
    printf("[Pool] Created: %zu blocks x %zu bytes = %zu bytes total\n",
           block_count, pool->block_size, total_size);
    
    return pool;
}

void* pool_alloc(MemoryPool* pool) {
    if (!pool) {
        return NULL;
    }
    
    if (!pool->free_list) {
        printf("[Pool] WARNING: Pool exhausted!\n");
        return NULL;
    }
    
    // 从空闲链表头取出一个块
    void* block = pool->free_list;
    pool->free_list = *(void**)block;
    
    // 清零并更新统计
    memset(block, 0, pool->block_size);
    pool->used_count++;
    pool->alloc_count++;
    
    return block;
}

void pool_free(MemoryPool* pool, void* ptr) {
    if (!pool || !ptr) {
        return;
    }
    
    // 检查指针是否在池的范围内
    char* start = (char*)pool->memory;
    char* end = start + pool->block_size * pool->block_count;
    char* p = (char*)ptr;
    
    if (p < start || p >= end) {
        printf("[Pool] ERROR: Pointer %p not in pool range!\n", ptr);
        return;
    }
    
    // 检查对齐
    if ((p - start) % pool->block_size != 0) {
        printf("[Pool] ERROR: Pointer %p not aligned to block boundary!\n", ptr);
        return;
    }
    
    // 加入空闲链表头
    *(void**)ptr = pool->free_list;
    pool->free_list = ptr;
    
    pool->used_count--;
    pool->free_count_stat++;
}

void pool_reset(MemoryPool* pool) {
    if (!pool) return;
    
    // 重新初始化空闲链表
    pool->free_list = pool->memory;
    char* current = (char*)pool->memory;
    
    for (size_t i = 0; i < pool->block_count - 1; i++) {
        char* next = current + pool->block_size;
        *(void**)current = next;
        current = next;
    }
    *(void**)current = NULL;
    
    pool->used_count = 0;
    printf("[Pool] Reset complete\n");
}

size_t pool_get_used_count(MemoryPool* pool) {
    return pool ? pool->used_count : 0;
}

size_t pool_get_free_count(MemoryPool* pool) {
    return pool ? (pool->block_count - pool->used_count) : 0;
}

size_t pool_get_capacity(MemoryPool* pool) {
    return pool ? pool->block_count : 0;
}

void pool_print_stats(MemoryPool* pool) {
    if (!pool) return;
    
    double usage = (double)pool->used_count / pool->block_count * 100.0;
    
    printf("\n+======= Memory Pool Stats =======+\n");
    printf("| Block size:   %10zu bytes |\n", pool->block_size);
    printf("| Capacity:     %10zu blocks|\n", pool->block_count);
    printf("| Used:         %10zu blocks|\n", pool->used_count);
    printf("| Free:         %10zu blocks|\n", pool->block_count - pool->used_count);
    printf("| Usage:        %10.1f%%      |\n", usage);
    printf("+---------------------------------+\n");
    printf("| Total allocs: %10zu       |\n", pool->alloc_count);
    printf("| Total frees:  %10zu       |\n", pool->free_count_stat);
    printf("+=================================+\n\n");
}

void pool_destroy(MemoryPool* pool) {
    if (pool) {
        if (pool->used_count > 0) {
            printf("[Pool] WARNING: %zu blocks still in use at destroy!\n",
                   pool->used_count);
        }
        printf("[Pool] Destroyed (allocs: %zu, frees: %zu)\n",
               pool->alloc_count, pool->free_count_stat);
        free(pool->memory);
        free(pool);
    }
}
```

### 可扩展内存池

```c
// expandable_pool.h
#ifndef EXPANDABLE_POOL_H
#define EXPANDABLE_POOL_H

#include <stddef.h>

typedef struct ExpandablePool ExpandablePool;

ExpandablePool* epool_create(size_t block_size, size_t initial_blocks);
void* epool_alloc(ExpandablePool* pool);
void epool_free(ExpandablePool* pool, void* ptr);
void epool_print_stats(ExpandablePool* pool);
void epool_destroy(ExpandablePool* pool);

#endif
```

```c
// expandable_pool.c
#include "expandable_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHUNKS 64

typedef struct {
    void* memory;
    size_t block_count;
} PoolChunk;

struct ExpandablePool {
    PoolChunk chunks[MAX_CHUNKS];
    int chunk_count;
    void* free_list;
    size_t block_size;
    size_t blocks_per_chunk;
    size_t total_blocks;
    size_t used_blocks;
};

static size_t align_size(size_t size) {
    size_t align = sizeof(void*);
    return (size + align - 1) & ~(align - 1);
}

static int add_chunk(ExpandablePool* pool) {
    if (pool->chunk_count >= MAX_CHUNKS) {
        printf("[EPool] Cannot add more chunks (max: %d)\n", MAX_CHUNKS);
        return -1;
    }
    
    size_t chunk_size = pool->block_size * pool->blocks_per_chunk;
    void* memory = malloc(chunk_size);
    if (!memory) {
        return -1;
    }
    
    // 初始化新块的空闲链表
    char* current = (char*)memory;
    for (size_t i = 0; i < pool->blocks_per_chunk - 1; i++) {
        char* next = current + pool->block_size;
        *(void**)current = next;
        current = next;
    }
    // 链接到现有空闲链表
    *(void**)current = pool->free_list;
    pool->free_list = memory;
    
    // 记录chunk
    pool->chunks[pool->chunk_count].memory = memory;
    pool->chunks[pool->chunk_count].block_count = pool->blocks_per_chunk;
    pool->chunk_count++;
    pool->total_blocks += pool->blocks_per_chunk;
    
    printf("[EPool] Added chunk #%d (%zu blocks)\n", 
           pool->chunk_count, pool->blocks_per_chunk);
    
    return 0;
}

ExpandablePool* epool_create(size_t block_size, size_t initial_blocks) {
    ExpandablePool* pool = (ExpandablePool*)calloc(1, sizeof(ExpandablePool));
    if (!pool) return NULL;
    
    pool->block_size = align_size(block_size < sizeof(void*) ? sizeof(void*) : block_size);
    pool->blocks_per_chunk = initial_blocks;
    pool->free_list = NULL;
    pool->chunk_count = 0;
    pool->total_blocks = 0;
    pool->used_blocks = 0;
    
    if (add_chunk(pool) != 0) {
        free(pool);
        return NULL;
    }
    
    printf("[EPool] Created with block_size=%zu, initial=%zu\n",
           pool->block_size, initial_blocks);
    
    return pool;
}

void* epool_alloc(ExpandablePool* pool) {
    if (!pool) return NULL;
    
    // 如果没有空闲块，扩展池
    if (!pool->free_list) {
        if (add_chunk(pool) != 0) {
            printf("[EPool] Failed to expand pool!\n");
            return NULL;
        }
    }
    
    void* block = pool->free_list;
    pool->free_list = *(void**)block;
    memset(block, 0, pool->block_size);
    pool->used_blocks++;
    
    return block;
}

void epool_free(ExpandablePool* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    *(void**)ptr = pool->free_list;
    pool->free_list = ptr;
    pool->used_blocks--;
}

void epool_print_stats(ExpandablePool* pool) {
    if (!pool) return;
    
    printf("\n+==== Expandable Pool Stats ====+\n");
    printf("| Chunks:       %10d      |\n", pool->chunk_count);
    printf("| Block size:   %10zu bytes|\n", pool->block_size);
    printf("| Total blocks: %10zu      |\n", pool->total_blocks);
    printf("| Used blocks:  %10zu      |\n", pool->used_blocks);
    printf("| Free blocks:  %10zu      |\n", pool->total_blocks - pool->used_blocks);
    printf("+===============================+\n\n");
}

void epool_destroy(ExpandablePool* pool) {
    if (!pool) return;
    
    for (int i = 0; i < pool->chunk_count; i++) {
        free(pool->chunks[i].memory);
    }
    printf("[EPool] Destroyed (%d chunks)\n", pool->chunk_count);
    free(pool);
}
```

### 使用示例：游戏实体管理

```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "memory_pool.h"

// 游戏实体结构
typedef struct {
    int id;
    float x, y, z;
    float vx, vy, vz;
    int health;
    int type;
    char name[24];
} Entity;

// 模拟实体更新
void update_entity(Entity* e) {
    e->x += e->vx;
    e->y += e->vy;
    e->z += e->vz;
}

int main() {
    printf("=== Memory Pool Demo - Game Entities ===\n\n");
    
    srand((unsigned)time(NULL));
    
    // 创建实体内存池
    const size_t POOL_SIZE = 1000;
    MemoryPool* entity_pool = pool_create(sizeof(Entity), POOL_SIZE);
    
    // 模拟游戏循环中的实体创建和销毁
    Entity* entities[100] = {0};
    int entity_count = 0;
    
    printf("--- Spawning 50 entities ---\n");
    for (int i = 0; i < 50; i++) {
        Entity* e = (Entity*)pool_alloc(entity_pool);
        if (e) {
            e->id = i;
            e->x = (float)(rand() % 100);
            e->y = (float)(rand() % 100);
            e->z = 0;
            e->vx = ((float)(rand() % 10) - 5) * 0.1f;
            e->vy = ((float)(rand() % 10) - 5) * 0.1f;
            e->vz = 0;
            e->health = 100;
            e->type = rand() % 3;
            snprintf(e->name, sizeof(e->name), "Entity_%d", i);
            entities[entity_count++] = e;
        }
    }
    
    pool_print_stats(entity_pool);
    
    // 模拟一些实体死亡
    printf("--- 20 entities destroyed ---\n");
    for (int i = 0; i < 20; i++) {
        int idx = rand() % entity_count;
        if (entities[idx]) {
            pool_free(entity_pool, entities[idx]);
            entities[idx] = entities[entity_count - 1];
            entities[entity_count - 1] = NULL;
            entity_count--;
        }
    }
    
    pool_print_stats(entity_pool);
    
    // 产生更多实体
    printf("--- Spawning 30 more entities ---\n");
    for (int i = 0; i < 30; i++) {
        Entity* e = (Entity*)pool_alloc(entity_pool);
        if (e) {
            e->id = 100 + i;
            e->health = 100;
            snprintf(e->name, sizeof(e->name), "NewEntity_%d", i);
            entities[entity_count++] = e;
        }
    }
    
    pool_print_stats(entity_pool);
    
    // 性能测试
    printf("--- Performance Test: 100000 alloc/free cycles ---\n");
    clock_t start = clock();
    
    for (int i = 0; i < 100000; i++) {
        Entity* e = (Entity*)pool_alloc(entity_pool);
        if (e) {
            e->id = i;
            pool_free(entity_pool, e);
        }
    }
    
    clock_t end = clock();
    double time_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("Time: %.2f ms (%.0f ops/ms)\n", time_ms, 200000.0 / time_ms);
    
    pool_print_stats(entity_pool);
    
    // 清理所有实体
    printf("--- Cleanup ---\n");
    for (int i = 0; i < entity_count; i++) {
        if (entities[i]) {
            pool_free(entity_pool, entities[i]);
        }
    }
    
    pool_destroy(entity_pool);
    
    return 0;
}

/* 输出示例:
=== Memory Pool Demo - Game Entities ===

[Pool] Created: 1000 blocks x 64 bytes = 64000 bytes total
--- Spawning 50 entities ---

+======= Memory Pool Stats =======+
| Block size:           64 bytes |
| Capacity:           1000 blocks|
| Used:                 50 blocks|
| Free:                950 blocks|
| Usage:               5.0%      |
+---------------------------------+
| Total allocs:         50       |
| Total frees:           0       |
+=================================+

--- 20 entities destroyed ---

+======= Memory Pool Stats =======+
| Block size:           64 bytes |
| Capacity:           1000 blocks|
| Used:                 30 blocks|
| Free:                970 blocks|
| Usage:               3.0%      |
+---------------------------------+
| Total allocs:         50       |
| Total frees:          20       |
+=================================+

--- Spawning 30 more entities ---

+======= Memory Pool Stats =======+
| Block size:           64 bytes |
| Capacity:           1000 blocks|
| Used:                 60 blocks|
| Free:                940 blocks|
| Usage:               6.0%      |
+---------------------------------+
| Total allocs:         80       |
| Total frees:          20       |
+=================================+

--- Performance Test: 100000 alloc/free cycles ---
Time: 3.50 ms (57143 ops/ms)

+======= Memory Pool Stats =======+
| Block size:           64 bytes |
| Capacity:           1000 blocks|
| Used:                 60 blocks|
| Free:                940 blocks|
| Usage:               6.0%      |
+---------------------------------+
| Total allocs:     100080       |
| Total frees:      100020       |
+=================================+

--- Cleanup ---
[Pool] Destroyed (allocs: 100080, frees: 100080)
*/
```

## 优缺点

### 优点
- **O(1)分配和释放**：常数时间操作，性能可预测
- **无内存碎片**：固定大小块不会产生碎片
- **缓存友好**：连续内存布局，提高缓存命中率
- **减少系统调用**：批量分配，减少malloc/free开销
- **适合实时系统**：确定性的分配时间

### 缺点
- 只能分配固定大小的块
- 需要预先估计所需容量
- 不适合大小差异大的对象
- 池满时需要处理（失败或扩展）
- 需要额外的代码来管理池

