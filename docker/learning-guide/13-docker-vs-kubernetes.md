# Section 13: Docker vs Kubernetes (High-Level Only)

## 13.1 Docker Is Not Kubernetes

```
CLEAR DISTINCTION:
+==================================================================+
||                                                                ||
||  DOCKER:                                                       ||
||  +----------------------------------------------------------+  ||
||  |  - Container RUNTIME                                     |  ||
||  |  - Runs on ONE machine                                   |  ||
||  |  - Builds and runs containers                            |  ||
||  |  - docker-compose for simple multi-container             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  KUBERNETES:                                                   ||
||  +----------------------------------------------------------+  ||
||  |  - Container ORCHESTRATOR                                |  ||
||  |  - Runs across MANY machines (cluster)                   |  ||
||  |  - Schedules, scales, heals containers                   |  ||
||  |  - Production-grade container management                 |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  ANALOGY:                                                      ||
||  +----------------------------------------------------------+  ||
||  |  Docker = A shipping container                           |  ||
||  |  Kubernetes = The shipping company that manages          |  ||
||  |              thousands of containers across a fleet      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

清晰区分：

Docker：容器运行时、在一台机器上运行、构建和运行容器、docker-compose用于简单多容器。

Kubernetes：容器编排器、跨多台机器（集群）运行、调度、扩展、修复容器、生产级容器管理。

类比：Docker是一个货运集装箱，Kubernetes是管理船队上数千个集装箱的航运公司。

---

## 13.2 How They Relate

```
DOCKER AND KUBERNETES RELATIONSHIP:
+==================================================================+
||                                                                ||
||  Kubernetes uses container runtimes (like containerd):         ||
||                                                                ||
||  +----------------------------------------------------------+  ||
||  |                   KUBERNETES CLUSTER                     |  ||
||  |                                                          |  ||
||  |  +------------+  +------------+  +------------+          |  ||
||  |  |   Node 1   |  |   Node 2   |  |   Node 3   |          |  ||
||  |  |            |  |            |  |            |          |  ||
||  |  | kubelet    |  | kubelet    |  | kubelet    |          |  ||
||  |  |    |       |  |    |       |  |    |       |          |  ||
||  |  |    v       |  |    v       |  |    v       |          |  ||
||  |  | containerd |  | containerd |  | containerd |          |  ||
||  |  |    |       |  |    |       |  |    |       |          |  ||
||  |  |    v       |  |    v       |  |    v       |          |  ||
||  |  | [pods]     |  | [pods]     |  | [pods]     |          |  ||
||  |  +------------+  +------------+  +------------+          |  ||
||  |                                                          |  ||
||  |  Control Plane:                                          |  ||
||  |  API Server, Scheduler, Controller Manager, etcd         |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  WORKFLOW:                                                     ||
||  1. You build Docker images                                    ||
||  2. Push to registry                                           ||
||  3. Kubernetes pulls and runs them on cluster                  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

Docker和Kubernetes关系：Kubernetes使用容器运行时（如containerd）。

Kubernetes集群：多个节点（Node 1/2/3），每个节点运行kubelet -> containerd -> pods。控制平面：API Server、Scheduler、Controller Manager、etcd。

工作流：1）你构建Docker镜像，2）推送到仓库，3）Kubernetes在集群上拉取并运行它们。

---

## 13.3 When Docker Alone Is Enough

```
USE DOCKER (without Kubernetes) WHEN:
+------------------------------------------------------------------+
|                                                                  |
|  GOOD FIT FOR DOCKER ALONE:                                      |
|  +------------------------------------------------------------+  |
|  |  - Single server deployment                               |  |
|  |  - Small team, simple application                         |  |
|  |  - Development and testing                                |  |
|  |  - CI/CD pipelines                                        |  |
|  |  - Side projects, MVPs                                    |  |
|  |  - Applications that don't need high availability         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  NEED KUBERNETES WHEN:                                           |
|  +------------------------------------------------------------+  |
|  |  - Multi-server deployment                                |  |
|  |  - Auto-scaling required                                  |  |
|  |  - Self-healing (restart failed containers)               |  |
|  |  - Rolling updates with zero downtime                     |  |
|  |  - Service discovery across cluster                       |  |
|  |  - Complex networking requirements                        |  |
|  |  - Multi-team, microservices architecture                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  RULE OF THUMB:                                                  |
|  +------------------------------------------------------------+  |
|  |  Start with Docker + docker-compose                       |  |
|  |  Add Kubernetes when you outgrow single server            |  |
|  |  Don't use Kubernetes just because it's trendy            |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

何时Docker足够（无需Kubernetes）：单服务器部署、小团队简单应用、开发和测试、CI/CD流水线、副项目/MVP、不需要高可用的应用。

何时需要Kubernetes：多服务器部署、需要自动扩展、自愈（重启失败容器）、零停机滚动更新、跨集群服务发现、复杂网络需求、多团队微服务架构。

经验法则：从Docker+docker-compose开始、超出单服务器时加Kubernetes、不要因为流行就用Kubernetes。

---

## Summary

```
DOCKER VS KUBERNETES SUMMARY:
+==================================================================+
||                                                                ||
||  DOCKER:                                                       ||
||  - Build and run containers                                    ||
||  - Single machine scope                                        ||
||  - Simple orchestration with docker-compose                    ||
||                                                                ||
||  KUBERNETES:                                                   ||
||  - Orchestrate containers across clusters                      ||
||  - Multi-machine scope                                         ||
||  - Production-grade scaling, healing, networking               ||
||                                                                ||
||  RELATIONSHIP:                                                 ||
||  - Kubernetes uses container runtimes (containerd)             ||
||  - Docker images work in Kubernetes                            ||
||  - Complementary, not competing                                ||
||                                                                ||
||  START SIMPLE:                                                 ||
||  Docker alone -> docker-compose -> Kubernetes (if needed)      ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Open Source Repositories to Study](14-repositories.md)
