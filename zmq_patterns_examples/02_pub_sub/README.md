# PUB / SUB —— 发布订阅

## 心智模型

```
                     ┌────────────┐
                     │ Publisher  │
                     └─────┬──────┘
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        Subscriber1  Subscriber2  Subscriber3
```

就像**微信群广播**：发消息的人根本不知道谁在看、有多少人在看。Publisher 只管往外发，Subscriber 自己决定订阅哪些 Topic，收不到自己没订阅的内容。

## 关键特性（务必记住）

- **无送达保证**：PUB 不知道有没有 SUB 连上，也不会等。SUB 没连上之前发的消息**直接丢弃**（不像 PUSH/PULL 或 REQ/REP 那样排队等待）。
- **过滤在订阅端也可能在发布端**（取决于实现），但心智模型上认为"发布端广播全部，订阅端各自过滤"。
- **慢订阅者会丢消息**：如果 Subscriber 处理不过来，Publisher 不会等它，队列满了直接丢（见示例3）。

## 什么场景用 PUB/SUB

- 一个数据源，多个消费者都需要**同样的**实时数据（行情、日志、事件通知、配置变更广播）。
- 消费者可以接受"偶尔丢消息"（比如实时行情，丢一条无所谓，反正很快有下一条）。
- 不需要知道"谁收到了"、不需要请求-响应。

## 什么场景不该用

- 每条消息都不能丢 -> PUB/SUB 不提供可靠性保证，需要额外设计确认机制或换用其他 pattern。
- 需要点对点、需要确认对方收到 -> 用 REQ/REP 或 DEALER/ROUTER。

## 3 个示例

| 示例 | 场景 | 学到什么 |
|---|---|---|
| [example1_topic_filter](example1_topic_filter/) | 新闻分类广播（NEWS/SPORT/AI） | 最基础的 topic 前缀过滤订阅 |
| [example2_multi_pub_multi_sub](example2_multi_pub_multi_sub/) | 多个新闻源汇总分发 | 多 Publisher + 多 Subscriber 组网，多段消息（Topic+Payload）|
| [example3_slow_subscriber_hwm](example3_slow_subscriber_hwm/) | 慢订阅者丢消息 | HWM（High Water Mark）如何影响丢消息行为 |

每个示例目录下都有独立的 `README.md` 说明如何运行。
