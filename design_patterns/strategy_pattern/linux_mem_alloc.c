/*
策略思想映射
    策略接口：内存分配 / 释放的函数指针（统一调用规范）；
    具体策略：slab（标准缓存）、slub（轻量缓存）、slob（极简链表）三种等价分配算法；
    上下文：内核内存管理子系统，可通过编译选项 / CONFIG 动态切换策略，核心分配逻辑不变。
*/

#include <stdio.h>
#include <stddef.h>

// 1. 策略接口：内存分配/释放函数指针类型（统一规范）
typedef void* (*MemAllocStrategy)(size_t size); // 分配策略
typedef void (*MemFreeStrategy)(void *ptr);     // 释放策略

// 2. 具体策略实现：三种内存分配算法
// 策略1：slab分配器（标准缓存池）
void* slab_alloc(size_t size) {
    printf("[slab策略] 分配%zu字节 → 基于缓存池分配\n", size);
    return (void*)0x100000; // 模拟分配地址
}
void slab_free(void *ptr) {
    printf("[slab策略] 释放地址%p → 归还到缓存池\n", ptr);
}

// 策略2：slub分配器（轻量级slab）
void* slub_alloc(size_t size) {
    printf("[slub策略] 分配%zu字节 → 轻量级缓存分配\n", size);
    return (void*)0x200000;
}
void slub_free(void *ptr) {
    printf("[slub策略] 释放地址%p → 轻量级缓存释放\n", ptr);
}

// 策略3：slob分配器（极简链表，嵌入式专用）
void* slob_alloc(size_t size) {
    printf("[slob策略] 分配%zu字节 → 极简链表分配\n", size);
    return (void*)0x300000;
}
void slob_free(void *ptr) {
    printf("[slob策略] 释放地址%p → 极简链表释放\n", ptr);
}

// 3. 上下文：内存管理上下文（封装策略，动态切换）
typedef struct {
    MemAllocStrategy alloc; // 当前分配策略
    MemFreeStrategy free;   // 当前释放策略
} SlabContext;

// 上下文方法：切换内存分配策略
void slab_set_strategy(SlabContext *ctx, MemAllocStrategy a, MemFreeStrategy f) {
    ctx->alloc = a; ctx->free = f;
    printf("已切换内存分配策略\n");
}

// 上下文方法：执行分配（内核核心逻辑，永不修改）
void* slab_alloc_mem(SlabContext *ctx, size_t size) {
    return ctx->alloc ? ctx->alloc(size) : NULL;
}

// 调用测试
int main() {
    SlabContext mm_ctx = {NULL, NULL};
    void *ptr;

    // 切换为slab策略
    slab_set_strategy(&mm_ctx, slab_alloc, slab_free);
    ptr = slab_alloc_mem(&mm_ctx, 128);
    mm_ctx.free(ptr);
    printf("\n");

    // 切换为slub策略
    slab_set_strategy(&mm_ctx, slub_alloc, slub_free);
    ptr = slab_alloc_mem(&mm_ctx, 256);
    mm_ctx.free(ptr);

    return 0;
}