# Lesson 1 - 认识 Peer

## 一、本实验目的

证明：**GlusterFS 没有中心化的"管理节点"/"Master"，Peer Pool 本质是一份被复制到每个节点本地磁盘上的、对称的成员关系表。**

具体要证明三件事：
1. 只在一个节点上执行 `peer probe`，关系会自动传播到集群里所有其他节点（不需要在每个节点上都手动 probe 一遍）。
2. 每个节点看到的 pool 视图是完全对称的——没有哪个节点比别的节点知道更多或更少。
3. 这份关系是持久化在本地磁盘上的，重启 `glusterd` 之后不需要重新联系"中心"就能恢复。

## 二、背景知识

只讲这一课需要的最小知识集：

- 每个跑 GlusterFS 的节点上都有一个管理进程 `glusterd`，负责节点间的集群管理通信（组建 pool、创建 volume、下发配置），它 **不经手用户的文件数据**（数据面是另一套机制，留到 Lesson 4/9）。
- `glusterd` 有自己的命令行客户端 `gluster`，通过本地 socket 找本机的 `glusterd`（`gluster` 命令必须在装了 `glusterd` 的机器上跑，且 `glusterd` 得是运行状态）。
- `gluster peer probe <hostname>` 是"邀请另一个节点加入我的信任池（Trusted Storage Pool）"的动作。
- 本课不涉及 Brick / Volume / Translator，那些是后面几课的内容。

## 三、提出问题

在动手之前，先想一想：

1. 如果我只在 node1 上执行 `gluster peer probe node2` 和 `gluster peer probe node3`，node2 会不会知道 node3 的存在？为什么？
2. `gluster peer status` 这条命令，它查询的是谁？是查了一个"中心配置服务"，还是查了本机的什么东西？
3. 如果把 node1 的容器重启（相当于重启了它的 `glusterd`），node1 还认得 node2、node3 吗？它是怎么"记起来"的？
4. 三个节点里，有没有哪一个必须"一直在线"，否则集群就散了？

先不要往下看，带着猜想去做实验。

## 四、实验拓扑

```
                        (探测前，三个节点互不相识)

        node1                  node2                  node3
     172.28.0.11            172.28.0.12            172.28.0.13
   glusterd 独立运行      glusterd 独立运行      glusterd 独立运行
   peers/ 目录为空        peers/ 目录为空        peers/ 目录为空
```

实验只会在 **node1** 上执行命令，探测 node2 和 node3；node2、node3 全程不会被直接操作。这是故意的：如果最后 node2 和 node3 也互相认得对方，就证明了传播是集群自己完成的，不是我在"人工同步"。

## 五、实验步骤

### Step 1：确认探测前三个节点都是一张空白状态

为什么先看这个？因为要证明"关系是探测之后才出现的"，就必须先有一个"之前什么都没有"的基准对照。

```bash
docker exec node1 gluster peer status
docker exec node2 gluster peer status
docker exec node3 gluster peer status

docker exec node1 ls -la /var/lib/glusterd/peers/
docker exec node2 ls -la /var/lib/glusterd/peers/
docker exec node3 ls -la /var/lib/glusterd/peers/
```

观察：`Number of Peers` 是不是都是 0；`peers/` 目录是不是都是空的。

### Step 2：只在 node1 上探测 node2 和 node3

为什么只在 node1 上做？因为要验证"探测是否会双向传播、传播到第三方"。

```bash
docker exec node1 gluster peer probe node2
docker exec node1 gluster peer probe node3
```

执行完之后，**不要碰 node2、node3**，直接去观察三个节点各自的状态（见下一步）。

### Step 3：三个节点分别观察

```bash
docker exec node1 gluster peer status
docker exec node2 gluster peer status
docker exec node3 gluster peer status

docker exec node1 gluster pool list
docker exec node2 gluster pool list
docker exec node3 gluster pool list
```

### Step 4：绕开 `gluster` 命令，直接看磁盘上的原始文件

这是本课最关键的一步——`gluster peer status` 只是命令行工具，它背后一定是读了什么东西。去看它读的是什么。

```bash
docker exec node1 cat /var/lib/glusterd/glusterd.info
docker exec node1 ls -la /var/lib/glusterd/peers/
docker exec node1 cat /var/lib/glusterd/peers/<某个UUID文件>
```

对三个节点都做一遍。

### Step 5：持久化验证——重启 node1 的 glusterd

```bash
docker restart node1
sleep 3
docker exec node1 gluster peer status
docker exec node1 ls -la /var/lib/glusterd/peers/
```

## 六、实验现象

### 现象 1：探测前，三个节点都是孤岛

```
--- node1 ---
Number of Peers: 0
--- node2 ---
Number of Peers: 0
--- node3 ---
Number of Peers: 0
```

`peers/` 目录三个节点都是空的（只有 `.` 和 `..`）。

### 现象 2：只探测了 node2/node3，但 node2 和 node3 互相也认识了

只在 node1 上执行了两条 `peer probe` 命令后，三个节点的 `peer status` 全部变成 "Number of Peers: 2"：

```
--- node1 ---
Number of Peers: 2
Hostname: node2   ...  State: Peer in Cluster (Connected)
Hostname: node3   ...  State: Peer in Cluster (Connected)

--- node2 ---
Number of Peers: 2
Hostname: node1.glusterlab-net   ...  State: Peer in Cluster (Connected)
Hostname: node3                  ...  State: Peer in Cluster (Connected)

--- node3 ---
Number of Peers: 2
Hostname: node1.glusterlab-net   ...  State: Peer in Cluster (Connected)
Hostname: node2                  ...  State: Peer in Cluster (Connected)
```

**node2 从没被我直接命令去认识 node3，但它认识了**——这是"传播"发生的直接证据。（完整输出见 [evidence/07-peer-status-after.txt](evidence/07-peer-status-after.txt)、[evidence/08-pool-list-after.txt](evidence/08-pool-list-after.txt)）

一个容易被忽略但很有意思的细节：node1 在自己的视角里把 node2/node3 记成了短域名 `node2`/`node3`（因为我是这么敲的命令），但 node2、node3 反过来记 node1 时用的是 `node1.glusterlab-net`（Docker 内嵌 DNS 返回的 FQDN）。**同一个物理关系，在不同节点上存的字符串可能不完全一样，但语义等价**——这提示 peer 关系的传播不是"复制一份原始命令"，而是"各自记录连接时获得的信息"。

### 现象 3：磁盘上的原始证据——peers/ 目录里全是 UUID 文件

`glusterd.info`（每个节点的"身份证"）：

```
--- node1 --- UUID=b64688a7-838c-41a3-bdd0-5d5f62780af4
--- node2 --- UUID=468ef93d-2fc3-4226-a10c-26b4f12c0648
--- node3 --- UUID=09483fe3-0d93-4205-9659-6d2a0b8e30bd
```

node1 的 `peers/` 目录：

```
-rw------- 1 root root 66 peers/09483fe3-0d93-4205-9659-6d2a0b8e30bd   # 这是 node3 的 UUID
-rw------- 1 root root 66 peers/468ef93d-2fc3-4226-a10c-26b4f12c0648   # 这是 node2 的 UUID

$ cat peers/468ef93d-2fc3-4226-a10c-26b4f12c0648
uuid=468ef93d-2fc3-4226-a10c-26b4f12c0648
state=3
hostname1=node2
```

**关键点**：node1 的 `peers/` 目录里，只有"别人"的文件（node2、node3 各一个），**没有自己的**。自己的身份记在 `glusterd.info` 里，别人的身份记在 `peers/<对方UUID>` 里。三个节点都是这个模式——完全对称，没有任何一个目录结构和别的不同。（完整输出见 [evidence/06-peers-dir-after.txt](evidence/06-peers-dir-after.txt)）

### 现象 4：重启之后，关系还在——不用重新 probe

```
$ docker restart node1
$ docker exec node1 gluster peer status
Number of Peers: 2
Hostname: node3   State: Peer in Cluster (Connected)
Hostname: node2   State: Peer in Cluster (Connected)
```

`peers/` 目录里的文件原样还在（还多出一个 `.tmp` 临时文件，是写入时"先写临时文件再原子改名"策略留下的痕迹，属于正常现象）。（完整输出见 [evidence/09-restart-persistence.txt](evidence/09-restart-persistence.txt)）

## 七、原理分析

```
gluster peer probe node2   (在 node1 上执行)
        |
        v
node1:glusterd  --RPC(管理面, TCP 24007)-->  node2:glusterd
        |                                         |
        | 1. 双方交换 UUID / hostname               |
        | 2. 各自在本地 peers/ 目录写一个文件         |
        |                                         |
        +-----------------<相互确认>----------------+
        |
        v
node1 再把这个新成员的信息，通过同样的管理面 RPC
广播给它已经认识的其他 peer（这里是 node3）
        |
        v
node3:glusterd 也和 node2:glusterd 建立管理面连接，
双方交换信息，各自落盘
```

设计的本质：**Peer Pool 不是"查询一个中心表"，而是"每个节点在本地维护一份自己认为对的成员表，节点之间通过管理面 RPC 互相同步这份表"**。`gluster peer status` 命令看起来像在查一个服务，实际上只是读了本机磁盘上的几个文件再格式化打印——这就是为什么它能在网络完全隔离的情况下依然给出（可能过期的）结果。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：搞一个独立的 Metadata/Membership Server（类似 ZooKeeper/etcd）来管理集群成员。**

- 好处：强一致性，任何时刻只有一个"真相来源"。
- 代价：引入了一个新的、独立于存储本身的关键组件。这个组件本身需要部署、需要高可用（否则它就是新的单点故障），需要运维团队多学一套系统。而 GlusterFS 的定位是"用普通目录 + 普通网络就能把闲置磁盘聚合成存储"，追求的是部署简单、依赖少。引入 etcd 类组件违背了这个初衷。

**备选方案 B：Master-Slave，选一个节点做"管理主节点"。**

- 好处：管理逻辑简单，任何变更都从主节点发起，天然有一个统一顺序。
- 代价：主节点是单点。主节点挂了，虽然已经在运行的 I/O 不受影响（因为管理面和数据面分离，见 Lesson 9），但集群管理操作（加节点、建 volume）会中断，直到选出新主。对于一个以"高可用存储"为卖点的系统，管理面自己先有单点这件事说不过去。

**GlusterFS 选择的方案：对称 Gossip 式复制。**

- 每个节点都是对等的（peer 这个词本身就是"对等方"的意思），谁都可以发起管理操作，谁都保存一份完整视图。
- 代价是"最终一致性"：如果网络分区，不同节点看到的 peer 视图可能短暂不一致（这也是为什么后面会有 Self Heal、Quorum 机制去处理"分裂之后怎么收敛"，但那是后面课的内容）。
- 对 GlusterFS 的场景来说，这个代价可以接受：集群成员变化是低频操作（加节点、删节点），不像文件读写那样要求强实时一致性。

一句话总结设计哲学：**GlusterFS 把"强一致性"这个昂贵的需求，只用在真正需要它的地方（少数管理操作），而不是为整个系统引入一个通用的强一致存储组件。**

## 九、源码位置（仅指路）

- `glusterd/src/` —— `glusterd` 管理面守护进程的实现，peer probe / handshake 的逻辑在这里。
- `libglusterfs/src/` —— 通用数据结构和工具函数（UUID 生成等）。
- 后面会用到的 `rpc/`、`xlators/` 暂时不用看。

## 十、本实验总结

1. Peer Pool 里没有主节点，任何节点发起的 `peer probe` 都会被扩散给已知的所有其他节点。
2. `gluster peer status` / `pool list` 不是查询远程服务，而是读取本机 `/var/lib/glusterd/` 下的本地文件。
3. 每个节点的 `peers/` 目录只存"别人"的信息，自己的身份写在 `glusterd.info` 里。
4. Peer 关系持久化在磁盘上，`glusterd` 重启后直接从磁盘恢复，不需要重新联系任何人。
5. 这是"最终一致的对称复制"设计，用来避免引入中心化的、独立于存储本身的单点组件。

## 十一、思考题

1. 如果我手动删掉 node1 上 `/var/lib/glusterd/peers/` 里的某一个文件，然后重启 node1 的 `glusterd`，node1 会怎么看这个集群？三个节点的视图还会一致吗？
2. 如果三个节点之间发生网络分区（node1 单独在一边，node2/node3 在另一边），此时都执行 `gluster peer status`，会看到什么？分区恢复之后呢？
3. 现象 2 里提到 node1 用短域名记录、node2/node3 用 FQDN 记录对方，这种"同一份关系在不同节点上字符串不同"的情况，会不会给后面创建 Volume（需要指定 `hostname:/brick/path`）带来麻烦？
4. 如果集群有 100 个节点，新加入第 101 个节点时，是不是需要对已有的 100 个节点都手动执行一次 `peer probe`？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 探测前三节点互不相识 | [evidence/01-before-probe-peer-status.txt](evidence/01-before-probe-peer-status.txt)、[evidence/02-before-probe-peers-dir.txt](evidence/02-before-probe-peers-dir.txt) |
| 只探测 node2/node3，两者也互相认识（证明传播） | [evidence/04-probe-commands.txt](evidence/04-probe-commands.txt)、[evidence/07-peer-status-after.txt](evidence/07-peer-status-after.txt)、[evidence/08-pool-list-after.txt](evidence/08-pool-list-after.txt) |
| 每个节点的身份 UUID 各不相同、独立生成 | [evidence/05-uuids-after.txt](evidence/05-uuids-after.txt) |
| Peer 关系落盘在 `peers/` 目录，三节点结构对称 | [evidence/06-peers-dir-after.txt](evidence/06-peers-dir-after.txt) |
| 重启 glusterd 后关系从磁盘恢复，无需重新 probe | [evidence/09-restart-persistence.txt](evidence/09-restart-persistence.txt) |

---

下一课：[Lesson 2 - Brick](../lesson02-brick/README.md)
