# Docker 依赖的 Linux 内核核心技术
> **目标：理解 Docker 为什么能够工作，而不是研究 Linux Kernel 实现细节。**
>
> Docker 本身几乎没有发明新的虚拟化技术，它只是把 Linux 已有的几个能力组合起来：
>
> - Namespace —— 让进程"看见不同的世界"
> - cgroups —— 控制进程能使用多少资源
> - Union FS / OverlayFS —— 让镜像具有分层、共享、Copy-on-Write 能力

可以把 Docker 理解成：

```
                 Docker
                    │
      ┌─────────────┼─────────────┐
      │             │             │
 Namespace       cgroups      OverlayFS
 (隔离)          (限制)         (文件系统)
```

它们共同实现了：

```
               一个普通 Linux 进程
                      │
      ┌───────────────┴───────────────┐
      │                               │
 Namespace                     cgroups
 改变它看到的世界              控制它拥有多少资源
      │                               │
      └───────────────┬───────────────┘
                      │
                 OverlayFS
          提供自己的根文件系统
                      │
                 Docker Container
```

---

# Docker 的核心思想

Docker 并没有创建"虚拟机器"。

Docker 做的事情其实非常简单：

> **启动一个普通 Linux 进程，然后骗它认为自己就是一台机器。**

例如：

正常 Linux：

```
Host

/
├── etc
├── usr
├── proc
├── home
├── ...
```

Docker：

```
Container

/
├── etc
├── usr
├── proc
├── app
├── ...
```

Container 里的程序认为：

> 我看到的就是整个 Linux。

实际上：

```
Host Linux

┌────────────────────────────┐
│ bash                       │
│ nginx                      │
│ mysql                      │
│ container process          │
│ python                     │
└────────────────────────────┘
```

Container 只是 Host 上的一个普通进程。

所以：

> **Container = 被隔离起来的一组 Linux 进程。**

---

# Docker 三大支柱

```
            Docker Container

      Namespace
      "我是谁？"

            +

       cgroups
   "我能用多少资源？"

            +

      OverlayFS
    "我的文件长什么样？"
```

理解 Docker，其实就是理解这三个问题。

---

# 一、Namespace —— 隔离（Isolation）

## 它解决什么问题？

如果没有 Namespace：

```
Host

PID

1
2
3
4
5
6
```

容器里的程序：

```
ps

PID

1
2
3
4
5
6
```

它会看到宿主机所有进程。

这显然不行。

Docker 希望：

```
Container

ps

PID

1
2
3
```

它只能看到自己的进程。

Namespace 就是完成这件事。

---

## Namespace 的作用

Namespace：

> **给进程创造一个"独立视角"。**

注意：

不是创造新的资源。

而是：

**改变它看到的资源。**

例如：

Host：

```
CPU
Memory
Process
Network
Filesystem
Hostname
```

Container：

```
CPU
Memory
Process
Network
Filesystem
Hostname
```

实际上还是同一个 Linux。

只是：

Container 看见的是：

```
Host Resource
        │
 Namespace
        │
Container View
```

所以：

Namespace 更像：

> **戴上一副眼镜。**

不同眼镜：

看到不同世界。

资源没有复制。

只是：

**可见性变了。**

---

# Namespace 提供哪些隔离？

Docker 最常用：

```
PID Namespace

Process Isolation
```

```
Network Namespace

独立网卡
独立IP
独立Route
```

```
Mount Namespace

独立文件系统
```

```
UTS Namespace

Hostname
```

```
IPC Namespace

共享内存
消息队列
```

```
User Namespace

UID/GID
```

---

## 心智模型

```
Namespace

不是创造资源

而是：

决定你能看到哪些资源。
```

一句话：

> **Namespace = View（视图）**

---

# 二、cgroups —— 资源控制

Namespace：

解决：

> 我能看到什么？

但是：

Container 可以一直占 CPU。

一直申请内存。

整个 Host 都可能被拖垮。

所以需要：

cgroups。

---

## cgroups 的作用

控制：

```
CPU

Memory

IO

Network

Process Count
```

例如：

限制：

```
CPU

2 cores
```

限制：

```
Memory

2GB
```

限制：

```
最多100个线程
```

所以：

```
Host

16 CPU
64GB RAM

Container A

2 CPU
2GB

Container B

4 CPU
8GB

Container C

1 CPU
512MB
```

实际上：

还是共享 Host。

但是：

Linux Scheduler 会保证：

每个 Container：

只能使用规定资源。

---

## 为什么可以做到？

因为：

Container 本质就是：

```
Linux Process
```

Linux 本来就管理所有进程。

所以：

Linux 完全可以：

```
限制某些进程：

CPU

Memory

IO
```

Docker：

只是：

把 Container 放进一个 cgroup。

结束。

所以：

```
Container

↓

Linux Process

↓

加入某个 cgroup

↓

Kernel 自动限制资源
```

---

## 心智模型

Namespace：

改变世界。

cgroups：

限制能力。

一句话：

> **cgroups = Resource Budget（资源预算）**

---

# 三、Union FS / OverlayFS —— 分层文件系统

这是 Docker 最巧妙的设计。

---

## 为什么需要？

假设：

Ubuntu：

```
/

2GB
```

如果：

100 个 Container。

每个复制一份：

```
100 × 2GB

200GB
```

太浪费。

Docker 希望：

```
Ubuntu

只保存一份。
```

怎么办？

OverlayFS。

---

# OverlayFS 的思想

多个目录：

叠起来。

```
Upper Layer

Writable

--------

Lower Layer

Read Only

--------

Lower Layer

Read Only
```

最终：

用户看到：

```
Merged View
```

像：

```
一个目录
```

实际上：

来自很多层。

---

## Docker Image

例如：

```
Ubuntu

↓

Python

↓

Flask

↓

My App
```

每一步：

都是：

一个 Layer。

```
Layer4

App

--------

Layer3

Flask

--------

Layer2

Python

--------

Layer1

Ubuntu
```

启动 Container：

Docker：

不会复制。

只是：

```
Mount：

Layer1
Layer2
Layer3
Layer4
```

形成：

```
Merged Root FS
```

---

# Copy-on-Write（CoW）

Container：

不能修改：

Image。

所以：

Image：

全部：

Read Only。

如果：

修改：

```
/etc/passwd
```

不会：

改 Image。

而是：

复制：

```
Upper Layer

/etc/passwd
```

以后：

优先读取：

Upper。

Lower：

保持不变。

这就是：

```
Copy

↓

Modify

↓

Shadow
```

所以：

100 个 Container：

共享：

Ubuntu。

只有修改部分：

才真正占空间。

---

## 为什么速度快？

创建 Container：

传统 VM：

```
复制整个磁盘
```

Docker：

```
创建：

一个空 Upper Layer

+

Mount OverlayFS
```

几乎：

瞬间完成。

---

## 心智模型

OverlayFS：

不是：

复制文件。

而是：

> **把多个目录"叠"成一个目录。**

一句话：

> **OverlayFS = Layer（分层） + Copy-on-Write（按需复制）**

---

# 三者之间如何协同工作

Docker 启动容器时，大致会完成下面几件事：

```
                docker run

                     │
                     ▼
         创建 OverlayFS 根文件系统
                     │
                     ▼
        创建 Namespace（隔离视图）
                     │
                     ▼
        加入对应的 cgroup（资源限制）
                     │
                     ▼
         启动一个普通 Linux 进程
                     │
                     ▼
            Container 开始运行
```

从内核角度看，它仍然只是一个普通进程，只不过：
- **Namespace** 决定它"看到什么"；
- **cgroups** 决定它"能用多少"；
- **OverlayFS** 决定它"使用哪套文件系统"。

---

# 三者关系对比

| 技术 | 解决的问题 | 基本思想 | Docker 中承担的角色 |
|------|------------|----------|---------------------|
| Namespace | 为什么容器彼此隔离？ | 给进程提供独立的资源视图，而不是复制资源 | 隔离进程、网络、挂载点、主机名等 |
| cgroups | 为什么容器不会抢占所有资源？ | 把一组进程放入资源配额中，由内核统一调度和限制 | CPU、内存、I/O、进程数等资源限制 |
| Union FS / OverlayFS | 为什么镜像可以分层、容器启动快且节省空间？ | 多层目录叠加为一个统一视图，并通过 Copy-on-Write 延迟复制 | 镜像分层、容器可写层、镜像共享 |

---

# Docker 的整体心智模型

可以把 Docker 想象成给一个普通 Linux 进程穿上三件"装备"：

```
                    普通 Linux 进程
                           │
          ┌────────────────┼────────────────┐
          │                │                │
   Namespace          cgroups         OverlayFS
   改变视角           限制资源         提供文件系统
          │                │                │
          └────────────────┼────────────────┘
                           │
                    Docker Container
```

因此，Docker 的本质可以概括为一句话：

> **Docker 并不是创建了一台新的操作系统，而是在 Linux 内核上启动一个普通进程，并利用 Namespace 提供隔离、cgroups 提供资源控制、OverlayFS 提供分层文件系统，让这个进程拥有"像一台独立机器一样"的运行环境。**