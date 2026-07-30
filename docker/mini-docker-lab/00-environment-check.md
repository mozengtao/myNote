# 预备:环境准备与自检

> 这一节不是实验,是"实验能不能做"的检查清单。跳过这一节,你可能在第二章就卡住,还搞不清楚是自己命令写错了,还是环境根本不支持。

---

## 为什么需要专门检查环境

后面所有实验都需要:

- 创建新的 Namespace(`unshare`/`clone` 系统调用)
- 挂载文件系统(`mount` 系统调用,包括 OverlayFS)
- 读写 `/sys/fs/cgroup` 下的控制文件
- 创建虚拟网卡(veth)、网桥(bridge)

这些操作在 Linux 里都需要 `CAP_SYS_ADMIN` / `CAP_NET_ADMIN` 之类的特权能力。很多"云端在线终端""IDE 内置终端""CI 容器"出于安全考虑,即使给你 root 用户,也会用 seccomp / 嵌套容器把这些系统调用挡掉。

**症状**:你以为自己是 root(`id` 显示 `uid=0`),执行 `unshare --pid --fork bash` 却报 `unshare: unshare failed: Operation not permitted`。这说明你所在的环境本身就是一层容器,不允许再创建新的 namespace——这不是命令写错了,是环境不支持,换一个真实的 Linux/WSL2/虚拟机才能继续。

---

## 自检清单

逐条在你打算做实验的机器上执行:

### 1. 内核版本

```bash
uname -r
```

要求 4.x 以上(本课程用到的 cgroup v2、OverlayFS 特性在较新的发行版默认都具备,例如 Ubuntu 20.04+、Debian 11+、WSL2 默认内核)。

### 2. 你是不是能拿到真正的特权

```bash
sudo unshare --pid --fork --mount-proc -- ps aux
```

**预期结果**:只看到 2~3 行进程(`ps` 自己和它的父进程)。

**如果报 `Operation not permitted`**:说明当前环境不支持创建新 namespace,需要换机器/换终端。常见"不支持"的环境包括:一些在线 IDE 的内置终端、被 seccomp 限制的 CI runner、未开启 nested virtualization 的某些云 IDE 容器。

### 3. cgroup v2 是否已挂载(unified hierarchy)

```bash
cat /sys/fs/cgroup/cgroup.controllers
```

**预期结果**:输出里应该包含 `cpu memory pids`,类似:

```
cpuset cpu io memory hugetlb pids rdma misc
```

本课程第十章的 cgroups 实验全部按 **cgroup v2** 语法编写(直接操作 `/sys/fs/cgroup/<name>/cgroup.procs`、`cpu.max`、`memory.max`、`pids.max`)。如果你的发行版还在用 cgroup v1(路径类似 `/sys/fs/cgroup/cpu/`、`/sys/fs/cgroup/memory/` 分开挂载),说明系统较老,建议升级发行版,本课程不再额外维护 v1 版本的命令。

### 4. OverlayFS 内核模块

```bash
grep overlay /proc/filesystems
```

**预期结果**:输出一行 `nodev	overlay`。如果没有,尝试 `sudo modprobe overlay`。

### 5. 网络工具

```bash
ip netns list        # 来自 iproute2,应该能正常执行(哪怕列表为空)
which iptables        # 第六章联网实验要用
```

如果 `ip` 命令不存在,安装 `iproute2`(Debian/Ubuntu:`sudo apt install iproute2`)。

### 6. Docker 本身(用来做对照实验)

```bash
docker version
```

第一章、第十二章需要用真实的 `docker run` 做对照观察,请确保 Docker 已安装且当前用户能执行(不一定要 root,在 `docker` 组里即可)。

---

## 如果你在 WSL2 上做实验

WSL2 本质上是一个真正的 Linux 内核(轻量虚拟机),因此:

- `unshare`、`mount`、cgroup v2 通常都可用。
- 网络实验(第六章 veth + bridge)在 WSL2 里可以做,但 WSL2 自己也用了一层 NAT 网络给外部世界,你会看到"两层 NAT"(容器 netns -> WSL2 -> Windows 主机 -> 外网),属于正常现象,不影响实验结论。
- 建议在 WSL2 里用 `sudo -i` 切到 root 再做实验,避免每条命令都要加 `sudo` 导致环境变量/PATH 不一致。

---

## 三个问题(本节没有正式实验,但仍然回顾一下)

**发生了什么?**
我们确认了当前机器具备创建 Namespace、挂载 OverlayFS、操作 cgroup v2、创建虚拟网络设备的能力。

**为什么会这样?**
这些能力都是 Linux 内核原生提供的系统调用/文件系统/虚拟设备,不需要额外安装"容器软件"——这恰恰是本课程要证明的核心事实的起点:Docker 依赖的地基,Linux 一直都有。

**Docker 哪里会用到?**
Docker/containerd/runc 在真正创建容器前,也会做几乎一样的能力探测(例如 runc 启动时检查 cgroup 版本、检查内核是否支持特定 namespace),只是这些检查被封装在了二进制内部,你平时看不到。

---

## 心智模型

```
"环境能不能跑容器"
        =
"内核允许不允许你调用这些系统调用"

不是:
"有没有装 Docker"
```

准备就绪后,进入 [第一章:Container 到底是什么](01-container-is-a-process.md)。
