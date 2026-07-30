# 从零实现 Mini Docker —— Learning by Building

> 不解释 Docker,而是带你亲手用 Linux 原生能力拼出一个 Docker。
> 拼完之后你会发现:
>
> **Docker 没有发明新的虚拟化技术,它只是把 Linux 已有的能力组合起来而已。**

---

## 这套课程是什么

这是一套 **动手实验课**,不是命令手册,也不是概念讲解文。

- 仓库里的 [`docker/learning-guide/`](../learning-guide/README.md) 回答的是"Docker 怎么用"。
- 这套 `mini-docker-lab/` 回答的是"Docker 为什么能工作"——通过让你亲手执行 `unshare`、`ip netns`、`mount -t overlay`、写 `cgroup.procs`,一步步拼出一个能跑 `bash` 的最小容器。

学完之后,你应该能自己回答:

- Docker 为什么不用 Hypervisor?
- 为什么 Container 本质就是一个 Linux Process?
- 为什么 Namespace 可以做到隔离?
- 为什么 cgroups 可以做到资源限制?
- 为什么 OverlayFS 可以做到 Image Layer?
- 为什么 Container 可以几乎瞬间启动?
- `docker run` 到底做了哪些事情?
- Container Runtime 到底干了什么?
- OCI Runtime 到底负责什么?

并且能默画出这张图,并解释每一层:

```
docker run
   |
   v
dockerd
   |
   v
containerd
   |
   v
runc
   |
   v
Linux Kernel
   |
   v
Namespace + cgroups + OverlayFS
   |
   v
bash
```

---

## 面向谁

假设你:

- 熟悉 Linux 基本命令、Shell
- 熟悉 C/C++、Python
- 有一些 Linux 网络开发经验
- **不**打算深入研究 Kernel 源码
- 目标不是"造一个 Docker 出来卖",而是"看懂 Docker 为什么成立"

因此本课程 **不会** 深入内核实现细节(比如 `clone()` 在内核里怎么分配 `task_struct`),而是聚焦三件事:

1. Linux 提供了什么能力
2. Docker 如何利用这些能力
3. 为什么这样组合以后就变成了 Container

---

## 环境要求(先读这个)

这套实验需要真正创建 namespace、挂载文件系统、操作 cgroup,**必须在一台你有 root 权限的真实 Linux(或 WSL2)上做**,不能在一些"沙箱化"的在线终端 / 受限容器里做(那些环境通常连 `unshare` 都会被 `Operation not permitted`)。

开始第一个实验前,请先完成 [`00-environment-check.md`](00-environment-check.md) 里的自检清单。

---

## 教学原则

本课程的每一章都遵循同一套原则,方便你预期每一章的"节奏":

1. **先观察现象,后解释原理**——先看到 `ps` 只有几个进程,再问"为什么",最后才讲 Namespace。
2. **一个实验只讲一个知识点**——绝不会把 Namespace 和 OverlayFS 混在一起讲。
3. **每个实验必答三个问题**——发生了什么?为什么会这样?Docker 哪里会用到?
4. **每个实验建立一个心智模型**——一句话记住这个技术"本质是什么",而不是"能干什么"。
5. **大量使用 ASCII 图**——不用文字堆砌解释。

---

## 章节总览

| 章节 | 文件 | 一句话知识点 | 预计时长 |
|---|---|---|---|
| 预备 | [00-environment-check.md](00-environment-check.md) | 确认环境能跑接下来所有实验 | 10 分钟 |
| 第一章 | [01-container-is-a-process.md](01-container-is-a-process.md) | Container 在宿主机眼里就是一个普通 Process | 20 分钟 |
| 第二章 | [02-pid-namespace.md](02-pid-namespace.md) | PID Namespace:改变"我能看到哪些进程" | 30 分钟 |
| 第三章 | [03-uts-namespace.md](03-uts-namespace.md) | UTS Namespace:改变"我的机器叫什么名字" | 20 分钟 |
| 第四章 | [04-mount-namespace.md](04-mount-namespace.md) | Mount Namespace:改变"我能看到哪些挂载点" | 30 分钟 |
| 第五章 | [05-network-namespace-isolation.md](05-network-namespace-isolation.md) | Network Namespace:改变"我有哪些网卡" | 25 分钟 |
| 第六章 | [06-network-veth-bridge.md](06-network-veth-bridge.md) | veth + bridge:把隔离的网络重新接回互联网 | 35 分钟 |
| 第七章 | [07-overlayfs.md](07-overlayfs.md) | OverlayFS:多个目录叠成一个目录 | 35 分钟 |
| 第八章 | [08-chroot.md](08-chroot.md) | chroot:改变"我的 `/` 指向哪里" | 25 分钟 |
| 第九章 | [09-namespace-plus-chroot.md](09-namespace-plus-chroot.md) | Namespace + chroot = 看起来像容器的 bash | 30 分钟 |
| 第十章 | [10-cgroups.md](10-cgroups.md) | cgroups:内核强制收口进程能用多少资源 | 40 分钟 |
| 第十一章 | [11-mini-container.md](11-mini-container.md) | 组合全部技术,得到 `mini-docker.sh` | 40 分钟 |
| 第十二章 | [12-docker-architecture-mapping.md](12-docker-architecture-mapping.md) | `docker run` 背后 dockerd/containerd/runc 分别干了什么 | 30 分钟 |
| 终章 | [13-mapping-to-docker-source-code.md](13-mapping-to-docker-source-code.md) | 把今天学的知识映射回 Docker/runc/containerd 真实源码目录 | 30 分钟 |

总计约 **12 个正式实验**,建议按顺序做,不要跳章——每一章都依赖上一章建立的心智模型。

---

## 统一心智模型(读完全部章节后回来看这张图)

```
                         Docker
                            |
            +---------------+---------------+
            |               |               |
        Namespace        cgroups        OverlayFS
        (改变 View)      (内核收口)     (多层叠成一层)
            |               |               |
            +---------------+---------------+
                            |
                     一个普通 Linux Process
                            |
                       Linux Kernel
```

**一句话总结整个课程:**

> Docker 并没有创造新的虚拟化技术,它只是将 Linux Kernel 已有的能力
> (Namespace、cgroups、OverlayFS、chroot、veth、bridge 等)
> 按照 OCI Runtime 的规范组织起来,
> 从而让一个普通 Linux 进程拥有了"像一台独立机器一样"的运行环境。

---

## 目录结构

```
docker/mini-docker-lab/
 |-- README.md                              <- 你在这里
 |-- 00-environment-check.md
 |-- 01-container-is-a-process.md
 |-- 02-pid-namespace.md
 |-- 03-uts-namespace.md
 |-- 04-mount-namespace.md
 |-- 05-network-namespace-isolation.md
 |-- 06-network-veth-bridge.md
 |-- 07-overlayfs.md
 |-- 08-chroot.md
 |-- 09-namespace-plus-chroot.md
 |-- 10-cgroups.md
 |-- 11-mini-container.md
 |-- 12-docker-architecture-mapping.md
 |-- 13-mapping-to-docker-source-code.md
 -- scripts/
      -- mini-docker.sh                     <- 第十一章的最终产物,可直接运行
```
