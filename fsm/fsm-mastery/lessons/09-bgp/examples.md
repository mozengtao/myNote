# 第 09 课：五个代表例（事件循环与定时器）

主例「教学 BGP 会话」的完整实现在 `code/`。表驱动是第 08 课的能力；本课看事件如何入队、钟如何变成事件。

---

## 例 1：BGP 会话主路径（主例）

**问题**：`START` 到 `ESTABLISHED`，再被 Hold 拆回 `IDLE`。中间 Keepalive 可以响，但不能拆会话。

**状态图**：见 `LESSON.md`。

**状态表**：五态；到期事件与报文事件同一张表。

**实现**：`code/c/bgp_fsm.c`、`code/cpp/bgp_fsm.cpp`、`code/python/bgp_fsm.py`。

**复杂度**：5 个扁平状态；3 只钟；容量 16 的数组队列。

**工业用途**：对等会话建立与保活。

**常见缺陷**：`tick` 里直接改状态；或 Keepalive 到期走了 Hold 的格子。

**重构**：不要把「正在发心跳」画成 `ESTABLISHED` 的子状态。

---

## 例 2：Hold 与 Keepalive

**问题**：两只钟同时武装在 `ESTABLISHED`。一只说「该我说」，一只说「该听却没听到」。

```
Keepalive -> 0  =>  enqueue KEEPALIVE_EXPIRED  =>  send KEEPALIVE, 仍 ESTABLISHED
Hold      -> 0  =>  enqueue HOLD_EXPIRED       =>  drop, IDLE
rcv KEEPALIVE_MSG => reset Hold (peer is alive)
```

教学拍数 Hold=5、Keepalive=2，只为了在轨迹里数得清。RFC 4271 常见默认是 Hold 90 秒、Keepalive 为 Hold 的三分之一；实现还必须协商，取双方较小值。

| 钟 | 到期事件 | 典型动作 |
|----|----------|----------|
| Keepalive | KEEPALIVE_EXPIRED | 发送，再武装自己 |
| Hold | HOLD_EXPIRED | 拆会话 |

**复杂度**：同一状态、两行表。

**工业用途**：发现死对等体，避免黑洞路由。

**常见缺陷**：收到报文只刷新 Keepalive；Hold 仍会误超时。

**重构**：BFD 可以比 Hold 更快地宣告对端死亡——那是另一路事件入同一队列，不是新状态层。

---

## 例 3：ConnectRetry

**问题**：`START` 之后 TCP 一直没起来。不能 `while (!connected)` 死等，也不能立刻放弃。

```
IDLE --START / arm ConnectRetry=3--> CONNECT
tick, tick, tick --> CONNECTRETRY_EXPIRED / retry open TCP --> CONNECT
```

这是第 07 课重试：自环 + 再武装。本课演示 **没有** 次数上限；练习可以加 `retries_left`。

RFC 把 Connect 失败后转到 Active、再听对端连过来。本课不实现 Active，到期仍停在 `CONNECT` 再试。

**复杂度**：一格自环 + 一只钟。

**工业用途**：对端尚未启动时的会话自动恢复。

**常见缺陷**：ConnectRetry 写成 `sleep`；或到期后回到 `IDLE` 却不自动 `START`，会话再也起不来（除非配置了自动启停——那又是额外事件）。

**重构**：失败后 `post(START)` 从 `IDLE` 再走会把表绕远；本课自环更短。自动启停策略用数据标志，不要加 `AUTO_STARTING` 状态。

---

## 例 4：Graceful Restart（只谈文字）

**问题**：控制面进程重启时，转发面若仍能按旧 FIB 转发，邻居不必立刻撤掉全部路由。RFC 4724 等扩展在 4271 的基础图 **之外** 增加协商与等待。

真实守护进程因此常常出现 RFC 4271 没画的局面，例如：

- 会话暂时断开，但前缀被标记为「等待重启结束」
- 额外的 Restart Timer，到期才真正清表

这些 **不是** 本课五个状态里的第六个。它们是另一组定时器 + 另一组事件，仍然可以入同一队列、查另一张表，或查同一张表的更多行。

本课代码 **零实现**。你只需要记住：工业实现和 RFC 基础图不一致，常常是因为后来的扩展，而不是因为有人不会画状态机。

**复杂度**：文字对照。

**工业用途**：控制面热升级、进程崩溃恢复。

**常见缺陷**：把 Graceful Restart 画成 `ESTABLISHED` 里面的子状态机，并在本课提前使用后面才教的套盒写法。不要。用「额外事件 + 额外钟」描述即可。

**重构**：第 10 课将展示两台扁平机协作；重启等待仍然可以是会话机旁边的另一台小机，而不是盒子套盒子。

---

## 例 5：BGP 坐在 TCP 上面（两台扁平机，不是一台套一台）

**问题**：BGP 报文跑在 TCP 上。是不是应该把 TCP 的十一态画进 BGP 的 `CONNECT` 框里？

不要。那是两台独立的扁平机，通过 **事件** 说话：

```
+------------------+     TCP_OPEN / TCP_FAIL      +------------------+
| teaching TCP     | ---------------------------> | teaching BGP     |
| (lesson 08)      |                              | (this lesson)    |
| CLOSED..TIME_WAIT| <--------------------------- | IDLE..ESTABLISHED|
+------------------+     "please connect"          +------------------+
```

- TCP `ESTABLISHED` 对 BGP 只是 `TCP_OPEN`
- BGP `IDLE` 拆会话时，对 TCP 只是 `APP_CLOSE`
- BGP 的 Hold 到期，不需要知道 TCP 是不是 `TIME_WAIT`

本课代码用假事件模拟这条缝，没有把两份 `Fsm` 链在一个进程里跑。第 10 课才会在 **同一份教学程序** 里放两台机。这里只要建立直觉：下层协议的状态名，不要抄进上层枚举。

**复杂度**：概念上 5 + 5（教学 TCP 五态），不是 5 乘 5。

**工业用途**：所有「应用协议跑在 TCP 上」的控制面。

**常见缺陷**：`CONNECT` 里再列 `SYN_SENT`，状态乘起来；或把队列误叫成并发角色邮箱。

**重构**：两台机各一张表、各一个 `dispatch`；需要缓冲时，入队的是 **对方听得懂的事件名**。
