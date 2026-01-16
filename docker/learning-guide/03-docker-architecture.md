# Section 3: Docker Architecture (High-Level)

## 3.1 Big Picture

```
DOCKER ARCHITECTURE STACK:
+==================================================================+
||                                                                ||
||  User Interface Layer                                          ||
||  +----------------------------------------------------------+  ||
||  |  docker CLI                                              |  ||
||  |  (docker run, docker build, docker pull, etc.)           |  ||
||  +----------------------------------------------------------+  ||
||                            | REST API                          ||
||                            v                                   ||
||  Docker Engine Layer                                           ||
||  +----------------------------------------------------------+  ||
||  |  dockerd (Docker daemon)                                 |  ||
||  |  - API server                                            |  ||
||  |  - Image management                                      |  ||
||  |  - Volume management                                     |  ||
||  |  - Network management                                    |  ||
||  |  - Build operations                                      |  ||
||  +----------------------------------------------------------+  ||
||                            | gRPC API                          ||
||                            v                                   ||
||  Container Runtime Layer                                       ||
||  +----------------------------------------------------------+  ||
||  |  containerd                                              |  ||
||  |  - Container lifecycle management                        |  ||
||  |  - Image distribution                                    |  ||
||  |  - Snapshot management                                   |  ||
||  +----------------------------------------------------------+  ||
||                            | OCI Runtime Spec                  ||
||                            v                                   ||
||  Low-Level Runtime Layer                                       ||
||  +----------------------------------------------------------+  ||
||  |  runc                                                    |  ||
||  |  - Create namespaces                                     |  ||
||  |  - Set up cgroups                                        |  ||
||  |  - Execute container process                             |  ||
||  +----------------------------------------------------------+  ||
||                            | System calls                      ||
||                            v                                   ||
||  Kernel Layer                                                  ||
||  +----------------------------------------------------------+  ||
||  |  Linux Kernel                                            |  ||
||  |  - Namespaces                                            |  ||
||  |  - Cgroups                                               |  ||
||  |  - Union filesystems                                     |  ||
||  |  - Network stack                                         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Docker架构栈：

用户接口层：docker CLI（docker run/build/pull等命令）。

Docker引擎层：dockerd（Docker守护进程），负责API服务、镜像管理、卷管理、网络管理、构建操作。

容器运行时层：containerd，负责容器生命周期管理、镜像分发、快照管理。

低级运行时层：runc，负责创建namespaces、设置cgroups、执行容器进程。

内核层：Linux内核，提供Namespaces、Cgroups、联合文件系统、网络栈。

### Why Docker Is Layered Like This

```
DESIGN RATIONALE:
+==================================================================+
||                                                                ||
||  SEPARATION OF CONCERNS:                                       ||
||                                                                ||
||  Layer          Responsibility             Can be replaced?    ||
||  -----------    ---------------------      ------------------  ||
||  docker CLI     User experience            Yes (podman, etc.)  ||
||  dockerd        High-level orchestration   Yes (containerd)    ||
||  containerd     Container lifecycle        Yes (cri-o, etc.)   ||
||  runc           Low-level execution        Yes (kata, gvisor)  ||
||                                                                ||
||  BENEFITS:                                                     ||
||                                                                ||
||  1. MODULARITY                                                 ||
||  +----------------------------------------------------------+  ||
||  | - Replace runc with gVisor for extra isolation           |  ||
||  | - Replace containerd with CRI-O for Kubernetes           |  ||
||  | - Docker CLI can talk to remote daemons                  |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  2. STANDARDIZATION (OCI)                                      ||
||  +----------------------------------------------------------+  ||
||  | - Runtime Spec: How to run a container                   |  ||
||  | - Image Spec: How to package container images            |  ||
||  | - Any compliant tool can work together                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  3. STABILITY                                                  ||
||  +----------------------------------------------------------+  ||
||  | - containerd runs as long-lived daemon                   |  ||
||  | - runc exits after container starts                      |  ||
||  | - Daemon restart doesn't kill containers                 |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

设计原理——关注点分离：

| 层 | 职责 | 可替换？ |
|----|------|---------|
| docker CLI | 用户体验 | 是(podman等) |
| dockerd | 高级编排 | 是(containerd) |
| containerd | 容器生命周期 | 是(cri-o等) |
| runc | 低级执行 | 是(kata, gvisor) |

好处：1) 模块化——可用gVisor替换runc获得额外隔离、用CRI-O替换containerd用于Kubernetes。2) 标准化(OCI)——运行时规范定义如何运行容器、镜像规范定义如何打包镜像、任何兼容工具可协同工作。3) 稳定性——containerd作为长期运行守护进程、runc在容器启动后退出、守护进程重启不杀容器。

---

## 3.2 Docker Client-Server Model

```
CLIENT-SERVER ARCHITECTURE:
+==================================================================+
||                                                                ||
||  Local Machine                                                 ||
||  +----------------------------------------------------------+  ||
||  |                                                          |  ||
||  |  Terminal                                                |  ||
||  |  $ docker run nginx                                      |  ||
||  |        |                                                 |  ||
||  |        v                                                 |  ||
||  |  +------------+         +-----------------------+        |  ||
||  |  | docker CLI | ------> | /var/run/docker.sock |        |  ||
||  |  +------------+  REST   | (Unix socket)        |        |  ||
||  |                  API    +-----------------------+        |  ||
||  |                               |                          |  ||
||  |                               v                          |  ||
||  |                    +-------------------+                 |  ||
||  |                    | dockerd (daemon)  |                 |  ||
||  |                    | Running as root   |                 |  ||
||  |                    +-------------------+                 |  ||
||  |                                                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Remote Machine (also possible)                                ||
||  +----------------------------------------------------------+  ||
||  |  $ DOCKER_HOST=tcp://remote:2375 docker run nginx        |  ||
||  |        |                                                 |  ||
||  |        +-------- TCP --------> dockerd on remote host    |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

客户端-服务器架构：

本地机器：docker CLI发送REST API请求到/var/run/docker.sock（Unix套接字），dockerd守护进程以root身份运行处理请求。

远程机器也可以：通过设置DOCKER_HOST=tcp://remote:2375，docker CLI可以通过TCP与远程主机上的dockerd通信。

### Why Docker Uses a Daemon

```
DAEMON DESIGN DECISION:
+------------------------------------------------------------------+
|                                                                  |
|  REASONS FOR DAEMON:                                             |
|                                                                  |
|  1. PRIVILEGE SEPARATION                                         |
|  +------------------------------------------------------------+  |
|  | - Creating namespaces requires root                       |  |
|  | - Cgroup operations require root                          |  |
|  | - Network setup requires root                             |  |
|  | - Daemon runs as root, user runs CLI as regular user      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  2. PERSISTENT STATE                                             |
|  +------------------------------------------------------------+  |
|  | - Containers keep running after CLI exits                 |  |
|  | - Daemon tracks all container state                       |  |
|  | - Image layers cached in daemon                           |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  3. CENTRAL MANAGEMENT                                           |
|  +------------------------------------------------------------+  |
|  | - Multiple CLI clients can connect                        |  |
|  | - CI/CD systems can use Docker API                        |  |
|  | - Remote management possible                              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  TRADEOFF: Security implications                                 |
|  +------------------------------------------------------------+  |
|  | - Access to docker socket = root on host                  |  |
|  | - Docker group membership is powerful                     |  |
|  | - Consider rootless Docker for more security              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

守护进程设计决策的原因：

1. 权限分离：创建namespaces需要root、cgroup操作需要root、网络设置需要root。守护进程以root运行，用户以普通用户运行CLI。

2. 持久状态：CLI退出后容器继续运行、守护进程跟踪所有容器状态、镜像层缓存在守护进程中。

3. 集中管理：多个CLI客户端可连接、CI/CD系统可使用Docker API、可远程管理。

权衡——安全影响：访问docker socket=主机root权限、docker组成员资格很强大、考虑rootless Docker获得更多安全。

### Security and Privilege Implications

```
DOCKER SECURITY MODEL:
+==================================================================+
||                                                                ||
||  WHO CAN RUN DOCKER COMMANDS?                                  ||
||                                                                ||
||  Option 1: As root                                             ||
||  +----------------------------------------------------------+  ||
||  |  $ sudo docker run ...                                   |  ||
||  |  Full access, obvious security implications              |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Option 2: Docker group                                        ||
||  +----------------------------------------------------------+  ||
||  |  $ sudo usermod -aG docker $USER                         |  ||
||  |  $ docker run ...  # No sudo needed                      |  ||
||  |                                                          |  ||
||  |  WARNING: Docker group = root equivalent!                |  ||
||  |  User can: mount host filesystem, access devices         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Option 3: Rootless Docker                                     ||
||  +----------------------------------------------------------+  ||
||  |  Docker daemon runs as regular user                      |  ||
||  |  Uses user namespaces                                    |  ||
||  |  More secure but some limitations                        |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CONTAINER ROOT != HOST ROOT (usually):                        ||
||  +----------------------------------------------------------+  ||
||  |  By default, root in container is root on host           |  ||
||  |  Use user namespaces to remap UIDs                       |  ||
||  |  Use USER directive in Dockerfile to run as non-root     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Docker安全模型——谁可以运行Docker命令？

选项1——以root：sudo docker run...，完全访问，明显的安全影响。

选项2——docker组：加入docker组后无需sudo运行docker。警告：docker组=root等效！用户可以：挂载主机文件系统、访问设备。

选项3——Rootless Docker：Docker守护进程以普通用户运行，使用用户namespaces，更安全但有一些限制。

容器root != 主机root（通常）：默认情况下，容器中的root是主机上的root。使用user namespaces重映射UID。在Dockerfile中使用USER指令以非root运行。

---

## 3.3 Component Deep Dive

```
COMPONENT RESPONSIBILITIES:
+==================================================================+
||                                                                ||
||  DOCKER CLI (docker)                                           ||
||  +----------------------------------------------------------+  ||
||  | - Parse user commands                                    |  ||
||  | - Format API requests                                    |  ||
||  | - Display responses                                      |  ||
||  | - Provide user-friendly interface                        |  ||
||  |                                                          |  ||
||  | Stateless: knows nothing without talking to daemon       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  DOCKER DAEMON (dockerd)                                       ||
||  +----------------------------------------------------------+  ||
||  | - REST API server                                        |  ||
||  | - Image pull/push/build                                  |  ||
||  | - Container creation requests                            |  ||
||  | - Volume management                                      |  ||
||  | - Network management                                     |  ||
||  | - Plugin management                                      |  ||
||  |                                                          |  ||
||  | Delegates actual container work to containerd            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CONTAINERD                                                    ||
||  +----------------------------------------------------------+  ||
||  | - Container lifecycle (create, start, stop, delete)      |  ||
||  | - Image management (pull, push, unpack)                  |  ||
||  | - Snapshot management (filesystem layers)                |  ||
||  | - Task management (container processes)                  |  ||
||  |                                                          |  ||
||  | Industry standard, used by Kubernetes too                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  RUNC                                                          ||
||  +----------------------------------------------------------+  ||
||  | - OCI runtime reference implementation                   |  ||
||  | - Creates namespaces                                     |  ||
||  | - Configures cgroups                                     |  ||
||  | - Sets up root filesystem                                |  ||
||  | - Executes container init process                        |  ||
||  |                                                          |  ||
||  | Short-lived: exits after container starts                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

组件职责：

Docker CLI (docker)：解析用户命令、格式化API请求、显示响应、提供用户友好界面。无状态：不与守护进程通信就什么都不知道。

Docker守护进程 (dockerd)：REST API服务器、镜像pull/push/build、容器创建请求、卷管理、网络管理、插件管理。将实际容器工作委托给containerd。

containerd：容器生命周期（创建、启动、停止、删除）、镜像管理、快照管理、任务管理。行业标准，Kubernetes也使用。

runc：OCI运行时参考实现、创建namespaces、配置cgroups、设置根文件系统、执行容器init进程。短命：容器启动后退出。

---

## Summary

```
DOCKER ARCHITECTURE KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  LAYERED DESIGN:                                               ||
||  CLI -> dockerd -> containerd -> runc -> kernel                ||
||                                                                ||
||  WHY LAYERED:                                                  ||
||  - Modularity (components can be replaced)                     ||
||  - Standardization (OCI specs)                                 ||
||  - Stability (daemon restart safe)                             ||
||                                                                ||
||  CLIENT-SERVER:                                                ||
||  - CLI is stateless client                                     ||
||  - Daemon is privileged server                                 ||
||  - Communication via Unix socket or TCP                        ||
||                                                                ||
||  SECURITY IMPLICATIONS:                                        ||
||  - Docker socket access = root access                          ||
||  - Docker group is root equivalent                             ||
||  - Consider rootless Docker for security                       ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Linux Primitives Behind Docker](04-linux-primitives.md)
