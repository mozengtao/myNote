# 04 TCP 三次握手：`nic_send(flags, seq, ack)`，序号活在 Ctx

> **钉死的心智：** 协议 FSM 决定「现在该发什么」；真正把比特送上网卡的是 `nic_send(flags, seq, ack)`。序号是 **扩展状态**，放在 Ctx（TCB），不是 Worker 的私货，也不是 State 枚举的一部分。

这是 RFC 793 三次握手的最小切片。转移表仍按 kind 查。发送参数从 Ctx 的 `local_seq` 和 Event 的 `seq` 填出来：`ack = event.seq + 1`。

## 状态 / 事件 / 动作

```
State:      CLOSED, LISTEN, SYN_SENT, SYN_RCVD, ESTABLISHED
Event:      PASSIVE_OPEN, ACTIVE_OPEN
            SYN(seq, ack), SYNACK(seq, ack), ACK(seq, ack)
Action:     NIC_SEND(flags, seq, ack) -> nic_send(flags, seq, ack)
Ctx:        state, local_seq, remote_seq
```

转移表（未列出的组合一律 REJECT）：

```
+------------+--------------+-------------+----------------------------------+
| State      | kind         | Next State  | Worker call                      |
+------------+--------------+-------------+----------------------------------+
| CLOSED     | PASSIVE_OPEN | LISTEN      | (none)                           |
| LISTEN     | SYN          | SYN_RCVD    | nic_send(SYN+ACK, local, seq+1)  |
| SYN_RCVD   | ACK          | ESTABLISHED | (none)                           |
| CLOSED     | ACTIVE_OPEN  | SYN_SENT    | nic_send(SYN, local, 0)          |
| SYN_SENT   | SYNACK       | ESTABLISHED | nic_send(ACK, local+1, seq+1)    |
+------------+--------------+-------------+----------------------------------+
```

`apply` 在接受后把 Decision 里的 `local_seq` / `remote_seq` 写回 Ctx。NIC Worker 只拿到三个参数，不知道自己在 LISTEN 还是 SYN_SENT。

状态图：

```
  CLOSED --PASSIVE_OPEN--> LISTEN --SYN--> SYN_RCVD --ACK--> ESTABLISHED

  CLOSED --ACTIVE_OPEN--> SYN_SENT --SYNACK--> ESTABLISHED
```

## 一次事件的三层走查

服务端 Ctx `local_seq=1000`，当前 `LISTEN`，收到 `SYN(seq=2000, ack=0)`：

```
[EVENT]    SYN(seq=2000, ack=0) | state=LISTEN
[DECISION] LISTEN + SYN -> SYN_RCVD
[DISPATCH] NIC_SEND(flags=SYN+ACK, seq=1000, ack=2001)
[EXEC]     nic_send(SYN+ACK, 1000, 2001)
```

- Decision：查表；`flags` 来自判定，`seq` 来自 Ctx，`ack` 来自 Event.seq+1。
- Dispatch：调用 `nic_send(SYN+ACK, 1000, 2001)`。
- Execution：驱动 / 网卡。FSM 没有写 socket。

## 如果把执行写进 FSM

```
if state == LISTEN and event.kind == SYN:
    build_synack_skb(seq, ack)
    ip_output()
    dma_kick()
    state = SYN_RCVD
```

协议判定和发包路径缠死。真实内核里 TCP 状态机填好序号，再调用 `tcp_transmit_skb()`。本例用 `nic_send` 扮演那一层。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

两个脚本化场景：

- `server` `local_seq=1000`: `PASSIVE_OPEN, SYN(2000,0), ACK(2001,1001), SYN(2002,1001)`
- `client` `local_seq=5000`: `ACTIVE_OPEN, SYNACK(8000,5001), ACK(8001,5001)`
