# Section 6: Docker Containers

## 6.1 What Happens When You Run a Container

```
docker run nginx - STEP BY STEP:
+==================================================================+
||                                                                ||
||  $ docker run nginx                                            ||
||                                                                ||
||  STEP 1: IMAGE RESOLUTION                                      ||
||  +----------------------------------------------------------+  ||
||  | "nginx" -> "nginx:latest" -> "docker.io/library/nginx"   |  ||
||  | Check local cache for image                              |  ||
||  | If not found: pull from registry                         |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  STEP 2: LAYER MOUNTING (Union Filesystem)                     ||
||  +----------------------------------------------------------+  ||
||  | Mount read-only image layers                             |  ||
||  | Create read-write container layer on top                 |  ||
||  | Result: merged filesystem view                           |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  STEP 3: NAMESPACE SETUP                                       ||
||  +----------------------------------------------------------+  ||
||  | Create PID namespace (container gets PID 1)              |  ||
||  | Create mount namespace (isolated filesystem)             |  ||
||  | Create network namespace (isolated network)              |  ||
||  | Create IPC namespace                                     |  ||
||  | Create UTS namespace (hostname)                          |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  STEP 4: CGROUP ASSIGNMENT                                     ||
||  +----------------------------------------------------------+  ||
||  | Create cgroup for container                              |  ||
||  | Apply memory limits (if specified)                       |  ||
||  | Apply CPU limits (if specified)                          |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  STEP 5: PROCESS EXECUTION                                     ||
||  +----------------------------------------------------------+  ||
||  | Set root filesystem (chroot-like)                        |  ||
||  | Set environment variables                                |  ||
||  | Set working directory                                    |  ||
||  | Execute: nginx -g "daemon off;"                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

docker run nginx - 逐步解析：

步骤1（镜像解析）："nginx" -> "nginx:latest" -> "docker.io/library/nginx"，检查本地缓存，未找到则从仓库拉取。

步骤2（层挂载-联合文件系统）：挂载只读镜像层、在顶部创建读写容器层、结果是合并的文件系统视图。

步骤3（Namespace设置）：创建PID namespace（容器获得PID 1）、创建mount namespace（隔离文件系统）、创建network namespace（隔离网络）、创建IPC namespace、创建UTS namespace（主机名）。

步骤4（Cgroup分配）：为容器创建cgroup、应用内存限制（如果指定）、应用CPU限制（如果指定）。

步骤5（进程执行）：设置根文件系统（类似chroot）、设置环境变量、设置工作目录、执行nginx -g "daemon off;"。

### Detailed Filesystem Setup

```
FILESYSTEM SETUP DETAIL:
+------------------------------------------------------------------+
|                                                                  |
|  BEFORE container run:                                           |
|  +------------------------------------------------------------+  |
|  |  /var/lib/docker/overlay2/                                |  |
|  |  ├── abc123/                    <- Image layer 1          |  |
|  |  │   └── diff/                                            |  |
|  |  │       ├── bin/                                         |  |
|  |  │       └── lib/                                         |  |
|  |  ├── def456/                    <- Image layer 2          |  |
|  |  │   └── diff/                                            |  |
|  |  │       └── etc/nginx/                                   |  |
|  |  └── ... (more image layers)                              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  AFTER container creation:                                       |
|  +------------------------------------------------------------+  |
|  |  /var/lib/docker/overlay2/                                |  |
|  |  ├── <container-id>/                                      |  |
|  |  │   ├── diff/          <- Writable layer (empty at start)|  |
|  |  │   ├── merged/        <- Unified view (what container   |  |
|  |  │   │                      sees as /)                    |  |
|  |  │   ├── work/          <- OverlayFS work directory       |  |
|  |  │   └── lower          <- Points to image layers         |  |
|  |  └── ...                                                  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Container process sees:                                         |
|  /  <- Actually /var/lib/docker/overlay2/<id>/merged/            |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

文件系统设置详情：

容器运行前：/var/lib/docker/overlay2/包含镜像层abc123（有bin/和lib/）、def456（有etc/nginx/）等。

容器创建后：/var/lib/docker/overlay2/<container-id>/包含diff/（可写层，开始时为空）、merged/（统一视图，容器看到的/）、work/（OverlayFS工作目录）、lower（指向镜像层）。

容器进程看到：/实际是/var/lib/docker/overlay2/<id>/merged/。

---

## 6.2 Containers as Processes

```
CONTAINER = PROCESS (with isolation):
+==================================================================+
||                                                                ||
||  HOST VIEW (ps aux):                                           ||
||  +----------------------------------------------------------+  ||
||  |  PID   COMMAND                                           |  ||
||  |  1     /sbin/init                                        |  ||
||  |  ...                                                     |  ||
||  |  5000  containerd                                        |  ||
||  |  5100  containerd-shim -namespace moby ...               |  ||
||  |  5115  nginx: master process nginx -g daemon off;        |  ||
||  |  5130  nginx: worker process                             |  ||
||  |  5131  nginx: worker process                             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CONTAINER VIEW (docker exec <id> ps aux):                     ||
||  +----------------------------------------------------------+  ||
||  |  PID   COMMAND                                           |  ||
||  |  1     nginx: master process nginx -g daemon off;        |  ||
||  |  15    nginx: worker process                             |  ||
||  |  16    nginx: worker process                             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  SAME PROCESS, different PID namespace views!                  ||
||                                                                ||
||  Host PID 5115 = Container PID 1                               ||
||  Host PID 5130 = Container PID 15                              ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

容器=进程（带隔离）：

主机视图(ps aux)：PID 1是/sbin/init、PID 5000是containerd、PID 5115是nginx主进程、PID 5130/5131是nginx worker进程。

容器视图(docker exec ps aux)：PID 1是nginx主进程、PID 15/16是nginx worker进程。

同一进程，不同PID namespace视图！主机PID 5115 = 容器PID 1，主机PID 5130 = 容器PID 15。

### The PID 1 Problem

```
PID 1 IN CONTAINERS:
+==================================================================+
||                                                                ||
||  IN NORMAL LINUX:                                              ||
||  +----------------------------------------------------------+  ||
||  |  PID 1 (/sbin/init or systemd) has special duties:       |  ||
||  |  - Reap orphaned zombie processes                        |  ||
||  |  - Handle signals (SIGTERM, SIGINT)                      |  ||
||  |  - Never exit (if it does, kernel panics)                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  IN CONTAINERS:                                                ||
||  +----------------------------------------------------------+  ||
||  |  Your application becomes PID 1!                         |  ||
||  |                                                          |  ||
||  |  PROBLEM 1: Zombie processes                             |  ||
||  |  If your app spawns children that exit,                  |  ||
||  |  and your app doesn't call wait(), zombies accumulate.   |  ||
||  |                                                          |  ||
||  |  PROBLEM 2: Signal handling                              |  ||
||  |  SIGTERM sent to container goes to PID 1                 |  ||
||  |  If your app ignores SIGTERM, docker stop waits,         |  ||
||  |  then sends SIGKILL after timeout (ungraceful)           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  SOLUTIONS:                                                    ||
||  +----------------------------------------------------------+  ||
||  |  1. Use init system (tini, dumb-init)                    |  ||
||  |     docker run --init nginx                              |  ||
||  |                                                          |  ||
||  |  2. Handle signals in your application                   |  ||
||  |     trap SIGTERM and gracefully shutdown                 |  ||
||  |                                                          |  ||
||  |  3. Use shell form carefully                             |  ||
||  |     CMD ["nginx"] vs CMD nginx                           |  ||
||  |     Exec form (first) makes nginx PID 1                  |  ||
||  |     Shell form makes /bin/sh PID 1, nginx is child       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

容器中的PID 1：

在正常Linux中：PID 1（/sbin/init或systemd）有特殊职责——收割孤儿僵尸进程、处理信号（SIGTERM、SIGINT）、永不退出（如果退出内核会panic）。

在容器中：你的应用成为PID 1！

问题1（僵尸进程）：如果你的应用产生子进程退出且不调用wait()，僵尸会累积。

问题2（信号处理）：发送到容器的SIGTERM去PID 1，如果你的应用忽略SIGTERM，docker stop等待然后超时后发送SIGKILL（不优雅）。

解决方案：1）使用init系统（tini、dumb-init）docker run --init。2）在应用中处理信号，捕获SIGTERM优雅关闭。3）小心使用shell形式——CMD ["nginx"]（exec形式使nginx成为PID 1）vs CMD nginx（shell形式使/bin/sh成为PID 1，nginx是子进程）。

### Why Containers Stop When Main Process Exits

```
CONTAINER LIFECYCLE:
+------------------------------------------------------------------+
|                                                                  |
|  Container state is TIED to PID 1:                               |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |  docker run nginx                                         |  |
|  |        |                                                  |  |
|  |        v                                                  |  |
|  |  [CREATED] -> [RUNNING] -> [EXITED]                       |  |
|  |                  |             ^                          |  |
|  |                  |             |                          |  |
|  |              PID 1 runs    PID 1 exits                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  When PID 1 exits:                                               |
|  1. Container transitions to EXITED state                        |
|  2. All other processes in container are killed                  |
|  3. Container's writable layer preserved (unless --rm)           |
|                                                                  |
|  COMMON MISTAKE:                                                 |
|  +------------------------------------------------------------+  |
|  |  Dockerfile:                                              |  |
|  |  CMD service nginx start                                  |  |
|  |                                                           |  |
|  |  What happens:                                            |  |
|  |  1. Shell starts as PID 1                                 |  |
|  |  2. "service nginx start" runs (starts nginx daemon)      |  |
|  |  3. Shell exits (command completed)                       |  |
|  |  4. Container stops (PID 1 exited!)                       |  |
|  |  5. nginx killed with container                           |  |
|  |                                                           |  |
|  |  FIX: nginx -g "daemon off;" (run in foreground)          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

容器生命周期：容器状态与PID 1绑定。

docker run nginx -> [CREATED] -> [RUNNING] -> [EXITED]。PID 1运行时容器运行，PID 1退出时容器退出。

当PID 1退出时：1）容器转到EXITED状态，2）容器中所有其他进程被杀，3）容器的可写层保留（除非--rm）。

常见错误：CMD service nginx start。发生的事：1）Shell作为PID 1启动，2）"service nginx start"运行（启动nginx守护进程），3）Shell退出（命令完成），4）容器停止（PID 1退出！），5）nginx随容器被杀。

修复：nginx -g "daemon off;"（前台运行）。

---

## 6.3 Container Lifecycle

```
CONTAINER STATES:
+==================================================================+
||                                                                ||
||                  docker create                                 ||
||                       |                                        ||
||                       v                                        ||
||              +----------------+                                ||
||              |    CREATED     |                                ||
||              +----------------+                                ||
||                       |                                        ||
||                docker start                                    ||
||                       |                                        ||
||                       v                                        ||
||              +----------------+    docker pause                ||
||              |    RUNNING     | ---------------+               ||
||              +----------------+                |               ||
||                |      |      |                 v               ||
||   docker stop  |      |      |  docker kill   +--------+      ||
||   (SIGTERM)    |      |      |  (SIGKILL)     | PAUSED |      ||
||                |      |      |                +--------+      ||
||                v      |      v                     |          ||
||                       |                  docker unpause        ||
||          (timeout)    |                            |           ||
||                       v                            |           ||
||              +----------------+ <------------------+           ||
||              |    EXITED      |                                ||
||              +----------------+                                ||
||                       |                                        ||
||                docker rm                                       ||
||                       |                                        ||
||                       v                                        ||
||              +----------------+                                ||
||              |    REMOVED     |                                ||
||              +----------------+                                ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

容器状态：

docker create -> CREATED（已创建）
docker start -> RUNNING（运行中）
docker pause -> PAUSED（已暂停）
docker unpause -> RUNNING
docker stop (SIGTERM) -> EXITED（已退出）
docker kill (SIGKILL) -> EXITED
docker rm -> REMOVED（已移除）

docker stop先发SIGTERM，超时后发SIGKILL。

---

## Summary

```
CONTAINERS KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  CONTAINER CREATION:                                           ||
||  1. Resolve image                                              ||
||  2. Mount filesystem layers                                    ||
||  3. Create namespaces (PID, mount, network, etc.)              ||
||  4. Configure cgroups                                          ||
||  5. Execute process                                            ||
||                                                                ||
||  CONTAINER = PROCESS:                                          ||
||  - Container IS a Linux process (with isolation)               ||
||  - Visible on host via ps                                      ||
||  - PID 1 in container = some PID on host                       ||
||                                                                ||
||  PID 1 RESPONSIBILITIES:                                       ||
||  - Handle signals                                              ||
||  - Reap zombies                                                ||
||  - Container stops when PID 1 exits                            ||
||                                                                ||
||  BEST PRACTICES:                                               ||
||  - Run processes in foreground (not as daemons)                ||
||  - Handle SIGTERM for graceful shutdown                        ||
||  - Consider using --init for proper signal handling            ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Dockerfile (From Zero)](07-dockerfile.md)
