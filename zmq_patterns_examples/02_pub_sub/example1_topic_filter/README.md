# 示例 1：主题过滤广播

一个 Publisher 持续广播三类消息：`NEWS`、`SPORT`、`AI`。Subscriber 只订阅 `AI`，验证"发布端广播全部，订阅端自己过滤"的心智模型。

## 场景

模拟一个新闻推送系统：后端持续产生各类新闻，客户端只关心自己感兴趣的分类。

## 心智模型

```
Publisher                     Subscriber (只订阅 "AI")
   │  send("NEWS: ...")            │
   │  send("SPORT: ...")           │  (自动过滤掉，不会到达应用层)
   │  send("AI: ...")              │
   │───────────────────────────▶   │  只收到 AI 开头的消息
```

`zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "AI", 2)` 是**前缀匹配**：只要消息的字节内容以 `"AI"` 开头就会被这个 Subscriber 收到。

## 编译 & 运行（C）

```bash
make
./publisher       # 终端1：先启动 Publisher（bind）
./subscriber      # 终端2：再启动 Subscriber（connect）
```

## 运行（Python）

```bash
python3 publisher.py
python3 subscriber.py
```

## 预期输出（Subscriber）

```
[Subscriber] Subscribed to topic: AI
[Subscriber] AI: message 2
[Subscriber] AI: message 5
[Subscriber] AI: message 8
...
```

（NEWS/SPORT 消息不会出现，因为被订阅过滤掉了。）

## 关键点

- **先 bind 再 connect** 并不是强制的，但 Subscriber 连接建立需要一点时间；如果 Subscriber 启动太晚，Publisher 已经发出去的消息会直接丢弃（PUB/SUB 无缓冲重放）。示例里 Publisher 会持续不断地发消息，所以不用担心错过全部——但一开始的几条很可能确实收不到，这正好演示了 PUB/SUB "no history" 的特性。
- 换成 `ZMQ_SUBSCRIBE, "", 0`（空字符串）就是订阅**全部**主题。
