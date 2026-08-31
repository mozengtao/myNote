# Lesson 8 - Self Heal

## 一、本实验目的

证明：**Self Heal 能够"知道该修哪个文件、该往哪个方向修"，靠的不是"扫描比对全部数据"，而是 Lesson 7 里看到的 AFR pending 标记（`trusted.afr.<volname>-client-N`）——存活的副本会记住"我欠 N 号副本多少笔账"，等它回来，就照着这本账去补。**

回到课程开头的问题："Self Heal 为什么能够恢复数据？"——这一课给出可验证的答案。

## 二、背景知识

- Lesson 7 已经看到，AFR 在每次写操作前后，会通过 `trusted.afr.dirty`、`trusted.afr.<volname>-client-N` 这类 xattr 记录"这次操作涉及哪些副本、有没有全部确认"。
- `glustershd`（gluster self-heal daemon）是每个节点上常驻的一个后台进程，专门负责巡检、执行 Self Heal，在 Lesson 3/7 的 `gluster volume status` 输出里已经能看到它（"Self-heal Daemon on ..."那几行）。
- `gluster volume heal <volname> info` 是查看"当前有哪些文件被判定为副本不一致，等待修复"的命令。

## 三、提出问题

1. 如果一个副本节点下线了一段时间，存活的副本要不要"记住"这段时间发生了什么变化？如果不记，节点回来之后怎么知道自己该补哪些内容？
2. "记账"这件事，是记在下线的那个节点上，还是记在存活的节点上？（提示：下线的节点根本不知道自己错过了什么）
3. 节点恢复上线后，Gluster 是把整个 Brick 的数据全量比对一遍，还是只根据"账本"处理真正变化过的那部分？这两种方式的代价差异有多大？
4. 如果两个副本都记录"对方欠自己的账"，会不会出现两边互相矛盾、谁也不知道该听谁的情况（也就是常说的"脑裂"）？

## 四、实验拓扑

```
                     写操作发生时: node3 已下线
        node1                  node2                  node3 (下线)
       在线，记账               在线，记账              不在线，不知情
  trusted.afr.gv-rep-      trusted.afr.gv-rep-
  client-2 = 待补 2 笔        client-2 = 待补 2 笔
       (client-2 就是 AFR 对 node3 这个子卷的内部编号)

                     node3 恢复后:
        node1 / node2 依然"记着账" ---heal---> node3 补齐差异
```

## 五、实验步骤

### Step 1：健康基线——确认 heal info 为空

```bash
docker exec node1 gluster volume heal gv-rep info
```

### Step 2：下线 node3，制造"副本落后"的场景

```bash
docker stop node3
```

### Step 3：node3 下线期间，通过 client 修改一个已有文件、新建一个文件

```bash
docker exec client bash -c "echo 'MODIFIED while node3 was down' > /mnt/gluster-rep/rep-test.txt"
docker exec client bash -c "echo 'created while node3 was down' > /mnt/gluster-rep/created-during-outage.txt"
```

### Step 4：在存活的 node1/node2 上查看 pending 标记

为什么做：这是本课最关键的一步——直接观察"账本"长什么样。

```bash
docker exec node1 getfattr -d -m . -e hex /data/brick2/rep-test.txt
```

### Step 5：查看此时的 `heal info`

```bash
docker exec node1 gluster volume heal gv-rep info
```

### Step 6：恢复 node3，看它自己磁盘上的数据是不是还停留在下线前的状态

```bash
docker start node3
docker exec node3 cat /data/brick2/rep-test.txt
docker exec node3 ls -la /data/brick2/
```

### Step 7：触发一次 Self Heal

```bash
docker exec node2 gluster volume heal gv-rep
```

### Step 8：验证 node3 的数据是否追平，`heal info` 是否清空，pending 标记是否清零

```bash
docker exec node3 cat /data/brick2/rep-test.txt
docker exec node1 gluster volume heal gv-rep info
docker exec node3 getfattr -d -m . -e hex /data/brick2/rep-test.txt
```

## 六、实验现象

### 现象 1：node3 下线期间，存活节点上出现了一个新的 xattr，值不再是零

```
node1: trusted.afr.gv-rep-client-2=0x000000020000000000000000
node2: trusted.afr.gv-rep-client-2=0x000000020000000000000000
```

对照 Lesson 7 现象 3 里"一切正常"时这个位置根本没有这个 xattr——**`gv-rep-client-2` 正是 AFR 给 node3 那个子卷分配的内部编号**（3 个子卷 client-0/1/2 分别对应 node1/node2/node3），这个 xattr 的出现，就是"存活节点记账"这件事最直接的物证：**它们各自都在自己的副本上标记"我这边比 client-2 多做了一些操作，它还不知道"**。（见 [evidence/03-pending-xattrs-on-survivors.txt](evidence/03-pending-xattrs-on-survivors.txt)）

### 现象 2：`heal info` 精确列出了到底哪些路径受影响

```
Brick node1:/data/brick2
/created-during-outage.txt
/
/rep-test.txt
Status: Connected
Number of entries: 3
```

**恰好是"新建的文件"、"根目录"（因为根目录下的目录项发生了变化）、"被修改的文件"这三项**，一个不多一个不少——这证明 Gluster 精确知道受影响范围，不需要也没有做全量扫描。（见 [evidence/04-heal-info-while-down.txt](evidence/04-heal-info-while-down.txt)）

### 现象 3：node3 恢复上线后，它自己的磁盘还是"过去的它"

```
$ docker exec node3 cat /data/brick2/rep-test.txt
replica payload content        <- 这是 Lesson7 写入的旧内容，不是 "MODIFIED..."
$ docker exec node3 ls -la /data/brick2/
(没有 created-during-outage.txt)
```

**node3 自己完全不知道自己"过时"了**——它没有任何机制能主动意识到这一点，这印证了思考问题 2：记账的主体必须是存活的一方，掉线方对"自己错过了什么"是彻底无感的。（见 [evidence/06-node3-stale-before-heal.txt](evidence/06-node3-stale-before-heal.txt)）

### 现象 4：从有"新鲜账本视图"的节点触发 heal，问题被精确修复

```
$ docker exec node2 gluster volume heal gv-rep
Launching heal operation to perform index self heal on volume gv-rep has been successful

$ docker exec node2 gluster volume heal gv-rep info
Number of entries: 0   (三个 Brick 都是 0)
```

（这里有个小插曲：一开始从 node1 触发 `heal` 失败了，因为 node1 当时对"node3 brick 是否已经重新上线"这条运行时状态的感知还没刷新过来——这也是个有意思的旁证：**运行时的"进程在线状态"和 Lesson 1/3 讲的"配置/成员关系"不是一回事，前者是易变的、有短暂滞后窗口的探活信息，后者才是持久化并强一致复制的部分**。换一个视图新鲜的节点（node2）发起就立刻成功了。见 [evidence/07-force-start-brick.txt](evidence/07-force-start-brick.txt)、[evidence/08-trigger-heal.txt](evidence/08-trigger-heal.txt)）

### 现象 5：heal 之后，node3 的数据和内容完全追平

```
$ docker exec node3 cat /data/brick2/rep-test.txt
MODIFIED while node3 was down
$ docker exec node3 cat /data/brick2/created-during-outage.txt
created while node3 was down
```

（见 [evidence/09-verify-after-heal.txt](evidence/09-verify-after-heal.txt)）

### 现象 6：账本被"划掉"——pending 标记恢复为全零

```
node1: trusted.afr.gv-rep-client-2=0x000000000000000000000000   <- 从 0x02... 变回全 0
node2: trusted.afr.gv-rep-client-2=0x000000000000000000000000
node3: (根本没有这个 xattr —— 一个节点不需要给自己记账)
```

（见 [evidence/10-xattrs-after-heal.txt](evidence/10-xattrs-after-heal.txt)）三个副本的 `trusted.gfid` 全程保持一致，证明 heal 操作没有创建"新文件"去替换，而是原地把内容和标记都同步好。

## 七、原理分析

```
写操作发生，某副本(node3)恰好下线:
   AFR 在存活副本(node1/node2)上把
   trusted.afr.gv-rep-client-2 计数 +1
   (语义: "我知道 client-2 应该也有这个变化，但它现在拿不到，记一笔账")
        |
        v
   node3 恢复上线，重新连接集群 (Lesson1 的 Peer 机制 + Lesson3 的配置同步)
        |
        v
   glustershd (自愈守护进程，每个节点常驻) 定期或被手动触发扫描:
     "有没有哪个文件，我看到的 pending 计数别人不是 0？"
        |
        v
   一旦发现 (通过 heal info 展示的那 3 个路径)，
   glustershd 发起一次 "source -> sink" 的同步:
     - 以 pending 计数为 0 的一方(或凭 changelog 判断更新的一方)作为数据源
     - 把内容/属性差异同步给落后的一方(node3)
        |
        v
   同步完成后，把 pending 计数清零 —— "账已经还清了"
```

**这套机制的关键洞察是：Gluster 把"哪些数据需要修复"这个问题，转化成了"读 xattr 里的一个计数器是否为零"，而不是"逐字节比较两份数据"**。计数器只在真正发生写操作、且当时确实有副本联系不上时才会变化——正常运行时这个开销几乎为零；出问题时，恢复的成本只和"实际变化过的文件数"成正比，而不是和"整个 Brick 的数据总量"成正比。这正是"pending 变更日志" (changelog) 类设计在分布式存储里被广泛采用的原因。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：节点恢复后，做一次全量的 `rsync`/`diff` 式比对，逐个文件比较内容和校验和。**

- 好处：实现简单，不需要维护额外的簿记状态；即便 pending 标记因为某些异常丢失了，这种方式依然能兜底。
- 代价：数据量越大，恢复时间越长——如果 Brick 里有几十 TB 数据，一次全量比对可能要跑数小时甚至数天，期间还要占用大量 I/O 和网络带宽，这对"节点短暂下线几分钟"这种常见场景是极大的浪费（本来只需要修几个文件，却要扫描全部）。

**备选方案 B：完全依赖外部的日志复制（比如把所有写操作记到一个共享的 WAL，节点恢复后重放 WAL）。**

- 好处：语义清晰，类似数据库的重做日志。
- 代价：这个共享 WAL 存在哪？如果是一个独立的中心化组件，又是"引入新的单点/新的强依赖"的老问题（呼应 Lesson 1）；如果每个节点自己维护一份 WAL，又要解决"WAL 之间怎么保持一致"的问题，复杂度并没有消失，只是转移了位置。

**GlusterFS 选择的方案：把"变更记录"直接附着在受影响的文件本身（xattr），存活的副本各自独立维护。**

- 好处：不需要额外的存储系统或日志文件——变更记录和它所描述的数据physically 长在一起，天然具备"文件搬到哪，变更记录就跟到哪"的特性（回忆 Lesson 2 的结论：xattr 就是为了这种和数据强绑定的簿记信息而存在的）。
- 好处 2：Self Heal 只需要遍历"pending 计数不为零"的条目（Gluster 内部会在一个索引目录 `.glusterfs/indices/` 里维护这些条目的快速索引，不需要遍历整个目录树），恢复速度只取决于"实际变更量"，和总数据量解耦。
- 代价：如果存活的副本自己也在 heal 完成前发生了硬盘损坏，这份"账本"就随之丢失了，此时就只能退回到笨办法（全量比对）——所以 Gluster 也提供了 `full` 模式的 heal 作为兜底手段。

一句话总结设计哲学：**用"事件发生时顺手记一笔账"替代"事后全量排查"，把恢复成本从"和总数据量成正比"降低到"和实际变更量成正比"——这是几乎所有增量备份、增量同步系统背后共享的核心思路，Gluster 只是把它做成了文件系统层面、用 xattr 承载的一种实现。**

## 九、源码位置（仅指路）

- `xlators/cluster/afr/src/afr-self-heal-common.c`、`afr-self-heald.c` —— Self Heal 具体的扫描、修复逻辑。
- `xlators/features/index/src/` —— 维护"哪些 GFID 待修复"的快速索引（`.glusterfs/indices/`），避免 Self Heal 需要遍历整个目录树寻找待修文件。

## 十、本实验总结

1. Self Heal 依赖的是 AFR 写操作时留下的 pending xattr（记账），不是事后的全量数据比对。
2. 记账的主体一定是"当时存活、感知到了变更"的一方；下线的一方对自己错过了什么完全无感。
3. `heal info` 能精确列出受影响的路径，证明恢复范围是按需计算的，而不是全量扫描的结果。
4. Self Heal 完成后，pending 计数清零，标志着"账已还清"，这是一个可验证、有终态的过程。
5. "运行时在线状态"（谁的 Brick 进程活着）和"持久化配置状态"（Peer/Volume 配置）是两类不同的状态，前者可能有短暂的滞后窗口，不能混为一谈。

## 十一、思考题

1. 如果 node1 和 node2 同时短暂断开过、又分别恢复，且这段时间内 client 因为网络问题分别往不同的存活副本上写入了冲突的数据，pending 计数机制还能干净地判断"该以谁为准"吗？这是不是就是常说的"脑裂"场景？Gluster 会怎么应对（提示：`gluster volume heal <vol> split-brain` 系列命令）？
2. 如果三副本卷同时有两个副本下线（只剩 1 个存活），存活的那个副本要不要给两个下线的副本分别记账？如果之后两个副本不是同时恢复，会发生什么？
3. `.glusterfs/indices/` 这个索引目录本身如果损坏了，会对 Self Heal 造成什么影响？这个索引目录本身需不需要被复制/修复？
4. 如果把 Self Heal 的触发方式从"手动 `gluster volume heal`"改成完全依赖 `glustershd` 的定期自动巡检，多久巡检一次比较合适？巡检太频繁和太不频繁分别有什么代价？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 健康基线，heal info 为空 | [evidence/00-baseline-heal-info.txt](evidence/00-baseline-heal-info.txt) |
| 停止 node3 | [evidence/01-stop-node3.txt](evidence/01-stop-node3.txt) |
| 下线期间修改/新建文件 | [evidence/02-write-while-node3-down.txt](evidence/02-write-while-node3-down.txt) |
| 存活节点上出现指向 node3 的 pending xattr | [evidence/03-pending-xattrs-on-survivors.txt](evidence/03-pending-xattrs-on-survivors.txt) |
| `heal info` 精确列出 3 个受影响路径 | [evidence/04-heal-info-while-down.txt](evidence/04-heal-info-while-down.txt) |
| 恢复 node3 容器 | [evidence/05-restart-node3.txt](evidence/05-restart-node3.txt) |
| node3 恢复后数据仍是旧的（对自己落后毫无感知） | [evidence/06-node3-stale-before-heal.txt](evidence/06-node3-stale-before-heal.txt) |
| 手动 force start brick 及后续状态排查 | [evidence/07-force-start-brick.txt](evidence/07-force-start-brick.txt) |
| 触发 heal 全过程（含一次因视图滞后失败的尝试） | [evidence/08-trigger-heal.txt](evidence/08-trigger-heal.txt) |
| heal 后 node3 数据完全追平 | [evidence/09-verify-after-heal.txt](evidence/09-verify-after-heal.txt) |
| heal 后 pending xattr 清零 | [evidence/10-xattrs-after-heal.txt](evidence/10-xattrs-after-heal.txt) |

---

上一课：[Lesson 7 - Replica](../lesson07-replica/README.md)
下一课：[Lesson 9 - Gluster RPC](../lesson09-gluster-rpc/README.md)
