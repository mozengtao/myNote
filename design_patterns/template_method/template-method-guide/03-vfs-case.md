# Case 1: VFS File Operations

## Subsystem Context

```
+=============================================================================+
|                         VFS ARCHITECTURE                                     |
+=============================================================================+

                           USER SPACE
    +----------------------------------------------------------+
    |   Application:  read(fd, buf, count)                     |
    +----------------------------------------------------------+
                              |
                              | System Call
                              v
    +----------------------------------------------------------+
    |                    SYSTEM CALL LAYER                      |
    |                    sys_read()                             |
    +----------------------------------------------------------+
                              |
                              v
    +----------------------------------------------------------+
    |                      VFS CORE                             |
    |  +--------------------------------------------------+    |
    |  |  vfs_read()  <-- TEMPLATE METHOD                 |    |
    |  |                                                  |    |
    |  |  1. Validate file                                |    |
    |  |  2. Check permissions                            |    |
    |  |  3. Security hook                                |    |
    |  |  4. CALL f_op->read() --------------------+      |    |
    |  |  5. Update access time                    |      |    |
    |  |  6. fsnotify                              |      |    |
    |  +-------------------------------------------|------+    |
    +------------------------------------------------|----------+
                                                     |
                                                     v
    +----------------------------------------------------------+
    |                   FILESYSTEM LAYER                        |
    |  +----------------+  +----------------+  +---------------+|
    |  |    ext4        |  |     NFS        |  |    tmpfs      ||
    |  | ext4_file_read |  | nfs_file_read  |  | shmem_read    ||
    |  +----------------+  +----------------+  +---------------+|
    +----------------------------------------------------------+
```

**中文说明：**

VFS（虚拟文件系统）是Linux内核中文件操作的抽象层。用户空间的`read()`系统调用经过系统调用层到达VFS核心的`vfs_read()`函数。`vfs_read()`是模板方法：它执行验证、权限检查、安全钩子，然后调用具体文件系统的`read`实现，最后更新访问时间和发送文件通知。不同文件系统（ext4、NFS、tmpfs等）只需实现自己的读取逻辑。

---

## The Template Method: vfs_read()

### Components

| Component | Role |
|-----------|------|
| **Template Method** | `vfs_read()` |
| **Fixed Steps** | Permission check, security hook, access time update, fsnotify |
| **Variation Point** | `file->f_op->read()` or `file->f_op->aio_read()` |
| **Ops Table** | `struct file_operations` |

### Control Flow Diagram

```
    vfs_read(file, buf, count, pos)
    ================================

    +----------------------------------+
    |  1. VALIDATE FILE                |
    |     - Check file is valid        |
    |     - Check read mode (FMODE_READ)|
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  2. CHECK PERMISSIONS            |
    |     - rw_verify_area()           |
    |     - Verify access range        |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  3. SECURITY HOOK                |
    |     - security_file_permission() |
    |     - LSM (SELinux, etc.) check  |
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  4. VARIATION POINT                    ||
    ||     if (f_op->read)                    ||
    ||         ret = f_op->read(file,buf,...) ||
    ||     else if (f_op->aio_read)           ||
    ||         ret = do_sync_read(file,...)   ||
    +==========================================+
                   |
                   v
    +----------------------------------+
    |  5. UPDATE STATISTICS            |
    |     - fsnotify_access(file)      |
    |     - inc_syscr(current)         |
    +----------------------------------+
                   |
                   v
              return ret
```

**中文说明：**

`vfs_read()`的控制流分为五个步骤：(1) 验证文件有效性和读取模式；(2) 检查权限和访问范围；(3) 调用安全模块钩子（如SELinux）；(4) **变化点**——调用具体文件系统的read实现；(5) 更新统计信息和发送fsnotify通知。文件系统只实现第4步，其他步骤由VFS框架强制执行。

---

## Why Template Method is Required Here

### 1. Security Cannot Be Optional

```
    WITHOUT TEMPLATE METHOD (DANGEROUS):

    /* Each filesystem implements own read */
    ssize_t ext4_read(file, buf, count) {
        // Oops, forgot security check!
        return do_read(file, buf, count);
    }

    ssize_t nfs_read(file, buf, count) {
        // Different security implementation?
        my_security_check();
        return do_read(file, buf, count);
    }

    PROBLEMS:
    - Inconsistent security enforcement
    - Some filesystems might skip checks
    - Cannot add new security hooks easily
```

### 2. Audit Must Be Centralized

```
    WITH TEMPLATE METHOD:

    vfs_read() {
        // ALL reads go through here
        audit_log_read(file);        // <-- Single audit point

        ret = f_op->read(file, ...);

        fsnotify_access(file);       // <-- Single notification point
    }

    BENEFITS:
    - One place to audit all file reads
    - Cannot be bypassed by filesystems
    - Easy to add monitoring
```

### 3. Access Time Updates Must Be Consistent

```
    vfs_read() {
        ret = f_op->read(...);

        // Always update access time
        // Filesystem cannot forget this
        if (ret > 0)
            file_accessed(file);
    }
```

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL VFS TEMPLATE METHOD SIMULATION
 * 
 * This demonstrates the Template Method pattern in VFS,
 * not actual kernel code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct file;

/* ==========================================================
 * OPS TABLE: file_operations
 * Each filesystem provides its own implementation
 * ========================================================== */
struct file_operations {
    ssize_t (*read)(struct file *file, char *buf, size_t count);
    ssize_t (*write)(struct file *file, const char *buf, size_t count);
    int (*open)(struct file *file);
    int (*release)(struct file *file);
};

/* ==========================================================
 * FILE STRUCTURE
 * Contains pointer to filesystem-specific operations
 * ========================================================== */
struct file {
    const char *name;
    int mode;                              /* FMODE_READ, FMODE_WRITE */
    void *private_data;                    /* Filesystem-specific data */
    const struct file_operations *f_op;    /* Operations table */
};

#define FMODE_READ   0x01
#define FMODE_WRITE  0x02

/* ==========================================================
 * FRAMEWORK FIXED STEPS (VFS Core)
 * These are called by the template, never by filesystems
 * ========================================================== */

static int security_file_permission(struct file *file, int mask)
{
    printf("  [VFS] Security check: file=%s, mask=%d\n", file->name, mask);
    /* In real kernel: LSM hooks (SELinux, AppArmor, etc.) */
    return 0;  /* 0 = permitted */
}

static int rw_verify_area(struct file *file, size_t count)
{
    printf("  [VFS] Verify area: count=%zu\n", count);
    /* In real kernel: check file limits, mandatory locking */
    return 0;  /* 0 = OK */
}

static void fsnotify_access(struct file *file)
{
    printf("  [VFS] fsnotify: access to %s\n", file->name);
    /* In real kernel: notify inotify/fanotify watchers */
}

static void inc_syscr(void)
{
    printf("  [VFS] Statistics: increment syscall read counter\n");
    /* In real kernel: per-task I/O accounting */
}

/* ==========================================================
 * TEMPLATE METHOD: vfs_read()
 * 
 * This is the core of the Template Method pattern.
 * - Fixed steps: validation, security, statistics
 * - Variation point: f_op->read()
 * ========================================================== */
ssize_t vfs_read(struct file *file, char *buf, size_t count)
{
    ssize_t ret;

    printf("[vfs_read] TEMPLATE METHOD START\n");

    /* ========== FIXED STEP 1: Validate file ========== */
    if (!file) {
        printf("  [VFS] ERROR: null file\n");
        return -1;  /* -EBADF */
    }
    printf("  [VFS] File validated: %s\n", file->name);

    /* ========== FIXED STEP 2: Check read permission ========== */
    if (!(file->mode & FMODE_READ)) {
        printf("  [VFS] ERROR: file not opened for reading\n");
        return -1;  /* -EBADF */
    }
    printf("  [VFS] Read mode OK\n");

    /* ========== FIXED STEP 3: Verify access area ========== */
    ret = rw_verify_area(file, count);
    if (ret < 0) {
        printf("  [VFS] ERROR: area verification failed\n");
        return ret;
    }

    /* ========== FIXED STEP 4: Security check ========== */
    ret = security_file_permission(file, FMODE_READ);
    if (ret < 0) {
        printf("  [VFS] ERROR: security check failed\n");
        return ret;
    }

    /* ========== VARIATION POINT: Call filesystem read ========== */
    printf("  [VFS] >>> Calling filesystem-specific read\n");
    if (file->f_op && file->f_op->read) {
        ret = file->f_op->read(file, buf, count);
    } else {
        printf("  [VFS] ERROR: no read operation\n");
        return -1;  /* -EINVAL */
    }
    printf("  [VFS] <<< Filesystem read returned: %zd\n", ret);

    /* ========== FIXED STEP 5: Post-read processing ========== */
    if (ret > 0) {
        fsnotify_access(file);
        inc_syscr();
    }

    printf("[vfs_read] TEMPLATE METHOD END, ret=%zd\n\n", ret);
    return ret;
}

/* ==========================================================
 * FILESYSTEM IMPLEMENTATIONS (Variation Points)
 * These only implement the actual read logic
 * ========================================================== */

/* --- EXT4-like filesystem implementation --- */
static ssize_t ext4_file_read(struct file *file, char *buf, size_t count)
{
    printf("    [ext4] Reading from disk storage\n");
    printf("    [ext4] Checking extent tree...\n");
    printf("    [ext4] Reading data blocks...\n");
    
    /* Simulate reading data */
    const char *data = "ext4 file content";
    size_t len = strlen(data);
    if (count > len) count = len;
    memcpy(buf, data, count);
    
    return count;
}

static const struct file_operations ext4_file_ops = {
    .read = ext4_file_read,
    .write = NULL,  /* Not implemented for this example */
    .open = NULL,
    .release = NULL,
};

/* --- NFS-like filesystem implementation --- */
static ssize_t nfs_file_read(struct file *file, char *buf, size_t count)
{
    printf("    [nfs] Sending READ RPC to server\n");
    printf("    [nfs] Waiting for network response...\n");
    printf("    [nfs] Received data from server\n");
    
    /* Simulate reading data */
    const char *data = "nfs remote content";
    size_t len = strlen(data);
    if (count > len) count = len;
    memcpy(buf, data, count);
    
    return count;
}

static const struct file_operations nfs_file_ops = {
    .read = nfs_file_read,
    .write = NULL,
    .open = NULL,
    .release = NULL,
};

/* --- tmpfs-like filesystem implementation --- */
static ssize_t tmpfs_file_read(struct file *file, char *buf, size_t count)
{
    printf("    [tmpfs] Reading from page cache\n");
    printf("    [tmpfs] No disk I/O needed\n");
    
    /* Simulate reading data */
    const char *data = "tmpfs memory content";
    size_t len = strlen(data);
    if (count > len) count = len;
    memcpy(buf, data, count);
    
    return count;
}

static const struct file_operations tmpfs_file_ops = {
    .read = tmpfs_file_read,
    .write = NULL,
    .open = NULL,
    .release = NULL,
};

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    char buf[256];
    
    printf("==============================================\n");
    printf("VFS TEMPLATE METHOD DEMONSTRATION\n");
    printf("==============================================\n\n");

    /* Create file objects with different filesystem backends */
    struct file ext4_file = {
        .name = "/data/document.txt",
        .mode = FMODE_READ,
        .f_op = &ext4_file_ops,
    };

    struct file nfs_file = {
        .name = "/mnt/server/remote.txt",
        .mode = FMODE_READ,
        .f_op = &nfs_file_ops,
    };

    struct file tmpfs_file = {
        .name = "/tmp/scratch.txt",
        .mode = FMODE_READ,
        .f_op = &tmpfs_file_ops,
    };

    /* 
     * All three use the SAME vfs_read() template method.
     * Only the filesystem-specific read differs.
     */
    printf("--- Reading from ext4 filesystem ---\n");
    vfs_read(&ext4_file, buf, sizeof(buf));

    printf("--- Reading from NFS filesystem ---\n");
    vfs_read(&nfs_file, buf, sizeof(buf));

    printf("--- Reading from tmpfs filesystem ---\n");
    vfs_read(&tmpfs_file, buf, sizeof(buf));

    /* Demonstrate security check working */
    printf("--- Attempting read on write-only file ---\n");
    struct file writeonly_file = {
        .name = "/dev/null",
        .mode = FMODE_WRITE,  /* No read permission */
        .f_op = &ext4_file_ops,
    };
    vfs_read(&writeonly_file, buf, sizeof(buf));

    return 0;
}
```

### Expected Output

```
==============================================
VFS TEMPLATE METHOD DEMONSTRATION
==============================================

--- Reading from ext4 filesystem ---
[vfs_read] TEMPLATE METHOD START
  [VFS] File validated: /data/document.txt
  [VFS] Read mode OK
  [VFS] Verify area: count=256
  [VFS] Security check: file=/data/document.txt, mask=1
  [VFS] >>> Calling filesystem-specific read
    [ext4] Reading from disk storage
    [ext4] Checking extent tree...
    [ext4] Reading data blocks...
  [VFS] <<< Filesystem read returned: 17
  [VFS] fsnotify: access to /data/document.txt
  [VFS] Statistics: increment syscall read counter
[vfs_read] TEMPLATE METHOD END, ret=17

--- Reading from NFS filesystem ---
[vfs_read] TEMPLATE METHOD START
  [VFS] File validated: /mnt/server/remote.txt
  [VFS] Read mode OK
  [VFS] Verify area: count=256
  [VFS] Security check: file=/mnt/server/remote.txt, mask=1
  [VFS] >>> Calling filesystem-specific read
    [nfs] Sending READ RPC to server
    [nfs] Waiting for network response...
    [nfs] Received data from server
  [VFS] <<< Filesystem read returned: 18
  [VFS] fsnotify: access to /mnt/server/remote.txt
  [VFS] Statistics: increment syscall read counter
[vfs_read] TEMPLATE METHOD END, ret=18

...
```

---

## What the Implementation is NOT Allowed to Do

```
+=============================================================================+
|              FILESYSTEM IMPLEMENTATION RESTRICTIONS                          |
+=============================================================================+

    FILESYSTEM CANNOT:

    1. SKIP SECURITY CHECKS
       ext4_file_read() cannot bypass security_file_permission()
       The check happens BEFORE ext4 code runs

    2. CONTROL WHEN IT RUNS
       ext4_file_read() is CALLED, it does not decide when to run
       VFS controls scheduling and ordering

    3. MODIFY RETURN SEMANTICS
       Return value goes through VFS post-processing
       Cannot return "magic" values to skip fsnotify

    4. ACCESS OTHER FILES' DATA
       Only receives its own file pointer
       Cannot read from other files' inodes

    5. CHANGE LOCKING PROTOCOL
       VFS may hold i_mutex during the call
       Filesystem cannot release it early

    6. SKIP ACCOUNTING
       inc_syscr() happens regardless of filesystem behavior
       I/O statistics are always updated

    +-----------------------------------------------------------------+
    |  THE FILESYSTEM IS A GUEST IN VFS'S EXECUTION CONTEXT           |
    |  IT CAN ONLY DO WHAT VFS EXPLICITLY ALLOWS                      |
    +-----------------------------------------------------------------+
```

**中文说明：**

文件系统实现的限制：(1) 不能跳过安全检查——检查在文件系统代码运行前发生；(2) 不能控制何时运行——由VFS决定调度；(3) 不能修改返回语义——返回值经过VFS后处理；(4) 不能访问其他文件数据——只接收自己的file指针；(5) 不能改变锁协议——VFS可能在调用期间持有i_mutex；(6) 不能跳过统计——I/O统计始终更新。文件系统是VFS执行上下文中的客人，只能做VFS明确允许的事情。

---

## Real Kernel Code Reference (v3.2)

### vfs_read() in fs/read_write.c

```c
/* Simplified from actual kernel code */
ssize_t vfs_read(struct file *file, char __user *buf, 
                 size_t count, loff_t *pos)
{
    ssize_t ret;

    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
        return -EFAULT;

    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }
    return ret;
}
```

### struct file_operations in include/linux/fs.h

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, 
                         unsigned long, loff_t);
    /* ... many more operations ... */
};
```

---

## Key Takeaways

1. **VFS owns the read path**: All file reads go through `vfs_read()`
2. **Security is mandatory**: Filesystem code runs only after security checks pass
3. **Statistics are automatic**: Filesystems cannot skip accounting
4. **Notification is centralized**: fsnotify sees all file access
5. **Filesystems are pluggable**: Any filesystem works with the same template
