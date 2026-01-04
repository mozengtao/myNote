# HOW｜架构策略

## 1. LSM 框架设计

```
LSM FRAMEWORK DESIGN
+=============================================================================+
|                                                                              |
|  THE HOOK-BASED ARCHITECTURE                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User Space                                                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  Application: open("/etc/passwd", O_RDONLY)                 │ │    │ |
|  │  │  └───────────────────────────┬────────────────────────────────┘ │    │ |
|  │  │                              │                                   │    │ |
|  │  │  ═══════════════════════════╪═══════════════════════════════   │    │ |
|  │  │                              │ syscall                           │    │ |
|  │  │  Kernel Space                ▼                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                       VFS Layer                             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  do_sys_open()                                              │ │    │ |
|  │  │  │       │                                                     │ │    │ |
|  │  │  │       ▼                                                     │ │    │ |
|  │  │  │  path_lookup()  ──────────────────────────────────────┐     │ │    │ |
|  │  │  │       │                                               │     │ │    │ |
|  │  │  │       │                                               ▼     │ │    │ |
|  │  │  │       │                                    ┌───────────────┐│ │    │ |
|  │  │  │       │                                    │ LSM HOOK      ││ │    │ |
|  │  │  │       │                                    │               ││ │    │ |
|  │  │  │       │                                    │ security_     ││ │    │ |
|  │  │  │       │                                    │ inode_        ││ │    │ |
|  │  │  │       │                                    │ permission()  ││ │    │ |
|  │  │  │       │                                    │               ││ │    │ |
|  │  │  │       │                                    │ Returns:      ││ │    │ |
|  │  │  │       │                                    │ 0 = allow     ││ │    │ |
|  │  │  │       │                                    │ -EACCES = deny││ │    │ |
|  │  │  │       │                                    └───────┬───────┘│ │    │ |
|  │  │  │       │                                            │        │ │    │ |
|  │  │  │       ◄────────────────────────────────────────────┘        │ │    │ |
|  │  │  │       │                                                     │ │    │ |
|  │  │  │       ▼                                                     │ │    │ |
|  │  │  │  [If allowed] proceed with open                             │ │    │ |
|  │  │  │  [If denied] return -EACCES to userspace                    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LSM ABSTRACTION LAYER                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Kernel Core                     LSM Layer           Security   │    │ |
|  │  │  (VFS, net, ...)                                     Modules    │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────┐              ┌─────────────┐    ┌────────────┐  │    │ |
|  │  │  │            │              │             │    │  SELinux   │  │    │ |
|  │  │  │  inode_    │  ──────────► │  security_  │───►│  selinux_  │  │    │ |
|  │  │  │  permission│              │  inode_     │    │  inode_    │  │    │ |
|  │  │  │            │              │  permission │    │  permission│  │    │ |
|  │  │  └────────────┘              │             │    └────────────┘  │    │ |
|  │  │                              │             │                    │    │ |
|  │  │                              │             │    ┌────────────┐  │    │ |
|  │  │                              │             │───►│  AppArmor  │  │    │ |
|  │  │                              │             │    │  apparmor_ │  │    │ |
|  │  │                              │             │    │  inode_... │  │    │ |
|  │  │                              │             │    └────────────┘  │    │ |
|  │  │                              │             │                    │    │ |
|  │  │                              │             │    ┌────────────┐  │    │ |
|  │  │                              │             │───►│   Smack    │  │    │ |
|  │  │                              └─────────────┘    └────────────┘  │    │ |
|  │  │                                                                  │    │ |
|  │  │  KEY BENEFIT: Kernel code doesn't know which LSM is active       │    │ |
|  │  │               Just calls generic security_*() functions          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  HOOK CATEGORIES                                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  FILE OPERATIONS (~50 hooks):                                    │    │ |
|  │  │  ├── security_inode_create()      - creating files              │    │ |
|  │  │  ├── security_inode_permission()  - access checks               │    │ |
|  │  │  ├── security_inode_link()        - hard links                  │    │ |
|  │  │  ├── security_file_open()         - opening files               │    │ |
|  │  │  ├── security_file_mmap()         - memory mapping              │    │ |
|  │  │  └── ...                                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  PROCESS OPERATIONS (~30 hooks):                                 │    │ |
|  │  │  ├── security_bprm_check()        - exec permission             │    │ |
|  │  │  ├── security_task_create()       - fork                        │    │ |
|  │  │  ├── security_task_kill()         - signal delivery             │    │ |
|  │  │  ├── security_ptrace_access()     - debugging                   │    │ |
|  │  │  └── ...                                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  NETWORK OPERATIONS (~40 hooks):                                 │    │ |
|  │  │  ├── security_socket_create()     - socket creation             │    │ |
|  │  │  ├── security_socket_bind()       - binding to port             │    │ |
|  │  │  ├── security_socket_connect()    - outgoing connections        │    │ |
|  │  │  ├── security_sk_receive_skb()    - incoming packets            │    │ |
|  │  │  └── ...                                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  IPC OPERATIONS (~20 hooks):                                     │    │ |
|  │  │  ├── security_msg_queue_*()       - message queues              │    │ |
|  │  │  ├── security_shm_*()             - shared memory               │    │ |
|  │  │  ├── security_sem_*()             - semaphores                  │    │ |
|  │  │  └── ...                                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  CAPABILITY HOOKS:                                               │    │ |
|  │  │  ├── security_capable()           - capability checks           │    │ |
|  │  │  └── security_capset()            - modifying caps              │    │ |
|  │  │                                                                  │    │ |
|  │  │  TOTAL: 200+ hooks for complete mediation                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**LSM 框架设计**：

**基于钩子的架构**：
1. 用户空间应用调用 `open("/etc/passwd")`
2. 系统调用进入内核
3. VFS 层处理请求
4. 在关键点调用 LSM 钩子：`security_inode_permission()`
5. 返回 0（允许）或 -EACCES（拒绝）
6. 如果允许，继续操作；如果拒绝，返回错误

**LSM 抽象层**：
- 内核核心调用 `security_inode_permission()`
- LSM 层分发到：SELinux、AppArmor、Smack 等
- **关键好处**：内核代码不知道哪个 LSM 活跃，只调用通用 security_*() 函数

**钩子类别**：
- 文件操作：~50 个钩子（create、permission、link、open、mmap）
- 进程操作：~30 个钩子（exec、fork、kill、ptrace）
- 网络操作：~40 个钩子（socket、bind、connect、receive）
- IPC 操作：~20 个钩子（msg、shm、sem）
- 能力钩子：capable、capset

总计：200+ 钩子用于完全调解

---

## 2. 策略与机制分离

```
POLICY VS MECHANISM SEPARATION
+=============================================================================+
|                                                                              |
|  THE PRINCIPLE                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  MECHANISM:  "How to enforce access control"                     │    │ |
|  │  │              Implemented by LSM framework + kernel hooks         │    │ |
|  │  │              Fixed, compiled into kernel                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  POLICY:     "What access to allow/deny"                         │    │ |
|  │  │              Defined by administrator                            │    │ |
|  │  │              Configurable at runtime                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  WHY SEPARATE?                                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Different deployments need different policies:                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Desktop (permissive):                                      │ │    │ |
|  │  │  │  • User runs many apps                                      │ │    │ |
|  │  │  │  • Apps need access to user files                           │ │    │ |
|  │  │  │  • Convenience > strict security                            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Server (strict):                                           │ │    │ |
|  │  │  │  • Few well-known services                                  │ │    │ |
|  │  │  │  • Each service tightly confined                            │ │    │ |
|  │  │  │  • Security > convenience                                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Container host (isolation-focused):                        │ │    │ |
|  │  │  │  • Each container fully isolated                            │ │    │ |
|  │  │  │  • No container escapes                                     │ │    │ |
|  │  │  │  • Defense in depth                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  SAME KERNEL, DIFFERENT POLICIES!                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SELINUX ARCHITECTURE                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐  │    │ |
|  │  │  │                    USERSPACE                               │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐ │  │    │ |
|  │  │  │  │              POLICY (administrator-defined)          │ │  │    │ |
|  │  │  │  │                                                       │ │  │    │ |
|  │  │  │  │  allow httpd_t httpd_content_t:file { read open };   │ │  │    │ |
|  │  │  │  │  deny  httpd_t shadow_t:file { read };               │ │  │    │ |
|  │  │  │  │                                                       │ │  │    │ |
|  │  │  │  │  Compiled by: checkpolicy, semodule                  │ │  │    │ |
|  │  │  │  └───────────────────────────┬──────────────────────────┘ │  │    │ |
|  │  │  │                              │                            │  │    │ |
|  │  │  └──────────────────────────────┼────────────────────────────┘  │    │ |
|  │  │                                 │ selinuxfs                     │    │ |
|  │  │  ════════════════════════════════════════════════════════════  │    │ |
|  │  │                                 │                               │    │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐  │    │ |
|  │  │  │                    KERNEL                                  │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐ │  │    │ |
|  │  │  │  │            SELinux Security Server                    │ │  │    │ |
|  │  │  │  │                                                       │ │  │    │ |
|  │  │  │  │  ┌─────────────────────────────────────────────────┐ │ │  │    │ |
|  │  │  │  │  │           Policy Database                        │ │ │  │    │ |
|  │  │  │  │  │  (loaded from userspace, stored in kernel)       │ │ │  │    │ |
|  │  │  │  │  └─────────────────────────────────────────────────┘ │ │  │    │ |
|  │  │  │  │                        │                              │ │  │    │ |
|  │  │  │  │  ┌─────────────────────▼─────────────────────────┐   │ │  │    │ |
|  │  │  │  │  │         Access Vector Cache (AVC)              │   │ │  │    │ |
|  │  │  │  │  │  (caches decisions for performance)            │   │ │  │    │ |
|  │  │  │  │  └─────────────────────┬─────────────────────────┘   │ │  │    │ |
|  │  │  │  │                        │                              │ │  │    │ |
|  │  │  │  └────────────────────────┼──────────────────────────────┘ │  │    │ |
|  │  │  │                           │                                │  │    │ |
|  │  │  │  ┌────────────────────────▼──────────────────────────────┐ │  │    │ |
|  │  │  │  │              SELinux Hook Functions                    │ │  │    │ |
|  │  │  │  │                                                        │ │  │    │ |
|  │  │  │  │  selinux_inode_permission(inode, mask)                 │ │  │    │ |
|  │  │  │  │  {                                                     │ │  │    │ |
|  │  │  │  │      sid = current_sid();           // Source context  │ │  │    │ |
|  │  │  │  │      isid = inode_sid(inode);       // Target context  │ │  │    │ |
|  │  │  │  │      return avc_has_perm(sid, isid, SECCLASS_FILE,     │ │  │    │ |
|  │  │  │  │                          mask);     // Check policy    │ │  │    │ |
|  │  │  │  │  }                                                     │ │  │    │ |
|  │  │  │  │                                                        │ │  │    │ |
|  │  │  │  └────────────────────────────────────────────────────────┘ │  │    │ |
|  │  │  │                                                             │  │    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘  │    │ |
|  │  │                                                                    │    │ |
|  │  │  MECHANISM: Hook functions, AVC, policy loading                    │    │ |
|  │  │  POLICY:    The allow/deny rules loaded from userspace             │    │ |
|  │  │                                                                    │    │ |
|  │  └────────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**策略与机制分离**：

**原则**：
- **机制**："如何强制访问控制" - 由 LSM 框架 + 内核钩子实现，固定编译进内核
- **策略**："允许/拒绝什么访问" - 由管理员定义，运行时可配置

**为什么分离**：不同部署需要不同策略
- **桌面**（宽松）：便利性 > 严格安全
- **服务器**（严格）：安全性 > 便利性
- **容器主机**（隔离）：每个容器完全隔离

**同一内核，不同策略！**

**SELinux 架构**：

**用户空间**：
- 策略（管理员定义）：`allow httpd_t httpd_content_t:file { read open };`
- 由 checkpolicy、semodule 编译

**内核**：
- SELinux 安全服务器
  - 策略数据库（从用户空间加载，存储在内核）
  - 访问向量缓存（AVC）（缓存决策以提高性能）
- SELinux 钩子函数
  - `selinux_inode_permission()` 调用 `avc_has_perm()` 检查策略

**机制**：钩子函数、AVC、策略加载
**策略**：从用户空间加载的 allow/deny 规则

---

## 3. 钩子生命周期

```
HOOK LIFECYCLE
+=============================================================================+
|                                                                              |
|  SECURITY BLOB ATTACHMENT                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  LSM needs to store security context with kernel objects:                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Object Creation:                                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  struct inode {                                             │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │      void *i_security;  ◄── LSM blob pointer                │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  struct task_struct {                                       │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │      void *security;    ◄── LSM blob pointer                │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  struct file {                                              │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │      void *f_security;  ◄── LSM blob pointer                │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Blob contains security labels:                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  SELinux blob for inode:                                    │ │    │ |
|  │  │  │  ┌───────────────────────────────────────────────────────┐ │ │    │ |
|  │  │  │  │  struct inode_security_struct {                        │ │ │    │ |
|  │  │  │  │      u32 sid;        // security ID (type)             │ │ │    │ |
|  │  │  │  │      u16 sclass;     // security class (file, dir...)  │ │ │    │ |
|  │  │  │  │      unsigned char initialized;                        │ │ │    │ |
|  │  │  │  │  };                                                    │ │ │    │ |
|  │  │  │  └───────────────────────────────────────────────────────┘ │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  SELinux blob for task:                                     │ │    │ |
|  │  │  │  ┌───────────────────────────────────────────────────────┐ │ │    │ |
|  │  │  │  │  struct task_security_struct {                         │ │ │    │ |
|  │  │  │  │      u32 osid;       // original SID                   │ │ │    │ |
|  │  │  │  │      u32 sid;        // current SID                    │ │ │    │ |
|  │  │  │  │      u32 exec_sid;   // SID on exec                    │ │ │    │ |
|  │  │  │  │  };                                                    │ │ │    │ |
|  │  │  │  └───────────────────────────────────────────────────────┘ │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  HOOK CALL FLOW                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ALLOCATION HOOKS (object creation):                             │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  inode_alloc_security()     - allocate inode blob          │ │    │ |
|  │  │  │  task_alloc()               - allocate task blob           │ │    │ |
|  │  │  │  file_alloc_security()      - allocate file blob           │ │    │ |
|  │  │  │  cred_alloc_blank()         - allocate credential blob     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Called when kernel creates new object                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  LABELING HOOKS (set security context):                          │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  inode_init_security()      - set label on new inode       │ │    │ |
|  │  │  │  d_instantiate()            - label from disk xattr        │ │    │ |
|  │  │  │  inode_setsecurity()        - set xattr label              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Label inherited from parent, policy, or xattr             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  ACCESS HOOKS (permission checks):                               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  inode_permission()         - read/write/exec checks       │ │    │ |
|  │  │  │  file_permission()          - per-file-descriptor checks   │ │    │ |
|  │  │  │  task_kill()                - signal permission            │ │    │ |
|  │  │  │  socket_connect()           - network connection           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Return 0 (allow) or -EACCES (deny)                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  FREE HOOKS (object destruction):                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  inode_free_security()      - free inode blob              │ │    │ |
|  │  │  │  task_free()                - free task blob               │ │    │ |
|  │  │  │  file_free_security()       - free file blob               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Called when kernel destroys object                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LSM STACKING                                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Multiple LSMs can be active simultaneously:                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  security_inode_permission()                                     │    │ |
|  │  │       │                                                          │    │ |
|  │  │       ├───► capability_inode_permission()  → 0 (allow)           │    │ |
|  │  │       │                                                          │    │ |
|  │  │       ├───► yama_inode_permission()        → 0 (allow)           │    │ |
|  │  │       │                                                          │    │ |
|  │  │       └───► selinux_inode_permission()     → -EACCES (deny)      │    │ |
|  │  │                                                                  │    │ |
|  │  │  RESULT: -EACCES (any deny = final deny)                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  ═══════════════════════════════════════════════════════════    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Hook aggregation rules:                                         │    │ |
|  │  │  • Restrictive: ALL must allow (deny wins)                       │    │ |
|  │  │  • Most security hooks are restrictive                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**钩子生命周期**：

**安全 Blob 附加**：

LSM 需要与内核对象存储安全上下文：
- `struct inode` 有 `i_security` 指针
- `struct task_struct` 有 `security` 指针
- `struct file` 有 `f_security` 指针

Blob 包含安全标签：
- SELinux inode blob：sid（类型）、sclass（安全类）
- SELinux task blob：osid、sid、exec_sid

**钩子调用流程**：

1. **分配钩子**（对象创建）：
   - `inode_alloc_security()`、`task_alloc()`、`file_alloc_security()`

2. **标记钩子**（设置安全上下文）：
   - `inode_init_security()`、`d_instantiate()`、`inode_setsecurity()`

3. **访问钩子**（权限检查）：
   - `inode_permission()`、`file_permission()`、`task_kill()`
   - 返回 0（允许）或 -EACCES（拒绝）

4. **释放钩子**（对象销毁）：
   - `inode_free_security()`、`task_free()`、`file_free_security()`

**LSM 堆叠**：
- 多个 LSM 可以同时活动
- 钩子聚合规则：限制性（所有必须允许，拒绝获胜）
- 示例：capability、yama 允许，但 selinux 拒绝 → 最终拒绝
