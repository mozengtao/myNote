/*
场景说明
内核挂载文件系统时，根据挂载参数（如-t ext4）调用对应文件系统的工厂函数，创建 ext4/tmpfs/ramfs 等文件系统实例，封装了文件系统的初始化逻辑。

核心说明
内核中vfs_kern_mount是文件系统工厂入口，根据fs_name匹配file_system_type结构体，调用mount函数创建文件系统实例；
新增文件系统时，只需实现file_system_type并注册，无需修改挂载核心逻辑，是工厂模式 “扩展友好” 的典型体现。
*/

#include <stdio.h>
#include <string.h>

// 1. 抽象产品：文件系统接口
typedef struct {
    const char *name;        // 文件系统名（ext4/tmpfs）
    void (*mount)(void);     // 挂载逻辑
    void (*umount)(void);    // 卸载逻辑
} file_system_type;

// 2. 具体产品1：ext4文件系统
static file_system_type ext4_fs = {
    .name = "ext4",
    .mount = (void)() { printf("[ext4] 挂载：解析超级块，初始化inode表\n"); },
    .umount = (void)() { printf("[ext4] 卸载：同步缓存，释放inode\n"); }
};

// 3. 具体产品2：tmpfs内存文件系统
static file_system_type tmpfs_fs = {
    .name = "tmpfs",
    .mount = (void)() { printf("[tmpfs] 挂载：分配内存页，初始化内存链表\n"); },
    .umount = (void)() { printf("[tmpfs] 卸载：释放内存页\n"); }
};

// 4. 工厂函数：根据文件系统名创建实例
file_system_type* fs_factory(const char *fs_name) {
    if (strcmp(fs_name, "ext4") == 0) {
        printf("文件系统工厂：创建ext4实例\n");
        return &ext4_fs;
    } else if (strcmp(fs_name, "tmpfs") == 0) {
        printf("文件系统工厂：创建tmpfs实例\n");
        return &tmpfs_fs;
    }
    return NULL;
}

// 测试：模拟mount命令（mount -t ext4 /dev/sda1 /mnt）
int main() {
    // 挂载ext4
    file_system_type *ext4 = fs_factory("ext4");
    if (ext4) ext4->mount();
    
    // 挂载tmpfs
    file_system_type *tmpfs = fs_factory("tmpfs");
    if (tmpfs) tmpfs->mount();
    return 0;
}