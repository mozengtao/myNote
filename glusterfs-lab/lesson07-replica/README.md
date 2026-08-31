# Lesson 7 - Replica

## 一、本实验目的

证明：**Replica（副本）不是"Brick 之间互相同步数据"，而是"Client 端有一个 AFR（Automatic File Replication）Translator，把每一次写操作原样并行发送给所有副本 Brick"。同步的发起方永远是 Client，Brick 之间没有直接通信。**

回到课程开头的问题："Replica 是如何同步数据的？"——这一课给出可验证的答案（下一课 Self Heal 再回答"万一某个副本没跟上，怎么补回来"）。

## 二、背景知识

- 到目前为止的 `gv-dist` 是 Distribute 类型：一个文件只存在于一个 Brick。本课创建一个新的 Volume `gv-rep`，类型是 **Replicate**：一个文件会同时存在于**所有** Brick 上（这里配置成 3 副本，即"每一份数据在 3 台机器上都有完整拷贝"）。
- 负责这件事的 Translator 叫 AFR（Automatic File Replication，内部名字类似 `cluster/replicate`），在 Lesson 6 已经知道了"扇出-收拢"这个模式，AFR 就是这个模式在"复制"场景下的具体应用。
- 本课不解决"某个副本掉线之后怎么追赶"的问题，那是 Self Heal（Lesson 8）的内容。这里只关心"一切正常时，数据是怎么变成三份一样的"。

## 三、提出问题

1. Replica 卷的总可用容量，应该是 N 个 Brick 容量之和，还是等于 1 个 Brick 的容量？为什么？
2. 一次 `write()`，AFR 是"先写第一个副本，等它成功了再写第二个"，还是"同时给所有副本发请求"？这两种方式对写延迟的影响完全不同，猜一猜 Gluster 选了哪种？
3. 如果 AFR 采用"client 并行发给所有副本"，会不会存在"两个副本写成功了，第三个还没收到请求，此时机器崩溃"的情况？Gluster 会怎么记录这种"没写完"的状态？
4. Replica 和 Distribute 在 Lesson 6 里看到的"扇出"行为，是同一回事吗？

## 四、实验拓扑

```
                       client (172.28.0.20)
                              |
                mount -t glusterfs node1:/gv-rep /mnt/gluster-rep
                              |
              AFR (cluster/replicate) Translator
                              |
        +---------------------+---------------------+
        |     (三路并行，不是依次)                      |
        v                     v                     v
      node1                 node2                 node3
   /data/brick2           /data/brick2           /data/brick2
   (新建的一组 brick，与     (同上)                  (同上)
    Lesson2-6 用的
    /data/brick1 无关)

                Volume: gv-rep (Replica 3)
```

这里特意新建了 `/data/brick2` 而不是复用 `/data/brick1`——因为一个目录一旦打上某个 Volume 的 `volume-id`，就不能被另一个 Volume 复用了（这也从侧面印证了 Lesson 3 的结论：那个 xattr 标记就是"归属关系的唯一凭证"）。

## 五、实验步骤

### Step 1：创建一个 Replica 3 的新卷并挂载

```bash
docker exec node1 mkdir -p /data/brick2   # node2、node3 同样操作
docker exec node1 gluster volume create gv-rep replica 3 \
  node1:/data/brick2 node2:/data/brick2 node3:/data/brick2 force
docker exec node1 gluster volume start gv-rep
docker exec client mount -t glusterfs node1:/gv-rep /mnt/gluster-rep
```

### Step 2：写一个文件，去三个 Brick 后端分别确认

```bash
docker exec client bash -c "echo 'replica payload content' > /mnt/gluster-rep/rep-test.txt"
docker exec node1 cat /data/brick2/rep-test.txt
docker exec node2 cat /data/brick2/rep-test.txt
docker exec node3 cat /data/brick2/rep-test.txt
docker exec client getfattr -n trusted.glusterfs.pathinfo -e text /mnt/gluster-rep/rep-test.txt
```

### Step 3：看 AFR 在文件上留下的簿记 xattr

```bash
docker exec node1 getfattr -d -m . -e hex /data/brick2/rep-test.txt
```

### Step 4：strace 观察一次真正的写入（`dd`），看写请求是并行发给三个 Brick，还是依次发送

```bash
strace -tt -f -p <gv-rep对应的glusterfs进程PID> -e trace=readv,writev -e write=7 -e read=7 -yy -s 150 -o /tmp/rep-write.strace
# 另一个终端:
dd if=/dev/zero of=/mnt/gluster-rep/rep-write-test.bin bs=4096 count=1
```

统计每个 Brick 对应的 socket 收到了多少次 `writev`，比较它们是否几乎同时发生、次数是否相等。

## 六、实验现象

### 现象 1：Replica 3 卷的总容量 ≈ 1 个 Brick 的容量，不是 3 倍

```
Filesystem      Size  Used Avail Use% Mounted on
node1:/gv-rep  1007G   66G  900G   7% /mnt/gluster-rep
```

对比 Lesson 4 里 `gv-dist`（Distribute，3 Brick）的 `3.0T`——**Replica 3 拿 3 台机器的容量，换来的是 1 份数据能扛住 2 台机器同时坏掉，而不是把容量叠加起来**。这是"用空间换可靠性"最直观的体现。（见 [evidence/02-mount-replica.txt](evidence/02-mount-replica.txt)）

### 现象 2：一个文件，三个 Brick 上都有一份内容完全相同的拷贝

```
--- node1 --- replica payload content
--- node2 --- replica payload content
--- node3 --- replica payload content
```

`pathinfo` 也证实了这一点，注意类型从 Lesson 5 的 `DISTRIBUTE` 变成了 `REPLICATE`，而且列出了全部三个位置（不是一个）：

```
trusted.glusterfs.pathinfo="(<REPLICATE:gv-rep-replicate-0>
  <POSIX(/data/brick2):node2:/data/brick2/rep-test.txt>
  <POSIX(/data/brick2):node3:/data/brick2/rep-test.txt>
  <POSIX(/data/brick2):node1:/data/brick2/rep-test.txt>)"
```

（见 [evidence/03-write-and-check-all-bricks.txt](evidence/03-write-and-check-all-bricks.txt)、[evidence/04-pathinfo-replica.txt](evidence/04-pathinfo-replica.txt)）

### 现象 3：文件上多了一个 DHT 从来没有的 xattr：`trusted.afr.dirty`

```
trusted.afr.dirty=0x000000000000000000000000   <- 24 个字节，全是 0
trusted.gfid=0x7fbb4ee89b2f4e74a998b20d3c2a7b24  <- 三个 Brick 上这个值完全相同
```

三个 Brick 上这个文件的 `trusted.afr.dirty` 都是全零，`trusted.gfid` 完全一致（见 [evidence/05-afr-xattrs.txt](evidence/05-afr-xattrs.txt)）。**这个"全零"状态本身就是证据**：`dirty` 记录的是"我怀疑自己和其他副本不一致"的标记位，全零表示"这次写操作三个副本都确认完成了，没有任何一方掉队"。这正是 Lesson 8 要利用的机制——如果强行让某个副本"跟不上"，这个值就不会是全零了。

### 现象 4：写入时，三个 Brick 收到的网络写入次数完全相同、时间几乎同步

对一次 `dd` 写入抓 strace，统计三个 Brick 对应 socket 的 `writev` 调用次数：

```
写给 node1 (fd 14): 16 次
写给 node2 (fd 9) : 16 次
写给 node3 (fd 10): 16 次
```

而且时间戳交替出现（`14 -> 9 -> 10 -> 14 -> 9 -> 10 ...`），间隔都在几百微秒以内。（完整原始记录见 [evidence/06-write-fanout-strace.txt](evidence/06-write-fanout-strace.txt)）**这证明 AFR 不是"写完一个副本再写下一个"的串行策略，而是把同一套操作序列（lookup、xattrop 标记"我要开始写了"、真正的 write、再 xattrop 标记"写完了"）并行广播给所有副本，几乎同时发生。**

## 七、原理分析

```
应用程序: write(fd, buf, 4096)
        |
        v
   AFR (cluster/replicate)
        |
        | 1. 先给每个副本发一个 "xattrop" 请求，把 trusted.afr.dirty 置位
        |    (相当于先立一个"我要动手了，如果中途失败，你们要记得自己可能不一致"的字条)
        |
        | 2. 把真正的 write 请求，同时发给全部 N 个副本子卷（不是依次）
        |
        | 3. 等全部副本都返回成功
        |
        | 4. 再发一次 xattrop，把 trusted.afr.dirty 清零
        |    (相当于说"这次大家都写完了，字条可以撕掉")
        |
        v
   全部确认后，才把最终结果返回给上层（FUSE -> 应用程序）
```

**这一整套"先立字条、并行写、再撕字条"的流程，就是为了解决现象 3 提出的那个问题**：如果写到一半机器崩溃，字条（`dirty` 标记）会一直留在存活的副本上，成为"我怀疑我和别人不一样"的物证——这是 Self Heal（下一课）能够"知道该修哪里"的根本依据。AFR 采用并行写而不是串行写，是因为**串行会让写延迟等于最慢那个副本的延迟之和**，而并行写的延迟只取决于最慢的那一个副本（近似于 `max()` 而不是 `sum()`）——这对追求"多副本高可用又不能牺牲太多性能"的场景是必要的权衡。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：Brick 之间互相同步（类似 MySQL 主从复制，client 只写一个"主 Brick"，其他 Brick 自己去拉数据）。**

- 好处：Client 逻辑简单，只维护一条连接。
- 代价：又是"谁是主"的问题——一旦引入"主 Brick"的概念，就要解决主宕机后怎么选新主、选主期间服务是否可用等一整套分布式共识问题，这类协议（Raft/Paxos）本身就很复杂，和 Gluster"去中心化"的设计哲学（Lesson 1、4 反复出现的主题）相悖。

**备选方案 B：Client 只写一个副本，由这个副本异步转发给其他副本（链式复制）。**

- 好处：Client 的写延迟只取决于第一个副本，看起来"更快"。
- 代价：数据"最终"才会一致，短时间内不同副本读到的内容可能不同（弱一致性）；而且"转发"这件事本身还是需要一个明确的顺序和触发机制，复杂度只是从 client 转移到了 Brick 端，并没有消失。

**GlusterFS 选择的方案：Client 端并行扇出写全部副本，用 xattr 记录"未完成"状态。**

- 好处：**一致性判断完全交给 Client 端的 AFR 逻辑处理**，Brick 端毫无感知自己是"第几个副本"，Brick 只是老老实实的 `storage/posix`——这符合 Lesson 2 的结论"Brick 只管存字节"，复制这种分布式语义被干净地隔离在 Translator 层，不污染存储层。
- 好处 2：因为是并行写，只要多数（或全部，取决于配置）副本写成功，client 就能拿到确认，延迟不会因为副本数增加而线性增长。
- 代价：Client 需要同时维护到所有副本的连接、需要更复杂的错误处理逻辑（一个副本超时怎么办、两个副本返回结果不一致怎么办）——这些复杂度从"要不要一个主节点"的问题，变成了"AFR 内部状态机要处理哪些异常分支"的问题，是可控、局部化的复杂度，而不是全局性的架构复杂度。

一句话总结设计哲学：**把"数据要在哪几台机器上有几份"这个问题，转化成"client 端一个 Translator 该给哪几个下游子卷发同样的请求"——本质上和 DHT 是同一套扇出-收拢框架的不同参数化实例，只是 DHT 选"发给一个"，AFR 选"发给全部"。**

## 九、源码位置（仅指路）

- `xlators/cluster/afr/src/` —— AFR（Automatic File Replication）的核心实现，`afr_dirty` 相关的标记逻辑、并行写的调度逻辑都在这里。
- `xlators/cluster/afr/src/afr-transaction.c` —— "先标记 dirty、再写、再清除 dirty"这一整套事务性流程的具体代码。

## 十、本实验总结

1. Replica 卷的可用容量等于单个 Brick 的容量，而不是所有 Brick 容量之和——这是用空间换可靠性的直接体现。
2. AFR 是 client 端的 Translator，负责把写操作并行扇出给所有副本，Brick 之间没有任何直接通信。
3. AFR 用 `trusted.afr.dirty` 这类 xattr 在写操作前后打标记，记录"这次操作是否所有副本都确认完成"。
4. 并行写（而非串行写）是为了让多副本的写延迟不随副本数线性增长。
5. Replica（AFR）和 Distribute（DHT）在 Translator 层面是同一套"扇出-收拢"框架的两种不同参数化：DHT 只发给一个目标子卷，AFR 发给全部子卷。

## 十一、思考题

1. 如果三个副本中有一个网络特别慢（比如延迟高出其他两个 10 倍），AFR 的"等全部确认"策略会不会让整体写延迟被这一个慢副本拖累？Gluster 有没有"多数确认即可返回"的选项？
2. 现象 3 里 `trusted.afr.dirty` 全零，意味着"没有任何未完成的写"。如果 client 在发完写请求、但还没收到任何一个 Brick 确认之前就崩溃了，此时三个 Brick 上的 `dirty` 状态会是什么样？（提示：想想"先标记再写"这个顺序,是为了应对谁先崩溃的场景）
3. Replica 3 里如果只有 2 个 Brick 确认写成功、第 3 个因为网络分区一直没响应，AFR 该继续等，还是该直接向应用返回成功？这个决策和"quorum（法定人数）"概念有什么关系？
4. 如果把 Replica 和 Distribute 组合使用（比如 6 个 Brick，分成 2 组，每组 3 副本，组间再做 Distribute），Translator 图会长什么样？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 创建并启动 Replica 3 卷 | [evidence/01-create-replica-volume.txt](evidence/01-create-replica-volume.txt) |
| 挂载后总容量 ≈ 单 Brick 容量，而非三倍 | [evidence/02-mount-replica.txt](evidence/02-mount-replica.txt) |
| 文件在三个 Brick 上都有完整拷贝，内容一致 | [evidence/03-write-and-check-all-bricks.txt](evidence/03-write-and-check-all-bricks.txt) |
| `pathinfo` 显示 REPLICATE 类型 + 全部三个位置 | [evidence/04-pathinfo-replica.txt](evidence/04-pathinfo-replica.txt) |
| `trusted.afr.dirty` 全零，GFID 三处一致 | [evidence/05-afr-xattrs.txt](evidence/05-afr-xattrs.txt) |
| strace 证实写请求并行扇出给三个 Brick，次数相等、时序交替 | [evidence/06-write-fanout-strace.txt](evidence/06-write-fanout-strace.txt) |

---

上一课：[Lesson 6 - Translator](../lesson06-translator/README.md)
下一课：[Lesson 8 - Self Heal](../lesson08-self-heal/README.md)
