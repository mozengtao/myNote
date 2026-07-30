# 第四章:Mount Namespace

## 1. 学习目标

理解 Mount Namespace 如何让一个进程拥有"自己的一套挂载点视图",并且明白为什么容器里挂载/卸载文件系统不会影响宿主机。这是第七、八、九章(OverlayFS、chroot、组合实验)能够成立的地基。

## 2. 前置知识

- 第二、三章的心智模型:Namespace = 改变 View
- 知道 `mount`/`umount` 的基本用法
- 知道"挂载点"是什么(把一个文件系统/目录挂到某个路径下,访问那个路径就相当于访问被挂载的内容)

## 3. 原理简介

正常情况下,一台 Linux 机器上所有进程共享**同一张挂载表**:

```
Host

挂载表
/            -> 根分区
/home        -> 数据盘
/mnt/usb     -> U 盘

任何进程执行 mount,新增的挂载点,其他所有进程立刻可见。
```

这在容器场景下是致命问题:如果容器 A 挂载了一个临时文件系统到 `/data`,这个挂载会立刻出现在宿主机和容器 B 的视图里——完全没有隔离可言。

Mount Namespace 解决的问题:**给一组进程一份"专属的挂载表副本"**,这个副本一开始是从父 Namespace 复制过来的,之后这组进程再做的 `mount`/`umount`,只影响自己这份副本,不会传播出去(除非显式配置了"共享传播")。

```
Host Mount Namespace

/            -> 根分区
/home        -> 数据盘

              |
              | unshare(CLONE_NEWNS)
              v

新的 Mount Namespace(继承了一份挂载表副本)

/            -> 根分区   (跟宿主一样,复制过来的)
/home        -> 数据盘   (跟宿主一样,复制过来的)
/data        -> tmpfs   (新增的,只在这个 Namespace 里可见)
```

这一章我们只验证"新增的挂载点不会泄漏出去",第七章会在这个基础上挂载 OverlayFS,第八、九章会用 `pivot_root` 彻底替换掉整个根目录。

## 4. 实验步骤

### Step 1:创建新的 Mount Namespace

执行:

```bash
sudo unshare --mount --fork -- bash
```

**为什么这么做**:`--mount` 对应 `CLONE_NEWNS`(历史原因,这个 flag 叫 `NEWNS` 而不是 `NEWMNT`,因为 Mount Namespace 是最早实现的 Namespace 类型,当时还没有"多种 Namespace"的概念)。这一步创建的新 Namespace 会先完整拷贝一份当前的挂载表。

### Step 2:在新 Namespace 里挂载一个 tmpfs

在新 shell 里执行:

```bash
mkdir -p /tmp/mini-ns-test
mount -t tmpfs tmpfs /tmp/mini-ns-test
echo "hello from new mount ns" > /tmp/mini-ns-test/hello.txt
cat /tmp/mini-ns-test/hello.txt
mount | grep mini-ns-test
```

**为什么这么做**:`tmpfs` 是一个纯内存文件系统,挂载后 `/tmp/mini-ns-test` 这个路径下的内容会被替换成一个全新的、空的文件系统,写文件不会碰到宿主机原来 `/tmp` 底下的任何东西。`mount | grep` 用来确认挂载点确实生效了。

### Step 3:去宿主机验证这个挂载点是否可见

不退出 Step 2 的 shell,新开一个终端(默认 Namespace)执行:

```bash
mount | grep mini-ns-test
cat /tmp/mini-ns-test/hello.txt
ls /tmp/mini-ns-test
```

观察:`mount | grep` 没有任何输出;`ls`/`cat` 要么显示目录是空的,要么提示文件不存在。

**为什么会这样**:宿主机的 Mount Namespace 里根本没有这条挂载记录。`/tmp/mini-ns-test` 这个目录本身(空目录)是两个 Namespace 共享看到的(因为它是在宿主机的根文件系统上创建的,创建目录不是"挂载"操作),但"在这个目录上挂了什么东西"是每个 Mount Namespace 各自独立记录的。这印证了第三章反复强调的结论:**Namespace 隔离的是"状态/视图",不是物理数据本身**——tmpfs 的数据只在内存里那一份,但"谁能看到它挂在哪儿"取决于你在哪个 Mount Namespace。

### Step 4:退出新 Namespace,验证清理

在 Step 2 的 shell 里执行:

```bash
exit
```

回到默认 shell,再次执行:

```bash
mount | grep mini-ns-test
```

观察:依然没有输出(其实从 Step 3 开始就没有过)。这一步是为了让你确认:那个 tmpfs 挂载点已经随着 Namespace 消亡而彻底消失,没有变成"孤儿挂载点"残留在系统里。

## 5. 实验结果分析

- 新 Mount Namespace 里做的挂载操作,默认不会传播到宿主机或其他 Namespace。
- 挂载表本身是"随 Namespace 走"的状态,而不是"跟着路径走"的属性——同一个路径,在不同 Namespace 里可能挂了完全不同的东西,也可能什么都没挂。
- 这正是容器"看到自己独立的文件系统"的地基:容器进程的 Mount Namespace 里,`/` 指向的其实是宿主机上某个镜像层目录,但容器完全不知道、也看不到宿主机真正的 `/`。

## 6. Docker 对应关系

```
今天实验:

unshare --mount --fork bash
mount -t tmpfs tmpfs /tmp/mini-ns-test

        |
        v

Docker:

docker run -v myvolume:/data nginx
或者
docker run --tmpfs /run nginx

        |
        v

runc 内部调用:

clone(CLONE_NEWNS | ...)
之后为容器执行一系列 mount() 调用:
  - mount 镜像的 OverlayFS 到新的根
  - mount volume/tmpfs 到容器内指定路径
  - mount /proc、/sys、/dev 等虚拟文件系统
```

容器"看起来"有一整套独立的文件系统,靠的就是:先有一个独立的 Mount Namespace(这一章),再在这个独立视图里把根目录换成镜像内容(第七、八、九章)。

## 7. 心智模型

```
Mount Namespace
    =
每个进程组拿到一份"自己专属的挂载表副本"
之后各自增删挂载点,互不干扰

不是:
复制了一份磁盘数据
```

## 8. 思考题

1. 如果 Mount Namespace 一开始是"完整拷贝"父 Namespace 的挂载表,那容器启动时看到的那么多挂载点(`/proc`、`/sys`、`/dev` 等)一开始也是从宿主机继承来的吗?容器运行时(runc)后续做了什么让它们变得"专属"?
2. Docker 的 `-v /host/path:/container/path` bind mount,和这一章的 tmpfs 挂载,在 Mount Namespace 隔离的意义上有什么共同点、又有什么本质区别?(提示:想想 bind mount 挂的是"已存在的宿主机路径",这意味着什么)
3. 如果不加 `unshare --mount`,直接在宿主机 Namespace 里执行 `mount -t tmpfs tmpfs /mnt/test`,再执行 `docker run` 启动一个容器,这个容器里能看到 `/mnt/test` 这个挂载点吗?为什么?

## 9. 延伸阅读

- `man 7 mount_namespaces`
- `man 2 mount`
- Kernel 文档:`Documentation/filesystems/sharedsubtree.rst`(进阶,讲挂载传播规则:private/shared/slave/unbindable)

---

下一章:[第五章:Network Namespace(隔离)](05-network-namespace-isolation.md) —— 把同样的套路用在网络设备上。
