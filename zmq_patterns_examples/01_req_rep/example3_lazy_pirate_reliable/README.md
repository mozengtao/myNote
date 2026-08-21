# 示例 3：Lazy Pirate 可靠 REQ/REP（超时 + 重试）

裸 REQ/REP 有一个致命弱点：**如果服务端挂了、卡住了，或者网络丢了这次请求，客户端会永久阻塞在 `zmq_recv()` 上**，因为 REQ 的状态机严格要求"发一个、等一个"，你没法在等待时再发一次或者取消。

这个示例演示业界标准解法 ——「**Lazy Pirate 模式**」：客户端用 `zmq_poll` 给等待加超时，超时就**放弃当前 socket、重新建一个 REQ socket**，重发同一个请求，重试若干次后彻底放弃。

## 场景

- 服务端偶尔"变慢"（模拟 GC 停顿 / 突发负载 / 假死），处理时间可能超过客户端能接受的超时。
- 客户端不愿意被永久卡死，需要有限次重试 + 最终放弃的策略。

## 心智模型

```
Client                              Server(有时很慢)
  │  send("Request 0")                  │
  │─────────────────────────────────▶   │  (处理耗时 4s，超过客户端2.5s超时)
  │  poll 2.5s ... 超时！               │
  │  关闭旧REQ socket，新建一个            │
  │  重新 connect + 重发同一个请求         │
  │─────────────────────────────────▶   │  (这次很快处理完)
  │◀──── "Reply to Request 0" ─────────  │
```

**关键心智**：REQ socket 一旦发送但没收到回复，就处于"半个问句"状态，**不能**指望它恢复正常再收——必须整个丢弃重建。这正是为什么 Lazy Pirate 需要"关闭旧 socket、开新 socket"而不是继续用同一个 socket 重试。

## 编译 & 运行（C）

```bash
make
./server        # 终端1：模拟一个偶尔很慢的服务
./client        # 终端2：带超时重试的客户端
```

## 运行（Python）

```bash
python3 server.py
python3 client.py
```

## 预期输出（Client，示例）

```
[Client] Sending: Request 0
[Client] Received reply: Reply to Request 0
[Client] Sending: Request 1
[Client] No response from server, retrying... (2 retries left)
[Client] Received reply: Reply to Request 1
[Client] Sending: Request 2
...
```

## 关键点

- 超时时长（客户端 2.5s）应该 **明显小于** 服务端偶尔出现的最坏处理时间（服务端模拟的最坏延迟 4s），才能观察到重试现象。
- 重试次数用尽后，客户端应该**明确放弃**并给出错误提示，而不是无限重试——这是设计可靠系统的基本原则。
- 该模式只解决"客户端不被卡死"的问题；如果服务端本身需要水平扩展/多个worker，应该进一步引入 [ROUTER/DEALER Broker](../../04_dealer_router/example2_broker_load_balance/)。
