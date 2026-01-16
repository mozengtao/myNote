# Section 7: Dockerfile (From Zero)

## 7.1 Dockerfile Mental Model

```
DOCKERFILE CONCEPT:
+==================================================================+
||                                                                ||
||  A Dockerfile is a BUILD RECIPE that describes:                ||
||  +----------------------------------------------------------+  ||
||  |  1. What base to start from (FROM)                       |  ||
||  |  2. What to install/configure (RUN)                      |  ||
||  |  3. What files to include (COPY/ADD)                     |  ||
||  |  4. How to run the application (CMD/ENTRYPOINT)          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  EXECUTION MODEL:                                              ||
||  +----------------------------------------------------------+  ||
||  |  Dockerfile is processed TOP TO BOTTOM                   |  ||
||  |  Each instruction creates a new layer                    |  ||
||  |  Layers are cached and reused                            |  ||
||  |                                                          |  ||
||  |  Instruction 1: FROM ubuntu                              |  ||
||  |       |                                                  |  ||
||  |       v creates Layer 1                                  |  ||
||  |  Instruction 2: RUN apt-get...                           |  ||
||  |       |                                                  |  ||
||  |       v creates Layer 2                                  |  ||
||  |  Instruction 3: COPY app /app                            |  ||
||  |       |                                                  |  ||
||  |       v creates Layer 3                                  |  ||
||  |  ...                                                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  IMPORTANT:                                                    ||
||  Dockerfile describes HOW TO BUILD the image,                  ||
||  not what happens when the container runs.                     ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Dockerfile概念：Dockerfile是构建配方，描述：1）从什么基础开始(FROM)，2）安装/配置什么(RUN)，3）包含什么文件(COPY/ADD)，4）如何运行应用(CMD/ENTRYPOINT)。

执行模型：Dockerfile从上到下处理，每条指令创建新层，层被缓存和重用。

重要：Dockerfile描述如何构建镜像，不是容器运行时发生什么。

---

## 7.2 Common Instructions (With Rationale)

### FROM - The Base

```
FROM INSTRUCTION:
+------------------------------------------------------------------+
|                                                                  |
|  SYNTAX: FROM <image>:<tag>                                      |
|                                                                  |
|  PURPOSE:                                                        |
|  - Sets the base image to build upon                             |
|  - Every Dockerfile must start with FROM                         |
|  - Brings in all layers from base image                          |
|                                                                  |
|  EXAMPLES:                                                       |
|  +------------------------------------------------------------+  |
|  |  FROM ubuntu:20.04      # Full Ubuntu (70 MB)             |  |
|  |  FROM python:3.9-slim   # Python with minimal Debian      |  |
|  |  FROM alpine:3.14       # Minimal Linux (5 MB)            |  |
|  |  FROM scratch           # Empty, for static binaries      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CHOOSING A BASE:                                                |
|  +------------------------------------------------------------+  |
|  |  Base           Size    When to Use                       |  |
|  |  -----------    ------  --------------------------------  |  |
|  |  scratch        0 MB    Static Go/Rust binaries           |  |
|  |  alpine         5 MB    Size matters, compatible app      |  |
|  |  slim variants  50 MB   Need more tools, smaller size     |  |
|  |  full OS        200 MB  Need many system packages         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

FROM指令：设置构建的基础镜像，每个Dockerfile必须以FROM开始，引入基础镜像的所有层。

示例：FROM ubuntu:20.04（完整Ubuntu 70MB）、FROM python:3.9-slim（Python带最小Debian）、FROM alpine:3.14（最小Linux 5MB）、FROM scratch（空的，用于静态二进制）。

选择基础：scratch 0MB用于静态Go/Rust二进制、alpine 5MB用于关注大小且兼容的应用、slim变体50MB需要更多工具但更小、完整OS 200MB需要很多系统包。

### RUN - Execute Commands

```
RUN INSTRUCTION:
+------------------------------------------------------------------+
|                                                                  |
|  SYNTAX:                                                         |
|  RUN <command>                   (shell form)                    |
|  RUN ["executable", "param"]     (exec form)                     |
|                                                                  |
|  PURPOSE:                                                        |
|  - Execute commands during IMAGE BUILD                           |
|  - Result becomes a new layer                                    |
|                                                                  |
|  GOOD PRACTICES:                                                 |
|  +------------------------------------------------------------+  |
|  |  BAD (creates 3 layers, apt cache in layer):              |  |
|  |  RUN apt-get update                                       |  |
|  |  RUN apt-get install -y nginx                             |  |
|  |  RUN apt-get clean                                        |  |
|  |                                                           |  |
|  |  GOOD (one layer, clean in same layer):                   |  |
|  |  RUN apt-get update && \                                  |  |
|  |      apt-get install -y --no-install-recommends nginx && \|  |
|  |      apt-get clean && \                                   |  |
|  |      rm -rf /var/lib/apt/lists/*                          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WHY COMBINE:                                                    |
|  - Fewer layers = smaller image                                  |
|  - Cleanup in same layer removes files from image                |
|  - Cleanup in separate layer just hides files (still in image)   |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

RUN指令：在镜像构建期间执行命令，结果成为新层。

好的做法对比：

差的做法（创建3层，apt缓存在层中）：分开写RUN apt-get update、RUN apt-get install、RUN apt-get clean。

好的做法（一层，同层清理）：合并写RUN apt-get update && apt-get install -y --no-install-recommends nginx && apt-get clean && rm -rf /var/lib/apt/lists/*。

为什么合并：更少层=更小镜像、同层清理从镜像移除文件、分层清理只是隐藏文件（仍在镜像中）。

### COPY vs ADD

```
COPY vs ADD:
+------------------------------------------------------------------+
|                                                                  |
|  COPY (preferred):                                               |
|  +------------------------------------------------------------+  |
|  |  COPY src dest                                            |  |
|  |  COPY package.json /app/                                  |  |
|  |  COPY . /app                                              |  |
|  |                                                           |  |
|  |  - Copies files from build context to image               |  |
|  |  - Simple, predictable                                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  ADD (use sparingly):                                            |
|  +------------------------------------------------------------+  |
|  |  ADD src dest                                             |  |
|  |  ADD archive.tar.gz /app/    # Auto-extracts!             |  |
|  |  ADD http://example.com/file /app/  # Downloads!          |  |
|  |                                                           |  |
|  |  - Same as COPY plus:                                     |  |
|  |  - Auto-extracts tar archives                             |  |
|  |  - Can download from URLs                                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  RECOMMENDATION:                                                 |
|  +------------------------------------------------------------+  |
|  |  - Use COPY for files and directories                     |  |
|  |  - Use ADD only when you need auto-extraction             |  |
|  |  - Don't use ADD for URL downloads (use RUN curl instead) |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

COPY vs ADD：

COPY（推荐）：COPY src dest，从构建上下文复制文件到镜像，简单可预测。

ADD（少用）：ADD src dest，与COPY相同但额外功能：自动解压tar档案、可从URL下载。

建议：使用COPY用于文件和目录、只在需要自动解压时使用ADD、不要用ADD下载URL（用RUN curl替代）。

### CMD vs ENTRYPOINT

```
CMD vs ENTRYPOINT:
+==================================================================+
||                                                                ||
||  CMD - Default command (can be overridden):                    ||
||  +----------------------------------------------------------+  ||
||  |  CMD ["nginx", "-g", "daemon off;"]                      |  ||
||  |                                                          |  ||
||  |  docker run nginx           # Runs: nginx -g "daemon off"|  ||
||  |  docker run nginx cat /etc  # Runs: cat /etc (overrides) |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  ENTRYPOINT - Fixed command (arguments appended):              ||
||  +----------------------------------------------------------+  ||
||  |  ENTRYPOINT ["nginx"]                                    |  ||
||  |                                                          |  ||
||  |  docker run nginx           # Runs: nginx                |  ||
||  |  docker run nginx -g daemon # Runs: nginx -g daemon      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  COMBINED (common pattern):                                    ||
||  +----------------------------------------------------------+  ||
||  |  ENTRYPOINT ["python"]                                   |  ||
||  |  CMD ["app.py"]                                          |  ||
||  |                                                          |  ||
||  |  docker run myapp           # Runs: python app.py        |  ||
||  |  docker run myapp test.py   # Runs: python test.py       |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  SHELL vs EXEC form:                                           ||
||  +----------------------------------------------------------+  ||
||  |  CMD nginx                   # Shell form: /bin/sh -c    |  ||
||  |  CMD ["nginx"]               # Exec form: direct exec    |  ||
||  |                                                          |  ||
||  |  PREFER EXEC FORM:                                       |  ||
||  |  - Process receives signals directly                     |  ||
||  |  - No shell interpretation                               |  ||
||  |  - Process is PID 1 (not shell)                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

CMD vs ENTRYPOINT：

CMD（默认命令，可覆盖）：CMD ["nginx", "-g", "daemon off;"]。docker run nginx运行nginx，docker run nginx cat /etc运行cat /etc（覆盖）。

ENTRYPOINT（固定命令，参数追加）：ENTRYPOINT ["nginx"]。docker run nginx运行nginx，docker run nginx -g daemon运行nginx -g daemon。

组合（常见模式）：ENTRYPOINT ["python"]和CMD ["app.py"]。docker run myapp运行python app.py，docker run myapp test.py运行python test.py。

Shell vs Exec形式：CMD nginx（Shell形式：/bin/sh -c）、CMD ["nginx"]（Exec形式：直接执行）。推荐Exec形式：进程直接接收信号、无shell解释、进程是PID 1（不是shell）。

### ENV and WORKDIR

```
ENV AND WORKDIR:
+------------------------------------------------------------------+
|                                                                  |
|  ENV - Set environment variables:                                |
|  +------------------------------------------------------------+  |
|  |  ENV NODE_ENV=production                                  |  |
|  |  ENV PATH="/app/bin:${PATH}"                              |  |
|  |                                                           |  |
|  |  - Available during build AND runtime                     |  |
|  |  - Can be overridden at runtime with -e                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  ARG - Build-time only variables:                                |
|  +------------------------------------------------------------+  |
|  |  ARG VERSION=1.0                                          |  |
|  |  RUN echo "Building version ${VERSION}"                   |  |
|  |                                                           |  |
|  |  - Only available during build                            |  |
|  |  - Not in final image (unless copied to ENV)              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WORKDIR - Set working directory:                                |
|  +------------------------------------------------------------+  |
|  |  WORKDIR /app                                             |  |
|  |  COPY . .        # Copies to /app                         |  |
|  |  RUN npm install # Runs in /app                           |  |
|  |                                                           |  |
|  |  - Creates directory if not exists                        |  |
|  |  - Affects all subsequent instructions                    |  |
|  |  - Better than RUN cd /app (doesn't persist)              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

ENV和WORKDIR：

ENV（设置环境变量）：ENV NODE_ENV=production，构建期间和运行时都可用，运行时可用-e覆盖。

ARG（仅构建时变量）：ARG VERSION=1.0，仅构建期间可用，不在最终镜像中（除非复制到ENV）。

WORKDIR（设置工作目录）：WORKDIR /app，如果不存在则创建目录，影响所有后续指令，比RUN cd /app好（cd不持久）。

---

## 7.3 Example: Minimal Real Dockerfile

```dockerfile
# Example: Python Flask Application
# ========================================

# Stage 1: Base with dependencies
FROM python:3.9-slim AS base

# Set environment variables
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1

# Create non-root user
RUN useradd --create-home --shell /bin/bash appuser

# Set working directory
WORKDIR /app

# Install dependencies first (changes less often)
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application code (changes more often)
COPY --chown=appuser:appuser . .

# Switch to non-root user
USER appuser

# Expose port (documentation)
EXPOSE 5000

# Health check
HEALTHCHECK --interval=30s --timeout=3s \
  CMD curl -f http://localhost:5000/health || exit 1

# Run the application
CMD ["python", "app.py"]
```

```
LINE-BY-LINE EXPLANATION:
+------------------------------------------------------------------+
|                                                                  |
|  FROM python:3.9-slim AS base                                    |
|  -> Uses slim Python image (smaller than full)                   |
|  -> AS base names this stage (for multi-stage builds)            |
|                                                                  |
|  ENV PYTHONDONTWRITEBYTECODE=1...                                |
|  -> Prevents .pyc files (saves space)                            |
|  -> Unbuffered output (better for logs)                          |
|  -> No pip cache (smaller image)                                 |
|                                                                  |
|  RUN useradd...                                                  |
|  -> Creates non-root user for security                           |
|                                                                  |
|  COPY requirements.txt (before COPY . .)                         |
|  -> Dependencies change less than code                           |
|  -> Enables layer caching for pip install                        |
|                                                                  |
|  RUN pip install...                                              |
|  -> Cached unless requirements.txt changes                       |
|                                                                  |
|  COPY --chown=appuser:appuser . .                                |
|  -> Copies with correct ownership                                |
|  -> Changes ownership in single layer                            |
|                                                                  |
|  USER appuser                                                    |
|  -> Runs as non-root (security best practice)                    |
|                                                                  |
|  HEALTHCHECK                                                     |
|  -> Allows Docker to check container health                      |
|                                                                  |
|  CMD ["python", "app.py"]                                        |
|  -> Exec form, python is PID 1                                   |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

逐行解释：

FROM python:3.9-slim AS base：使用slim Python镜像（比完整的小），AS base命名此阶段（用于多阶段构建）。

ENV设置：防止.pyc文件（节省空间）、无缓冲输出（更好的日志）、无pip缓存（更小镜像）。

RUN useradd：创建非root用户（安全）。

COPY requirements.txt（在COPY . .之前）：依赖变化比代码少，启用pip install的层缓存。

COPY --chown=appuser：以正确所有权复制，单层改变所有权。

USER appuser：以非root运行（安全最佳实践）。

HEALTHCHECK：允许Docker检查容器健康。

CMD ["python", "app.py"]：Exec形式，python是PID 1。

---

## Summary

```
DOCKERFILE KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  MENTAL MODEL:                                                 ||
||  - Dockerfile = build recipe, not runtime script               ||
||  - Each instruction = potential new layer                      ||
||  - Order matters for caching                                   ||
||                                                                ||
||  KEY INSTRUCTIONS:                                             ||
||  - FROM: Base image                                            ||
||  - RUN: Execute during build                                   ||
||  - COPY: Add files to image                                    ||
||  - CMD/ENTRYPOINT: Default runtime command                     ||
||  - ENV: Environment variables                                  ||
||  - WORKDIR: Working directory                                  ||
||                                                                ||
||  BEST PRACTICES:                                               ||
||  - Order: least -> most frequently changing                    ||
||  - Combine RUN commands to reduce layers                       ||
||  - Use exec form for CMD/ENTRYPOINT                            ||
||  - Run as non-root user                                        ||
||  - Use .dockerignore                                           ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Docker Networking](08-docker-networking.md)
