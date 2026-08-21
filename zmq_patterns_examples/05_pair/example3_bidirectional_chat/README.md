# 示例 3：双向全双工聊天 Demo

一个真正"能玩"的例子：两个终端窗口互相聊天，输入一行文字回车发送，同时能随时收到对方发来的消息——不需要区分"谁是客户端谁是服务端"，因为 PAIR 两端完全对等。

## 场景

直观感受 PAIR "没有角色区分"这个心智模型：如果用 REQ/REP 做聊天，你会发现严格交替的限制让"双方同时都想说话"这种最自然的聊天场景根本没法实现；PAIR 天生就没有这个限制。

## 心智模型

```
Peer A (bind)  ◀─────────────────▶  Peer B (connect)
   │  可以随时发，也可以随时收              │  同样，可以随时发，也可以随时收
   │  用 zmq_poll 同时监听：                │
   │    - stdin（用户输入）                 │
   │    - PAIR socket（对方消息）           │
```

技术上的关键点：需要**同时监听两个输入源**——键盘输入（stdin）和网络消息（PAIR socket）。用 `zmq_poll` 把 `stdin` 的文件描述符和 ZMQ socket 放在一起监听，就能做到"谁先来数据就处理谁"，这是实现"交互式网络程序"的标准写法。

## 编译 & 运行（C）

```bash
make
./peer_a       # 终端1
./peer_b       # 终端2
```

在任意一个终端输入文字回车，另一个终端会实时显示。输入 `/quit` 退出。

## 运行（Python）

```bash
python3 peer_a.py
python3 peer_b.py
```

## 预期效果

```
终端1 (Peer A)                          终端2 (Peer B)
[PeerA] Connected. Type messages,       [PeerB] Connected. Type messages,
below /quit to exit.                    below /quit to exit.
> hello from A                          [A]: hello from A
                                         > hi, got it
[B]: hi, got it
> /quit                                 [A]: /quit
(退出)                                   (对方退出，本端也可继续或手动退出)
```

## 关键点

- 如果把这个例子改成 REQ/REP，你会发现：A 发一句话之后必须先等 B 回一句才能再发，B 也是同理——完全无法实现"我连续说两句"或"双方同时打字"的自然聊天体验，这正是 PAIR 存在的意义。
- 本例是 PAIR **跨进程/跨网络**使用的示例，但官方文档仍建议：如果不是纯粹的"1对1、无需负载均衡、无需多连接管理"场景，跨网络更推荐 DEALER/ROUTER 或 REQ/REP，因为它们有更完善的重连、多连接等特性。
