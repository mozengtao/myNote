# Section 14: Open Source Repositories to Study

## 14.1 Core Docker/Container Repositories

```
REPOSITORY LANDSCAPE:
+==================================================================+
||                                                                ||
||                    DOCKER ECOSYSTEM                            ||
||                                                                ||
||  User Interface:                                               ||
||  +----------------------------------------------------------+  ||
||  |  docker/cli        - Docker command line tool            |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  Docker Engine:                                                ||
||  +----------------------------------------------------------+  ||
||  |  moby/moby         - Docker engine (dockerd)             |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  Container Runtime:                                            ||
||  +----------------------------------------------------------+  ||
||  |  containerd/containerd  - Industry-standard runtime      |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  Low-Level Runtime:                                            ||
||  +----------------------------------------------------------+  ||
||  |  opencontainers/runc    - OCI reference implementation   |  ||
||  +----------------------------------------------------------+  ||
||                            |                                   ||
||                            v                                   ||
||  Specifications:                                               ||
||  +----------------------------------------------------------+  ||
||  |  opencontainers/runtime-spec  - How to run containers    |  ||
||  |  opencontainers/image-spec    - How to package images    |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

仓库全景：

用户接口：docker/cli（Docker命令行工具）。
Docker引擎：moby/moby（Docker引擎dockerd）。
容器运行时：containerd/containerd（行业标准运行时）。
低级运行时：opencontainers/runc（OCI参考实现）。
规范：opencontainers/runtime-spec（如何运行容器）、opencontainers/image-spec（如何打包镜像）。

---

## 14.2 What to Study in Each

### moby/moby (Docker Engine)

```
MOBY/MOBY - THE DOCKER ENGINE:
+------------------------------------------------------------------+
|  URL: https://github.com/moby/moby                               |
|                                                                  |
|  WHAT IT IS:                                                     |
|  - The upstream project for Docker Engine                        |
|  - "Moby" is the open source project, "Docker" is the product    |
|                                                                  |
|  WHAT TO READ:                                                   |
|  +------------------------------------------------------------+  |
|  |  /daemon/                - Docker daemon code              |  |
|  |  /api/                   - REST API definitions            |  |
|  |  /builder/               - Image build logic               |  |
|  |  /image/                 - Image storage/management        |  |
|  |  /container/             - Container lifecycle             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WHY STUDY:                                                      |
|  - Understand Docker's architecture decisions                    |
|  - See how API translates to container operations                |
|  - Learn production-quality Go code                              |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

moby/moby - Docker引擎：Docker Engine的上游项目。"Moby"是开源项目，"Docker"是产品。

阅读什么：/daemon/（守护进程代码）、/api/（REST API定义）、/builder/（镜像构建逻辑）、/image/（镜像存储/管理）、/container/（容器生命周期）。

为什么学习：理解Docker架构决策、看API如何转换为容器操作、学习生产质量的Go代码。

### containerd/containerd

```
CONTAINERD - CONTAINER RUNTIME:
+------------------------------------------------------------------+
|  URL: https://github.com/containerd/containerd                   |
|                                                                  |
|  WHAT IT IS:                                                     |
|  - Industry-standard container runtime                           |
|  - Used by Docker AND Kubernetes                                 |
|  - CNCF graduated project                                        |
|                                                                  |
|  WHAT TO READ:                                                   |
|  +------------------------------------------------------------+  |
|  |  /runtime/v2/           - Shim and runtime interface      |  |
|  |  /pkg/cri/              - Kubernetes CRI implementation   |  |
|  |  /snapshots/            - Filesystem snapshot drivers     |  |
|  |  /images/               - Image handling                  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WHY STUDY:                                                      |
|  - Core container lifecycle management                           |
|  - Interface between Docker/K8s and runc                         |
|  - gRPC API design patterns                                      |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

containerd - 容器运行时：行业标准容器运行时，被Docker和Kubernetes使用，CNCF毕业项目。

阅读什么：/runtime/v2/（Shim和运行时接口）、/pkg/cri/（Kubernetes CRI实现）、/snapshots/（文件系统快照驱动）、/images/（镜像处理）。

为什么学习：核心容器生命周期管理、Docker/K8s和runc之间的接口、gRPC API设计模式。

### opencontainers/runc

```
RUNC - LOW-LEVEL RUNTIME:
+------------------------------------------------------------------+
|  URL: https://github.com/opencontainers/runc                     |
|                                                                  |
|  WHAT IT IS:                                                     |
|  - Reference implementation of OCI runtime spec                  |
|  - Actually creates namespaces and cgroups                       |
|  - Executes the container process                                |
|                                                                  |
|  WHAT TO READ:                                                   |
|  +------------------------------------------------------------+  |
|  |  /libcontainer/         - Core container library          |  |
|  |  /libcontainer/cgroups/ - Cgroup implementation           |  |
|  |  /libcontainer/nsenter/ - Namespace entry                 |  |
|  |  /libcontainer/configs/ - Container configuration         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WHY STUDY:                                                      |
|  - See how Linux primitives are used                             |
|  - Understand the lowest level of containers                     |
|  - Learn about namespaces/cgroups in practice                    |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

runc - 低级运行时：OCI运行时规范的参考实现，实际创建namespaces和cgroups，执行容器进程。

阅读什么：/libcontainer/（核心容器库）、/libcontainer/cgroups/（Cgroup实现）、/libcontainer/nsenter/（Namespace进入）、/libcontainer/configs/（容器配置）。

为什么学习：看Linux原语如何使用、理解容器的最低级别、实践中学习namespaces/cgroups。

### OCI Specifications

```
OCI SPECIFICATIONS:
+------------------------------------------------------------------+
|  URLs:                                                           |
|  - https://github.com/opencontainers/runtime-spec                |
|  - https://github.com/opencontainers/image-spec                  |
|                                                                  |
|  WHAT THEY ARE:                                                  |
|  - Industry standards for container formats and runtimes         |
|  - Ensure interoperability between tools                         |
|                                                                  |
|  WHAT TO READ:                                                   |
|  +------------------------------------------------------------+  |
|  |  RUNTIME-SPEC:                                            |  |
|  |  - spec.md          - Container configuration format      |  |
|  |  - config.md        - What goes in config.json            |  |
|  |  - runtime.md       - Lifecycle operations                |  |
|  |                                                           |  |
|  |  IMAGE-SPEC:                                              |  |
|  |  - spec.md          - Overall image format                |  |
|  |  - layer.md         - Layer format (tar + gzip)           |  |
|  |  - manifest.md      - Image manifest format               |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WHY STUDY:                                                      |
|  - Understand what makes an "image" or "container"               |
|  - See the contract between tools                                |
|  - Design your own tools that work with containers               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

OCI规范：容器格式和运行时的行业标准，确保工具间互操作性。

阅读什么：
运行时规范：spec.md（容器配置格式）、config.md（config.json内容）、runtime.md（生命周期操作）。
镜像规范：spec.md（整体镜像格式）、layer.md（层格式tar+gzip）、manifest.md（镜像清单格式）。

为什么学习：理解什么构成"镜像"或"容器"、看工具间的契约、设计与容器协作的自己的工具。

### docker-library/official-images

```
OFFICIAL IMAGES:
+------------------------------------------------------------------+
|  URL: https://github.com/docker-library/official-images          |
|                                                                  |
|  WHAT IT IS:                                                     |
|  - Definitions for official Docker Hub images                    |
|  - nginx, python, postgres, etc.                                 |
|                                                                  |
|  WHAT TO READ:                                                   |
|  +------------------------------------------------------------+  |
|  |  /library/              - Image definition files          |  |
|  |                                                           |  |
|  |  Then look at the linked Dockerfiles:                     |  |
|  |  https://github.com/docker-library/python                 |  |
|  |  https://github.com/docker-library/nginx                  |  |
|  |  etc.                                                     |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  WHY STUDY:                                                      |
|  - Learn Dockerfile best practices from experts                  |
|  - See how to handle multiple architectures                      |
|  - Understand security and maintenance practices                 |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

官方镜像：官方Docker Hub镜像的定义（nginx、python、postgres等）。

阅读什么：/library/（镜像定义文件），然后查看链接的Dockerfile如docker-library/python、docker-library/nginx等。

为什么学习：从专家学习Dockerfile最佳实践、看如何处理多架构、理解安全和维护实践。

---

## Summary

```
LEARNING PATH FROM REPOSITORIES:
+==================================================================+
||                                                                ||
||  BEGINNER:                                                     ||
||  1. docker-library/official-images                             ||
||     -> Learn Dockerfile best practices                         ||
||                                                                ||
||  INTERMEDIATE:                                                 ||
||  2. opencontainers/runtime-spec                                ||
||     -> Understand container standards                          ||
||  3. opencontainers/runc                                        ||
||     -> See Linux primitives in action                          ||
||                                                                ||
||  ADVANCED:                                                     ||
||  4. containerd/containerd                                      ||
||     -> Container lifecycle management                          ||
||  5. moby/moby                                                  ||
||     -> Full Docker engine architecture                         ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Mental Model Summary](15-mental-model.md)
