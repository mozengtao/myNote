# 第三章:UTS Namespace

## 1. 学习目标

用一个最简单的 Namespace(UTS)彻底巩固"Namespace = 改变 View"这个套路,并证明它和 PID Namespace 是完全独立、可以单独使用的。

## 2. 前置知识

- 第二章的心智模型:Namespace = 换一张视图给你看
- 知道 `hostname` 命令的作用

## 3. 原理简介

UTS 是个历史遗留名字(Unix Timesharing System),现在它只负责两个字段:

- `hostname`(主机名)
- `domainname`(NIS 域名,现代场景基本不用)

如果没有 UTS Namespace,所有进程共享同一个 hostname:

```
Host

hostname = "prod-server-01"

进程 A 改了 hostname -> 全局都变了,别的进程也看到新名字
```

这在单机多容器场景下明显不合理——你肯定希望每个容器能有自己的 hostname(比如 `docker run` 默认会把容器 ID 前缀设为 hostname),而不是所有容器共享宿主机的 hostname,或者互相能改对方看到的 hostname。

UTS Namespace 就是给这两个字段单独开一份"视图副本"。

## 4. 实验步骤

### Step 1:记录宿主机当前 hostname

执行:

```bash
hostname
```

记下这个值,比如 `my-laptop`。

**为什么这么做**:作为对照组,等下要证明修改不会影响它。

### Step 2:创建一个新的 UTS Namespace 并修改 hostname

执行:

```bash
sudo unshare --uts --fork -- bash
```

进入新 shell 后执行:

```bash
hostname mini-container-01
hostname
```

观察:第二次 `hostname` 输出 `mini-container-01`。

**为什么这么做**:`--uts` 创建一个新的 UTS Namespace。这里不需要 `--mount-proc`,因为 hostname 不依赖 `/proc` 挂载——它存的是内核里一个跟当前 UTS Namespace 绑定的字符串,`hostname` 命令通过 `sethostname()`/`gethostname()` 系统调用读写它,不涉及文件系统挂载。

### Step 3:验证宿主机没有被影响

不退出 Step 2 的 shell,新开一个终端(在宿主机的默认 Namespace 里)执行:

```bash
hostname
```

观察:输出的仍然是 Step 1 记录的原始值(比如 `my-laptop`),**没有变成** `mini-container-01`。

**为什么会这样**:宿主机 shell 和 Step 2 的 bash 现在属于两个不同的 UTS Namespace 实例。`sethostname()` 只修改"调用者当前所在 UTS Namespace"里的那份数据,不会传播到其他 Namespace。

### Step 4:回到 Step 2 的 shell,再退出,确认变化只是临时的

在 Step 2 的 shell 里执行:

```bash
exit
hostname
```

观察:回到宿主机默认 Namespace 后,`hostname` 又变回了原始值。

**为什么会这样**:`mini-container-01` 这个值只存在于那个已经消失的 UTS Namespace 里。Namespace 是跟着"最后一个引用它的进程"的生命周期走的——当 Step 2 的 bash 退出、没有其他进程再引用这个 Namespace 时,内核就会把它连同里面的 hostname 值一起释放掉。

## 5. 实验结果分析

- hostname 的修改被限定在了当前 UTS Namespace 内,对宿主机和其他 Namespace 完全不可见。
- 这证明了 Namespace 之间是相互独立的资源域(domain),每个域有自己的一份状态。
- Namespace 不是"持久化配置",它的生命周期和引用它的进程绑定,进程都退出了,Namespace 状态也就没了(除非有工具专门把它 bind mount 到文件系统上持久化,比如 `ip netns` 会做这件事,后面第五章会遇到)。

## 6. Docker 对应关系

```
今天实验:

unshare --uts --fork bash
hostname mini-container-01

        |
        v

Docker:

docker run --hostname=web01 nginx

        |
        v

runc 内部调用:

clone(CLONE_NEWUTS | ...)
之后调用 sethostname("web01")
```

容器里执行 `hostname` 或者查看 `/etc/hostname`,看到的正是这个被隔离过的独立值,和宿主机、和其他容器都互不干扰。

## 7. 心智模型

```
UTS Namespace
    =
让 hostname/domainname 这两个全局变量
从"全机器共享一份"
变成"每个 Namespace 一份"
```

这跟 PID Namespace 是完全一样的套路,只是隔离的对象从"进程编号表"换成了"两个字符串"。**这正是 Namespace 机制的通用模式:找一类原本全局共享的内核状态,让它可以按 Namespace 分组。**

## 8. 思考题

1. 为什么 UTS Namespace 不需要类似 `--mount-proc` 这样的额外挂载步骤,而 PID Namespace 需要?这跟这两类资源"是不是文件系统可见的"有什么关系?
2. 如果两个容器共享同一个 UTS Namespace(Docker 支持通过 `--uts=container:<name>` 共享),会发生什么?什么场景下你会希望这样做?
3. Linux 里除了 hostname,还有哪些"全局共享的状态"你觉得也应该能被 Namespace 化?(这其实就是在类比后面要学的 Mount、Network、IPC、User Namespace)

## 9. 延伸阅读

- `man 7 uts_namespaces`
- `man 2 sethostname`

---

下一章:[第四章:Mount Namespace](04-mount-namespace.md) —— 这一章的"View"会变得更有实用价值:改变一个进程能看到哪些挂载点。
