# Section 4: Linux Primitives Behind Docker (Critical Section)

This section is the most important for truly understanding containers.

## 4.1 Namespaces

Namespaces provide **isolation** - they make a process believe it has its own instance of global system resources.

```
NAMESPACE OVERVIEW:
+==================================================================+
||                                                                ||
||  Without Namespaces (normal process):                          ||
||  +----------------------------------------------------------+  ||
||  |  Process sees:                                           |  ||
||  |  - ALL processes on system (ps aux)                      |  ||
||  |  - ALL network interfaces                                |  ||
||  |  - Real hostname                                         |  ||
||  |  - Global filesystem                                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  With Namespaces (containerized process):                      ||
||  +----------------------------------------------------------+  ||
||  |  Process sees:                                           |  ||
||  |  - Only its own processes (isolated PID space)           |  ||
||  |  - Only its network interfaces                           |  ||
||  |  - Container hostname                                    |  ||
||  |  - Container's filesystem view                           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  KERNEL creates illusion of dedicated system resources         ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Namespace概述：

无Namespace（普通进程）：进程看到系统上所有进程、所有网络接口、真实主机名、全局文件系统。

有Namespace（容器化进程）：进程只看到自己的进程（隔离的PID空间）、自己的网络接口、容器主机名、容器的文件系统视图。

内核创建专用系统资源的幻象。

### The Six (Seven) Namespaces

```
NAMESPACE TYPES:
+==================================================================+
||                                                                ||
||  NAMESPACE   ISOLATES                  DOCKER USE              ||
||  ----------  ------------------------  ----------------------- ||
||  PID         Process IDs               Container has PID 1     ||
||  Mount       Filesystem mounts         Container root fs       ||
||  Network     Network stack             Container network       ||
||  IPC         Inter-process comm        Shared memory, semaphores
||  UTS         Hostname, domain          Container hostname      ||
||  User        User/group IDs            UID mapping (optional)  ||
||  Cgroup*     Cgroup root               Cgroup isolation        ||
||                                                                ||
||  * Cgroup namespace added in Linux 4.6                         ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Namespace类型表：

| Namespace | 隔离什么 | Docker用途 |
|-----------|---------|------------|
| PID | 进程ID | 容器有自己的PID 1 |
| Mount | 文件系统挂载 | 容器根文件系统 |
| Network | 网络栈 | 容器网络 |
| IPC | 进程间通信 | 共享内存、信号量 |
| UTS | 主机名、域名 | 容器主机名 |
| User | 用户/组ID | UID映射（可选）|
| Cgroup* | Cgroup根 | Cgroup隔离 |

*Cgroup namespace在Linux 4.6添加

### PID Namespace

```
PID NAMESPACE:
+------------------------------------------------------------------+
|                                                                  |
|  HOST VIEW:                                                      |
|  +------------------------------------------------------------+  |
|  |  PID 1: /sbin/init (systemd)                               |  |
|  |  PID 2: kthreadd                                           |  |
|  |  ...                                                       |  |
|  |  PID 5000: containerd                                      |  |
|  |  PID 5100: container process (nginx)  <-- Real PID         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CONTAINER VIEW:                                                 |
|  +------------------------------------------------------------+  |
|  |  PID 1: nginx  <-- Same process, but sees itself as PID 1  |  |
|  |  PID 2: nginx worker                                       |  |
|  |  PID 3: nginx worker                                       |  |
|  |                                                            |  |
|  |  Cannot see host's PID 1, 2, or 5000                       |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  IMPLICATIONS:                                                   |
|  - Container has its own PID 1 (important for signals)           |
|  - kill inside container only affects container processes        |
|  - Host can still see and kill container processes               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

PID Namespace：

主机视图：PID 1是/sbin/init(systemd)、PID 5000是containerd、PID 5100是容器进程(nginx)——这是真实PID。

容器视图：PID 1是nginx——同一个进程但看自己为PID 1、PID 2和3是nginx worker。看不到主机的PID 1、2或5000。

含义：容器有自己的PID 1（对信号重要）、容器内的kill只影响容器进程、主机仍可看到并kill容器进程。

### Mount Namespace

```
MOUNT NAMESPACE:
+==================================================================+
||                                                                ||
||  HOST FILESYSTEM:                                              ||
||  /                                                             ||
||  ├── bin/                                                      ||
||  ├── etc/                                                      ||
||  ├── home/                                                     ||
||  ├── var/                                                      ||
||  │   └── lib/docker/overlay2/abc123/merged/  <-- Container fs  ||
||  └── ...                                                       ||
||                                                                ||
||  CONTAINER FILESYSTEM VIEW:                                    ||
||  /  <-- This is actually /var/lib/docker/overlay2/abc123/merged
||  ├── bin/     (from image)                                     ||
||  ├── etc/     (from image)                                     ||
||  ├── app/     (from image)                                     ||
||  └── tmp/     (container writable layer)                       ||
||                                                                ||
||  Container process does:                                       ||
||  open("/etc/passwd", ...)                                      ||
||                                                                ||
||  Kernel translates to:                                         ||
||  open("/var/lib/docker/overlay2/abc123/merged/etc/passwd", ...)
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Mount Namespace：

主机文件系统：/下有bin、etc、home、var等，容器文件系统在/var/lib/docker/overlay2/abc123/merged/。

容器文件系统视图：/实际是/var/lib/docker/overlay2/abc123/merged，包含来自镜像的bin、etc、app和容器可写层tmp。

容器进程执行open("/etc/passwd", ...)，内核翻译为open("/var/lib/docker/overlay2/abc123/merged/etc/passwd", ...)。

### Network Namespace

```
NETWORK NAMESPACE:
+==================================================================+
||                                                                ||
||  HOST NETWORK:                                                 ||
||  +----------------------------------------------------------+  ||
||  |  eth0: 192.168.1.100                                     |  ||
||  |  docker0: 172.17.0.1 (bridge)                            |  ||
||  |  vethXXXX: <connected to container>                      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CONTAINER NETWORK:                                            ||
||  +----------------------------------------------------------+  ||
||  |  eth0: 172.17.0.2 (different from host eth0!)            |  ||
||  |  lo: 127.0.0.1                                           |  ||
||  |                                                          |  ||
||  |  Container's eth0 is connected to host's vethXXXX        |  ||
||  |  via virtual ethernet pair                               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  NETWORK ISOLATION:                                            ||
||  +----------------------------------------------------------+  ||
||  |  Container cannot see host's real eth0                   |  ||
||  |  Container has own routing table                         |  ||
||  |  Container has own iptables rules                        |  ||
||  |  Container has own network ports (can both use port 80)  |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Network Namespace：

主机网络：eth0是192.168.1.100、docker0是172.17.0.1（网桥）、vethXXXX连接到容器。

容器网络：eth0是172.17.0.2（与主机eth0不同！）、lo是127.0.0.1。容器的eth0通过虚拟以太网对连接到主机的vethXXXX。

网络隔离：容器看不到主机真实eth0、容器有自己的路由表、容器有自己的iptables规则、容器有自己的网络端口（都可以使用端口80）。

### What Namespaces Do NOT Provide

```
NAMESPACE LIMITATIONS:
+------------------------------------------------------------------+
|                                                                  |
|  NAMESPACES DO NOT ISOLATE:                                      |
|                                                                  |
|  1. KERNEL                                                       |
|  +------------------------------------------------------------+  |
|  | - All containers share host kernel                        |  |
|  | - Kernel exploit in container = host compromised          |  |
|  | - syscalls go to the same kernel                          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  2. TIME                                                         |
|  +------------------------------------------------------------+  |
|  | - No time namespace until Linux 5.6                       |  |
|  | - System clock is shared                                  |  |
|  | - Container cannot have different time                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  3. KERNEL KEYRING                                               |
|  +------------------------------------------------------------+  |
|  | - Shared across all containers                            |  |
|  | - Cryptographic keys may leak                             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  4. /proc AND /sys (partially)                                   |
|  +------------------------------------------------------------+  |
|  | - Some procfs entries show host information               |  |
|  | - /proc/meminfo shows host memory                         |  |
|  | - CPU info may leak                                       |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Namespace限制——不隔离什么：

1. 内核：所有容器共享宿主机内核、容器中的内核漏洞利用=主机被攻破、syscalls去同一个内核。

2. 时间：Linux 5.6前无时间namespace、系统时钟共享、容器不能有不同时间。

3. 内核密钥环：跨所有容器共享、加密密钥可能泄露。

4. /proc和/sys（部分）：一些procfs条目显示主机信息、/proc/meminfo显示主机内存、CPU信息可能泄露。

---

## 4.2 Cgroups (Control Groups)

Cgroups provide **resource limits** - they control how much CPU, memory, and I/O a process can use.

```
CGROUPS OVERVIEW:
+==================================================================+
||                                                                ||
||  WITHOUT CGROUPS:                                              ||
||  +----------------------------------------------------------+  ||
||  |  Container process can:                                  |  ||
||  |  - Use 100% CPU (starve other processes)                 |  ||
||  |  - Allocate unlimited memory (OOM kill others)           |  ||
||  |  - Do unlimited disk I/O (slow down system)              |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  WITH CGROUPS:                                                 ||
||  +----------------------------------------------------------+  ||
||  |  Container process limited to:                           |  ||
||  |  - 50% CPU (or 0.5 CPUs)                                 |  ||
||  |  - 512 MB memory (killed if exceeded)                    |  ||
||  |  - 100 MB/s disk I/O                                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CGROUP HIERARCHY:                                             ||
||  +----------------------------------------------------------+  ||
||  |  /sys/fs/cgroup/                                         |  ||
||  |  ├── cpu/                                                |  ||
||  |  │   └── docker/                                         |  ||
||  |  │       └── <container-id>/                             |  ||
||  |  │           ├── cpu.shares                              |  ||
||  |  │           └── tasks (PIDs in this cgroup)             |  ||
||  |  ├── memory/                                             |  ||
||  |  │   └── docker/                                         |  ||
||  |  │       └── <container-id>/                             |  ||
||  |  │           ├── memory.limit_in_bytes                   |  ||
||  |  │           └── memory.usage_in_bytes                   |  ||
||  |  └── ...                                                 |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Cgroups概述：

无Cgroups：容器进程可以使用100% CPU（饿死其他进程）、分配无限内存（OOM kill其他进程）、无限磁盘I/O（拖慢系统）。

有Cgroups：容器进程限制为50% CPU（或0.5个CPU）、512MB内存（超过被kill）、100MB/s磁盘I/O。

Cgroup层次结构：/sys/fs/cgroup/下有cpu/、memory/等，每个下面有docker/<container-id>/目录，包含限制文件如cpu.shares、memory.limit_in_bytes。

### CPU Limits

```
CPU CGROUP CONTROLS:
+------------------------------------------------------------------+
|                                                                  |
|  CPU SHARES (relative weight):                                   |
|  +------------------------------------------------------------+  |
|  |  Container A: cpu.shares = 1024                           |  |
|  |  Container B: cpu.shares = 512                            |  |
|  |                                                           |  |
|  |  When competing for CPU:                                  |  |
|  |  A gets 2x more CPU time than B                           |  |
|  |  But if B is idle, A can use all CPU                      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CPU QUOTA (absolute limit):                                     |
|  +------------------------------------------------------------+  |
|  |  cpu.cfs_quota_us = 50000                                 |  |
|  |  cpu.cfs_period_us = 100000                               |  |
|  |                                                           |  |
|  |  Container can use 50000/100000 = 50% of one CPU          |  |
|  |  Even if CPU is idle, container is throttled              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  DOCKER USAGE:                                                   |
|  +------------------------------------------------------------+  |
|  |  docker run --cpus=0.5 nginx       # 50% of 1 CPU         |  |
|  |  docker run --cpus=2 nginx         # Use 2 CPUs           |  |
|  |  docker run --cpu-shares=512 nginx # Relative weight      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

CPU Cgroup控制：

CPU Shares（相对权重）：容器A的cpu.shares=1024，容器B的cpu.shares=512。竞争CPU时A获得B的2倍CPU时间，但如果B空闲A可用全部CPU。

CPU Quota（绝对限制）：cpu.cfs_quota_us=50000、cpu.cfs_period_us=100000。容器可使用50000/100000=50%的一个CPU。即使CPU空闲，容器也被限流。

Docker用法：docker run --cpus=0.5（50%的1个CPU）、--cpus=2（用2个CPU）、--cpu-shares=512（相对权重）。

### Memory Limits

```
MEMORY CGROUP CONTROLS:
+------------------------------------------------------------------+
|                                                                  |
|  MEMORY LIMIT:                                                   |
|  +------------------------------------------------------------+  |
|  |  memory.limit_in_bytes = 536870912 (512 MB)               |  |
|  |                                                           |  |
|  |  If container tries to use more:                          |  |
|  |  1. Kernel reclaims memory (swap, page cache)             |  |
|  |  2. If still over limit: OOM killer kills process         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  MEMORY + SWAP LIMIT:                                            |
|  +------------------------------------------------------------+  |
|  |  memory.memsw.limit_in_bytes = memory + swap limit        |  |
|  |                                                           |  |
|  |  Set equal to memory limit to disable swap for container  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  DOCKER USAGE:                                                   |
|  +------------------------------------------------------------+  |
|  |  docker run --memory=512m nginx     # 512 MB limit        |  |
|  |  docker run --memory=512m --memory-swap=512m nginx        |  |
|  |                                      # No swap allowed    |  |
|  |  docker run --memory-reservation=256m nginx               |  |
|  |                                      # Soft limit         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

内存Cgroup控制：

内存限制：memory.limit_in_bytes = 536870912 (512MB)。如果容器尝试使用更多：1）内核回收内存（swap、页缓存），2）如果仍超限：OOM killer杀进程。

内存+Swap限制：memory.memsw.limit_in_bytes = 内存+swap限制。设为等于内存限制可禁用容器swap。

Docker用法：docker run --memory=512m（512MB限制）、--memory-swap=512m（不允许swap）、--memory-reservation=256m（软限制）。

---

## 4.3 Union Filesystems

Union filesystems are how Docker implements **layered images**.

```
UNION FILESYSTEM CONCEPT:
+==================================================================+
||                                                                ||
||  TRADITIONAL FILESYSTEM:                                       ||
||  +----------------------------------------------------------+  ||
||  |  /                                                       |  ||
||  |  └── app/                                                |  ||
||  |      ├── code.py    (one location, one version)          |  ||
||  |      └── config.txt                                      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  UNION FILESYSTEM (OverlayFS):                                 ||
||  +----------------------------------------------------------+  ||
||  |                                                          |  ||
||  |  Upper layer (read-write):                               |  ||
||  |  +-----------------------+                               |  ||
||  |  | config.txt (modified) |  <- Container's changes       |  ||
||  |  +-----------------------+                               |  ||
||  |            |                                             |  ||
||  |            v  merged view                                |  ||
||  |  Lower layer 2 (read-only):                              |  ||
||  |  +-----------------------+                               |  ||
||  |  | code.py               |  <- Application layer         |  ||
||  |  +-----------------------+                               |  ||
||  |            |                                             |  ||
||  |            v                                             |  ||
||  |  Lower layer 1 (read-only):                              |  ||
||  |  +-----------------------+                               |  ||
||  |  | /bin, /lib, etc.      |  <- Base OS layer             |  ||
||  |  +-----------------------+                               |  ||
||  |                                                          |  ||
||  |  RESULT: Process sees unified filesystem:                |  ||
||  |  /                                                       |  ||
||  |  ├── bin/ (from layer 1)                                 |  ||
||  |  ├── lib/ (from layer 1)                                 |  ||
||  |  └── app/                                                |  ||
||  |      ├── code.py (from layer 2)                          |  ||
||  |      └── config.txt (from upper layer, modified)         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

联合文件系统概念：

传统文件系统：/app/code.py在一个位置一个版本。

联合文件系统（OverlayFS）：上层（读写）包含容器的修改如config.txt；下层2（只读）是应用层如code.py；下层1（只读）是基础OS层如/bin、/lib。

结果：进程看到统一文件系统——/bin和/lib来自层1、/app/code.py来自层2、/app/config.txt来自上层（已修改）。

### How OverlayFS Works

```
OVERLAYFS MECHANICS:
+------------------------------------------------------------------+
|                                                                  |
|  READ OPERATION:                                                 |
|  +------------------------------------------------------------+  |
|  |  Process reads /app/code.py                               |  |
|  |  1. Check upper layer - not found                         |  |
|  |  2. Check lower layer 2 - FOUND, return file              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WRITE OPERATION (Copy-on-Write):                                |
|  +------------------------------------------------------------+  |
|  |  Process modifies /app/config.txt                         |  |
|  |  1. File exists in lower layer                            |  |
|  |  2. COPY file to upper layer                              |  |
|  |  3. Modify the copy in upper layer                        |  |
|  |  4. Lower layer unchanged (read-only)                     |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  DELETE OPERATION (Whiteout):                                    |
|  +------------------------------------------------------------+  |
|  |  Process deletes /app/oldfile.txt                         |  |
|  |  1. Cannot modify lower layer                             |  |
|  |  2. Create "whiteout" marker in upper layer               |  |
|  |  3. File appears deleted in merged view                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CREATE NEW FILE:                                                |
|  +------------------------------------------------------------+  |
|  |  Process creates /app/newfile.txt                         |  |
|  |  1. File written directly to upper layer                  |  |
|  |  2. Lower layers unchanged                                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

OverlayFS机制：

读操作：进程读/app/code.py。1）检查上层——未找到，2）检查下层2——找到，返回文件。

写操作（写时复制）：进程修改/app/config.txt。1）文件存在于下层，2）复制文件到上层，3）修改上层的副本，4）下层不变（只读）。

删除操作（Whiteout）：进程删除/app/oldfile.txt。1）不能修改下层，2）在上层创建"whiteout"标记，3）文件在合并视图中显示为已删除。

创建新文件：进程创建/app/newfile.txt。1）文件直接写入上层，2）下层不变。

### Why Docker Images Are Layered

```
BENEFITS OF LAYERED IMAGES:
+==================================================================+
||                                                                ||
||  IMAGE SHARING:                                                ||
||  +----------------------------------------------------------+  ||
||  |  Image A:           Image B:           Image C:          |  ||
||  |  +------------+     +------------+     +------------+    |  ||
||  |  | App A      |     | App B      |     | App C      |    |  ||
||  |  +------------+     +------------+     +------------+    |  ||
||  |  | Python     |     | Python     |     | Node.js    |    |  ||
||  |  +------------+     +------------+     +------------+    |  ||
||  |  | Ubuntu     |     | Ubuntu     |     | Ubuntu     |    |  ||
||  |  +------------+     +------------+     +------------+    |  ||
||  |                                                          |  ||
||  |  Disk usage WITHOUT sharing: 3 x Ubuntu layer            |  ||
||  |  Disk usage WITH sharing: 1 x Ubuntu layer (shared!)     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EFFICIENT UPDATES:                                            ||
||  +----------------------------------------------------------+  ||
||  |  When you update only your app:                          |  ||
||  |  - Download only the changed app layer                   |  ||
||  |  - Ubuntu and Python layers cached                       |  ||
||  |  - 5 MB download instead of 500 MB                       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  BUILD CACHING:                                                ||
||  +----------------------------------------------------------+  ||
||  |  Dockerfile builds layer by layer                        |  ||
||  |  If instruction unchanged, reuse cached layer            |  ||
||  |  Fast rebuilds when only code changes                    |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

分层镜像的好处：

镜像共享：镜像A、B、C都有不同的应用层但共享Ubuntu基础层。无共享磁盘使用：3倍Ubuntu层；有共享磁盘使用：1倍Ubuntu层（共享！）。

高效更新：当只更新你的应用时只下载改变的应用层，Ubuntu和Python层缓存。5MB下载而非500MB。

构建缓存：Dockerfile逐层构建，如果指令未改变则重用缓存层，只有代码变化时快速重建。

---

## Summary

```
LINUX PRIMITIVES SUMMARY:
+==================================================================+
||                                                                ||
||  NAMESPACES (Isolation):                                       ||
||  - PID: Process ID isolation (container has own PID 1)         ||
||  - Mount: Filesystem isolation (container has own root)        ||
||  - Network: Network isolation (own interfaces, ports)          ||
||  - IPC: Shared memory isolation                                ||
||  - UTS: Hostname isolation                                     ||
||  - User: UID/GID mapping                                       ||
||                                                                ||
||  CGROUPS (Resource Limits):                                    ||
||  - CPU: shares, quotas, periods                                ||
||  - Memory: hard limits, soft limits, OOM behavior              ||
||  - I/O: bandwidth limits, IOPS limits                          ||
||                                                                ||
||  UNION FILESYSTEMS (Layering):                                 ||
||  - OverlayFS: layers lower (read-only) + upper (read-write)    ||
||  - Copy-on-write: efficient disk usage                         ||
||  - Image sharing: multiple containers share base layers        ||
||                                                                ||
||  KEY INSIGHT:                                                  ||
||  Docker is a USER-FRIENDLY INTERFACE to these kernel features. ||
||  There is no "container" kernel object - just processes with   ||
||  namespaces and cgroups.                                       ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Docker Images](05-docker-images.md)
