# 第二章:PID Namespace

## 1. 学习目标

理解 PID Namespace:一个进程如何"看到自己是 1 号进程",却完全不知道宿主机上还有几千个其他进程在跑。

## 2. 前置知识

- 第一章的结论:Container 是一个普通进程
- 知道 `ps`、`/proc` 的基本用法
- 知道 Linux 里 PID 1 有特殊地位(init 进程,负责回收孤儿进程)

## 3. 原理简介

正常情况下,Linux 所有进程共享**同一张 PID 表**:

```
Host

PID 表
1    systemd
...
2050 sshd
2103 bash
2200 ps
```

任何进程执行 `ps`,看到的都是这同一张表。这在多租户场景下是个问题:租户 A 的进程能看到、甚至能对租户 B 的进程发信号(如果权限够的话),租户 A 也能看到宿主机跑了哪些"不该看到"的东西。

PID Namespace 解决的问题很简单:**给一组进程一张"专属的、从 1 开始编号"的 PID 表**,这张表只包含它们自己和它们的后代,看不到、也影响不到表外的进程。

```
Host PID Namespace

PID
100  bash
101  ps
102  (new pid ns 的入口进程,在宿主机里编号是102)

                  |
                  | unshare(CLONE_NEWPID)
                  v

新的 PID Namespace(在 102 内部)

PID
1    bash    <- 就是宿主机的 102,但在这里自己看到的编号是 1
2    ps
```

关键点:**这不是复制了一份进程,也不是创建了两个 bash**。它还是宿主机上编号 102 的那一个进程,只是它和它的子孙进程,查看/操作 PID 时看到的是另一套编号规则。这就是为什么标题反复强调 Namespace 改变的是"View"(视图),不是资源本身。

## 4. 实验步骤

### Step 1:在没有 Namespace 隔离的情况下先看一眼"正常世界"

执行:

```bash
ps aux | wc -l
```

观察:这台机器上可能有几十到上百个进程。

**为什么这么做**:先建立一个对照组,等下进入新 Namespace 后再对比这个数字的巨大落差。

### Step 2:创建一个新的 PID Namespace

执行:

```bash
sudo unshare --pid --fork --mount-proc -- bash
```

进入新 shell 后执行:

```bash
ps aux
echo "my pid is: $$"
```

**为什么这么做**:

- `--pid`:创建一个新的 PID Namespace。
- `--fork`:必须加。因为 `unshare(CLONE_NEWPID)` 系统调用本身**不会**让调用者自己进入新 namespace 的第一个进程位置(PID 1 必须是"下一个被 fork 出来的进程"),所以 `unshare` 命令要先 fork 一个子进程,让这个子进程(也就是你马上要跑的 `bash`)去做 PID 1。
- `--mount-proc`:必须加,原因看 Step 3。

观察:`echo $$` 打印出 `1`。**在新 Namespace 里,这个 bash 就是 1 号进程。**

### Step 3:观察为什么必须加 `--mount-proc`

假设去掉 `--mount-proc`,只执行:

```bash
sudo unshare --pid --fork -- bash
ps aux
```

观察:你会惊讶地发现,`ps aux` **仍然显示了宿主机的一大堆进程**,而不是只有 1、2 两个。

**为什么会这样**:`ps` 并不会自己判断"我在哪个 PID Namespace",它就是简单粗暴地读 `/proc` 目录下的数字子目录来列出进程。而 `/proc` 是**挂载点**,不会因为你换了 PID Namespace 就自动跟着换内容——除非你重新挂载它。所以 `--mount-proc` 做的事情是:在新的 PID Namespace 里,重新 `mount -t proc proc /proc`,让 `/proc` 显示的内容与你**当前所在的 PID Namespace** 匹配,而不是继承宿主机那份旧的 `/proc`。

这是本课程第一次遇到"Namespace 之间会互相影响"的例子:PID Namespace 决定了"进程编号规则",但 `ps` 看到的结果还依赖 Mount Namespace 里 `/proc` 挂载的是谁的视图。这也是为什么 Docker/runc 每次启动容器都会重新挂载一个新的 `/proc`。

### Step 4:证明宿主机仍然能看到这个"1 号进程"

新开一个终端(不要 exit 掉 Step 2 的 shell),在宿主机执行:

```bash
ps -ef | grep bash
```

观察:你能找到刚才那个 bash,它在**宿主机**的 PID 是一个正常的三位数/四位数(比如 8341),而不是 1。

**为什么会这样**:同一个进程,在不同的 PID Namespace 里看到的编号不同——在子 Namespace 里它是 1,在宿主机(父)Namespace 里它是 8341。PID Namespace 是**分层的**:宿主机永远能看到所有子 Namespace 里的进程(用宿主机自己的编号),但子 Namespace 反过来看不到宿主机、也看不到兄弟 Namespace。

## 5. 实验结果分析

- 新 Namespace 里的进程"认为"自己是 PID 1,但这只是一个视图,底层内核仍然用宿主机的真实 PID 管理它的调度、信号等。
- `ps` 显示对不对,取决于 `/proc` 有没有被重新挂载,这是 PID Namespace 和 Mount Namespace 协作的第一个例子。
- PID Namespace 是有层级关系的树状结构,父 Namespace 全知全能,子 Namespace 只知道自己这一支。

## 6. Docker 对应关系

```
今天实验:

unshare --pid --fork --mount-proc bash

        |
        v

Docker:

docker run busybox sleep 3600

        |
        v

runc 内部调用:

clone(CLONE_NEWPID | ...)
        |
之后在子进程里执行:
mount(NULL, "/proc", "proc", ...)   <- 对应 --mount-proc
```

这也解释了一个常见现象:**为什么容器里 `ps` 只看到自己的进程,`top` 显示的 PID 1 通常是你的启动命令(比如 `nginx`、`sleep`)。**

## 7. 心智模型

```
PID Namespace
    =
换一张"进程编号表"给你看

不是:
真的把进程复制了一份、隔离了一份资源
```

## 8. 思考题

1. 为什么 PID Namespace 不需要"复制进程"就能实现隔离?复制进程和"换一张视图"在开销上有什么本质区别?这跟"容器启动几乎瞬间完成"有什么关系?
2. 容器里的 PID 1 如果不正确处理 `SIGTERM`,会发生什么?(提示:结合"PID 1 在 Linux 里有特殊职责——收养孤儿进程、不会被默认信号处理器杀死"来思考,这也是为什么很多镜像要用 `tini`/`dumb-init` 做 PID 1)
3. 如果宿主机重启了(或者 dockerd 挂了再重新连接),Docker 是怎么重新找回这些容器进程,并且知道它们对应哪个容器名字的?PID Namespace 本身能提供这个信息吗?

## 9. 延伸阅读

- `man 7 pid_namespaces`
- `man 1 unshare`
- Kernel 文档:`Documentation/admin-guide/namespaces.rst`

---

下一章:[第三章:UTS Namespace](03-uts-namespace.md) —— 一个更简单的 Namespace,用来热身理解"改变 View"这个套路能复用到任何资源上。
