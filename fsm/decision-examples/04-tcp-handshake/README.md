# 04 TCP 三次握手：SEND 是 Action，NIC 才发包

> **钉死的心智：** 协议 FSM 决定「现在该发 SYN+ACK」；真正把比特送上网卡的是 Network Worker。

这是 RFC 793 三次握手的最小切片，只保留 5 个状态。服务端 `PASSIVE_OPEN` 进入 `LISTEN`（无发包）；收到 `SYN` 后判定应回复 `SYN+ACK`。客户端 `ACTIVE_OPEN` 判定应发 `SYN`；收到 `SYNACK` 后判定应发 `ACK`。已建立后再来 `SYN`/`ACK` 一律 REJECT。

## 状态 / 事件 / 动作

```
State:   CLOSED, LISTEN, SYN_SENT, SYN_RCVD, ESTABLISHED
Event:   PASSIVE_OPEN, ACTIVE_OPEN, SYN, SYNACK, ACK
Action:  SEND_SYN, SEND_SYNACK, SEND_ACK
```

转移表（未列出的组合一律 REJECT）：

```
+------------+--------------+-------------+-------------+
| State      | Event        | Next State  | Actions     |
+------------+--------------+-------------+-------------+
| CLOSED     | PASSIVE_OPEN | LISTEN      | (none)      |
| LISTEN     | SYN          | SYN_RCVD    | SEND_SYNACK |
| SYN_RCVD   | ACK          | ESTABLISHED | (none)      |
| CLOSED     | ACTIVE_OPEN  | SYN_SENT    | SEND_SYN    |
| SYN_SENT   | SYNACK       | ESTABLISHED | SEND_ACK    |
+------------+--------------+-------------+-------------+
```

状态图：

```
  CLOSED --PASSIVE_OPEN--> LISTEN --SYN--> SYN_RCVD --ACK--> ESTABLISHED

  CLOSED --ACTIVE_OPEN--> SYN_SENT --SYNACK--> ESTABLISHED
```

`PASSIVE_OPEN` 和 `SYN_RCVD + ACK` 的 Action 为空：判定「进入下一状态」本身不需要发包。

## 一次事件的三层走查

服务端当前 `LISTEN`，收到 `SYN`：

```
[EVENT]    SYN | state=LISTEN
[DECISION] LISTEN + SYN -> SYN_RCVD  actions=[SEND_SYNACK]
[DISPATCH] SEND_SYNACK
[EXEC]     nic -> SEND SYN+ACK
```

- Decision：查表，决定下一状态和「该发 SYN+ACK」。
- Dispatch：把 `SEND_SYNACK` 交给 NIC Worker。
- Execution：`tcp_transmit` / 驱动 / 网卡。FSM 没有写 socket。

## 如果把执行写进 FSM

```
if state == LISTEN and event == SYN:
    build_synack_skb()
    ip_output()
    dma_kick()
    state = SYN_RCVD
```

协议判定和发包路径缠死。真实内核里 TCP 状态机只决定「发什么」；`tcp_transmit_skb()` 才执行。本例用 `worker_nic` 扮演那一层。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

两个脚本化场景，都从 `CLOSED` 开始：

- `server`: `PASSIVE_OPEN, SYN, ACK, SYN`（最后一个 `SYN` 应 REJECT）
- `client`: `ACTIVE_OPEN, SYNACK, ACK`（已建立后再 `ACK` 应 REJECT）
