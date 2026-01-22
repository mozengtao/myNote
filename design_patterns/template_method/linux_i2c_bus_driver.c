#include <stdio.h>
#include <string.h>

// 1. 【抽象可变步骤】I2C驱动结构体（内核定义：linux/i2c.h）
typedef struct {
    const char *name;         // 匹配用设备名
    int (*probe)(int i2c_id); // 匹配成功初始化
    void (*remove)(int i2c_id);// 移除清理
} i2c_driver;

// 2. 【固定模板骨架】内核I2C设备匹配逻辑
int i2c_match_device(const i2c_driver *drv, const char *dev_name) {
    printf("[内核模板] I2C设备匹配 → 设备名：%s\n", dev_name);
    return strcmp(drv->name, dev_name) == 0;
}

// 3. 【固定模板骨架】内核I2C枚举+探测流程
void i2c_enumerate_and_probe(const i2c_driver *drv, int i2c_id, const char *dev_name) {
    printf("[内核模板] 枚举I2C设备 → I2C总线ID：%d\n", i2c_id);
    if (i2c_match_device(drv, dev_name)) {
        printf("[内核模板] 匹配成功 → 调用I2C驱动probe\n");
        drv->probe(i2c_id);
    }
}

// 4. 【固定模板骨架】内核I2C驱动注册流程
int i2c_register_driver(const i2c_driver *drv) {
    printf("[内核模板] 注册I2C驱动 → 加入I2C驱动链表\n");
    return 0;
}

// ===================== 具体实现：I2C温湿度传感器驱动 =====================
int dht11_i2c_probe(int i2c_id) {
    printf("[I2C传感器驱动] 初始化DHT11 → 配置I2C地址0x48，读取校准值\n");
    return 0;
}
void dht11_i2c_remove(int i2c_id) {
    printf("[I2C传感器驱动] 移除DHT11 → 释放I2C总线\n");
}
// 绑定：I2C传感器驱动结构体
const i2c_driver dht11_i2c_drv = {
    .name = "dht11",
    .probe = dht11_i2c_probe,
    .remove = dht11_i2c_remove
};

// 调用测试
int main() {
    i2c_register_driver(&dht11_i2c_drv); // 注册I2C传感器驱动
    i2c_enumerate_and_probe(&dht11_i2c_drv, 1, "dht11"); // 枚举I2C1上的DHT11
    return 0;
}