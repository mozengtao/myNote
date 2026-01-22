/*
场景说明
内核在调试模式下，会给kmalloc/kfree添加 “内存统计、泄漏检测、越界检查” 等装饰功能：核心功能是原生的内存分配 / 释放，装饰器不修改原生逻辑，仅在分配 / 释放时添加附加统计功能，生产模式下可直接移除装饰器

核心说明
内存统计装饰器持有原生kmalloc/kfree接口，分配 / 释放时添加统计功能；
调试模式下启用装饰器，可监控内存泄漏（分配次数≠释放次数）；
生产模式下直接使用原生接口，无任何性能开销。
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 模拟内核内存分配核心接口（Component）
typedef struct {
    void* (*alloc)(size_t size); // 核心分配接口
    void (*free)(void *ptr);     // 核心释放接口
} mem_ops;

// ---------------------- 具体组件：原生kmalloc/kfree（无附加功能） ----------------------
static void* kmalloc_base(size_t size) {
    void *ptr = malloc(size);
    printf("[核心功能] kmalloc分配内存：%p（大小=%zu）\n", ptr, size);
    return ptr;
}

static void kfree_base(void *ptr) {
    printf("[核心功能] kfree释放内存：%p\n", ptr);
    free(ptr);
}

static mem_ops mem_base_ops = {
    .alloc = kmalloc_base,
    .free = kfree_base
};

// ---------------------- 具体装饰器：内存统计装饰器 ----------------------
typedef struct {
    mem_ops core_ops;          // 持有核心内存接口
    uint64_t alloc_count;      // 附加统计：分配次数
    uint64_t free_count;       // 附加统计：释放次数
    uint64_t total_alloc_size; // 附加统计：总分配大小
} mem_stat_decorator;

// 统计装饰器-分配：调用核心分配+统计
static void* mem_stat_alloc(mem_stat_decorator *decorator, size_t size) {
    // 调用核心功能
    void *ptr = decorator->core_ops.alloc(size);
    // 附加功能：统计
    if (ptr) {
        decorator->alloc_count++;
        decorator->total_alloc_size += size;
        printf("[统计装饰] 累计分配次数=%lu，总大小=%lu字节\n", 
               decorator->alloc_count, decorator->total_alloc_size);
    }
    return ptr;
}

// 统计装饰器-释放：调用核心释放+统计
static void mem_stat_free(mem_stat_decorator *decorator, void *ptr) {
    // 调用核心功能
    decorator->core_ops.free(ptr);
    // 附加功能：统计
    decorator->free_count++;
    printf("[统计装饰] 累计释放次数=%lu，剩余未释放次数=%lu\n", 
           decorator->free_count, decorator->alloc_count - decorator->free_count);
}

// 初始化统计装饰器
static void mem_stat_decorator_init(mem_stat_decorator *decorator, mem_ops core) {
    decorator->core_ops = core;
    decorator->alloc_count = 0;
    decorator->free_count = 0;
    decorator->total_alloc_size = 0;
    // 替换为装饰后的接口
    decorator->core_ops.alloc = (void* (*)(size_t))mem_stat_alloc;
    decorator->core_ops.free = (void (*)(void*))mem_stat_free;
}

// ---------------------- 上层内核模块：无感知使用统计装饰后的内存接口 ----------------------
int main() {
    // 初始化统计装饰器，包裹原生内存接口
    mem_stat_decorator stat_dec;
    mem_stat_decorator_init(&stat_dec, mem_base_ops);
    mem_ops mem_ops = stat_dec.core_ops;
    
    // 上层分配内存：透明统计
    printf("\n=== 执行统计装饰后的内存分配 ===\n");
    void *p1 = mem_ops.alloc(1024);
    void *p2 = mem_ops.alloc(2048);
    
    // 上层释放内存：透明统计
    printf("\n=== 执行统计装饰后的内存释放 ===\n");
    mem_ops.free(p1);
    
    return 0;
}