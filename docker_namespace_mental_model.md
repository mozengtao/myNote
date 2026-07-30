# Docker 底层原理（Namespace / cgroups / Mount / Network）的统一心智模型

> **一句话总结：**
>
> Docker 没有发明新的虚拟化技术，它只是将 Linux Kernel 已有能力（Namespace、cgroups、OverlayFS、veth、bridge、pivot_root、chroot 等）按照 OCI Runtime 的规范组合起来，让一个普通 Linux Process 看起来像一台独立机器。

---

# 一、Docker 的整体架构

```
docker run

        │

        ▼

     dockerd
        │

        ▼

    containerd
        │

        ▼

       runc
        │

        ▼

Linux Kernel
        │
 ┌──────┼───────────────────────────┐
 │      │           │               │
 ▼      ▼           ▼               ▼
Namespace cgroups OverlayFS     Network
        │
        ▼
 一个普通 Linux Process
```

Docker 本身并不管理 CPU、Memory、PID、Network。

真正管理这些资源的是：

- Linux Kernel

Docker 只是：

- 创建 Namespace
- 创建 cgroup
- 创建 OverlayFS
- 创建网络
- 启动 Process

---

# 二、Container 到底是什么？

Container **不是 Kernel 对象。**

Kernel 根本不知道：

```
Container
Image
Volume
Bridge
```

Kernel 只知道：

```
task_struct
namespace
mount
socket
cgroup
inode
```

因此：

```
Container
      │
      ▼
Docker Runtime 的 Metadata
      │
      ▼
Kernel Resources
```

可以理解为：

```
Container

↓

Host PID
Mount Namespace
Network Namespace
OverlayFS
cgroup
```

所以：

**Container = 一条 Metadata + 一组 Kernel Resource 的引用。**

---

# 三、Docker Metadata 存在哪里？

Docker 自己维护：

```
/var/lib/docker/

containers/

overlay2/

network/

volumes/
```

例如：

```
Container

ID
Name
Image
Host PID
State
Network
Mount
```

因此：

```
docker ps
```

并不是：

```
询问 Kernel：

有哪些 Container？
```

而是：

```
读取 Metadata

↓

检查 Host PID 是否还存在

↓

更新 Running / Exited

↓

输出 docker ps
```

所以：

Container 是 Docker Runtime 的概念。

不是 Kernel 的概念。

---

# 四、PID Namespace

## 核心思想

PID Namespace：

并不是：

```
复制 Process
```

而是：

```
给同一个 Process

不同 Namespace

不同编号
```

例如：

```
task_struct

      │

 ┌────┴────┐

 │         │

Host     Container

PID=34567 PID=1
```

这是：

同一个 Process。

不是两个。

---

## 为什么不用复制 Process？

复制意味着：

```
新的 task_struct
新的 Memory
新的 fd table
新的 Socket
```

开销巨大。

Namespace：

只是：

```
增加：

PID Mapping
```

Kernel 并没有复制任何 Process。

因此：

Container 启动速度几乎等于：

```
fork()

+

execve()
```

---

## PID Namespace 为什么需要重新 mount /proc？

PID Namespace：

已经生效：

```
Process Visibility
```

但是：

```
ps

↓

读取：

/proc
```

因此：

必须：

```
mount -t proc proc /proc
```

让 procfs：

重新根据：

当前 PID Namespace

生成新的内容。

所以：

```
PID Namespace

↓

决定可见 Process

↓

procfs

↓

把结果展示成文件
```

因此：

`/proc` 是：

**PID Namespace 的窗口。**

不是 PID Namespace 本身。

---

# 五、为什么 docker inspect 看到 Host PID，而容器里看到 PID=1？

因为：

```
Host Namespace

PID=34567

↓

Container Namespace

PID=1
```

两个编号：

对应：

同一个：

```
task_struct
```

Docker：

保存的是：

```
Host PID
```

Container：

看到的是：

```
Namespace PID
```

---

# 六、docker stop 的真正过程

```
docker stop

        │

SIGTERM

        │

等待（默认10秒）

        │

退出了吗？

      │

   Yes│No

      ▼

 SIGKILL
```

Docker：

最终：

还是：

```
kill(pid,SIGTERM)
```

没有新的停止机制。

---

# 七、为什么 PID1 很特殊？

PID1：

Linux 默认认为：

```
Namespace 的 init
```

它：

需要：

- 收养孤儿进程
- wait() 回收 Zombie
- 正确处理 Signal

因此：

很多程序：

如果直接变成 PID1：

收到：

```
SIGTERM
```

可能：

不会正常退出。

Docker：

于是：

等待：

10 秒。

最后：

```
SIGKILL
```

---

## tini / dumb-init 的作用

它们：

专门做：

```
Signal Forward

+

wait()

↓

回收 Zombie
```

因此：

很多官方镜像：

都会：

```
ENTRYPOINT tini
```

---

# 八、Mount Namespace

## 核心思想

Mount Namespace：

隔离的不是：

```
文件
```

而是：

```
Mount Tree
```

即：

整个：

挂载关系。

---

## 创建过程

第一步：

```
clone(CLONE_NEWNS)
```

复制：

```
Mount Tree
```

不是：

复制磁盘。

第二步：

```
pivot_root()

或者

chroot()
```

切换：

RootFS。

第三步：

重新 mount：

```
proc
sysfs
tmpfs
bind mount
```

最终：

形成：

Container RootFS。

---

## 为什么 /proc、/sys、/dev 一开始看起来像宿主机？

因为：

最开始：

继承：

```
Mount Tree
```

随后：

runc：

重新组织：

```
pivot_root()

↓

重新 mount

↓

最终形成容器 RootFS
```

---

# 九、Bind Mount 与 tmpfs

共同点：

都是：

```
mount()
```

都是：

Mount Tree：

上的：

一个 Mount。

---

## tmpfs

```
mount -t tmpfs
```

Kernel：

创建：

新的：

Memory Filesystem。

每次：

都是：

新的实例。

---

## Bind Mount

```
mount --bind

Host Path

Container Path
```

不是：

复制目录。

而是：

```
增加：

新的访问入口
```

例如：

```
Host

/home/data

       ▲

       │

bind

       │

Container

/data
```

因此：

Container：

修改：

```
/data
```

Host：

立即：

看到。

因为：

共享：

同一个 inode。

---

## 总结

| tmpfs | bind mount |
|--------|------------|
| 新建文件系统 | 引用已有路径 |
| 生命周期跟挂载一致 | 生命周期跟源目录一致 |
| 默认不会影响宿主机磁盘 | 修改直接影响宿主机 |

---

# 十、Network Namespace

## 核心思想

Network Namespace：

隔离的不只是：

```
网卡
```

而是：

```
完整 Network Stack
```

包括：

```
lo
eth0

Routing Table

ARP Cache

TCP Table

UDP Table

iptables

conntrack
```

可以理解成：

```
每个 Namespace

=

一台独立 Linux
```

---

## 为什么 ip netns add 要 bind mount？

```
unshare --net
```

创建：

```
Namespace

↓

依赖 Process
```

Process：

退出：

Namespace：

消失。

---

而：

```
ip netns add
```

创建：

```
Namespace

↓

bind mount

↓

保持引用

↓

即使没有 Process

仍然存在
```

为什么？

因为：

Docker：

需要：

```
先创建 Namespace

↓

创建 veth

↓

配置 IP

↓

配置 Route

↓

配置 DNS

↓

最后

启动 Process
```

所以：

Network Namespace：

必须：

能够：

脱离：

Process：

独立存在。

---

# 十一、为什么两个容器都能监听 TCP 80？

真正：

Kernel：

检查的是：

```
(Network Namespace,

IP,

Port)
```

不是：

```
Port
```

因此：

```
Container A

TCP Table

80

OK
```

```
Container B

TCP Table

80

OK
```

两个：

监听：

80。

互不冲突。

因为：

TCP Table：

不同。

---

# 十二、为什么 curl localhost 能工作？

每个：

Network Namespace：

都有：

自己的：

```
lo
```

刚创建时：

```
DOWN
```

Docker：

会：

自动：

```
ip link set lo up
```

于是：

```
curl localhost

↓

127.0.0.1

↓

lo

↓

TCP Stack

↓

Container 自己监听的服务
```

整个：

请求：

没有：

离开：

当前 Namespace。

---

# 十三、UTS Namespace

UTS：

隔离：

```
hostname

domainname
```

程序：

直接：

调用：

```
gethostname()
```

Kernel：

返回：

当前：

UTS Namespace：

中的：

hostname。

因此：

不像：

PID Namespace：

需要：

重新 mount：

```
/proc
```

---

## 为什么可以共享 UTS Namespace？

Docker：

支持：

```
--uts=container:<name>
```

于是：

两个：

Container：

共享：

同一个：

```
hostname
```

例如：

```
Container A

hostname=db01
```

Container B：

立即：

也是：

```
db01
```

因为：

共享的是：

同一个：

UTS Namespace Object。

---

适用场景：

- Sidecar
- Debug Container
- Kubernetes Pod

---

# 十四、Namespace 的统一设计哲学

所有 Namespace：

其实都在做同一件事情：

把：

```
全局唯一

↓

变成

每个 Namespace 一份
```

例如：

| 全局资源 | Namespace 后 |
|-----------|--------------|
| PID | PID Namespace |
| Mount Tree | Mount Namespace |
| Network Stack | Network Namespace |
| hostname | UTS Namespace |
| IPC | IPC Namespace |
| User ID Mapping | User Namespace |
| cgroup View | Cgroup Namespace |
| Time | Time Namespace |

---

# 十五、为什么 Container 启动如此快？

VM：

```
创建 VM

↓

BIOS

↓

Kernel

↓

systemd

↓

bash
```

Docker：

```
clone()

↓

Namespace

↓

OverlayFS

↓

cgroup

↓

execve()
```

Docker：

没有：

启动：

新的 Kernel。

只是：

启动：

一个普通：

Linux Process。

---

# 十六、最终统一心智模型（建议牢记）

```
                        Docker

                           │

                  Docker Runtime

       (Container Metadata & Lifecycle)

                           │

      ┌────────────────────┼────────────────────┐
      │                    │                    │
      ▼                    ▼                    ▼
 Namespace             OverlayFS            cgroups
      │                    │                    │
      │                    │                    │
 View / Isolation      RootFS View        Resource Limit
      │                    │                    │
      └────────────────────┼────────────────────┘
                           │
                           ▼
                 Linux Kernel Resources
      (task_struct / namespace / mount / socket)
                           │
                           ▼
                  一个普通 Linux Process
```

> **一句话总结整个 Docker：**
>
> Docker 从来没有创造新的"容器"对象，它只是把 Linux Kernel 已有的 Namespace（隔离视图）、Mount/OverlayFS（隔离文件系统视图）、Network（隔离网络协议栈）和 cgroups（限制资源）组合起来，再用 Docker Runtime 维护容器元数据与生命周期，从而让一个普通 Linux 进程拥有了"像一台独立机器一样"的运行环境。