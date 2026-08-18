# 附录 QUIC：四个短例

主例在 `code/`。流和丢包恢复只出现在文字里。

---

## 例 1：连接主路径（主例）

**问题**：客户端发出 Initial，等到确认，再等到握手完成。

```
SEND_INITIAL --INITIAL_ACK--> HANDSHAKE --HANDSHAKE_DONE--> CONNECTED
```

**实现**：`code/c/quic.c`、`code/cpp/quic.cpp`、`code/python/quic.py`。

**常见缺陷**：`HANDSHAKE_DONE` 从 `SEND_INITIAL` 直接进 `CONNECTED`，跳过 Initial 被确认。

---

## 例 2：应用关闭与 draining

应用要关时先发 CONNECTION_CLOSE，再留一段时间收迟到包，然后真正消失。

| Current State | Event | Action | Next |
|---------------|-------|--------|------|
| CONNECTED | APP_CLOSE | CONNECTION_CLOSE | DRAINING |
| DRAINING | IDLE_TIMEOUT | drained | CLOSED |

**常见缺陷**：`APP_CLOSE` 直接 `CLOSED`，对端重传的包会当成新连接。

---

## 例 3：空闲超时与路径失败

已连接空闲：直接 `CLOSED`。已连接路径失败：教学进 `DRAINING`。握手尚未完成的路径失败：直接 `CLOSED`。

```
CONNECTED --IDLE_TIMEOUT--> CLOSED
CONNECTED --PATH_FAIL--> DRAINING
HANDSHAKE --PATH_FAIL--> CLOSED
```

**常见缺陷**：路径失败时拆掉整台进程，或在握手失败后仍创建 stream。

---

## 例 4：流、丢包、非 TCP（只讲，不实现）

每条 QUIC stream 有自己的发送/接收状态，和连接是否 `CONNECTED` 正交：连接已 draining 时流不能再开，但旧流的复位帧仍可能到达。

丢包恢复看 packet number space 和 PTO，不在连接五态上自环重传 Initial。

QUIC 不是 TCP：没有 `SYN_SENT` / `TIME_WAIT` 那些名字，Connection ID 让路径可以变。

**常见缺陷**：用本附录五态去实现流控窗口。
