# Lesson 6 - Translator

## 一、本实验目的

证明：**Translator 是 GlusterFS 唯一的功能实现单元——所有能力（分布、副本、缓存、权限、日志）都是一个个独立的 Translator，它们像 Unix 管道一样按声明式的图（volfile）串联起来；一次文件系统调用，本质上是这张图上的一次"信号传播"：从上往下经过若干层处理，可能在某一层被"扇出"成多个并行的下游请求，再把结果汇聚、逐层往上传回。**

## 二、背景知识

- Lesson 3、4 已经看过 client volfile 和 brick volfile 里那一长串 `volume xxx / type yyy / subvolumes zzz / end-volume` 的结构，那就是 Translator 图的文本表示。
- 每个 Translator 只做一件很窄的事情（比如 `performance/write-behind` 只管"写请求要不要先在内存缓一下再发出去"），对下游 Translator 而言，它看到的还是一套标准的文件操作接口（open/read/write/lookup/...），这就是为什么它们能像积木一样自由拼接。
- 本课要用两种手段验证这套"图"到底是不是"真的在工作"：(1) 抓一次真实的系统调用轨迹，看一个文件操作到底触发了哪些底层动作；(2) 主动关掉一个 Translator，看配置图和行为会不会同步变化。

## 三、提出问题

1. 如果 Translator 只是"配置文件里的一个节点"，它凭什么能改变实际的网络行为？配置和行为之间的桥梁是什么？
2. 一次 `write()` 系统调用，在到达 Brick 之前，是"直来直去"一条链路，还是会在某一层被"复制"成多份？
3. 如果我在 `gluster volume set` 里关掉某个性能相关的 Translator，这个变化立刻会体现在正在运行的 client 进程里吗，还是要重新挂载？
4. Lesson 5 发现文件是按哈希落到唯一一个 Brick 上的——那创建文件时，client 是只联系了那一个目标 Brick，还是联系了全部 3 个？

## 四、实验拓扑

沿用现有环境，重点关注 **client 侧的 `glusterfs` 用户态进程**内部：

```
                     应用程序 (bash: echo > file)
                              |
                        write()/openat() 系统调用
                              |
                              v
                    Linux 内核 VFS + FUSE 内核模块
                              |
                     /dev/fuse (一个特殊字符设备)
                              |
                              v
              client 容器里的 glusterfs 用户态进程 (PID 106)
                   [Translator 调用图，从 Lesson3 的 volfile 展开]
                              |
                    分别经三条独立 TCP 连接
                              |
              +---------------+---------------+
              |               |               |
     node1:52943(brick)  node2:60832(brick) node3:60791(brick)
```

## 五、实验步骤

### Step 1：一边对 client 进程做 strace，一边创建一个新文件

为什么做：直接观察"一次 FUSE 请求进来，触发了哪些底层 read/write"，这是"配置图在运行时到底做了什么"的第一手证据。（容器需要额外的 `SYS_PTRACE` capability 才能 strace 别的进程，已在启动 client 容器时加上）

```bash
strace -tt -f -p <glusterfs进程PID> -e trace=read,write,readv,writev -e read=7 -e write=7 -yy -s 200 -o /tmp/client.strace
# 另一个终端:
echo "payload" > /mnt/gluster/fusetrace-1.txt
```

（`7` 是这个进程里 `/dev/fuse` 对应的文件描述符号，需要先用 `ls -la /proc/<pid>/fd/` 确认）

### Step 2：分析这段 strace，重点看"一进一出"之间夹了什么

### Step 3：关掉一个 Translator，对比 volfile 变化

为什么做：验证"配置图"和"运行时行为"是同一个东西的两种呈现方式——图变了，行为必须跟着变。

```bash
docker exec node1 cat /var/lib/glusterd/vols/gv-dist/trusted-gv-dist.tcp-fuse.vol > before.vol
docker exec node1 gluster volume set gv-dist performance.write-behind off
docker exec node1 cat /var/lib/glusterd/vols/gv-dist/trusted-gv-dist.tcp-fuse.vol > after.vol
diff before.vol after.vol
```

## 六、实验现象

### 现象 1：一次 "create fusetrace-1.txt" 的 FUSE 请求，触发了对三个 Brick 的并行请求

strace 片段（完整原始文件见 [evidence/01-fuse-to-network-fanout-strace.txt](evidence/01-fuse-to-network-fanout-strace.txt)），按时间顺序整理：

```
[1] readv(7</dev/fuse>, ...)                                  <- 内核把"创建 fusetrace-1.txt"的请求递进来
[2] writev(12<TCP 172.28.0.20:49148->172.28.0.11:52943>, ...)  <- 几乎同时，写给 node1 的 Brick
    (payload 里能看到明文的 "fusetrace-1.txt")
[3] writev(9 <TCP 172.28.0.20:49146->172.28.0.12:60832>, ...)  <- 同时写给 node2 的 Brick
[4] writev(10<TCP 172.28.0.20:49145->172.28.0.13:60791>, ...)  <- 同时写给 node3 的 Brick
[5] readv(12<...node1...>) / readv(9<...node2...>) / readv(10<...node3...>)  <- 三个 Brick 分别回包
[6] writev(7</dev/fuse>, ...)                                  <- 汇总结果后，写回内核，应用程序的系统调用才返回
```

也就是说：**一次创建文件的操作，client 并不是只联系了 Lesson 5 算出来的那一个目标 Brick，而是先向全部 3 个 Brick 发起了并行请求**（这是 DHT 的 LOOKUP 阶段——它要确认这个文件名在其他 Brick 上是否已经意外存在，这正是 Lesson 4 里 `hello.txt` 出现两次那个"意外"的另一面：DHT 本来是要在创建时做这层检查的，只是那次是绕过 Gluster 直接在后端造出的冲突，DHT 没有机会介入）。确认无冲突之后，才会真正把 `create` 发给哈希算出来的目标 Brick 完成创建。

**这直接回答了"Translator 在整个流程中的作用是什么"**：`/dev/fuse` 那一侧，应用程序看到的永远是"一进一出"的一次系统调用；但在 Translator 图内部，这一次调用可能被 DHT 这一层"扇出"成多个并行的下游请求，等它们都有了结果，再由同一层把结果"收拢"成一个答案，往上传。**没有 Translator 这一层，FUSE 提供的单一请求/响应模型和"数据分布在多台机器上"这个事实之间，就没有办法对接起来。**

### 现象 2：关闭一个 Translator，配置图里对应的节点被整块删除，上下游自动重新连接

```diff
< volume gv-dist-write-behind
<     type performance/write-behind
<     subvolumes gv-dist-utime
< end-volume
<
75c70
<     subvolumes gv-dist-write-behind
---
>     subvolumes gv-dist-utime
```

（见 [evidence/05-volfile-diff.txt](evidence/05-volfile-diff.txt)）`gv-dist-write-behind` 这个节点被整段删除，它原来的上游（`gv-dist-open-behind`）现在直接接到它原来的下游（`gv-dist-utime`）——**这就是"图"的字面意思：删掉一个节点，边要重新连接**。这也验证了现象 1 的推论：Translator 不是"钩子"或"中间件注解"，而是真正意义上的图节点，删掉一个，调用链路的形状就实实在在地变了。（关闭/恢复完整过程见 [evidence/02-toggle-translator-before.txt](evidence/02-toggle-translator-before.txt)、[evidence/03-toggle-command.txt](evidence/03-toggle-command.txt)、[evidence/04-toggle-translator-after.txt](evidence/04-toggle-translator-after.txt)、[evidence/06-toggle-translator-restored.txt](evidence/06-toggle-translator-restored.txt)）

## 七、原理分析

```
                     一次 write() 系统调用
                              |
                              v
                    FUSE (标准的、单进单出的请求/响应协议)
                              |
                              v
     +------------------------------------------------------+
     |                Translator 调用图                       |
     |                                                        |
     |   io-stats (记日志/计数，原样转发)                        |
     |        |                                              |
     |   io-threads (丢进线程池，原样转发)                       |
     |        |                                              |
     |   md-cache / quick-read / open-behind / write-behind   |
     |        (各自决定要不要缓存、要不要提前返回，原样转发或短路)     |
     |        |                                              |
     |   DHT (cluster/distribute)                             |
     |        | 根据 fop 类型决定"扇出"策略:                     |
     |        |   lookup/create -> 广播给所有子卷 (一致性检查)     |
     |        |   已知位置的 read/write -> 只发给目标子卷           |
     |        |                                              |
     |   protocol/client x N (每个对应一个 Brick 的 TCP 连接)     |
     +------------------------------------------------------+
                              |
                       RPC over TCP (Lesson 9 展开)
                              |
                              v
                       N 个 Brick 上的 glusterfsd
```

**Translator 图之所以能做"扇出-收拢"，关键在于每一层都实现了同一套"fop 回调接口"**：每个 Translator 既是下游看到的"服务提供者"，又是上游看到的"下一个子卷"。DHT 想要同时问 3 个 Brick，只需要在自己的 `lookup()` 实现里，对它挂着的 3 个 `subvolumes` 分别调用一次 `lookup()`，用一个计数器等所有回调都返回后再调用自己上游传进来的回调——这是标准的"扇出-收拢（scatter-gather）"编程模式，Gluster 把它做成了一个通用框架，所有做分布式聚合的 Translator（DHT、AFR）都复用这个模式。

## 八、继续深入：如果我是作者，为什么这样设计？

**备选方案 A：把所有功能写进一个巨大的单体函数（比如一个 2 万行的 `handle_write()`）。**

- 好处：没有跨层调用的开销，性能理论上限更高。
- 代价：任何一个功能的增删（比如"要不要开启缓存"）都要改动这个巨石函数，风险和复杂度随功能数量呈指数增长；不同用户想要的功能组合千差万别（有的要 Replica 不要缓存，有的要缓存不要 Replica），单体函数没法只编译出用户需要的那部分。

**备选方案 B：把每个功能做成一个独立进程，用管道或消息队列串起来（类似 Unix 命令行管道 `cmd1 | cmd2 | cmd3`）。**

- 好处：隔离性更好，一个功能崩溃不影响别的。
- 代价：进程间通信的开销（上下文切换、数据拷贝）对于文件系统这种延迟敏感的场景是不可接受的——每一次 `read()` 都要经过好几次进程切换，性能会明显下降。

**GlusterFS 选择的方案：进程内的插件化调用图（Translator），共享同一个地址空间，用统一的函数指针表（fop 接口）作为"插槽标准"。**

- 好处：图的形状可以按用户需求任意增删（这正是现象 2 展示的），但因为都在同一个进程里，层与层之间只是普通的函数调用，没有 IPC 开销。
- 好处 2：新功能只需要实现标准的 fop 接口就能"插"进图的任意位置，这就是为什么 Gluster 社区能陆续加入 Quota、Bitrot 检测、加密等功能，而不用重写核心代码——`xlators/` 目录下几十个子目录，每一个都是独立开发、独立编译的模块。
- 代价：所有 Translator 运行在同一个进程地址空间，一个 Translator 的内存越界 bug（尤其是 C 语言写的）可能拖垂整个进程，隔离性弱于"独立进程"方案。这是"性能"和"故障隔离"之间的经典取舍。

一句话总结设计哲学：**用统一接口把"功能"做成可插拔积木，图的连接关系用声明式配置描述、可以动态重排；执行时利用同进程函数调用换取性能，用规范的扇出-收拢框架统一处理"一份请求对应多份下游"的分布式聚合问题。**

## 九、源码位置（仅指路）

- `libglusterfs/src/xlator.c`、`libglusterfs/src/graph.c` —— Translator 的通用接口定义、图的解析与构建。
- `xlators/` 目录下每一个子目录都是一类 Translator：`cluster/`（DHT、AFR 等分布式聚合逻辑）、`performance/`（各种缓存）、`features/`（权限、配额、变更日志等附加功能）、`protocol/`（client/server 网络协议）、`storage/posix/`（对接本地文件系统）。
- `xlators/cluster/dht/src/dht-common.c` 里的 `dht_lookup` 系列函数，就是现象 1 里"扇出-收拢"逻辑的具体实现。

## 十、本实验总结

1. Volume 配置图（volfile）不是静态文档，它直接决定运行时会发生哪些网络请求——图变了，行为立刻变。
2. 一次系统调用在 FUSE 这一层是单进单出的，但进入 Translator 图之后，可能会被某一层（如 DHT）"扇出"成对多个 Brick 的并行请求。
3. `create`/`lookup` 这类操作会广播给所有相关子卷做一致性检查，`read`/已知位置的 `write` 则只发给目标 Brick，扇出策略因 fop 类型和 Translator 而不同。
4. 所有 Translator 共享同一套标准 fop 接口，这是它们能自由拼接、扇出-收拢框架能通用化的根本原因。
5. Translator 运行在同一进程地址空间里，用普通函数调用而非进程间通信串联，这是 Gluster 兼顾"可插拔"与"高性能"的关键取舍。

## 十一、思考题

1. 如果把 DHT 从 client volfile 里删掉（只留 1 个 Brick，不做分布式），`create` 还需要广播给"所有子卷"吗？"扇出"这个动作和"分布式"这个需求是不是强绑定的？
2. Lesson 7 要学 Replica（AFR），如果你现在猜测 AFR 的"扇出"策略，你觉得它对 `write` 操作会怎么扇出？和 DHT 对 `write` 的处理方式（只发给目标 Brick）相比，会不会不同？
3. 现象 2 里关闭 Translator 之后，`volume set` 立刻生效了吗，还是需要客户端重新挂载？如果需要重新挂载，这跟 Lesson 4 提到的"volfile 版本轮询机制"是什么关系？
4. 如果一个 Translator 在处理扇出请求时，其中一个下游子卷永远不回应（比如网络分区），"收拢"逻辑要等到什么时候才能返回？这会不会是所有基于扇出-收拢模式的操作共有的一个薄弱点？

## 十二、证据链索引

| 结论 | 证据文件 |
|---|---|
| 一次创建文件的 FUSE 请求被 DHT 扇出成对 3 个 Brick 的并行 TCP 请求 | [evidence/01-fuse-to-network-fanout-strace.txt](evidence/01-fuse-to-network-fanout-strace.txt) |
| 关闭 write-behind 前的 client volfile | [evidence/02-toggle-translator-before.txt](evidence/02-toggle-translator-before.txt) |
| 执行 `gluster volume set ... off` | [evidence/03-toggle-command.txt](evidence/03-toggle-command.txt) |
| 关闭后的 volfile，对应 Translator 节点消失 | [evidence/04-toggle-translator-after.txt](evidence/04-toggle-translator-after.txt) |
| 前后 diff，清晰展示图节点被删除、上下游重新连接 | [evidence/05-volfile-diff.txt](evidence/05-volfile-diff.txt) |
| 恢复设置后 volfile 与最初逐字节相同 | [evidence/06-toggle-translator-restored.txt](evidence/06-toggle-translator-restored.txt) |

---

上一课：[Lesson 5 - Distributed Hash](../lesson05-distributed-hash/README.md)
下一课：[Lesson 7 - Replica](../lesson07-replica/README.md)
