// file: fs_template.c
#include <stdio.h>

// 1. 定义操作模板结构体
typedef struct file_operations {
    int (*open)(const char *path);
    int (*read)(void *buf, size_t size);
    int (*write)(const void *buf, size_t size);
    int (*close)(void);
} fops_t;

// 2. 文件系统基类
typedef struct {
    const char *name;
    fops_t *fops;
} file_system;

// 3. 具体文件系统实现
// 3.1 procfs实现
int proc_open(const char *path) {
    printf("[procfs] 打开虚拟文件: %s\n", path);
    return 0;
}
int proc_read(void *buf, size_t size) {
    printf("[procfs] 读取进程信息\n");
    return 0;
}
int proc_write(const void *buf, size_t size) {
    printf("[procfs] 写入配置\n");
    return 0;
}
int proc_close(void) {
    printf("[procfs] 关闭文件\n");
    return 0;
}

fops_t proc_fops = {
    .open = proc_open,
    .read = proc_read,
    .write = proc_write,
    .close = proc_close,
};

// 3.2 ext4实现
int ext4_open(const char *path) {
    printf("[ext4] 打开磁盘文件: %s\n", path);
    return 0;
}
int ext4_read(void *buf, size_t size) {
    printf("[ext4] 读取磁盘数据\n");
    return 0;
}
int ext4_write(const void *buf, size_t size) {
    printf("[ext4] 写入磁盘数据\n");
    return 0;
}
int ext4_close(void) {
    printf("[ext4] 关闭文件，刷新缓存\n");
    return 0;
}

fops_t ext4_fops = {
    .open = ext4_open,
    .read = ext4_read,
    .write = ext4_write,
    .close = ext4_close,
};

// 4. 模板方法 - 统一文件操作接口
int vfs_open(file_system *fs, const char *path) {
    printf("VFS: 开始打开文件\n");
    int ret = fs->fops->open(path);
    printf("VFS: 文件打开完成\n");
    return ret;
}

int vfs_read(file_system *fs, void *buf, size_t size) {
    printf("VFS: 开始读取文件\n");
    int ret = fs->fops->read(buf, size);
    printf("VFS: 读取完成，字节数: %d\n", ret);
    return ret;
}

// 5. 使用示例
int main() {
    // 创建不同的文件系统实例
    file_system procfs = { .name = "proc", .fops = &proc_fops };
    file_system ext4fs = { .name = "ext4", .fops = &ext4_fops };

    // 通过统一接口操作不同文件系统
    printf("=== 操作procfs ===\n");
    vfs_open(&procfs, "/proc/cpuinfo");
    vfs_read(&procfs, NULL, 1024);

    printf("\n=== 操作ext4 ===\n");
    vfs_open(&ext4fs, "/home/test.txt");
    vfs_write(&ext4fs, "data", 4);

    return 0;
}