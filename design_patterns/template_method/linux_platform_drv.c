// file: platform_template.c
#include <stdio.h>

// 1. 平台驱动操作模板
typedef struct platform_driver_ops {
    int (*probe)(void);
    int (*remove)(void);
    int (*suspend)(void);
    int (*resume)(void);
} platform_ops_t;

// 2. 平台设备驱动结构
typedef struct platform_driver {
    const char *name;
    platform_ops_t *ops;
} platform_driver_t;

// 3. 具体驱动实现
// 3.1 GPIO驱动
int gpio_probe(void) {
    printf("[GPIO驱动] 映射GPIO寄存器，申请IRQ\n");
    return 0;
}

int gpio_remove(void) {
    printf("[GPIO驱动] 释放GPIO引脚，注销设备\n");
    return 0;
}

platform_ops_t gpio_ops = {
    .probe = gpio_probe,
    .remove = gpio_remove,
};

// 3.2 I2C控制器驱动
int i2c_probe(void) {
    printf("[I2C驱动] 初始化I2C适配器，注册算法\n");
    return 0;
}

int i2c_suspend(void) {
    printf("[I2C驱动] 保存寄存器状态，进入低功耗\n");
    return 0;
}

int i2c_resume(void) {
    printf("[I2C驱动] 恢复寄存器状态，重新初始化\n");
    return 0;
}

platform_ops_t i2c_ops = {
    .probe = i2c_probe,
    .suspend = i2c_suspend,
    .resume = i2c_resume,
};

// 4. 模板方法 - 总线核心处理
int platform_driver_register(platform_driver_t *drv) {
    printf("平台总线: 注册驱动 %s\n", drv->name);
    
    // 固定步骤1: 添加到驱动链表
    printf("平台总线: 添加到全局驱动列表\n");
    
    // 固定步骤2: 匹配设备
    printf("平台总线: 尝试匹配设备树中的设备\n");
    
    // 可变步骤: 如果匹配成功，调用驱动的probe
    if (1) { // 假设匹配成功
        printf("平台总线: 找到匹配设备，调用驱动的probe\n");
        drv->ops->probe();
    }
    
    return 0;
}

int platform_driver_unregister(platform_driver_t *drv) {
    printf("平台总线: 注销驱动 %s\n", drv->name);
    
    // 可变步骤: 调用驱动的remove
    if (drv->ops->remove)
        drv->ops->remove();
    
    // 固定步骤: 从链表移除
    printf("平台总线: 从驱动列表移除\n");
    
    return 0;
}

// 5. 使用示例
int main() {
    platform_driver_t gpio_drv = { .name = "gpio-xx", .ops = &gpio_ops };
    platform_driver_t i2c_drv = { .name = "i2c-xx", .ops = &i2c_ops };
    
    printf("=== 注册GPIO驱动 ===\n");
    platform_driver_register(&gpio_drv);
    platform_driver_unregister(&gpio_drv);
    
    printf("\n=== 注册I2C驱动 ===\n");
    platform_driver_register(&i2c_drv);
    // 模拟电源管理
    i2c_drv.ops->suspend();
    i2c_drv.ops->resume();
    
    return 0;
}