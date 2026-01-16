# Section 5: Docker Images

## 5.1 What an Image Really Is

```
IMAGE ANATOMY:
+==================================================================+
||                                                                ||
||  A Docker image is NOT:                                        ||
||  - A running process                                           ||
||  - An executable file                                          ||
||  - A virtual machine disk                                      ||
||                                                                ||
||  A Docker image IS:                                            ||
||  +----------------------------------------------------------+  ||
||  |                                                          |  ||
||  |  FILESYSTEM LAYERS (the content):                        |  ||
||  |  +------------------------------------------------------+|  ||
||  |  | Layer 3: App files        sha256:abc123...           ||  ||
||  |  +------------------------------------------------------+|  ||
||  |  | Layer 2: Python packages  sha256:def456...           ||  ||
||  |  +------------------------------------------------------+|  ||
||  |  | Layer 1: Ubuntu base      sha256:789xyz...           ||  ||
||  |  +------------------------------------------------------+|  ||
||  |                                                          |  ||
||  |  + METADATA (the configuration):                         |  ||
||  |  +------------------------------------------------------+|  ||
||  |  | - Environment variables (ENV)                        ||  ||
||  |  | - Default command (CMD)                              ||  ||
||  |  | - Exposed ports (EXPOSE)                             ||  ||
||  |  | - Working directory (WORKDIR)                        ||  ||
||  |  | - User to run as (USER)                              ||  ||
||  |  | - Parent image reference                             ||  ||
||  |  | - Creation timestamp                                 ||  ||
||  |  +------------------------------------------------------+|  ||
||  |                                                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

镜像解剖：

Docker镜像不是：运行中的进程、可执行文件、虚拟机磁盘。

Docker镜像是：文件系统层（内容）——层3是App文件、层2是Python包、层1是Ubuntu基础；加上元数据（配置）——环境变量(ENV)、默认命令(CMD)、暴露端口(EXPOSE)、工作目录(WORKDIR)、运行用户(USER)、父镜像引用、创建时间戳。

### Content-Addressable Storage

```
CONTENT-ADDRESSABLE STORAGE:
+------------------------------------------------------------------+
|                                                                  |
|  Every layer is identified by SHA256 hash of its CONTENT:        |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |  Layer contents:                                          |  |
|  |  /bin/bash (file contents...)                             |  |
|  |  /lib/x86_64-linux-gnu/libc.so.6 (file contents...)       |  |
|  |  ...                                                      |  |
|  |                                                           |  |
|  |  SHA256(all contents) = abc123def456...                   |  |
|  |                                                           |  |
|  |  This hash IS the layer ID                                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  IMPLICATIONS:                                                   |
|                                                                  |
|  1. Same content = same hash = same layer                        |
|     Two Dockerfiles that install the same Ubuntu version         |
|     share the same base layer (not duplicated on disk)           |
|                                                                  |
|  2. Integrity verification                                       |
|     If content changes, hash changes                             |
|     Corrupted layers detected automatically                      |
|                                                                  |
|  3. Efficient distribution                                       |
|     "Do you have layer abc123?" instead of "Do you have Ubuntu?" |
|     Pull only missing layers                                     |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

内容寻址存储：每个层由其内容的SHA256哈希标识。

层内容包括/bin/bash、/lib/x86_64-linux-gnu/libc.so.6等文件，SHA256(所有内容) = abc123def456...，这个哈希就是层ID。

含义：1）相同内容=相同哈希=相同层——两个安装相同Ubuntu版本的Dockerfile共享同一基础层（磁盘上不重复）。2）完整性验证——内容变化则哈希变化，自动检测损坏的层。3）高效分发——"你有层abc123吗？"而非"你有Ubuntu吗？"，只拉取缺失的层。

### Image vs Container

```
IMAGE VS CONTAINER:
+==================================================================+
||                                                                ||
||  IMAGE (Template):                                             ||
||  +----------------------------------------------------------+  ||
||  |  - Read-only filesystem layers                           |  ||
||  |  - Static configuration                                  |  ||
||  |  - Can exist without running                             |  ||
||  |  - Shared by multiple containers                         |  ||
||  |  - Like a CLASS in OOP                                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||                    docker run                                  ||
||                        |                                       ||
||                        v                                       ||
||                                                                ||
||  CONTAINER (Instance):                                         ||
||  +----------------------------------------------------------+  ||
||  |  - Image layers (read-only, shared)                      |  ||
||  |  - Container layer (read-write, unique)                  |  ||
||  |  - Running process(es)                                   |  ||
||  |  - Runtime state (memory, CPU, network)                  |  ||
||  |  - Like an INSTANCE in OOP                               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  ONE IMAGE -> MANY CONTAINERS:                                 ||
||                                                                ||
||  nginx:latest (image)                                          ||
||       |                                                        ||
||       +-> container1 (running, port 8080)                      ||
||       +-> container2 (running, port 8081)                      ||
||       +-> container3 (stopped)                                 ||
||                                                                ||
||  All share same image layers, each has own container layer     ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

镜像vs容器：

镜像（模板）：只读文件系统层、静态配置、可以不运行就存在、被多个容器共享、像OOP中的类。

容器（实例）：镜像层（只读、共享）、容器层（读写、唯一）、运行中的进程、运行时状态（内存、CPU、网络）、像OOP中的实例。

一个镜像->多个容器：nginx:latest镜像可创建container1(运行中端口8080)、container2(运行中端口8081)、container3(已停止)。所有共享相同镜像层，每个有自己的容器层。

---

## 5.2 Image Layers and Caching

```
DOCKERFILE TO LAYERS:
+==================================================================+
||                                                                ||
||  Dockerfile:                                                   ||
||  +----------------------------------------------------------+  ||
||  |  FROM ubuntu:20.04                                       |  ||
||  |  RUN apt-get update && apt-get install -y python3        |  ||
||  |  COPY requirements.txt /app/                             |  ||
||  |  RUN pip install -r /app/requirements.txt                |  ||
||  |  COPY . /app                                             |  ||
||  |  CMD ["python3", "/app/main.py"]                         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Resulting Layers:                                             ||
||  +----------------------------------------------------------+  ||
||  |  Layer 6: CMD metadata (no filesystem change)            |  ||
||  |  Layer 5: App code copied                    ~1 MB       |  ||
||  |  Layer 4: pip packages installed             ~50 MB      |  ||
||  |  Layer 3: requirements.txt copied            ~1 KB       |  ||
||  |  Layer 2: python3 installed                  ~200 MB     |  ||
||  |  Layer 1: ubuntu:20.04 base                  ~70 MB      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Each RUN, COPY, ADD creates a new layer                       ||
||  FROM brings in existing layers                                ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Dockerfile到层：

Dockerfile包含FROM ubuntu:20.04、RUN apt-get安装python3、COPY requirements.txt、RUN pip install、COPY应用代码、CMD。

结果层：层6是CMD元数据（无文件系统变化）、层5是复制的应用代码约1MB、层4是pip包约50MB、层3是requirements.txt约1KB、层2是安装的python3约200MB、层1是ubuntu:20.04基础约70MB。

每个RUN、COPY、ADD创建新层，FROM引入现有层。

### Why Instruction Order Matters

```
DOCKERFILE ORDERING FOR CACHING:
+==================================================================+
||                                                                ||
||  BAD ORDER (cache often invalidated):                          ||
||  +----------------------------------------------------------+  ||
||  |  FROM ubuntu:20.04                                       |  ||
||  |  COPY . /app                    <- Code changes often    |  ||
||  |  RUN apt-get update && ...      <- Must rebuild this     |  ||
||  |  RUN pip install ...            <- And this              |  ||
||  |  CMD [...]                                               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  When code changes: rebuild everything after COPY              ||
||  Every code change = 5+ minutes rebuild                        ||
||                                                                ||
||  GOOD ORDER (cache preserved):                                 ||
||  +----------------------------------------------------------+  ||
||  |  FROM ubuntu:20.04                                       |  ||
||  |  RUN apt-get update && ...      <- Rarely changes, cached|  ||
||  |  COPY requirements.txt /app/    <- Changes less often    |  ||
||  |  RUN pip install ...            <- Rebuilds if deps change
||  |  COPY . /app                    <- Code changes here     |  ||
||  |  CMD [...]                                               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  When code changes: only rebuild COPY . and after              ||
||  Every code change = seconds rebuild                           ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Dockerfile指令顺序影响缓存：

差的顺序（缓存经常失效）：COPY应用代码放在apt-get和pip之前。代码变化时必须重建apt-get和pip层。每次代码变化=5+分钟重建。

好的顺序（缓存保留）：apt-get放最前（很少变化，缓存）、COPY requirements.txt（变化较少）、pip install（依赖变化才重建）、COPY应用代码（代码在这里变化）。代码变化时只重建COPY之后的层。每次代码变化=秒级重建。

### Cache Invalidation Rules

```
CACHE INVALIDATION:
+------------------------------------------------------------------+
|                                                                  |
|  RULE: If a layer changes, ALL subsequent layers must rebuild   |
|                                                                  |
|  Layer 1: FROM ubuntu         [cached]                           |
|  Layer 2: RUN apt-get         [cached]                           |
|  Layer 3: COPY package.json   [CHANGED - file modified]          |
|  Layer 4: RUN npm install     [MUST REBUILD]                     |
|  Layer 5: COPY . /app         [MUST REBUILD]                     |
|  Layer 6: CMD                 [MUST REBUILD]                     |
|                                                                  |
|  WHAT TRIGGERS INVALIDATION:                                     |
|  +------------------------------------------------------------+  |
|  | COPY/ADD: File content or metadata changed                |  |
|  | RUN: Command string changed (even whitespace!)            |  |
|  | ENV: Value changed                                        |  |
|  | ARG: Value changed                                        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CACHE GOTCHA:                                                   |
|  +------------------------------------------------------------+  |
|  | RUN apt-get update         <- Cached! Even if repos changed
|  |                                                           |  |
|  | To force refresh:                                         |  |
|  | RUN apt-get update && apt-get install -y package          |  |
|  | (combine so apt-get install busts the cache)              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

缓存失效：

规则：如果一个层变化，所有后续层必须重建。

层1 FROM ubuntu [缓存]、层2 RUN apt-get [缓存]、层3 COPY package.json [变化-文件修改]、层4 RUN npm install [必须重建]、层5 COPY . [必须重建]、层6 CMD [必须重建]。

什么触发失效：COPY/ADD——文件内容或元数据变化、RUN——命令字符串变化（包括空格！）、ENV/ARG——值变化。

缓存陷阱：RUN apt-get update会被缓存！即使仓库已变化。强制刷新方法：RUN apt-get update && apt-get install -y package（合并使apt-get install打破缓存）。

---

## Summary

```
DOCKER IMAGES KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  IMAGE STRUCTURE:                                              ||
||  - Filesystem layers (read-only, content-addressed)            ||
||  - Metadata (environment, command, ports, etc.)                ||
||  - NOT a running process                                       ||
||                                                                ||
||  LAYERING BENEFITS:                                            ||
||  - Shared layers across images (disk efficiency)               ||
||  - Incremental downloads (pull only new layers)                ||
||  - Build caching (rebuild only changed layers)                 ||
||                                                                ||
||  DOCKERFILE BEST PRACTICES:                                    ||
||  - Order instructions from least to most frequently changed    ||
||  - Combine related RUN commands to reduce layers               ||
||  - Use .dockerignore to exclude unnecessary files              ||
||  - Understand cache invalidation rules                         ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Docker Containers](06-docker-containers.md)
