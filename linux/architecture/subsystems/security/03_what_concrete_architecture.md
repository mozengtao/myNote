# WHAT｜具体架构

## 1. 模式：钩子表

```
PATTERNS: HOOK TABLES
+=============================================================================+
|                                                                              |
|  STRUCT SECURITY_HOOK_LIST                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  LSM hook registration uses linked lists:                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  struct security_hook_list {                                │ │    │ |
|  │  │  │      struct hlist_node       list;   // linked list node    │ │    │ |
|  │  │  │      struct hlist_head       *head;  // head of hook list   │ │    │ |
|  │  │  │      union security_list_options hook; // function pointer  │ │    │ |
|  │  │  │      char                    *lsm;   // LSM name            │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Each hook has a list head                               │ │    │ |
|  │  │  │  static struct hlist_head security_hook_heads = {           │ │    │ |
|  │  │  │      .inode_permission = ...,                               │ │    │ |
|  │  │  │      .file_open = ...,                                      │ │    │ |
|  │  │  │      .task_kill = ...,                                      │ │    │ |
|  │  │  │      // 200+ hook points                                    │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  HOOK DISPATCH:                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  security_hook_heads.inode_permission                       │ │    │ |
|  │  │  │         │                                                   │ │    │ |
|  │  │  │         ▼                                                   │ │    │ |
|  │  │  │  ┌─────────────────┐   ┌─────────────────┐   ┌───────────┐ │ │    │ |
|  │  │  │  │ capability      │──►│ selinux         │──►│ apparmor  │ │ │    │ |
|  │  │  │  │ _inode_perm()   │   │ _inode_perm()   │   │ _inode_...│ │ │    │ |
|  │  │  │  └─────────────────┘   └─────────────────┘   └───────────┘ │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Each registered LSM adds its function to the list          │ │    │ |
|  │  │  │  Hook dispatcher calls all functions in order               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CALL_INT_HOOK MACRO                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  #define call_int_hook(FUNC, IRC, ...) ({                        │    │ |
|  │  │      int RC = IRC;                 // default return code        │    │ |
|  │  │      struct security_hook_list *P;                               │    │ |
|  │  │                                                                  │    │ |
|  │  │      hlist_for_each_entry(P, &security_hook_heads.FUNC, list) {  │    │ |
|  │  │          RC = P->hook.FUNC(__VA_ARGS__);                         │    │ |
|  │  │          if (RC != 0)                                            │    │ |
|  │  │              break;               // stop on first denial        │    │ |
|  │  │      }                                                           │    │ |
|  │  │      RC;                                                         │    │ |
|  │  │  })                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Used by:                                                        │    │ |
|  │  │  int security_inode_permission(struct inode *inode, int mask) {  │    │ |
|  │  │      return call_int_hook(inode_permission, 0, inode, mask);     │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式：钩子表**

**struct security_hook_list**：
- LSM 钩子注册使用链表
- 包含：链表节点、钩子列表头、函数指针、LSM 名称
- 每个钩子有一个列表头（inode_permission、file_open、task_kill 等）

**钩子分发**：
- `security_hook_heads.inode_permission` 指向
- capability_inode_perm() → selinux_inode_perm() → apparmor_inode_...
- 每个注册的 LSM 将其函数添加到列表
- 钩子分发器按顺序调用所有函数

**call_int_hook 宏**：
- 遍历钩子链表
- 调用每个注册的函数
- 在第一个拒绝时停止
- `security_inode_permission()` 使用此宏

---

## 2. 核心结构：security_operations

```
CORE STRUCTURES: SECURITY_OPERATIONS
+=============================================================================+
|                                                                              |
|  UNION SECURITY_LIST_OPTIONS (function pointer union)                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  union security_list_options {                                   │    │ |
|  │  │      // FILE HOOKS                                               │    │ |
|  │  │      int (*inode_permission)(struct inode *, int);               │    │ |
|  │  │      int (*inode_create)(struct inode *, struct dentry *, ...);  │    │ |
|  │  │      int (*inode_link)(struct dentry *, struct inode *, ...);    │    │ |
|  │  │      int (*file_open)(struct file *);                            │    │ |
|  │  │      int (*file_permission)(struct file *, int);                 │    │ |
|  │  │      int (*mmap_file)(struct file *, unsigned long, ...);        │    │ |
|  │  │                                                                  │    │ |
|  │  │      // PROCESS HOOKS                                            │    │ |
|  │  │      int (*bprm_check_security)(struct linux_binprm *);          │    │ |
|  │  │      int (*task_create)(unsigned long);                          │    │ |
|  │  │      int (*task_kill)(struct task_struct *, struct siginfo *,    │    │ |
|  │  │                       int, u32);                                 │    │ |
|  │  │      int (*ptrace_access_check)(struct task_struct *, ...);      │    │ |
|  │  │                                                                  │    │ |
|  │  │      // NETWORK HOOKS                                            │    │ |
|  │  │      int (*socket_create)(int, int, int, int);                   │    │ |
|  │  │      int (*socket_bind)(struct socket *, struct sockaddr *, int);│    │ |
|  │  │      int (*socket_connect)(struct socket *, struct sockaddr *,...);│   │ |
|  │  │      int (*socket_sendmsg)(struct socket *, struct msghdr *, int);│    │ |
|  │  │                                                                  │    │ |
|  │  │      // CAPABILITY HOOKS                                         │    │ |
|  │  │      int (*capable)(const struct cred *, struct user_namespace *,│    │ |
|  │  │                     int, int);                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ... 200+ function pointers                               │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SELINUX STRUCTURES                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Security ID (SID) - represents a security context            │    │ |
|  │  │  typedef u32 security_id_t;                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Access Vector - bitmask of permissions                       │    │ |
|  │  │  typedef u32 access_vector_t;                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Context string mapping                                       │    │ |
|  │  │  // SID 523 ↔ "system_u:object_r:httpd_content_t:s0"             │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Access Vector Cache entry                                    │    │ |
|  │  │  struct avc_entry {                                              │    │ |
|  │  │      u32 ssid;              // source SID (subject)              │    │ |
|  │  │      u32 tsid;              // target SID (object)               │    │ |
|  │  │      u16 tclass;            // target class (file, socket,...)   │    │ |
|  │  │      struct av_decision avd; // cached decision                  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct av_decision {                                            │    │ |
|  │  │      u32 allowed;           // permitted operations              │    │ |
|  │  │      u32 auditallow;        // ops to audit when allowed         │    │ |
|  │  │      u32 auditdeny;         // ops to audit when denied          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Inode security label                                         │    │ |
|  │  │  struct inode_security_struct {                                  │    │ |
|  │  │      struct inode *inode;       // back pointer                  │    │ |
|  │  │      struct list_head list;     // for orphan inodes             │    │ |
|  │  │      u32 sid;                   // security ID                   │    │ |
|  │  │      u32 task_sid;              // creating task's SID           │    │ |
|  │  │      u16 sclass;                // security class                │    │ |
|  │  │      unsigned char initialized; // is label set?                 │    │ |
|  │  │      spinlock_t lock;                                            │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Process security label                                       │    │ |
|  │  │  struct task_security_struct {                                   │    │ |
|  │  │      u32 osid;          // SID prior to last exec                │    │ |
|  │  │      u32 sid;           // current SID                           │    │ |
|  │  │      u32 exec_sid;      // SID to set on exec                    │    │ |
|  │  │      u32 create_sid;    // SID for file creation                 │    │ |
|  │  │      u32 keycreate_sid; // SID for key creation                  │    │ |
|  │  │      u32 sockcreate_sid;// SID for socket creation               │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**核心结构**：

**union security_list_options**：
- 函数指针联合，包含 200+ 钩子
- **文件钩子**：inode_permission、inode_create、file_open、mmap_file
- **进程钩子**：bprm_check_security、task_create、task_kill、ptrace_access_check
- **网络钩子**：socket_create、socket_bind、socket_connect
- **能力钩子**：capable

**SELinux 结构**：

- **Security ID (SID)**：表示安全上下文的 u32
- **Access Vector**：权限位掩码
- **上下文字符串映射**：SID 523 ↔ "system_u:object_r:httpd_content_t:s0"

**AVC 条目**：
- ssid：源 SID（主体）
- tsid：目标 SID（对象）
- tclass：目标类（file、socket）
- avd：缓存的决策（allowed、auditallow、auditdeny）

**inode_security_struct**：inode、sid、task_sid、sclass、initialized

**task_security_struct**：osid、sid、exec_sid、create_sid

---

## 3. 控制流：权限检查

```
CONTROL FLOW: PERMISSION CHECKS
+=============================================================================+
|                                                                              |
|  FILE ACCESS CHECK PATH                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User calls: open("/var/www/index.html", O_RDONLY)               │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  sys_open() [fs/open.c]                                          │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  do_sys_open()                                                   │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  do_filp_open()                                                  │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ├──────────────────────────────────────────┐        │    │ |
|  │  │              │                                          ▼        │    │ |
|  │  │              │                              path_openat()        │    │ |
|  │  │              │                                          │        │    │ |
|  │  │              │                                          ▼        │    │ |
|  │  │              │                              link_path_walk()     │    │ |
|  │  │              │                              (resolve each        │    │ |
|  │  │              │                               path component)     │    │ |
|  │  │              │                                          │        │    │ |
|  │  │              │                     for each directory:  │        │    │ |
|  │  │              │                                          ▼        │    │ |
|  │  │              │                         ┌────────────────────────┐│    │ |
|  │  │              │                         │ may_lookup()           ││    │ |
|  │  │              │                         │          │             ││    │ |
|  │  │              │                         │          ▼             ││    │ |
|  │  │              │                         │ inode_permission()     ││    │ |
|  │  │              │                         │          │             ││    │ |
|  │  │              │                         │          ▼             ││    │ |
|  │  │              │                         │ ┌──────────────────────┴│    │ |
|  │  │              │                         │ │ security_inode_       │    │ |
|  │  │              │                         │ │ permission(inode,MAY_X│    │ |
|  │  │              │                         │ │         │             │    │ |
|  │  │              │                         │ │         ▼             │    │ |
|  │  │              │                         │ │ selinux_inode_perm()  │    │ |
|  │  │              │                         │ │         │             │    │ |
|  │  │              │                         │ │         ▼             │    │ |
|  │  │              │                         │ │ avc_has_perm()        │    │ |
|  │  │              │                         │ │ Check: httpd_t can    │    │ |
|  │  │              │                         │ │ search httpd_content_t│    │ |
|  │  │              │                         │ └───────────────────────│    │ |
|  │  │              │                         └────────────────────────┘│    │ |
|  │  │              │                                                   │    │ |
|  │  │              │  After path resolved:                             │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │  may_open()                                                   ││    │ |
|  │  │  │          │                                                    ││    │ |
|  │  │  │          ▼                                                    ││    │ |
|  │  │  │  inode_permission(inode, MAY_READ)                            ││    │ |
|  │  │  │          │                                                    ││    │ |
|  │  │  │          ▼                                                    ││    │ |
|  │  │  │  ┌────────────────────────────────────────────────────────┐  ││    │ |
|  │  │  │  │  security_inode_permission(inode, MAY_READ)            │  ││    │ |
|  │  │  │  │          │                                              │  ││    │ |
|  │  │  │  │          ▼                                              │  ││    │ |
|  │  │  │  │  selinux_inode_permission()                             │  ││    │ |
|  │  │  │  │          │                                              │  ││    │ |
|  │  │  │  │          ├── current_sid()     // get process SID       │  ││    │ |
|  │  │  │  │          │   → httpd_t                                  │  ││    │ |
|  │  │  │  │          │                                              │  ││    │ |
|  │  │  │  │          ├── inode_sid(inode)  // get file SID          │  ││    │ |
|  │  │  │  │          │   → httpd_content_t                          │  ││    │ |
|  │  │  │  │          │                                              │  ││    │ |
|  │  │  │  │          └── avc_has_perm(httpd_t, httpd_content_t,     │  ││    │ |
|  │  │  │  │                           SECCLASS_FILE, FILE__READ)    │  ││    │ |
|  │  │  │  │                    │                                    │  ││    │ |
|  │  │  │  │                    ▼                                    │  ││    │ |
|  │  │  │  │              ┌───────────────────────────────────────┐  │  ││    │ |
|  │  │  │  │              │  AVC Cache Lookup                     │  │  ││    │ |
|  │  │  │  │              │                                       │  │  ││    │ |
|  │  │  │  │              │  if (cached)                          │  │  ││    │ |
|  │  │  │  │              │      return cached_decision;          │  │  ││    │ |
|  │  │  │  │              │  else                                 │  │  ││    │ |
|  │  │  │  │              │      decision = security_compute_av();│  │  ││    │ |
|  │  │  │  │              │      cache_decision();                │  │  ││    │ |
|  │  │  │  │              │      return decision;                 │  │  ││    │ |
|  │  │  │  │              │                                       │  │  ││    │ |
|  │  │  │  │              └───────────────────────────────────────┘  │  ││    │ |
|  │  │  │  │                                                          │  ││    │ |
|  │  │  │  │  Returns: 0 (allowed) or -EACCES (denied)               │  ││    │ |
|  │  │  │  │                                                          │  ││    │ |
|  │  │  │  └────────────────────────────────────────────────────────┘  ││    │ |
|  │  │  │                                                               ││    │ |
|  │  │  └──────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  │  If all checks pass:                                             │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  vfs_open() → file descriptor returned to user                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制流：权限检查**

**文件访问检查路径**：

1. 用户调用 `open("/var/www/index.html", O_RDONLY)`
2. `sys_open()` → `do_sys_open()` → `do_filp_open()`
3. `path_openat()` → `link_path_walk()`（解析每个路径组件）

4. **对每个目录**：
   - `may_lookup()` → `inode_permission()`
   - `security_inode_permission(inode, MAY_EXEC)`
   - SELinux 检查：httpd_t 能否搜索 httpd_content_t

5. **路径解析后**：
   - `may_open()` → `inode_permission(inode, MAY_READ)`
   - `security_inode_permission(inode, MAY_READ)`

6. **selinux_inode_permission()**：
   - `current_sid()` → httpd_t（进程 SID）
   - `inode_sid()` → httpd_content_t（文件 SID）
   - `avc_has_perm(httpd_t, httpd_content_t, FILE, READ)`

7. **AVC 缓存查找**：
   - 如果缓存命中：返回缓存决策
   - 否则：`security_compute_av()` 计算，缓存，返回

8. 返回 0（允许）或 -EACCES（拒绝）

9. 如果所有检查通过：`vfs_open()` → 文件描述符返回给用户

---

## 4. 扩展点

```
EXTENSION POINTS
+=============================================================================+
|                                                                              |
|  REGISTERING A NEW LSM                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Define your LSM                                              │    │ |
|  │  │  DEFINE_LSM(my_lsm) = {                                          │    │ |
|  │  │      .name = "my_lsm",                                           │    │ |
|  │  │      .init = my_lsm_init,                                        │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Register hooks                                               │    │ |
|  │  │  static struct security_hook_list my_hooks[] = {                 │    │ |
|  │  │      LSM_HOOK_INIT(inode_permission, my_inode_permission),       │    │ |
|  │  │      LSM_HOOK_INIT(file_open, my_file_open),                     │    │ |
|  │  │      LSM_HOOK_INIT(task_kill, my_task_kill),                     │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  int __init my_lsm_init(void) {                                  │    │ |
|  │  │      security_add_hooks(my_hooks, ARRAY_SIZE(my_hooks),          │    │ |
|  │  │                         "my_lsm");                               │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Implement hook functions                                     │    │ |
|  │  │  static int my_inode_permission(struct inode *inode, int mask) { │    │ |
|  │  │      // Your security logic here                                 │    │ |
|  │  │      if (should_deny(...))                                       │    │ |
|  │  │          return -EACCES;                                         │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  BPF LSM (DYNAMIC SECURITY)                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // BPF program attached to LSM hook                             │    │ |
|  │  │  SEC("lsm/file_open")                                            │    │ |
|  │  │  int BPF_PROG(restrict_file_open, struct file *file) {           │    │ |
|  │  │      // Get file path                                            │    │ |
|  │  │      const char *path = file->f_path.dentry->d_name.name;        │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Block access to sensitive files                          │    │ |
|  │  │      if (is_sensitive_file(path))                                │    │ |
|  │  │          return -EPERM;                                          │    │ |
|  │  │                                                                  │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Benefits:                                                       │    │ |
|  │  │  • No kernel recompilation                                       │    │ |
|  │  │  • Dynamic policy updates                                        │    │ |
|  │  │  • Safe (BPF verifier)                                           │    │ |
|  │  │  • Programmable (not just config files)                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SECURITY FILESYSTEM (securityfs)                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /sys/kernel/security/                                           │    │ |
|  │  │  ├── selinux/                                                    │    │ |
|  │  │  │   ├── enforce          # 0=permissive, 1=enforcing            │    │ |
|  │  │  │   ├── policy           # loaded binary policy                 │    │ |
|  │  │  │   ├── booleans/        # runtime policy toggles               │    │ |
|  │  │  │   └── avc/             # AVC statistics                       │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── apparmor/                                                   │    │ |
|  │  │  │   ├── profiles         # loaded profiles                      │    │ |
|  │  │  │   └── .load            # load new profile                     │    │ |
|  │  │  │                                                               │    │ |
|  │  │  └── lockdown             # kernel lockdown status               │    │ |
|  │  │                                                                  │    │ |
|  │  │  LSMs export control interfaces here                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**扩展点**：

**注册新 LSM**：
1. 用 `DEFINE_LSM()` 定义 LSM
2. 定义钩子数组 `security_hook_list`
3. 在 init 函数中调用 `security_add_hooks()`
4. 实现钩子函数

**BPF LSM（动态安全）**：
- BPF 程序附加到 LSM 钩子
- 示例：`SEC("lsm/file_open")` 阻止敏感文件访问
- 好处：
  - 无需内核重新编译
  - 动态策略更新
  - 安全（BPF 验证器）
  - 可编程

**安全文件系统（securityfs）**：
- `/sys/kernel/security/`
- selinux/：enforce、policy、booleans、avc
- apparmor/：profiles、.load
- LSM 在此导出控制接口

---

## 5. 代价：运行时开销

```
COSTS: RUNTIME OVERHEAD
+=============================================================================+
|                                                                              |
|  HOOK OVERHEAD                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Per-hook overhead:                                              │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  1. Function call overhead                                  │ │    │ |
|  │  │  │     • Indirect call through function pointer                │ │    │ |
|  │  │  │     • ~10-20 cycles                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  2. Hook list traversal                                     │ │    │ |
|  │  │  │     • Walk linked list of registered hooks                  │ │    │ |
|  │  │  │     • ~5 cycles per LSM                                     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  3. Policy decision (SELinux)                               │ │    │ |
|  │  │  │     • AVC cache hit: ~50 cycles                             │ │    │ |
|  │  │  │     • AVC cache miss: ~1000-5000 cycles (policy lookup)     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Total per check: 100-200 cycles (cached)                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  File open path: ~5-10 security checks                           │    │ |
|  │  │  Total overhead: 500-2000 cycles (~0.5-1 microsecond)            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MEMORY OVERHEAD                                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Security blobs on kernel objects:                               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  SELinux inode blob:     ~40 bytes per inode                │ │    │ |
|  │  │  │  SELinux task blob:      ~48 bytes per task                 │ │    │ |
|  │  │  │  SELinux file blob:      ~24 bytes per open file            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  System with 100K inodes: ~4 MB                             │ │    │ |
|  │  │  │  System with 1000 tasks:  ~48 KB                            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  AVC cache:                                                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Default: 512 entries                                       │ │    │ |
|  │  │  │  Entry size: ~64 bytes                                      │ │    │ |
|  │  │  │  Total: ~32 KB                                              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Can be tuned via /selinuxfs/avc/cache_threshold            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Policy memory:                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Loaded policy: 1-10 MB (depends on complexity)             │ │    │ |
|  │  │  │  Reference policy: ~5 MB                                    │ │    │ |
|  │  │  │  Minimal policy: ~1 MB                                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  REAL-WORLD IMPACT                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Benchmarks (typical):                                           │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Operation           Without SELinux   With SELinux         │ │    │ |
|  │  │  │  ───────────────────────────────────────────────────────   │ │    │ |
|  │  │  │  File open           1.0 μs            1.2-1.5 μs           │ │    │ |
|  │  │  │  Process fork        50 μs             52-55 μs             │ │    │ |
|  │  │  │  Socket create       2 μs              2.3-2.5 μs           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Overall system impact: 1-7% slowdown                       │ │    │ |
|  │  │  │  (I/O-heavy workloads see higher impact)                    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  OPTIMIZATION TECHNIQUES:                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  1. AVC caching                                             │ │    │ |
|  │  │  │     95%+ cache hit rate in steady state                     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  2. Static branches                                         │ │    │ |
|  │  │  │     if (selinux_enabled) → branch prediction                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  3. RCU for read-mostly data                                │ │    │ |
|  │  │  │     Policy, SID mapping are read-heavy                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  4. Permissive mode for debugging                           │ │    │ |
|  │  │  │     Log denials but don't enforce                           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**代价：运行时开销**

**钩子开销**：
1. 函数调用开销：间接调用，~10-20 周期
2. 钩子列表遍历：每个 LSM ~5 周期
3. 策略决策：AVC 缓存命中 ~50 周期，缓存未命中 ~1000-5000 周期

每次检查总计：100-200 周期（缓存情况）
文件打开路径：5-10 次安全检查
总开销：500-2000 周期（~0.5-1 微秒）

**内存开销**：
- SELinux inode blob：~40 字节/inode
- SELinux task blob：~48 字节/task
- AVC 缓存：默认 512 条目，~32 KB
- 策略内存：1-10 MB

**真实影响**：
- 文件打开：1.0 → 1.2-1.5 μs
- 进程 fork：50 → 52-55 μs
- 整体系统影响：1-7% 减速

**优化技术**：
1. AVC 缓存：稳态 95%+ 命中率
2. 静态分支：分支预测
3. RCU 用于读多写少数据
4. 宽容模式用于调试
