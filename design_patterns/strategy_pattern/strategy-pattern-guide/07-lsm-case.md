# Case 5: Linux Security Modules (LSM) - Policy Strategy

## Subsystem Background

```
+=============================================================================+
|                    LSM ARCHITECTURE                                          |
+=============================================================================+

                          KERNEL CORE
                          ===========

    +------------------------------------------------------------------+
    |                     Various subsystems                            |
    |                                                                   |
    |   MECHANISM (Fixed):                                              |
    |   - File open/read/write (VFS)                                    |
    |   - Process creation (fork/exec)                                  |
    |   - Network operations (sockets)                                  |
    |   - IPC operations                                                |
    |                                                                   |
    |   At each sensitive operation:                                    |
    |   +----------------------------------------------------+         |
    |   | if (security_xxx() != 0)                           |         |
    |   |     return -EACCES;                                |         |
    |   +----------------------------------------------------+         |
    |                                                                   |
    +------------------------------------------------------------------+
                                |
                                | security_xxx() delegates to
                                v
    +------------------------------------------------------------------+
    |                   SECURITY MODULE STRATEGIES                      |
    |                   (Strategy Pattern)                              |
    |                                                                   |
    |   +------------------+  +------------------+  +------------------+|
    |   |    SELinux       |  |    AppArmor      |  |     SMACK        ||
    |   | (MAC - labels)   |  | (path-based)     |  | (simple MAC)     ||
    |   +------------------+  +------------------+  +------------------+|
    |                                                                   |
    |   +------------------+  +------------------+                      |
    |   |     TOMOYO       |  |    Yama          |                      |
    |   | (path-based MAC) |  | (ptrace control) |                      |
    |   +------------------+  +------------------+                      |
    |                                                                   |
    +------------------------------------------------------------------+

    KEY INSIGHT:
    - Kernel core provides MECHANISM (access control hooks)
    - Security module provides POLICY (allow/deny decisions)
```

**中文说明：**

LSM架构：内核核心在各种敏感操作点（文件打开、进程创建、网络操作、IPC等）放置安全钩子调用。如果`security_xxx()`返回非零，操作被拒绝。核心将决策委托给安全模块：SELinux（基于标签的强制访问控制）、AppArmor（基于路径）、SMACK（简单MAC）、TOMOYO（基于路径的MAC）、Yama（ptrace控制）。关键洞察：内核核心提供机制（访问控制钩子），安全模块提供策略（允许/拒绝决策）。

---

## The Strategy Interface: struct security_operations

### Components

| Component | Role |
|-----------|------|
| **Strategy Interface** | `struct security_operations` |
| **Replaceable Algorithm** | SELinux, AppArmor, SMACK, etc. |
| **Selection Mechanism** | Boot-time (single primary module) |

### The Interface (Partial)

```c
struct security_operations {
    char name[SECURITY_NAME_MAX + 1];

    /* === FILE OPERATIONS === */
    int (*inode_create)(struct inode *dir, struct dentry *dentry, int mode);
    int (*inode_unlink)(struct inode *dir, struct dentry *dentry);
    int (*inode_permission)(struct inode *inode, int mask);
    int (*file_open)(struct file *file, const struct cred *cred);

    /* === PROCESS OPERATIONS === */
    int (*task_create)(unsigned long clone_flags);
    int (*task_kill)(struct task_struct *p, struct siginfo *info, int sig);
    int (*bprm_check_security)(struct linux_binprm *bprm);

    /* === NETWORK OPERATIONS === */
    int (*socket_create)(int family, int type, int protocol, int kern);
    int (*socket_connect)(struct socket *sock, struct sockaddr *addr, int addrlen);
    int (*socket_sendmsg)(struct socket *sock, struct msghdr *msg, int size);

    /* === IPC OPERATIONS === */
    int (*msg_queue_msgsnd)(struct msg_queue *msq, struct msg_msg *msg);
    int (*shm_shmat)(struct shmid_kernel *shp, char __user *shmaddr, int shmflg);

    /* ... 150+ hooks total ... */
};
```

### Control Flow: How Core Uses Security Strategy

```
    vfs_open() - File Open with Security Check
    ==========================================

    +----------------------------------+
    |  Open file request               |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  VFS: Lookup inode, dentry       |  MECHANISM
    |  (Normal file lookup)            |  (Core)
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  security_inode_permission()           ||  STRATEGY
    ||  -> security_ops->inode_permission()   ||  (Security
    ||  (Can this process access this file?)  ||   decides)
    +==========================================+
                   |
           +-------+-------+
           |               |
           v               v
    +-------------+  +------------------+
    |   ALLOW     |  |      DENY        |
    |   (ret = 0) |  | (ret = -EACCES)  |
    +-------------+  +------------------+
           |               |
           v               v
    +----------------------------------+
    |  Continue with open              |  or return error
    +----------------------------------+


    sys_execve() - Process Execution with Security Check
    ====================================================

    +----------------------------------+
    |  Execute new program             |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  Load binary, setup bprm         |  MECHANISM
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  security_bprm_check()                 ||  STRATEGY
    ||  -> security_ops->bprm_check_security  ||
    ||  (Can this process execute this file?) ||
    +==========================================+
                   |
           +-------+-------+
           |               |
           v               v
       ALLOW            DENY
```

**中文说明：**

`vfs_open()`的控制流：打开文件请求到达，VFS执行正常的inode和dentry查找（机制），然后调用`security_inode_permission()`——安全模块决定是否允许此进程访问此文件。如果返回0则允许继续打开，如果返回-EACCES则拒绝。`sys_execve()`类似：加载二进制文件后调用`security_bprm_check()`让安全模块决定是否允许执行。

---

## Why Strategy is Required Here

### 1. Different Security Models for Different Environments

```
    ENVIRONMENT            BEST SECURITY MODULE
    ===========            ====================

    Government/Military    SELinux
    +-------------------+  - Fine-grained mandatory access control
    | Strict policy     |  - Type enforcement
    | Labeled data      |  - Multi-level security
    +-------------------+

    Desktop/Consumer       AppArmor
    +-------------------+  - Path-based (easier to understand)
    | Simple config     |  - Profile-based confinement
    | Per-app profiles  |  - Less complex than SELinux
    +-------------------+

    Embedded/IoT           SMACK
    +-------------------+  - Simple, lightweight
    | Resource limited  |  - Easy to configure
    | Simple policy     |
    +-------------------+

    Enterprise Server      SELinux or AppArmor
    +-------------------+  - Depends on distro/admin preference
    | Mixed needs       |  - RHEL uses SELinux
    | Compliance        |  - Ubuntu uses AppArmor
    +-------------------+
```

### 2. Policy Should Not Be Hardcoded

```
    WITHOUT LSM (WRONG):
    +-------------------------------------------------------+
    | int vfs_open(file) {                                  |
    |     if (current->uid != file->owner_uid &&            |
    |         !(file->mode & S_IROTH))                      |
    |         return -EACCES;                               |
    |     // Hardcoded policy!                              |
    | }                                                     |
    +-------------------------------------------------------+

    WITH LSM (CORRECT):
    +-------------------------------------------------------+
    | int vfs_open(file) {                                  |
    |     ret = security_file_open(file, cred);             |
    |     if (ret)                                          |
    |         return ret;                                   |
    |     // Security module decides policy                 |
    | }                                                     |
    +-------------------------------------------------------+

    BENEFITS:
    - Policy is configurable without kernel changes
    - Different modules for different needs
    - Policy can be updated at runtime (within module)
```

**中文说明：**

为什么需要策略：(1) 不同环境需要不同安全模型——政府/军事需要SELinux的细粒度强制访问控制、桌面用户需要AppArmor的简单配置、嵌入式设备需要SMACK的轻量级方案。(2) 策略不应硬编码——没有LSM时策略会被硬编码在内核中；有LSM时，核心只调用安全钩子，安全模块决定策略。好处：策略可配置而无需修改内核、不同模块满足不同需求、策略可在运行时更新。

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL LSM STRATEGY PATTERN SIMULATION
 * 
 * Demonstrates how security modules work as strategies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct cred;
struct inode;
struct file;

/* ==========================================================
 * CREDENTIAL STRUCTURE (Simplified)
 * ========================================================== */
struct cred {
    int uid;
    int gid;
    const char *selinux_label;  /* For SELinux */
    const char *apparmor_profile;  /* For AppArmor */
};

/* ==========================================================
 * INODE STRUCTURE (Simplified)
 * ========================================================== */
struct inode {
    int owner_uid;
    int mode;
    const char *selinux_label;  /* For SELinux */
    const char *path;  /* For AppArmor */
};

/* ==========================================================
 * FILE STRUCTURE (Simplified)
 * ========================================================== */
struct file {
    struct inode *inode;
    int flags;
};

/* ==========================================================
 * SECURITY_OPERATIONS: Strategy Interface
 * ========================================================== */
struct security_operations {
    const char *name;
    
    /* File operations */
    int (*inode_permission)(struct inode *inode, int mask, struct cred *cred);
    int (*file_open)(struct file *file, struct cred *cred);
    
    /* Process operations */
    int (*task_create)(struct cred *cred, unsigned long flags);
    int (*bprm_check)(const char *filename, struct cred *cred);
};

/* Permission masks */
#define MAY_READ   0x04
#define MAY_WRITE  0x02
#define MAY_EXEC   0x01

/* ==========================================================
 * SELINUX STRATEGY IMPLEMENTATION
 * Label-based mandatory access control
 * ========================================================== */

static int selinux_inode_permission(struct inode *inode, int mask, 
                                     struct cred *cred)
{
    printf("  [SELinux] Checking permission:\n");
    printf("  [SELinux]   Subject label: %s\n", cred->selinux_label);
    printf("  [SELinux]   Object label:  %s\n", inode->selinux_label);
    printf("  [SELinux]   Access: %s%s%s\n",
           (mask & MAY_READ) ? "r" : "-",
           (mask & MAY_WRITE) ? "w" : "-",
           (mask & MAY_EXEC) ? "x" : "-");
    
    /* Simplified SELinux policy check */
    /* In real SELinux: check type enforcement rules */
    
    if (strcmp(cred->selinux_label, "unconfined_t") == 0) {
        printf("  [SELinux] ALLOW: unconfined domain\n");
        return 0;
    }
    
    if (strcmp(cred->selinux_label, "httpd_t") == 0 &&
        strcmp(inode->selinux_label, "httpd_sys_content_t") == 0) {
        printf("  [SELinux] ALLOW: httpd can access httpd_sys_content\n");
        return 0;
    }
    
    if (strcmp(cred->selinux_label, "httpd_t") == 0 &&
        strcmp(inode->selinux_label, "shadow_t") == 0) {
        printf("  [SELinux] DENY: httpd cannot access shadow file\n");
        return -1;  /* Permission denied */
    }
    
    printf("  [SELinux] DENY: no matching allow rule\n");
    return -1;
}

static int selinux_bprm_check(const char *filename, struct cred *cred)
{
    printf("  [SELinux] Checking execution of %s\n", filename);
    printf("  [SELinux]   Domain: %s\n", cred->selinux_label);
    
    /* Domain transition check */
    if (strcmp(cred->selinux_label, "user_t") == 0 &&
        strstr(filename, "/usr/sbin/") != NULL) {
        printf("  [SELinux] DENY: user_t cannot execute sbin programs\n");
        return -1;
    }
    
    printf("  [SELinux] ALLOW: execution permitted\n");
    return 0;
}

static const struct security_operations selinux_ops = {
    .name = "selinux",
    .inode_permission = selinux_inode_permission,
    .file_open = NULL,
    .task_create = NULL,
    .bprm_check = selinux_bprm_check,
};

/* ==========================================================
 * APPARMOR STRATEGY IMPLEMENTATION
 * Path-based profile confinement
 * ========================================================== */

static int apparmor_inode_permission(struct inode *inode, int mask,
                                      struct cred *cred)
{
    printf("  [AppArmor] Checking permission:\n");
    printf("  [AppArmor]   Profile: %s\n", cred->apparmor_profile);
    printf("  [AppArmor]   Path: %s\n", inode->path);
    printf("  [AppArmor]   Access: %s%s%s\n",
           (mask & MAY_READ) ? "r" : "-",
           (mask & MAY_WRITE) ? "w" : "-",
           (mask & MAY_EXEC) ? "x" : "-");
    
    /* Simplified AppArmor profile check */
    /* In real AppArmor: check path rules in profile */
    
    if (strcmp(cred->apparmor_profile, "unconfined") == 0) {
        printf("  [AppArmor] ALLOW: unconfined profile\n");
        return 0;
    }
    
    if (strcmp(cred->apparmor_profile, "firefox") == 0) {
        /* Firefox profile: can read /home, /tmp, limited /etc */
        if (strncmp(inode->path, "/home/", 6) == 0 ||
            strncmp(inode->path, "/tmp/", 5) == 0) {
            printf("  [AppArmor] ALLOW: firefox can access %s\n", inode->path);
            return 0;
        }
        if (strncmp(inode->path, "/etc/shadow", 11) == 0) {
            printf("  [AppArmor] DENY: firefox cannot access shadow\n");
            return -1;
        }
    }
    
    printf("  [AppArmor] DENY: not in profile allow list\n");
    return -1;
}

static int apparmor_bprm_check(const char *filename, struct cred *cred)
{
    printf("  [AppArmor] Checking execution of %s\n", filename);
    printf("  [AppArmor]   Profile: %s\n", cred->apparmor_profile);
    
    if (strcmp(cred->apparmor_profile, "firefox") == 0 &&
        strcmp(filename, "/bin/sh") == 0) {
        printf("  [AppArmor] DENY: firefox cannot execute shell\n");
        return -1;
    }
    
    printf("  [AppArmor] ALLOW: execution permitted\n");
    return 0;
}

static const struct security_operations apparmor_ops = {
    .name = "apparmor",
    .inode_permission = apparmor_inode_permission,
    .file_open = NULL,
    .task_create = NULL,
    .bprm_check = apparmor_bprm_check,
};

/* ==========================================================
 * CAPABILITY-ONLY (Default - no MAC)
 * ========================================================== */

static int capability_inode_permission(struct inode *inode, int mask,
                                        struct cred *cred)
{
    printf("  [Capability] Using standard Unix permissions\n");
    printf("  [Capability] ALLOW: DAC only, no MAC\n");
    return 0;  /* Always allow (let DAC handle it) */
}

static const struct security_operations capability_ops = {
    .name = "capability",
    .inode_permission = capability_inode_permission,
    .file_open = NULL,
    .task_create = NULL,
    .bprm_check = NULL,
};

/* ==========================================================
 * KERNEL CORE (MECHANISM)
 * ========================================================== */

/* Currently active security module */
static const struct security_operations *security_ops = &capability_ops;

/* Register security module (boot time) */
static int register_security(const struct security_operations *ops)
{
    printf("[LSM CORE] Registering security module: %s\n", ops->name);
    security_ops = ops;
    return 0;
}

/* Core: Security hook for inode permission */
static int security_inode_permission(struct inode *inode, int mask,
                                      struct cred *cred)
{
    if (security_ops && security_ops->inode_permission)
        return security_ops->inode_permission(inode, mask, cred);
    return 0;
}

/* Core: Security hook for program execution */
static int security_bprm_check(const char *filename, struct cred *cred)
{
    if (security_ops && security_ops->bprm_check)
        return security_ops->bprm_check(filename, cred);
    return 0;
}

/* ==========================================================
 * KERNEL OPERATIONS USING SECURITY HOOKS
 * ========================================================== */

/* VFS: Open file with security check */
static int vfs_open(struct file *file, struct cred *cred)
{
    int ret;
    
    printf("[VFS] vfs_open: %s\n", file->inode->path);
    
    /* MECHANISM: Normal permission check */
    printf("[VFS] DAC check (standard Unix permissions)... OK\n");
    
    /* STRATEGY: LSM security check */
    ret = security_inode_permission(file->inode, MAY_READ, cred);
    if (ret) {
        printf("[VFS] Access denied by security module\n");
        return ret;
    }
    
    printf("[VFS] File opened successfully\n");
    return 0;
}

/* Exec: Execute program with security check */
static int do_execve(const char *filename, struct cred *cred)
{
    int ret;
    
    printf("[EXEC] do_execve: %s\n", filename);
    
    /* MECHANISM: Load binary, setup */
    printf("[EXEC] Loading binary...\n");
    
    /* STRATEGY: LSM security check */
    ret = security_bprm_check(filename, cred);
    if (ret) {
        printf("[EXEC] Execution denied by security module\n");
        return ret;
    }
    
    printf("[EXEC] Execution permitted\n");
    return 0;
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("================================================\n");
    printf("LSM STRATEGY PATTERN DEMONSTRATION\n");
    printf("================================================\n");

    /* Create test credentials */
    struct cred web_server_selinux = {
        .uid = 48,  /* apache user */
        .selinux_label = "httpd_t",
        .apparmor_profile = "apache2",
    };

    struct cred web_server_user = {
        .uid = 1000,
        .selinux_label = "user_t",
        .apparmor_profile = "firefox",
    };

    /* Create test inodes */
    struct inode web_content = {
        .owner_uid = 48,
        .selinux_label = "httpd_sys_content_t",
        .path = "/var/www/html/index.html",
    };

    struct inode shadow_file = {
        .owner_uid = 0,
        .selinux_label = "shadow_t",
        .path = "/etc/shadow",
    };

    struct file web_file = { .inode = &web_content };
    struct file shadow = { .inode = &shadow_file };

    /* === Test with SELinux === */
    printf("\n=== SELINUX MODULE ===\n");
    register_security(&selinux_ops);

    printf("\n--- Web server accessing web content ---\n");
    vfs_open(&web_file, &web_server_selinux);

    printf("\n--- Web server accessing shadow file ---\n");
    vfs_open(&shadow, &web_server_selinux);

    printf("\n--- User executing sbin program ---\n");
    do_execve("/usr/sbin/useradd", &web_server_user);

    /* === Test with AppArmor === */
    printf("\n=== APPARMOR MODULE ===\n");
    register_security(&apparmor_ops);

    printf("\n--- Firefox accessing /home ---\n");
    struct inode home_file = {
        .path = "/home/user/document.txt",
    };
    struct file home = { .inode = &home_file };
    vfs_open(&home, &web_server_user);

    printf("\n--- Firefox accessing shadow file ---\n");
    vfs_open(&shadow, &web_server_user);

    printf("\n--- Firefox executing shell ---\n");
    do_execve("/bin/sh", &web_server_user);

    /* === Test with no MAC (capability only) === */
    printf("\n=== CAPABILITY ONLY (NO MAC) ===\n");
    register_security(&capability_ops);

    printf("\n--- Any access (only DAC applies) ---\n");
    vfs_open(&shadow, &web_server_user);

    printf("\n================================================\n");
    printf("KEY OBSERVATIONS:\n");
    printf("1. Same kernel code, different security decisions\n");
    printf("2. SELinux uses labels, AppArmor uses paths\n");
    printf("3. Security module is selected at boot\n");
    printf("4. Kernel core doesn't know policy details\n");
    printf("================================================\n");

    return 0;
}
```

---

## What the Kernel Core Does NOT Control

```
+=============================================================================+
|              WHAT CORE DOES NOT CONTROL (Strategy Owns)                      |
+=============================================================================+

    THE CORE DOES NOT DECIDE:

    1. ACCESS CONTROL DECISIONS
       +-------------------------------------------------------+
       | SELinux: type enforcement rules                       |
       | AppArmor: profile path rules                          |
       | SMACK: label comparisons                              |
       | Core just asks "is this allowed?"                     |
       +-------------------------------------------------------+

    2. SECURITY LABELING SCHEME
       +-------------------------------------------------------+
       | SELinux: user:role:type:level                         |
       | AppArmor: profile names                               |
       | SMACK: simple text labels                             |
       +-------------------------------------------------------+

    3. POLICY LANGUAGE AND FORMAT
       +-------------------------------------------------------+
       | SELinux: complex policy language, compiled policy     |
       | AppArmor: profile files with path globs               |
       | Each module has its own policy format                 |
       +-------------------------------------------------------+

    4. DOMAIN TRANSITIONS
       +-------------------------------------------------------+
       | SELinux: type_transition rules                        |
       | AppArmor: profile transitions (px, ix, etc.)          |
       | When and how to change security context               |
       +-------------------------------------------------------+

    THE CORE ONLY PROVIDES:
    - Hook points at sensitive operations
    - Credential and object structures
    - Attribute storage (xattrs for labels)
    - Hook invocation infrastructure
```

**中文说明：**

核心不控制的内容：(1) 访问控制决策——SELinux用类型强制规则、AppArmor用配置文件路径规则、SMACK用标签比较，核心只问"这是否允许？"；(2) 安全标签方案——SELinux用`user:role:type:level`、AppArmor用配置文件名、SMACK用简单文本标签；(3) 策略语言和格式——每个模块有自己的策略格式；(4) 域转换——何时以及如何改变安全上下文。核心只提供：敏感操作的钩子点、凭证和对象结构、属性存储、钩子调用基础设施。

---

## Real Kernel Code Reference (v3.2)

### struct security_operations in include/linux/security.h

```c
struct security_operations {
    char name[SECURITY_NAME_MAX + 1];

    int (*inode_permission)(struct inode *inode, int mask);
    int (*file_open)(struct file *file, const struct cred *cred);
    int (*bprm_check_security)(struct linux_binprm *bprm);
    /* ... 150+ hooks ... */
};
```

### Security hook invocation in security/security.c

```c
int security_inode_permission(struct inode *inode, int mask)
{
    if (unlikely(IS_PRIVATE(inode)))
        return 0;
    return security_ops->inode_permission(inode, mask);
}
```

---

## Key Takeaways

1. **Complete policy encapsulation**: Each LSM is a complete security policy
2. **Boot-time selection**: One primary module chosen at boot
3. **150+ hook points**: Comprehensive coverage of sensitive operations
4. **Policy vs mechanism**: Core provides hooks, module provides decisions
5. **Different paradigms**: Label-based (SELinux) vs path-based (AppArmor)
