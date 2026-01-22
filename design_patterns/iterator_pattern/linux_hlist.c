/*
场景说明
hlist是内核专为哈希表设计的单向哈希链表（减少空链表的内存开销），内核通过hlist_for_each/hlist_for_each_entry宏实现迭代器，用于遍历哈希表中的元素（如进程 PID 哈希表、文件系统索引）

核心说明
hlist迭代器宏封装了哈希链表的 “单向节点移动”“空节点判断” 等细节，上层遍历逻辑与普通链表一致；
内核哈希表（如pid_hash）通过hlist迭代器实现高效遍历，且哈希桶扩容 / 缩容时，遍历逻辑无需修改
*/

#include <stdio.h>
#include <stdlib.h>

// 模拟内核hlist哈希链表节点
struct hlist_node {
    struct hlist_node *next, **pprev;
};
struct hlist_head {
    struct hlist_node *first;
};

// 初始化hlist头
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

// 模拟container_of/offsetof宏（同案例1）
#define offsetof(type, member) ((size_t)&((type*)0)->member)
#define container_of(ptr, type, member) ({          \
    const typeof(((type*)0)->member) *__mptr = (ptr);\
    (type*)((char*)__mptr - offsetof(type, member));})

// ---------------------- hlist迭代器宏 ----------------------
#define hlist_for_each_entry(pos, head, member)     \
    for (pos = hlist_entry((head)->first, typeof(*pos), member); \
         pos;                                       \
         pos = hlist_entry((pos)->member.next, typeof(*pos), member))

#define hlist_entry(ptr, type, member) container_of(ptr, type, member)

// ---------------------- 业务结构体：文件索引（模拟inode） ----------------------
struct inode {
    int ino;                // 索引节点号
    char filename[32];      // 文件名
    struct hlist_node node; // hlist节点
};

// 主函数测试：遍历哈希链表（文件索引）
int main() {
    // 1. 初始化hlist头（哈希桶）
    struct hlist_head inode_hash[1]; // 简化为1个哈希桶
    INIT_HLIST_HEAD(&inode_hash[0]);

    // 2. 创建2个文件节点并加入hlist
    struct inode i1 = {.ino = 1001, .filename = "test.txt"};
    struct inode i2 = {.ino = 1002, .filename = "data.log"};
    i1.node.next = &i2.node; i2.node.pprev = &i1.node.next;
    i1.node.pprev = &inode_hash[0].first;
    inode_hash[0].first = &i1.node;

    // 3. 使用迭代器宏遍历hlist（上层无需关心哈希链表结构）
    printf("\n=== 遍历文件索引哈希链表 ===\n");
    struct inode *pos;
    hlist_for_each_entry(pos, &inode_hash[0], node) {
        printf("inode号：%d，文件名：%s\n", pos->ino, pos->filename);
    }

    return 0;
}