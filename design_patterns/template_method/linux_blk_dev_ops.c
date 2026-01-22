#include <stdio.h>

// 1. 【抽象可变步骤】块设备操作函数指针结构体（内核定义：linux/blkdev.h）
typedef struct {
    int (*open)(void);          // 打开块设备
    int (*rw_block)(int rw, int block_id, char *buf); // 读写块（rw=0读/1写）
    void (*release)(void);      // 释放块设备
} block_device_operations;

// 2. 【固定模板骨架】内核块设备注册流程（所有块设备共用，永不修改）
int register_blkdev(int major, const char *name, const block_device_operations *ops) {
    printf("[内核模板] 注册块设备 → 主设备号：%d，设备名：%s\n", major, name);
    // 内核底层逻辑：分配设备号、绑定操作集、加入块设备链表
    return 0;
}

// 3. 【固定模板骨架】内核通用块读写流程（所有块设备共用）
int blkdev_rw(int rw, int block_id, char *buf, const block_device_operations *ops) {
    printf("[内核模板] 块设备IO调度 → 操作：%s，块号：%d\n", rw==0?"读":"写", block_id);
    int ret = ops->rw_block(rw, block_id, buf); // 调用驱动具体实现
    printf("[内核模板] 块IO完成 → 结果：%d\n", ret);
    return ret;
}

// ===================== 具体实现：SD卡块设备驱动 =====================
int sdcard_open(void) {
    printf("[SD卡驱动] 初始化SD卡，配置SPI总线\n");
    return 0;
}
int sdcard_rw_block(int rw, int block_id, char *buf) {
    printf("[SD卡驱动] %s块 → 块号：%d，数据：%s\n", rw==0?"读取":"写入", block_id, buf);
    return 1; // 模拟操作成功
}
void sdcard_release(void) {
    printf("[SD卡驱动] 释放SD卡SPI总线\n");
}
// 绑定：SD卡操作集
const block_device_operations sdcard_fops = {
    .open = sdcard_open,
    .rw_block = sdcard_rw_block,
    .release = sdcard_release
};

// 调用测试
int main() {
    char buf[] = "test_data";
    register_blkdev(31, "sdcard", &sdcard_fops); // 注册SD卡块设备
    blkdev_rw(0, 100, buf, &sdcard_fops);        // 读SD卡第100块
    return 0;
}