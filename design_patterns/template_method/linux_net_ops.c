// file: net_template.c
#include <stdio.h>
#include <string.h>

// 1. 网络设备操作模板
typedef struct net_device_ops {
    int (*open)(void);
    int (*stop)(void);
    int (*start_xmit)(void *data, int len);
    int (*set_mac)(unsigned char *addr);
    int (*get_stats)(void);
} ndev_ops_t;

// 2. 网络设备结构
typedef struct net_device {
    char name[16];
    unsigned char mac_addr[6];
    ndev_ops_t *ops;
    struct {
        unsigned long tx_packets;
        unsigned long tx_bytes;
    } stats;
} netdev_t;

// 3. 具体网卡驱动实现
// 3.1 e1000驱动 (Intel千兆网卡)
int e1000_open(void) {
    printf("[e1000] 初始化PCI设备，分配DMA缓冲区\n");
    return 0;
}

int e1000_start_xmit(void *data, int len) {
    printf("[e1000] 通过PCIe DMA发送 %d 字节数据\n", len);
    return 0;
}

int e1000_set_mac(unsigned char *addr) {
    printf("[e1000] 设置MAC地址到EEPROM\n");
    return 0;
}

ndev_ops_t e1000_ops = {
    .open = e1000_open,
    .start_xmit = e1000_start_xmit,
    .set_mac = e1000_set_mac,
};

// 3.2 virtio_net驱动 (虚拟化网卡)
int virtio_open(void) {
    printf("[virtio] 协商virtio特性，配置virtqueue\n");
    return 0;
}

int virtio_start_xmit(void *data, int len) {
    printf("[virtio] 通过virtqueue发送 %d 字节到宿主机\n", len);
    return 0;
}

ndev_ops_t virtio_ops = {
    .open = virtio_open,
    .start_xmit = virtio_start_xmit,
};

// 4. 模板方法 - 统一的网络栈处理
int netdev_open(netdev_t *dev) {
    printf("网络栈: 准备打开设备 %s\n", dev->name);
    
    // 固定步骤1: 分配资源
    printf("网络栈: 分配接收/发送缓冲区\n");
    
    // 可变步骤: 调用具体驱动的open
    int ret = dev->ops->open();
    
    // 固定步骤2: 更新状态
    printf("网络栈: 设备状态更新为UP\n");
    
    return ret;
}

int netdev_xmit(netdev_t *dev, void *data, int len) {
    printf("网络栈: 开始发送数据包\n");
    
    // 固定步骤1: 校验和计算
    printf("网络栈: 计算IP/TCP校验和\n");
    
    // 固定步骤2: 更新统计
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += len;
    
    // 可变步骤: 调用具体驱动的发送函数
    int ret = dev->ops->start_xmit(data, len);
    
    // 固定步骤3: 触发软中断
    printf("网络栈: 触发NET_TX_SOFTIRQ\n");
    
    return ret;
}

// 5. 使用示例
int main() {
    // 创建不同的网络设备
    netdev_t e1000_dev = {
        .name = "eth0",
        .mac_addr = {0x00, 0x0C, 0x29, 0xXX, 0xXX, 0xXX},
        .ops = &e1000_ops
    };
    
    netdev_t virtio_dev = {
        .name = "vnet0",
        .ops = &virtio_ops
    };
    
    unsigned char data[1500];
    
    printf("=== 操作e1000物理网卡 ===\n");
    netdev_open(&e1000_dev);
    netdev_xmit(&e1000_dev, data, 1500);
    
    printf("\n=== 操作virtio虚拟网卡 ===\n");
    netdev_open(&virtio_dev);
    netdev_xmit(&virtio_dev, data, 512);
    
    return 0;
}