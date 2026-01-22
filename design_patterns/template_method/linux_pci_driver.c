#include <stdio.h>
#include <string.h>

// PCI设备ID结构体（驱动与设备匹配的依据）
typedef struct {
    int vendor_id;  // 厂商ID
    int device_id;  // 设备ID
} pci_device_id;

// 1. 【抽象可变步骤】PCI驱动结构体（内核定义：linux/pci.h）
typedef struct {
    const pci_device_id *id_table; // 匹配ID表
    int (*probe)(int dev_id);      // 设备匹配成功后初始化
    void (*remove)(int dev_id);    // 设备移除时清理
} pci_driver;

// 2. 【固定模板骨架】内核PCI设备匹配逻辑（所有PCI驱动共用）
int pci_match_device(const pci_driver *drv, int vendor_id, int device_id) {
    printf("[内核模板] PCI设备匹配 → 厂商ID：%d，设备ID：%d\n", vendor_id, device_id);
    // 遍历驱动ID表，匹配则返回1
    for (const pci_device_id *id = drv->id_table; id->vendor_id != 0; id++) {
        if (id->vendor_id == vendor_id && id->device_id == device_id) {
            return 1;
        }
    }
    return 0;
}

// 3. 【固定模板骨架】内核PCI枚举+探测流程（所有PCI设备共用）
void pci_enumerate_and_probe(const pci_driver *drv, int dev_id, int vendor_id, int device_id) {
    printf("[内核模板] 枚举PCI设备 → 设备编号：%d\n", dev_id);
    if (pci_match_device(drv, vendor_id, device_id)) {
        printf("[内核模板] 匹配成功 → 调用驱动probe\n");
        drv->probe(dev_id); // 调用驱动初始化逻辑
    } else {
        printf("[内核模板] 匹配失败 → 跳过\n");
    }
}

// 4. 【固定模板骨架】内核PCI驱动注册流程
int pci_register_driver(const pci_driver *drv) {
    printf("[内核模板] 注册PCI驱动 → 加入驱动链表\n");
    return 0;
}

// ===================== 具体实现：PCI网卡驱动 =====================
// 1. 定义匹配ID表（Intel网卡：厂商ID=8086，设备ID=100E）
static const pci_device_id eth_pci_ids[] = {
    {8086, 100E}, // 匹配Intel网卡
    {0, 0}        // 结束标记
};
// 2. 驱动具体实现
int eth_pci_probe(int dev_id) {
    printf("[PCI网卡驱动] 初始化网卡 → 配置DMA、中断\n");
    return 0;
}
void eth_pci_remove(int dev_id) {
    printf("[PCI网卡驱动] 移除网卡 → 释放DMA、中断\n");
}
// 3. 绑定：PCI网卡驱动结构体
const pci_driver eth_pci_drv = {
    .id_table = eth_pci_ids,
    .probe = eth_pci_probe,
    .remove = eth_pci_remove
};

// 调用测试
int main() {
    pci_register_driver(&eth_pci_drv); // 注册PCI网卡驱动
    // 模拟枚举到Intel网卡（设备ID=1，厂商ID=8086，设备ID=100E）
    pci_enumerate_and_probe(&eth_pci_drv, 1, 8086, 100E);
    return 0;
}