# 第八章:chroot

## 1. 学习目标

亲手用 `chroot` 把一个普通目录变成某个进程眼里的"根目录 `/`",并且亲眼验证:**光有 chroot,进程依然能看到宿主机的进程列表、网络、hostname——chroot 只解决了"文件系统入口"这一件事,不是完整的隔离方案**。这个"不完整"正是下一章要解决的问题。

## 2. 前置知识

- 第七章:OverlayFS 搭出来的 `merged` 目录,可以理解为"一份完整的最小文件系统"
- 知道 Linux 下二进制程序依赖动态链接库(`ldd` 命令能看依赖)
- 知道什么是"根目录 `/`"

## 3. 原理简介

任何 Linux 进程访问文件,最终都是从某个"根目录"开始一层层解析路径的。正常情况下,所有进程的根目录都是同一个物理磁盘的根分区:

```
Host

/  (真正的磁盘根目录)
 |-- etc
 |-- usr
 |-- bin
 |-- home
```

`chroot(path)` 这个系统调用做的事情极其朴素:**把调用进程(及其之后的子进程)对 `/` 的解析起点,换成 `path` 这个目录**。从这一刻起,这个进程不管怎么 `cd ..`,都跳不出这个新的根——因为对它来说,这个目录本身就是"宇宙的根"。

```
chroot("/home/user/my-rootfs")

之后这个进程看到的:

/                          <- 其实是宿主机的 /home/user/my-rootfs
 |-- bin
      -- busybox
```

注意:chroot 只换了"文件路径解析的起点",它**不会**创建新的 PID Namespace、Network Namespace、UTS Namespace——这个进程的进程列表视图、网络、hostname 跟宿主机完全一样,没有任何变化。这一点本章会用实验直接证明。

## 4. 实验步骤

### Step 1:准备一个最小 rootfs

用静态链接的 `busybox` 是最省事的做法,它一个二进制文件自带了几十个常用命令(`ls`、`cat`、`sh`、`ps` 等),不依赖任何外部动态库。

执行:

```bash
mkdir -p ~/chroot-lab/rootfs/bin
which busybox
ldd $(which busybox)
```

**为什么这么做**:先确认你机器上的 `busybox` 是不是静态链接的——如果 `ldd` 输出类似 `not a dynamic executable`,说明是静态的,可以直接复制这一个文件就够用;如果它依赖了一堆 `.so`,后面还需要把这些依赖库连同 `busybox` 一起复制进 rootfs,否则程序会因为找不到动态库而无法运行(这也是 chroot 环境里最常见的报错"No such file or directory",其实是找不到依赖库,不是找不到程序本身)。

大多数发行版的 `busybox` 是静态的,以下步骤按静态的情况写:

```bash
cp $(which busybox) ~/chroot-lab/rootfs/bin/busybox
cd ~/chroot-lab/rootfs/bin
for cmd in sh ls cat ps echo mkdir; do
  ln -s busybox $cmd
done
```

**为什么这么做**:`busybox` 会根据"自己是通过哪个文件名被调用的"来决定表现成哪个命令(这叫 multi-call binary)。建几个软链接,就相当于在这个最小系统里"安装"了 `sh`、`ls`、`cat` 等命令,而实际上磁盘上只有一份 `busybox` 二进制。

### Step 2:执行 chroot

执行:

```bash
sudo chroot ~/chroot-lab/rootfs /bin/sh
```

进入之后执行:

```
/bin/ls /
```

观察:`/` 下面只能看到 `bin` 这一个目录,里面是刚才建的那些软链接——**这就是这个 shell 眼里完整的整个文件系统**,它访问不到、也看不到宿主机真正的 `/etc`、`/home` 等任何内容。

**为什么这么做**:验证 chroot 最基本的效果——文件系统视图被换成了一个封闭的子集。

### Step 3:证明它依然能看到宿主机的进程

在 chroot 环境里执行(用刚建的软链接):

```
/bin/ps
```

大概率会报错(因为没有 `/proc`,`ps` 读不到数据)或者只显示极少信息。这时候退出 chroot,回到宿主机,重新观察一下:

```bash
exit
mount --bind /proc ~/chroot-lab/rootfs/proc 2>/dev/null || sudo mkdir -p ~/chroot-lab/rootfs/proc
sudo mount -t proc proc ~/chroot-lab/rootfs/proc
sudo chroot ~/chroot-lab/rootfs /bin/sh
/bin/ps aux
```

观察:一旦给 chroot 环境挂上了 `/proc`,`ps` 立刻能看到**宿主机上完整的进程列表**,不只是这个 chroot 环境里的进程。

**为什么会这样**:`chroot` 没有创建新的 PID Namespace。`/proc` 挂载进来之后,内核提供的仍然是宿主机那一份真实的进程信息——这个 chroot 出来的 shell,和宿主机上的任何一个普通 shell,在"能看到哪些进程"这件事上没有任何区别。

### Step 4:证明它依然共享网络和 hostname

在 chroot 的 `sh` 里(如果你复制了对应的 busybox 子命令,或者直接看 `/proc`)执行:

```
cat /proc/sys/kernel/hostname
```

观察:显示的是宿主机真实的 hostname,和第三章"新建 UTS Namespace 之后才能独立修改 hostname"形成鲜明对比。

**为什么会这样**:同样的道理,`chroot` 根本没有涉及 UTS Namespace、Network Namespace。这个进程和宿主机上的其他进程共享网络栈、共享 hostname、共享 PID 表——它唯一被改变的,只有"文件路径解析的起点"。

## 5. 实验结果分析

- `chroot` 是一个非常"轻"的操作,只改变文件系统访问的根,不涉及进程、网络、主机名的任何隔离。
- 光靠 `chroot` 搭不出一个"看起来像独立机器"的环境——它能看到宿主机全部进程、共享网络、共享 hostname,这在安全和多租户场景下远远不够(历史上也确实存在多种"chroot 逃逸"手法,这也是为什么现代容器从不单独依赖 chroot)。
- 但 `chroot`(或它的现代替代品 `pivot_root`)仍然是"给进程换根文件系统"这件事的核心机制——第七章的 OverlayFS `merged` 目录,加上这一章的 chroot,已经具备了"独立文件系统视图"的全部拼图,只是还缺"进程/网络/主机名隔离"这一半。

## 6. Docker 对应关系

```
今天实验:

chroot rootfs /bin/sh

        |
        v

Docker(概念上):

runc 在启动容器进程前,会对 OverlayFS 合并出的目录
执行 pivot_root(比 chroot 更彻底、更安全的"换根"系统调用)

pivot_root 和 chroot 的目标是一样的(换根),
但 pivot_root 会把旧的根挂载点挪到一个临时目录再卸载掉,
彻底切断新进程访问原根文件系统的路径,
不像 chroot 那样理论上还可能被特殊手法"逃逸"回旧根。
```

## 7. 心智模型

```
chroot
    =
只换"文件路径解析的起点"

不是:
换了一整台机器的运行环境
```

## 8. 思考题

1. 为什么只做 `chroot`,而不配合 PID/Network/UTS Namespace,不能算是真正意义上的"容器隔离"?结合这一章 Step 3、Step 4 的观察举例说明。
2. `chroot` 环境里,如果这个进程本身是 root 用户,历史上有哪些已知的"跳出 chroot"手法?(不需要深入研究细节,只需要知道"chroot 不是安全边界"这个结论,以及为什么 Docker 从不单独依赖它)
3. `pivot_root` 相比 `chroot` 多做的"把旧根挪走再卸载"这一步,如果不做会有什么风险?(提示:结合第四章 Mount Namespace 的挂载表概念思考——旧的挂载点如果还在,是不是意味着还有路径能访问回去?)

## 9. 延伸阅读

- `man 2 chroot`
- `man 2 pivot_root`
- `man 1 busybox`

---

下一章:[第九章:组合 Namespace + chroot](09-namespace-plus-chroot.md) —— 把前面八章的所有拼图第一次真正拼在一起。
