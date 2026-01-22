#include <stdio.h>
#include <string.h>

// 1. 【抽象可变步骤】SPI驱动结构体（内核定义：linux/spi/spi.h）
typedef struct {
    const char *name;         // 匹配用设备名
    int (*probe)(int spi_id); // 匹配成功初始化
    void (*remove)(int spi_id);// 移除清理
} spi_driver;

// 2. 【固定模板骨架】内核SPI设备匹配逻辑
int spi_match_device(const spi_driver *drv, const char *dev_name) {
    printf("[内核模板] SPI设备匹配 → 设备名：%s\n", dev_name);
    return strcmp(drv->name, dev_name) == 0; // 名字匹配则成功
}

// 3. 【固定模板骨架】内核SPI枚举+探测流程
void spi_enumerate_and_probe(const spi_driver *drv, int spi_id, const char *dev_name) {
    printf("[内核模板] 枚举SPI设备 → SPI总线ID：%d\n", spi_id);
    if (spi_match_device(drv, dev_name)) {
        printf("[内核模板] 匹配成功 → 调用SPI驱动probe\n");
        drv->probe(spi_id);
    }
}

// 4. 【固定模板骨架】内核SPI驱动注册流程
int spi_register_driver(const spi_driver *drv) {
    printf("[内核模板] 注册SPI驱动 → 加入SPI驱动链表\n");
    return 0;
}

// ===================== 具体实现：SPI显示屏驱动 =====================
int lcd_spi_probe(int spi_id) {
    printf("[SPI屏驱动] 初始化显示屏 → 配置SPI速率=10MHz，复位屏幕\n");
    return 0;
}
void lcd_spi_remove(int spi_id) {
    printf("[SPI屏驱动] 移除显示屏 → 关闭SPI总线\n");
}
// 绑定：SPI屏驱动结构体
const spi_driver lcd_spi_drv = {
    .name = "lcd_st7789",
    .probe = lcd_spi_probe,
    .remove = lcd_spi_remove
};

// 调用测试
int main() {
    spi_register_driver(&lcd_spi_drv); // 注册SPI屏驱动
    spi_enumerate_and_probe(&lcd_spi_drv, 0, "lcd_st7789"); // 枚举SPI0上的屏
    return 0;
}