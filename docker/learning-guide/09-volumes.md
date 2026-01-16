# Section 9: Docker Volumes and Data Persistence

## 9.1 Why Containers Are Ephemeral

```
CONTAINER FILESYSTEM LIFECYCLE:
+==================================================================+
||                                                                ||
||  docker run nginx                                              ||
||       |                                                        ||
||       v                                                        ||
||  +----------------------------------------------------------+  ||
||  |  Image layers (read-only)                                |  ||
||  +----------------------------------------------------------+  ||
||  |  Container layer (read-write)  <- Data written here      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Container runs, data written to container layer...            ||
||                                                                ||
||  docker rm container                                           ||
||       |                                                        ||
||       v                                                        ||
||  +----------------------------------------------------------+  ||
||  |  Container layer DELETED                                 |  ||
||  |  ALL DATA IN CONTAINER IS GONE                           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Image layers remain (they're read-only, shared)               ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

容器文件系统生命周期：docker run nginx创建只读镜像层和读写容器层。数据写入容器层。

docker rm container删除容器层——容器中所有数据消失！镜像层保留（只读、共享）。

### Immutable Infrastructure Mindset

```
THE IMMUTABLE INFRASTRUCTURE PRINCIPLE:
+------------------------------------------------------------------+
|                                                                  |
|  TRADITIONAL (mutable):                                          |
|  +------------------------------------------------------------+  |
|  |  Server deployed                                          |  |
|  |      -> patch applied                                     |  |
|  |      -> config changed                                    |  |
|  |      -> more patches                                      |  |
|  |      -> nobody knows exact state                          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  DOCKER (immutable):                                             |
|  +------------------------------------------------------------+  |
|  |  Image v1.0 deployed                                      |  |
|  |      -> need change?                                      |  |
|  |      -> build NEW image v1.1                              |  |
|  |      -> deploy v1.1, destroy v1.0 container               |  |
|  |      -> exact state known (it's the image!)               |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CONSEQUENCE:                                                    |
|  +------------------------------------------------------------+  |
|  |  Containers are disposable                                |  |
|  |  Data must NOT live inside containers                     |  |
|  |  Data must be stored OUTSIDE in volumes                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

不可变基础设施原则：

传统（可变）：服务器部署 -> 打补丁 -> 改配置 -> 更多补丁 -> 没人知道确切状态。

Docker（不可变）：镜像v1.0部署 -> 需要改变？ -> 构建新镜像v1.1 -> 部署v1.1销毁v1.0容器 -> 确切状态已知（就是镜像！）。

结果：容器是可丢弃的、数据不能存在容器内、数据必须存在外部的卷中。

---

## 9.2 Volumes vs Bind Mounts

```
DATA STORAGE OPTIONS:
+==================================================================+
||                                                                ||
||  VOLUME (Docker-managed):                                      ||
||  +----------------------------------------------------------+  ||
||  |  docker volume create mydata                             |  ||
||  |  docker run -v mydata:/app/data nginx                    |  ||
||  |                                                          |  ||
||  |  Host: /var/lib/docker/volumes/mydata/_data/             |  ||
||  |              |                                           |  ||
||  |              v                                           |  ||
||  |  Container: /app/data/                                   |  ||
||  |                                                          |  ||
||  |  Docker manages the storage location                     |  ||
||  |  Portable across systems                                 |  ||
||  |  Can be backed up with docker commands                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  BIND MOUNT (Host path):                                       ||
||  +----------------------------------------------------------+  ||
||  |  docker run -v /home/user/data:/app/data nginx           |  ||
||  |                                                          |  ||
||  |  Host: /home/user/data/                                  |  ||
||  |              |                                           |  ||
||  |              v                                           |  ||
||  |  Container: /app/data/                                   |  ||
||  |                                                          |  ||
||  |  You control the exact location                          |  ||
||  |  Good for development (code sync)                        |  ||
||  |  Host-dependent (not portable)                           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

数据存储选项：

Volume（Docker管理）：docker volume create mydata，docker run -v mydata:/app/data nginx。主机位置：/var/lib/docker/volumes/mydata/_data/映射到容器/app/data/。Docker管理存储位置、跨系统可移植、可用docker命令备份。

Bind Mount（主机路径）：docker run -v /home/user/data:/app/data nginx。主机/home/user/data/映射到容器/app/data/。你控制确切位置、适合开发（代码同步）、依赖主机（不可移植）。

### Comparison Table

```
VOLUMES VS BIND MOUNTS:
+------------------------------------------------------------------+
|                                                                  |
|  Aspect            | Volume              | Bind Mount            |
|  ------------------+---------------------+---------------------- |
|  Location          | Docker manages      | You specify path      |
|  Portability       | High                | Low (host-dependent)  |
|  Backup            | docker volume cmds  | Manual                |
|  Performance       | Best on Linux       | Same as volume        |
|  Pre-populated     | Yes, from image     | No                    |
|  Use case          | Production data     | Development           |
|  Security          | More isolated       | Access to host paths  |
|                                                                  |
|  WHEN TO USE VOLUME:                                             |
|  - Database storage                                              |
|  - Persistent application data                                   |
|  - Data that should survive container recreation                 |
|  - When you don't care where data is stored                      |
|                                                                  |
|  WHEN TO USE BIND MOUNT:                                         |
|  - Development: sync code into container                         |
|  - Configuration files from host                                 |
|  - When you need specific host location                          |
|  - Sharing data between host and container                       |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Volume vs Bind Mount：

| 方面 | Volume | Bind Mount |
|------|--------|------------|
| 位置 | Docker管理 | 你指定路径 |
| 可移植性 | 高 | 低（依赖主机）|
| 备份 | docker volume命令 | 手动 |
| 性能 | Linux上最佳 | 与volume相同 |
| 预填充 | 是，从镜像 | 否 |
| 使用场景 | 生产数据 | 开发 |

何时用Volume：数据库存储、持久应用数据、需要在容器重建后保留的数据。

何时用Bind Mount：开发代码同步、主机配置文件、需要特定主机位置、主机和容器间共享数据。

### Volume Commands

```
VOLUME MANAGEMENT:
+------------------------------------------------------------------+
|                                                                  |
|  # Create volume                                                 |
|  docker volume create mydata                                     |
|                                                                  |
|  # List volumes                                                  |
|  docker volume ls                                                |
|                                                                  |
|  # Inspect volume                                                |
|  docker volume inspect mydata                                    |
|  # Shows: Mountpoint: /var/lib/docker/volumes/mydata/_data       |
|                                                                  |
|  # Use volume in container                                       |
|  docker run -v mydata:/app/data nginx                            |
|                                                                  |
|  # Anonymous volume (auto-named)                                 |
|  docker run -v /app/data nginx                                   |
|                                                                  |
|  # Remove volume (only if not in use)                            |
|  docker volume rm mydata                                         |
|                                                                  |
|  # Remove all unused volumes                                     |
|  docker volume prune                                             |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

卷管理：docker volume create创建卷、docker volume ls列出卷、docker volume inspect检查卷（显示挂载点）、docker run -v使用卷、docker volume rm删除卷、docker volume prune删除所有未使用卷。

---

## Summary

```
VOLUMES KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  CONTAINERS ARE EPHEMERAL:                                     ||
||  - Container layer deleted with container                      ||
||  - Data must live outside in volumes                           ||
||                                                                ||
||  VOLUMES (DOCKER-MANAGED):                                     ||
||  - Best for persistent data                                    ||
||  - Portable, Docker handles location                           ||
||  - Use for databases, uploads, etc.                            ||
||                                                                ||
||  BIND MOUNTS (HOST PATHS):                                     ||
||  - Best for development                                        ||
||  - Direct access to host filesystem                            ||
||  - Use for code sync, config files                             ||
||                                                                ||
||  RULE OF THUMB:                                                ||
||  - Production: volumes                                         ||
||  - Development: bind mounts                                    ||
||                                                                ||
+==================================================================+
```

**Next Section**: [docker-compose (Multi-Container Systems)](10-docker-compose.md)
