# Section 12: Common Beginner Mistakes

## 12.1 Treating Containers Like VMs

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  "I'll SSH into the container to install software"             ||
||  "I'll run multiple services in one container"                 ||
||  "I'll update the container and commit the changes"            ||
||                                                                ||
||  VM THINKING:                                                  ||
||  +----------------------------------------------------------+  ||
||  |  Create VM -> Install stuff -> Run services -> Manage    |  ||
||  |  - SSH in to fix things                                  |  ||
||  |  - Install patches manually                              |  ||
||  |  - Keep VM running long-term                             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CONTAINER THINKING:                                           ||
||  +----------------------------------------------------------+  ||
||  |  Build image -> Run container -> Problem? Rebuild image  |  ||
||  |  - Don't SSH (debug with logs, exec if needed)           |  ||
||  |  - Don't patch (build new image)                         |  ||
||  |  - Containers are disposable                             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

错误——把容器当VM：

VM思维：创建VM -> 安装东西 -> 运行服务 -> 管理。SSH进去修复、手动安装补丁、长期运行VM。

容器思维：构建镜像 -> 运行容器 -> 有问题？重建镜像。不SSH（用日志调试，必要时exec）、不打补丁（构建新镜像）、容器是可丢弃的。

---

## 12.2 Multiple Services in One Container

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  "I'll run nginx + app + redis in one container"               ||
||                                                                ||
||  BAD:                                                          ||
||  +----------------------------------------------------------+  ||
||  |  Container                                               |  ||
||  |  +-----+  +-----+  +-------+                             |  ||
||  |  |nginx|  | app |  | redis |                             |  ||
||  |  +-----+  +-----+  +-------+                             |  ||
||  |                                                          |  ||
||  |  PROBLEMS:                                               |  ||
||  |  - Which one is PID 1?                                   |  ||
||  |  - How to restart just nginx?                            |  ||
||  |  - How to scale just the app?                            |  ||
||  |  - Logs mixed together                                   |  ||
||  |  - One crashes, others affected                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  GOOD:                                                         ||
||  +----------------------------------------------------------+  ||
||  |  Container 1    Container 2    Container 3               |  ||
||  |  +-------+      +-------+      +-------+                 |  ||
||  |  | nginx |      |  app  |      | redis |                 |  ||
||  |  +-------+      +-------+      +-------+                 |  ||
||  |                                                          |  ||
||  |  BENEFITS:                                               |  ||
||  |  - Each has one concern                                  |  ||
||  |  - Scale independently                                   |  ||
||  |  - Update independently                                  |  ||
||  |  - Clearer logs                                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  RULE: One process per container (mostly)                      ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

错误——一个容器多服务：

差的做法：一个容器运行nginx+app+redis。问题：哪个是PID 1？如何只重启nginx？如何只扩展app？日志混在一起、一个崩溃影响其他。

好的做法：三个容器各运行一个服务。好处：每个只有一个关注点、独立扩展、独立更新、清晰日志。

规则：一个容器一个进程（大多数情况）。

---

## 12.3 Running as Root

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  Dockerfile that runs as root (default):                       ||
||  +----------------------------------------------------------+  ||
||  |  FROM ubuntu                                             |  ||
||  |  COPY app /app                                           |  ||
||  |  CMD ["/app"]                                            |  ||
||  |  # Process runs as root in container                     |  ||
||  |  # If container escaped, attacker is root on host!       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CORRECT:                                                      ||
||  +----------------------------------------------------------+  ||
||  |  FROM ubuntu                                             |  ||
||  |  RUN useradd -r -s /bin/false appuser                    |  ||
||  |  COPY --chown=appuser:appuser app /app                   |  ||
||  |  USER appuser                                            |  ||
||  |  CMD ["/app"]                                            |  ||
||  |  # Process runs as non-root                              |  ||
||  |  # Container escape = non-root user (much safer)         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  WHY IT MATTERS:                                               ||
||  - Container isolation is not perfect                          ||
||  - Kernel bugs can allow escape                                ||
||  - Non-root limits damage from compromise                      ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

错误——以root运行：

默认Dockerfile以root运行进程。如果容器逃逸，攻击者在主机上是root！

正确做法：创建非root用户，用USER切换。进程以非root运行，容器逃逸=非root用户（更安全）。

为什么重要：容器隔离不完美、内核bug可能允许逃逸、非root限制被攻破的损害。

---

## 12.4 Baking Secrets into Images

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  EXPOSING SECRETS:                                             ||
||                                                                ||
||  BAD: In Dockerfile                                            ||
||  +----------------------------------------------------------+  ||
||  |  ENV API_KEY=sk-1234567890abcdef                         |  ||
||  |  ENV DATABASE_PASSWORD=supersecret                       |  ||
||  |  # Secrets visible to anyone with image access!          |  ||
||  |  # docker history shows all ENV values                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  BAD: Copying secret files                                     ||
||  +----------------------------------------------------------+  ||
||  |  COPY .env /app/.env                                     |  ||
||  |  COPY aws_credentials /root/.aws/credentials             |  ||
||  |  # Secrets in image layer (even if deleted later!)       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CORRECT:                                                      ||
||  +----------------------------------------------------------+  ||
||  |  # Pass at runtime:                                      |  ||
||  |  docker run -e API_KEY=$API_KEY myapp                    |  ||
||  |                                                          |  ||
||  |  # Use Docker secrets (Swarm):                           |  ||
||  |  docker secret create api_key ./api_key.txt              |  ||
||  |                                                          |  ||
||  |  # Mount secrets file:                                   |  ||
||  |  docker run -v /secure/secrets:/secrets:ro myapp         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

错误——将密钥烘焙到镜像：

差的做法：在Dockerfile中用ENV或COPY密钥文件。密钥对任何有镜像访问权的人可见！docker history显示所有ENV值。密钥在镜像层中（即使稍后删除！）。

正确做法：运行时传递docker run -e、使用Docker secrets（Swarm）、挂载密钥文件docker run -v /secure/secrets:/secrets:ro。

---

## 12.5 Ignoring Image Size

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  "Image size doesn't matter, disk is cheap"                    ||
||                                                                ||
||  BLOATED IMAGE:                                                ||
||  +----------------------------------------------------------+  ||
||  |  FROM ubuntu:latest                                      |  ||
||  |  RUN apt-get update                                      |  ||
||  |  RUN apt-get install -y build-essential python3 nodejs   |  ||
||  |  RUN apt-get install -y vim curl wget git                |  ||
||  |  COPY . /app                                             |  ||
||  |  # Result: 2 GB image for a 10 MB app                    |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  WHY SIZE MATTERS:                                             ||
||  +----------------------------------------------------------+  ||
||  |  - Deployment time: 2 GB download vs 50 MB               |  ||
||  |  - Registry storage costs                                |  ||
||  |  - CI/CD pipeline time                                   |  ||
||  |  - Container startup (layer extraction)                  |  ||
||  |  - Attack surface (more packages = more CVEs)            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  OPTIMIZED IMAGE:                                              ||
||  +----------------------------------------------------------+  ||
||  |  FROM python:3.9-alpine                                  |  ||
||  |  COPY requirements.txt .                                 |  ||
||  |  RUN pip install --no-cache-dir -r requirements.txt      |  ||
||  |  COPY app.py .                                           |  ||
||  |  CMD ["python", "app.py"]                                |  ||
||  |  # Result: 50 MB image                                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

错误——忽略镜像大小：

膨胀镜像：使用ubuntu:latest、安装build-essential、python3、nodejs、vim、curl、wget、git。结果：10MB应用的2GB镜像。

为什么大小重要：部署时间（2GB下载vs50MB）、仓库存储成本、CI/CD流水线时间、容器启动（层解压）、攻击面（更多包=更多CVE）。

优化镜像：使用python:3.9-alpine、只安装需要的。结果：50MB镜像。

---

## 12.6 Using latest Tag in Production

```
THE MISTAKE:
+==================================================================+
||                                                                ||
||  FROM python:latest                                            ||
||                                                                ||
||  PROBLEMS:                                                     ||
||  +----------------------------------------------------------+  ||
||  |  DAY 1: python:latest = Python 3.9.7                     |  ||
||  |  Build works, tests pass, deployed.                      |  ||
||  |                                                          |  ||
||  |  DAY 30: python:latest = Python 3.10.0                   |  ||
||  |  Rebuild fails because dependency incompatible!          |  ||
||  |  Or worse: builds, but breaks in production.             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CORRECT:                                                      ||
||  +----------------------------------------------------------+  ||
||  |  FROM python:3.9.7-slim-buster                           |  ||
||  |  # Exact version: reproducible builds                    |  ||
||  |  # Update deliberately when ready                        |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  FOR YOUR OWN IMAGES:                                          ||
||  +----------------------------------------------------------+  ||
||  |  # Tag with version/commit                               |  ||
||  |  docker build -t myapp:v1.2.3 .                          |  ||
||  |  docker build -t myapp:abc123 .  # Git commit            |  ||
||  |                                                          |  ||
||  |  # NOT: docker build -t myapp:latest .                   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

错误——在生产中使用latest标签：

问题：第1天python:latest是Python 3.9.7构建成功。第30天python:latest是Python 3.10.0重建失败因为依赖不兼容！或更糟：构建成功但生产中出错。

正确做法：FROM python:3.9.7-slim-buster（精确版本：可重现构建、准备好时故意更新）。

对于自己的镜像：用版本/提交标记docker build -t myapp:v1.2.3。

---

## Summary

```
COMMON MISTAKES SUMMARY:
+==================================================================+
||                                                                ||
||  MISTAKE                   | CORRECT APPROACH                  ||
||  --------------------------+-----------------------------------||
||  Treat as VM               | Immutable, rebuild don't patch    ||
||  Multiple services         | One process per container         ||
||  Run as root               | Create and use non-root user      ||
||  Secrets in image          | Pass at runtime                   ||
||  Ignore image size         | Optimize with slim bases, stages  ||
||  Use :latest               | Use specific version tags         ||
||                                                                ||
||  ROOT CAUSE:                                                   ||
||  Most mistakes come from applying VM or traditional server     ||
||  thinking to containers. Containers are:                       ||
||  - Immutable (rebuild, don't modify)                           ||
||  - Single-purpose (one thing well)                             ||
||  - Ephemeral (disposable, recreatable)                         ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Docker vs Kubernetes](13-docker-vs-kubernetes.md)
