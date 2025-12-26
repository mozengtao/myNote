# VFS Architecture Study: Registration and Plug-in Architecture

## 1. The Registration Pattern

```
+------------------------------------------------------------------+
|  PLUGIN ARCHITECTURE VIA REGISTRATION                            |
+------------------------------------------------------------------+

    Traditional (Bad) Approach:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* VFS core must know about all filesystems */            │
    │  switch (fs_type) {                                         │
    │      case FS_EXT4: return ext4_mount(...);                 │
    │      case FS_NFS:  return nfs_mount(...);                  │
    │      case FS_PROC: return proc_mount(...);                 │
    │      // Adding new FS requires modifying this switch       │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    VFS (Good) Approach:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* Filesystems register themselves */                     │
    │  register_filesystem(&ext4_fs_type);  // ext4 module init  │
    │  register_filesystem(&nfs_fs_type);   // nfs module init   │
    │  register_filesystem(&proc_fs_type);  // proc init         │
    │                                                              │
    │  /* VFS looks up by name at mount time */                  │
    │  fs_type = get_fs_type("ext4");                            │
    │  fs_type->mount(...)                                        │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 传统方式：核心代码必须知道所有实现，添加新实现需要修改核心
- VFS 方式：实现自行注册，核心代码通过名称查找，完全解耦

---

## 2. struct file_system_type

```c
/* include/linux/fs.h lines 1859-1876 */

struct file_system_type {
    const char *name;           /* "ext4", "nfs", "proc", etc. */
    int fs_flags;               /* FS_REQUIRES_DEV, etc. */
    
    /* THE KEY: Mount function pointer */
    struct dentry *(*mount) (struct file_system_type *, int,
                             const char *, void *);
    
    void (*kill_sb) (struct super_block *);  /* Cleanup */
    
    struct module *owner;       /* Module owning this type */
    struct file_system_type *next;  /* Linked list of all types */
    struct list_head fs_supers; /* All superblocks of this type */
    
    /* Lock debugging keys */
    struct lock_class_key s_lock_key;
    struct lock_class_key s_umount_key;
    /* ... */
};
```

### Example: ext4 Filesystem Type

```c
/* fs/ext4/super.c */

static struct dentry *ext4_mount(struct file_system_type *fs_type,
                                 int flags, const char *dev_name, void *data)
{
    return mount_bdev(fs_type, flags, dev_name, data, ext4_fill_super);
}

static struct file_system_type ext4_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "ext4",
    .mount      = ext4_mount,
    .kill_sb    = kill_block_super,
    .fs_flags   = FS_REQUIRES_DEV,  /* Needs block device */
};

/* Module initialization */
static int __init ext4_init_fs(void)
{
    /* ... allocate caches, init journal ... */
    
    err = register_filesystem(&ext4_fs_type);
    if (err)
        goto out_unregister;
    
    return 0;
}
module_init(ext4_init_fs);
```

---

## 3. Registration Mechanism

```c
/* fs/filesystems.c */

/* Global linked list of filesystem types */
static struct file_system_type *file_systems;
static DEFINE_RWLOCK(file_systems_lock);

/**
 * register_filesystem - Register a filesystem with the kernel
 */
int register_filesystem(struct file_system_type *fs)
{
    int res = 0;
    struct file_system_type **p;

    /* Sanity check: no dots in name */
    BUG_ON(strchr(fs->name, '.'));
    
    /* Already registered? */
    if (fs->next)
        return -EBUSY;
    
    /* Initialize superblock list */
    INIT_LIST_HEAD(&fs->fs_supers);
    
    write_lock(&file_systems_lock);
    
    /* Find end of list (or duplicate) */
    p = find_filesystem(fs->name, strlen(fs->name));
    if (*p)
        res = -EBUSY;  /* Name already taken */
    else
        *p = fs;       /* Add to list */
    
    write_unlock(&file_systems_lock);
    return res;
}
EXPORT_SYMBOL(register_filesystem);

/**
 * unregister_filesystem - Remove a filesystem
 */
int unregister_filesystem(struct file_system_type *fs)
{
    struct file_system_type **tmp;

    write_lock(&file_systems_lock);
    tmp = &file_systems;
    while (*tmp) {
        if (fs == *tmp) {
            *tmp = fs->next;
            fs->next = NULL;
            write_unlock(&file_systems_lock);
            synchronize_rcu();  /* Wait for readers */
            return 0;
        }
        tmp = &(*tmp)->next;
    }
    write_unlock(&file_systems_lock);
    return -EINVAL;
}
EXPORT_SYMBOL(unregister_filesystem);
```

---

## 4. Mount Flow

```
+------------------------------------------------------------------+
|  MOUNT FLOW: mount -t ext4 /dev/sda1 /mnt                       |
+------------------------------------------------------------------+

    User space: mount("ext4", "/dev/sda1", "/mnt", ...)
                              │
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  sys_mount()                                                │
    │  └── do_mount()                                            │
    │      └── do_new_mount()                                    │
    │          ├── get_fs_type("ext4")  ← Find registered type  │
    │          │       │                                          │
    │          │       ▼                                          │
    │          │   Lookup in file_systems linked list            │
    │          │   Return &ext4_fs_type                          │
    │          │                                                   │
    │          └── vfs_kern_mount(type, flags, dev, data)        │
    │              └── mount_fs(type, ...)                       │
    │                  └── type->mount(...)  ← ext4_mount()     │
    │                      │                                      │
    │                      ▼                                      │
    │                  mount_bdev()  ← For block device FS       │
    │                      └── ext4_fill_super()                 │
    │                          │                                  │
    │                          ▼                                  │
    │                      Create super_block, read from disk    │
    │                      Initialize ext4-specific structures   │
    │                      Return root dentry                     │
    └─────────────────────────────────────────────────────────────┘
```

### Code Path Detail

```c
/* fs/namespace.c */
static int do_new_mount(struct path *path, char *type, int flags,
                        int mnt_flags, char *name, void *data)
{
    struct file_system_type *fs_type;
    struct vfsmount *mnt;
    
    /* LOOKUP: Find filesystem type by name */
    fs_type = get_fs_type(type);
    if (!fs_type)
        return -ENODEV;  /* Unknown filesystem */
    
    /* MOUNT: Call filesystem's mount function */
    mnt = vfs_kern_mount(fs_type, flags, name, data);
    
    put_filesystem(fs_type);
    
    if (IS_ERR(mnt))
        return PTR_ERR(mnt);
    
    /* Attach to mount point */
    return do_add_mount(mnt, path, mnt_flags);
}

/* Get filesystem type by name */
struct file_system_type *get_fs_type(const char *name)
{
    struct file_system_type *fs;
    
    read_lock(&file_systems_lock);
    for (fs = file_systems; fs; fs = fs->next) {
        if (strcmp(fs->name, name) == 0) {
            if (try_module_get(fs->owner)) {
                read_unlock(&file_systems_lock);
                return fs;
            }
        }
    }
    read_unlock(&file_systems_lock);
    
    /* Try to load module (request_module("fs-xxx")) */
    return NULL;
}
```

---

## 5. Why Registration Scales

```
+------------------------------------------------------------------+
|  SCALABILITY OF REGISTRATION                                     |
+------------------------------------------------------------------+

    1. NO CORE CHANGES NEEDED
    ┌─────────────────────────────────────────────────────────────┐
    │  Adding ext5 requires:                                      │
    │  ✓ Write fs/ext5/ code                                     │
    │  ✓ Call register_filesystem(&ext5_fs_type)                 │
    │  ✗ No changes to fs/namespace.c                            │
    │  ✗ No changes to fs/super.c                                │
    │  ✗ No changes to any VFS file                              │
    └─────────────────────────────────────────────────────────────┘

    2. COMPILE-TIME INDEPENDENCE
    ┌─────────────────────────────────────────────────────────────┐
    │  VFS compiles without any filesystem:                      │
    │  $ make fs/                                                 │
    │  # Produces VFS object files                               │
    │  # No dependency on fs/ext4/, fs/nfs/, etc.               │
    └─────────────────────────────────────────────────────────────┘

    3. RUNTIME LOADING
    ┌─────────────────────────────────────────────────────────────┐
    │  Filesystems can be loaded as modules:                     │
    │  $ modprobe ext4     # Calls module_init → register_fs    │
    │  $ mount -t ext4 ... # Now works                          │
    │  $ rmmod ext4        # Calls module_exit → unregister_fs  │
    └─────────────────────────────────────────────────────────────┘

    4. NAME-BASED LOOKUP
    ┌─────────────────────────────────────────────────────────────┐
    │  mount -t ext4 /dev/sda1 /mnt                              │
    │          └─── String lookup, no compiled-in constants      │
    │                                                              │
    │  User space doesn't need to know FS implementation details │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Similar Patterns in Other Subsystems

```
+------------------------------------------------------------------+
|  REGISTRATION PATTERN ACROSS KERNEL                              |
+------------------------------------------------------------------+

    DEVICE DRIVERS:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct pci_driver {                                        │
    │      const char *name;                                      │
    │      const struct pci_device_id *id_table;                 │
    │      int (*probe)(struct pci_dev *, const struct pci_device_id *);
    │      void (*remove)(struct pci_dev *);                     │
    │  };                                                         │
    │                                                              │
    │  pci_register_driver(&my_driver);                          │
    └─────────────────────────────────────────────────────────────┘

    NETWORK PROTOCOLS:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct proto_ops {                                         │
    │      int family;                                            │
    │      int (*bind)(struct socket *, ...);                    │
    │      int (*connect)(struct socket *, ...);                 │
    │  };                                                         │
    │                                                              │
    │  sock_register(&my_family_ops);                            │
    └─────────────────────────────────────────────────────────────┘

    INPUT DEVICES:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct input_handler {                                     │
    │      const char *name;                                      │
    │      int (*connect)(struct input_handler *, ...);          │
    │      void (*disconnect)(struct input_handle *);            │
    │  };                                                         │
    │                                                              │
    │  input_register_handler(&my_handler);                      │
    └─────────────────────────────────────────────────────────────┘

    PATTERN:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Define struct with name + function pointers            │
    │  2. Provide register/unregister functions                  │
    │  3. Core maintains linked list                             │
    │  4. Core looks up by name or ID                            │
    │  5. Core dispatches via function pointers                  │
    └─────────────────────────────────────────────────────────────┘
```

---

## 7. Module Lifecycle

```
+------------------------------------------------------------------+
|  FILESYSTEM MODULE LIFECYCLE                                     |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │  modprobe ext4                                              │
    │      │                                                       │
    │      ▼                                                       │
    │  ext4_init_fs()                                             │
    │  ├── kmem_cache_create("ext4_inode_cache", ...)            │
    │  ├── kmem_cache_create("ext4_free_data", ...)              │
    │  ├── ext4_init_mballoc()                                   │
    │  ├── ext4_init_xattr()                                      │
    │  ├── jbd2_journal_init()                                   │
    │  └── register_filesystem(&ext4_fs_type)  ← REGISTER       │
    │                                                              │
    │  [ext4 is now available for mount]                         │
    │                                                              │
    │  rmmod ext4                                                 │
    │      │                                                       │
    │      ▼                                                       │
    │  ext4_exit_fs()                                             │
    │  ├── unregister_filesystem(&ext4_fs_type)  ← UNREGISTER   │
    │  ├── ext4_exit_xattr()                                      │
    │  ├── ext4_exit_mballoc()                                   │
    │  └── kmem_cache_destroy(...)                               │
    └─────────────────────────────────────────────────────────────┘

    SAFETY: Cannot unload if any mount exists
    ┌─────────────────────────────────────────────────────────────┐
    │  try_module_get(fs->owner)  — Called during mount          │
    │  module_put(fs->owner)      — Called during unmount        │
    │                                                              │
    │  rmmod fails if refcount > 0                               │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  REGISTRATION PATTERN SUMMARY                                    |
+------------------------------------------------------------------+

    COMPONENTS:
    • file_system_type: Name + mount function + cleanup
    • register_filesystem(): Add to global list
    • get_fs_type(): Lookup by name
    • unregister_filesystem(): Remove from list

    BENEFITS:
    • Zero VFS changes for new filesystems
    • Compile-time independence
    • Runtime loading/unloading
    • Name-based extensibility

    SIMILAR PATTERNS:
    • PCI drivers: pci_register_driver
    • Network: sock_register
    • Input: input_register_handler
    • Block: register_blkdev

    CORE IDEA:
    ┌─────────────────────────────────────────────────────────────┐
    │  Core code defines interface (struct with function ptrs)   │
    │  Plugins implement interface and register at init time     │
    │  Core looks up by name/id and dispatches via pointers     │
    │  No switch statements, no hardcoded dependencies           │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **注册机制**：`file_system_type` + `register_filesystem()` + 名称查找
- **好处**：添加新文件系统无需修改 VFS，编译时独立，运行时加载
- **相同模式**：PCI 驱动、网络协议、输入设备都使用注册模式
- **核心思想**：定义接口，插件注册，名称查找，函数指针分发

