# REQ / REP —— 请求响应

## 心智模型

```
Client                Server
  │                      │
 REQ  ── send request ──▶ REP
  │                      │
 REQ  ◀── send reply  ── REP
```

REQ/REP 就是 **RPC**：客户端发一个请求，阻塞等待，服务端处理后回一个响应。

**铁律：send/recv 必须严格交替**，两端都是如此：

```
Client:  send -> recv -> send -> recv -> ...
Server:  recv -> send -> recv -> send -> ...
```

违反顺序（比如连续 send 两次）会得到 `EFSM` 错误——这是 ZMQ 内部状态机在保护"一问一答"的语义。

## 什么场景用 REQ/REP

- 需要**同步**的、一对一的请求-响应交互（类似 HTTP、传统 RPC）。
- 请求量不大，客户端可以接受阻塞等待。
- **不需要**在等回复的同时发新请求（如果需要，应该用 [DEALER/ROUTER](../04_dealer_router/README.md)）。

## 什么场景不该用

- 需要并发发多个请求、不想被"一问一答"卡住 -> 用 DEALER。
- 需要广播 -> 用 PUB/SUB。
- 服务端可能长时间无响应、或客户端要做超时重试 -> 裸 REQ/REP 会卡死，需要 Lazy Pirate 模式（见示例 3）。

## 3 个示例

| 示例 | 场景 | 学到什么 |
|---|---|---|
| [example1_basic_rpc](example1_basic_rpc/) | 最基础的 Ping-Pong | send/recv 严格交替的心智模型 |
| [example2_calculator_service](example2_calculator_service/) | 结构化计算服务 | 多部分消息（multipart）传参、错误响应设计 |
| [example3_lazy_pirate_reliable](example3_lazy_pirate_reliable/) | 带超时+重试的可靠调用 | 裸 REQ/REP 遇到服务端失联会永久阻塞；如何用 poll+超时+重建 socket 解决 |

每个示例目录下都有独立的 `README.md` 说明如何运行。
