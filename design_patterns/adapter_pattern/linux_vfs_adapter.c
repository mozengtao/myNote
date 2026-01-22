/*
场景说明
Linux 内核的虚拟文件系统（VFS）是适配器模式的极致体现：ext4/tmpfs/ramfs/NFS 等不同文件系统的实现逻辑和接口差异极大，但 VFS 定义了统一的file_operations目标接口，每个文件系统通过适配器层（如ext4_file_operations）将自身接口转换为 VFS 接口，使内核和用户态进程无需关心具体的文件系统类型。

核心说明
VFS 的file_operations是统一目标接口，每个文件系统的xxx_file_operations是适配器层；
适配器层将 ext4/tmpfs 的原生操作（如ext4_open_file）转换为 VFS 的open接口，内核核心逻辑仅与 VFS 交互；
新增文件系统时，只需实现对应的适配器（xxx_file_operations），无需修改 VFS 核心，符合 “开闭原则”。
*/

#include <stdio.h>
#include <string.h>

// 模拟VFS统一目标接口（文件操作）
typedef struct {
    int (*open)(const char *path);  // 统一open接口
    int (*read)(int fd, char *buf); // 统一read接口
} file_operations;

// ---------------------- 被适配者：不同文件系统的原生接口 ----------------------
// 被适配者1：ext4文件系统原生接口
int ext4_open_file(const char *file_path) {
    printf("[被适配者-ext4] 打开文件：%s（解析inode、超级块）\n", file_path);
    return 1; // 返回fd=1
}
int ext4_read_file(int file_fd, char *buffer) {
    printf("[被适配者-ext4] 读取fd=%d（从磁盘读取）\n", file_fd);
    strcpy(buffer, "ext4_file_content");
    return strlen(buffer);
}

// 被适配者2：tmpfs内存文件系统原生接口
int tmpfs_open(const char *path) {
    printf("[被适配者-tmpfs] 打开文件：%s（分配内存inode）\n", path);
    return 2; // 返回fd=2
}
int tmpfs_read(int fd, char *buf) {
    printf("[被适配者-tmpfs] 读取fd=%d（从内存读取）\n", fd);
    strcpy(buf, "tmpfs_file_content");
    return strlen(buf);
}

// ---------------------- 适配器：VFS适配器封装不同文件系统 ----------------------
// ext4适配器：实现VFS统一接口
static file_operations ext4_fops = {
    .open = (const char *path) {
        return ext4_open_file(path); // 接口转换
    },
    .read = (int fd, char *buf) {
        return ext4_read_file(fd, buf); // 接口转换
    }
};

// tmpfs适配器：实现VFS统一接口
static file_operations tmpfs_fops = {
    .open = (const char *path) {
        return tmpfs_open(path); // 接口转换
    },
    .read = (int fd, char *buf) {
        return tmpfs_read(fd, buf); // 接口转换
    }
};

// ---------------------- 上层用户态进程：仅依赖VFS统一接口 ----------------------
void user_read_file(file_operations *fops, const char *path) {
    printf("\n[用户态进程] 读取文件：%s\n", path);
    int fd = fops->open(path);
    char buf[32] = {0};
    fops->read(fd, buf);
    printf("[用户态进程] 文件内容：%s\n", buf);
}

// 主函数测试
int main() {
    // 读取ext4文件（通过VFS适配器）
    user_read_file(&ext4_fops, "/ext4/test.txt");
    
    // 读取tmpfs文件（通过VFS适配器，上层逻辑无需修改）
    user_read_file(&tmpfs_fops, "/tmpfs/test.txt");
    
    return 0;
}