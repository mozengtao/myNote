# DEALER / ROUTER —— 异步请求响应 / 交换机

## 心智模型

ROUTER 就像一台**交换机**：它知道每个连接进来的对端是谁（每个连接一个 Identity），可以按 Identity 主动把消息发给指定的连接，而不是像 REP 那样只能"回给刚刚问我的那个人"。

DEALER 是 REQ 的"无限制"版本：**没有严格的 send/recv 交替要求**，可以连续发多个请求而不必等每个的回复，天然支持异步、并发。

```
DEALER Client(s)                     ROUTER Server
    │                                     │
    │  send req1 ─┐                       │ 知道每个连接的 identity:
    │  send req2 ─┼───────────────────▶   │   ClientA, ClientB, ClientC ...
    │  send req3 ─┘                       │
    │  recv rep1 (无序，谁先处理完谁先回)   │◀── 可以按identity主动推送
```

## 关键特性

- **DEALER**：像"没有状态机限制的 REQ"，send 多次不用等 recv，recv 到的回复顺序也不保证跟发送顺序一致（如果对端是多个 worker 各自异步处理）。
- **ROUTER**：接收消息时，第一帧自动是"发送方的 Identity"（一个 ZMQ 自动分配或手动设置的标识），回复时必须把 Identity 作为第一帧发回去，ZMQ 才知道该发给哪个连接。
- **身份识别 + 主动推送**：普通的 REP/PULL 只能被动等对方发消息，ROUTER 可以对**任意已知的 Identity** 主动 `send()`，不需要对方先发消息过来。

## 什么场景用 DEALER/ROUTER

- REQ/REP 严格交替限制成了瓶颈：客户端要并发发多个请求、不想被卡住 -> 用 DEALER。
- 需要做**中间代理/网关**，在多个客户端和多个 worker 之间做负载均衡（"扩展请求-响应"模式）-> ROUTER 做中转。
- 服务端需要**主动推送**消息给特定客户端（而不是等客户端来问）-> ROUTER 记住 Identity 后随时推送。

## 什么场景不该用

- 简单的一问一答、不需要并发 -> 用 REQ/REP 更简单，不用手动管理 Identity 和帧结构。
- 纯广播、不需要知道对方是谁 -> 用 PUB/SUB。

## 3 个示例

| 示例 | 场景 | 学到什么 |
|---|---|---|
| [example1_async_rpc](example1_async_rpc/) | ROUTER 并发处理多个 DEALER 客户端 | 对比 REQ/REP 的阻塞限制，理解异步请求-响应 |
| [example2_broker_load_balance](example2_broker_load_balance/) | ROUTER-DEALER 中间 Broker | 经典"扩展请求-响应"模式，REQ 客户端 <-> Broker <-> REP-like Worker |
| [example3_identity_push](example3_identity_push/) | 基于 Identity 的主动推送通知 | ROUTER 如何记录并按需主动推送给指定客户端 |

每个示例目录下都有独立的 `README.md` 说明如何运行。
