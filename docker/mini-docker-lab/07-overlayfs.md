# 第七章:OverlayFS

## 1. 学习目标

亲手搭建一个 `lower + upper + work -> merged` 的 OverlayFS,观察 Copy-on-Write(写时复制)和 Whiteout(删除标记)的真实行为,理解 Docker 镜像"分层、共享、秒级启动"的文件系统基础。

## 2. 前置知识

- 知道"目录"和"文件系统挂载"的区别
- 第四章:Mount Namespace(这一章挂载的 OverlayFS,视图隔离原理与第四章一致)
- 不需要事先了解 Union FS 的历史(AUFS、UnionFS 等),知道 OverlayFS 是现在的主流实现即可

## 3. 原理简介

想象你要给 1000 个容器提供"一样的 Ubuntu 基础文件系统",但同时允许每个容器各自随意修改文件、互不影响。两个笨办法都不好:

- **每个容器复制一份完整文件系统**:1000 份 Ubuntu,浪费海量磁盘,而且复制大文件系统会很慢——这跟"容器秒级启动"的体验矛盾。
- **1000 个容器共享同一份文件系统,直接改**:一个容器的修改会污染所有其他容器和原始镜像,完全不可接受。

OverlayFS 用一个很朴素的思路解决这个矛盾:**只读的基础层大家共享,谁写谁自己单独记一份差异**。

```
lower  (只读,比如 Ubuntu 基础文件系统,可以被无数容器共享)
  +
upper  (可读写,这个容器自己的修改,一开始是空的)
  +
work   (OverlayFS 内部记账用的工作目录,用户不直接碰)
  =
merged (最终看到的、合并后的视图——这就是容器看到的 "/" )
```

规则很简单:

- 读一个文件:`upper` 里有就用 `upper` 的,没有就去 `lower` 里找。
- 第一次写一个只存在于 `lower` 的文件:先把它整个复制到 `upper`("copy-up"),然后在 `upper` 里的这份副本上修改。`lower` 里的原始文件**永远不会被改动**。
- 删除一个只存在于 `lower` 的文件:没法真的从 `lower` 删(它是只读的,而且可能被别的容器共享),所以在 `upper` 里创建一个特殊的"whiteout"标记,告诉 OverlayFS"合并视图里假装这个文件不存在"。

## 4. 实验步骤

### Step 1:搭建四个目录

执行:

```bash
mkdir -p ~/overlay-lab/{lower,upper,work,merged}
cd ~/overlay-lab
echo "I am from lower layer, unmodified" > lower/shared.txt
echo "I only exist in lower layer" > lower/only-in-lower.txt
```

**为什么这么做**:模拟一个"基础镜像层"(`lower`),里面有两个文件,分别用来观察"修改一个已有文件"和"删除一个已有文件"两种行为。`upper` 和 `work` 先留空,`merged` 是待会儿看到的最终视图。

### Step 2:挂载 OverlayFS

执行:

```bash
sudo mount -t overlay overlay \
  -o lowerdir=$(pwd)/lower,upperdir=$(pwd)/upper,workdir=$(pwd)/work \
  $(pwd)/merged

ls merged/
cat merged/shared.txt
```

观察:`merged` 目录下能看到 `lower` 里的两个文件,内容和 `lower` 里一模一样。

**为什么这么做**:这条 `mount` 命令告诉内核:"把这三个目录叠成一个视图,挂载到 `merged`"。此刻 `upper` 是空的,所以 `merged` 看到的完全就是 `lower` 的内容——这跟 Docker 容器刚启动、还没写过任何文件时的状态是一致的。

### Step 3:观察 Copy-on-Write(修改一个已有文件)

执行:

```bash
echo "modified through merged view" > merged/shared.txt
cat merged/shared.txt      # 通过合并视图看
cat lower/shared.txt       # 直接看 lower 原始内容
ls upper/                  # 看 upper 里发生了什么
cat upper/shared.txt
```

观察:

- `merged/shared.txt` 显示的是修改后的新内容。
- `lower/shared.txt` **完全没变**,还是原始内容。
- `upper/shared.txt` 出现了,内容是修改后的新内容——OverlayFS 把整个文件复制到了 `upper`,然后在这份副本上应用了你的写操作。

**为什么会这样**:这就是 Copy-on-Write。写操作永远发生在 `upper`,`lower` 保持只读、永不改变。这意味着:如果 100 个容器共享同一个 `lower`,某一个容器往里面写文件,完全不会影响其他 99 个容器和原始镜像——因为它们都是各自读自己的 `upper`(没有就回退去读共享的 `lower`)。

### Step 4:观察 Whiteout(删除一个只存在于 lower 的文件)

执行:

```bash
rm merged/only-in-lower.txt
ls merged/                 # 观察合并视图
ls lower/                  # 观察 lower 是否真的被删了
ls -la upper/               # 重点看这里
```

观察:

- `merged/` 里已经看不到 `only-in-lower.txt` 了。
- `lower/only-in-lower.txt` **依然存在**,文件本体丝毫未动。
- `upper/` 目录下出现了一个同名的 `only-in-lower.txt`,用 `ls -la` 看它的类型——是一个**字符设备文件**(权限位显示为 `c`,主次设备号通常是 `0, 0`)。

用下面命令再确认一次它的类型:

```bash
stat upper/only-in-lower.txt
```

**为什么会这样**:因为 `rm` 没法真的删掉 `lower` 里的文件(只读、还可能被共享),OverlayFS 于是在 `upper` 里放了一个特殊的"whiteout"文件(约定用主次设备号都是 0 的字符设备来表示),这是一个纯粹的**标记/墓碑**,告诉合并逻辑:"即使 `lower` 里有这个文件,合并视图里也要当它不存在"。

### Step 5:验证 lower 层本身完全干净、可以被复用

执行:

```bash
sudo umount merged
ls lower/
```

观察:卸载 OverlayFS 之后,直接看 `lower` 目录,两个文件都还在,内容也都是最原始的样子——就好像刚才所有的修改和删除都从未发生过。

**为什么会这样**:这一步是全章的核心结论——`lower` 全程没有被写入过一个字节。所有的"修改"和"删除",实际上都只是往 `upper` 里追加数据/标记。这正是为什么 Docker 镜像的每一层可以被无数容器共享:它们全部把这一层当作只读的 `lower`,各自的运行时改动全部隔离在自己独立的 `upper` 里。

## 5. 实验结果分析

- 修改和删除都不会碰 `lower`,`lower` 可以放心地被任意多个 `merged` 视图共享。
- "修改"的真实代价是"整份文件复制到 `upper`"(copy-up),这意味着修改一个巨大的只读文件、哪怕只改一个字节,开销也不小——这也是实践中"避免在容器里对大文件做小范围修改"的原因之一。
- "删除"不是真删除,是打了一个 whiteout 标记,这个机制让"多层叠加"和"共享底层"两个目标同时成立。

## 6. Docker 对应关系

```
今天实验:

mount -t overlay overlay \
  -o lowerdir=...,upperdir=...,workdir=... \
  merged

        |
        v

Docker Image:

一个 Image 由多个只读层组成(每条 Dockerfile 指令产生一层)
docker run 时:
  - 所有 Image 层依次作为 lowerdir(可以有很多层叠加)
  - Docker 为这个容器新建一个空的 upperdir(容器可写层)
  - 挂载出 merged,就是容器看到的根文件系统

docker commit / docker build 缓存复用得以成立,
本质就是因为 lower 层内容不可变、可以被内容寻址(按内容 hash 命名),
天然适合被多个镜像、多个容器共享。
```

`overlay2` 就是 Docker 官方推荐的 storage driver,内部实现和这一章完全一致,只是它自动管理多层 `lowerdir`(用冒号分隔多个只读层路径)。

## 7. 心智模型

```
OverlayFS
    =
多个目录"叠"成一个目录看
写操作永远落在最上面那一层

不是:
复制一份完整的目录
```

## 8. 思考题

1. 为什么 OverlayFS 能让"1000 个容器共享同一个 Ubuntu 基础镜像"这件事,既节省磁盘空间,又不需要额外加锁/加同步机制来防止容器之间互相干扰?
2. 如果一个容器在运行时修改了一个几百 MB 的大文件里的一个字节,基于这一章看到的 Copy-on-Write 机制,实际发生的磁盘 I/O 大概是什么量级?这对镜像设计(比如"大文件是否应该拆分成多个小文件")有什么启发?
3. `docker commit` 把一个运行中容器的修改保存成新镜像层,结合这一章的 `upper` 目录来看,`docker commit` 大概率是在读哪个目录的内容?

## 9. 延伸阅读

- `man 8 mount.overlay`(部分发行版是 `man 5 overlayfs`)
- Kernel 文档:`Documentation/filesystems/overlayfs.rst`
- Docker 官方文档:[overlayfs driver](https://docs.docker.com/engine/storage/drivers/overlayfs-driver/)

---

下一章:[第八章:chroot](08-chroot.md) —— 学会怎么把 `merged` 这样的目录,变成一个进程真正认为的"根目录"。
