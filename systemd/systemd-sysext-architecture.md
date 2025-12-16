# Systemd System Extension (Sysext) Architecture

## Table of Contents
1. [Overview](#overview)
2. [Core Architecture](#core-architecture)
3. [Design Principles](#design-principles)
4. [How Sysext Works](#how-sysext-works)
5. [Integration with Yocto/BitBake](#integration-with-yoctobitbake)
6. [Build Process in BitBake](#build-process-in-bitbake)
7. [Runtime Behavior](#runtime-behavior)
8. [Practical Examples](#practical-examples)

---

## Overview

### What is Systemd-Sysext?

```
+============================================================================+
||                    SYSTEM EXTENSION (SYSEXT) CONCEPT                     ||
+============================================================================+
|                                                                            |
|   PROBLEM: How to extend an immutable/read-only OS image?                  |
|   =========================================================                |
|                                                                            |
|   Traditional Approach (Problematic):                                      |
|   +---------------------------+                                            |
|   | Base OS Image             |                                            |
|   | (read-only squashfs)      |   Cannot modify!                           |
|   |                           |   Need to rebuild entire image             |
|   | /usr/bin/...              |   for any change                           |
|   | /usr/lib/...              |                                            |
|   +---------------------------+                                            |
|                                                                            |
|   SOLUTION: System Extensions                                              |
|   ============================                                             |
|                                                                            |
|   +---------------------------+     +---------------------------+          |
|   | Base OS Image             |     | Extension Image           |          |
|   | (immutable)               |  +  | (sysext.squashfs)         |          |
|   |                           |     |                           |          |
|   | /usr/bin/base-tools       |     | /usr/bin/gdb              |          |
|   | /usr/lib/base-libs        |     | /usr/lib/gdb-libs         |          |
|   +---------------------------+     +---------------------------+          |
|                |                              |                            |
|                +------------+  +--------------+                            |
|                             |  |                                           |
|                             v  v                                           |
|                    +---------------------------+                           |
|                    | Merged View (OverlayFS)   |                           |
|                    |                           |                           |
|                    | /usr/bin/base-tools  ✓    |                           |
|                    | /usr/bin/gdb         ✓    |                           |
|                    | /usr/lib/base-libs   ✓    |                           |
|                    | /usr/lib/gdb-libs    ✓    |                           |
|                    +---------------------------+                           |
|                                                                            |
+============================================================================+
```

**中文说明：**
Systemd-sysext（系统扩展）解决了一个核心问题：如何在不修改只读/不可变基础系统镜像的情况下扩展系统功能。

传统方法的问题：
- 基础系统镜像通常是只读的（如使用 squashfs）
- 任何修改都需要重新构建整个镜像
- 无法在运行时动态添加软件包

Sysext 解决方案：
- 将扩展内容打包为独立的 squashfs 镜像
- 运行时通过 OverlayFS 将扩展镜像与基础镜像合并
- 用户看到的是统一的文件系统视图
- 可以动态添加/移除扩展，无需重启或重建基础镜像

---

## Core Architecture

### System Extension Architecture Overview

```
+============================================================================+
||                      SYSEXT CORE ARCHITECTURE                            ||
+============================================================================+
|                                                                            |
|                           USER SPACE VIEW                                  |
|   +--------------------------------------------------------------------+  |
|   |                    Unified Filesystem                               |  |
|   |                                                                     |  |
|   |   /usr/                                                             |  |
|   |   ├── bin/          (merged from base + extensions)                 |  |
|   |   ├── lib/          (merged from base + extensions)                 |  |
|   |   ├── lib64/        (merged from base + extensions)                 |  |
|   |   ├── share/        (merged from base + extensions)                 |  |
|   |   └── lib/extension-release.d/  (extension metadata)                |  |
|   |                                                                     |  |
|   +--------------------------------------------------------------------+  |
|                                    ^                                       |
|                                    | OverlayFS Mount                       |
|                                    |                                       |
|   +--------------------------------------------------------------------+  |
|   |                     SYSTEMD-SYSEXT DAEMON                          |  |
|   |                                                                     |  |
|   |  +------------------+  +------------------+  +------------------+   |  |
|   |  | Extension        |  | Extension        |  | Extension        |   |  |
|   |  | Discovery        |  | Validation       |  | Activation       |   |  |
|   |  +------------------+  +------------------+  +------------------+   |  |
|   |          |                     |                     |              |  |
|   |          v                     v                     v              |  |
|   |  - Scan directories    - Check metadata      - Mount squashfs       |  |
|   |  - Find .sysext files  - Verify compatibility- Setup overlay        |  |
|   |  - Parse names         - Check signatures    - Merge hierarchies    |  |
|   |                                                                     |  |
|   +--------------------------------------------------------------------+  |
|                                    ^                                       |
|                                    |                                       |
|   +--------------------------------------------------------------------+  |
|   |                    EXTENSION STORAGE LOCATIONS                     |  |
|   |                                                                     |  |
|   |  /usr/lib/extensions/        (immutable, built into image)          |  |
|   |  ├── base-utils.sysext.squashfs                                     |  |
|   |  └── network-tools.sysext.squashfs                                  |  |
|   |                                                                     |  |
|   |  /var/lib/extensions/        (mutable, runtime additions)           |  |
|   |  ├── debug-tools.sysext.squashfs                                    |  |
|   |  └── custom-app.sysext.squashfs                                     |  |
|   |                                                                     |  |
|   |  /run/extensions/            (temporary, cleared on reboot)         |  |
|   |  └── temp-debug.sysext.squashfs                                     |  |
|   |                                                                     |  |
|   +--------------------------------------------------------------------+  |
|                                                                            |
+============================================================================+
```

**中文说明：**
Sysext 核心架构包含三个主要层次：

1. **用户空间视图**：
   - 用户和应用程序看到的是统一的文件系统
   - `/usr/` 目录下的内容来自基础镜像和所有激活的扩展的合并
   - 扩展元数据存储在 `/usr/lib/extension-release.d/`

2. **Systemd-sysext 守护进程**：
   - **扩展发现**：扫描指定目录，查找 `.sysext` 文件
   - **扩展验证**：检查元数据、验证兼容性、检查签名
   - **扩展激活**：挂载 squashfs、设置 overlay、合并文件层次

3. **扩展存储位置**：
   - `/usr/lib/extensions/`：不可变位置，构建时内置的扩展
   - `/var/lib/extensions/`：可变位置，运行时添加的扩展
   - `/run/extensions/`：临时位置，重启后清除

### Extension File Structure

```
+============================================================================+
||                    EXTENSION FILE INTERNAL STRUCTURE                     ||
+============================================================================+
|                                                                            |
|   gdb-debug.sysext.squashfs                                                |
|   ========================                                                 |
|                                                                            |
|   +--------------------------------------------------------------------+  |
|   | SQUASHFS CONTAINER (compressed, read-only)                         |  |
|   |                                                                     |  |
|   |   /                                                                 |  |
|   |   └── usr/                                                          |  |
|   |       ├── bin/                                                      |  |
|   |       │   ├── gdb                    (executable)                   |  |
|   |       │   ├── gdbserver               (executable)                   |  |
|   |       │   └── gcore                   (executable)                   |  |
|   |       │                                                             |  |
|   |       ├── lib/                                                      |  |
|   |       │   ├── libgdb.so              (shared library)               |  |
|   |       │   └── gdb/                                                  |  |
|   |       │       └── python/            (GDB python scripts)           |  |
|   |       │                                                             |  |
|   |       ├── share/                                                    |  |
|   |       │   └── gdb/                                                  |  |
|   |       │       └── syscalls/          (syscall definitions)          |  |
|   |       │                                                             |  |
|   |       └── lib/                                                      |  |
|   |           └── extension-release.d/                                  |  |
|   |               └── extension-release.gdb-debug   (REQUIRED!)         |  |
|   |                                                                     |  |
|   +--------------------------------------------------------------------+  |
|                                                                            |
|   EXTENSION-RELEASE FILE CONTENTS:                                         |
|   =================================                                        |
|                                                                            |
|   +--------------------------------------------------------------------+  |
|   | # /usr/lib/extension-release.d/extension-release.gdb-debug         |  |
|   |                                                                     |  |
|   | ID=_any                      # Compatible with any distro           |  |
|   | # OR                                                                |  |
|   | ID=vecima                    # Specific distro ID                   |  |
|   | VERSION_ID=1.0               # Distro version                       |  |
|   |                                                                     |  |
|   | SYSEXT_LEVEL=1.0             # Extension API level                  |  |
|   | SYSEXT_SCOPE=system          # system, portable, or initrd          |  |
|   | ARCHITECTURE=x86-64          # Target architecture                  |  |
|   +--------------------------------------------------------------------+  |
|                                                                            |
|   NAMING CONVENTION:                                                       |
|   ==================                                                       |
|                                                                            |
|   <name>.<variant>.sysext.squashfs                                         |
|     │       │        │       │                                             |
|     │       │        │       └── Compression format                        |
|     │       │        └── Sysext marker (required)                          |
|     │       └── Optional variant (e.g., vserver-host)                      |
|     └── Extension name (e.g., gdb-debug)                                   |
|                                                                            |
|   Examples:                                                                |
|   - gdb-debug.sysext.squashfs                                              |
|   - vserver-utilities-vserver-host.sysext.squashfs                         |
|   - network-tools.aarch64.sysext.squashfs                                  |
|                                                                            |
+============================================================================+
```

**中文说明：**
扩展文件的内部结构：

1. **Squashfs 容器**：
   - 使用 squashfs 格式，压缩且只读
   - 内部结构必须以 `/usr/` 开头（sysext 只能扩展 `/usr/` 目录）
   - 包含可执行文件、库文件、数据文件等

2. **extension-release 文件**（必需）：
   - 位置：`/usr/lib/extension-release.d/extension-release.<name>`
   - `ID`：目标发行版标识，`_any` 表示兼容所有发行版
   - `VERSION_ID`：发行版版本
   - `SYSEXT_LEVEL`：扩展 API 级别
   - `SYSEXT_SCOPE`：扩展范围（system/portable/initrd）
   - `ARCHITECTURE`：目标架构

3. **命名规范**：
   - 格式：`<name>.<variant>.sysext.squashfs`
   - `.sysext` 标记是必需的
   - 变体（variant）是可选的，用于区分不同配置

---

## Design Principles

### Core Design Philosophy

```
+============================================================================+
||                       SYSEXT DESIGN PRINCIPLES                           ||
+============================================================================+
|                                                                            |
|   PRINCIPLE 1: IMMUTABILITY                                                |
|   ==========================                                               |
|                                                                            |
|   +-------------------------+     +-------------------------+              |
|   | Base System             |     | Extension               |              |
|   | (NEVER modified)        |     | (NEVER modified)        |              |
|   |                         |     |                         |              |
|   | Read-only squashfs      |     | Read-only squashfs      |              |
|   | Verified integrity      |     | Verified integrity      |              |
|   +-------------------------+     +-------------------------+              |
|              │                              │                              |
|              └──────────────┬───────────────┘                              |
|                             │                                              |
|                             v                                              |
|                   +-------------------+                                    |
|                   | OverlayFS         |                                    |
|                   | (Read-only merge) |                                    |
|                   +-------------------+                                    |
|                                                                            |
|   Benefits:                                                                |
|   - Atomic updates (replace entire extension)                              |
|   - Easy rollback (remove extension, refresh)                              |
|   - Integrity verification (no runtime modifications)                      |
|   - Reproducible systems (same inputs = same outputs)                      |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   PRINCIPLE 2: COMPOSABILITY                                               |
|   ===========================                                              |
|                                                                            |
|   +-------+  +-------+  +-------+  +-------+                               |
|   | Base  |  | Ext A |  | Ext B |  | Ext C |                               |
|   | Image |  | (gdb) |  | (perf)|  | (app) |                               |
|   +-------+  +-------+  +-------+  +-------+                               |
|       │          │          │          │                                   |
|       │          │          │          │                                   |
|       v          v          v          v                                   |
|   +--------------------------------------------------+                     |
|   |              Compose as needed                   |                     |
|   |                                                  |                     |
|   | Production:  Base                                |                     |
|   | Development: Base + Ext A + Ext B                |                     |
|   | Testing:     Base + Ext A + Ext B + Ext C        |                     |
|   +--------------------------------------------------+                     |
|                                                                            |
|   Benefits:                                                                |
|   - Mix and match extensions                                               |
|   - Different configurations for different environments                    |
|   - No need to rebuild base image                                          |
|   - Smaller download sizes (only get what you need)                        |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   PRINCIPLE 3: ISOLATION                                                   |
|   ======================                                                   |
|                                                                            |
|   Each extension is:                                                       |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |  ┌─────────────────────────────────────────────────────────────┐ |    |
|   |  │ Self-contained                                               │ |    |
|   |  │ - All dependencies included OR                               │ |    |
|   |  │ - Dependencies in base image OR                              │ |    |
|   |  │ - Dependencies in another extension                          │ |    |
|   |  └─────────────────────────────────────────────────────────────┘ |    |
|   |                                                                   |    |
|   |  ┌─────────────────────────────────────────────────────────────┐ |    |
|   |  │ Versioned independently                                      │ |    |
|   |  │ - Extension version != base image version                    │ |    |
|   |  │ - Can update extension without updating base                 │ |    |
|   |  └─────────────────────────────────────────────────────────────┘ |    |
|   |                                                                   |    |
|   |  ┌─────────────────────────────────────────────────────────────┐ |    |
|   |  │ Removable without side effects                               │ |    |
|   |  │ - Removing extension doesn't break base system               │ |    |
|   |  │ - Clean activation/deactivation                              │ |    |
|   |  └─────────────────────────────────────────────────────────────┘ |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   PRINCIPLE 4: SECURITY                                                    |
|   =====================                                                    |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |                     Security Layers                               |    |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |   Layer 1: Read-only filesystem                                   |    |
|   |   +---------------------------------------------------------+    |    |
|   |   | Extensions cannot be modified at runtime                 |    |    |
|   |   | Prevents malware persistence                             |    |    |
|   |   +---------------------------------------------------------+    |    |
|   |                                                                   |    |
|   |   Layer 2: Metadata validation                                    |    |
|   |   +---------------------------------------------------------+    |    |
|   |   | extension-release file must match system                 |    |    |
|   |   | Architecture and version checks                          |    |    |
|   |   +---------------------------------------------------------+    |    |
|   |                                                                   |    |
|   |   Layer 3: Optional cryptographic signing                         |    |
|   |   +---------------------------------------------------------+    |    |
|   |   | dm-verity for integrity verification                     |    |    |
|   |   | Signature verification before activation                 |    |    |
|   |   +---------------------------------------------------------+    |    |
|   |                                                                   |    |
|   |   Layer 4: Namespace restrictions                                 |    |
|   |   +---------------------------------------------------------+    |    |
|   |   | Only /usr/ hierarchy can be extended                     |    |    |
|   |   | Cannot modify /etc/, /var/, /home/                       |    |    |
|   |   +---------------------------------------------------------+    |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
Sysext 的核心设计原则：

**原则 1：不可变性（Immutability）**
- 基础系统和扩展都是只读的 squashfs 镜像
- 通过 OverlayFS 只读合并，运行时不修改任何原始文件
- 好处：原子更新、易于回滚、完整性验证、可重现系统

**原则 2：可组合性（Composability）**
- 扩展可以按需组合
- 不同环境可使用不同组合（生产环境最小化，开发环境添加调试工具）
- 无需重建基础镜像，只下载需要的扩展

**原则 3：隔离性（Isolation）**
- 每个扩展自包含（包含所有依赖或依赖基础镜像/其他扩展）
- 独立版本控制（扩展版本与基础镜像版本无关）
- 可安全移除（移除扩展不会破坏基础系统）

**原则 4：安全性（Security）**
- 只读文件系统：运行时无法修改，防止恶意软件持久化
- 元数据验证：检查 extension-release 文件与系统匹配
- 可选加密签名：dm-verity 完整性验证
- 命名空间限制：只能扩展 `/usr/` 目录，不能修改 `/etc/`、`/var/`

---

## How Sysext Works

### Activation Process Flow

```
+============================================================================+
||                     SYSEXT ACTIVATION PROCESS                            ||
+============================================================================+
|                                                                            |
|   STEP 1: DISCOVERY                                                        |
|   ==================                                                       |
|                                                                            |
|   systemd-sysext refresh                                                   |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | Scan extension directories:                                       |    |
|   |                                                                   |    |
|   | /usr/lib/extensions/                                              |    |
|   |    └── found: base-utils.sysext.squashfs                          |    |
|   |                                                                   |    |
|   | /var/lib/extensions/                                              |    |
|   |    └── found: gdb-debug.sysext.squashfs                           |    |
|   |                                                                   |    |
|   | /run/extensions/                                                  |    |
|   |    └── (empty)                                                    |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   STEP 2: VALIDATION                                                       |
|   ===================                                                      |
|                                                                            |
|   For each extension found:                                                |
|   +------------------------------------------------------------------+    |
|   | 1. Mount squashfs temporarily                                     |    |
|   |    $ mount -t squashfs gdb-debug.sysext.squashfs /tmp/validate    |    |
|   |                                                                   |    |
|   | 2. Read extension-release file                                    |    |
|   |    $ cat /tmp/validate/usr/lib/extension-release.d/extension-...  |    |
|   |                                                                   |    |
|   | 3. Validate against system os-release                             |    |
|   |    +----------------------------+                                 |    |
|   |    | extension-release:         |                                 |    |
|   |    | ID=vecima                  |    Must match or be _any        |    |
|   |    | VERSION_ID=1.0             |    Must be compatible           |    |
|   |    | ARCHITECTURE=x86-64        |    Must match system arch       |    |
|   |    +----------------------------+                                 |    |
|   |             vs                                                    |    |
|   |    +----------------------------+                                 |    |
|   |    | /etc/os-release:           |                                 |    |
|   |    | ID=vecima                  |                                 |    |
|   |    | VERSION_ID=1.0             |                                 |    |
|   |    +----------------------------+                                 |    |
|   |                                                                   |    |
|   | 4. If validation fails, skip extension with warning               |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   STEP 3: PREPARE HIERARCHY                                                |
|   ==========================                                               |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | Create mount structure:                                           |    |
|   |                                                                   |    |
|   | /run/systemd/sysext/                                              |    |
|   | ├── extensions/                                                   |    |
|   | │   ├── base-utils/        (squashfs mount point)                 |    |
|   | │   │   └── usr/...                                               |    |
|   | │   └── gdb-debug/         (squashfs mount point)                 |    |
|   | │       └── usr/...                                               |    |
|   | └── usr/                   (overlay work/upper dirs)              |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   STEP 4: MOUNT OVERLAY                                                    |
|   ======================                                                   |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | OverlayFS mount command (conceptual):                             |    |
|   |                                                                   |    |
|   | mount -t overlay overlay \                                        |    |
|   |   -o lowerdir=/usr:\                       # Base system /usr     |    |
|   |              /run/systemd/sysext/extensions/base-utils/usr:\      |    |
|   |              /run/systemd/sysext/extensions/gdb-debug/usr \       |    |
|   |   /usr                                     # Mount point          |    |
|   |                                                                   |    |
|   | Result: /usr now shows merged content from all layers             |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   STEP 5: FINALIZE                                                         |
|   ================                                                         |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | - Update /run/systemd/sysext/status                               |    |
|   | - Send notification to systemd                                    |    |
|   | - Extensions are now active and visible                           |    |
|   |                                                                   |    |
|   | Verification:                                                     |    |
|   | $ which gdb                                                       |    |
|   | /usr/bin/gdb                                                      |    |
|   |                                                                   |    |
|   | $ systemd-sysext status                                           |    |
|   | HIERARCHY EXTENSIONS SINCE                                        |    |
|   | /usr      base-utils  Mon 2024-01-15 10:00:00 UTC                 |    |
|   | /usr      gdb-debug   Mon 2024-01-15 10:00:05 UTC                 |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
Sysext 激活过程流程：

**步骤 1：发现（Discovery）**
- 执行 `systemd-sysext refresh` 命令
- 扫描三个扩展目录：`/usr/lib/extensions/`、`/var/lib/extensions/`、`/run/extensions/`
- 收集所有 `.sysext.squashfs` 文件

**步骤 2：验证（Validation）**
- 临时挂载每个 squashfs 文件
- 读取 `extension-release` 文件
- 与系统的 `/etc/os-release` 进行比对
- 检查 ID、VERSION_ID、ARCHITECTURE 是否匹配
- 验证失败的扩展会被跳过并记录警告

**步骤 3：准备层次结构（Prepare Hierarchy）**
- 在 `/run/systemd/sysext/` 创建挂载结构
- 每个扩展有独立的挂载点

**步骤 4：挂载 Overlay（Mount Overlay）**
- 使用 OverlayFS 将所有层合并
- 基础系统的 `/usr` 作为最底层
- 扩展按顺序叠加在上面
- 最终挂载到 `/usr`

**步骤 5：完成（Finalize）**
- 更新状态文件
- 通知 systemd
- 扩展现在可用，可以通过 `systemd-sysext status` 查看

### OverlayFS Merge Behavior

```
+============================================================================+
||                      OVERLAYFS MERGE BEHAVIOR                            ||
+============================================================================+
|                                                                            |
|   HOW FILES ARE MERGED:                                                    |
|   =====================                                                    |
|                                                                            |
|   Layer Stack (bottom to top):                                             |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | TOP: Extension B (gdb-debug)                                      |    |
|   |     /usr/bin/gdb           (provides GDB)                         |    |
|   |     /usr/bin/common-tool   (version 2.0)                          |    |
|   +------------------------------------------------------------------+    |
|                              ↑ (higher priority)                           |
|   +------------------------------------------------------------------+    |
|   | MID: Extension A (base-utils)                                     |    |
|   |     /usr/bin/ls            (provides ls)                          |    |
|   |     /usr/bin/common-tool   (version 1.0) ← SHADOWED by above      |    |
|   +------------------------------------------------------------------+    |
|                              ↑                                             |
|   +------------------------------------------------------------------+    |
|   | BOT: Base Image                                                   |    |
|   |     /usr/bin/sh            (base shell)                           |    |
|   |     /usr/lib/libc.so       (base libraries)                       |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   RESULTING MERGED VIEW:                                                   |
|   ======================                                                   |
|                                                                            |
|   /usr/bin/                                                                |
|   ├── sh              ← from Base Image                                    |
|   ├── ls              ← from Extension A                                   |
|   ├── gdb             ← from Extension B                                   |
|   └── common-tool     ← from Extension B (shadows Extension A)             |
|                                                                            |
|   /usr/lib/                                                                |
|   └── libc.so         ← from Base Image                                    |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   CONFLICT RESOLUTION:                                                     |
|   ====================                                                     |
|                                                                            |
|   When same file exists in multiple layers:                                |
|                                                                            |
|   +----------------------+     +----------------------+                    |
|   | Extension B          |     | Extension A          |                    |
|   | /usr/bin/tool v2.0   | WIN | /usr/bin/tool v1.0   | LOSE               |
|   +----------------------+     +----------------------+                    |
|            ↑                                                               |
|            │                                                               |
|            └── Upper layers always win (shadow lower layers)               |
|                                                                            |
|   Order matters! Extensions loaded later have higher priority.             |
|                                                                            |
|   Loading order typically:                                                 |
|   1. /usr/lib/extensions/*   (alphabetical)                                |
|   2. /var/lib/extensions/*   (alphabetical)                                |
|   3. /run/extensions/*       (alphabetical)                                |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   DIRECTORY MERGING:                                                       |
|   ==================                                                       |
|                                                                            |
|   Directories are merged, not shadowed:                                    |
|                                                                            |
|   Base:        /usr/share/doc/                                             |
|                ├── base-readme.txt                                         |
|                └── license.txt                                             |
|                                                                            |
|   Extension:   /usr/share/doc/                                             |
|                └── gdb-manual.txt                                          |
|                                                                            |
|   Merged:      /usr/share/doc/                                             |
|                ├── base-readme.txt    ← from base                          |
|                ├── license.txt        ← from base                          |
|                └── gdb-manual.txt     ← from extension                     |
|                                                                            |
+============================================================================+
```

**中文说明：**
OverlayFS 合并行为：

**文件合并方式**：
- 所有层从下到上堆叠
- 底层是基础镜像，上层是扩展
- 用户看到的是所有层合并后的视图

**冲突解决**：
- 当同一文件存在于多个层时，上层文件覆盖（遮蔽）下层文件
- 后加载的扩展优先级更高
- 加载顺序通常按目录和字母顺序：
  1. `/usr/lib/extensions/*`
  2. `/var/lib/extensions/*`
  3. `/run/extensions/*`

**目录合并**：
- 目录是合并的，不是遮蔽的
- 不同层的同名目录内容会合并显示
- 这允许多个扩展向同一目录添加不同文件

---

## Integration with Yocto/BitBake

### Yocto Sysext Integration Overview

```
+============================================================================+
||                  YOCTO/BITBAKE SYSEXT INTEGRATION                        ||
+============================================================================+
|                                                                            |
|   BUILD SYSTEM ARCHITECTURE:                                               |
|   ==========================                                               |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |                     YOCTO BUILD SYSTEM                            |    |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |  meta-layers/                                                     |    |
|   |  ├── meta-poky/              (base Yocto layer)                   |    |
|   |  ├── meta-oe/                (OpenEmbedded layer)                 |    |
|   |  ├── meta-custom/            (custom recipes)                     |    |
|   |  │   └── recipes-sysext/                                          |    |
|   |  │       ├── base-sysext/                                         |    |
|   |  │       │   └── base-sysext.bb                                   |    |
|   |  │       ├── debug-sysext/                                        |    |
|   |  │       │   └── debug-sysext.bb                                  |    |
|   |  │       └── sysext-image.bbclass                                 |    |
|   |  └── meta-bsp/               (BSP-specific layer)                 |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           │ bitbake <recipe>                                               |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   |                     BUILD PROCESS                                 |    |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |   1. Recipe Parsing                                               |    |
|   |      ├── Read recipe variables                                    |    |
|   |      ├── Resolve dependencies                                     |    |
|   |      └── Plan build tasks                                         |    |
|   |                                                                   |    |
|   |   2. Package Building                                             |    |
|   |      ├── do_compile: Build packages (gdb, strace, etc.)           |    |
|   |      ├── do_install: Install to staging                           |    |
|   |      └── do_package: Create package files                         |    |
|   |                                                                   |    |
|   |   3. Image/Sysext Creation                                        |    |
|   |      ├── do_rootfs: Assemble files into rootfs                    |    |
|   |      ├── do_image: Create image (squashfs)                        |    |
|   |      └── do_image_complete: Finalize output                       |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   |                     BUILD OUTPUT                                  |    |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |   tmp/deploy/images/<machine>/                                    |    |
|   |   ├── core-image-minimal.squashfs     (base OS image)             |    |
|   |   ├── base-sysext.sysext.squashfs     (base utilities ext)        |    |
|   |   ├── debug-sysext.sysext.squashfs    (debug tools ext)           |    |
|   |   └── manifest files, checksums, etc.                             |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
Yocto/BitBake 与 Sysext 的集成：

**构建系统架构**：
- 使用标准 Yocto 层结构
- 在自定义层（如 `meta-custom`）中添加 sysext 配方
- 包含 sysext 专用的 bbclass 文件定义构建逻辑

**构建过程**：
1. **配方解析**：读取变量、解析依赖、规划构建任务
2. **软件包构建**：编译、安装、打包各个软件包
3. **镜像/Sysext 创建**：组装 rootfs、创建 squashfs 镜像

**构建输出**：
- 基础系统镜像（如 `core-image-minimal.squashfs`）
- 各种 sysext 扩展（如 `debug-sysext.sysext.squashfs`）
- 清单文件和校验和

### Recipe Structure for Sysext

```
+============================================================================+
||                      SYSEXT RECIPE STRUCTURE                             ||
+============================================================================+
|                                                                            |
|   TYPICAL SYSEXT RECIPE:                                                   |
|   ======================                                                   |
|                                                                            |
|   # recipes-sysext/debug-sysext/debug-sysext.bb                            |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # -*- mode: bitbake -*-                                           |    |
|   |                                                                   |    |
|   | SUMMARY = "Debug tools system extension"                          |    |
|   | DESCRIPTION = "Provides GDB and debugging utilities"              |    |
|   | LICENSE = "MIT"                                                    |    |
|   |                                                                   |    |
|   | # Inherit sysext image class                                      |    |
|   | inherit sysext-image                                              |    |
|   |                                                                   |    |
|   | # Packages to include                                             |    |
|   | IMAGE_INSTALL = " \                                                |    |
|   |     gdb \                                                          |    |
|   |     gdbserver \                                                    |    |
|   |     strace \                                                       |    |
|   |     ltrace \                                                       |    |
|   | "                                                                  |    |
|   |                                                                   |    |
|   | # Extension metadata                                               |    |
|   | SYSEXT_NAME = "debug-tools"                                        |    |
|   | SYSEXT_ID = "_any"                                                 |    |
|   | SYSEXT_LEVEL = "1.0"                                               |    |
|   |                                                                   |    |
|   | # Output format                                                    |    |
|   | IMAGE_FSTYPES = "squashfs"                                         |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   SYSEXT-IMAGE.BBCLASS STRUCTURE:                                          |
|   ================================                                         |
|                                                                            |
|   # classes/sysext-image.bbclass                                           |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # Inherit base image class                                        |    |
|   | inherit image                                                      |    |
|   |                                                                   |    |
|   | # Default sysext settings                                          |    |
|   | SYSEXT_NAME ?= "${PN}"                                             |    |
|   | SYSEXT_ID ?= "_any"                                                |    |
|   | SYSEXT_LEVEL ?= "1.0"                                              |    |
|   | SYSEXT_SCOPE ?= "system"                                           |    |
|   |                                                                   |    |
|   | # Force rootfs structure for sysext                                |    |
|   | # (all files must be under /usr/)                                  |    |
|   | SYSEXT_ROOTFS_PREFIX = "/usr"                                      |    |
|   |                                                                   |    |
|   | # Create extension-release file                                    |    |
|   | python do_create_extension_release() {                             |    |
|   |     import os                                                      |    |
|   |     rootfs = d.getVar('IMAGE_ROOTFS')                              |    |
|   |     name = d.getVar('SYSEXT_NAME')                                  |    |
|   |     sysext_id = d.getVar('SYSEXT_ID')                               |    |
|   |     level = d.getVar('SYSEXT_LEVEL')                                |    |
|   |                                                                   |    |
|   |     release_dir = os.path.join(rootfs,                             |    |
|   |         'usr/lib/extension-release.d')                             |    |
|   |     os.makedirs(release_dir, exist_ok=True)                        |    |
|   |                                                                   |    |
|   |     release_file = os.path.join(release_dir,                       |    |
|   |         f'extension-release.{name}')                               |    |
|   |                                                                   |    |
|   |     with open(release_file, 'w') as f:                             |    |
|   |         f.write(f'ID={sysext_id}\n')                                |    |
|   |         f.write(f'SYSEXT_LEVEL={level}\n')                          |    |
|   | }                                                                   |    |
|   |                                                                   |    |
|   | addtask create_extension_release before do_image after do_rootfs   |    |
|   |                                                                   |    |
|   | # Rename output to .sysext.squashfs                                 |    |
|   | python do_rename_sysext() {                                        |    |
|   |     # Rename image.squashfs to image.sysext.squashfs               |    |
|   |     ...                                                            |    |
|   | }                                                                   |    |
|   |                                                                   |    |
|   | addtask rename_sysext after do_image before do_image_complete      |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
Sysext 配方结构：

**典型 Sysext 配方**：
- `SUMMARY/DESCRIPTION`：描述扩展用途
- `inherit sysext-image`：继承 sysext 镜像类
- `IMAGE_INSTALL`：指定要包含的软件包
- `SYSEXT_NAME/ID/LEVEL`：扩展元数据
- `IMAGE_FSTYPES`：输出格式（squashfs）

**sysext-image.bbclass 结构**：
- 继承基础 image 类
- 定义默认 sysext 设置
- `do_create_extension_release`：创建 extension-release 文件
- `do_rename_sysext`：将输出重命名为 `.sysext.squashfs` 格式
- 使用 `addtask` 将任务插入构建流程

---

## Build Process in BitBake

### Complete Build Flow for Sysext

```
+============================================================================+
||                    BITBAKE SYSEXT BUILD FLOW                             ||
+============================================================================+
|                                                                            |
|   $ bitbake debug-sysext                                                   |
|           │                                                                |
|           v                                                                |
|   PHASE 1: DEPENDENCY RESOLUTION                                           |
|   ================================                                         |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | BitBake resolves dependency tree:                                 |    |
|   |                                                                   |    |
|   |                    debug-sysext                                   |    |
|   |                         │                                         |    |
|   |         ┌───────────────┼───────────────┐                         |    |
|   |         │               │               │                         |    |
|   |         v               v               v                         |    |
|   |       gdb           strace          ltrace                        |    |
|   |         │               │               │                         |    |
|   |    ┌────┴────┐     ┌────┴────┐     ┌────┴────┐                    |    |
|   |    │         │     │         │     │         │                    |    |
|   |    v         v     v         v     v         v                    |    |
|   | python3  expat   libc     libc   libelf   libc                    |    |
|   |                                                                   |    |
|   | All dependencies must be built first                              |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   PHASE 2: PACKAGE BUILDING                                                |
|   =========================                                                |
|                                                                            |
|   For each package (gdb, strace, ltrace, etc.):                            |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | TASK: do_fetch                                                    |    |
|   | - Download source code                                            |    |
|   | - Verify checksums                                                 |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_unpack                                                   |    |
|   | - Extract source archive                                          |    |
|   | - Apply patches                                                   |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_configure                                                |    |
|   | - Run ./configure or cmake                                        |    |
|   | - Set build options                                               |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_compile                                                  |    |
|   | - Build source code                                               |    |
|   | - Generate binaries                                               |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_install                                                  |    |
|   | - Install to ${D} (staging directory)                             |    |
|   | - Create proper directory structure                               |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_package                                                  |    |
|   | - Split into packages (gdb, gdb-dbg, gdb-dev, etc.)               |    |
|   | - Generate package metadata                                       |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   PHASE 3: SYSEXT IMAGE CREATION                                           |
|   ==============================                                           |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | TASK: do_rootfs                                                   |    |
|   |                                                                   |    |
|   | 1. Create empty rootfs directory                                  |    |
|   |    mkdir -p ${IMAGE_ROOTFS}/usr/{bin,lib,share}                   |    |
|   |                                                                   |    |
|   | 2. Install selected packages                                      |    |
|   |    for pkg in IMAGE_INSTALL:                                      |    |
|   |        install_package(pkg, ${IMAGE_ROOTFS})                      |    |
|   |                                                                   |    |
|   | 3. Result:                                                        |    |
|   |    ${IMAGE_ROOTFS}/                                               |    |
|   |    └── usr/                                                       |    |
|   |        ├── bin/gdb                                                |    |
|   |        ├── bin/gdbserver                                          |    |
|   |        ├── bin/strace                                             |    |
|   |        ├── bin/ltrace                                             |    |
|   |        └── lib/...                                                |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_create_extension_release (from sysext-image.bbclass)     |    |
|   |                                                                   |    |
|   | Create: ${IMAGE_ROOTFS}/usr/lib/extension-release.d/              |    |
|   |                        extension-release.debug-tools              |    |
|   |                                                                   |    |
|   | Contents:                                                         |    |
|   | ID=_any                                                           |    |
|   | SYSEXT_LEVEL=1.0                                                  |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_image                                                    |    |
|   |                                                                   |    |
|   | Create squashfs image:                                            |    |
|   | mksquashfs ${IMAGE_ROOTFS} \                                      |    |
|   |            ${DEPLOY_DIR_IMAGE}/debug-sysext.squashfs \            |    |
|   |            -comp xz -noappend                                     |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_rename_sysext                                            |    |
|   |                                                                   |    |
|   | Rename to proper sysext naming:                                   |    |
|   | mv debug-sysext.squashfs debug-tools.sysext.squashfs              |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | TASK: do_image_complete                                           |    |
|   |                                                                   |    |
|   | Final output:                                                     |    |
|   | tmp/deploy/images/<machine>/debug-tools.sysext.squashfs           |    |
|   |                                                                   |    |
|   | Generate checksums and manifests                                  |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
BitBake Sysext 完整构建流程：

**阶段 1：依赖解析**
- BitBake 解析依赖树
- 所有依赖包必须先构建
- 例如：debug-sysext 依赖 gdb、strace、ltrace，它们又依赖 libc、python3 等

**阶段 2：软件包构建**
- `do_fetch`：下载源代码
- `do_unpack`：解压并打补丁
- `do_configure`：配置构建选项
- `do_compile`：编译源代码
- `do_install`：安装到暂存目录
- `do_package`：分割成多个包（主包、调试包、开发包等）

**阶段 3：Sysext 镜像创建**
- `do_rootfs`：创建 rootfs 目录结构，安装所选软件包
- `do_create_extension_release`：创建 extension-release 元数据文件
- `do_image`：使用 mksquashfs 创建压缩镜像
- `do_rename_sysext`：重命名为 `.sysext.squashfs` 格式
- `do_image_complete`：生成校验和和清单文件

### Build Configuration Options

```
+============================================================================+
||                    BUILD CONFIGURATION OPTIONS                           ||
+============================================================================+
|                                                                            |
|   LOCAL.CONF OPTIONS:                                                      |
|   ====================                                                     |
|                                                                            |
|   # build/conf/local.conf                                                  |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # Add GDB to specific sysext                                      |    |
|   | IMAGE_INSTALL:append:pn-debug-sysext = " gdb gdbserver"           |    |
|   |                                                                   |    |
|   | # Set sysext metadata globally                                    |    |
|   | SYSEXT_ID = "mycompany"                                           |    |
|   | SYSEXT_LEVEL = "2.0"                                              |    |
|   |                                                                   |    |
|   | # Squashfs compression settings                                   |    |
|   | EXTRA_IMAGECMD:squashfs = "-comp zstd -Xcompression-level 19"     |    |
|   |                                                                   |    |
|   | # Enable debug tweaks for sysext                                  |    |
|   | EXTRA_IMAGE_FEATURES:append:pn-debug-sysext = " debug-tweaks"     |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   DISTRO.CONF OPTIONS:                                                     |
|   =====================                                                    |
|                                                                            |
|   # meta-custom/conf/distro/mydistro.conf                                  |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # Default sysext settings for distro                              |    |
|   | DISTRO_SYSEXT_ID = "${DISTRO}"                                    |    |
|   | DISTRO_SYSEXT_VERSION = "${DISTRO_VERSION}"                       |    |
|   |                                                                   |    |
|   | # Default packages for all sysexts                                |    |
|   | SYSEXT_BASE_PACKAGES = "base-files"                               |    |
|   |                                                                   |    |
|   | # Signing configuration                                           |    |
|   | SYSEXT_SIGN_ENABLE = "1"                                          |    |
|   | SYSEXT_SIGN_KEY = "${TOPDIR}/signing-key.pem"                     |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   MACHINE.CONF OPTIONS:                                                    |
|   ======================                                                   |
|                                                                            |
|   # meta-bsp/conf/machine/mymachine.conf                                   |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # Machine-specific sysext architecture                            |    |
|   | SYSEXT_ARCHITECTURE = "x86-64"                                    |    |
|   |                                                                   |    |
|   | # Default sysexts to build for this machine                       |    |
|   | MACHINE_SYSEXT_PACKAGES = " \                                      |    |
|   |     base-sysext \                                                  |    |
|   |     network-sysext \                                               |    |
|   | "                                                                  |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
构建配置选项：

**local.conf 选项**：
- `IMAGE_INSTALL:append:pn-<recipe>`：向特定配方添加软件包
- `SYSEXT_ID/LEVEL`：全局设置扩展元数据
- `EXTRA_IMAGECMD:squashfs`：squashfs 压缩设置
- `EXTRA_IMAGE_FEATURES`：添加镜像特性（如调试）

**distro.conf 选项**：
- 定义发行版级别的 sysext 默认设置
- 配置签名选项

**machine.conf 选项**：
- 定义目标架构
- 指定该机器默认构建的 sysext 列表

---

## Runtime Behavior

### Sysext Lifecycle on Target System

```
+============================================================================+
||                      SYSEXT RUNTIME LIFECYCLE                            ||
+============================================================================+
|                                                                            |
|   BOOT SEQUENCE:                                                           |
|   ===============                                                          |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | 1. System Boot                                                    |    |
|   |    ├── Kernel loads                                               |    |
|   |    ├── initramfs runs                                             |    |
|   |    └── systemd starts as PID 1                                    |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | 2. Early Boot Services                                            |    |
|   |    ├── systemd-sysext.service starts                              |    |
|   |    ├── Ordered after: local-fs.target                             |    |
|   |    └── Ordered before: sysinit.target                             |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | 3. Sysext Activation                                              |    |
|   |    ├── Discover extensions in /usr/lib/extensions/                |    |
|   |    ├── Discover extensions in /var/lib/extensions/                |    |
|   |    ├── Validate all extensions                                    |    |
|   |    ├── Mount squashfs images                                      |    |
|   |    └── Create overlay on /usr                                     |    |
|   +------------------------------------------------------------------+    |
|           │                                                                |
|           v                                                                |
|   +------------------------------------------------------------------+    |
|   | 4. System Ready                                                   |    |
|   |    ├── All extensions active                                      |    |
|   |    ├── /usr shows merged content                                  |    |
|   |    └── Services can use extension binaries                        |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   RUNTIME OPERATIONS:                                                      |
|   ====================                                                     |
|                                                                            |
|   Add Extension at Runtime:                                                |
|   +------------------------------------------------------------------+    |
|   | # Copy new extension to mutable directory                         |    |
|   | $ cp new-tools.sysext.squashfs /var/lib/extensions/               |    |
|   |                                                                   |    |
|   | # Refresh extensions (re-scan and re-mount)                       |    |
|   | $ systemd-sysext refresh                                          |    |
|   |                                                                   |    |
|   | # Verify                                                          |    |
|   | $ systemd-sysext status                                           |    |
|   | HIERARCHY EXTENSIONS  SINCE                                       |    |
|   | /usr      base-tools  Mon 2024-01-15 10:00:00 UTC                 |    |
|   | /usr      new-tools   Mon 2024-01-15 15:30:00 UTC  <-- NEW        |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   Remove Extension at Runtime:                                             |
|   +------------------------------------------------------------------+    |
|   | # Remove extension file                                           |    |
|   | $ rm /var/lib/extensions/debug-tools.sysext.squashfs              |    |
|   |                                                                   |    |
|   | # Refresh to deactivate                                           |    |
|   | $ systemd-sysext refresh                                          |    |
|   |                                                                   |    |
|   | # Or use unmerge to deactivate all                                |    |
|   | $ systemd-sysext unmerge                                          |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   Temporary Extension (for debugging):                                     |
|   +------------------------------------------------------------------+    |
|   | # Add to /run/extensions/ (cleared on reboot)                     |    |
|   | $ cp debug.sysext.squashfs /run/extensions/                       |    |
|   | $ systemd-sysext refresh                                          |    |
|   |                                                                   |    |
|   | # After reboot, extension is gone automatically                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   MANAGEMENT COMMANDS:                                                     |
|   ====================                                                     |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # View current status                                             |    |
|   | $ systemd-sysext status                                           |    |
|   |                                                                   |    |
|   | # List available extensions                                       |    |
|   | $ systemd-sysext list                                             |    |
|   |                                                                   |    |
|   | # Refresh (re-scan and re-mount)                                  |    |
|   | $ systemd-sysext refresh                                          |    |
|   |                                                                   |    |
|   | # Merge extensions (activate)                                     |    |
|   | $ systemd-sysext merge                                            |    |
|   |                                                                   |    |
|   | # Unmerge extensions (deactivate all)                             |    |
|   | $ systemd-sysext unmerge                                          |    |
|   |                                                                   |    |
|   | # Show details of specific extension                              |    |
|   | $ systemd-analyze cat-config extension-release.debug-tools        |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
Sysext 运行时生命周期：

**启动顺序**：
1. 系统启动，内核加载，initramfs 运行，systemd 启动
2. `systemd-sysext.service` 在 `local-fs.target` 之后、`sysinit.target` 之前启动
3. 发现、验证、挂载所有扩展，创建 overlay
4. 系统就绪，所有扩展激活

**运行时操作**：
- **添加扩展**：复制到 `/var/lib/extensions/`，运行 `systemd-sysext refresh`
- **移除扩展**：删除文件，运行 `refresh` 或 `unmerge`
- **临时扩展**：放入 `/run/extensions/`，重启后自动清除

**管理命令**：
- `status`：查看当前状态
- `list`：列出可用扩展
- `refresh`：重新扫描并挂载
- `merge`：激活扩展
- `unmerge`：停用所有扩展

---

## Practical Examples

### Example 1: Creating GDB Sysext for Debugging

```
+============================================================================+
||              EXAMPLE: CREATING GDB DEBUG SYSEXT                          ||
+============================================================================+
|                                                                            |
|   STEP 1: CREATE RECIPE                                                    |
|   ======================                                                   |
|                                                                            |
|   $ mkdir -p meta-custom/recipes-debug/gdb-sysext/                         |
|                                                                            |
|   # meta-custom/recipes-debug/gdb-sysext/gdb-sysext.bb                     |
|   +------------------------------------------------------------------+    |
|   | SUMMARY = "GDB Debug System Extension"                            |    |
|   | LICENSE = "MIT"                                                    |    |
|   | LIC_FILES_CHKSUM = "file://${COREBASE}/meta/COPYING.MIT;\          |    |
|   |                    md5=3da9cfbcb788c80a0384361b4de20420"           |    |
|   |                                                                   |    |
|   | inherit image                                                      |    |
|   |                                                                   |    |
|   | IMAGE_INSTALL = " \                                                |    |
|   |     gdb \                                                          |    |
|   |     gdbserver \                                                    |    |
|   |     libstdc++ \                                                    |    |
|   | "                                                                  |    |
|   |                                                                   |    |
|   | IMAGE_FSTYPES = "squashfs"                                         |    |
|   | IMAGE_ROOTFS_SIZE = "0"                                            |    |
|   | IMAGE_OVERHEAD_FACTOR = "1.0"                                      |    |
|   |                                                                   |    |
|   | # Create extension-release file                                    |    |
|   | create_extension_release() {                                       |    |
|   |     install -d ${IMAGE_ROOTFS}/usr/lib/extension-release.d        |    |
|   |     echo "ID=_any" > \                                             |    |
|   |       ${IMAGE_ROOTFS}/usr/lib/extension-release.d/extension-\     |    |
|   |       release.gdb-debug                                            |    |
|   |     echo "SYSEXT_LEVEL=1.0" >> \                                   |    |
|   |       ${IMAGE_ROOTFS}/usr/lib/extension-release.d/extension-\     |    |
|   |       release.gdb-debug                                            |    |
|   | }                                                                   |    |
|   |                                                                   |    |
|   | ROOTFS_POSTPROCESS_COMMAND += "create_extension_release;"          |    |
|   |                                                                   |    |
|   | # Rename output file                                               |    |
|   | python do_rename_sysext() {                                        |    |
|   |     import os, glob                                                |    |
|   |     deploy = d.getVar('DEPLOY_DIR_IMAGE')                          |    |
|   |     pn = d.getVar('PN')                                            |    |
|   |     for f in glob.glob(f"{deploy}/{pn}*.squashfs"):                |    |
|   |         newname = f.replace('.squashfs', '.sysext.squashfs')       |    |
|   |         os.rename(f, newname)                                      |    |
|   | }                                                                   |    |
|   | addtask rename_sysext after do_image_complete                      |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   STEP 2: BUILD SYSEXT                                                     |
|   ====================                                                     |
|                                                                            |
|   $ cd poky                                                                |
|   $ source oe-init-build-env                                               |
|   $ bitbake gdb-sysext                                                     |
|                                                                            |
|   Output:                                                                  |
|   tmp/deploy/images/<machine>/gdb-sysext.sysext.squashfs                   |
|                                                                            |
|   STEP 3: VERIFY CONTENTS                                                  |
|   =======================                                                  |
|                                                                            |
|   $ unsquashfs -l tmp/deploy/images/<machine>/gdb-sysext.sysext.squashfs   |
|   squashfs-root                                                            |
|   squashfs-root/usr                                                        |
|   squashfs-root/usr/bin                                                    |
|   squashfs-root/usr/bin/gdb                                                |
|   squashfs-root/usr/bin/gdbserver                                          |
|   squashfs-root/usr/lib                                                    |
|   squashfs-root/usr/lib/extension-release.d                                |
|   squashfs-root/usr/lib/extension-release.d/extension-release.gdb-debug    |
|   ...                                                                      |
|                                                                            |
|   STEP 4: DEPLOY TO TARGET                                                 |
|   ========================                                                 |
|                                                                            |
|   $ scp gdb-sysext.sysext.squashfs target:/var/lib/extensions/             |
|   $ ssh target "systemd-sysext refresh"                                    |
|   $ ssh target "which gdb"                                                 |
|   /usr/bin/gdb                                                             |
|                                                                            |
+============================================================================+
```

**中文说明：**
示例 1：创建 GDB 调试 Sysext

**步骤 1：创建配方**
- 创建目录结构
- 编写 `.bb` 配方文件
- 指定要包含的软件包（gdb、gdbserver）
- 添加创建 extension-release 文件的任务
- 添加重命名输出文件的任务

**步骤 2：构建 Sysext**
- 初始化构建环境
- 运行 `bitbake gdb-sysext`
- 输出在 `tmp/deploy/images/` 目录

**步骤 3：验证内容**
- 使用 `unsquashfs -l` 查看 squashfs 内容
- 确认包含必要的文件和 extension-release

**步骤 4：部署到目标设备**
- 复制到 `/var/lib/extensions/`
- 运行 `systemd-sysext refresh`
- 验证 gdb 可用

### Example 2: Adding Package to Existing Sysext

```
+============================================================================+
||           EXAMPLE: ADDING PACKAGE TO EXISTING SYSEXT                     ||
+============================================================================+
|                                                                            |
|   SCENARIO: Add strace to vserver-utilities sysext                         |
|                                                                            |
|   METHOD 1: Using local.conf                                               |
|   ===========================                                              |
|                                                                            |
|   # build/conf/local.conf                                                  |
|   +------------------------------------------------------------------+    |
|   | # Append strace to vserver-utilities sysext                       |    |
|   | IMAGE_INSTALL:append:pn-vserver-utilities-sysext = " strace"      |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   $ bitbake vserver-utilities-sysext                                       |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   METHOD 2: Using bbappend file                                            |
|   =============================                                            |
|                                                                            |
|   # meta-custom/recipes-sysext/vserver-utilities-sysext/                   |
|   #            vserver-utilities-sysext.bbappend                           |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # Add debug tools to vserver-utilities                            |    |
|   | IMAGE_INSTALL += " \                                               |    |
|   |     strace \                                                       |    |
|   |     gdb \                                                          |    |
|   |     gdbserver \                                                    |    |
|   | "                                                                  |    |
|   |                                                                   |    |
|   | # Add debug packages for better debugging                          |    |
|   | IMAGE_INSTALL += " \                                               |    |
|   |     ${@bb.utils.contains('DISTRO_FEATURES', 'debug', \            |    |
|   |         'gdb-dbg strace-dbg', '', d)} \                            |    |
|   | "                                                                  |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   $ bitbake vserver-utilities-sysext                                       |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   METHOD 3: Create separate debug sysext (recommended)                     |
|   =====================================================                    |
|                                                                            |
|   Instead of modifying base utilities, create dedicated debug sysext:      |
|                                                                            |
|   $ mkdir -p meta-custom/recipes-debug/debug-tools-sysext/                 |
|                                                                            |
|   # meta-custom/recipes-debug/debug-tools-sysext/debug-tools-sysext.bb     |
|   +------------------------------------------------------------------+    |
|   | SUMMARY = "Debug Tools System Extension"                          |    |
|   | LICENSE = "MIT"                                                    |    |
|   | ...                                                                |    |
|   |                                                                   |    |
|   | IMAGE_INSTALL = " \                                                |    |
|   |     gdb \                                                          |    |
|   |     gdbserver \                                                    |    |
|   |     strace \                                                       |    |
|   |     ltrace \                                                       |    |
|   |     valgrind \                                                     |    |
|   |     perf \                                                         |    |
|   | "                                                                  |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   Benefits of separate sysext:                                             |
|   - Base sysext stays small for production                                 |
|   - Debug sysext only deployed when needed                                 |
|   - Easy to add/remove debug capabilities                                  |
|                                                                            |
+============================================================================+
```

**中文说明：**
示例 2：向现有 Sysext 添加软件包

**方法 1：使用 local.conf**
- 最快的方式，适合临时测试
- 在 `IMAGE_INSTALL:append:pn-<recipe>` 中添加软件包

**方法 2：使用 bbappend 文件**
- 更正式的方式，适合长期维护
- 创建 `.bbappend` 文件扩展原配方
- 可以使用条件判断（如检查 DISTRO_FEATURES）

**方法 3：创建独立的调试 sysext（推荐）**
- 最佳实践：保持基础 sysext 精简
- 调试工具放在独立的 sysext 中
- 仅在需要时部署调试 sysext
- 易于添加/移除调试能力

---

## Summary

```
+============================================================================+
||                              SUMMARY                                     ||
+============================================================================+
|                                                                            |
|   KEY TAKEAWAYS:                                                           |
|   ===============                                                          |
|                                                                            |
|   1. WHAT IS SYSEXT?                                                       |
|      - Systemd feature for extending immutable OS images                   |
|      - Uses squashfs + OverlayFS to merge extensions with base system      |
|      - Only extends /usr/ hierarchy                                        |
|                                                                            |
|   2. DESIGN PRINCIPLES                                                     |
|      - Immutability: Extensions are read-only                              |
|      - Composability: Mix and match extensions                             |
|      - Isolation: Each extension is self-contained                         |
|      - Security: Validation, signing, namespace restrictions               |
|                                                                            |
|   3. YOCTO INTEGRATION                                                     |
|      - Create sysext recipes inheriting image class                        |
|      - Add extension-release file creation task                            |
|      - Output .sysext.squashfs format                                      |
|                                                                            |
|   4. RUNTIME OPERATIONS                                                    |
|      - systemd-sysext refresh: Activate extensions                         |
|      - systemd-sysext unmerge: Deactivate extensions                       |
|      - Extensions in /var/lib/extensions/ persist across reboots           |
|      - Extensions in /run/extensions/ are temporary                        |
|                                                                            |
|   5. BEST PRACTICES                                                        |
|      - Keep base image minimal                                             |
|      - Create separate sysexts for different purposes                      |
|      - Use debug sysext only when needed                                   |
|      - Verify Build IDs for symbol files                                   |
|                                                                            |
+============================================================================+
|                                                                            |
|   QUICK REFERENCE:                                                         |
|   =================                                                        |
|                                                                            |
|   | Task                    | Command/Location                       |    |
|   |-------------------------|----------------------------------------|    |
|   | Build sysext            | bitbake <sysext-recipe>                |    |
|   | Output location         | tmp/deploy/images/<machine>/           |    |
|   | Deploy to target        | scp to /var/lib/extensions/            |    |
|   | Activate                | systemd-sysext refresh                 |    |
|   | Check status            | systemd-sysext status                  |    |
|   | Deactivate              | systemd-sysext unmerge                 |    |
|   | View contents           | unsquashfs -l <file>.sysext.squashfs   |    |
|                                                                            |
+============================================================================+
```

**中文总结：**

1. **什么是 Sysext？**
   - Systemd 提供的扩展不可变系统镜像的功能
   - 使用 squashfs + OverlayFS 合并扩展与基础系统
   - 仅能扩展 `/usr/` 目录层次

2. **设计原则**
   - 不可变性、可组合性、隔离性、安全性

3. **Yocto 集成**
   - 创建继承 image 类的 sysext 配方
   - 添加 extension-release 文件创建任务
   - 输出 `.sysext.squashfs` 格式

4. **运行时操作**
   - `refresh` 激活，`unmerge` 停用
   - `/var/lib/extensions/` 持久化，`/run/extensions/` 临时

5. **最佳实践**
   - 保持基础镜像最小化
   - 为不同用途创建独立 sysext
   - 仅在需要时使用调试 sysext

---

## Appendix A: How SquashFS Works

### SquashFS Overview

```
+============================================================================+
||                         SQUASHFS ARCHITECTURE                            ||
+============================================================================+
|                                                                            |
|   WHAT IS SQUASHFS?                                                        |
|   =================                                                        |
|                                                                            |
|   SquashFS is a compressed, read-only filesystem for Linux.                |
|   It is designed to be as compact as possible while maintaining            |
|   fast random access to files.                                             |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |                    SQUASHFS CHARACTERISTICS                       |    |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |   ✓ Read-only         - Cannot be modified after creation         |    |
|   |   ✓ Compressed        - Multiple compression algorithms           |    |
|   |   ✓ Block-based       - Random access without full decompression  |    |
|   |   ✓ Deduplicated      - Identical blocks stored once              |    |
|   |   ✓ Metadata cached   - Fast directory/file lookups               |    |
|   |   ✓ Sparse support    - Efficient handling of sparse files        |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   COMPARISON WITH OTHER FILESYSTEMS:                                       |
|   ==================================                                       |
|                                                                            |
|   +------------------+----------+-----------+------------+------------+   |
|   | Feature          | SquashFS | ext4      | EROFS      | CramFS     |   |
|   +------------------+----------+-----------+------------+------------+   |
|   | Read-only        | Yes      | No        | Yes        | Yes        |   |
|   | Compression      | Multiple | No        | LZ4/LZMA   | zlib       |   |
|   | Block size       | 4K-1MB   | 1K-64K    | 4K         | 4K         |   |
|   | Max file size    | 16 EiB   | 16 TiB    | 16 EiB     | 16 MiB     |   |
|   | Deduplication    | Yes      | No        | Yes        | No         |   |
|   | Xattr support    | Yes      | Yes       | Yes        | No         |   |
|   | NFS export       | Yes      | Yes       | Yes        | No         |   |
|   +------------------+----------+-----------+------------+------------+   |
|                                                                            |
+============================================================================+
```

**中文说明：**
SquashFS 概述：

**什么是 SquashFS？**
- SquashFS 是 Linux 的压缩只读文件系统
- 设计目标是尽可能紧凑，同时保持快速随机访问

**SquashFS 特性**：
- **只读**：创建后无法修改，保证数据完整性
- **压缩**：支持多种压缩算法
- **块级别**：无需完全解压即可随机访问
- **去重**：相同数据块只存储一次
- **元数据缓存**：快速的目录和文件查找
- **稀疏文件支持**：高效处理稀疏文件

### SquashFS Internal Structure

```
+============================================================================+
||                    SQUASHFS FILE FORMAT STRUCTURE                        ||
+============================================================================+
|                                                                            |
|   OVERALL FILE LAYOUT:                                                     |
|   ====================                                                     |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |                      SQUASHFS IMAGE FILE                          |    |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |   Offset 0                                                        |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                    SUPERBLOCK (96 bytes)                   |  |    |
|   |   |  - Magic number: 0x73717368 ("hsqs" little-endian)         |  |    |
|   |   |  - Inode count, block size, compression type               |  |    |
|   |   |  - Modification time, root inode reference                 |  |    |
|   |   |  - Byte counts for all sections                            |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                    COMPRESSION OPTIONS                     |  |    |
|   |   |  - Algorithm-specific parameters                           |  |    |
|   |   |  - Only present for certain compressors                    |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                      DATA BLOCKS                           |  |    |
|   |   |                                                            |  |    |
|   |   |  +-------+ +-------+ +-------+ +-------+ +-------+        |  |    |
|   |   |  |Block 0| |Block 1| |Block 2| |Block 3| |Block N|        |  |    |
|   |   |  |  LZ4  | |  LZ4  | |  LZ4  | | STORED| |  LZ4  |        |  |    |
|   |   |  | 32KB  | | 28KB  | | 45KB  | | 128KB | | 12KB  |        |  |    |
|   |   |  +-------+ +-------+ +-------+ +-------+ +-------+        |  |    |
|   |   |                                                            |  |    |
|   |   |  Each block: compressed independently (typ. 128KB)         |  |    |
|   |   |  Incompressible blocks stored uncompressed                 |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                    FRAGMENT BLOCKS                         |  |    |
|   |   |                                                            |  |    |
|   |   |  For files smaller than block size:                        |  |    |
|   |   |  +--------------------------------------------------+     |  |    |
|   |   |  | Frag 0   | Frag 1   | Frag 2   | ... | Frag N    |     |  |    |
|   |   |  | file_a   | file_b   | file_c   |     | file_z    |     |  |    |
|   |   |  | (2KB)    | (500B)   | (15KB)   |     | (1KB)     |     |  |    |
|   |   |  +--------------------------------------------------+     |  |    |
|   |   |  Multiple small files packed into single blocks            |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                    INODE TABLE                             |  |    |
|   |   |                                                            |  |    |
|   |   |  +--------+ +--------+ +--------+ +--------+              |  |    |
|   |   |  | Inode  | | Inode  | | Inode  | | Inode  |              |  |    |
|   |   |  |  Type  | |  Type  | |  Type  | |  Type  |              |  |    |
|   |   |  |  File  | |  Dir   | |Symlink | |  File  |              |  |    |
|   |   |  | Perms  | | Perms  | | Perms  | | Perms  |              |  |    |
|   |   |  | Size   | | Count  | | Target | | Size   |              |  |    |
|   |   |  | Blocks | | Offset | | ...    | | Blocks |              |  |    |
|   |   |  +--------+ +--------+ +--------+ +--------+              |  |    |
|   |   |                                                            |  |    |
|   |   |  Compressed metadata blocks                                |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                   DIRECTORY TABLE                          |  |    |
|   |   |                                                            |  |    |
|   |   |  +--------------------------------------------------+     |  |    |
|   |   |  | Dir Header | Entry 1  | Entry 2  | ... | Entry N |     |  |    |
|   |   |  | (count,    | (name,   | (name,   |     | (name,  |     |  |    |
|   |   |  |  start)    |  inode)  |  inode)  |     |  inode) |     |  |    |
|   |   |  +--------------------------------------------------+     |  |    |
|   |   |                                                            |  |    |
|   |   |  Sorted by name for binary search                          |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                   FRAGMENT TABLE                           |  |    |
|   |   |  - Maps fragment index to block location                   |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                    EXPORT TABLE                            |  |    |
|   |   |  - For NFS export support                                  |  |    |
|   |   |  - Maps inode number to inode location                     |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                     UID/GID TABLE                          |  |    |
|   |   |  - Deduplicated user/group IDs                             |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                              |                                    |    |
|   |                              v                                    |    |
|   |   +-----------------------------------------------------------+  |    |
|   |   |                    XATTR TABLE                             |  |    |
|   |   |  - Extended attributes storage                             |  |    |
|   |   +-----------------------------------------------------------+  |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
SquashFS 内部文件格式结构：

**整体布局**：
1. **超级块（Superblock）**：96 字节
   - 魔数：0x73717368（"hsqs" 小端序）
   - inode 数量、块大小、压缩类型
   - 修改时间、根 inode 引用
   - 各段的字节计数

2. **压缩选项**：算法特定参数

3. **数据块（Data Blocks）**：
   - 每个块独立压缩（通常 128KB）
   - 不可压缩的块以原始形式存储

4. **片段块（Fragment Blocks）**：
   - 用于小于块大小的文件
   - 多个小文件打包到单个块中

5. **Inode 表**：文件元数据（类型、权限、大小等）

6. **目录表**：按名称排序，支持二分查找

7. **其他表**：片段表、导出表、UID/GID 表、扩展属性表

### SquashFS Compression Algorithms

```
+============================================================================+
||                   SQUASHFS COMPRESSION ALGORITHMS                        ||
+============================================================================+
|                                                                            |
|   SUPPORTED COMPRESSORS:                                                   |
|   ======================                                                   |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |    Algorithm    |  Ratio  |  Speed   |  Memory  |   Use Case      |   |
|   +-----------------+---------+----------+----------+-----------------+   |
|   | GZIP (zlib)     |  Good   |  Medium  |   Low    | General purpose |   |
|   | LZ4             |  Low    |  Fast    |   Low    | Speed priority  |   |
|   | LZ4_HC          |  Medium |  Slow    |   Low    | Better ratio    |   |
|   | LZMA            |  Best   |  Slow    |  High    | Size priority   |   |
|   | XZ              |  Best   |  Slow    |  High    | Archival        |   |
|   | ZSTD            |  Great  |  Fast    |  Medium  | Best balance    |   |
|   +-----------------+---------+----------+----------+-----------------+   |
|                                                                            |
|   COMPRESSION LEVEL IMPACT:                                                |
|   =========================                                                |
|                                                                            |
|   Example: 1GB source directory                                            |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | Compressor | Level | Size    | Create Time | Mount Speed         |   |
|   +------------+-------+---------+-------------+---------------------+   |
|   | GZIP       |   1   | 450 MB  |    15 sec   |  Fast               |   |
|   | GZIP       |   9   | 380 MB  |    90 sec   |  Fast               |   |
|   | LZ4        |   -   | 520 MB  |     5 sec   |  Very Fast          |   |
|   | ZSTD       |   3   | 400 MB  |    10 sec   |  Very Fast          |   |
|   | ZSTD       |  19   | 320 MB  |   180 sec   |  Fast               |   |
|   | XZ         |   6   | 280 MB  |   300 sec   |  Medium             |   |
|   | XZ         |   9   | 260 MB  |   600 sec   |  Medium             |   |
|   +------------+-------+---------+-------------+---------------------+   |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   BLOCK-LEVEL COMPRESSION:                                                 |
|   ========================                                                 |
|                                                                            |
|   Why block-level matters for random access:                               |
|                                                                            |
|   Traditional archive (tar.gz):                                            |
|   +------------------------------------------------------------------+    |
|   |  [Compressed Stream - must decompress from beginning]             |    |
|   |  File A -> File B -> File C -> File D -> File E -> File F        |    |
|   |  ^                                       ^                        |    |
|   |  |                                       |                        |    |
|   |  Start                           Want File D?                     |    |
|   |                                  Must decompress A, B, C first!   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   SquashFS (block-based):                                                  |
|   +------------------------------------------------------------------+    |
|   |  +--------+ +--------+ +--------+ +--------+ +--------+          |    |
|   |  |Block 0 | |Block 1 | |Block 2 | |Block 3 | |Block 4 |          |    |
|   |  |File A  | |File A  | |File B  | |File C,D| |File E,F|          |    |
|   |  |(part)  | |(part)  | |        | |        | |        |          |    |
|   |  +--------+ +--------+ +--------+ +--------+ +--------+          |    |
|   |                                       ^                          |    |
|   |                                       |                          |    |
|   |                              Want File D?                        |    |
|   |                              Only decompress Block 3!            |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   Block size trade-offs:                                                   |
|   +------------------------------------------------------------------+    |
|   |  Block Size  |  Compression Ratio  |  Random Access Speed        |   |
|   +--------------+---------------------+-----------------------------+   |
|   |    4 KB      |  Poor (low context) |  Excellent (small blocks)   |   |
|   |   32 KB      |  Good               |  Very Good                  |   |
|   |  128 KB      |  Very Good          |  Good (default)             |   |
|   |  256 KB      |  Excellent          |  Medium                     |   |
|   |    1 MB      |  Best               |  Poor (large blocks)        |   |
|   +--------------+---------------------+-----------------------------+   |
|                                                                            |
+============================================================================+
```

**中文说明：**
SquashFS 压缩算法：

**支持的压缩器**：
| 算法 | 压缩率 | 速度 | 内存 | 适用场景 |
|------|--------|------|------|----------|
| GZIP | 良好 | 中等 | 低 | 通用 |
| LZ4 | 较低 | 极快 | 低 | 速度优先 |
| ZSTD | 优秀 | 快 | 中等 | 最佳平衡 |
| XZ | 最佳 | 慢 | 高 | 存档/大小优先 |

**块级压缩的重要性**：
- 传统归档（如 tar.gz）必须从头解压到目标文件
- SquashFS 每个块独立压缩，只需解压包含目标数据的块
- 这使得随机访问非常高效

**块大小权衡**：
- 小块（4KB）：压缩率差，但随机访问快
- 大块（1MB）：压缩率好，但随机访问慢
- 默认 128KB 是较好的平衡点

### SquashFS Creation and Usage

```
+============================================================================+
||                   SQUASHFS CREATION AND USAGE                            ||
+============================================================================+
|                                                                            |
|   CREATING SQUASHFS:                                                       |
|   ==================                                                       |
|                                                                            |
|   Basic creation:                                                          |
|   +------------------------------------------------------------------+     |
|   | # Create from directory                                           |    |
|   | $ mksquashfs /path/to/source output.squashfs                      |    |
|   |                                                                   |    |
|   | # With specific compression                                       |    |
|   | $ mksquashfs source/ output.squashfs -comp zstd                   |    |
|   |                                                                   |    |
|   | # With compression level                                          |    |
|   | $ mksquashfs source/ output.squashfs -comp zstd \                 |    |
|   |              -Xcompression-level 19                               |    |
|   |                                                                   |    |
|   | # With specific block size                                        |    |
|   | $ mksquashfs source/ output.squashfs -b 256K                      |    |
|   |                                                                   |    |
|   | # Exclude files                                                   |    |
|   | $ mksquashfs source/ output.squashfs -e '*.log' -e 'tmp/*'        |    |
|   |                                                                   |    |
|   | # Preserve extended attributes                                    |    |
|   | $ mksquashfs source/ output.squashfs -xattrs                      |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   ADVANCED OPTIONS:                                                        |
|   =================                                                        |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # All files owned by root                                         |    |
|   | $ mksquashfs source/ output.squashfs -all-root                    |    |
|   |                                                                   |    |
|   | # Append to existing squashfs                                     |    |
|   | $ mksquashfs newfiles/ existing.squashfs                          |    |
|   |                                                                   |    |
|   | # Create reproducible image (fixed timestamps)                    |    |
|   | $ mksquashfs source/ output.squashfs \                            |    |
|   |              -mkfs-time 0 -all-time 0                             |    |
|   |                                                                   |    |
|   | # Set number of parallel compression threads                      |    |
|   | $ mksquashfs source/ output.squashfs -processors 8                |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   MOUNTING SQUASHFS:                                                       |
|   ==================                                                       |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # Standard mount                                                  |    |
|   | $ mount -t squashfs image.squashfs /mnt/point                     |    |
|   |                                                                   |    |
|   | # Mount with loop device (usually automatic)                      |    |
|   | $ mount -o loop image.squashfs /mnt/point                         |    |
|   |                                                                   |    |
|   | # Mount read-only (implicit, but explicit)                        |    |
|   | $ mount -o ro -t squashfs image.squashfs /mnt/point               |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   INSPECTING SQUASHFS:                                                     |
|   ====================                                                     |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | # List contents without mounting                                  |    |
|   | $ unsquashfs -l image.squashfs                                    |    |
|   |                                                                   |    |
|   | # Extract contents                                                |    |
|   | $ unsquashfs image.squashfs                                       |    |
|   | # Creates squashfs-root/ directory                                |    |
|   |                                                                   |    |
|   | # Extract to specific directory                                   |    |
|   | $ unsquashfs -d /path/to/dest image.squashfs                      |    |
|   |                                                                   |    |
|   | # Extract specific files                                          |    |
|   | $ unsquashfs image.squashfs usr/bin/gdb                           |    |
|   |                                                                   |    |
|   | # Show filesystem statistics                                      |    |
|   | $ unsquashfs -s image.squashfs                                    |    |
|   | Found a valid SQUASHFS 4:0 superblock on image.squashfs.          |    |
|   | Creation or last append time: Mon Dec 15 10:00:00 2024            |    |
|   | Filesystem size: 45.23 Mbytes (46315.52 Kbytes)                   |    |
|   | Compression: zstd                                                 |    |
|   | Block size: 131072                                                |    |
|   | Number of inodes: 1523                                            |    |
|   | Number of fragments: 128                                          |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
SquashFS 创建和使用：

**创建 SquashFS**：
```bash
# 基本创建
mksquashfs /path/to/source output.squashfs

# 指定压缩算法
mksquashfs source/ output.squashfs -comp zstd

# 指定压缩级别
mksquashfs source/ output.squashfs -comp zstd -Xcompression-level 19

# 指定块大小
mksquashfs source/ output.squashfs -b 256K
```

**高级选项**：
- `-all-root`：所有文件归 root 所有
- `-mkfs-time 0`：创建可重现的镜像
- `-processors 8`：设置并行压缩线程数

**挂载 SquashFS**：
```bash
mount -t squashfs image.squashfs /mnt/point
```

**查看 SquashFS**：
- `unsquashfs -l`：列出内容（无需挂载）
- `unsquashfs -s`：显示文件系统统计信息
- `unsquashfs`：提取内容

---

## Appendix B: How OverlayFS Works

### OverlayFS Overview

```
+============================================================================+
||                         OVERLAYFS ARCHITECTURE                           ||
+============================================================================+
|                                                                            |
|   WHAT IS OVERLAYFS?                                                       |
|   ==================                                                       |
|                                                                            |
|   OverlayFS is a union filesystem that combines multiple directory         |
|   trees into a single unified view. It's part of the Linux kernel          |
|   since version 3.18.                                                      |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |                    OVERLAYFS CHARACTERISTICS                      |    |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |   ✓ Union mount      - Merge multiple directories                 |    |
|   |   ✓ Copy-on-write    - Changes don't affect lower layers          |    |
|   |   ✓ Transparent      - Applications see single filesystem         |    |
|   |   ✓ Efficient        - No data duplication until write            |    |
|   |   ✓ In-kernel        - Native Linux support, fast                 |    |
|   |   ✓ Stackable        - Multiple lower layers supported            |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
|   BASIC CONCEPT:                                                           |
|   ==============                                                           |
|                                                                            |
|                           MERGED VIEW                                      |
|                      (what users/apps see)                                 |
|                    +---------------------+                                 |
|                    |      /merged/       |                                 |
|                    |  file_a  (upper)    |                                 |
|                    |  file_b  (lower)    |                                 |
|                    |  file_c  (lower)    |                                 |
|                    |  file_d  (upper)    |                                 |
|                    +---------------------+                                 |
|                              ^                                             |
|                              |                                             |
|           +-----------------+|+-----------------+                          |
|           |                  |                  |                          |
|   +-------v-------+  +-------v-------+  +-------v-------+                  |
|   |    UPPER      |  |    LOWER 1    |  |    LOWER 2    |                  |
|   |  (read-write) |  |  (read-only)  |  |  (read-only)  |                  |
|   +---------------+  +---------------+  +---------------+                  |
|   | file_a (new)  |  | file_b        |  | file_c        |                  |
|   | file_d (mod)  |  | file_d (orig) |  |               |                  |
|   | .wh.file_e    |  | file_e        |  |               |                  |
|   +---------------+  +---------------+  +---------------+                  |
|                                                                            |
|   - Upper layer: writable, stores modifications                            |
|   - Lower layers: read-only, original content                              |
|   - Merged view: transparent combination of all layers                     |
|                                                                            |
+============================================================================+
```

**中文说明：**
OverlayFS 概述：

**什么是 OverlayFS？**
- OverlayFS 是一种联合文件系统，将多个目录树合并为单一视图
- 自 Linux 内核 3.18 起成为内核的一部分

**OverlayFS 特性**：
- **联合挂载**：合并多个目录
- **写时复制**：修改不影响底层
- **透明性**：应用程序看到单一文件系统
- **高效**：写入前不复制数据
- **内核原生**：Linux 原生支持，性能好
- **可堆叠**：支持多个底层

**基本概念**：
- **上层（Upper）**：可读写，存储修改
- **下层（Lower）**：只读，原始内容
- **合并视图（Merged）**：所有层的透明组合

### OverlayFS Layer Operations

```
+============================================================================+
||                      OVERLAYFS LAYER OPERATIONS                          ||
+============================================================================+
|                                                                            |
|   OPERATION 1: READING FILES                                               |
|   ==========================                                               |
|                                                                            |
|   When reading a file:                                                     |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |                                                                   |    |
|   |   Application: open("/merged/config.txt")                         |    |
|   |                          |                                        |    |
|   |                          v                                        |    |
|   |   +--------------------------------------------------+           |    |
|   |   |  OverlayFS lookup sequence:                       |           |    |
|   |   |                                                   |           |    |
|   |   |  1. Check UPPER layer                             |           |    |
|   |   |     /upper/config.txt exists? ──┬── YES ──> Use it|           |    |
|   |   |                                 │                 |           |    |
|   |   |                                 NO                |           |    |
|   |   |                                 │                 |           |    |
|   |   |  2. Check LOWER layer 1         v                 |           |    |
|   |   |     /lower1/config.txt exists? ─┬── YES ──> Use it|           |    |
|   |   |                                 │                 |           |    |
|   |   |                                 NO                |           |    |
|   |   |                                 │                 |           |    |
|   |   |  3. Check LOWER layer 2         v                 |           |    |
|   |   |     /lower2/config.txt exists? ─┬── YES ──> Use it|           |    |
|   |   |                                 │                 |           |    |
|   |   |                                 NO                |           |    |
|   |   |                                 v                 |           |    |
|   |   |  4. Return ENOENT (file not found)                |           |    |
|   |   +--------------------------------------------------+            |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   OPERATION 2: CREATING NEW FILES                                          |
|   ================================                                         |
|                                                                            |
|   New files are always created in upper layer:                             |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |   Application: create("/merged/newfile.txt")                      |    |
|   |                          |                                        |    |
|   |                          v                                        |    |
|   |   +--------------------------------------------------+            |    |
|   |   |  1. File created directly in UPPER layer         |            |    |
|   |   |     /upper/newfile.txt                           |            |    |
|   |   |                                                  |            |    |
|   |   |  2. Visible immediately in merged view           |            |    |
|   |   |     /merged/newfile.txt                          |            |    |
|   |   |                                                   |           |    |
|   |   |  3. Lower layers unchanged                        |           |    |
|   |   +--------------------------------------------------+            |    |
|   |                                                                   |    |
|   |   BEFORE:                      AFTER:                             |    |
|   |   Upper: (empty)              Upper: newfile.txt                  |    |
|   |   Lower: file_a               Lower: file_a                       |    |
|   |   Merged: file_a              Merged: file_a, newfile.txt         |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   OPERATION 3: MODIFYING EXISTING FILES (COPY-UP)                          |
|   ================================================                         |
|                                                                            |
|   When modifying a file from lower layer:                                  |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |   Application: write("/merged/config.txt", data)                  |    |
|   |   (config.txt exists only in lower layer)                         |    |
|   |                          |                                        |    |
|   |                          v                                        |    |
|   |   +--------------------------------------------------+            |    |
|   |   |  COPY-UP PROCESS:                                 |           |    |
|   |   |                                                   |           |    |
|   |   |  1. Copy file from lower to upper                 |           |    |
|   |   |     /lower/config.txt -> /upper/config.txt        |           |    |
|   |   |                                                   |           |    |
|   |   |  2. Apply modification to upper copy              |           |    |
|   |   |     write(data) to /upper/config.txt              |           |    |
|   |   |                                                   |           |    |
|   |   |  3. Original lower file unchanged                 |           |    |
|   |   |     /lower/config.txt (still original)            |           |    |
|   |   |                                                   |           |    |
|   |   |  4. Merged view shows upper version               |           |    |
|   |   |     /merged/config.txt (modified)                 |           |    |
|   |   +--------------------------------------------------+            |    |
|   |                                                                   |    |
|   |   BEFORE:                      AFTER:                             |    |
|   |   Upper: (empty)              Upper: config.txt (modified)        |    |
|   |   Lower: config.txt (v1)      Lower: config.txt (v1, unchanged)   |    |
|   |   Merged: config.txt (v1)     Merged: config.txt (modified)       |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   OPERATION 4: DELETING FILES (WHITEOUT)                                   |
|   ======================================                                   |
|                                                                            |
|   When deleting a file from lower layer:                                   |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |   Application: unlink("/merged/oldfile.txt")                      |    |
|   |   (oldfile.txt exists in lower layer)                             |    |
|   |                          |                                        |    |
|   |                          v                                        |    |
|   |   +--------------------------------------------------+           |    |
|   |   |  WHITEOUT PROCESS:                                |           |    |
|   |   |                                                   |           |    |
|   |   |  1. Cannot delete from lower (read-only)          |           |    |
|   |   |                                                   |           |    |
|   |   |  2. Create WHITEOUT file in upper layer           |           |    |
|   |   |     /upper/.wh.oldfile.txt                        |           |    |
|   |   |     (character device with 0/0 major/minor)       |           |    |
|   |   |                                                   |           |    |
|   |   |  3. Whiteout "hides" the lower file               |           |    |
|   |   |     Merged view: file appears deleted             |           |    |
|   |   |                                                   |           |    |
|   |   |  4. Lower file still exists but invisible         |           |    |
|   |   +--------------------------------------------------+           |    |
|   |                                                                   |    |
|   |   BEFORE:                      AFTER:                             |    |
|   |   Upper: (empty)              Upper: .wh.oldfile.txt (whiteout)   |    |
|   |   Lower: oldfile.txt          Lower: oldfile.txt (unchanged)      |    |
|   |   Merged: oldfile.txt         Merged: (file hidden/deleted)       |    |
|   |                                                                   |    |
|   |   WHITEOUT FILE TYPES:                                            |    |
|   |   +------------------------------------------------------+       |    |
|   |   | .wh.<filename>     - Whiteout single file             |       |    |
|   |   | .wh..wh..opq       - Opaque directory (hide all below)|       |    |
|   |   +------------------------------------------------------+       |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
OverlayFS 层操作：

**操作 1：读取文件**
- 按顺序查找：先上层，再下层
- 找到即返回，找不到返回 ENOENT

**操作 2：创建新文件**
- 新文件直接在上层创建
- 下层不变
- 合并视图立即可见

**操作 3：修改现有文件（Copy-up）**
- 从下层复制文件到上层
- 在上层副本上应用修改
- 下层原文件不变
- 合并视图显示上层版本

**操作 4：删除文件（Whiteout）**
- 无法从只读下层删除
- 在上层创建 whiteout 文件（`.wh.<filename>`）
- Whiteout 文件"隐藏"下层文件
- 下层文件仍存在但不可见

### OverlayFS Mount Syntax

```
+============================================================================+
||                       OVERLAYFS MOUNT SYNTAX                             ||
+============================================================================+
|                                                                            |
|   MOUNT COMMAND STRUCTURE:                                                 |
|   ========================                                                 |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |  mount -t overlay overlay \                                       |    |
|   |        -o lowerdir=<lower>,upperdir=<upper>,workdir=<work> \      |    |
|   |        <mountpoint>                                               |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   MOUNT OPTIONS:                                                           |
|   ==============                                                           |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   | Option      | Required | Description                              |    |
|   +-------------+----------+------------------------------------------+    |
|   | lowerdir    | Yes      | Read-only lower layer(s), colon-separated|    |
|   | upperdir    | No*      | Read-write upper layer                   |    |
|   | workdir     | No*      | Work directory (same fs as upper)        |    |
|   | redirect_dir| No       | Enable directory rename support          |    |
|   | index       | No       | Enable inode index for hardlinks         |    |
|   | metacopy    | No       | Only copy metadata on copy-up            |    |
|   | volatile    | No       | Skip fsync (faster, less safe)           |    |
|   +-------------+----------+------------------------------------------+    |
|   | * Required for read-write mount                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   EXAMPLES:                                                                |
|   =========                                                                |
|                                                                            |
|   Read-write overlay (typical usage):                                      |
|   +------------------------------------------------------------------+     |
|   | # Setup directories                                               |    |
|   | $ mkdir -p /tmp/overlay/{lower,upper,work,merged}                 |    |
|   |                                                                   |    |
|   | # Populate lower layer                                            |    |
|   | $ echo "original" > /tmp/overlay/lower/file.txt                   |    |
|   |                                                                   |    |
|   | # Mount overlay                                                   |    |
|   | $ mount -t overlay overlay \                                      |    |
|   |         -o lowerdir=/tmp/overlay/lower,\                          |    |
|   |            upperdir=/tmp/overlay/upper,\                          |    |
|   |            workdir=/tmp/overlay/work \                            |    |
|   |         /tmp/overlay/merged                                       |    |
|   |                                                                   |    |
|   | # Verify                                                          |    |
|   | $ cat /tmp/overlay/merged/file.txt                                |    |
|   | original                                                          |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   Read-only overlay (multiple lower layers):                               |
|   +------------------------------------------------------------------+     |
|   | # For sysext: base + extension1 + extension2                      |    |
|   | $ mount -t overlay overlay \                                      |    |
|   |         -o lowerdir=/usr:\                                        |    |
|   |            /run/sysext/ext1/usr:\                                 |    |
|   |            /run/sysext/ext2/usr \                                 |    |
|   |         /usr                                                      |    |
|   |                                                                   |    |
|   | # Note: rightmost lowerdir has lowest priority                    |    |
|   | # Priority: /usr > ext1/usr > ext2/usr                            |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   Multiple lower layers (stacking):                                        |
|   +------------------------------------------------------------------+     |
|   | # Layer order (colon-separated, left = higher priority)           |    |
|   | $ mount -t overlay overlay \                                      |    |
|   |         -o lowerdir=/layer1:/layer2:/layer3,\                     |    |
|   |            upperdir=/upper,\                                      |    |
|   |            workdir=/work \                                        |    |
|   |         /merged                                                   |    |
|   |                                                                   |    |
|   | # Lookup order: upper -> layer1 -> layer2 -> layer3               |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
+============================================================================+
```

**中文说明：**
OverlayFS 挂载语法：

**挂载命令结构**：
```bash
mount -t overlay overlay \
      -o lowerdir=<lower>,upperdir=<upper>,workdir=<work> \
      <mountpoint>
```

**挂载选项**：
| 选项 | 必需 | 描述 |
|------|------|------|
| lowerdir | 是 | 只读下层，冒号分隔多层 |
| upperdir | 否* | 读写上层 |
| workdir | 否* | 工作目录，需与上层同文件系统 |
| redirect_dir | 否 | 启用目录重命名支持 |
| index | 否 | 启用硬链接索引 |

（* 读写挂载时必需）

**示例**：
- **读写 overlay**：典型用法，有上层和工作目录
- **只读 overlay**：多个下层（如 sysext），无上层
- **多层堆叠**：冒号分隔，左侧优先级更高

### OverlayFS Internal Mechanics

```
+============================================================================+
||                    OVERLAYFS INTERNAL MECHANICS                          ||
+============================================================================+
|                                                                            |
|   DIRECTORY HANDLING:                                                      |
|   ===================                                                      |
|                                                                            |
|   How directories are merged:                                              |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   |   UPPER /upper/dir/          LOWER /lower/dir/                    |    |
|   |   +------------------+       +------------------+                 |    |
|   |   | file_a.txt       |       | file_b.txt       |                 |    |
|   |   | file_c.txt       |       | file_c.txt       | <- shadowed    |    |
|   |   | subdir/          |       | file_d.txt       |                 |    |
|   |   +------------------+       +------------------+                 |    |
|   |                                                                   |    |
|   |                    MERGED /merged/dir/                            |    |
|   |                    +------------------+                           |    |
|   |                    | file_a.txt  (U)  |  <- from upper            |    |
|   |                    | file_b.txt  (L)  |  <- from lower            |    |
|   |                    | file_c.txt  (U)  |  <- upper shadows lower   |    |
|   |                    | file_d.txt  (L)  |  <- from lower            |    |
|   |                    | subdir/     (U)  |  <- from upper            |    |
|   |                    +------------------+                           |    |
|   |                                                                   |    |
|   |   Directory entries from all layers are combined                  |    |
|   |   Files with same name: upper shadows lower                       |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   OPAQUE DIRECTORIES:                                                      |
|   ===================                                                      |
|                                                                            |
|   When you want to completely replace a directory:                         |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |   SCENARIO: Replace /etc/app.d/ entirely                          |    |
|   |                                                                   |    |
|   |   LOWER /lower/etc/app.d/     UPPER /upper/etc/app.d/             |    |
|   |   +------------------+        +------------------+                |    |
|   |   | old_config1.conf |        | .wh..wh..opq     | <- opaque flag|     |
|   |   | old_config2.conf |        | new_config.conf  |                |    |
|   |   | old_config3.conf |        +------------------+                |    |
|   |   +------------------+                                            |    |
|   |                                                                   |    |
|   |                    MERGED /merged/etc/app.d/                      |    |
|   |                    +------------------+                           |    |
|   |                    | new_config.conf  |  <- only upper visible    |    |
|   |                    +------------------+                           |    |
|   |                                                                   |    |
|   |   The .wh..wh..opq file makes directory "opaque"                  |    |
|   |   All lower layer contents are hidden                             |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   WORKDIR INTERNALS:                                                       |
|   ==================                                                       |
|                                                                            |
|   The workdir is used for atomic operations:                               |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |   COPY-UP ATOMICITY:                                              |    |
|   |                                                                   |    |
|   |   1. Create temp file in workdir                                  |    |
|   |      /work/work/#123                                              |    |
|   |                                                                   |    |
|   |   2. Copy data from lower to temp                                 |    |
|   |      cp /lower/file.txt /work/work/#123                           |    |
|   |                                                                   |    |
|   |   3. Rename temp to upper (atomic)                                |    |
|   |      mv /work/work/#123 /upper/file.txt                           |    |
|   |                                                                   |    |
|   |   This ensures copy-up is atomic:                                 |    |
|   |   - Either complete or not started                                |    |
|   |   - No partial copies visible                                     |    |
|   |                                                                   |    |
|   |   REQUIREMENTS:                                                   |    |
|   |   - workdir must be on same filesystem as upperdir                |    |
|   |   - workdir must be empty before mount                            |    |
|   |   - workdir should not be accessed directly                       |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   INODE HANDLING:                                                          |
|   ===============                                                          |
|                                                                            |
|   How overlay presents inodes to applications:                             |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |   INODE MAPPING:                                                  |    |
|   |                                                                   |    |
|   |   Lower file inode: 12345 (on lower fs)                           |    |
|   |   Upper file inode: 67890 (on upper fs)                           |    |
|   |   Overlay inode:    ABCDE (synthesized)                           |    |
|   |                                                                   |    |
|   |   Applications see overlay inodes, not underlying inodes          |    |
|   |                                                                   |    |
|   |   COPY-UP INODE CHANGE:                                           |    |
|   |   Before copy-up: overlay inode based on lower                    |    |
|   |   After copy-up:  overlay inode based on upper                    |    |
|   |                                                                   |    |
|   |   WARNING: Inode number may change after copy-up!                 |    |
|   |   This can affect applications that cache inodes                  |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   PERFORMANCE CHARACTERISTICS:                                             |
|   ============================                                             |
|                                                                            |
|   +------------------------------------------------------------------+    |
|   | Operation         | Performance | Notes                           |   |
|   +-------------------+-------------+---------------------------------+   |
|   | Read (no copy-up) | Native      | Direct access to lower/upper    |   |
|   | Read (after copy) | Native      | Access to upper                 |   |
|   | Write new file    | Native      | Direct to upper                 |   |
|   | Write (copy-up)   | Slow first  | Copy whole file, then native    |   |
|   | Delete lower file | Fast        | Just create whiteout            |   |
|   | Directory listing | Medium      | Merge entries from all layers   |   |
|   | Stat              | Fast        | Cached overlay inode            |   |
|   +-------------------+-------------+---------------------------------+   |
|   |                                                                   |   |
|   | Best practices:                                                   |   |
|   | - Minimize copy-ups for large files                               |   |
|   | - Pre-copy files expected to be modified                          |   |
|   | - Use metacopy=on for metadata-only changes                       |   |
|   +------------------------------------------------------------------+    |
|                                                                            |
+============================================================================+
```

**中文说明：**
OverlayFS 内部机制：

**目录处理**：
- 来自所有层的目录条目被合并
- 同名文件：上层遮蔽下层
- 目录内容是所有层的联合

**不透明目录**：
- `.wh..wh..opq` 文件使目录"不透明"
- 所有下层内容被隐藏
- 用于完全替换目录内容

**工作目录内部**：
- 用于原子操作
- Copy-up 过程：先在 workdir 创建临时文件，完成后原子重命名到 upper
- 必须与 upperdir 在同一文件系统

**Inode 处理**：
- 应用程序看到的是 overlay 合成的 inode
- Copy-up 后 inode 可能改变
- 这可能影响缓存 inode 的应用程序

**性能特征**：
| 操作 | 性能 | 说明 |
|------|------|------|
| 读取（无 copy-up）| 原生 | 直接访问下层/上层 |
| 写入新文件 | 原生 | 直接写入上层 |
| 写入（需 copy-up）| 首次慢 | 需复制整个文件 |
| 删除下层文件 | 快 | 只需创建 whiteout |

### OverlayFS in Sysext Context

```
+============================================================================+
||                    OVERLAYFS IN SYSEXT CONTEXT                           ||
+============================================================================+
|                                                                            |
|   HOW SYSEXT USES OVERLAYFS:                                               |
|   ==========================                                               |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |   Sysext creates a READ-ONLY overlay (no upper/work):             |    |
|   |                                                                   |    |
|   |   mount -t overlay overlay \                                      |    |
|   |         -o lowerdir=/usr:\                      <- base system    |    |
|   |            /run/sysext/ext1/usr:\               <- extension 1    |    |
|   |            /run/sysext/ext2/usr \               <- extension 2    |    |
|   |         /usr                                    <- mount point    |    |
|   |                                                                   |    |
|   |   No upperdir/workdir = completely read-only                      |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   LAYER STACKING ORDER:                                                    |
|   =====================                                                    |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |   Priority (highest to lowest):                                   |    |
|   |                                                                   |    |
|   |   /usr (base) > /run/sysext/ext1/usr > /run/sysext/ext2/usr       |    |
|   |   ^^^^^^^^^^^^^                                                   |    |
|   |   Leftmost in lowerdir = highest priority                         |    |
|   |                                                                   |    |
|   |   Wait, isn't base system lowest priority?                        |    |
|   |   NO! In sysext, base system has HIGHEST priority!                |    |
|   |                                                                   |    |
|   |   This means:                                                     |    |
|   |   - Extensions CANNOT override base system files                  |    |
|   |   - Extensions can only ADD new files                             |    |
|   |   - This is a SECURITY feature                                    |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   VISUAL REPRESENTATION:                                                   |
|   ======================                                                   |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |   +-------------------------------------------+                   |    |
|   |   |          MERGED /usr (read-only)          |                   |    |
|   |   +-------------------------------------------+                   |    |
|   |                        ^                                          |    |
|   |                        | overlay mount                            |    |
|   |        +---------------+---------------+                          |    |
|   |        |               |               |                          |    |
|   |   +----v----+    +-----v-----+   +-----v-----+                    |    |
|   |   | Base    |    | Extension |   | Extension |                    |    |
|   |   | /usr    |    | 1 /usr    |   | 2 /usr    |                    |    |
|   |   | (prio 1)|    | (prio 2)  |   | (prio 3)  |                    |    |
|   |   +---------+    +-----------+   +-----------+                    |    |
|   |   | bin/    |    | bin/      |   | bin/      |                    |    |
|   |   |  ls     |    |  gdb      |   |  perf     |                    |    |
|   |   |  cat    |    |  strace   |   |           |                    |    |
|   |   | lib/    |    | lib/      |   | lib/      |                    |    |
|   |   |  libc   |    |  libgdb   |   |  libperf  |                    |    |
|   |   +---------+    +-----------+   +-----------+                    |    |
|   |                                                                   |    |
|   |   Result in merged /usr:                                          |    |
|   |   +-------------------------------------------+                   |    |
|   |   | bin/ls      (from base)                   |                   |    |
|   |   | bin/cat     (from base)                   |                   |    |
|   |   | bin/gdb     (from ext1 - NEW file)        |                   |    |
|   |   | bin/strace  (from ext1 - NEW file)        |                   |    |
|   |   | bin/perf    (from ext2 - NEW file)        |                   |    |
|   |   | lib/libc    (from base)                   |                   |    |
|   |   | lib/libgdb  (from ext1 - NEW file)        |                   |    |
|   |   | lib/libperf (from ext2 - NEW file)        |                   |    |
|   |   +-------------------------------------------+                   |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   WHAT HAPPENS IF EXTENSION TRIES TO OVERRIDE BASE FILE?                   |
|   ======================================================                   |
|                                                                            |
|   +------------------------------------------------------------------+     |
|   |                                                                   |    |
|   |   Base /usr:        Extension /usr:                               |    |
|   |   +-----------+     +-----------+                                 |    |
|   |   | bin/ls    |     | bin/ls    |  <- attacker tries to replace   |    |
|   |   | (v1.0)    |     | (malware) |                                 |    |
|   |   +-----------+     +-----------+                                 |    |
|   |                                                                   |    |
|   |   Result (base has higher priority):                              |    |
|   |   +-------------------------------------------+                   |    |
|   |   | bin/ls (v1.0 from base - SAFE!)           |                   |    |
|   |   +-------------------------------------------+                   |    |
|   |                                                                   |    |
|   |   The malicious extension's ls is IGNORED                         |    |
|   |   because base system has higher priority.                        |    |
|   |                                                                   |    |
|   |   This is why sysext is SECURE:                                   |    |
|   |   - Extensions can only ADD functionality                         |    |
|   |   - Cannot replace/override base system binaries                  |    |
|   |   - Cannot inject malicious versions of system tools              |    |
|   |                                                                   |    |
|   +------------------------------------------------------------------+     |
|                                                                            |
+============================================================================+
```

**中文说明：**
Sysext 中的 OverlayFS：

**Sysext 如何使用 OverlayFS**：
- 创建只读 overlay（无 upperdir/workdir）
- 基础系统 `/usr` 作为最高优先级层
- 扩展按顺序叠加在后面

**层堆叠顺序**：
- 优先级：基础系统 > 扩展1 > 扩展2
- **重要**：基础系统优先级最高
- 扩展只能添加新文件，不能覆盖基础系统文件
- 这是一个安全特性

**安全考虑**：
- 如果扩展试图覆盖基础系统文件（如 `/usr/bin/ls`）
- 基础系统版本会被使用，扩展版本被忽略
- 这防止恶意扩展替换系统工具

**这就是为什么 sysext 是安全的**：
- 扩展只能添加功能
- 无法替换/覆盖基础系统二进制文件
- 无法注入恶意版本的系统工具

---

## Appendix C: Quick Command Reference

```
+============================================================================+
||                      QUICK COMMAND REFERENCE                             ||
+============================================================================+
|                                                                            |
|   SQUASHFS COMMANDS:                                                       |
|   ==================                                                       |
|                                                                            |
|   # Create squashfs                                                        |
|   mksquashfs <source> <output.squashfs> [options]                          |
|     -comp <algo>      : Compression (gzip/lz4/zstd/xz)                     |
|     -b <size>         : Block size (4K-1M)                                 |
|     -Xcompression-level <n> : Compression level                            |
|     -all-root         : All files owned by root                            |
|     -noappend         : Don't append to existing squashfs                  |
|                                                                            |
|   # List contents                                                          |
|   unsquashfs -l <image.squashfs>                                           |
|                                                                            |
|   # Extract                                                                |
|   unsquashfs [-d <dest>] <image.squashfs> [files...]                       |
|                                                                            |
|   # Show stats                                                             |
|   unsquashfs -s <image.squashfs>                                           |
|                                                                            |
|   # Mount                                                                  |
|   mount -t squashfs <image.squashfs> <mountpoint>                          |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   OVERLAYFS COMMANDS:                                                      |
|   ===================                                                      |
|                                                                            |
|   # Read-write overlay                                                     |
|   mount -t overlay overlay \                                               |
|         -o lowerdir=<lower>,upperdir=<upper>,workdir=<work> \              |
|         <mountpoint>                                                       |
|                                                                            |
|   # Read-only overlay (multiple lowers)                                    |
|   mount -t overlay overlay \                                               |
|         -o lowerdir=<lower1>:<lower2>:<lower3> \                           |
|         <mountpoint>                                                       |
|                                                                            |
|   # Check overlay mounts                                                   |
|   mount | grep overlay                                                     |
|   cat /proc/mounts | grep overlay                                          |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   SYSEXT COMMANDS:                                                         |
|   ================                                                         |
|                                                                            |
|   # View status                                                            |
|   systemd-sysext status                                                    |
|                                                                            |
|   # List extensions                                                        |
|   systemd-sysext list                                                      |
|                                                                            |
|   # Activate extensions                                                    |
|   systemd-sysext refresh                                                   |
|   systemd-sysext merge                                                     |
|                                                                            |
|   # Deactivate extensions                                                  |
|   systemd-sysext unmerge                                                   |
|                                                                            |
|   --------------------------------------------------------------------------
|                                                                            |
|   YOCTO/BITBAKE COMMANDS:                                                  |
|   =======================                                                  |
|                                                                            |
|   # Build sysext                                                           |
|   bitbake <sysext-recipe>                                                  |
|                                                                            |
|   # Build with verbose output                                              |
|   bitbake <sysext-recipe> -v                                               |
|                                                                            |
|   # Clean and rebuild                                                      |
|   bitbake <sysext-recipe> -c cleansstate && bitbake <sysext-recipe>        |
|                                                                            |
|   # Find output                                                            |
|   find tmp/deploy/images -name "*.sysext.squashfs"                         |
|                                                                            |
+============================================================================+
```

**中文说明：**
快速命令参考：

**SquashFS 命令**：
- `mksquashfs`：创建 squashfs 镜像
- `unsquashfs -l`：列出内容
- `unsquashfs`：提取内容
- `unsquashfs -s`：显示统计信息
- `mount -t squashfs`：挂载

**OverlayFS 命令**：
- 读写 overlay：需要 lowerdir、upperdir、workdir
- 只读 overlay：只需多个 lowerdir
- `mount | grep overlay`：查看 overlay 挂载

**Sysext 命令**：
- `systemd-sysext status`：查看状态
- `systemd-sysext list`：列出扩展
- `systemd-sysext refresh`：刷新/激活
- `systemd-sysext unmerge`：停用

**Yocto/BitBake 命令**：
- `bitbake <recipe>`：构建
- `bitbake -c cleansstate`：清理并重建
- `find tmp/deploy/images -name "*.sysext.squashfs"`：查找输出

