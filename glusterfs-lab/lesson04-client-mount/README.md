# Lesson 4 - Client Mount

## 一、本实验目的

证明：**Gluster Client 不需要一个中心化的 Metadata Server 来告诉它"文件在哪个节点"。它只需要在挂载那一刻，向"随便一个"存活节点问一次完整的配置图（volfile），之后就凭这份本地配置图自己找到所有 Brick，和它们建立直接连接。**

## 二、背景知识

- `mount -t glusterfs <host>:<volume> <mountpoint>` 是标准的 FUSE 挂载方式：内核把 `/mnt/gluster` 上的所有文件操作转发给用户态的 `glusterfs` 进程（这就是 Lesson 6 会讲的 Translator 调用栈的起点）。
- 这里的 `<host>` 只是"挂载时用来问路的联系人"，不代表这个节点有任何特殊地位——本课要验证的正是这一点。
- Lesson 3 已经看过 client volfile 的内容：3 个 `protocol/client` 子卷，各自 `remote-host` 指向 node1/node2/node3。本课要用抓包实际验证"这份配置是怎么到达 client 手里的"，以及"client 拿到之后做了什么"。

## 三、提出问题

1. `mount -t glusterfs node1:/gv-dist /mnt/gluster` 命令里只出现了 `node1` 这一个地址，client 怎么会知道 node2、node3 的存在？
2. 如果挂载成功之后，把 `node1` 关掉，client 还能不能继续读写数据？
3. 如果 `node1` 一开始就是关着的，还能不能用 `node1:/gv-dist` 挂载成功？这个答案和第 2 问是否矛盾？如果不矛盾，说明了什么？
4. `df -h` 看到的 Volume 总容量，会是 1 个 Brick 的容量，还是 3 个 Brick 加起来？这能说明什么？

## 四、实验拓扑

```
                         client (172.28.0.20)
                       只知道 node1 这一个地址
                                |
                       mount -t glusterfs node1:/gv-dist
                                |
        +-----------------------+-----------------------+
        |                       |                       |
      node1                   node2                   node3
   172.28.0.11             172.28.0.12             172.28.0.13
  glusterd:24007          glusterd:24007          glusterd:24007
  glusterfsd(brick)       glusterfsd(brick)       glusterfsd(brick)
```

新增了一个专门的 `client` 容器，和 node1/2/3 完全对等地挂在同一个网络下，只是它没有加入 Gluster 的 Peer Pool（不跑 `gluster peer probe`），单纯扮演"使用者"的角色。

## 五、实验步骤

### Step 1：挂载前确认 client 和任何节点都没有连接

```bash
docker exec client ss -tnp
```

### Step 2：一边抓包，一边挂载

为什么做：`mount` 这个动作背后一定有网络交互，抓包能看到 client 具体和谁、在哪个端口上说了什么话。

```bash
docker exec -d client tcpdump -i eth0 -w /tmp/mount.pcap \
  'host 172.28.0.11 or host 172.28.0.12 or host 172.28.0.13'
docker exec client mount -t glusterfs node1:/gv-dist /mnt/gluster
```

### Step 3：分析抓包结果

```bash
docker cp client:/tmp/mount.pcap ./mount.pcap
tcpdump -r mount.pcap -nn 'tcp[tcpflags] & tcp-syn != 0 and tcp[tcpflags] & tcp-ack == 0'
```

只看 SYN 包（即"发起了哪些新连接"），不看后续数据。

### Step 4：写文件，看它落在哪个 Brick

```bash
docker exec client bash -c "echo 'written via fuse client' > /mnt/gluster/testfile.txt"
docker exec node1 find /data/brick1 -name testfile.txt
docker exec node2 find /data/brick1 -name testfile.txt
docker exec node3 find /data/brick1 -name testfile.txt
```

### Step 5：关掉挂载时联系的那个节点（node1），测试已挂载的 client 是否还能工作

```bash
docker stop node1
docker exec client bash -c "echo 'written after node1 is down' > /mnt/gluster/after-node1-down.txt"
docker exec client ls -la /mnt/gluster/
```

### Step 6：卸载后，尝试用一个"已经死了"的节点做新挂载的入口

```bash
docker exec client umount /mnt/gluster
docker exec client mount -t glusterfs node1:/gv-dist /mnt/gluster   # 预期失败
docker exec client mount -t glusterfs node2:/gv-dist /mnt/gluster   # 换一个活着的节点
```

## 六、实验现象

### 现象 1：`df -h` 显示的容量是三个 Brick 之和

```
Filesystem      Size  Used Avail Use% Mounted on
node1:/gv-dist  3.0T  198G  2.7T   7% /mnt/gluster
```

只连了 `node1` 一个地址，但看到的容量是三台机器容量之和——**这本身就是"client 知道全部 3 个 Brick"的间接证据**。

### 现象 2：抓包看到的连接顺序，完整还原了"client 怎么知道其他节点"的过程

```
1) client(49151) -> node1:24007   SYN   <- 第一次接触，问 node1 的 glusterd 要配置
2) client(49150) -> node1:24007   SYN   <- (GETSPEC 之外的管理面握手)
3) client(49149) -> node2:24007   SYN   <- 拿到 volfile 后，发现里面提到 node2，主动去问 node2 的 glusterd
4) client(49148) -> node1:53176   SYN   <- 从 node1:24007 那问到了 brick 真实端口，直连
5) client(49147) -> node3:24007   SYN   <- 同理去问 node3 的 glusterd
6) client(49146) -> node2:60832   SYN   <- 从 node2:24007 问到端口后，直连 node2 的 brick
7) client(49145) -> node3:60791   SYN   <- 同理直连 node3 的 brick
```

（完整原始抓包见 [evidence/02-mount.pcap](evidence/02-mount.pcap)，可用 Wireshark 打开；文字摘要见 [evidence/03-pcap-summary.txt](evidence/03-pcap-summary.txt)）

这 7 个连接精确对应了 Lesson 3 看到的 client volfile 里的 3 个 `protocol/client` 子卷，以及 `gluster volume status` 里看到的 3 个 Brick 端口（`53176`/`60832`/`60791`，和 Lesson 3 记录的完全一致）。**client 只在命令行里被告知了 node1 一个地址，剩下的全部信息，是它自己在 24007 端口（`glusterd` 的管理面端口）问出来的。**

### 现象 3：文件精确落在某一个 Brick 上（这里是 node3），不是分散存了三份

```
$ docker exec node3 find /data/brick1 -name testfile.txt -exec ls -la {} \;
-rw-r--r-- 2 root root 24 Aug 29 05:28 /data/brick1/testfile.txt
```

（node1、node2 上都没有这个文件——完整输出见 [evidence/05-where-did-it-land.txt](evidence/05-where-did-it-land.txt)）这就是 DHT 在起作用（下一课细讲），这里先记住一个事实：**Distribute 类型的 Volume，一个文件只会落在一个 Brick 上**。

顺带一提，`ls -la` 显示这个文件的硬链接数是 `2`——这不是巧合，Gluster 在 `.glusterfs/` 目录下还维护了一份用 GFID 命名的硬链接，方便通过 GFID 反查路径。这个细节先记住，Lesson 5/6 会用到。

### 现象 4：关掉 node1 之后，已挂载的 client 完全不受影响，可以继续写文件

```
--- 在已挂载的 client 上写新文件 ---
written after node1 is down
--- ls 挂载目录 ---
after-node1-down.txt
hello.txt      <- 出现了两次！见下面的"意外发现"
hello.txt
testfile.txt
```

（完整输出见 [evidence/06-kill-node1-after-mount.txt](evidence/06-kill-node1-after-mount.txt)）这直接证明：**client 一旦挂载完成，就不再依赖"当初问路的那个节点"继续存活。**

**一个意外但很有价值的发现**：`hello.txt` 出现了两次。原因是 Lesson 2 里我们绕开 Gluster、直接在三个节点的 `/data/brick1` 里各自 `touch` 了一个同名的 `hello.txt`（那时候还没建 Volume）。DHT 假设"一个文件名只会存在于一个 Brick 上"，所以做目录聚合（`readdir`）时**不会做重名检测和去重**——它天真地把每个子卷返回的条目直接拼在一起。这提醒我们：**绕开 Gluster 直接操作 Brick 目录，会破坏 Gluster 自己维护的不变量**，这正好回答了 Lesson 2 思考题里提出的问题。

### 现象 5：用已宕机的 node1 做新挂载的入口，直接失败

```
$ docker exec client mount -t glusterfs node1:/gv-dist /mnt/gluster
Mounting glusterfs on /mnt/gluster failed.
退出码: 1
```

（见 [evidence/07-new-mount-via-dead-node1.txt](evidence/07-new-mount-via-dead-node1.txt)）**这和现象 4 并不矛盾**：现象 4 证明的是"已经建立的连接不依赖问路节点"，现象 5 证明的是"问路本身需要一个能连上的联系人"——两者的边界正是"挂载那一刻"。

### 现象 6：换用存活的 node2 做入口，挂载能成功，但紧接着的 `ls` 明显变慢

```
$ docker exec client mount -t glusterfs node2:/gv-dist /mnt/gluster
退出码: 0
$ docker exec client ls -la /mnt/gluster/     # 这一步卡了大约 3 分钟才返回
```

（见 [evidence/08-new-mount-via-node2.txt](evidence/08-new-mount-via-node2.txt)、[evidence/09-note-ls-hang-explains-availability.txt](evidence/09-note-ls-hang-explains-availability.txt)）原因：`mount` 命令本身只是"拿到 volfile"，很快就返回；但这份 volfile 里依然写着 node1 是其中一个子卷，DHT 做目录聚合时要等所有子卷都应答（或超时放弃）才能返回完整结果，其中连 node1 的那个子卷一直在后台反复重连一个宕机的节点，直到放弃才让 `ls` 恢复。

**这是本课最重要的推论**：挂载入口（问路对象）可以是任意存活节点，是完全对等、可替换的；但 Volume 对每一个 Brick 的依赖是真实的、无法回避的——少一个 Brick，Distribute 类型的 Volume 就会有一部分数据/一部分操作明显变慢或不可用。这正是 Lesson 7（Replica）要解决的问题的动机。

## 七、原理分析

```
应用程序:  mount -t glusterfs node1:/gv-dist /mnt/gluster
                |
                v
     [1] 挂载协商 (glusterfs 进程 <--TCP:24007--> node1:glusterd)
         "GETSPEC" RPC: 我要挂 gv-dist，把配置图给我
                |
                v
     [2] node1:glusterd 把本地磁盘上 /var/lib/glusterd/vols/gv-dist/
         的 client volfile 原样返回
                |
                v
     [3] glusterfs 进程按 volfile 逐层构建 Translator 图
         (gv-dist-client-0/1/2 -> dht -> ... -> fuse)
                |
                v
     [4] 每个 protocol/client 子卷启动时,
         主动连它自己 remote-host 记录的那台机器的 24007
         问一句: "/data/brick1 现在监听在哪个端口?" (portmap 查询)
                |
                v
     [5] 拿到端口号后，直连该 Brick 的 glusterfsd 进程
         (这条连接才是以后真正的数据面通道)
                |
                v
     挂载完成，FUSE 内核模块开始把 /mnt/gluster 上的
     系统调用转发给这个 glusterfs 用户态进程
```

关键认知：**"问路"这个动作发生了不止一次，而是"先问一次拿到全局地图，再对地图上每个点分别问一次门牌号"**。第一次问路（拿 volfile）只需要联系任意一个节点；后面几次问门牌号，是针对每个 Brick 各自的 `glusterd` 分别问的——这就是为什么关掉 node1 之后，client 早就跟 node2、node3 建立的连接完全不受影响。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：volfile 里直接写死每个 Brick 的端口号，省去"问门牌号"这一步。**

- 好处：少一次网络往返，挂载更快。
- 代价：Brick 进程的端口不是固定分配的（`glusterfsd` 启动时由 `glusterd` 动态分配空闲端口，重启后可能变化），如果写死端口，Brick 进程一旦重启换了端口，所有已挂载的 client 就再也连不上它了。用"运行时问端口"换来了"端口可以自由变化"的灵活性。

**备选方案 B：Client 直接把所有请求都发给挂载时联系的那个节点，让那个节点做转发（类似反向代理）。**

- 好处：Client 逻辑更简单，只维护一条连接。
- 代价：这个"入口节点"就成了所有流量的中转站，变成新的性能瓶颈和单点——这正是 Gluster 想避免的"事后加中心节点"陷阱。

**GlusterFS 选择的方案：一次性拉取全局配置图，之后所有连接都由 client 自己直接发起、直接维护。**

- 好处：数据面完全去中心化——三个 Brick 的吞吐是叠加的，而不是被一个转发节点的带宽卡住。挂载点故障和数据读写完全解耦（现象 4/5 已验证）。
- 代价：这份配置图有"新鲜度"问题——如果集群拓扑变化（比如扩容），已挂载的旧 client 不会自动知道，需要 Gluster 的"轮询新 volfile 版本"机制介入（这也解释了为什么有时候扩容之后要求客户端重新挂载才能看到新 Brick）。

## 九、源码位置（仅指路）

- `xlators/mount/fuse/src/` —— FUSE 桥接层，内核请求进入 Gluster 用户态世界的第一站。
- `xlators/protocol/client/src/` —— 每个 Brick 对应的客户端连接逻辑，端口发现（portmap）逻辑在这里。
- `rpc/rpc-lib/src/` —— 通用 RPC 框架，24007 管理面和 Brick 数据面用的是同一套 RPC 机制的不同 program。

## 十、本实验总结

1. Client 挂载只需要知道"一个"存活节点的地址，这个节点只是"问路对象"，没有任何特殊地位。
2. Client 通过管理面端口（24007）先拿到完整的 volfile（配置图），再对图里的每个 Brick 分别问它的真实监听端口，最后直连数据面。
3. 挂载完成之后，client 与"问路节点"之间没有任何持续依赖——那个节点挂掉不影响已经建立的连接。
4. 但"问路节点挂了导致新挂载失败"和"某个 Brick 挂了导致该 Brick 相关的操作变慢/超时"是两个独立的问题，后者暴露了 Distribute 卷没有冗余的代价。
5. 绕开 Gluster 直接操作 Brick 后端目录会破坏 Gluster 自身维护的不变量（比如"文件名唯一分布在一个 Brick"这个假设）。

## 十一、思考题

1. 如果挂载命令写成 `mount -t glusterfs -o backup-volfile-servers=node2:node3 node1:/gv-dist /mnt/gluster`，现象 5 的结果会不会不同？为什么这能作为 Metadata Server 概念的"平替"？
2. 现象 6 里 `ls` 卡住了几分钟——如果这是一个 Replica 卷（每个文件在多个 Brick 都有副本），同样关掉一个节点，`ls` 还会卡这么久吗？（先猜，Lesson 7 会验证）
3. 为什么 `protocol/client` 问端口这一步不能也写进 volfile 里一次性问清楚，而要每个 Brick 单独问一次？
4. 如果这个 Volume 有 1000 个 Brick，client 挂载时是不是要发起 1000 次"问门牌号"的请求？这对挂载速度意味着什么？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 挂载后总容量是三个 Brick 之和 | 见现象1（[04-write-test-file.txt](evidence/04-write-test-file.txt) 前的 `df -h` 输出，也记录于 Lesson README 正文）|
| 挂载前无任何连接 | [evidence/01-before-mount-netstat.txt](evidence/01-before-mount-netstat.txt) |
| 抓包还原挂载全过程：先问 24007 拿配置，再逐个问端口，最后直连 Brick | [evidence/02-mount.pcap](evidence/02-mount.pcap)、[evidence/03-pcap-summary.txt](evidence/03-pcap-summary.txt) |
| 写入文件精确落在一个 Brick 上 | [evidence/05-where-did-it-land.txt](evidence/05-where-did-it-land.txt) |
| 关闭问路节点后，已挂载 client 不受影响 | [evidence/06-kill-node1-after-mount.txt](evidence/06-kill-node1-after-mount.txt) |
| 用宕机节点做新挂载入口会失败 | [evidence/07-new-mount-via-dead-node1.txt](evidence/07-new-mount-via-dead-node1.txt) |
| 换存活节点挂载成功，但受影响 Brick 拖慢聚合操作 | [evidence/08-new-mount-via-node2.txt](evidence/08-new-mount-via-node2.txt)、[evidence/09-note-ls-hang-explains-availability.txt](evidence/09-note-ls-hang-explains-availability.txt) |

---

上一课：[Lesson 3 - Volume](../lesson03-volume/README.md)
下一课：[Lesson 5 - Distributed Hash](../lesson05-distributed-hash/README.md)
