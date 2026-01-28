# Case 2: Filesystem Registration

Filesystem registration allows the VFS to support multiple filesystem types.

---

## Subsystem Context

```
+=============================================================================+
|                    FILESYSTEM REGISTRATION                                   |
+=============================================================================+

    VFS ROLE:
    =========

    - Maintains list of filesystem types
    - Handles mount requests
    - Routes operations to correct filesystem
    - Provides common interface


    REGISTRATION FLOW:
    ==================

    1. Filesystem module loads
           |
           v
    2. register_filesystem(&my_fs_type)
           |
           v
    3. Added to file_systems list
           |
           v
    4. mount("", "/mnt", "myfs", ...)
           |
           v
    5. VFS finds "myfs" in list
           |
           v
    6. Calls my_fs_type.mount()


    FILESYSTEM TYPE STRUCTURE:
    ==========================

    struct file_system_type {
        const char *name;        /* "ext4", "nfs", etc. */
        int fs_flags;
        struct dentry *(*mount)(...);
        void (*kill_sb)(sb);
        struct module *owner;
        /* ... */
    };
```

**中文说明：**

文件系统注册：VFS维护文件系统类型列表，处理挂载请求，将操作路由到正确的文件系统。文件系统模块加载时调用register_filesystem，挂载时VFS在列表中查找并调用mount回调。

---

## Key Structures

```c
/* Filesystem type - one per filesystem */
struct file_system_type {
    const char *name;
    int fs_flags;
    struct dentry *(*mount)(struct file_system_type *, int,
                            const char *, void *);
    void (*kill_sb)(struct super_block *);
    struct module *owner;
    struct file_system_type *next;
    /* ... */
};

/* Example: ext4 */
static struct file_system_type ext4_fs_type = {
    .name       = "ext4",
    .mount      = ext4_mount,
    .kill_sb    = kill_block_super,
    .fs_flags   = FS_REQUIRES_DEV,
    .owner      = THIS_MODULE,
};
```

---

## Minimal Simulation

```c
/* Simplified filesystem registration */

#include <stdio.h>
#include <string.h>

struct file_system_type {
    const char *name;
    int (*mount)(const char *dev, const char *dir);
    void (*umount)(const char *dir);
    struct file_system_type *next;
};

/* Global filesystem list */
static struct file_system_type *file_systems = NULL;

/* Register filesystem */
int register_filesystem(struct file_system_type *fs)
{
    printf("[VFS] Registering filesystem: %s\n", fs->name);
    fs->next = file_systems;
    file_systems = fs;
    return 0;
}

/* Unregister filesystem */
int unregister_filesystem(struct file_system_type *fs)
{
    struct file_system_type **p;
    
    printf("[VFS] Unregistering filesystem: %s\n", fs->name);
    
    for (p = &file_systems; *p; p = &(*p)->next) {
        if (*p == fs) {
            *p = fs->next;
            return 0;
        }
    }
    return -1;
}

/* Find filesystem by name */
struct file_system_type *get_fs_type(const char *name)
{
    struct file_system_type *fs;
    
    for (fs = file_systems; fs; fs = fs->next) {
        if (strcmp(fs->name, name) == 0)
            return fs;
    }
    return NULL;
}

/* Mount syscall (simplified) */
int do_mount(const char *dev, const char *dir, const char *type)
{
    struct file_system_type *fs;
    
    printf("[VFS] mount(%s, %s, %s)\n", dev, dir, type);
    
    fs = get_fs_type(type);
    if (!fs) {
        printf("[VFS] Unknown filesystem: %s\n", type);
        return -1;
    }
    
    return fs->mount(dev, dir);
}

/* ====== EXAMPLE FILESYSTEM ====== */

int myfs_mount(const char *dev, const char *dir)
{
    printf("  [MYFS] Mounting %s on %s\n", dev, dir);
    printf("  [MYFS] Reading superblock...\n");
    printf("  [MYFS] Mount complete\n");
    return 0;
}

void myfs_umount(const char *dir)
{
    printf("  [MYFS] Unmounting %s\n", dir);
}

static struct file_system_type myfs_type = {
    .name = "myfs",
    .mount = myfs_mount,
    .umount = myfs_umount,
};

int main(void)
{
    printf("=== FILESYSTEM REGISTRATION ===\n\n");
    
    /* Module loads */
    register_filesystem(&myfs_type);
    
    /* User mounts */
    printf("\n");
    do_mount("/dev/sda1", "/mnt", "myfs");
    
    printf("\n");
    do_mount("/dev/sdb1", "/data", "unknownfs");
    
    /* Module unloads */
    printf("\n");
    unregister_filesystem(&myfs_type);
    
    return 0;
}
```

---

## What Core Does NOT Control

```
    VFS Controls:
    -------------
    [X] Filesystem list
    [X] Mount/umount syscalls
    [X] Finding filesystem by name

    Filesystem Controls:
    -------------------
    [X] How to mount (superblock)
    [X] How to read/write
    [X] On-disk format
```

---

## Version

Based on **Linux kernel v3.2** fs/filesystems.c.
