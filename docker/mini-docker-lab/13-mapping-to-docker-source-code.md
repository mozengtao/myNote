# 终章:把 Docker 的源码重新映射到今天学过的知识

> 这一章不是新实验,是整门课程的"收官对照表"。目标只有一个:让你下次打开 `runc`/`containerd`/`moby` 的源码时,不再觉得那是一堆陌生代码,而是能一眼认出"这段代码在做我亲手做过的哪个实验"。

---

## 1. 为什么要读这一章

前面十二章,你用 `unshare`、`ip netns`、`mount -t overlay`、手写 cgroup 文件、一个 Shell 脚本,重新实现了 Docker 的核心骨架。

这一章要做的事情很简单:把这些动手经验,和三个真实开源项目的源码目录对上号:

- [`opencontainers/runc`](https://github.com/opencontainers/runc) —— OCI Runtime 的参考实现,对应第二~十章你手工调用的系统调用。
- [`containerd/containerd`](https://github.com/containerd/containerd) —— 容器生命周期管理,对应第十二章 `containerd-shim` 那一层,以及第七章 OverlayFS 的"自动分层"部分。
- [`moby/moby`](https://github.com/moby/moby)(Docker Engine)—— 面向用户的 `dockerd`,对应第十二章顶层,以及第六章网络的"自动化"部分。

> 提醒:开源项目的目录结构会随版本演进而调整,下面给出的路径是撰写本课程时(2026 年)几个主流版本里能找到的**典型位置和典型思路**,用来建立"课程知识 <-> 源码位置"的直觉,不是逐行对照的 API 文档。实际阅读源码时,建议直接在仓库里用 `grep`/全局搜索关键字(比如 `CLONE_NEWPID`、`cpu.max`、`pivot_root`)去定位当前版本的具体实现。

---

## 2. 逐章映射表

### 第二~五章:Namespace(PID/UTS/Mount/Network)

对应 `runc` 的 `libcontainer` 包。

- 关键目录:`libcontainer/`、`libcontainer/specconv/`
- 关键概念:runc 读取 OCI `config.json` 里的 `linux.namespaces` 列表,转换成一组 `CLONE_NEW*` flag,最终通过 `clone()`/`unshare()` 系统调用创建对应的 Namespace——这和你在第二~五章手敲的 `unshare --pid`、`unshare --uts`、`unshare --mount`、`unshare --net` 是同一件事,只是 runc 用 Go(通过 cgo 调用底层 C 代码)一次性把多个 flag 组合在一次 `clone()` 调用里完成。
- 特别值得读的文件:`libcontainer/nsenter/nsexec.c`。这是一段用 C 写、通过 cgo 编译进 runc 二进制的代码,专门负责在 Go runtime 完全初始化、开始跑多线程之前,尽早完成 namespace 的创建/进入。这是一个很实际的工程细节:Go 程序默认是多线程的,而部分 Namespace 操作(尤其是 PID Namespace)在多线程环境下语义复杂、容易出错,所以 runc 选择在 C 代码层面、Go runtime 介入之前就把这件事做掉。

### 第四、七、八、九章:Mount Namespace + OverlayFS + chroot/pivot_root

这里要分两层看,分别对应"谁负责挂 OverlayFS"和"谁负责换根"。

- **换根这一步(第八、九章)**:`runc` 的 `libcontainer/rootfs_linux.go`,核心函数思路对应 `pivot_root` 的调用逻辑——先把新 rootfs bind mount 到自己身上(满足 `pivot_root` 的前置条件),再调用 `pivot_root()` 把旧根挪到一个临时目录,最后卸载并清理旧根的挂载点。这正是第八章思考题里提到的"`pivot_root` 比 `chroot` 多做的那一步"。
- **OverlayFS 分层这一步(第七章)**:**不在 runc 里**,而是在更上层完成。containerd 侧对应 `core/snapshots/overlay/`(overlay snapshotter,负责把镜像的多个层挂载成一个 `merged` 目录);较早期或独立使用 Docker Engine 时,这部分逻辑历史上在 moby 仓库的 `daemon/graphdriver/overlay2/` 里。等这个 `merged` 目录准备好之后,它的路径才会被写进 OCI `config.json` 的 `root.path` 字段,交给 runc 去做"换根"。**这正呼应了第七、八章刻意分开讲的原因**:OverlayFS 解决"内容从哪来",chroot/pivot_root 解决"怎么把这个内容变成新进程的根"——在真实架构里,这两件事分别由不同的组件负责。

### 第六章:veth + bridge

对应 Docker 的网络子系统(历史上是独立的 `moby/libnetwork` 仓库,现在代码并入了 `moby/moby`,通常在 `daemon/libnetwork/drivers/bridge/` 一带)。

- 关键概念:`bridge` driver 里会有专门负责"创建/配置网桥"(对应第六章 Step 1)、"创建 veth pair 并分配到容器 Network Namespace"(对应 Step 2、3)、"配置 iptables 规则(包括 MASQUERADE 和 `-p` 端口映射用到的 DNAT)"(对应 Step 4~6)的代码模块。第一次读这部分源码时,建议直接搜索 `veth`、`MASQUERADE`、`iptables` 这几个关键字定位。

### 第十章:cgroups

对应 `runc` 的 `libcontainer/cgroups/` 目录,内部通常按版本分包,例如面向 cgroup v2 的 `fs2` 子目录。

- 关键概念:每一种资源对应一个独立的小模块(CPU、memory、pids 等),每个模块的核心逻辑就是"把 OCI `config.json` 里 `linux.resources` 对应字段的数值,格式化后写入对应的 cgroup 控制文件"——例如把 CPU 配额写成 `<quota> <period>` 的格式写入 `cpu.max`,这和第十章你手动执行的 `echo "50000 100000" > cpu.max` 是同一个动作,只是包了一层 Go 的文件写入和错误处理逻辑。

### 第十一章:Mini Container 脚本 <-> runc 的 `create`/`start` 命令

`mini-docker.sh` 里 `setup_cgroup()` + `unshare ... chroot ...` 这个主流程,大致对应 runc 的 `create`(准备 Namespace/cgroup/rootfs,让容器进程停在"已创建但未运行用户命令"的状态)和 `start`(真正 `execve()` 用户命令)两个子命令背后的逻辑——只是 runc 把这两步拆开,是为了配合上层(containerd-shim)在两者之间做一些额外的状态同步和钩子(hook)调用。

### 第十二章:dockerd / containerd / containerd-shim 分层

- `dockerd` 主体在 `moby/moby` 仓库的 `daemon/` 目录,其中 `daemon/oci_linux.go`(或类似命名的文件)一类的代码,负责把 Docker 自己的容器配置(`HostConfig`、`Config`)"翻译"成标准的 OCI `config.json`——这一步就是把你在这门课里学到的一个个具体技术点(要不要隔离网络、限制多少内存),转换成 runc 能理解的标准化描述。
- `containerd-shim` 对应 `containerd/containerd` 仓库的 `core/runtime/v2/runc/`(shim v2 针对 runc 的实现),核心职责是启动/监控 runc、转发信号、在容器进程退出后正确 `wait()` 回收资源、并把状态汇报给上层 containerd。

---

## 3. 一张图收尾:把源码位置也标进最终架构图

```
docker run
    |
    v
dockerd  (moby/moby: daemon/)
    |         负责:HostConfig -> OCI config.json 翻译(namespaces/resources 字段)
    v
containerd  (containerd/containerd: core/*, 镜像与 snapshotter 管理)
    |         负责:core/snapshots/overlay -> 对应第七章 OverlayFS
    v
containerd-shim-runc-v2  (containerd: core/runtime/v2/runc)
    |         负责:长期看护容器进程,对应第十二章的父进程角色
    v
runc  (opencontainers/runc: libcontainer/)
    |         负责:nsenter -> 第二~五章 Namespace
    |               rootfs_linux.go -> 第八、九章 pivot_root
    |               cgroups/fs2 -> 第十章 cgroups
    v
Linux Kernel
    |
    v
Namespace + cgroups + OverlayFS(已挂载好的 merged 目录)
    |
    v
bash / nginx / 你的业务进程
```

---

## 4. 回答课程开头提出的问题

现在可以逐一回答这门课最初列出的九个问题了(建议你先自己口头回答一遍,再对照下面的答案):

**Docker 为什么不用 Hypervisor?**
因为它要隔离的粒度是"进程看到什么、能用多少资源",而不是"完整模拟一台硬件"。Namespace + cgroups 已经能在同一个内核之上实现足够强的隔离,不需要为每个容器单独跑一个内核(那是虚拟机要解决的问题),换来的是容器启动快、开销小。

**为什么 Container 本质就是一个 Linux Process?**
第一章亲眼验证过:宿主机的 `ps`、`kill` 能直接看到、操作容器主进程,内核里根本没有独立的"容器"对象,`docker inspect` 拿到的就是一个普通的宿主机 PID。

**为什么 Namespace 可以做到隔离?**
因为它把原本"全局共享一份"的内核状态(PID 表、hostname、挂载表、网络栈等),变成"每个 Namespace 各一份"。隔离的不是硬件资源,是"视图"。

**为什么 cgroups 可以做到资源限制?**
因为内核在进程真正申请 CPU 时间片/内存页/新 PID 的那一刻,会检查它所属 cgroup 的配额,超了就限流/杀死/拒绝——进程本身对此无感知也无法绕过。

**为什么 OverlayFS 可以做到 Image Layer?**
因为它用"多层叠加、只读共享、写时复制到最上层"的方式,让无数容器共享同一份只读基础镜像,同时各自的修改互不干扰,不需要物理复制整个文件系统。

**为什么 Container 可以几乎瞬间启动?**
因为"创建 Namespace"是内核里很轻量的操作(不是复制资源,只是切换指针/建新表),OverlayFS 挂载也不涉及数据复制,不需要像虚拟机那样启动一个完整的内核和引导流程。

**`docker run` 到底做了哪些事情?**
CLI 把命令转成 API 请求给 dockerd;dockerd 把镜像信息、资源限制、网络配置整理成 OCI Bundle 交给 containerd;containerd 通过 containerd-shim 调 runc;runc 创建 Namespace、挂 cgroup、`pivot_root` 切根,最后 `execve()` 出用户的命令。

**Container Runtime 到底干了什么?**
"Runtime"这个词在不同语境下所指略有不同:广义的容器运行时(如 containerd)负责镜像管理、容器生命周期、和上层编排系统(如 Kubernetes)对接;狭义的 OCI Runtime(如 runc)只负责"这一次"把一个已经准备好的 rootfs + 配置,变成一个真正跑起来的、被隔离和限制的进程。

**OCI Runtime 到底负责什么?**
只负责一件事:读一份标准化的 `config.json`(描述要哪些 Namespace、挂载什么、cgroup 限制多少、启动什么命令),然后严格按照这份描述把进程创建出来。它不管镜像怎么下载、怎么存储,也不管上层怎么编排——这也是为什么 `kata-containers`(用虚拟机实现隔离)、`runsc`/gVisor(用户态内核)也能实现同一份 OCI Runtime Spec,替换掉 `runc`,而上层 containerd/dockerd 几乎不用改。

---

## 5. 课程收尾:统一心智模型

回到 [README.md](README.md) 里的这张图,现在你应该能给它的每一个箭头都配上一个自己亲手做过的实验:

```
                         Docker
                            |
            +---------------+---------------+
            |               |               |
        Namespace        cgroups        OverlayFS
      (第二~五章:改变 View)  (第十章:内核收口)  (第七章:多层叠成一层)
            |               |               |
            +---------------+---------------+
                            |
                     一个普通 Linux Process
                          (第一章)
                            |
                       Linux Kernel
```

> Docker 并没有创造新的虚拟化技术,它只是将 Linux Kernel 已有的能力
> (Namespace、cgroups、OverlayFS、chroot、veth、bridge 等)
> 按照 OCI Runtime 的规范组织起来,
> 从而让一个普通 Linux 进程拥有了"像一台独立机器一样"的运行环境。

到这里,这门课程正式结束。建议的下一步:找一个真实的开源项目(比如 [`Liz Rice: containers-from-scratch`](https://github.com/lizrice/containers-from-scratch) 或者直接读 `runc` 源码),用今天建立的心智模型去读它——你会发现代码不再陌生。

---

## 6. 延伸阅读

- [`opencontainers/runc`](https://github.com/opencontainers/runc)
- [`opencontainers/runtime-spec`](https://github.com/opencontainers/runtime-spec)
- [`containerd/containerd`](https://github.com/containerd/containerd)
- [`moby/moby`](https://github.com/moby/moby)
- Liz Rice, *"Containers from Scratch"* 系列演讲/仓库
- Michael Kerrisk, *The Linux Programming Interface*(namespace、cgroup 相关章节)
