/*
场景说明
Slab 分配器是 Linux 内核中享元模式的极致体现：内核中task_struct（进程结构体）、file_struct（文件结构体）等对象被频繁创建 / 销毁，Slab 将这些对象预先分配为大小固定的内存块池（享元对象池），复用内存块而非每次调用malloc/free，既减少内存碎片，又降低对象创建开销。
内部状态：内存块大小、对象类型（固定、共享）；
外部状态：内存块绑定的进程 / 文件数据（可变、动态填充）。

核心说明
内部状态：Slab 缓存的obj_size（对象大小）是固定的内部状态，所有复用的对象都保持该大小；
复用核心：kmem_cache_alloc优先从空闲链表取对象，而非每次malloc，可减少 90% 以上的内存分配开销；
内核价值：Slab 分配器是内核内存管理的基石，解决了 “频繁创建小对象导致的内存碎片” 问题。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 模拟内核Slab缓存结构（享元工厂+对象池）
typedef struct {
    char *name;          // 缓存名（如"task_struct_cache"）
    size_t obj_size;     // 每个对象的大小（内部状态，固定）
    void **free_list;    // 空闲对象链表（享元对象池）
    int free_count;      // 空闲对象数
} kmem_cache_t;

// 全局Slab缓存池（存储不同类型的Slab缓存）
#define MAX_CACHE 5
static kmem_cache_t slab_caches[MAX_CACHE];
static int cache_count = 0;

// ---------------------- 享元工厂：创建/获取Slab缓存 ----------------------
static kmem_cache_t* kmem_cache_create(const char *name, size_t obj_size) {
    // 1. 检查是否已有同类型缓存（复用已有池）
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(slab_caches[i].name, name) == 0 && slab_caches[i].obj_size == obj_size) {
            printf("[Slab工厂] 命中已有缓存：%s（对象大小=%zu）\n", name, obj_size);
            return &slab_caches[i];
        }
    }

    // 2. 未命中：创建新Slab缓存，预分配空闲对象（享元预创建）
    if (cache_count >= MAX_CACHE) return NULL;
    kmem_cache_t *cache = &slab_caches[cache_count++];
    cache->name = malloc(strlen(name) + 1);
    strcpy(cache->name, name);
    cache->obj_size = obj_size;
    cache->free_list = NULL;
    cache->free_count = 0;

    // 预分配3个空闲对象（享元对象），避免运行时malloc
    for (int i = 0; i < 3; i++) {
        void *obj = malloc(obj_size);
        memset(obj, 0, obj_size);
        // 加入空闲链表（简化实现：用指针低位存储下一个节点）
        obj = (void*)((unsigned long)obj | (unsigned long)cache->free_list);
        cache->free_list = obj;
        cache->free_count++;
    }
    printf("[Slab工厂] 创建新缓存：%s（对象大小=%zu，预分配空闲数=%d）\n",
           name, obj_size, cache->free_count);
    return cache;
}

// ---------------------- 享元获取：从Slab缓存分配对象（复用核心） ----------------------
static void* kmem_cache_alloc(kmem_cache_t *cache) {
    if (!cache || cache->free_count == 0) {
        // 无空闲对象，临时分配（模拟Slab扩容）
        void *obj = malloc(cache->obj_size);
        memset(obj, 0, cache->obj_size);
        printf("[Slab分配] 缓存无空闲，临时分配对象：%s\n", cache->name);
        return obj;
    }

    // 复用空闲链表中的对象（享元核心逻辑）
    void *obj = cache->free_list;
    cache->free_list = (void*)((unsigned long)obj & ~0xFF); // 提取下一个节点
    cache->free_count--;
    printf("[Slab分配] 复用空闲对象：%s（剩余空闲=%d）\n", cache->name, cache->free_count);
    return obj;
}

// ---------------------- 享元回收：归还对象到Slab缓存 ----------------------
static void kmem_cache_free(kmem_cache_t *cache, void *obj) {
    // 重置外部状态（清空业务数据），保留内部状态（内存块大小）
    memset(obj, 0, cache->obj_size);
    // 加入空闲链表，供后续复用
    obj = (void*)((unsigned long)obj | (unsigned long)cache->free_list);
    cache->free_list = obj;
    cache->free_count++;
    printf("[Slab回收] 对象归还缓存：%s（剩余空闲=%d）\n", cache->name, cache->free_count);
}

// ---------------------- 上层使用：模拟内核分配task_struct ----------------------
int main() {
    // 1. 创建task_struct的Slab缓存（内部状态：大小=64字节）
    kmem_cache_t *task_cache = kmem_cache_create("task_struct_cache", 64);

    // 2. 分配对象（优先复用预分配的空闲对象）
    void *task1 = kmem_cache_alloc(task_cache);
    void *task2 = kmem_cache_alloc(task_cache);
    void *task3 = kmem_cache_alloc(task_cache);
    void *task4 = kmem_cache_alloc(task_cache); // 无空闲，临时分配

    // 3. 回收对象（归还到缓存，重置外部状态）
    kmem_cache_free(task_cache, task1);

    // 4. 再次分配：复用回收的对象
    void *task5 = kmem_cache_alloc(task_cache);

    return 0;
}