/*
场景说明
内核网络协议栈中，数据包的校验和计算是典型的装饰器模式：核心功能是数据包的发送 / 接收，校验和装饰器不修改核心收发逻辑，仅在发送前计算校验和、接收后验证校验和，上层协议（如 TCP/UDP）无感知

核心说明
校验和装饰器持有原生网络收发接口，发送前计算校验和，接收后验证校验和；
TCP/UDP/IP 协议的校验和均基于此装饰器模式实现；
可灵活启用 / 禁用校验和（如本地回环数据包可跳过校验和，减少开销）
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 模拟网络数据包结构体
typedef struct {
    char data[64];
    int len;
    uint16_t checksum; // 校验和字段
} sk_buff;

// 模拟网络收发核心接口（Component）
typedef struct {
    int (*send)(sk_buff *skb); // 核心发送接口
    int (*recv)(sk_buff *skb); // 核心接收接口
} net_ops;

// ---------------------- 具体组件：原生网络收发（无校验和） ----------------------
static int net_base_send(sk_buff *skb) {
    printf("[核心功能] 发送数据包：%.*s（长度=%d）\n", skb->len, skb->data, skb->len);
    return 0;
}

static int net_base_recv(sk_buff *skb) {
    strncpy(skb->data, "network data", sizeof(skb->data)-1);
    skb->len = strlen(skb->data);
    skb->checksum = 0x1234; // 模拟接收的校验和
    printf("[核心功能] 接收数据包：%.*s（长度=%d）\n", skb->len, skb->data, skb->len);
    return 0;
}

static net_ops net_base_ops = {
    .send = net_base_send,
    .recv = net_base_recv
};

// ---------------------- 具体装饰器：校验和装饰器 ----------------------
typedef struct {
    net_ops core_ops; // 持有核心网络接口
} checksum_decorator;

// 计算16位校验和（附加功能）
static uint16_t calc_checksum(const char *data, int len) {
    uint32_t sum = 0;
    for (int i=0; i<len; i++) sum += (uint8_t)data[i];
    return (uint16_t)(sum & 0xFFFF);
}

// 校验和装饰器-发送：计算校验和+调用核心发送
static int checksum_send(checksum_decorator *decorator, sk_buff *skb) {
    // 附加功能：计算校验和
    skb->checksum = calc_checksum(skb->data, skb->len);
    printf("[校验和装饰] 计算校验和=0x%04X\n", skb->checksum);
    // 调用核心发送功能
    return decorator->core_ops.send(skb);
}

// 校验和装饰器-接收：调用核心接收+验证校验和
static int checksum_recv(checksum_decorator *decorator, sk_buff *skb) {
    // 调用核心接收功能
    int ret = decorator->core_ops.recv(skb);
    // 附加功能：验证校验和
    uint16_t calc_sum = calc_checksum(skb->data, skb->len);
    if (skb->checksum != calc_sum) {
        printf("[校验和装饰] 校验和错误！接收=0x%04X，计算=0x%04X\n", skb->checksum, calc_sum);
        return -1;
    }
    printf("[校验和装饰] 校验和验证通过=0x%04X\n", skb->checksum);
    return ret;
}

// 初始化校验和装饰器
static void checksum_decorator_init(checksum_decorator *decorator, net_ops core) {
    decorator->core_ops = core;
    decorator->core_ops.send = (int (*)(sk_buff*))checksum_send;
    decorator->core_ops.recv = (int (*)(sk_buff*))checksum_recv;
}

// ---------------------- 上层协议：无感知使用校验和装饰后的接口 ----------------------
int main() {
    // 初始化校验和装饰器，包裹原生网络接口
    checksum_decorator cs_dec;
    checksum_decorator_init(&cs_dec, net_base_ops);
    net_ops net_ops = cs_dec.core_ops;
    
    // 上层发送数据包：透明计算校验和
    printf("\n=== 执行校验和装饰后的发送 ===\n");
    sk_buff send_skb = {.data = "hello network", .len = 13};
    net_ops.send(&send_skb);
    
    // 上层接收数据包：透明验证校验和
    printf("\n=== 执行校验和装饰后的接收 ===\n");
    sk_buff recv_skb = {0};
    net_ops.recv(&recv_skb);
    
    return 0;
}