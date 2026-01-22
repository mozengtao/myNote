// file: block_template.c
#include <stdio.h>

// 1. 块设备操作模板
typedef struct block_device_ops {
    int (*open)(void);
    int (*release)(void);
    int (*ioctl)(unsigned int cmd, unsigned long arg);
    int (*submit_bio)(void *bio);
} blk_ops_t;

// 2. 块设备结构
typedef struct gendisk {
    char name[32];
    int major;
    int minor;
    blk_ops_t *ops;
} gendisk_t;

// 3. 具体块设备驱动实现
// 3.1 SCSI磁盘驱动
int scsi_open(void) {
    printf("[SCSI] 发送INQUIRY命令，检测磁盘\n");
    return 0;
}

int scsi_submit_bio(void *bio) {
    printf("[SCSI] 转换BIO为SCSI命令，放入队列\n");
    return 0;
}

blk_ops_t scsi_ops = {
    .open = scsi_open,
    .submit_bio = scsi_submit_bio,
};

// 3.2 NVMe驱动
int nvme_open(void) {
    printf("[NVMe] 配置Admin队列，识别命名空间\n");
    return 0;
}

int nvme_submit_bio(void *bio) {
    printf("[NVMe] 提交NVMe命令到SQ，门铃寄存\n");
    return 0;
}

blk_ops_t nvme_ops = {
    .open = nvme_open,
    .submit_bio = nvme_submit_bio,
};

// 4. 模板方法 - 通用块层处理
void submit_bio(gendisk_t *disk, void *bio) {
    printf("块层: 收到BIO请求\n");
    
    // 固定步骤1: 合并相邻请求
    printf("块层: 尝试合并相邻请求\n");
    
    // 固定步骤2: 调度算法
    printf("块层: 调用调度器(CFQ/NOOP/Deadline)\n");
    
    // 可变步骤: 调用具体驱动的提交函数
    disk->ops->submit_bio(bio);
    
    // 固定步骤3: 触发完成中断
    printf("块层: 等待完成中断\n");
}

// 5. 使用示例
int main() {
    gendisk_t scsi_disk = { .name = "sda", .ops = &scsi_ops };
    gendisk_t nvme_disk = { .name = "nvme0n1", .ops = &nvme_ops };
    
    printf("=== 操作SCSI硬盘 ===\n");
    scsi_disk.ops->open();
    submit_bio(&scsi_disk, NULL);
    
    printf("\n=== 操作NVMe SSD ===\n");
    nvme_disk.ops->open();
    submit_bio(&nvme_disk, NULL);
    
    return 0;
}