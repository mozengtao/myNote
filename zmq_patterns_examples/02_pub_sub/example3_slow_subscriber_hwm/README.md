# 示例 3：慢订阅者丢消息现象 + HWM

PUB/SUB **不提供可靠性保证**：Publisher 从不等待、从不重发。如果 Subscriber 处理速度跟不上，消息会在队列堆积到 **HWM（High Water Mark）** 上限后被**直接丢弃**，Publisher 完全不知情，也不会因此变慢。

这个示例故意构造这种场景：Publisher 高速发送 1000 条带序号的消息，Subscriber 故意"处理很慢"（每条消息模拟 5ms 处理耗时），同时把 HWM 调得很小，让丢消息现象在几秒内就能被肉眼观察到。

## 场景

- 实时行情、传感器数据、监控指标广播：数据量大、频率高，消费者一旦跟不上，"丢一些旧数据"通常比"阻塞生产者等消费者"更合理。
- 通过本例理解：**为什么 PUB/SUB 天生就是"最多送达一次、允许丢失"的模式**，而不是像 PUSH/PULL 或 REQ/REP 那样有队列缓冲兜底。

## 心智模型

```
Publisher (SNDHWM=20, 快速发送1000条)
     │
     │  队列(每个订阅者一份) 堆到 20 条就开始丢弃新消息
     ▼
Subscriber (故意每条sleep 5ms，处理很慢)
     │
     └─▶ 收到的序号出现跳跃 = 中间的消息被丢了
```

## HWM 是什么

- `ZMQ_SNDHWM`：发送端每个"管道"（对应一个已连接的对端）最多缓存多少条待发消息，超过后**新消息会被丢弃**（PUB 场景）或阻塞（其他 socket 类型，如 PUSH）。
- `ZMQ_RCVHWM`：接收端本地缓存队列上限，超过后同理丢弃/阻塞。
- 默认值通常是 1000。本例故意调小（比如 20），让"丢消息"现象更快、更明显地出现，不用真的发几百万条消息才能观察到。

## 编译 & 运行（C）

```bash
make
./subscriber      # 终端1：先启动订阅者（处理很慢）
./publisher       # 终端2：再启动发布者（发送很快）
```

## 运行（Python）

```bash
python3 subscriber.py
python3 publisher.py
```

## 预期输出（Subscriber，示例）

```
[Subscriber] Subscribed, RCVHWM=20, simulating slow processing (5ms/msg)
[Subscriber] recv SEQ 0
[Subscriber] recv SEQ 1
[Subscriber] *** Gap detected: expected 2, got 47 (lost 45 messages) ***
[Subscriber] recv SEQ 47
...
[Subscriber] Done. Sent=1000 (by publisher), Received=123, Lost>=877
```

**具体丢失数量每次运行会不同**（取决于机器调度、网络栈缓冲），这正说明 PUB/SUB 的丢消息行为是"尽力而为"，不是精确可控的。

## 关键点

- 想要"不丢消息"，PUB/SUB **不是正确的工具**——应该用 REQ/REP（会阻塞等待）、PUSH/PULL（队列会阻塞生产者而不是丢弃，见 [03_push_pull](../../03_push_pull/README.md)），或者在应用层加确认/重传机制。
- 调大 HWM 只能"延缓"丢消息出现的时间点，不能从根本上解决"消费者持续比生产者慢"的问题——那是架构问题（需要加 Subscriber 数量、优化处理速度，或者换用有反压的 pattern）。
