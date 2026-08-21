# 示例 2：双进程心跳保活（TCP）

两个独立的进程（Node A、Node B）通过 PAIR + TCP 建立一对一连接，彼此每秒发一次心跳 `PING <name> <seq>`，同时监控"多久没收到对方消息了"——如果超过 3 秒没收到心跳，判断对方可能已经离线。

## 场景

跨进程/跨主机的简单保活检测：两个节点建立一条专用通道，用于确认"对方还活着"，同时这条通道也可以复用来传输控制指令（不止是心跳）。

## 心智模型

```
Node A (PAIR, bind tcp://*:5571)  ◀───────▶  Node B (PAIR, connect tcp://...:5571)
      │  每1秒发一次 "PING A <seq>"                    │  每1秒发一次 "PING B <seq>"
      │  同时接收对方心跳，刷新 last_seen                │  同样逻辑，完全对等
      │  若 3秒未收到对方消息 -> 打印警告                 │
```

两端代码逻辑**完全相同**（只有 bind/connect 和名字不同），这正体现了 PAIR "无角色区分、完全对等"的心智模型——不像 REQ/REP 或 PUB/SUB 那样两端逻辑必须不同。

## 编译 & 运行（C）

```bash
make
./node_a     # 终端1：先起绑定端
./node_b     # 终端2：再起连接端
```

尝试：运行一段时间后，`Ctrl+C` 停掉 `node_b`，观察 `node_a` 几秒后打印"心跳丢失"警告。

## 运行（Python）

```bash
python3 node_a.py
python3 node_b.py
```

## 预期输出（Node A，示例）

```
[NodeA] PAIR bound on tcp://*:5571
[NodeA] Sent: PING A 0
[NodeA] Received: PING B 0
[NodeA] Sent: PING A 1
[NodeA] Received: PING B 1
...
(此时 Ctrl+C 停掉 NodeB)
[NodeA] Sent: PING A 5
[NodeA] Sent: PING A 6
[NodeA] *** WARNING: no message from peer for 3.1s, peer might be down ***
```

## 关键点

- PAIR 没有自动的"对端断开检测"通知（不像 TCP 的 `close()` 那样立刻知道），需要应用层自己实现心跳超时判断——本例展示了最简单的实现方式。
- `zmq_poll` 超时机制是实现"既要收消息、又要定时发心跳、又要检测超时"三件事同时进行的标准写法，贯穿本代码库的多个示例（[01_req_rep 示例3](../../01_req_rep/example3_lazy_pirate_reliable/)、[04_dealer_router 示例3](../../04_dealer_router/example3_identity_push/)都用了同样的思路）。
