/*
场景说明
内核块设备映射器（Device Mapper，dm）是适配器模式的典型：不同的底层块设备（sda1/loop0/LVM）有不同的操作接口，但 dm 通过适配器层将这些设备适配为统一的块设备接口，向上提供/dev/mapper/设备，使文件系统、应用程序无需关心底层块设备的类型。

核心说明
dm 适配器层将不同底层块设备的接口转换为统一的block_dev_ops接口，上层文件系统仅与 dm 适配器交互；
dm-crypt、dm-raid 等功能均基于此适配器模式：在 dm 适配器层添加加密 /raid 逻辑，底层块设备接口无需修改；
适配器层实现了 “逻辑与物理分离”，用户可灵活组合不同底层块设备，而上层无需感知。
*/

#include <stdio.h>
#include <string.h>

// 模拟内核块设备目标接口（统一操作接口）
typedef struct {
    int (*read)(int sector, char *buf, int len); // 统一读接口
    int (*write)(int sector, char *buf, int len);// 统一写接口
} block_dev_ops;

// ---------------------- 被适配者：不同底层块设备的原生接口 ----------------------
// 被适配者1：物理磁盘sda1的原生接口
int sda1_read(int sec, char *b, int l) {
    printf("[被适配者-sda1] 读扇区%d，长度%d\n", sec, l);
    strcpy(b, "sda1_data");
    return l;
}
int sda1_write(int sec, char *b, int l) {
    printf("[被适配者-sda1] 写扇区%d，数据：%s\n", sec, b);
    return l;
}

// 被适配者2：循环设备loop0的原生接口
int loop0_read(int sector_num, char *buffer, int length) {
    printf("[被适配者-loop0] 读扇区%d，长度%d\n", sector_num, length);
    strcpy(buffer, "loop0_data");
    return length;
}
int loop0_write(int sector_num, char *buffer, int length) {
    printf("[被适配者-loop0] 写扇区%d，数据：%s\n", sector_num, buffer);
    return length;
}

// ---------------------- 适配器：dm适配器封装底层块设备 ----------------------
// dm适配器1：适配sda1到统一块设备接口
static block_dev_ops dm_sda1_adapter = {
    .read = (int sec, char *b, int l) {
        return sda1_read(sec, b, l); // 接口转换
    },
    .write = (int sec, char *b, int l) {
        return sda1_write(sec, b, l); // 接口转换
    }
};

// dm适配器2：适配loop0到统一块设备接口
static block_dev_ops dm_loop0_adapter = {
    .read = (int sec, char *b, int l) {
        return loop0_read(sec, b, l); // 接口转换
    },
    .write = (int sec, char *b, int l) {
        return loop0_write(sec, b, l); // 接口转换
    }
};

// ---------------------- 上层文件系统：仅依赖统一块设备接口 ----------------------
void fs_read(block_dev_ops *bdev, int sector, char *buf) {
    printf("\n[上层ext4文件系统] 读取块设备扇区%d\n", sector);
    bdev->read(sector, buf, 8);
    printf("[上层ext4文件系统] 读取数据：%s\n", buf);
}

// 主函数测试
int main() {
    char buf[32] = {0};
    
    // 读取dm适配后的sda1设备（/dev/mapper/sda1）
    fs_read(&dm_sda1_adapter, 100, buf);
    
    // 读取dm适配后的loop0设备（/dev/mapper/loop0）
    fs_read(&dm_loop0_adapter, 100, buf);
    
    return 0;
}