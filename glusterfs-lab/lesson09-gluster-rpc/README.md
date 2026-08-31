# Lesson 9 - Gluster RPC

## 一、本实验目的

证明：**GlusterFS 把"管理面"和"数据面"实现成两套完全独立的 RPC 通道——管理面固定监听 24007 端口，负责集群管理、配置分发、端口查询；数据面则是每个 Brick 进程各自监听一个动态端口，一旦 client 和 Brick 建立了数据面连接，这条连接完全不依赖管理面继续存活。**

回到课程开头的问题："Client 到底和谁通信？"——这一课给出最终、最精确的答案：**client 和"管理面"、"数据面"分别通信，而且这两者可以独立失效而互不影响。**

## 二、背景知识

- Lesson 4 已经看到挂载过程"先问 24007 拿配置，再问 24007 问端口，最后直连 Brick 端口"这个顺序；Lesson 3 也看到了 volfile 里 `option remote-host`（数据面要连的目标）和 `info` 文件里那组 `username`/`password`（内部认证凭证）。
- 本课要做的，是把"管理面"和"数据面"这两个概念，用一次可控的网络故障实验，做一次干净的物理切割验证：**只掐断管理面，看数据面是否真的完全不受影响**。
- Gluster 的 RPC 基于 ONC RPC（Sun RPC，和早期 NFS 同源的协议家族）做了扩展，用标准的 **RPC 记录标记（Record Marking）** 做 TCP 上的消息分帧——这个细节会在下面的抓包/strace 分析里直接验证。

## 三、提出问题

1. 如果只是 24007 端口（管理面）失效，而 Brick 进程本身好好地在运行，正在进行中的文件读写会不会受影响？
2. 如果反过来——只有某个 Brick 进程挂了，但 24007（管理面）还活着——`gluster volume status` 之类的管理命令还能正常工作吗？（这个在 Lesson 8 恢复 node3 时其实已经间接验证过一部分）
3. Client 和 Brick 之间的数据面连接，第一次建立的时候需要 24007 参与吗？建立之后呢？
4. Gluster 有没有必要为管理面和数据面设计两套完全不同的协议？还是说它们本质上是同一套 RPC 机制，只是"话题"不同？

## 四、实验拓扑

```
                        client (172.28.0.20)
                     已经挂载 gv-rep，三条数据面连接都已建立
              +----------------+----------------+
              |                |                |
      (数据面,已连接)    (数据面,已连接)     (数据面,已连接)
              |                |                |
            node1            node2             node3
        24007(管理面)    24007(管理面)      24007(管理面)
        52052(brick)     58508(brick)      49933(brick)
                              ^
                              |
                    本课实验: 只用 iptables 屏蔽
                    node2 的 24007 入站流量
                    (58508 完全不动)
```

## 五、实验步骤

### Step 1：回顾端口划分基线

```bash
docker exec node1 gluster volume status gv-rep
docker exec node2 ss -tlnp | grep 24007
```

### Step 2：只屏蔽 node2 的管理面端口（24007），不动数据面端口

为什么做：这是本课最核心的操控变量——精确制造"管理面失效，数据面健康"这个单一条件的故障，而不是像 Lesson 4/8 那样把整个节点都停掉（那样管理面和数据面会同时失效，没法区分到底是谁在起作用）。

```bash
docker exec node2 iptables -A INPUT -p tcp --dport 24007 -j DROP
```

### Step 3：验证数据面（已建立的连接）是否还能正常工作

```bash
docker exec client bash -c "echo 'written while node2 mgmt port blocked' > /mnt/gluster-rep/rpc-test-during-block.txt"
docker exec node2 cat /data/brick2/rpc-test-during-block.txt
```

### Step 4：验证管理面操作是否真的受影响

```bash
timeout 15 docker exec node1 gluster volume status gv-rep
timeout 15 docker exec client mount -t glusterfs node2:/gv-rep /mnt/gluster-rep-2
```

### Step 5：解除屏蔽，确认一切恢复

```bash
docker exec node2 iptables -D INPUT -p tcp --dport 24007 -j DROP
```

## 六、实验现象

### 现象 1：屏蔽 node2 的 24007 后，client 依然能正常写入 node2 的 Brick

```
$ docker exec client bash -c "echo '...' > /mnt/gluster-rep/rpc-test-during-block.txt"
$ docker exec node2 cat /data/brick2/rpc-test-during-block.txt
written while node2 mgmt port blocked
$ docker exec node1 cat ... / docker exec node3 cat ...   # 三份内容都一致
```

（见 [evidence/01-block-management-port.txt](evidence/01-block-management-port.txt)、[evidence/02-data-plane-still-works.txt](evidence/02-data-plane-still-works.txt)）**node2 的管理面被彻底屏蔽的情况下，AFR 依然成功把这次写操作复制到了 node2 的 Brick 上**——这是"数据面独立于管理面"最直接、最有力的证据：client 早在挂载时就已经和 node2 的 Brick 进程（监听在 58508）建立了独立的 TCP 连接，这条连接的存续和 24007 端口毫无关系。

### 现象 2：与此同时，跨节点的管理操作却被实实在在地卡住了

```
$ timeout 15 docker exec node1 gluster volume status gv-rep
退出码: 124   (被 timeout 强制终止，说明它在 15 秒内没有正常返回)

$ timeout 15 docker exec client mount -t glusterfs node2:/gv-rep /mnt/gluster-rep-2
退出码: 124   (新挂载同样卡死)
```

（见 [evidence/03-cross-node-mgmt-blocked.txt](evidence/03-cross-node-mgmt-blocked.txt)、[evidence/04-new-mount-via-blocked-node.txt](evidence/04-new-mount-via-blocked-node.txt)）`node1` 想要拿到 `gv-rep` 完整状态，必须问过每一个相关节点的 `glusterd`（因为 Lesson 1 就已经确认没有一个"中心管理节点"能替 node2 回答"你的 Brick 现在健康吗"这种只有 node2 自己知道的信息）；同理，用 node2 做新挂载的入口，第一步 GETSPEC 就得连 node2 的 24007，自然也卡死。**这组对照精确地把"管理面"和"数据面"的边界钉死在了 24007 这一个端口上。**

### 现象 3：解除屏蔽后，一切立刻恢复

```
$ docker exec node2 iptables -D INPUT -p tcp --dport 24007 -j DROP
$ docker exec node1 gluster volume status gv-rep   # 三个 Brick + 三个 Self-heal Daemon 全部正常列出
$ docker exec client mount -t glusterfs node2:/gv-rep /mnt/gluster-rep-2   # 秒级完成
```

（见 [evidence/05-unblock-and-verify.txt](evidence/05-unblock-and-verify.txt)）证明故障是精确可控、可逆的，不是"把 node2 弄坏了"，只是单纯掐断了一个端口。

### 现象 4：回看历史证据——RPC 消息本身也验证了"记录标记"协议细节

回顾 Lesson 4 抓包、Lesson 6/7 strace 里反复出现的这种字节模式：

```
writev(12<TCP:...>, [{iov_base="\200\0\2p", iov_len=4}, ...
```

`\200\0\2p` 就是 `0x80 00 02 70`：最高位 `1`（`0x80` 的最高 bit）是 RPC **记录标记（Record Marking）** 里的"这是最后一个分片"标志位，其余 31 位（`0x000270` = 624）是这个 RPC 消息的字节长度。这正是标准 ONC RPC over TCP 的分帧方式——**Gluster 的管理面和数据面走的虽然是两条独立的连接、传输两类完全不同的"业务内容"（一个传集群配置，一个传文件数据），但它们复用的是同一套底层 RPC 编解码机制**。这解释了为什么 Gluster 的源码里管理面（`glusterd`）和数据面（`protocol/client`、`protocol/server`）能共享同一个 `rpc/rpc-lib/` 基础设施。

## 七、原理分析

```
                        node2 上的两个独立监听端口
        +--------------------------+  +--------------------------+
        |   glusterd (PID 1)       |  |   glusterfsd (Brick 进程)  |
        |   监听 24007              |  |   监听 58508 (启动时动态分配) |
        |                          |  |                          |
        |   职责: 集群成员管理、       |  |   职责: 真正的文件读写、       |
        |   Volume 配置分发、         |  |   AFR/DHT 转发过来的具体     |
        |   GETSPEC、端口查询(portmap) |  |   POSIX 操作                |
        +--------------------------+  +--------------------------+
              两者是完全独立的进程，独立的监听 socket，
              一个挂了不会拖累另一个 (除非整个节点/容器都没了)
```

**这个"控制面 / 数据面分离"是分布式系统里一个反复出现的经典模式**（在网络交换机、SDN 控制器、Kubernetes 的 apiserver vs kubelet 之间都能看到同样的思路）：**决策/配置类操作（低频、可以容忍稍高延迟、需要强一致）** 和 **实际数据传输（高频、要求低延迟高吞吐、能接受最终一致）** 有完全不同的非功能性需求，硬塞进同一条通道、同一个进程里，会让两者的设计互相妥协。分开之后，数据面可以做得极度精简（`glusterfsd` 只需要理解 POSIX 操作的 RPC 编码），管理面则可以承载更复杂的协商逻辑（Peer 握手、配置版本比对、CLI 命令），互不干扰。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：所有通信（管理 + 数据）都走同一个端口、同一个进程。**

- 好处：部署更简单，只需要开放一个端口，防火墙规则更少。
- 代价：这个进程一旦因为处理某个管理请求卡住（比如集群规模很大，某次 Peer 状态同步耗时很长），会连带影响正在传输的文件数据——把两类完全不同 SLA 要求的流量塞进一个队列，天然会有相互排队、相互拖累的风险。

**备选方案 B：数据面也做成中心化转发（所有 I/O 先经过管理进程 `glusterd` 再转发到 Brick）。**

- 好处：可以在转发这一层统一做限流、审计、权限控制。
- 代价：`glusterd` 就变成了所有数据流量的必经之路，直接违背了 Lesson 4 已经验证的"数据面完全去中心化、多 Brick 吞吐可以叠加"的设计目标——这会让 `glusterd` 变成新的性能瓶颈和单点故障。

**GlusterFS 选择的方案：管理面（`glusterd`，固定端口 24007）和数据面（`glusterfsd`，每个 Brick 独立进程 + 动态端口）彻底分离，共享同一套底层 RPC 框架但跑在不同进程、不同端口上。**

- 好处：现象 1-3 已经完整验证——两者的可用性互相独立，运维上可以针对两类流量制定不同的网络策略（比如给管理面单独设置更严格的防火墙规则、给数据面开放更大的带宽配额）。
- 好处 2：复用同一套 RPC 基础设施（记录标记分帧、认证机制）意味着不需要为两类通信各写一套网络代码，降低了工程实现复杂度，这是"共享基础设施、分离业务逻辑"的又一次体现（和 Lesson 6 的 Translator 设计哲学一脉相承）。
- 代价：运维时需要理解两套端口体系（一个固定、一堆动态），排障时要分清楚"是管理面问题还是数据面问题"——这正是本课通过实验想训练的判断力。

一句话总结设计哲学：**低频强一致的控制逻辑和高频高吞吐的数据传输，永远不要塞进同一条通道；用独立的进程和端口物理隔离两者，同时通过共享的底层协议基础设施避免重复造轮子。**

## 九、源码位置（仅指路）

- `rpc/rpc-lib/src/rpc-transport.c`、`rpc/rpc-lib/src/rpcsvc.c` —— 通用 RPC 传输层，记录标记分帧、认证等逻辑在这里，管理面和数据面共用这套代码。
- `glusterd/src/glusterd-pmap.c` —— glusterd 里的端口映射（portmap）服务，就是 Lesson 4 里 client "问门牌号"这一步在服务端的实现。
- `xlators/protocol/client/src/`、`xlators/protocol/server/src/` —— 数据面（client-brick 之间）的 RPC 实现。

## 十、本实验总结

1. Gluster 有两套完全独立的 RPC 通道：管理面固定在 24007，数据面每个 Brick 各自一个动态端口。
2. 屏蔽管理面端口，已经建立的数据面连接完全不受影响，client 依然能正常读写数据。
3. 屏蔽管理面端口，任何需要联系该节点 `glusterd` 的操作（跨节点状态查询、以该节点为入口的新挂载）都会失败或挂起。
4. 两个平面虽然独立，但共享同一套底层 RPC 编解码机制（ONC RPC 记录标记），这一点可以直接从抓包/strace 里的字节模式验证。
5. "控制面/数据面分离"不是 Gluster 独创，而是分布式系统里应对"不同 SLA 需求不能混用同一通道"这一共性问题的经典解法。

## 十一、思考题

1. 如果反过来，只屏蔽某个 Brick 的数据面端口（比如 58508），但保留 24007，`gluster volume status` 会显示什么状态？client 已经建立的连接会怎样？这和本课的实验互为镜像，你能预测结果吗？
2. 数据面端口是"动态分配"的（每次 Brick 进程重启，端口可能变化），如果防火墙策略是提前配置好的静态规则（比如只开放 49152-49251 这个范围），Gluster 的这个设计会带来什么运维上的额外工作？
3. 管理面用的是固定端口 24007，这是不是意味着管理面本身也是一个"单点"？结合 Lesson 1 的结论（每个节点的 `glusterd` 都是对等的），"单点"这个词用在这里准确吗？
4. 如果要给 Gluster 的管理面和数据面加密（TLS），是不是需要分别为两套 RPC 通道单独配置证书？这和很多系统"网络层加密一次搞定所有流量"的做法有什么不同的权衡？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 端口划分基线：24007 管理面 + 各 Brick 独立数据面端口 | [evidence/00-port-baseline.txt](evidence/00-port-baseline.txt) |
| 用 iptables 精确屏蔽 node2 的 24007，不影响其他端口 | [evidence/01-block-management-port.txt](evidence/01-block-management-port.txt) |
| 管理面被屏蔽期间，数据面写入 node2 Brick 依然成功 | [evidence/02-data-plane-still-works.txt](evidence/02-data-plane-still-works.txt) |
| 跨节点管理查询（`volume status`）超时卡死 | [evidence/03-cross-node-mgmt-blocked.txt](evidence/03-cross-node-mgmt-blocked.txt) |
| 以被屏蔽节点为入口的新挂载同样超时卡死 | [evidence/04-new-mount-via-blocked-node.txt](evidence/04-new-mount-via-blocked-node.txt) |
| 解除屏蔽后一切立即恢复正常 | [evidence/05-unblock-and-verify.txt](evidence/05-unblock-and-verify.txt) |
| RPC 记录标记字节模式（引用 Lesson 4/6/7 已采集的抓包/strace） | [../lesson04-client-mount/evidence/02-mount.pcap](../lesson04-client-mount/evidence/02-mount.pcap)、[../lesson07-replica/evidence/06-write-fanout-strace.txt](../lesson07-replica/evidence/06-write-fanout-strace.txt) |

---

上一课：[Lesson 8 - Self Heal](../lesson08-self-heal/README.md)
下一课：[Lesson 10 - write() 完整生命周期](../lesson10-write-lifecycle/README.md)
