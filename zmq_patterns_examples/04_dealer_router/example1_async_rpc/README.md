# 示例 1：DEALER 异步请求 —— 打破 REQ/REP 的阻塞限制

用 REQ/REP 时，客户端**必须**严格 `send -> recv -> send -> recv`，一次只能有一个"在途"请求。这个示例用 DEALER 客户端连续发送 5 个请求**完全不等回复**，然后再统一收 5 个回复，证明 DEALER 没有这个限制。

## 场景

客户端需要"流水线化"发出多个请求（比如批量查询），不想每次都傻等一个来回的网络延迟，DEALER 可以把请求"攒着发"，回复"批量收"，整体吞吐量更高。

## 心智模型

```
DEALER Client                        ROUTER Server
   │  send req0 ─┐
   │  send req1 ─┤ 连续发5个，不等回复！
   │  send req2 ─┤ (REQ在这里会直接报EFSM错误)
   │  send req3 ─┤
   │  send req4 ─┘
   │──────────────────────────────▶  │  ROUTER 逐个处理(每个自动带上是谁发的identity)
   │◀────────────────────────────── │  逐个回复(带上identity告诉ROUTER发给谁)
   │  recv x5 (收完5个回复)          │
```

可以同时运行**多个** DEALER 客户端实例，ROUTER 能自动区分是哪个客户端发来的（通过内部的 identity 帧），分别正确地把回复送回对应客户端——即使它们的请求是交错到达的。

## 编译 & 运行（C）

```bash
make
./server              # 终端1
./client A &          # 终端2：客户端A，并发发5个请求
./client B &          # 终端3：客户端B，同时也发5个请求
```

## 运行（Python）

```bash
python3 server.py
python3 client.py A &
python3 client.py B &
```

## 预期输出（Client A，示例）

```
[Client A] Sending 5 requests without waiting for replies...
[Client A] Sent: req 0
[Client A] Sent: req 1
[Client A] Sent: req 2
[Client A] Sent: req 3
[Client A] Sent: req 4
[Client A] Received reply: Reply to req 0
[Client A] Received reply: Reply to req 1
...
```

## 关键点

- 如果把 `client.c` 里的 socket 换成 `ZMQ_REQ`、把 server 换成 `ZMQ_REP`，连续发 5 次 `send` 而不 `recv` 会直接报错（`EFSM`）——这正是 DEALER 存在的意义。
- ROUTER 收到消息时，第一帧是 ZMQ 自动生成的 Identity（客户端不用关心也看不到这个帧），回复时必须把这个 Identity 原样带上作为第一帧，ZMQ 才知道要发给哪个连接——本例的 `server.c`/`server.py` 演示了这个"收/发都带 identity"的固定套路。
