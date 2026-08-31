# Lesson 5 - Distributed Hash (DHT)

## 一、本实验目的

证明：**文件最终写到哪个 Brick，不是随机、不是轮询、不是按容量负载均衡决定的，而是"对文件名做哈希，落在哪个 Brick 预先划定的哈希区间里，就写到哪个 Brick"——这是一个纯粹的、确定性的计算过程，不需要查任何表。**

回到课程开头的问题："文件为什么最终写到了 node2？"——这一课给出可验证的答案。

## 二、背景知识

- Lesson 3 已经见过 client volfile 里有一层 `cluster/distribute`（内部名字 `gv-dist-dht`），当时没有展开讲它具体做什么，本课就是讲它。
- DHT = Distributed Hash Table。GlusterFS 的 DHT translator 会把一个 32 位整数空间（`0x00000000` ~ `0xFFFFFFFF`）**切成若干段连续区间**，每个 Brick 分配到一段。
- 每个目录（包括 Volume 根目录）在每个 Brick 上都会被打一个 `trusted.glusterfs.dht` 扩展属性，记录"这个 Brick 在这个目录下负责哪一段区间"。
- 创建文件时，DHT 对**文件名**（不是文件内容）做哈希，得到一个 32 位数字，看这个数字落在哪个 Brick 的区间里，就把文件交给那个 Brick 的 `protocol/client` 子卷去创建。

## 三、提出问题

1. 三个 Brick 各自负责的哈希区间，是人为指定的，还是自动算出来的？会不会有重叠或空隙？
2. 同一个文件名，两次计算的哈希值一样吗？如果我删除一个文件再用同名重新创建，它还会落在同一个 Brick 上吗？
3. 决定文件位置的，是"文件的内容"还是"文件的名字"？如果我改名（`mv a.txt b.txt`），文件会不会跟着"搬家"到另一个 Brick？
4. 如果哈希函数的输出不是完全均匀的，17 个文件放进 3 个 Brick，会是严格的 1:1:1 吗？

## 四、实验拓扑

```
                          Volume gv-dist (Distribute)
                       32bit 哈希空间 [0x00000000, 0xFFFFFFFF]
                       被切成三段，分别交给三个 Brick 负责

     node2                        node3                       node1
[0x00000000,               [0x55555555,                [0xAAAAAAAA,
 0x55555554]                0xAAAAAAA9]                 0xFFFFFFFF]
   约前 1/3                    约中 1/3                     约后 1/3

           client 写入文件时: hash(文件名) -> 32bit 整数 -> 落在哪段区间就交给哪个 Brick
```

## 五、实验步骤

### Step 1：看每个 Brick 到底分到了哪段哈希区间

为什么做：这是 DHT 工作的"配置基础"，必须先看到区间划分，才能验证后面文件的去向是不是符合这个划分。

```bash
docker exec node1 getfattr -d -m . -e hex /data/brick1
docker exec node2 getfattr -d -m . -e hex /data/brick1
docker exec node3 getfattr -d -m . -e hex /data/brick1
```

`trusted.glusterfs.dht` 的十六进制值要这样解读（16 字节，分 4 段，每段 4 字节 / 8 个十六进制字符）：

```
0x 00000001 | 00000000 | AAAAAAAA | FFFFFFFF
   version    commit哈希   区间起点     区间终点
```

### Step 2：批量创建不同文件名的文件

为什么做：单个文件的落点没有统计意义，多个不同名字的文件放在一起，才能看出"按名字确定性分布"这个规律。

```bash
for i in $(seq -w 1 15); do
  docker exec client bash -c "echo 'content of file-$i' > /mnt/gluster/file-$i.txt"
done
```

### Step 3：权威查询每个文件实际落在哪个 Brick

为什么做：`getfattr -n trusted.glusterfs.pathinfo` 是 Gluster 提供的"官方调试接口"，直接问 DHT 翻译层"这个文件你交给了谁"，比自己满世界 `find` 更权威、更准确。

```bash
docker exec client getfattr -n trusted.glusterfs.pathinfo -e text /mnt/gluster/file-01.txt
```

### Step 4：确定性验证——删除重建，是否还落在原来的 Brick

为什么做：验证"文件名决定位置"这个假说的因果性，而不仅仅是相关性。

```bash
docker exec client rm /mnt/gluster/file-01.txt
docker exec client bash -c "echo 'recreated content' > /mnt/gluster/file-01.txt"
docker exec client getfattr -n trusted.glusterfs.pathinfo -e text /mnt/gluster/file-01.txt
```

### Step 5：统计分布，看是否大致均衡

```bash
for n in node1 node2 node3; do
  docker exec $n bash -c "ls /data/brick1/file-*.txt | wc -l"
done
```

## 六、实验现象

### 现象 1：三个 Brick 的哈希区间正好无缝拼成完整的 32 位空间

```
node2: trusted.glusterfs.dht = 0x00000001 00000000 00000000 55555554
node3: trusted.glusterfs.dht = 0x00000001 00000000 55555555 AAAAAAA9
node1: trusted.glusterfs.dht = 0x00000001 00000000 AAAAAAAA FFFFFFFF
```

拼起来正好是 `[0x00000000, 0x55555554] + [0x55555555, 0xAAAAAAA9] + [0xAAAAAAAA, 0xFFFFFFFF]` —— **首尾相接，无重叠、无空隙，正好三等分整个 32 位空间**。这是 Gluster 在 `gluster volume create` 那一刻自动算好、写入每个 Brick 的（回忆 Lesson 3：这正是"打标记"里没细讲的那部分内容）。（完整输出见 [evidence/01-dht-layout-xattr.txt](evidence/01-dht-layout-xattr.txt)）

### 现象 2：15 个文件分布在三个 Brick 上，符合各自的哈希区间

```
file-01.txt -> node3     file-06.txt -> node2     file-11.txt -> node2
file-02.txt -> node2     file-07.txt -> node1     file-12.txt -> node2
file-03.txt -> node3     file-08.txt -> node2     file-13.txt -> node1
file-04.txt -> node1     file-09.txt -> node3     file-14.txt -> node2
file-05.txt -> node3     file-10.txt -> node1     file-15.txt -> node3
```

统计：node1 = 4 个，node2 = 6 个，node3 = 5 个（见 [evidence/03-pathinfo-per-file.txt](evidence/03-pathinfo-per-file.txt)、[evidence/06-distribution-stats.txt](evidence/06-distribution-stats.txt)）——不是均分成 5/5/5，但也没有明显偏斜；这正是哈希分布的特点：**局部有波动，样本量越大越接近均匀**（这是概率统计规律，不是 Gluster 的 bug）。

### 现象 3：交叉验证——`pathinfo` 说的位置和实际后端目录完全对得上

```
$ docker exec node3 find /data/brick1 -name "file-01.txt" -exec cat {} \;
content of file-01
```

`pathinfo` 说 `file-01.txt` 在 node3，直接去 node3 的后端目录找，确实存在且内容正确。（见 [evidence/04-cross-verify.txt](evidence/04-cross-verify.txt)）

### 现象 4：删除重建，还是回到同一个 Brick——证明这是"计算"而不是"记录"

```
$ docker exec client rm /mnt/gluster/file-01.txt
$ docker exec client bash -c "echo 'recreated content' > /mnt/gluster/file-01.txt"
$ getfattr -n trusted.glusterfs.pathinfo ... file-01.txt
"(<DISTRIBUTE:gv-dist-dht> <POSIX(/data/brick1):node3:/data/brick1/file-01.txt>)"
```

文件被彻底删除、连 inode 都不在了，重新创建后依然精确落在 node3——**这不可能是"记住了上次的位置"，只可能是"名字没变，哈希值就没变，算出来的目标 Brick 自然也没变"**。（见 [evidence/05-determinism-test.txt](evidence/05-determinism-test.txt)）

## 七、原理分析

```
应用程序: creat("/mnt/gluster/file-01.txt")
        |
        v
   DHT translator (gv-dist-dht)
        |
        | 1. hash = HashFunction("file-01.txt")   <- 只用文件名，跟内容无关
        | 2. 遍历自己缓存的每个子卷的 layout (起点,终点)
        | 3. 找到 hash 落在哪个 [起点,终点] 区间里
        |
        v
   命中 gv-dist-client-2 (对应 node3)
        |
        v
   把 create 请求转发给这个特定的 protocol/client 子卷
        |
        v
   经 RPC 发到 node3 的 glusterfsd，真正在 /data/brick1 下创建文件
```

**这是全课程最关键的一次"去中心化"体现**：DHT 不需要问任何人"这个文件该放哪"——它自己拿着挂载时缓存下来的 layout 表（现象 1 里看到的那几个 xattr 值），本地做一次哈希计算就能确定目标，零网络往返。这也直接回答了"Client 为什么知道文件在哪"——它不是"知道"，而是"能算出来"，这是本质区别：**知道需要查表/问人，算出来只需要一个确定性函数**。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：维护一个"文件名 -> Brick"的映射表（类似一个索引/字典）。**

- 好处：可以做任意复杂的放置策略（比如按大小分类、按访问热度分类）。
- 代价：这个表本身要存在哪？如果存中心数据库，就是重新引入了 Metadata Server（回到 Lesson 4 想避免的问题）；如果每个节点都存一份完整映射表，创建一个文件就要把这条记录同步给所有节点——文件越多，这个表越大，同步成本越高，最终会成为整个系统的瓶颈。

**备选方案 B：轮询（Round-robin）——第 1 个文件给 Brick A，第 2 个给 Brick B，依次循环。**

- 好处：分布非常均匀。
- 代价：**必须要有一个全局计数器**，才能知道"上一个文件给了谁，这次该给谁"——这个计数器天然是有状态的、需要被同步的东西，多个 client 并发写入时还要处理计数器的并发竞争。本质上又是"需要一个共享状态"的老问题。

**GlusterFS 选择的方案：一致性哈希 + 预先划分好的区间表。**

- 好处：区间表（layout）是在"目录创建"这种低频操作时算好并固化下来的，此后每一次文件创建都是纯计算、无状态、无需协调——这是"用一次性的、低频的协调成本，换取持续的、高频的无协调操作"，和 Lesson 4 volfile 的设计思路是同一个哲学的不同应用。
- 代价：正因为区间是"预先划分并固化"的，如果后续增加 Brick，就必须重新划分区间（后面会讲到 rebalance），这时已经存在的文件如果不主动搬迁，可能会出现"用旧区间算出来的位置"和"新区间该在的位置"不一致的情况——DHT 用一种叫 "linkto file"（占位链接）的机制处理这个过渡态，这也是很多人抱怨 Gluster 扩容后需要 rebalance 的原因。

一句话总结设计哲学：**用哈希函数把"存储位置"这个需要被记住的状态，变成了一个可以被随时重新计算的结果——这是分布式系统里"用计算换存储、用计算换协调"的经典手法。**

## 九、源码位置（仅指路）

- `xlators/cluster/dht/src/` —— DHT 翻译器的核心实现，哈希函数、layout 计算、rebalance 逻辑都在这里。
- `libglusterfs/src/hashfn.c`（或类似路径，版本不同可能略有差异）—— 具体的哈希算法实现。

## 十、本实验总结

1. 每个 Brick 在每个目录下都被分配一段连续的 32 位哈希区间，所有 Brick 的区间拼起来正好覆盖整个空间、无缝无重叠。
2. 文件的目标 Brick 是对**文件名**做哈希算出来的，和文件内容、和创建顺序都无关。
3. 这个过程是纯本地计算，client 不需要问任何人（呼应 Lesson 4 的"零协调"发现）。
4. 因为是确定性哈希，同名文件删除重建、总是落在同一个 Brick。
5. 样本量大时，文件会大致均匀地散布在所有 Brick 上，但不保证每次都严格相等。

## 十一、思考题

1. 如果给 Volume 新增第 4 个 Brick，三个已有 Brick 的哈希区间会怎么变？已经存在的文件会不会自动"搬家"到新的区间划分上？
2. 如果我 `mv file-01.txt renamed.txt`，文件会不会因为哈希值变了而被迫转移到另一个 Brick？（提示：想想"重命名"在 POSIX 语义上是一个元数据操作还是数据搬移操作，Gluster 会如何在保证语义的同时处理这个矛盾）
3. 如果 Volume 有一万个 Brick，layout 表（区间列表）会有多大？client 缓存这么大的表，代价是什么？
4. 现象 1 里看到区间是"三等分"，如果三个 Brick 的实际磁盘容量不一样（一个 1TB，两个 4TB），DHT 还会平均分配哈希区间吗？这会带来什么问题？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 三个 Brick 的哈希区间无缝三等分整个 32 位空间 | [evidence/01-dht-layout-xattr.txt](evidence/01-dht-layout-xattr.txt) |
| 批量写入 15 个文件 | [evidence/02-write-batch-files.txt](evidence/02-write-batch-files.txt) |
| 用 `pathinfo` 权威查询每个文件的真实落点 | [evidence/03-pathinfo-per-file.txt](evidence/03-pathinfo-per-file.txt) |
| 直接在后端目录交叉验证 `pathinfo` 的准确性 | [evidence/04-cross-verify.txt](evidence/04-cross-verify.txt) |
| 删除重建同名文件，落点不变，证明是计算而非记录 | [evidence/05-determinism-test.txt](evidence/05-determinism-test.txt) |
| 15 个文件在三个 Brick 上的分布统计（4/6/5） | [evidence/06-distribution-stats.txt](evidence/06-distribution-stats.txt) |

---

上一课：[Lesson 4 - Client Mount](../lesson04-client-mount/README.md)
下一课：[Lesson 6 - Translator](../lesson06-translator/README.md)
