#include <stdio.h>
// 1. 【抽象步骤】内核核心结构体：file_operations 函数指针集合
typedef struct {
    int  (*open)(void);        // 打开：可变步骤
    int  (*read)(char *buf);   // 读取：可变步骤
    int  (*write)(char *buf);  // 写入：可变步骤
    void (*release)(void);     // 释放：可变步骤
} file_operations;

// 2. 【固定骨架】VFS模板函数：内核通用的读流程，所有设备共用，永不修改
int sys_read(const file_operations *fops, char *buf) {
    printf("[VFS模板] 进入统一读流程 → 查找设备/文件映射\n");
    int ret = fops->read(buf); // 调用具体的读实现
    printf("[VFS模板] 读流程结束 → 返回数据\n");
    return ret;
}

// ===================== 具体实现：串口设备驱动 =====================
int uart_open(void)  { printf("[串口驱动] 初始化GPIO+波特率\n"); return 0; }
int uart_read(char *buf) { printf("[串口驱动] 从寄存器读取数据\n"); return 1; }
int uart_write(char *buf){ printf("[串口驱动] 向寄存器写入数据\n"); return 1; }
void uart_release(void) { printf("[串口驱动] 释放串口GPIO\n"); }
// 绑定：串口的操作集
const file_operations uart_fops = {
    .open = uart_open,
    .read = uart_read,
    .write = uart_write,
    .release = uart_release
};

// ===================== 具体实现：LED字符设备 =====================
int led_open(void)  { printf("[LED驱动] 初始化LED GPIO为输出\n"); return 0; }
int led_read(char *buf) { printf("[LED驱动] 读取LED状态\n"); return 1; }
int led_write(char *buf){ printf("[LED驱动] 控制LED亮灭\n"); return 1; }
void led_release(void) { printf("[LED驱动] 释放LED GPIO\n"); }
// 绑定：LED的操作集
const file_operations led_fops = {
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release
};

// 调用测试
int main() {
    char buf[32];
    sys_read(&uart_fops, buf); // 串口读：模板+串口实现
    sys_read(&led_fops, buf);  // LED读：模板+LED实现
    return 0;
}