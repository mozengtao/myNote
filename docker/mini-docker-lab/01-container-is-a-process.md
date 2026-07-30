# 第一章:Container 到底是什么?

## 1. 学习目标

亲眼验证一件事:**一个正在运行的 Docker Container,在宿主机内核眼里,只是一个普通的 Linux Process。** 没有任何"容器对象"存在于内核里。

## 2. 前置知识

- 会执行 `docker run`、`docker ps`、`docker inspect`
- 知道 Linux 里 `ps`、`pstree` 怎么用
- 知道什么是 PID(进程号)

## 3. 原理简介

在学 Namespace、cgroups 之前,必须先破除一个误解:

> "Docker Container 是一种特殊的、被内核识别的运行实体。"

这是错的。Linux 内核里根本没有 `struct container` 这种东西。内核只认识 `task_struct`(进程/线程)。

Docker Container 之所以"看起来"像一台独立的机器,是因为这个进程被附加了几个特殊属性:

- 它属于一组特殊的 Namespace(看到的 PID、hostname、挂载点、网络都是"private view")
- 它被放进了一个 cgroup(资源使用被内核限制)
- 它的根目录 `/` 被换成了镜像的文件系统(chroot/pivot_root)

除掉这三样东西,剩下的就是一个**普通进程**,宿主机的 `ps`、`kill`、`/proc` 都能直接看到它、操作它。这一章只验证这句话本身,后面的章节才会逐个解释这三样东西怎么实现。

## 4. 实验步骤

### Step 1:启动一个容器,观察它在宿主机的样子

执行:

```bash
docker run -d --name mini-lab-1 busybox sleep 3600
```

观察:

```bash
docker inspect --format '{{.State.Pid}}' mini-lab-1
```

**为什么这么做**:`docker inspect` 里的 `.State.Pid` 是 Docker 记录的"这个容器主进程在宿主机上的真实 PID"。这一步是为了证明——Docker 内部本来就知道容器只是一个宿主机 PID,它只是把这个 PID 包了一层名字(`mini-lab-1`)和一堆元数据。

假设输出是 `12345`。

### Step 2:直接在宿主机用普通 Linux 工具看这个 PID

执行:

```bash
ps -p 12345 -o pid,ppid,cmd
ps -ef --forest | grep -A2 -B2 12345
```

观察:`ps` 能直接看到这个进程,`cmd` 列显示的是 `sleep 3600`,`ppid` 通常指向 `containerd-shim` 进程,而不是 `dockerd`。

**为什么会这样**:因为这个进程本来就活在宿主机的进程表里,`ps` 遍历的正是宿主机 `/proc`。Docker 没有把它"藏"起来,只是给它换了一层它自己看到的"视图"(下一章会讲)。

### Step 3:直接用宿主机命令操作它,不通过 docker 命令

执行:

```bash
kill -STOP 12345
docker ps          # 观察容器状态
kill -CONT 12345
```

观察:容器会被"冻结"(状态可能显示不健康或无响应),用 `docker ps` 你甚至能看到容器进程被暂停/恢复了,而你全程没有用任何 `docker` 命令去操作它,只用了最原始的 `kill`。

**为什么会这样**:`docker pause`/`docker unpause` 内部本质上就是通过 cgroup freezer(或直接信号)来控制这个宿主机进程。既然它是普通进程,普通的信号机制天然对它有效。

### Step 4:对比 Namespace 文件

执行:

```bash
sudo ls -l /proc/12345/ns/
sudo ls -l /proc/self/ns/
```

观察:两组输出格式一样,都是链接到类似 `pid:[4026531836]` 的字符串,但**数字(inode 号)不同**。

**为什么会这样**:`/proc/<pid>/ns/*` 下每个文件代表这个进程"属于哪一个 Namespace 实例"。数字不同,说明容器进程和你当前 shell 进程属于不同的 Namespace 实例——这就是"隔离"的全部本质:不是两个不同的世界,而是同一个内核给你们分别打开了两扇不同的窗户。这一点会在第二章详细展开。

## 5. 实验结果分析

- `docker inspect` 拿到的 PID,是宿主机的真实 PID,不是"容器内部"的什么虚拟号。
- `ps`、`kill` 这些跟容器毫无关系的原生 Linux 工具,可以直接看到、直接操作这个进程。
- 容器进程和普通进程的唯一结构性差异,现在能看到的只有 `/proc/<pid>/ns/*` 指向的 Namespace 编号不同。

这说明:**"容器"是一个运维/编排层面的概念(由 Docker/containerd 维护的一组元数据),而不是内核层面的实体。内核只看到进程。**

## 6. Docker 对应关系

```
docker run -d busybox sleep 3600
        |
        v
dockerd 调用 containerd (gRPC)
        |
        v
containerd 调用 containerd-shim-runc-v2
        |
        v
containerd-shim 调用 runc create/start
        |
        v
runc 调用 clone()/unshare() 等系统调用
        |
        v
最终变成宿主机 /proc 里的一个普通 PID
```

## 7. 心智模型

```
Container
    =
一个被贴上特殊标签的 Linux Process

不是:
一台被内核特殊识别的"虚拟机器"
```

## 8. 思考题

1. 如果 Container 只是一个普通进程,那 `docker stop` 和直接 `kill` 这个宿主机 PID,本质上有什么区别?(提示:想想 `docker stop` 会先发什么信号,等待多久,再发什么信号)
2. 为什么 `docker inspect` 能拿到宿主机 PID,而你在容器内部执行 `echo $$` 或 `ps` 看到的 PID 却通常是 `1`?这两个 PID 是同一个进程吗?
3. 如果内核根本不认识"容器"这个概念,那么 `docker ps` 展示的容器列表信息,到底存在哪里?

## 9. 延伸阅读

- `man 5 proc`(重点看 `/proc/[pid]/ns/` 一节)
- `man 1 ps`
- containerd 官方文档:[Container Runtime Concepts](https://containerd.io/docs/)

---

下一章:[第二章:PID Namespace](02-pid-namespace.md) —— 解释刚才看到的"PID 视图不同"到底是怎么回事。
