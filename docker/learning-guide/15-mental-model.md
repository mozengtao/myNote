# Section 15: Mental Model Summary

## 15.1 Docker in One Diagram

```
DOCKER MENTAL MODEL:
+==================================================================+
||                                                                ||
||  WHAT YOU WRITE:                                               ||
||  +----------------------------------------------------------+  ||
||  |  Dockerfile         docker-compose.yml                   |  ||
||  |  (Build recipe)     (Multi-container definition)         |  ||
||  +----------------------------------------------------------+  ||
||                    |                 |                         ||
||            docker build      docker-compose up                 ||
||                    |                 |                         ||
||                    v                 v                         ||
||  WHAT DOCKER CREATES:                                          ||
||  +----------------------------------------------------------+  ||
||  |  IMAGE                NETWORK              VOLUME         |  ||
||  |  (Filesystem layers)  (Virtual network)   (Persistent     |  ||
||  |                                            storage)       |  ||
||  +----------------------------------------------------------+  ||
||                    |                                           ||
||              docker run                                        ||
||                    |                                           ||
||                    v                                           ||
||  WHAT RUNS:                                                    ||
||  +----------------------------------------------------------+  ||
||  |  CONTAINER = Linux process with:                         |  ||
||  |  +------------------------------------------------------+|  ||
||  |  | - Namespaces (isolated view of system)               ||  ||
||  |  | - Cgroups (resource limits)                          ||  ||
||  |  | - Union filesystem (layered storage)                 ||  ||
||  |  +------------------------------------------------------+|  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Docker心智模型：

你写的：Dockerfile（构建配方）、docker-compose.yml（多容器定义）。

Docker创建的：镜像（文件系统层）、网络（虚拟网络）、卷（持久存储）。

运行的：容器=Linux进程带Namespaces（系统隔离视图）、Cgroups（资源限制）、联合文件系统（分层存储）。

---

## 15.2 How to Think About Containers

```
CONTAINER THINKING:
+==================================================================+
||                                                                ||
||  CONTAINERS ARE:                                               ||
||  +----------------------------------------------------------+  ||
||  |  PROCESSES, not VMs                                      |  ||
||  |  - Just Linux processes with extra isolation             |  ||
||  |  - No guest OS, shared kernel                            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  +----------------------------------------------------------+  ||
||  |  IMMUTABLE, not patchable                                |  ||
||  |  - Don't update running containers                       |  ||
||  |  - Build new image, deploy new container                 |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  +----------------------------------------------------------+  ||
||  |  EPHEMERAL, not persistent                               |  ||
||  |  - Container dies, data dies (unless in volume)          |  ||
||  |  - Design for container recreation                       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  +----------------------------------------------------------+  ||
||  |  SINGLE-PURPOSE, not multi-service                       |  ||
||  |  - One container = one concern                           |  ||
||  |  - Multiple services = multiple containers               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  +----------------------------------------------------------+  ||
||  |  PACKAGED APPLICATIONS, not operating systems            |  ||
||  |  - Package your app and its dependencies                 |  ||
||  |  - Not a place to "install software"                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

容器思维：

容器是进程，不是VM：只是带额外隔离的Linux进程，无Guest OS，共享内核。

容器是不可变的，不可打补丁：不更新运行中的容器，构建新镜像部署新容器。

容器是短暂的，不是持久的：容器死亡数据消失（除非在卷中），为容器重建设计。

容器是单一用途的，不是多服务的：一个容器=一个关注点，多服务=多容器。

容器是打包的应用，不是操作系统：打包你的应用及其依赖，不是"安装软件"的地方。

---

## 15.3 What Docker Guarantees

```
DOCKER GUARANTEES:
+------------------------------------------------------------------+
|                                                                  |
|  REPRODUCIBILITY                                                 |
|  +------------------------------------------------------------+  |
|  |  Same image = same behavior                               |  |
|  |  Build once, run anywhere (with same kernel)              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  ISOLATION                                                       |
|  +------------------------------------------------------------+  |
|  |  Containers don't see each other's processes              |  |
|  |  Containers have separate filesystems                     |  |
|  |  Containers can have separate networks                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  RESOURCE LIMITS                                                 |
|  +------------------------------------------------------------+  |
|  |  Containers can be limited in CPU, memory, I/O            |  |
|  |  One container can't exhaust host resources               |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  PORTABILITY                                                     |
|  +------------------------------------------------------------+  |
|  |  Images work on any Docker host                           |  |
|  |  Registry for easy distribution                           |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Docker保证：

可重现性：相同镜像=相同行为，一次构建到处运行（相同内核）。

隔离：容器看不到彼此的进程、容器有独立文件系统、容器可有独立网络。

资源限制：容器可限制CPU/内存/IO、一个容器不能耗尽主机资源。

可移植性：镜像在任何Docker主机上工作、通过仓库易于分发。

---

## 15.4 What Docker Does NOT Guarantee

```
DOCKER DOES NOT GUARANTEE:
+------------------------------------------------------------------+
|                                                                  |
|  SECURITY BOUNDARY                                               |
|  +------------------------------------------------------------+  |
|  |  Containers share kernel with host                        |  |
|  |  Kernel vulnerabilities affect all containers             |  |
|  |  Not as isolated as VMs                                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  PERFORMANCE ISOLATION                                           |
|  +------------------------------------------------------------+  |
|  |  Without limits, containers compete for resources         |  |
|  |  I/O contention is possible                               |  |
|  |  Network bandwidth shared                                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  DATA PERSISTENCE                                                |
|  +------------------------------------------------------------+  |
|  |  Container filesystem is ephemeral by default             |  |
|  |  Must explicitly use volumes for persistence              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  AUTOMATIC SCALING/HEALING                                       |
|  +------------------------------------------------------------+  |
|  |  Docker alone doesn't restart crashed containers          |  |
|  |  (unless restart policy set)                              |  |
|  |  No automatic scaling (need Kubernetes for that)          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Docker不保证：

安全边界：容器与主机共享内核、内核漏洞影响所有容器、不如VM隔离。

性能隔离：无限制时容器竞争资源、可能I/O争用、网络带宽共享。

数据持久性：容器文件系统默认短暂、必须显式使用卷实现持久化。

自动扩展/修复：Docker本身不重启崩溃的容器（除非设置重启策略）、无自动扩展（需要Kubernetes）。

---

## 15.5 When NOT to Use Docker

```
DOCKER MAY NOT BE THE BEST CHOICE WHEN:
+==================================================================+
||                                                                ||
||  HEAVY GUI APPLICATIONS                                        ||
||  +----------------------------------------------------------+  ||
||  |  Docker is designed for headless services                |  ||
||  |  GUI applications are complex to containerize            |  ||
||  |  Better: native install or VM                            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  KERNEL DEPENDENCIES                                           ||
||  +----------------------------------------------------------+  ||
||  |  Need specific kernel modules                            |  ||
||  |  Need different kernel version                           |  ||
||  |  Better: VM with desired kernel                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  MAXIMUM PERFORMANCE                                           ||
||  +----------------------------------------------------------+  ||
||  |  Some overhead from overlay filesystem                   |  ||
||  |  Network NAT adds latency                                |  ||
||  |  For HFT, bare metal might be needed                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  STRONG MULTI-TENANCY ISOLATION                                ||
||  +----------------------------------------------------------+  ||
||  |  Untrusted code from different customers                 |  ||
||  |  Container escape is a real concern                      |  ||
||  |  Better: VMs or specialized solutions (gVisor, Firecracker)
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXTREMELY SIMPLE DEPLOYMENTS                                  ||
||  +----------------------------------------------------------+  ||
||  |  Single binary, no dependencies                          |  ||
||  |  Docker adds complexity                                  |  ||
||  |  Just copy the binary                                    |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Docker可能不是最佳选择的场景：

重GUI应用：Docker为无头服务设计，GUI应用容器化复杂，更好选择：原生安装或VM。

内核依赖：需要特定内核模块、不同内核版本，更好选择：有所需内核的VM。

最大性能：overlay文件系统有开销、网络NAT增加延迟，高频交易可能需要裸金属。

强多租户隔离：来自不同客户的不信任代码、容器逃逸是真正关注点，更好选择：VM或gVisor/Firecracker。

极简部署：单二进制无依赖，Docker增加复杂性，直接复制二进制即可。

---

## 15.6 Final Summary

```
DOCKER IN 60 SECONDS:
+==================================================================+
||                                                                ||
||  WHAT: Container runtime using Linux namespaces and cgroups    ||
||                                                                ||
||  WHY: Package app + dependencies, run anywhere, reproducibly   ||
||                                                                ||
||  HOW:                                                          ||
||  - Dockerfile -> docker build -> Image                         ||
||  - Image -> docker run -> Container (running process)          ||
||                                                                ||
||  KEY CONCEPTS:                                                 ||
||  - Images: Read-only filesystem layers + metadata              ||
||  - Containers: Running processes with isolation                ||
||  - Volumes: Persistent data storage                            ||
||  - Networks: Container communication                           ||
||                                                                ||
||  LINUX PRIMITIVES:                                             ||
||  - Namespaces: Isolation (PID, mount, network, etc.)           ||
||  - Cgroups: Resource limits (CPU, memory, I/O)                 ||
||  - Union FS: Layered filesystems                               ||
||                                                                ||
||  BEST PRACTICES:                                               ||
||  - One process per container                                   ||
||  - Immutable: rebuild, don't patch                             ||
||  - Non-root user                                               ||
||  - Secrets at runtime, not in image                            ||
||  - Specific version tags                                       ||
||                                                                ||
||  DOCKER IS:                                                    ||
||  - Great for packaging and deploying applications              ||
||  - Not a VM, not perfect security, not automatic orchestration ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

60秒总结Docker：

什么：使用Linux namespaces和cgroups的容器运行时。

为什么：打包应用+依赖，到处运行，可重现。

如何：Dockerfile -> docker build -> 镜像 -> docker run -> 容器（运行中的进程）。

关键概念：镜像（只读文件系统层+元数据）、容器（带隔离的运行进程）、卷（持久数据存储）、网络（容器通信）。

Linux原语：Namespaces（隔离）、Cgroups（资源限制）、Union FS（分层文件系统）。

最佳实践：一个容器一个进程、不可变（重建不打补丁）、非root用户、运行时传递密钥、具体版本标签。

Docker是：打包部署应用的好工具，不是VM、不是完美安全、不是自动编排。

---

**Congratulations!** You now have a solid understanding of Docker from first principles.

Return to [README](README.md) for the complete guide index.
