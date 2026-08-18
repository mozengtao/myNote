# 第 08 课：五个代表例（表驱动协议 FSM）

主例「教学 TCP」的完整实现在 `code/`。其余四例用来对照：同一张表思想，不同的省略程度。

本课继续使用状态、转移、事件、动作、超时、重试。新增加的是 **用表编码转移**，不是新的状态层。

---

## 例 1：教学 TCP 主路径（主例）

**问题**：用五态八事件走通被动打开、主动打开、关闭；动作只打印 `send SYN` 等。

**状态图**：见 `LESSON.md` 中「本课代码实际实现的图」。RFC 全图同文件，供对照。

**状态表**：`CLOSED` / `LISTEN` / `SYN_SENT` / `ESTABLISHED` / `TIME_WAIT`；查找 `table[state][event]`。

**实现**：`code/c/tcp_fsm.c`、`code/cpp/tcp_fsm.cpp`、`code/python/tcp_fsm.py`。

**复杂度**：5 个扁平状态；稀疏表约 11 格有定义。

**工业用途**：帮助阅读真实 TCP 控制面时先有一张可运行的小图。

**常见缺陷**：把教学五态当成 RFC 十一态；或在 `dispatch` 里重新写 `switch`。

**重构**：加 `SYN_RCVD` 只加枚举和格子，不改查找循环。引擎骨架见课程 `engine/`。

---

## 例 2：TIME_WAIT

**问题**：主动关闭后不能立刻复用同一四元组，要等 2MSL，以免迟到的段串到新连接上。

本课把「发 FIN 之后的所有中间态」压成直接进入 `TIME_WAIT`，然后一次 `TIMEOUT` 回 `CLOSED`。

```
ESTABLISHED --APP_CLOSE / send FIN--> TIME_WAIT --TIMEOUT--> CLOSED
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| ESTABLISHED | APP_CLOSE | 有格 | send FIN | TIME_WAIT |
| TIME_WAIT | TIMEOUT | 有格 | （无） | CLOSED |
| TIME_WAIT | APP_CONNECT | 无格 | ignore | TIME_WAIT |

**复杂度**：在主例里只占两行。真实栈还有 timewait 控制块、端口复用选项（那是策略，不是本课表结构）。

**工业用途**：连接拆除、本地端口耗尽排查。

**常见缺陷**：`TIMEOUT` 与 `SYN_SENT` 的重传写成同一个 handler，到期后在 `TIME_WAIT` 里误发 SYN。

**重构**：把事件拆成 `MSL_EXPIRED`（见练习）。本例保持一个 `TIMEOUT`，靠状态选格。

---

## 例 3：同时打开（只画图，不实现）

**问题**：两端同时 `connect`，都进入 `SYN_SENT`，各自收到对端的 SYN（不是 SYN-ACK）。

RFC 路径：

```
+----------+  rcv SYN / send SYN-ACK  +-----------+  rcv ACK  +--------------+
| SYN_SENT | -----------------------> | SYN_RCVD  | --------> | ESTABLISHED  |
+----------+                          +-----------+           +--------------+
```

本课代码 **没有** `SYN_RCVD`，也 **没有** `SYN_SENT + RCV_SYN` 这一格。若在教学程序里喂 `RCV_SYN` 给 `SYN_SENT`，轨迹必须是 `ignore`。

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| SYN_SENT | RCV_SYN | RFC：有；本课：无格 | send SYN-ACK | SYN_RCVD（本课省略） |
| SYN_RCVD | RCV_ACK | RFC：有；本课：无此状态 | （无） | ESTABLISHED |

**复杂度**：RFC 多一态；教学零实现。

**工业用途**：合法但少见。栈必须正确，快路径通常不照顾它。

**常见缺陷**：在本课 `main` 里「顺便实现同时打开」，破坏「明确省略 `SYN_RCVD`」的教学契约。

**重构**：若练习要做同时打开，先加回 `SYN_RCVD`，再加格子。不要用框中框表示「打开方式」。

---

## 例 4：重传（图 / 文字）

**问题**：主动打开发出 SYN 后，对端或网络没有 SYN-ACK。RTO 到期应再发，次数与间隔是第 07 课的重试；本课只演示 **一次** 重传格。

```
CLOSED --APP_CONNECT / send SYN--> SYN_SENT --TIMEOUT / retransmit SYN--> SYN_SENT
SYN_SENT --RCV_SYNACK / send ACK--> ESTABLISHED
```

| 第 07 课播放器 | 本课 SYN 重传 |
|----------------|----------------|
| CONNECTING | SYN_SENT |
| TIMEOUT | TIMEOUT（RTO） |
| retries_left | 本课表里 **没有** 次数（省略预算） |
| GIVE_UP | 本课未实现；次数用尽应回 CLOSED |

主例 `main` 只喂一次 `TIMEOUT` 再喂 `RCV_SYNACK`。完整预算（3 次后放弃）请回到第 07 课的数据字段；本课强调的是 **那一行表**，不是计数器。

**复杂度**：自环一格。

**工业用途**：握手可靠。真实 RTO 会根据 RTT 估计变化。

**常见缺陷**：在表里做重传，却在动作里 `sleep`；或无限自环没有预算（本课演示路径故意只打一枪，讲义必须说清省略了放弃）。

**重构**：把 `retries_left` 放进 `Fsm` 数据，动作用监护决定是重传还是放弃——监护再次不好塞进二维表，这是表驱动的边界。第 09 课用到期事件区分「再试」和「放弃」。

---

## 例 5：DPDK 类用户态 TCP（只谈文字）

**问题**：数据包在用户空间轮询收发，TCP 控制面仍要回答「当前该发 SYN 还是 ACK」。

和本课相同的部分：

- 状态名仍来自 RFC
- 仍有「应用命令 / 收到段 / 定时器」三类输入
- 仍应能画出与第 08 课同类的表（哪怕工业代码不是二维数组）

和本课不同的部分：

```
+------------------+     poll      +------------------+
| NIC queues       | ------------> | user TCP stack   |
| (DPDK-class)     |               | control + data   |
+------------------+               +------------------+
                                          |
                                          | timers from TSC / cycles
                                          v
                                   same conceptual FSM
```

- 分发不是内核软中断，而是轮询循环里「有段就当 `RCV_*`」
- 定时器不是 jiffies，常是时间戳相减
- 快路径更激进，因为线程模型与内核不同

本例 **不写代码、不链任何用户态栈源码**。你只需要能说：表驱动描述的是控制面，轮询描述的是事件从哪来。事件从哪来，是第 09 课队列要解决的问题。

**复杂度**：概念对照，零行教学代码。

**工业用途**：用户态协议栈、NFV。

**常见缺陷**：以为用户态就不再需要状态机；或把轮询循环当成新的状态层。

**重构**：把「网卡可读」变成入队事件——留给第 09 课，不要在 TCP 教学程序里开线程。
