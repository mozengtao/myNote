/*
C 语言无 “继承”“类”，但通过「函数指针结构体（目标接口）+ 适配器封装被适配者」可完美实现适配器模式，以下是内核风格的通用范式（SPI 控制器适配器，内核最典型的适配器场景）：

代码说明
无侵入性：厂商 A/B 的 SPI 控制器接口未做任何修改，仅通过适配器层转换；
接口统一：上层spi_device_operate仅调用spi_adapter_ops接口，无需知道底层是厂商 A 还是 B；
扩展友好：新增厂商 C 的 SPI 控制器时，只需新增spi_c_adapter，无需修改上层逻辑；
内核风格：用函数指针结构体定义目标接口，适配器直接封装被适配者，贴近内核实现习惯。
*/

#include <stdio.h>
#include <string.h>

// ---------------------- 1. 被适配者（Adaptee）：不同厂商的SPI控制器原生接口 ----------------------
// 厂商A的SPI控制器接口（不兼容：参数为reg/val，函数名不同）
void spi_ctrl_a_write(int reg, int val) {
    printf("[被适配者-厂商A SPI] 写寄存器0x%x，值0x%x\n", reg, val);
}
int spi_ctrl_a_read(int reg) {
    printf("[被适配者-厂商A SPI] 读寄存器0x%x → 返回0x12\n", reg);
    return 0x12;
}

// 厂商B的SPI控制器接口（不兼容：参数为addr/data，函数名不同）
void spi_ctrl_b_send(int addr, int data) {
    printf("[被适配者-厂商B SPI] 发地址0x%x，数据0x%x\n", addr, data);
}
int spi_ctrl_b_recv(int addr) {
    printf("[被适配者-厂商B SPI] 收地址0x%x → 返回0x34\n", addr);
    return 0x34;
}

// ---------------------- 2. 目标接口（Target）：内核统一的SPI操作接口 ----------------------
typedef struct {
    void (*write)(int addr, int data); // 统一写接口
    int (*read)(int addr);             // 统一读接口
} spi_adapter_ops; // 目标接口：上层模块仅依赖此接口

// ---------------------- 3. 适配器（Adapter）：封装被适配者，实现目标接口 ----------------------
// 适配器1：适配厂商A的SPI控制器
static spi_adapter_ops spi_a_adapter = {
    // 实现目标写接口：转换参数，调用厂商A的原生接口
    .write = (int addr, int data) {
        spi_ctrl_a_write(addr, data); // 接口转换：统一接口 → 厂商A接口
    },
    // 实现目标读接口：转换参数，调用厂商A的原生接口
    .read = (int addr) {
        return spi_ctrl_a_read(addr); // 接口转换：统一接口 → 厂商A接口
    }
};

// 适配器2：适配厂商B的SPI控制器
static spi_adapter_ops spi_b_adapter = {
    // 实现目标写接口：转换参数，调用厂商B的原生接口
    .write = (int addr, int data) {
        spi_ctrl_b_send(addr, data); // 接口转换：统一接口 → 厂商B接口
    },
    // 实现目标读接口：转换参数，调用厂商B的原生接口
    .read = (int addr) {
        return spi_ctrl_b_recv(addr); // 接口转换：统一接口 → 厂商B接口
    }
};

// ---------------------- 4. 上层模块：仅依赖目标接口，无需感知底层差异 ----------------------
void spi_device_operate(spi_adapter_ops *adapter, int addr, int write_data) {
    printf("\n[上层SPI驱动] 执行统一SPI操作\n");
    adapter->write(addr, write_data); // 调用统一目标接口
    int read_data = adapter->read(addr);
    printf("[上层SPI驱动] 读取结果：0x%x\n", read_data);
}

// 主函数测试
int main() {
    // 使用厂商A的SPI适配器（上层逻辑无需修改）
    spi_device_operate(&spi_a_adapter, 0x10, 0x20);
    
    // 使用厂商B的SPI适配器（上层逻辑无需修改）
    spi_device_operate(&spi_b_adapter, 0x10, 0x20);
    
    return 0;
}