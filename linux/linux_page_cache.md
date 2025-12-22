# Linux Kernel Page Cache Architecture (v3.2)

## Overview

This document explains **page cache architecture** in Linux kernel v3.2, focusing on caching strategies, writeback, and eviction.

---

## Page Cache Purpose

```
+------------------------------------------------------------------+
|  WHY PAGE CACHE EXISTS                                           |
+------------------------------------------------------------------+

    WITHOUT PAGE CACHE:
    
    User Process          Disk
    ┌───────────┐        ┌───────────┐
    │ read(fd)  │───────▶│ Disk I/O  │  ← Every read hits disk!
    │           │◀───────│ (~10ms)   │    ~10,000,000 ns
    └───────────┘        └───────────┘

    WITH PAGE CACHE:
    
    User Process          Page Cache          Disk
    ┌───────────┐        ┌───────────┐      ┌───────────┐
    │ read(fd)  │───────▶│  Cached?  │      │           │
    │           │        │    YES    │      │           │
    │           │◀───────│  Return   │      │           │
    │           │        │  (~100ns) │      │           │
    └───────────┘        └───────────┘      └───────────┘
                              │
                         If NO (miss)
                              │
                              ▼
                         ┌───────────┐
                         │ Read from │
                         │   disk    │
                         │  Cache it │
                         └───────────┘

    SPEEDUP:
    +----------------------------------------------------------+
    | Disk access:    ~10,000,000 ns (10 ms)                    |
    | Cache hit:      ~100 ns                                   |
    | Speedup:        100,000x for cached data!                 |
    +----------------------------------------------------------+
```

**中文解释：**
- 无页缓存：每次读取都访问磁盘（约 10ms）
- 有页缓存：缓存命中时直接返回（约 100ns），加速 100,000 倍
- 页缓存目的：利用内存缓存磁盘数据，减少 I/O 延迟

---

## Page Cache Structure

```
+------------------------------------------------------------------+
|  PAGE CACHE ORGANIZATION                                         |
+------------------------------------------------------------------+

    struct address_space (per-inode cache):
    
    ┌─────────────────────────────────────────────────────────────┐
    │  struct inode                                                │
    │  ├── i_mapping ──────▶ struct address_space                 │
    │  │                     ├── host (back to inode)             │
    │  │                     ├── nrpages (page count)             │
    │  │                     ├── page_tree (radix tree)           │
    │  │                     └── a_ops (operations)               │
    │  └── ...                                                    │
    └─────────────────────────────────────────────────────────────┘
    
    RADIX TREE (page_tree):
    
                        root
                         │
            ┌────────────┼────────────┐
            │            │            │
          slot[0]      slot[1]      slot[2]
            │            │            │
            ▼            ▼            ▼
        ┌───────┐   ┌───────┐   ┌───────┐
        │Page 0 │   │Page 1 │   │Page 2 │   ...
        │offset │   │offset │   │offset │
        │  0    │   │ 4096  │   │ 8192  │
        └───────┘   └───────┘   └───────┘
    
    LOOKUP: O(log n) where n = file size / page size
```

**中文解释：**
- 每个 inode 有 address_space 结构管理其缓存页
- page_tree 是基数树（radix tree），按偏移量索引页
- 查找复杂度：O(log n)，n = 文件大小 / 页大小

---

## Read Path

```
+------------------------------------------------------------------+
|  READ PATH: sys_read() → Page Cache → Disk                       |
+------------------------------------------------------------------+

    sys_read(fd, buf, count)
           │
           ▼
    vfs_read(file, buf, count, &pos)
           │
           ▼
    file->f_op->read() or do_sync_read()
           │
           ▼
    generic_file_aio_read()
           │
           ▼
    ┌──────────────────────────────────────┐
    │ find_get_page(mapping, index)        │
    │   - Look up page in radix tree       │
    └──────────────┬───────────────────────┘
                   │
         ┌─────────┴─────────┐
         │                   │
      Page found         Page NOT found
         │                   │
         ▼                   ▼
    ┌──────────┐      ┌───────────────────────┐
    │ Return   │      │ page_cache_read()     │
    │ cached   │      │   - Allocate page     │
    │ data     │      │   - Submit I/O        │
    └──────────┘      │   - Wait for I/O      │
                      │   - Add to cache      │
                      └───────────────────────┘

    READ-AHEAD:
    +----------------------------------------------------------+
    | If sequential read detected:                              |
    | - Speculatively read ahead 128KB-2MB                      |
    | - Async I/O (doesn't block current read)                  |
    | - Amortizes disk seek cost over many pages                |
    +----------------------------------------------------------+
```

**中文解释：**
- 读路径：sys_read → vfs_read → 文件操作 → find_get_page
- 缓存命中：直接返回缓存数据
- 缓存未命中：分配页、提交 I/O、等待完成、添加到缓存
- 预读：检测到顺序读取时，异步预读 128KB-2MB

---

## Write Path

```
+------------------------------------------------------------------+
|  WRITE PATH: sys_write() → Page Cache → Disk                     |
+------------------------------------------------------------------+

    BUFFERED WRITE (default):
    
    sys_write(fd, buf, count)
           │
           ▼
    vfs_write(file, buf, count, &pos)
           │
           ▼
    file->f_op->write() or do_sync_write()
           │
           ▼
    generic_file_aio_write()
           │
           ▼
    ┌──────────────────────────────────────┐
    │ 1. Find or create page in cache      │
    │ 2. Copy user data to page            │
    │ 3. Mark page DIRTY                   │
    │ 4. Return to user (write complete!)  │
    └──────────┬───────────────────────────┘
               │
               │  Data is in memory, not on disk yet!
               │
               ▼
    ┌──────────────────────────────────────┐
    │ Later: Background writeback          │
    │ - pdflush/flush kernel threads       │
    │ - Periodic (every 5 seconds)         │
    │ - When dirty ratio exceeds threshold │
    │ - On fsync() or sync()               │
    └──────────────────────────────────────┘

    WRITE TIMELINE:
    
    User write()    Page dirty    Writeback    Disk write
         │              │             │            │
         ▼              ▼             ▼            ▼
    ─────┼──────────────┼─────────────┼────────────┼─────────▶
         │              │             │            │     time
         │◀────────────▶│             │            │
         │  ~1 μs       │             │            │
         │  (fast!)     │◀───────────▶│◀──────────▶│
                        │  seconds    │   ~10 ms   │
                        │  (async)    │  (disk)    │
```

**中文解释：**
- 缓冲写入：复制数据到缓存页、标记脏、立即返回（约 1μs）
- 实际写盘：后台延迟进行（pdflush 线程）
- 写回触发：定期（5秒）、脏页比例超阈值、fsync/sync

---

## Eviction and LRU

```
+------------------------------------------------------------------+
|  PAGE EVICTION (Memory Pressure)                                 |
+------------------------------------------------------------------+

    LRU LISTS (Least Recently Used):
    
    ┌─────────────────────────────────────────────────────────────┐
    │  ACTIVE LRU (frequently accessed)                           │
    │  ┌──────┬──────┬──────┬──────┬──────┐                       │
    │  │Page A│Page B│Page C│Page D│Page E│  ← Recently accessed  │
    │  └──────┴──────┴──────┴──────┴──────┘                       │
    │                                                              │
    │  INACTIVE LRU (candidates for eviction)                      │
    │  ┌──────┬──────┬──────┬──────┬──────┐                       │
    │  │Page X│Page Y│Page Z│Page W│Page V│  ← Evict from here    │
    │  └──────┴──────┴──────┴──────┴──────┘                       │
    └─────────────────────────────────────────────────────────────┘

    EVICTION DECISION:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  Memory pressure detected                                    │
    │         │                                                    │
    │         ▼                                                    │
    │  Scan inactive LRU                                           │
    │         │                                                    │
    │         ▼                                                    │
    │  ┌──────────────────┐                                       │
    │  │ Page accessed    │─── YES ──▶ Move to active LRU         │
    │  │ recently?        │                                       │
    │  └────────┬─────────┘                                       │
    │           │ NO                                               │
    │           ▼                                                  │
    │  ┌──────────────────┐                                       │
    │  │ Page dirty?      │─── YES ──▶ Write back first           │
    │  └────────┬─────────┘                                       │
    │           │ NO                                               │
    │           ▼                                                  │
    │  ┌──────────────────┐                                       │
    │  │ EVICT PAGE       │                                       │
    │  │ (free memory)    │                                       │
    │  └──────────────────┘                                       │
    └─────────────────────────────────────────────────────────────┘

    REFERENCED BIT:
    +----------------------------------------------------------+
    | Page accessed → Set PG_referenced bit                     |
    | Eviction scan → Check and clear PG_referenced             |
    | If referenced → Give second chance (move to active)       |
    | If not referenced → Evict                                 |
    +----------------------------------------------------------+
```

**中文解释：**
- LRU 列表：活跃列表（频繁访问）、非活跃列表（驱逐候选）
- 驱逐决策：
  1. 最近访问？→ 移到活跃列表
  2. 脏页？→ 先写回
  3. 否则驱逐
- 引用位：给被访问页"第二次机会"

---

## Backpressure

```
+------------------------------------------------------------------+
|  WRITE THROTTLING (Backpressure)                                 |
+------------------------------------------------------------------+

    PROBLEM:
    +----------------------------------------------------------+
    | Fast writers can create too many dirty pages              |
    | - Writeback can't keep up                                 |
    | - Memory fills with dirty pages                           |
    | - System runs out of clean pages                          |
    +----------------------------------------------------------+

    SOLUTION: Dirty Ratio Limits
    
    Dirty pages %
         │
    100% ┼──────────────────────────────────────────
         │                           ▲
         │                           │ HARD LIMIT
    40%  ┼─────────────────── dirty_ratio ──────────
         │                   ┃
         │                   ┃ BLOCK new writes
         │                   ┃ until below ratio
    20%  ┼─────────────────── dirty_background_ratio
         │             ┃
         │             ┃ START background writeback
         │             ┃
      0% ┼─────────────┼─────────────────────────────▶
                       │                        time
                 Writeback active

    BEHAVIOR:
    +----------------------------------------------------------+
    | < 20% dirty: Normal operation, no throttling              |
    | 20-40% dirty: Background writeback active                 |
    | > 40% dirty: Writers BLOCKED until ratio drops            |
    +----------------------------------------------------------+
```

**中文解释：**
- 问题：快速写入者创建过多脏页，写回跟不上
- 解决方案：脏页比例限制
  - < 20%：正常操作
  - 20-40%：后台写回活跃
  - > 40%：阻塞新写入直到比例下降

---

## User-Space Cache Design

```c
/* User-space LRU cache inspired by page cache */

#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

struct cache_entry {
    uint64_t key;
    void *value;
    size_t size;
    
    /* LRU list links */
    struct cache_entry *lru_prev;
    struct cache_entry *lru_next;
    
    /* Hash chain */
    struct cache_entry *hash_next;
    
    /* Flags */
    unsigned dirty : 1;
    unsigned referenced : 1;
};

struct lru_cache {
    /* Hash table for O(1) lookup */
    struct cache_entry **buckets;
    size_t num_buckets;
    
    /* LRU lists */
    struct cache_entry *active_head;
    struct cache_entry *active_tail;
    struct cache_entry *inactive_head;
    struct cache_entry *inactive_tail;
    
    /* Limits */
    size_t max_size;
    size_t current_size;
    
    /* Writeback callback */
    void (*writeback)(uint64_t key, void *value);
    
    pthread_mutex_t lock;
};

/* Find in cache - like find_get_page() */
void *cache_lookup(struct lru_cache *cache, uint64_t key)
{
    pthread_mutex_lock(&cache->lock);
    
    size_t bucket = key % cache->num_buckets;
    struct cache_entry *e = cache->buckets[bucket];
    
    while (e) {
        if (e->key == key) {
            /* Mark as referenced (second chance) */
            e->referenced = 1;
            pthread_mutex_unlock(&cache->lock);
            return e->value;
        }
        e = e->hash_next;
    }
    
    pthread_mutex_unlock(&cache->lock);
    return NULL;  /* Cache miss */
}

/* Evict entries if over limit - like shrink_page_list() */
void cache_evict_if_needed(struct lru_cache *cache)
{
    while (cache->current_size > cache->max_size) {
        /* Scan inactive list */
        struct cache_entry *victim = cache->inactive_tail;
        
        if (!victim) {
            /* Promote from active to inactive */
            victim = cache->active_tail;
            if (!victim) break;
            /* Move to inactive head */
        }
        
        if (victim->referenced) {
            /* Second chance - move to active */
            victim->referenced = 0;
            /* Move to active head */
            continue;
        }
        
        if (victim->dirty) {
            /* Writeback first */
            cache->writeback(victim->key, victim->value);
            victim->dirty = 0;
        }
        
        /* Evict */
        /* Remove from hash and LRU */
        cache->current_size -= victim->size;
        free(victim->value);
        free(victim);
    }
}

/* Insert into cache - like add_to_page_cache_lru() */
void cache_insert(struct lru_cache *cache, uint64_t key, 
                  void *value, size_t size)
{
    pthread_mutex_lock(&cache->lock);
    
    struct cache_entry *e = malloc(sizeof(*e));
    e->key = key;
    e->value = value;
    e->size = size;
    e->dirty = 0;
    e->referenced = 0;
    
    /* Add to hash */
    size_t bucket = key % cache->num_buckets;
    e->hash_next = cache->buckets[bucket];
    cache->buckets[bucket] = e;
    
    /* Add to inactive LRU head */
    e->lru_next = cache->inactive_head;
    e->lru_prev = NULL;
    if (cache->inactive_head)
        cache->inactive_head->lru_prev = e;
    cache->inactive_head = e;
    if (!cache->inactive_tail)
        cache->inactive_tail = e;
    
    cache->current_size += size;
    
    /* Evict if needed */
    cache_evict_if_needed(cache);
    
    pthread_mutex_unlock(&cache->lock);
}
```

**中文解释：**
- 用户态 LRU 缓存：模拟内核页缓存
- 哈希表 + 双向链表实现 O(1) 查找和 LRU
- 驱逐策略：引用位（第二次机会）、脏页写回
- 背压：超过 max_size 时驱逐条目

