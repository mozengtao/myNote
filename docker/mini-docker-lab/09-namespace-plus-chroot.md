# 第九章:组合 Namespace + chroot

## 1. 学习目标

把第二~五章(PID/UTS/Mount/Network Namespace)加上第八章(chroot)第一次组合在一起,得到一个"看起来、摸起来都像独立容器"的 `bash`——但明确知道它还完全没有资源限制(这是第十章要补的最后一块拼图)。

## 2. 前置知识

- 第二章:PID Namespace
- 第三章:UTS Namespace
- 第四章:Mount Namespace
- 第五章:Network Namespace(隔离)
- 第八章:chroot,以及"为什么单独 chroot 不够"

## 3. 原理简介

前面每一章都在单独验证"一种视图可以被隔离",这一章不引入任何新概念,只做一件事:**把这些独立的隔离维度叠加到同一个进程上**。

```
一个进程同时具备:

PID Namespace     -> 看不到宿主机的进程,自己是 PID 1
UTS Namespace     -> 有自己独立的 hostname
Mount Namespace    -> 有自己独立的挂载表
Network Namespace  -> 有自己独立(此刻是空的)网络栈
        +
chroot 到一个最小 rootfs -> 文件系统入口也被换掉

        =
一个"感觉自己活在一台独立机器上"的 bash/sh
```

这基本上就是 `docker run` 在执行用户命令之前,`runc` 完成的核心工作(还差 cgroups 资源限制,下一章补上)。

## 4. 实验步骤

### Step 1:准备好 rootfs(复用第八章的产物,补全几个命令)

```bash
mkdir -p ~/mini-ns-lab/rootfs/{bin,proc}
cp $(which busybox) ~/mini-ns-lab/rootfs/bin/busybox
cd ~/mini-ns-lab/rootfs/bin
for cmd in sh ls ps hostname mount echo cat mkdir sleep; do
  ln -sf busybox $cmd
done
```

**为什么这么做**:这一章要在 chroot 环境内部执行 `hostname`(验证 UTS 隔离)、`ps`(验证 PID 隔离)、`mount`(在新 Mount Namespace 里重新挂载 `/proc`),所以要把这几个命令对应的软链接都建好。

### Step 2:一条命令同时创建四种 Namespace 并 chroot 进去

执行:

```bash
cd ~/mini-ns-lab
sudo unshare --pid --uts --mount --net --fork \
  --mount-proc="$(pwd)/rootfs/proc" \
  chroot "$(pwd)/rootfs" /bin/sh -c '/bin/hostname mini-box && exec /bin/sh'
```

**为什么这么做**(逐个 flag 解释):

- `--pid --uts --mount --net`:一次性创建四种独立的 Namespace,对应第二、三、四、五章分别学过的内容。
- `--fork`:和第二章的原因一样,PID Namespace 要求"下一个 fork 出来的进程"才能成为新 Namespace 里的 PID 1。
- `--mount-proc="$(pwd)/rootfs/proc"`:这是本章唯一的新技巧。因为 `chroot` 是在 `unshare` fork 出的新进程里、且是在**换根之前**发生的,`--mount-proc` 指定的路径必须是"换根前、宿主机视角下能看到的路径"——也就是 `rootfs/proc` 这个宿主机路径,而不是换根后的 `/proc`。`unshare` 会在新的 Mount Namespace 里把 `/proc` 挂载到这个路径上;等一会儿 `chroot` 把根切换到 `rootfs` 之后,这个挂载点从新进程的视角看正好就是 `/proc`。
- `chroot "$(pwd)/rootfs" /bin/sh -c '...'`:换根,并在新根里执行一个 shell 脚本:先 `hostname mini-box` 修改主机名(验证 UTS 隔离是独立生效的),再用 `exec /bin/sh` 替换成一个交互式 shell(留在这个"容器"里让你继续观察)。

### Step 3:在这个"类容器" shell 里逐项验证隔离效果

进入之后依次执行:

```
/bin/ps aux
```

**观察**:只能看到 1、2 两个进程(`sh` 本身和 `ps`),对应第二章的 PID Namespace 效果。

```
/bin/hostname
```

**观察**:显示 `mini-box`,不是宿主机真实的 hostname,对应第三章的 UTS Namespace 效果。

```
/bin/ls /
```

**观察**:只能看到 `bin`、`proc` 两个目录,看不到宿主机真正的 `/etc`、`/home` 等任何内容,对应第八章的 chroot 效果。

```
/bin/mount | grep proc
```

**观察**:能看到 `/proc` 被挂载为 `proc` 类型,这正是 Step 2 里 `--mount-proc` 帮你自动做好的挂载(对应第四章 Mount Namespace + 第二章"必须重新挂载 /proc"的知识点)。

### Step 4:再开一个宿主机终端,交叉验证隔离是双向不可见的

不退出 Step 3 的 shell,新开一个宿主机终端执行:

```bash
hostname                 # 应该还是宿主机原始的名字,不是 mini-box
ps -ef | grep busybox    # 能找到那个 sh 进程,但 PID 是宿主机的真实编号(不是 1)
```

**为什么会这样**:和前面每一章的结论一致——隔离永远是"子 Namespace 看不到父/宿主机",但"父/宿主机始终能看到子 Namespace 里的一切"(用它自己的编号系统)。

## 5. 实验结果分析

- 单独看,PID/UTS/Mount/Network Namespace 加 chroot,每一个都只解决一个很窄的问题;叠加在一起之后,效果发生了质变——从"某个视图被改变的普通进程",变成了一个从里面看几乎无法分辨是不是运行在一台独立机器上的环境。
- 这一步依然缺资源限制:在这个"容器"里,你可以随意占满 CPU、吃光内存、疯狂 fork 进程,完全不受约束,会直接影响宿主机上的其他工作负载——这正是下一章 cgroups 要解决的问题。
- 这一章也没有加 User Namespace(UID/GID 隔离),所以这个"容器"里的 root,其实就是宿主机的 root(如果不加额外的能力限制,存在安全风险)。这一点课程不展开(属于要深入内核权限模型的部分),但值得记在心里:生产级容器运行时通常会结合 User Namespace、Capabilities、Seccomp 等做更细粒度的权限收敛。

## 6. Docker 对应关系

```
今天实验:

unshare --pid --uts --mount --net --fork \
  --mount-proc=rootfs/proc \
  chroot rootfs /bin/sh

        |
        v

Docker:

docker run -it ubuntu bash

        |
        v

runc 按 OCI Runtime Spec(config.json)描述的 namespaces 列表,
依次调用 clone()/unshare() 创建对应的 Namespace,
再对 OverlayFS 合并出的目录执行 pivot_root(比 chroot 更彻底),
最后 execve() 用户指定的命令(这里是 bash)
```

到这一章为止,我们已经用命令行工具手工重现了 `runc` 在 `create`/`start` 阶段做的绝大部分事情——只差 cgroups(下一章)和把这些步骤自动化成一个脚本(第十一章)。

## 7. 心智模型

```
Namespace(多个) + chroot
    =
把"改变一种视图"的多个独立能力叠加到同一个进程上
叠加得越多,这个进程就越"像"一台独立机器

不是:
存在某种叫"容器"的、内核原生支持的单一开关
```

## 8. 思考题

1. 这一章的组合里,如果去掉 `--net`,这个"容器" shell 会不会立刻具备访问外网的能力?为什么(结合第五、六章的结论)?
2. 为什么说"这个环境里的 root 就是宿主机的 root"是一个安全隐患?你能想到哪些"看起来在容器里,实际上能影响宿主机"的操作?(不需要写出具体攻击手法,只需要说明风险的来源)
3. 如果要在这一章的基础上,让这个"容器"重新具备第六章那样的外网访问能力,你需要在 Step 2 执行前后,额外做哪几件事?（提示:回顾第六章的 veth + bridge 步骤，想想它们能不能直接套用在这里新建的 Network Namespace 上）

## 9. 延伸阅读

- `man 1 unshare`(重点看 `--mount-proc` 参数说明)
- `man 7 user_namespaces`(选读,了解 User Namespace 大致解决什么问题)
- OCI Runtime Specification:[config.json 的 `namespaces` 字段](https://github.com/opencontainers/runtime-spec/blob/main/config-linux.md#namespaces)

---

下一章:[第十章:cgroups](10-cgroups.md) —— 补上最后一块拼图:限制这个"容器"能用多少 CPU、内存、进程数。
