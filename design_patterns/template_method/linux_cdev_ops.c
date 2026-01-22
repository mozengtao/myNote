#include <stdio.h>
// 1. 抽象步骤：文件操作函数指针（复用）
typedef struct {
    int (*open)(void);
    int (*read)(char *buf);
} file_operations;

// 2. 字符设备核心结构体：绑定 设备号 + 操作集
typedef struct {
    int dev_id;                // 设备号
    const file_operations *ops;// 绑定具体驱动实现
} cdev;

// 3. 【固定骨架】内核模板函数：字符设备注册流程，所有驱动共用
int register_chrdev(cdev *dev, int dev_id, const file_operations *fops) {
    printf("[内核模板] 注册字符设备 → 分配设备号：%d\n", dev_id);
    dev->dev_id = dev_id;
    dev->ops = fops;
    printf("[内核模板] 注册完成，绑定操作集\n");
    return 0;
}

// ===================== 具体实现：按键驱动 =====================
int key_open(void) { printf("[按键驱动] 配置GPIO为输入模式\n"); return 0; }
int key_read(char *buf) { printf("[按键驱动] 读取按键电平状态\n"); return 1; }
const file_operations key_fops = {.open=key_open, .read=key_read};

// 调用测试
int main() {
    cdev key_dev;
    register_chrdev(&key_dev, 240, &key_fops); // 注册按键设备
    key_dev.ops->open(); // 执行具体打开逻辑
    return 0;
}