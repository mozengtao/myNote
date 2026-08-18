# 附录 QUIC 参考答案

先自己做 `extras/quic/quiz.md`，再对答案。

## 1. 不是 UDP 上的 TCP 十一态

QUIC 连接生命周期和 TCP 的 `SYN_SENT` / `TIME_WAIT` 等不是一一对应。例如 TCP 的 `TIME_WAIT` 在 QUIC 里没有同名状态：关闭用 `DRAINING`/`CLOSING` 加空闲计时，端口复用问题由连接 ID 解决，不靠四元组等待 2MSL。

## 2. 流和连接分开

流是连接里面的多条可靠字节流，各有自己的开关机。连接的 `CONNECTED` 表示握手完成、可以承载流；不等于每一条流都开着。draining 时连接正在关门，不允许 new stream。

## 3. PTO 不是连接表上的 TIMEOUT

PTO 到期应推动丢包恢复（重传某个 packet number 空间里的包），不是把连接从 `CONNECTED` 打回握手态。连接表上的 `TIMEOUT` 若写成「重传握手」，会把恢复层和连接层缠死。

## 4. SEND_INITIAL + HANDSHAKE_DONE 缺格

还没走完 Initial 被确认、Handshake 密钥可用这一段。若允许跳到 `CONNECTED`，等于声称握手已完成，却漏掉了中间那次确认（教学里对应 `INITIAL_ACK` 之后才进入 `HANDSHAKE`）。

## 5. PATH_FAIL 两段脚本

已连接：`...` 到达 `CONNECTED` 后喂 `PATH_FAIL`，结束状态 `DRAINING`。

握手中：停在 `SEND_INITIAL` 或 `HANDSHAKE` 时喂 `PATH_FAIL`，结束状态 `CLOSED`。

最短例子：

```
CONNECT/INITIAL 路径到达 CONNECTED, PATH_FAIL  -> DRAINING
init 后停留 SEND_INITIAL, PATH_FAIL           -> CLOSED
```
