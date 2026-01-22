/*
场景说明
内核遍历 PCI 总线时，会根据设备的 vendor ID（厂商 ID）和 device ID（设备 ID），匹配并创建对应 PCI 驱动实例（如 Intel 网卡、NVIDIA 显卡），工厂函数封装了 “ID 匹配 + 驱动实例创建” 的核心逻辑。

核心说明
内核中pci_match_device+pci_register_driver构成了 PCI 设备工厂：内核遍历 PCI 总线设备，通过 vendor/device ID 匹配驱动，调用probe创建驱动实例；
用户 / 驱动开发者只需实现pci_driver结构体并注册，无需关心内核的匹配逻辑，完美体现 “创建封装、接口统一” 的工厂思想。
*/

#include <stdio.h>
#include <string.h>

// 模拟内核PCI设备ID结构体（驱动与设备匹配的依据）
typedef struct {
    int vendor_id;  // 厂商ID（如Intel=8086，NVIDIA=10de）
    int device_id;  // 设备ID
} pci_device_id;

// 1. 抽象产品：PCI驱动接口（所有PCI驱动遵循此规范）
typedef struct {
    const pci_device_id *id_table; // 匹配ID表
    void (*probe)(void);           // 设备匹配成功后初始化
    void (*remove)(void);          // 设备移除时清理
} pci_driver;

// 2. 具体产品1：Intel网卡PCI驱动
static const pci_device_id eth_pci_ids[] = {
    {8086, 100E}, // Intel 82540EM网卡（vendor=8086，device=100E）
    {0, 0}        // 结束标记
};
static pci_driver eth_pci_drv = {
    .id_table = eth_pci_ids,
    .probe = (void)() { printf("[Intel网卡驱动] 初始化DMA、中断\n"); },
    .remove = (void)() { printf("[Intel网卡驱动] 释放DMA、中断\n"); }
};

// 3. 具体产品2：NVIDIA显卡PCI驱动
static const pci_device_id gpu_pci_ids[] = {
    {10de, 1e82}, // NVIDIA RTX 2080（vendor=10de，device=1e82）
    {0, 0}
};
static pci_driver gpu_pci_drv = {
    .id_table = gpu_pci_ids,
    .probe = (void)() { printf("[NVIDIA显卡驱动] 初始化显存、显示控制器\n"); },
    .remove = (void)() { printf("[NVIDIA显卡驱动] 释放显存、显示控制器\n"); }
};

// 4. 工厂函数1：PCI设备匹配（核心：根据ID找到对应驱动）
static pci_driver* pci_match_factory(int vendor_id, int device_id) {
    // 遍历所有PCI驱动，匹配ID（内核中是遍历驱动链表）
    pci_driver *drivers[] = {&eth_pci_drv, &gpu_pci_drv, NULL};
    for (int i=0; drivers[i]; i++) {
        const pci_device_id *id = drivers[i]->id_table;
        for (; id->vendor_id != 0; id++) {
            if (id->vendor_id == vendor_id && id->device_id == device_id) {
                printf("PCI工厂：匹配到驱动（vendor=%d, device=%d）\n", vendor_id, device_id);
                return drivers[i];
            }
        }
    }
    printf("PCI工厂：无匹配驱动（vendor=%d, device=%d）\n", vendor_id, device_id);
    return NULL;
}

// 5. 工厂函数2：创建PCI驱动实例（封装匹配+初始化）
pci_driver* pci_create_driver(int vendor_id, int device_id) {
    pci_driver *drv = pci_match_factory(vendor_id, device_id);
    if (drv) {
        drv->probe(); // 匹配成功，初始化驱动
    }
    return drv;
}

// 测试
int main() {
    // 创建Intel网卡驱动实例
    pci_create_driver(8086, 100E);
    printf("---\n");
    // 创建NVIDIA显卡驱动实例
    pci_create_driver(10de, 1e82);
    return 0;
}