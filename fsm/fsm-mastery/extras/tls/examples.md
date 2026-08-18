# 附录 TLS：四个短例

主例在 `code/`。密码学全部不出现。

---

## 例 1：握手主路径（主例）

**问题**：客户端要等到 Finished 才把连接当成可用。

```
START --CONNECT--> WAIT_SERVER_HELLO --SERVER_HELLO--> WAIT_FINISHED
WAIT_FINISHED --CERT_OK--> WAIT_FINISHED --FINISHED--> ESTABLISHED
```

**实现**：`code/c/tls.c`、`code/cpp/tls.cpp`、`code/python/tls.py`。

**常见缺陷**：`SERVER_HELLO` 直接进 `ESTABLISHED`，证书和 Finished 被跳过。

---

## 例 2：握手超时

等 ServerHello 太久应关闭，而不是卡在 `WAIT_SERVER_HELLO`。

| Current State | Event | Action | Next |
|---------------|-------|--------|------|
| WAIT_SERVER_HELLO | TIMEOUT | timeout | CLOSED |
| ESTABLISHED | TIMEOUT | 无格 | ESTABLISHED |

**常见缺陷**：握手超时和已建立后的空闲超时共用一个 handler。

---

## 例 3：Alert

对端或本地判定失败，发/收 Alert 后进入 `CLOSED`。教学不区分 fatal / warning。

```
WAIT_FINISHED --ALERT--> CLOSED
ESTABLISHED --ALERT--> CLOSED
```

**常见缺陷**：warning Alert 把整条连接拆掉；或 fatal Alert 仍留在 `ESTABLISHED` 读应用数据。

---

## 例 4：0-RTT 与票证（只讲，不实现）

0-RTT：ClientHello 就可带应用数据，服务器可以拒绝，客户端必须能回退到完整握手。

票证：握手之后的 `NewSessionTicket` 给下一次 PSK 恢复用，不是本附录五个状态里的一员。

**常见缺陷**：把「有票证」做成 `START` 的子状态；或把 0-RTT 数据成功当成握手已完成。
