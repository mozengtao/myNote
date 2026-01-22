/*
场景说明
sk_buff（套接字缓冲区）是内核网络协议栈中存储数据包的核心结构体，数据包收发频率极高（每秒数万次）。内核通过skb_pool缓存池复用sk_buff对象，避免每次收发数据包都创建 / 销毁sk_buff，减少内存分配开销和碎片。
内部状态：缓冲区大小（1500 字节，以太网 MTU）、协议类型（TCP/UDP）；
外部状态：数据包内容、收发端口、时间戳。

核心说明
内部状态复用：buf_size（1500 字节）是固定的内部状态，回收时无需修改，仅需清空外部状态（data）；
性能优化：内核中skb_alloc/skb_free复用sk_buff可减少 90% 以上的内存分配开销，是网络协议栈高性能的关键；
状态重置：外部状态（数据包内容）在回收时清空，确保复用的对象仅需填充新数据即可使用。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 模拟sk_buff结构体（享元对象）
typedef struct sk_buff {
    int proto;          // 协议类型（TCP=6/UDP=17，内部状态）
    size_t buf_size;    // 缓冲区大小（1500字节，内部状态）
    char *data;         // 数据包内容（外部状态，动态填充）
    struct sk_buff *next; // 空闲链表节点
} sk_buff_t;

// sk_buff缓存池（享元工厂）
static sk_buff_t *skb_pool = NULL;
static int pool_size = 0;
#define MAX_SKB_POOL 5

// 初始化skb缓存池（预分配享元对象）
static void skb_pool_init() {
    for (int i = 0; i < MAX_SKB_POOL; i++) {
        sk_buff_t *skb = malloc(sizeof(sk_buff_t));
        skb->proto = 0;
        skb->buf_size = 1500; // 以太网MTU，内部状态固定
        skb->data = malloc(skb->buf_size);
        skb->next = skb_pool;
        skb_pool = skb;
        pool_size++;
    }
    printf("[skb池] 初始化完成，预分配=%d个skb对象（缓冲区大小=1500）\n", pool_size);
}

// 享元获取：从缓存池分配skb（复用核心）
static sk_buff_t* skb_alloc(int proto) {
    if (!skb_pool) {
        // 池空，临时创建（模拟扩容）
        sk_buff_t *skb = malloc(sizeof(sk_buff_t));
        skb->proto = proto;
        skb->buf_size = 1500;
        skb->data = malloc(skb->buf_size);
        printf("[skb分配] 池空，临时创建skb（协议=%d）\n", proto);
        return skb;
    }

    // 复用池中的skb（享元核心）
    sk_buff_t *skb = skb_pool;
    skb_pool = skb->next;
    pool_size--;
    skb->proto = proto; // 设置内部状态（协议类型）
    memset(skb->data, 0, skb->buf_size); // 清空外部状态（数据包）
    printf("[skb分配] 复用池中的skb（协议=%d，剩余池大小=%d）\n", proto, pool_size);
    return skb;
}

// 享元回收：归还skb到缓存池（重置外部状态）
static void skb_free(sk_buff_t *skb) {
    memset(skb->data, 0, skb->buf_size); // 清空外部状态（数据包）
    skb->next = skb_pool;
    skb_pool = skb;
    pool_size++;
    printf("[skb回收] 归还skb到池（剩余池大小=%d）\n", pool_size);
}

// 发送数据包（填充外部状态）
static void send_packet(int proto, const char *data, int len) {
    sk_buff_t *skb = skb_alloc(proto);
    // 填充外部状态：数据包内容
    strncpy(skb->data, data, len > skb->buf_size ? skb->buf_size : len);
    printf("[发送数据包] 内部状态：协议=%d 缓冲区=%zu | 外部状态：数据=%s\n",
           skb->proto, skb->buf_size, skb->data);
    // 模拟发送后回收（复用关键）
    skb_free(skb);
}

// 主函数测试
int main() {
    skb_pool_init();

    // 发送TCP数据包（复用池中的skb）
    send_packet(6, "TCP data: hello", 14);
    // 发送UDP数据包（复用池中的skb）
    send_packet(17, "UDP data: world", 14);
    // 继续发送，直到池空
    send_packet(6, "TCP data: test1", 12);
    send_packet(6, "TCP data: test2", 12);
    send_packet(6, "TCP data: test3", 12);
    send_packet(6, "TCP data: test4", 12); // 池空，临时创建

    return 0;
}