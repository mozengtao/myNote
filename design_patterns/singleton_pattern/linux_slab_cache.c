/*
场景说明
内核 slab 分配器为每种固定大小的内存块（如task_struct、file结构体）创建一个唯一的 slab 缓存实例，避免重复创建缓存池导致的内存浪费。

核心说明
每种内核对象（如task_struct、file）对应一个唯一的 slab 缓存实例；
实例为饿汉式初始化，内核启动时创建；
单例 + 享元结合：单例保证缓存池唯一，享元复用空闲对象，减少内存分配开销。
*/

#include <stdio.h>
#include <stdlib.h>

// 1. slab缓存单例结构体
typedef struct {
    char *name;         // 缓存名称（如"task_struct"）
    size_t obj_size;    // 每个对象的大小
    void **free_list;   // 空闲对象链表
    int free_count;     // 空闲对象数
} kmem_cache;

// 2. 静态全局slab缓存实例（针对task_struct的唯一缓存）
static kmem_cache task_struct_cache = {
    .name = "task_struct",
    .obj_size = 1024,   // task_struct大小约1024字节
    .free_list = NULL,
    .free_count = 0
};

// 3. 唯一访问接口：获取task_struct的slab缓存
static inline kmem_cache* kmem_cache_get_task_struct(void) {
    return &task_struct_cache;
}

// 4. 业务接口：分配task_struct对象
void* kmem_cache_alloc_task_struct(void) {
    kmem_cache *cache = kmem_cache_get_task_struct();
    void *obj;
    // 优先复用空闲对象（享元+单例）
    if (cache->free_count > 0) {
        obj = cache->free_list[--cache->free_count];
        printf("复用task_struct对象：%p\n", obj);
    } else {
        // 无空闲则新建
        obj = malloc(cache->obj_size);
        printf("新建task_struct对象：%p\n", obj);
    }
    return obj;
}

// 测试
int main() {
    printf("task_struct slab缓存实例地址：%p\n", kmem_cache_get_task_struct());
    void *t1 = kmem_cache_alloc_task_struct(); // 新建
    void *t2 = kmem_cache_alloc_task_struct(); // 新建
    // 归还t1到空闲链表（模拟free）
    task_struct_cache.free_list = realloc(task_struct_cache.free_list, sizeof(void*));
    task_struct_cache.free_list[task_struct_cache.free_count++] = t1;
    void *t3 = kmem_cache_alloc_task_struct(); // 复用t1
    return 0;
}