/*
场景说明
文件系统中，每个文件对应一个inode（索引节点），包含文件大小、权限、存储位置等核心属性（内部状态）。内核为避免每次访问文件都从磁盘读取inode（耗时），会将已加载的inode缓存到内存中（享元池），多个进程访问同一文件时复用同一个 inode 对象，仅文件的打开模式、偏移量等（外部状态）不同

核心说明
内部状态：inode的ino（节点号）、size（文件大小）是不可变的内部状态，存储在缓存中；
外部状态：文件的打开模式（r/rw）、读写偏移量是外部状态，由file结构体携带，不存储在inode中；
性能提升：复用inode可避免每次访问文件都读取磁盘，提升文件系统性能达 10 倍以上。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 模拟inode结构体（享元对象，存储内部状态）
typedef struct {
    int ino;          // inode号（唯一标识，内部状态）
    size_t size;      // 文件大小（内部状态）
    char mode[8];     // 文件权限（如"rw-r--r--"，内部状态）
} inode_t;

// inode缓存池（享元工厂）
#define MAX_INODE_CACHE 5
static inode_t inode_cache[MAX_INODE_CACHE];
static int cache_count = 0;

// 享元工厂：获取inode（复用已有缓存，未命中则从磁盘加载）
static inode_t* get_inode(int ino) {
    // 1. 检查缓存：命中则返回（享元复用）
    for (int i = 0; i < cache_count; i++) {
        if (inode_cache[i].ino == ino) {
            printf("[inode缓存] 命中：ino=%d（文件大小=%zu）\n", ino, inode_cache[i].size);
            return &inode_cache[i];
        }
    }

    // 2. 未命中：从磁盘加载（模拟），加入缓存
    if (cache_count >= MAX_INODE_CACHE) return NULL;
    inode_t *new_inode = &inode_cache[cache_count++];
    new_inode->ino = ino;
    // 模拟从磁盘读取inode属性（内部状态，固定）
    new_inode->size = ino * 1024;  // 简化：ino*1024为文件大小
    strcpy(new_inode->mode, "rw-r--r--");
    printf("[inode缓存] 从磁盘加载：ino=%d，加入缓存（当前缓存数=%d）\n", ino, cache_count);
    return new_inode;
}

// 模拟文件打开：复用inode缓存（外部状态传入）
static void open_file(int ino, const char *open_mode) {
    inode_t *inode = get_inode(ino);
    if (!inode) return;
    // 内部状态：inode->ino/size/mode（共享）
    // 外部状态：open_mode（文件打开模式，可变）
    printf("[文件打开] ino=%d | 内部状态：大小=%zu 权限=%s | 外部状态：打开模式=%s\n",
           ino, inode->size, inode->mode, open_mode);
}

// 主函数测试
int main() {
    // 第一次打开文件：加载inode到缓存
    open_file(1001, "r");
    // 第二次打开同一文件：复用缓存中的inode（仅外部状态不同）
    open_file(1001, "rw");
    // 打开新文件：加载新inode到缓存
    open_file(1002, "r");

    return 0;
}