# Lesson 3 - Volume

## 一、本实验目的

证明：**Volume 不是一种"实体存储"，而是一份配置——它描述了"用哪些 Brick"、"怎么把它们组合起来"，最终被 `glusterd` 编译成具体的配置文件（volfile），分发给集群里的每一个节点。**

## 二、背景知识

只讲这一课需要的最小知识：

- Volume 是用户真正挂载和使用的对象（"我要挂载一个 Gluster Volume"，而不是"我要挂载一个 Brick"）。
- 一个 Volume 由一个或多个 Brick 组成，`gluster volume create <name> <brick1> <brick2> ...` 就是把 Lesson 2 里那些孤立的目录，正式组织成一个整体。
- 本课不解释 Volume 内部具体每一层 Translator 是干什么的（那是 Lesson 6 的内容），这里只关心：Volume 创建之后，磁盘上多了什么、这些"多出来的东西"长什么样。

## 三、提出问题

1. `gluster volume create` 执行之后，数据实际上被搬动了吗？还是只是"贴了个标签"？
2. Volume 的配置信息，存在哪一个节点上？是不是只有执行 `create` 命令的那个节点知道？
3. Lesson 2 里我们看到 Brick 目录上没有任何 Gluster 相关的扩展属性——创建 Volume 之后，会不会多出来？多出来的是什么？
4. 如果客户端要挂载这个 Volume，它需要知道"哪些节点、哪些目录"这些信息——这些信息从哪来？（先记住这个问题，Lesson 4 会正式回答，这里只需要猜一猜"信息会存在哪"）

## 四、实验拓扑

```
        node1                  node2                  node3
     172.28.0.11            172.28.0.12            172.28.0.13
   /data/brick1           /data/brick1           /data/brick1
   (Lesson2 留下的         (Lesson2 留下的         (Lesson2 留下的
    普通目录，里面           普通目录，里面           普通目录，里面
    有 hello.txt)           有 hello.txt)           有 hello.txt)

                  只在 node1 上执行 volume create/start
```

## 五、实验步骤

### Step 1：先尝试直接创建（预期会遇到一个"保护性拒绝"）

为什么做：不是所有目录都能直接拿来当 Brick——Gluster 有一些安全检查，故意先撞一下这个检查，比直接绕过更有教学意义。

```bash
docker exec node1 gluster volume create gv-dist \
  node1:/data/brick1 node2:/data/brick1 node3:/data/brick1
```

### Step 2：用 `force` 越过检查，正式创建

为什么做：本实验环境里 Brick 目录本来就在容器的根分区上（没有独立挂载点），触发了 Gluster"不建议用根分区做存储后端"的保护性检查。生产环境应该用独立挂载的磁盘/分区来避免这个问题；实验环境里用 `force` 明确表示"我知道风险，就是要这么做"。

```bash
docker exec node1 gluster volume create gv-dist \
  node1:/data/brick1 node2:/data/brick1 node3:/data/brick1 force
docker exec node1 gluster volume start gv-dist
```

### Step 3：查看 Volume 信息

```bash
docker exec node1 gluster volume info gv-dist
docker exec node1 gluster volume status gv-dist
```

### Step 4：回头看 Brick 目录——现在有 xattr 了吗？

为什么做：直接对照 Lesson 2 结尾时"空白"的 `getfattr` 输出，验证 Volume 创建到底往 Brick 目录里写了什么。

```bash
docker exec node1 getfattr -d -m . -e hex /data/brick1
docker exec node2 getfattr -d -m . -e hex /data/brick1
docker exec node3 getfattr -d -m . -e hex /data/brick1
```

### Step 5：只在 node1 上执行了 create/start，去 node2、node3 看看它们是否也"知道"这个 Volume

为什么做：呼应 Lesson 1 的发现——验证 Volume 配置是不是也会像 Peer 信息一样被复制到所有节点。

```bash
docker exec node1 ls -la /var/lib/glusterd/vols/gv-dist/
docker exec node2 ls -la /var/lib/glusterd/vols/gv-dist/
docker exec node3 ls -la /var/lib/glusterd/vols/gv-dist/
docker exec node1 cat /var/lib/glusterd/vols/gv-dist/cksum
docker exec node2 cat /var/lib/glusterd/vols/gv-dist/cksum
docker exec node3 cat /var/lib/glusterd/vols/gv-dist/cksum
```

### Step 6：看真正的配置文件长什么样

为什么做：这是本课的核心——Volume 最终被"编译"成了什么。

```bash
docker exec node1 cat /var/lib/glusterd/vols/gv-dist/info
docker exec node1 cat /var/lib/glusterd/vols/gv-dist/trusted-gv-dist.tcp-fuse.vol
docker exec node1 cat /var/lib/glusterd/vols/gv-dist/gv-dist.node1.data-brick1.vol
```

## 六、实验现象

### 现象 1：第一次创建被拒绝——而且理由跟"目录不干净"无关

```
volume create: gv-dist: failed: The brick node1:/data/brick1 is being
created in the root partition. It is recommended that you don't use
the system's root partition for storage backend. Or use 'force' at
the end of the command if you want to override this behavior.
```

这提示我们一个容易被忽略的细节：Gluster 的这层保护检查的是"是不是独立挂载点"，**不是**检查目录是否为空（我们直接用带 `hello.txt` 的目录也创建成功了）。（见 [evidence/01-create-attempt-dirty.txt](evidence/01-create-attempt-dirty.txt)）

### 现象 2：加 `force` 之后创建成功，`Type: Distribute`

```
Volume Name: gv-dist
Type: Distribute
Volume ID: b8c09893-49bc-42cf-b8f0-4882904c06e0
Status: Started
Number of Bricks: 3
Bricks:
Brick1: node1:/data/brick1
Brick2: node2:/data/brick1
Brick3: node3:/data/brick1
```

因为创建时没有加 `replica` 关键字，Gluster 默认把这 3 个 Brick 组织成 **Distribute**（分布）类型——这个类型的含义会在 Lesson 5 具体展开。（见 [evidence/02-create-with-force.txt](evidence/02-create-with-force.txt)、[evidence/04-volume-info-status.txt](evidence/04-volume-info-status.txt)）

`volume status` 还显示了每个 Brick 对应一个独立的 TCP 端口（`53176`、`60832`、`60791`）和独立的进程 PID——**每个 Brick 背后是一个独立的进程**，这个发现会在 Lesson 9 讲 RPC 时用到，先记住。

### 现象 3：Brick 目录上多出来两个 xattr

对照 Lesson 2 结尾时的"空白"：

```
# file: data/brick1
trusted.gfid=0x00000000000000000000000000000001
trusted.glusterfs.volume-id=0xb8c0989349bc42cfb8f04882904c06e0
```

三个节点上的 `trusted.glusterfs.volume-id` **完全一致**，并且正好等于 `gluster volume info` 里的 `Volume ID: b8c09893-49bc-42cf-b8f0-4882904c06e0`。这就是"Volume 创建"这个动作在 Brick 上留下的**全部**痕迹——没有搬动任何数据，只是在目录上打了两个标记：`trusted.gfid`（这个目录作为 Gluster 命名空间里的"根 GFID"）和 `trusted.glusterfs.volume-id`（标记"我属于哪个 Volume"）。（见 [evidence/05-getfattr-after-volume.txt](evidence/05-getfattr-after-volume.txt)）

### 现象 4：只在 node1 上操作，但三个节点的配置目录逐字节相同

```
--- node1 --- info=3019646081
--- node2 --- info=3019646081
--- node3 --- info=3019646081
```

`/var/lib/glusterd/vols/gv-dist/` 目录在三个节点上文件列表、文件大小完全一致，`cksum` 文件里的校验和也完全一致。这再次印证 Lesson 1 的结论：**管理面配置（不管是 Peer 信息还是 Volume 配置）都是被复制到每个节点本地磁盘的，没有一个"中心配置服务器"**。（见 [evidence/06-vols-dir-all-nodes.txt](evidence/06-vols-dir-all-nodes.txt)、[evidence/08-cksum-all-nodes.txt](evidence/08-cksum-all-nodes.txt)）

### 现象 5：`info` 文件——Volume 的"源配置"

```
type=0
count=3
replica_count=1
volume-id=b8c09893-49bc-42cf-b8f0-4882904c06e0
username=8ecb76ec-8304-46fe-b024-351587f34fa2
password=77f0f621-b9cd-48cd-85f8-434fa278314d
brick-0=node1:-data-brick1
brick-1=node2:-data-brick1
brick-2=node3:-data-brick1
```

这是一份人类可读的"配置源码"——`glusterd` 会把它"编译"成后面几个 `.vol` 文件。注意里面有一组自动生成的 `username`/`password`，这不是给终端用户登录用的，而是 Gluster 内部 client-brick 通信时做身份校验的凭证（Lesson 9 讲 RPC 时会用到这个细节）。（见 [evidence/07-info-file.txt](evidence/07-info-file.txt)）

### 现象 6：client 端 volfile ——这是 Lesson 4 挂载时 client 真正拿到的东西

```
volume gv-dist-client-0
    type protocol/client
    option remote-host node1
    option remote-subvolume /data/brick1
    ...
end-volume
(client-1 指向 node2，client-2 指向 node3，结构一样)

volume gv-dist-dht
    type cluster/distribute
    subvolumes gv-dist-client-0 gv-dist-client-1 gv-dist-client-2
end-volume

volume gv-dist-utime ... subvolumes gv-dist-dht
volume gv-dist-write-behind ... subvolumes gv-dist-utime
volume gv-dist-open-behind ... subvolumes gv-dist-write-behind
volume gv-dist-quick-read ... subvolumes gv-dist-open-behind
volume gv-dist-md-cache ... subvolumes gv-dist-quick-read
volume gv-dist-io-threads ... subvolumes gv-dist-md-cache
volume gv-dist (type debug/io-stats) ... subvolumes gv-dist-io-threads
```

这份文件本质是一个 **翻译器（Translator）调用栈的声明式配置**：从下往上先是 3 个 `protocol/client`（各自知道要连哪台机器的哪个 Brick），往上叠了一层 `cluster/distribute`（决定文件该去哪个 Brick），再往上叠了一串性能优化层（`write-behind`/`quick-read`/`md-cache`/`io-threads`）。**这就是"Volume"这个概念在物理上的真身**：一张写死了"该找谁、该按什么顺序处理请求"的配置图。这一层每个节点是干什么的，会在 Lesson 6 详细展开，这里先建立一个印象：Volume = Translator 调用栈的蓝图。（见 [evidence/09-client-volfile.txt](evidence/09-client-volfile.txt)）

### 现象 7：Brick 端（`glusterfsd`）volfile——服务端也有一整套 Translator

```
volume gv-dist-posix        type storage/posix       (最底层，直接对接文件系统)
volume gv-dist-trash        subvolumes gv-dist-posix
volume gv-dist-changelog    subvolumes gv-dist-trash
volume gv-dist-bitrot-stub  ...
volume gv-dist-access-control ...
volume gv-dist-locks        ...
...(省略若干层)...
volume gv-dist-index        option xattrop-pending-watchlist trusted.afr.gv-dist-
volume /data/brick1 (type debug/io-stats)  subvolumes gv-dist-quota
volume gv-dist-server       type protocol/server
    option auth.login.8ecb76ec-...password.77f0f621-...
    subvolumes /data/brick1
end-volume
```

顶层是 `protocol/server`，用刚才在 `info` 文件里看到的那组 `username`/`password` 做校验——这和 client volfile 里的凭证**完全对应**，这就是 client 与 brick 建立连接时互相认证的机制。（见 [evidence/10-brick-side-volfile.txt](evidence/10-brick-side-volfile.txt)）

## 七、原理分析

```
用户输入:
  gluster volume create gv-dist node1:/data/brick1 node2:/data/brick1 node3:/data/brick1
        |
        v
node1:glusterd
  1. 校验参数（是不是根分区、目录是否已被其他 volume 占用……）
  2. 生成 volume-id（UUID）
  3. 在 Brick 目录上打两个 xattr：trusted.gfid、trusted.glusterfs.volume-id
  4. 把 "info"（源配置）+ 一组编译好的 volfile（client 端/每个 brick 端）
     写到本地 /var/lib/glusterd/vols/gv-dist/
        |
        v
node1:glusterd --管理面 RPC--> node2:glusterd, node3:glusterd
  把同一份配置（逐字节相同）复制过去，对方也落盘到自己的
  /var/lib/glusterd/vols/gv-dist/，并用 cksum 校验一致性
        |
        v
gluster volume start gv-dist
  三个节点各自根据本地的 "gv-dist.<hostname>.<brick>.vol"
  启动一个 glusterfsd 进程，去实际管理自己那个 Brick
```

**Volume 创建不搬数据，只做三件事**：打标记（xattr）、生成配置（volfile）、把配置复制给所有节点。这解释了为什么"创建 Volume"几乎是瞬间完成的操作，即便 Brick 目录里已经有几十 GB 数据——因为它压根不摸数据本体。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：Volume 配置存一个中心数据库（比如一个 MySQL 表：volume -> bricks 映射）。**

- 好处：查询方便，天然支持复杂事务。
- 代价：又是"引入一个新的强依赖组件"的老问题（呼应 Lesson 1 的结论）。而且，如果客户端每次挂载都要连这个数据库查配置，这个数据库就成了新的单点、新的性能瓶颈。

**备选方案 B：客户端每次访问文件都实时向 `glusterd` 问"这个文件在哪个 Brick"。**

- 好处：配置变更能立刻生效。
- 代价：每一次文件访问都要多一次"问路"的网络往返，在真实高并发场景下，这个"元数据服务"本身就会成为吞吐瓶颈——这正是许多分布式文件系统（比如早期 HDFS 的 NameNode）被诟病的地方。

**GlusterFS 选择的方案：把"问路"这件事提前到"挂载"那一刻做一次，之后全靠本地缓存的配置自己计算。**

- 客户端挂载时只问一次："给我这个 Volume 的配置图"，拿到手的就是我们刚才看到的那份 volfile。
- 之后每一次文件操作，客户端凭这份本地配置图 + 一个哈希算法（Lesson 5 的内容）**自己算出**该去连哪个 Brick，完全不需要再问任何人。
- 代价是：如果集群拓扑变化（加节点、Brick 迁移），已经挂载的旧客户端不会自动感知，需要重新触发一次配置拉取（这也是为什么后面版本的 Gluster 有"client 定期轮询 volfile 版本"的机制）。

一句话总结设计哲学：**把本可以摊在"每次请求"上的开销，一次性预付在"连接建立"那一刻，用一点"配置滞后"的代价换取运行时的极致简单和高吞吐。**

## 九、源码位置（仅指路）

- `glusterd/src/glusterd-volgen.c` —— volfile 生成逻辑，把用户的 `volume create` 参数"编译"成一堆 `.vol` 文件的地方。
- `libglusterfs/src/graph.c` —— 解析 `.vol` 文件、构建 Translator 调用图的通用逻辑。

## 十、本实验总结

1. `gluster volume create` 不搬数据，只做三件事：校验、打 xattr 标记、生成并复制配置。
2. Volume 的"真身"是一组 `.vol` 配置文件（volfile），描述了一张 Translator 调用图。
3. 客户端用的 volfile 和 Brick 端用的 volfile 是不同的两份文件，各自的 Translator 组合不一样。
4. Volume 配置和 Peer 信息一样，会被复制到集群里每一个节点本地磁盘，用 `cksum` 校验一致性。
5. Brick 一旦"入伙"某个 Volume，会被打上 `trusted.glusterfs.volume-id` 标记，这个标记就是它归属关系的唯一凭证。

## 十一、思考题

1. 如果我把 node1 的 `/var/lib/glusterd/vols/gv-dist/` 整个目录删掉，node1 的 `glusterd` 重启之后还知道这个 Volume 吗？其他节点会不会帮它恢复？
2. 现象 6 里看到 client volfile 里有 3 个 `protocol/client`，如果以后 Volume 扩容加了第 4 个 Brick，这个数字会变成 4 吗？已经挂载的客户端会立刻知道吗？
3. 如果直接手改某个节点上 `.vol` 文件的内容（不通过 `gluster volume set`），会发生什么？为什么 Gluster 要专门维护一个 `cksum` 文件？
4. Brick 目录被打上的 `trusted.glusterfs.volume-id` 是"目录级别"的标记，那 Brick 目录**里面**的每一个文件呢？它们有没有类似的归属标记？（提示：留意 Lesson 5 会揭晓文件级别的 GFID 和 layout 信息）

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 用非独立分区目录创建会被保护性拒绝，与目录是否为空无关 | [evidence/01-create-attempt-dirty.txt](evidence/01-create-attempt-dirty.txt) |
| `force` 创建成功，默认类型为 Distribute；每个 Brick 对应独立端口/进程 | [evidence/02-create-with-force.txt](evidence/02-create-with-force.txt)、[evidence/03-start-volume.txt](evidence/03-start-volume.txt)、[evidence/04-volume-info-status.txt](evidence/04-volume-info-status.txt) |
| Volume 创建后 Brick 目录多出 `trusted.gfid`/`trusted.glusterfs.volume-id`，且三节点一致 | [evidence/05-getfattr-after-volume.txt](evidence/05-getfattr-after-volume.txt) |
| 只在 node1 操作，配置逐字节复制到 node2/node3，cksum 一致 | [evidence/06-vols-dir-all-nodes.txt](evidence/06-vols-dir-all-nodes.txt)、[evidence/08-cksum-all-nodes.txt](evidence/08-cksum-all-nodes.txt) |
| `info` 文件是源配置，包含内部认证凭证 | [evidence/07-info-file.txt](evidence/07-info-file.txt) |
| client 端 volfile 展示完整 Translator 调用栈 | [evidence/09-client-volfile.txt](evidence/09-client-volfile.txt) |
| brick 端（`glusterfsd`）volfile 展示服务端 Translator 栈与认证凭证对应关系 | [evidence/10-brick-side-volfile.txt](evidence/10-brick-side-volfile.txt) |

---

上一课：[Lesson 2 - Brick](../lesson02-brick/README.md)
下一课：[Lesson 4 - Client Mount](../lesson04-client-mount/README.md)
