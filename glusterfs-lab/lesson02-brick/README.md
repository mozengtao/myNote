# Lesson 2 - Brick

## 一、本实验目的

证明：**Brick 本质就是一个普通目录，位于一个普通文件系统（ext4/xfs/overlay 都行）之上。GlusterFS 没有发明任何新的磁盘格式，也不要求专门格式化。**

## 二、背景知识

只讲这一课需要的最小知识：

- Brick（砖）是 GlusterFS 里"一份存储空间"的最小单位。官方文档说"a brick is the combination of a node and an export directory"——直白点说：`某台机器:某个目录`。
- 本课只谈"Brick 是什么"，还不谈"Brick 怎么被组织成 Volume"（那是 Lesson 3），所以这里创建的目录暂时不会被 `gluster volume create` 使用。

## 三、提出问题

1. 为什么 Brick 不用一种特殊的磁盘格式（类似 ZFS 的 zpool、Ceph 的 OSD 需要专门 prepare 磁盘）？
2. 为什么不用数据库（比如把文件内容存进 SQLite/RocksDB）来实现存储？
3. 如果 Brick 只是目录，那 GlusterFS 怎么在这个目录里存"这个文件属于哪个 Volume、副本关系是什么"这类元数据？会不会和目录里本来的用户文件混在一起、冲突？
4. 如果我直接绕开 Gluster，用 `cp`/`rm` 直接操作 Brick 目录里的文件，会发生什么？

先猜一猜，再往下看。

## 四、实验拓扑

```
        node1                  node2                  node3
     172.28.0.11            172.28.0.12            172.28.0.13
   /data/brick1           /data/brick1           /data/brick1
   (刚 mkdir 出来的        (刚 mkdir 出来的        (刚 mkdir 出来的
    空目录)                 空目录)                 空目录)
```

三个节点上分别创建同名目录 `/data/brick1`，此时它们之间 **完全没有任何关联**——目前只是三台机器上各自的一个普通空目录。

## 五、实验步骤

### Step 1：创建目录

为什么做：验证"创建 Brick"这个动作，本质上就是 `mkdir`，没有任何专门命令。

```bash
docker exec node1 mkdir -p /data/brick1
docker exec node2 mkdir -p /data/brick1
docker exec node3 mkdir -p /data/brick1
```

观察：`ls -la /data/`，应该看到一个空目录，权限和普通目录没有区别。

### Step 2：看这个目录到底"坐落"在什么文件系统上

为什么做：证明不需要专门的磁盘格式——这里甚至是跑在容器的 overlayfs 之上（生产环境一般会是 xfs/ext4，但道理一样：Gluster 不关心底层是什么文件系统，只要支持扩展属性 xattr 即可，这是唯一的硬性要求）。

```bash
docker exec node1 stat -f /data/brick1
docker exec node1 df -T /data/brick1
```

### Step 3：像操作普通目录一样操作它

为什么做：直接验证"读写就是普通文件系统调用"，没有任何 Gluster 特有的步骤介入。

```bash
docker exec node1 bash -c "echo 'hello from node1' > /data/brick1/hello.txt"
docker exec node1 find /data/brick1
docker exec node1 cat /data/brick1/hello.txt
```

### Step 4：检查扩展属性（xattr）——这是本课最关键的一步

为什么做：很多人以为"Gluster 目录"会有点特殊标记。在加入 Volume 之前，用 `getfattr` 验证：一个"裸的"目录上，没有任何 Gluster 相关的扩展属性。

```bash
docker exec node1 getfattr -d -m . -e hex /data/brick1
docker exec node1 getfattr -d -m . -e hex /data/brick1/hello.txt
```

### Step 5：brick 可以是文件系统里任意路径

为什么做：证明 Brick 和"位置"没有任何强制约束——它不需要是根目录下的一级目录，不需要挂载点，随便一个深层嵌套目录都能当 Brick。

```bash
docker exec node1 mkdir -p /srv/whatever/deep/nested/mybrick
docker exec node1 bash -c "echo works > /srv/whatever/deep/nested/mybrick/probe.txt"
docker exec node1 find /srv/whatever
```

## 六、实验现象

**现象 1**：`mkdir -p /data/brick1` 就是普通的 `mkdir`，没有任何输出、没有任何"格式化"过程。

**现象 2**：`stat -f` 显示这个目录所在的文件系统类型是 `overlayfs`（本实验环境），Blocks/Inodes 统计和整个容器根文件系统完全一致——说明 Gluster 根本没有单独划出一块特殊存储区域，Brick 就是普通文件系统上的普通目录：

```
File: "/data/brick1"
    ID: afbc62996122e7cc Namelen: 255     Type: overlayfs
Filesystem     Type     1K-blocks     Used Available Use% Mounted on
overlay        overlay 1055762868 58607020 943452376   6% /
```

**现象 3**：`echo` / `cat` / `find` 都工作得和操作任何普通目录一样：

```
$ docker exec node1 find /data/brick1
/data/brick1
/data/brick1/hello.txt
$ docker exec node1 cat /data/brick1/hello.txt
hello from node1
```

**现象 4（最重要，也最容易被忽略）**：`getfattr` 在加入 Volume 之前，**完全没有任何输出**——没有一个字节的 Gluster 元数据：

```
$ docker exec node1 getfattr -d -m . -e hex /data/brick1
(空)
$ docker exec node1 getfattr -d -m . -e hex /data/brick1/hello.txt
(空)
```

请记住这个"之前"的状态——**Lesson 3 创建 Volume 之后，我们会回来对比同一个目录上多出来的扩展属性，那才是 Gluster 真正留下的痕迹**。

**现象 5**：任意深度嵌套的目录都能被当作 Brick 使用，没有位置限制。

完整原始输出见 [evidence/](evidence/) 目录：
`01-mkdir-brick.txt`、`02-filesystem-type.txt`、`03-touch-find.txt`、`04-getfattr-before-volume.txt`、`05-arbitrary-path.txt`。

## 七、原理分析

```
应用程序 open()/write()/read()
        |
        v
   本地文件系统 (ext4 / xfs / overlay / 任何 POSIX 文件系统)
        |
        v
   /data/brick1/xxx   <-- 这就是 Brick，普通 inode + dentry
```

Brick 层完全没有引入新的 I/O 路径——数据最终还是走 Linux 标准的 VFS -> 具体文件系统驱动 -> 块设备。GlusterFS 的所有"分布式能力"（分片、副本、自愈）都不是在这一层实现的，而是在 **更上层的 Translator**（下一课会讲的概念）里实现的。Brick 层唯一被 Gluster 用到的"特权"是 **扩展属性（xattr）**——Linux 文件系统原生支持在每个文件/目录上挂载任意的 key-value 元数据，Gluster 用这个能力存放它自己的簿记信息（GFID、副本状态、DHT layout 等），而不需要另开一个数据库或索引文件。这就是为什么"支持 xattr"是 Gluster 对底层文件系统唯一的硬性要求，而"是哪种文件系统"完全不重要。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：自己发明一种磁盘格式（像 Ceph 的 BlueStore 那样直接管理裸块设备）。**

- 好处：可以针对分布式场景做深度优化（比如更好的元数据布局、更快的元数据查找）。
- 代价：工程量巨大——要写自己的分配器、日志、崩溃恢复逻辑，相当于重新发明一个文件系统。而且丧失了"直接用 `ls`/`find`/`rsync`/`fsck` 等现成 Linux 工具"这个巨大的运维便利性。

**备选方案 B：用数据库存文件内容或索引（类似把文件切块存进 KV store）。**

- 好处：可能有更灵活的元数据查询能力。
- 代价：大文件顺序读写的性能会打折——数据库通常不是为"流式读写大文件"设计的；而且一旦数据库损坏，恢复成本远高于一个普通文件系统（后者有成熟的 `fsck`、有内核多年打磨的稳定性）。

**GlusterFS 选择的方案：直接把本地文件系统当 Brick，用 xattr 存簿记信息。**

- 好处 1：**运维复用**——Brick 坏了，直接用文件系统层面的工具（`fsck`、`rsync`、`tar`）就能诊断和恢复，不需要学习一套全新工具链。
- 好处 2：**故障隔离**——一个 Brick 出问题，影响范围就是"这个目录"，排查思路和排查普通磁盘故障没有区别。
- 好处 3：**渐进式采用**——理论上你甚至可以把一个已经有数据的目录直接拿来做实验（虽然生产环境不建议），这大大降低了"迁移到 Gluster"的心理门槛。
- 代价：性能上限受限于底层文件系统本身（比如 ext4 目录项数量过多时的查找性能），Gluster 后期版本也确实为此推荐用 XFS 并做了一些专门优化，但这是"在现有地基上做优化"，而不是"造一个新地基"。

一句话总结设计哲学：**能复用 Linux 内核几十年打磨出来的稳定组件（文件系统），就不要自己重新发明。分布式能力应该在存储之上的一层去解决，而不是侵入存储本身。**

## 九、源码位置（仅指路）

- `xlators/storage/posix/` —— 这是 Gluster 里"和本地文件系统直接打交道"的 Translator（Posix Translator），Brick 的读写最终都会落到这里的系统调用。
- `libglusterfs/src/` —— GFID、xattr 相关的通用工具函数。

## 十、本实验总结

1. Brick = 某台节点上的一个普通目录，创建方式就是 `mkdir`。
2. Brick 对底层文件系统没有特殊格式要求，只要求支持扩展属性（xattr）。
3. 在没有被 Volume 使用之前，Brick 目录上没有任何 Gluster 相关的元数据痕迹。
4. Gluster 的簿记信息（GFID、副本状态等）未来会以 xattr 形式挂在文件/目录上，而不是另建数据库。
5. 分布式能力不在 Brick 这一层实现，Brick 只负责"老老实实存字节"。

## 十一、思考题

1. 如果 Brick 改成一个块设备（Block Device），而不是文件系统里的目录，会怎样？Gluster 还能用 xattr 存元数据吗？
2. 如果我直接在 Brick 目录里用 `rm` 删掉一个文件，绕过 Gluster 的 Volume 挂载点，会发生什么？（提示：想想 Lesson 1 学到的"没有中心服务器会拦截你"这件事，在 Brick 层面是不是同样成立？）
3. 现在往 `/data/brick1` 里写的 `hello.txt`，在 Lesson 3 把这个目录做成 Volume 之后，这个文件还会在吗？Gluster 会不会因为目录"不干净"而拒绝？
4. 为什么 Gluster 选择 xattr 而不是在 Brick 目录下放一个隐藏的 `.gluster_meta` 文件来存元数据？两者有什么本质区别（提示：想想原子性、想想文件被整体复制/移动时元数据会不会跟丢）？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 创建 Brick 就是普通 `mkdir`，三节点结构一致 | [evidence/01-mkdir-brick.txt](evidence/01-mkdir-brick.txt) |
| Brick 所在文件系统是普通文件系统（这里是 overlayfs），没有专门格式化 | [evidence/02-filesystem-type.txt](evidence/02-filesystem-type.txt) |
| 读写 Brick 目录就是标准 POSIX 文件操作 | [evidence/03-touch-find.txt](evidence/03-touch-find.txt) |
| 加入 Volume 之前，目录上没有任何 Gluster xattr（关键对照组，Lesson 3 会回来对比） | [evidence/04-getfattr-before-volume.txt](evidence/04-getfattr-before-volume.txt) |
| Brick 可以是任意深度的任意路径，没有位置限制 | [evidence/05-arbitrary-path.txt](evidence/05-arbitrary-path.txt) |

---

上一课：[Lesson 1 - 认识 Peer](../lesson01-peer/README.md)
下一课：[Lesson 3 - Volume](../lesson03-volume/README.md)
