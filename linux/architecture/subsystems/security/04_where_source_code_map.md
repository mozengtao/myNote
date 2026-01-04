# WHERE｜源代码地图

## 1. security/ 目录结构

```
SECURITY/ DIRECTORY STRUCTURE
+=============================================================================+
|                                                                              |
|  SECURITY SUBSYSTEM LAYOUT                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  security/                                                               │ |
|  │  │                                                                       │ |
|  │  │  LSM FRAMEWORK                                                        │ |
|  │  ├── security.c           ◄── Core LSM infrastructure                   │ |
|  │  │                             security_hook_heads                       │ |
|  │  │                             security_add_hooks()                      │ |
|  │  │                             All security_*() wrapper functions        │ |
|  │  │                                                                       │ |
|  │  ├── lsm_audit.c          ◄── Common audit logging                      │ |
|  │  │                                                                       │ |
|  │  ├── commoncap.c          ◄── POSIX capabilities                        │ |
|  │  │                             cap_capable()                             │ |
|  │  │                             cap_bprm_creds_from_file()                │ |
|  │  │                                                                       │ |
|  │  │  MAJOR LSMs                                                           │ |
|  │  ├── selinux/             ◄── SELinux implementation                    │ |
|  │  │   ├── hooks.c          ◄── Hook functions                            │ |
|  │  │   │                         selinux_inode_permission()                │ |
|  │  │   │                         selinux_file_open()                       │ |
|  │  │   │                                                                   │ |
|  │  │   ├── avc.c            ◄── Access Vector Cache                       │ |
|  │  │   │                         avc_has_perm()                            │ |
|  │  │   │                         avc_lookup()                              │ |
|  │  │   │                                                                   │ |
|  │  │   ├── ss/              ◄── Security Server                           │ |
|  │  │   │   ├── services.c       security_compute_av()                     │ |
|  │  │   │   ├── sidtab.c         SID ↔ context mapping                     │ |
|  │  │   │   ├── policydb.c       Policy database                           │ |
|  │  │   │   └── ebitmap.c        Bitmap operations                         │ |
|  │  │   │                                                                   │ |
|  │  │   ├── selinuxfs.c      ◄── /sys/fs/selinux interface                │ |
|  │  │   ├── netlabel.c       ◄── Network labeling                          │ |
|  │  │   └── xfrm.c           ◄── IPsec labeling                            │ |
|  │  │                                                                       │ |
|  │  ├── apparmor/            ◄── AppArmor implementation                   │ |
|  │  │   ├── lsm.c            ◄── Hook functions                            │ |
|  │  │   ├── apparmorfs.c     ◄── /sys/kernel/security/apparmor            │ |
|  │  │   ├── policy.c         ◄── Policy loading                            │ |
|  │  │   ├── domain.c         ◄── Domain transitions                        │ |
|  │  │   └── file.c           ◄── File access mediation                     │ |
|  │  │                                                                       │ |
|  │  ├── smack/               ◄── Smack implementation                      │ |
|  │  │   ├── smack_lsm.c      ◄── Hook functions                            │ |
|  │  │   ├── smack_access.c   ◄── Access decisions                          │ |
|  │  │   └── smackfs.c        ◄── /smackfs interface                        │ |
|  │  │                                                                       │ |
|  │  │  MINOR/STACKING LSMs                                                  │ |
|  │  ├── yama/                ◄── ptrace restrictions                       │ |
|  │  │   └── yama_lsm.c                                                      │ |
|  │  │                                                                       │ |
|  │  ├── loadpin/             ◄── Module loading restrictions               │ |
|  │  │   └── loadpin.c                                                       │ |
|  │  │                                                                       │ |
|  │  ├── lockdown/            ◄── Kernel lockdown                           │ |
|  │  │   └── lockdown.c                                                      │ |
|  │  │                                                                       │ |
|  │  ├── safesetid/           ◄── setuid/setgid restrictions                │ |
|  │  │   └── lsm.c                                                           │ |
|  │  │                                                                       │ |
|  │  ├── bpf/                 ◄── BPF LSM                                   │ |
|  │  │   └── hooks.c              Programmable security                     │ |
|  │  │                                                                       │ |
|  │  │  SPECIAL                                                              │ |
|  │  ├── keys/                ◄── Kernel keyring security                   │ |
|  │  │                                                                       │ |
|  │  ├── integrity/           ◄── File integrity                            │ |
|  │  │   ├── ima/             ◄── Integrity Measurement Architecture        │ |
|  │  │   └── evm/             ◄── Extended Verification Module              │ |
|  │  │                                                                       │ |
|  │  └── tomoyo/              ◄── TOMOYO LSM                                │ |
|  │      ├── tomoyo.c                                                        │ |
|  │      ├── domain.c                                                        │ |
|  │      └── file.c                                                          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  INCLUDE FILES                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  include/linux/security.h     ◄── security_*() prototypes              │ |
|  │                                    struct security_hook_list            │ |
|  │                                                                          │ |
|  │  include/linux/lsm_hooks.h    ◄── Hook definitions                      │ |
|  │                                    union security_list_options          │ |
|  │                                    LSM_HOOK_INIT() macro                │ |
|  │                                                                          │ |
|  │  include/linux/lsm_audit.h    ◄── Audit structures                      │ |
|  │                                                                          │ |
|  │  include/uapi/linux/capability.h  ◄── Capability constants              │ |
|  │                                        CAP_NET_ADMIN, etc.              │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**security/ 目录结构**：

**LSM 框架**：
- `security.c`：核心 LSM 基础设施，security_hook_heads，所有 security_*() 包装函数
- `lsm_audit.c`：通用审计日志
- `commoncap.c`：POSIX 能力

**主要 LSM**：
- `selinux/`：
  - `hooks.c`：钩子函数
  - `avc.c`：访问向量缓存
  - `ss/`：安全服务器（services.c、sidtab.c、policydb.c）
  - `selinuxfs.c`：/sys/fs/selinux 接口

- `apparmor/`：
  - `lsm.c`：钩子函数
  - `policy.c`：策略加载
  - `domain.c`：域转换

- `smack/`：
  - `smack_lsm.c`：钩子函数
  - `smack_access.c`：访问决策

**次要/可堆叠 LSM**：
- `yama/`：ptrace 限制
- `loadpin/`：模块加载限制
- `lockdown/`：内核锁定
- `bpf/`：可编程安全

**特殊**：
- `keys/`：内核密钥环安全
- `integrity/`：文件完整性（IMA、EVM）

---

## 2. 架构锚点：security_operations

```
ARCHITECTURAL ANCHORS
+=============================================================================+
|                                                                              |
|  SECURITY HOOK HEADS (security/security.c)                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct security_hook_heads {                                    │    │ |
|  │  │      struct hlist_head binder_set_context_mgr;                   │    │ |
|  │  │      struct hlist_head binder_transaction;                       │    │ |
|  │  │      struct hlist_head ptrace_access_check;                      │    │ |
|  │  │      struct hlist_head ptrace_traceme;                           │    │ |
|  │  │      struct hlist_head capget;                                   │    │ |
|  │  │      struct hlist_head capset;                                   │    │ |
|  │  │      struct hlist_head capable;                                  │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      struct hlist_head inode_permission;     ◄── FILE ACCESS     │    │ |
|  │  │      struct hlist_head inode_create;                             │    │ |
|  │  │      struct hlist_head inode_link;                               │    │ |
|  │  │      struct hlist_head inode_unlink;                             │    │ |
|  │  │      struct hlist_head file_permission;                          │    │ |
|  │  │      struct hlist_head file_open;            ◄── FILE OPEN       │    │ |
|  │  │      struct hlist_head file_mmap;                                │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      struct hlist_head task_create;          ◄── PROCESS         │    │ |
|  │  │      struct hlist_head task_kill;                                │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      struct hlist_head socket_create;        ◄── NETWORK         │    │ |
|  │  │      struct hlist_head socket_bind;                              │    │ |
|  │  │      struct hlist_head socket_connect;                           │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      // 200+ hook heads                                          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  static struct security_hook_heads security_hook_heads           │    │ |
|  │  │      __lsm_ro_after_init;                                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SELINUX SECURITY STATE                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Global SELinux state (security/selinux/include/objsec.h)     │    │ |
|  │  │  struct selinux_state {                                          │    │ |
|  │  │      bool disabled;           // SELinux disabled                │    │ |
|  │  │      bool enforcing;          // Enforcing mode                  │    │ |
|  │  │      bool initialized;        // Policy loaded                   │    │ |
|  │  │                                                                  │    │ |
|  │  │      struct selinux_avc *avc; // Access Vector Cache             │    │ |
|  │  │      struct selinux_ss *ss;   // Security Server                 │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Access Vector Cache                                          │    │ |
|  │  │  struct selinux_avc {                                            │    │ |
|  │  │      unsigned int avc_cache_threshold;                           │    │ |
|  │  │      struct avc_cache avc_cache;                                 │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct avc_cache {                                              │    │ |
|  │  │      struct hlist_head slots[AVC_CACHE_SLOTS];                   │    │ |
|  │  │      spinlock_t slots_lock[AVC_CACHE_SLOTS];                     │    │ |
|  │  │      atomic_t lru_hint;                                          │    │ |
|  │  │      atomic_t active_nodes;                                      │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Security Server                                              │    │ |
|  │  │  struct selinux_ss {                                             │    │ |
|  │  │      struct policydb policydb;  // The loaded policy             │    │ |
|  │  │      struct sidtab sidtab;      // SID table                     │    │ |
|  │  │      rwlock_t policy_rwlock;                                     │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**架构锚点**：

**security_hook_heads**（security/security.c）：
- 包含 200+ 钩子头
- 关键钩子：
  - 文件访问：inode_permission、inode_create、file_open、file_mmap
  - 进程：task_create、task_kill
  - 网络：socket_create、socket_bind、socket_connect

**SELinux 安全状态**：

**selinux_state**：
- disabled：SELinux 禁用
- enforcing：强制模式
- initialized：策略已加载
- avc：访问向量缓存
- ss：安全服务器

**selinux_avc**：
- avc_cache：缓存槽数组和锁

**selinux_ss**：
- policydb：加载的策略
- sidtab：SID 表
- policy_rwlock：策略读写锁

---

## 3. 控制中心：security_inode_permission()

```
CONTROL HUBS
+=============================================================================+
|                                                                              |
|  SECURITY_INODE_PERMISSION (security/security.c)                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int security_inode_permission(struct inode *inode, int mask) {  │    │ |
|  │  │      if (unlikely(IS_PRIVATE(inode)))                            │    │ |
|  │  │          return 0;                                               │    │ |
|  │  │      return call_int_hook(inode_permission, 0, inode, mask);     │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Called from:                                                    │    │ |
|  │  │  • inode_permission() [fs/namei.c]                               │    │ |
|  │  │  • generic_permission() [fs/namei.c]                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Dispatches to:                                                  │    │ |
|  │  │  • selinux_inode_permission()                                    │    │ |
|  │  │  • apparmor_inode_permission()                                   │    │ |
|  │  │  • smack_inode_permission()                                      │    │ |
|  │  │  • (other registered LSMs)                                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER KEY CONTROL HUBS                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  File Operations:                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  security_file_open()       [security/security.c]                │    │ |
|  │  │      Called from: do_dentry_open() [fs/open.c]                   │    │ |
|  │  │      Checks: Can process open this specific file?                │    │ |
|  │  │                                                                  │    │ |
|  │  │  security_file_permission() [security/security.c]                │    │ |
|  │  │      Called from: file_permission() [include/linux/fs.h]         │    │ |
|  │  │      Checks: Per-operation checks on open file                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  security_mmap_file()       [security/security.c]                │    │ |
|  │  │      Called from: do_mmap() [mm/mmap.c]                          │    │ |
|  │  │      Checks: Memory mapping permissions                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Process Operations:                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  security_bprm_check()      [security/security.c]                │    │ |
|  │  │      Called from: search_binary_handler() [fs/exec.c]            │    │ |
|  │  │      Checks: Can process exec this binary?                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  security_task_kill()       [security/security.c]                │    │ |
|  │  │      Called from: check_kill_permission() [kernel/signal.c]      │    │ |
|  │  │      Checks: Can process send signal to target?                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  security_ptrace_access_check() [security/security.c]            │    │ |
|  │  │      Called from: ptrace_may_access() [kernel/ptrace.c]          │    │ |
|  │  │      Checks: Can process ptrace target?                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Network Operations:                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  security_socket_create()   [security/security.c]                │    │ |
|  │  │      Called from: __sock_create() [net/socket.c]                 │    │ |
|  │  │      Checks: Can process create this type of socket?             │    │ |
|  │  │                                                                  │    │ |
|  │  │  security_socket_bind()     [security/security.c]                │    │ |
|  │  │      Called from: __sys_bind() [net/socket.c]                    │    │ |
|  │  │      Checks: Can process bind to this address/port?              │    │ |
|  │  │                                                                  │    │ |
|  │  │  security_socket_connect()  [security/security.c]                │    │ |
|  │  │      Called from: __sys_connect() [net/socket.c]                 │    │ |
|  │  │      Checks: Can process connect to this address?                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  SELinux Specific:                                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  avc_has_perm()             [security/selinux/avc.c]             │    │ |
|  │  │      Core SELinux permission check                               │    │ |
|  │  │      Checks AVC cache, falls back to security server             │    │ |
|  │  │                                                                  │    │ |
|  │  │  security_compute_av()      [security/selinux/ss/services.c]     │    │ |
|  │  │      Compute access vector from policy                           │    │ |
|  │  │      Called on AVC miss                                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  sel_write_load()           [security/selinux/selinuxfs.c]       │    │ |
|  │  │      Load new policy from userspace                              │    │ |
|  │  │      Triggered by: semodule, load_policy                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制中心**：

**security_inode_permission()**：
- 位置：security/security.c
- 调用自：inode_permission()、generic_permission()
- 分发到：selinux_inode_permission()、apparmor_inode_permission() 等

**其他关键控制中心**：

**文件操作**：
- `security_file_open()`：进程能打开这个文件吗？
- `security_file_permission()`：打开文件的每操作检查
- `security_mmap_file()`：内存映射权限

**进程操作**：
- `security_bprm_check()`：进程能执行这个二进制吗？
- `security_task_kill()`：进程能发送信号吗？
- `security_ptrace_access_check()`：进程能 ptrace 目标吗？

**网络操作**：
- `security_socket_create()`：进程能创建这类 socket 吗？
- `security_socket_bind()`：进程能绑定到这个地址/端口吗？
- `security_socket_connect()`：进程能连接到这个地址吗？

**SELinux 特定**：
- `avc_has_perm()`：核心权限检查，检查 AVC 缓存
- `security_compute_av()`：从策略计算访问向量
- `sel_write_load()`：从用户空间加载新策略

---

## 4. 阅读策略

```
READING STRATEGY
+=============================================================================+
|                                                                              |
|  RECOMMENDED READING ORDER                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  LEVEL 1: FRAMEWORK                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. include/linux/lsm_hooks.h                                    │    │ |
|  │  │     • All hook definitions                                       │    │ |
|  │  │     • union security_list_options                                │    │ |
|  │  │     • LSM_HOOK_INIT macro                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. include/linux/security.h                                     │    │ |
|  │  │     • security_*() prototypes                                    │    │ |
|  │  │     • What the kernel calls                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. security/security.c                                          │    │ |
|  │  │     • Hook dispatch logic                                        │    │ |
|  │  │     • security_add_hooks()                                       │    │ |
|  │  │     • call_int_hook() macro                                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 2: SELINUX OVERVIEW                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  4. security/selinux/include/objsec.h                            │    │ |
|  │  │     • Security blob structures                                   │    │ |
|  │  │     • inode_security_struct                                      │    │ |
|  │  │     • task_security_struct                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  5. security/selinux/hooks.c (first 500 lines)                   │    │ |
|  │  │     • Hook registration                                          │    │ |
|  │  │     • selinux_hooks[] array                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  6. security/selinux/avc.c                                       │    │ |
|  │  │     • avc_has_perm()                                             │    │ |
|  │  │     • Cache lookup logic                                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 3: SELINUX DEEP DIVE                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  7. security/selinux/hooks.c (specific hooks)                    │    │ |
|  │  │     • selinux_inode_permission()                                 │    │ |
|  │  │     • selinux_file_open()                                        │    │ |
|  │  │     • selinux_bprm_check_security()                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  8. security/selinux/ss/services.c                               │    │ |
|  │  │     • security_compute_av()                                      │    │ |
|  │  │     • Policy decision engine                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  9. security/selinux/ss/policydb.c                               │    │ |
|  │  │     • Policy database structures                                 │    │ |
|  │  │     • Type enforcement rules                                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 4: ALTERNATIVES                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  10. security/apparmor/lsm.c                                     │    │ |
|  │  │      • Compare: simpler than SELinux                             │    │ |
|  │  │      • Path-based approach                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  11. security/yama/yama_lsm.c                                    │    │ |
|  │  │      • Minimal LSM example                                       │    │ |
|  │  │      • Good for learning LSM structure                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  12. security/bpf/hooks.c                                        │    │ |
|  │  │      • BPF integration with LSM                                  │    │ |
|  │  │      • Modern extensibility                                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**第 1 层：框架**
1. `include/linux/lsm_hooks.h`：所有钩子定义
2. `include/linux/security.h`：security_*() 原型
3. `security/security.c`：钩子分发逻辑

**第 2 层：SELinux 概述**
4. `security/selinux/include/objsec.h`：安全 blob 结构
5. `security/selinux/hooks.c`：钩子注册
6. `security/selinux/avc.c`：缓存查找逻辑

**第 3 层：SELinux 深入**
7. `security/selinux/hooks.c`：具体钩子
8. `security/selinux/ss/services.c`：策略决策引擎
9. `security/selinux/ss/policydb.c`：策略数据库

**第 4 层：替代方案**
10. `security/apparmor/lsm.c`：比 SELinux 简单
11. `security/yama/yama_lsm.c`：最小 LSM 示例
12. `security/bpf/hooks.c`：BPF 与 LSM 集成

---

## 5. 验证方法

```
VALIDATION APPROACH
+=============================================================================+
|                                                                              |
|  METHOD 1: AUDIT LOG (ausearch)                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # View SELinux denials                                                  │ |
|  │  ausearch -m avc -ts recent                                              │ |
|  │                                                                          │ |
|  │  # Example output:                                                       │ |
|  │  type=AVC msg=audit(1234567890.123:456): avc: denied { read } for        │ |
|  │    pid=1234 comm="httpd" name="secret.txt"                               │ |
|  │    scontext=system_u:system_r:httpd_t:s0                                 │ |
|  │    tcontext=system_u:object_r:admin_home_t:s0                            │ |
|  │    tclass=file                                                           │ |
|  │                                                                          │ |
|  │  # Search for specific type                                              │ |
|  │  ausearch -m avc -c httpd                                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 2: SELINUX TOOLS                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Check current mode                                                    │ |
|  │  getenforce                                                              │ |
|  │  → Enforcing                                                             │ |
|  │                                                                          │ |
|  │  # Get file context                                                      │ |
|  │  ls -Z /var/www/html/index.html                                          │ |
|  │  → system_u:object_r:httpd_sys_content_t:s0                              │ |
|  │                                                                          │ |
|  │  # Get process context                                                   │ |
|  │  ps -eZ | grep httpd                                                     │ |
|  │  → system_u:system_r:httpd_t:s0  1234 ?  httpd                           │ |
|  │                                                                          │ |
|  │  # Check if access would be allowed                                      │ |
|  │  sesearch -A -s httpd_t -t httpd_sys_content_t -c file -p read           │ |
|  │  → allow httpd_t httpd_sys_content_t:file { read open };                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 3: FTRACE                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Trace security_inode_permission calls                                 │ |
|  │  echo 'p:myprobe security_inode_permission' > /sys/kernel/debug/         │ |
|  │      tracing/kprobe_events                                               │ |
|  │  echo 1 > /sys/kernel/debug/tracing/events/kprobes/myprobe/enable        │ |
|  │  cat /sys/kernel/debug/tracing/trace_pipe                                │ |
|  │                                                                          │ |
|  │  # See all security hooks being called                                   │ |
|  │  echo 'security_*' > /sys/kernel/debug/tracing/set_ftrace_filter         │ |
|  │  echo function > /sys/kernel/debug/tracing/current_tracer                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 4: AVC STATISTICS                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # View AVC cache statistics                                             │ |
|  │  cat /sys/fs/selinux/avc/cache_stats                                     │ |
|  │                                                                          │ |
|  │  lookups hits misses allocations reclaims frees                          │ |
|  │  1234567 1200000 34567 34567 100 100                                     │ |
|  │                                                                          │ |
|  │  # Calculate hit rate                                                    │ |
|  │  hit_rate = hits / lookups = 97%                                         │ |
|  │                                                                          │ |
|  │  # Flush AVC cache (for testing)                                         │ |
|  │  echo 1 > /sys/fs/selinux/avc/cache_stats                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 5: PERMISSIVE MODE                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Set to permissive (log but don't enforce)                             │ |
|  │  setenforce 0                                                            │ |
|  │                                                                          │ |
|  │  # Run your application                                                  │ |
|  │  ./my_app                                                                │ |
|  │                                                                          │ |
|  │  # See what WOULD be denied                                              │ |
|  │  ausearch -m avc -ts recent                                              │ |
|  │                                                                          │ |
|  │  # Generate policy from denials                                          │ |
|  │  audit2allow -a -M my_policy                                             │ |
|  │                                                                          │ |
|  │  # Re-enable enforcement                                                 │ |
|  │  setenforce 1                                                            │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**验证方法**：

**方法 1：审计日志（ausearch）**
- `ausearch -m avc -ts recent`：查看最近 SELinux 拒绝
- 输出包含：pid、comm、scontext、tcontext、tclass

**方法 2：SELinux 工具**
- `getenforce`：检查当前模式
- `ls -Z`：获取文件上下文
- `ps -eZ`：获取进程上下文
- `sesearch`：检查访问是否允许

**方法 3：ftrace**
- 跟踪 security_inode_permission 调用
- 查看所有安全钩子调用

**方法 4：AVC 统计**
- `/sys/fs/selinux/avc/cache_stats`
- 计算命中率 = hits / lookups

**方法 5：宽容模式**
- `setenforce 0`：设置为宽容模式
- 运行应用，查看会被拒绝什么
- `audit2allow`：从拒绝生成策略
- `setenforce 1`：重新启用强制
