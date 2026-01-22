/*
场景说明
内核字符设备框架提供了统一的工厂接口，根据设备类型（LED / 串口 / 按键）创建对应字符设备实例，封装了设备号分配、操作集绑定等创建细节。

核心说明
内核中cdev_init+cdev_add构成字符设备工厂：用户只需提供file_operations（操作集），工厂函数自动完成设备号分配、设备注册等创建逻辑；
所有字符设备（LED / 串口 / 按键）均通过此工厂创建，用户无需关心内核的设备管理细节。
*/

#include <stdio.h>
#include <string.h>

// 1. 抽象产品：字符设备操作接口
typedef struct {
    void (*open)(void);
    void (*read)(void);
} chrdev_ops;

// 2. 具体产品：不同字符设备的操作集
static chrdev_ops led_ops = { .open=() { printf("LED打开\n"); }, .read=() { printf("LED读取\n"); } };
static chrdev_ops key_ops = { .open=() { printf("按键打开\n"); }, .read=() { printf("按键读取\n"); } };

// 3. 字符设备结构体（产品实例）
typedef struct {
    int dev_num;       // 设备号
    chrdev_ops *ops;   // 绑定的操作集
} cdev;

// 4. 工厂函数：创建字符设备实例（封装设备号分配+操作集绑定）
cdev* cdev_factory(const char *dev_name, int dev_num) {
    cdev *dev = (cdev*)malloc(sizeof(cdev));
    if (!dev) return NULL;
    
    // 绑定操作集（创建逻辑封装）
    if (strcmp(dev_name, "led") == 0) {
        dev->ops = &led_ops;
    } else if (strcmp(dev_name, "key") == 0) {
        dev->ops = &key_ops;
    } else {
        free(dev);
        return NULL;
    }
    
    // 分配设备号（创建逻辑封装）
    dev->dev_num = dev_num;
    printf("字符设备工厂：创建%s设备（设备号=%d）\n", dev_name, dev_num);
    return dev;
}

// 测试
int main() {
    // 创建LED字符设备（设备号240）
    cdev *led_dev = cdev_factory("led", 240);
    if (led_dev) led_dev->ops->open();
    
    // 创建按键字符设备（设备号241）
    cdev *key_dev = cdev_factory("key", 241);
    if (key_dev) key_dev->ops->read();
    return 0;
}