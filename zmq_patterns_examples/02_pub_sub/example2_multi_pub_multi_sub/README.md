# 示例 2：多发布者 + 多订阅者（多段消息）

两个独立的新闻源（Source A、Source B）各自广播，一个订阅者同时连接两个源，只订阅 `AI` 分类，把两个源的 `AI` 新闻汇总打印。消息用 **3 帧的多部分消息** `[topic, source, headline]` 表达结构化数据。

## 场景

真实系统里数据源往往不是唯一的（比如多个交易所行情、多个机房日志），订阅端经常需要**同时连接多个 Publisher**，ZMQ 的 SUB socket 天然支持一个 socket `connect()` 多个地址。

## 心智模型

```
Source A (bind :5559) ──┐
                          ├──▶ Subscriber (SUB connect 两个地址，订阅 "AI")
Source B (bind :5560) ──┘
```

- 一个 SUB socket 可以 `connect()` 任意多个 PUB 端点，ZMQ 自动把它们的消息合并到同一个接收队列。
- Subscriber 完全不需要区分"消息是从哪个连接来的"，除非业务字段里自己带上来源信息（本例中 `source` 帧就是业务自己加的）。

## 消息格式（3 帧）

```
Frame 0: topic     例如 "AI"
Frame 1: source    例如 "SourceA"
Frame 2: headline  例如 "AI headline #3"
```

**订阅过滤只作用于 Frame 0**（第一帧），这是 ZMQ SUB 前缀匹配的规则。

## 编译 & 运行（C）

```bash
make
./publisher_a      # 终端1
./publisher_b      # 终端2
./subscriber       # 终端3：连接 A 和 B，只订阅 AI
```

## 运行（Python）

```bash
python3 publisher_a.py
python3 publisher_b.py
python3 subscriber.py
```

## 预期输出（Subscriber）

```
[Subscriber] Subscribed to topic: AI (from both Source A and Source B)
[Subscriber] [SourceA] AI headline #2
[Subscriber] [SourceB] AI headline #1
[Subscriber] [SourceA] AI headline #5
...
```

## 关键点

- 一个 SUB socket 聚合多个 PUB 源，是 PUB/SUB **扇入（fan-in）** 的典型用法。
- 多段消息里，只有第一帧参与 Subscribe 过滤，设计协议时要把"用于路由/过滤的字段"放在第一帧。
