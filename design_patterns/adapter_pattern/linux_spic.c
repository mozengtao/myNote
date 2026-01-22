/*
场景说明
内核 SPI 核心层定义了统一的spi_transfer目标接口，但不同 CPU 厂商（如 Intel/ARM/ 树莓派）的 SPI 控制器硬件原生接口差异极大（参数、寄存器、操作逻辑不同）。内核通过spi_master适配器结构体，将不同厂商的 SPI 控制器接口转换为统一的spi_transfer接口，使上层 SPI 设备驱动（如传感器、显示屏）无需适配不同硬件

核心说明
内核中spi_master是核心适配器结构体，每个厂商的 SPI 控制器只需实现transfer接口（适配器层），即可接入 SPI 核心层；
上层 SPI 设备驱动（如温湿度传感器）仅调用spi_transfer，无需关心底层是 BCM2835 还是 Intel PCH 的 SPI 控制器；
适配器层仅做接口转换，不修改硬件原生操作逻辑，保证了硬件驱动的稳定性。
*/

#include <stdio.h>

// 模拟内核SPI核心层的目标接口（统一操作接口）
typedef struct {
    // 目标接口：SPI传输（上层驱动仅调用此接口）
    int (*transfer)(int addr, char *tx_buf, char *rx_buf, int len);
} spi_master; // SPI适配器核心结构体

// ---------------------- 被适配者：不同厂商的SPI控制器原生接口 ----------------------
// 厂商1：树莓派BCM2835 SPI控制器原生接口
int bcm2835_spi_xfer(int reg, char *tx, char *rx, int length) {
    printf("[被适配者-BCM2835 SPI] 硬件操作：reg=0x%x, len=%d\n", reg, length);
    memcpy(rx, tx, length); // 模拟数据传输
    return 0;
}

// 厂商2：Intel PCH SPI控制器原生接口
int intel_pch_spi_send_recv(int address, char *send, char *recv, int size) {
    printf("[被适配者-Intel PCH SPI] 硬件操作：addr=0x%x, size=%d\n", address, size);
    memcpy(recv, send, size); // 模拟数据传输
    return 0;
}

// ---------------------- 适配器：封装被适配者，实现目标接口 ----------------------
// 适配器1：树莓派BCM2835 SPI适配器
static int bcm2835_spi_adapter_xfer(int addr, char *tx_buf, char *rx_buf, int len) {
    // 接口转换：统一目标接口 → BCM2835原生接口
    return bcm2835_spi_xfer(addr, tx_buf, rx_buf, len);
}
static spi_master bcm2835_spi_master = {
    .transfer = bcm2835_spi_adapter_xfer // 实现目标接口
};

// 适配器2：Intel PCH SPI适配器
static int intel_pch_spi_adapter_xfer(int addr, char *tx_buf, char *rx_buf, int len) {
    // 接口转换：统一目标接口 → Intel PCH原生接口
    return intel_pch_spi_send_recv(addr, tx_buf, rx_buf, len);
}
static spi_master intel_pch_spi_master = {
    .transfer = intel_pch_spi_adapter_xfer // 实现目标接口
};

// ---------------------- 上层SPI设备驱动：仅依赖统一目标接口 ----------------------
void spi_sensor_read(spi_master *master, int sensor_addr, char *data, int len) {
    printf("\n[上层传感器驱动] 读取SPI传感器数据\n");
    master->transfer(sensor_addr, NULL, data, len); // 调用统一接口
    printf("[上层传感器驱动] 读取完成：%.*s\n", len, data);
}

// 主函数测试（模拟不同平台的SPI传感器驱动）
int main() {
    char data[4] = {0};
    
    // 树莓派平台：使用BCM2835 SPI适配器
    spi_sensor_read(&bcm2835_spi_master, 0x48, data, 4);
    
    // Intel平台：使用Intel PCH SPI适配器（上层驱动无需修改）
    spi_sensor_read(&intel_pch_spi_master, 0x48, data, 4);
    
    return 0;
}