# 第十二章:Docker 架构对照分析

## 1. 学习目标

回到真实的 `docker run`,用 `pstree`/`ps --forest`、`docker inspect`、`/proc/<pid>/ns` 等工具,亲眼验证 `docker run` 背后 `dockerd -> containerd -> containerd-shim -> runc -> 容器进程` 这条链路是真实存在的进程树,并且逐层说清楚每一层到底负责什么、为什么要拆成这么多层。

## 2. 前置知识

- 第一章:容器是宿主机上的普通进程
- 第九、十一章:知道 runc 大致在做什么(Namespace + chroot/pivot_root + cgroups)
- 会用 `pstree`、`ps -ef --forest`

## 3. 原理简介

前面十一章我们一直在用 `unshare`/`chroot`/cgroup 文件手工"扮演 runc"。这一章反过来:观察真实的 Docker,验证它是不是真的按这个骨架实现的,以及为什么整个链路要拆成 dockerd、containerd、containerd-shim、runc 四层,而不是一个大程序全部搞定。

拆层的核心动机是**职责分离 + 容错**:

```
dockerd          面向用户:镜像管理、网络、volume、Compose/Swarm、CLI/API 兼容性
    |
containerd       面向"容器生命周期":镜像拉取与存储、容器元数据、跨运行时抽象(不止 runc)
    |
containerd-shim  面向"这一个容器进程":真正的父进程,dockerd/containerd 重启也不影响容器存活
    |
runc             面向"这一次启动":一次性工具,创建完 Namespace/cgroup/rootfs、exec 目标进程后就退出
    |
容器进程(比如 nginx)
```

一个关键设计:**`runc` 用完即走,不会常驻**。真正长期"看着"容器进程的,是 `containerd-shim`。这样即使升级/重启 `dockerd` 甚至 `containerd`,正在运行的容器也不会被杀掉——这是 Docker 从早期"重启 dockerd 会导致所有容器退出"这个痛点演化出来的架构决策。

## 4. 实验步骤

### Step 1:启动一个容器,观察真实的进程树

```bash
docker run -d --name mini-lab-12 nginx
pstree -p $(pgrep -f containerd$) 2>/dev/null || true
ps -ef --forest | grep -E "dockerd|containerd|nginx" | grep -v grep
```

**为什么这么做**:用 `--forest`/`pstree` 的树状展示,直接看清楚谁是谁的父进程,而不是靠猜。

**预期观察**(具体 PID 因机器而异,但层级关系应该一致):

```
dockerd
  \_ containerd
       \_ containerd-shim-runc-v2
            \_ nginx
                 \_ nginx
```

注意 `runc` 本身通常**看不到**了——它执行完创建工作、把容器进程 `exec` 出来之后就已经退出。

### Step 2:验证 containerd-shim 才是容器进程真正的父进程

```bash
NGINX_PID=$(docker inspect --format '{{.State.Pid}}' mini-lab-12)
ps -o pid,ppid,cmd -p $NGINX_PID
ps -o pid,ppid,cmd -p $(ps -o ppid= -p $NGINX_PID)
```

**观察**:`nginx` 主进程的父进程(`ppid`)不是 `dockerd`,也不是 `containerd`,而是一个 `containerd-shim-runc-v2` 进程。

**为什么会这样**:这印证了上面"拆层"里提到的关键设计——`containerd-shim` 是容器进程真正的父进程(在 Unix 进程模型里,父进程负责 `wait()` 回收子进程、转发某些信号)。`dockerd`/`containerd` 只是通过 gRPC 和 `containerd-shim` 通信来"遥控"容器,它们的重启不会导致 `containerd-shim` 或它看护的容器进程被杀死(`containerd-shim` 会被"过继"给 `init`/`containerd` 重新接管)。

### Step 3:验证 runc 只是"一次性工具"

```bash
docker run -d --name mini-lab-12b busybox sleep 60
sleep 2
ps -ef | grep runc | grep -v grep
```

**观察**:容器已经在正常运行(`docker ps` 能看到),但进程列表里几乎找不到存活的 `runc` 进程(容器刚启动的极短瞬间可能能抓到一次,过后就没有了)。

**为什么会这样**:`runc create`/`runc start` 完成创建 Namespace、配置 cgroup、执行 `pivot_root`、`execve()` 目标程序这一整套工作后,`runc` 进程本身的使命就结束了,会正常退出。真正持续存在、占用系统资源的,只有容器进程本身和看护它的 `containerd-shim`——这也是为什么容器启动之后几乎没有额外的常驻管理进程开销。

### Step 4:用 `/proc/<pid>/ns` 验证 Namespace 归属,和你自己手工做的实验对比

```bash
sudo ls -l /proc/$NGINX_PID/ns/
sudo ls -l /proc/self/ns/
```

**观察**:两组输出里,`pid`、`uts`、`mnt`、`net` 等条目指向的编号(inode)不同,和第一章的观察完全一致——只是这次的进程是由 `dockerd -> containerd -> containerd-shim -> runc` 这条真实链路创建出来的,而不是你手动执行的 `unshare`。

### Step 5:验证 cgroup 归属

```bash
cat /proc/$NGINX_PID/cgroup
sudo find /sys/fs/cgroup -maxdepth 3 -iname "*$(docker inspect --format '{{.Id}}' mini-lab-12)*" 2>/dev/null
```

**观察**:能找到一条以容器完整 ID 命名的 cgroup 路径,里面的 `cpu.max`、`memory.max` 等文件,和第十章你手工创建的 `mini-cpu`/`mini-mem`/`mini-pids` 是同一类文件,只是路径和命名规则由 Docker/containerd 自动生成和管理。

## 5. 实验结果分析

- 真实 `docker run` 之后形成的进程树,和这门课程反复强调的"容器就是被 Namespace/cgroup/chroot 包装过的普通进程"完全吻合,能用最原始的 `ps`/`pstree` 直接观察验证,没有任何"魔法"。
- `dockerd -> containerd -> containerd-shim -> runc` 这四层,分别对应"用户接口层"“生命周期与镜像管理层”“容器进程看护层”“一次性创建层”,拆分的核心目的是让"控制平面组件的重启/升级"和"数据平面(正在运行的容器)"解耦。
- `runc` 是这条链路里唯一直接调用本课程学过的系统调用(`clone`/`unshare`/`mount`/`pivot_root`/写 cgroup 文件)的一环,其余各层都是围绕它做编排、调度、状态管理。

## 6. Docker 对应关系

本章就是"对应关系"本身,这里直接给出用户在最开始要求理解的那张完整链路图,并标注每一层对应到本课程哪一章:

```
docker run
    |
    v
CLI 把命令转换成 REST API 请求
    |
    v
dockerd                         <- 镜像/网络/volume 管理(第六、七章涉及的网络与文件系统概念)
    |  (gRPC)
    v
containerd                      <- 容器生命周期、镜像存储管理
    |
    v
containerd-shim-runc-v2         <- 容器进程的真正父进程,负责长期看护
    |
    v
runc                             <- 一次性执行:
    |                               clone/unshare(第二~五章:Namespace)
    |                               mount/pivot_root(第四、七、八章:Mount NS + OverlayFS + chroot)
    |                               写 cgroup 文件(第十章:cgroups)
    v
Linux Kernel
    |
    v
容器进程(比如 nginx)的 bash/业务进程
```

## 7. 心智模型

```
Docker 的分层架构
    =
把"用户想要什么"(dockerd)
和"容器实际怎么活着"(containerd-shim)
和"容器怎么被造出来"(runc)
三件事解耦成三个独立进程

不是:
一个巨大的单体程序从头管到尾
```

## 8. 思考题

1. 如果你执行 `sudo systemctl restart docker`(重启 dockerd),正在运行的容器会不会被杀掉?结合这一章观察到的进程父子关系,解释原因。
2. `runc` 已经退出了,但容器还在正常提供服务(比如 nginx 还在监听 80 端口)。这说明容器进程的存活,依赖的是"某个进程一直存在并保持它不被回收",这个角色是谁?如果这个角色也挂了会发生什么?
3. `containerd` 被设计成不只支持 `runc`,理论上还能对接其他 OCI Runtime 实现(比如基于虚拟机的 `kata-containers`、基于 gVisor 的 `runsc`)。结合这门课的心智模型,你觉得"OCI Runtime Spec"这个标准,规定的到底是"用什么技术实现隔离",还是"隔离完成后需要满足什么行为约定"?为什么这个区别很重要?

## 9. 延伸阅读

- containerd 官方文档:[Architecture](https://containerd.io/docs/)
- runc 与 containerd-shim 的关系:[containerd/containerd `runtime/v2` 设计文档](https://github.com/containerd/containerd/blob/main/core/runtime/v2/README.md)
- OCI Runtime Specification:[github.com/opencontainers/runtime-spec](https://github.com/opencontainers/runtime-spec)

---

最后一章:[把 Docker 的源码重新映射到今天学过的知识](13-mapping-to-docker-source-code.md) —— 把本课程学到的每一个概念,对应到真实开源项目里的具体源码目录/文件。
