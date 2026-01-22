/*
代码说明
抽象产品：chrdev_ops定义了所有字符设备的统一接口，保证不同设备的操作方式一致；
工厂封装：chrdev_factory集中处理设备类型判断和实例返回，用户无需知道led_dev_ops/uart_dev_ops的存在；
扩展友好：新增按键设备时，只需新增key_dev_ops结构体，并在工厂函数中添加"key"的判断分支即可。
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 1. 抽象产品：字符设备操作接口（所有字符设备遵循此规范）
typedef struct {
    void (*open)(void);  // 打开设备
    void (*read)(void);  // 读取数据
    void (*write)(void); // 写入数据
} chrdev_ops;

// 2. 具体产品1：LED设备（实现抽象产品接口）
static chrdev_ops led_dev_ops = {
    .open = (void)() { printf("[LED设备] 打开GPIO，配置输出模式\n"); },
    .read = (void)() { printf("[LED设备] 读取LED状态（亮/灭）\n"); },
    .write = (void)() { printf("[LED设备] 控制LED亮灭\n"); }
};

// 3. 具体产品2：串口设备（实现抽象产品接口）
static chrdev_ops uart_dev_ops = {
    .open = (void)() { printf("[串口设备] 初始化波特率115200，配置引脚\n"); },
    .read = (void)() { printf("[串口设备] 从接收寄存器读取数据\n"); },
    .write = (void)() { printf("[串口设备] 向发送寄存器写入数据\n"); }
};

// 4. 简单工厂函数：封装创建逻辑，根据设备名返回对应产品实例
chrdev_ops* chrdev_factory(const char *dev_name) {
    if (dev_name == NULL) return NULL;
    
    // 根据参数判断创建哪种产品，封装所有创建细节
    if (strcmp(dev_name, "led") == 0) {
        printf("工厂创建LED设备实例\n");
        return &led_dev_ops;
    } else if (strcmp(dev_name, "uart") == 0) {
        printf("工厂创建串口设备实例\n");
        return &uart_dev_ops;
    }
    
    printf("工厂：未知设备类型 %s\n", dev_name);
    return NULL;
}

// 5. 用户逻辑：仅调用工厂接口，无需关心创建细节
int main() {
    // 创建LED设备实例
    chrdev_ops *led_dev = chrdev_factory("led");
    if (led_dev) {
        led_dev->open();
        led_dev->read();
    }
    printf("---\n");
    
    // 创建串口设备实例
    chrdev_ops *uart_dev = chrdev_factory("uart");
    if (uart_dev) {
        uart_dev->open();
        uart_dev->write();
    }
    
    // 扩展：新增按键设备，仅需新增产品实现+工厂函数判断，无需修改用户逻辑
    return 0;
}