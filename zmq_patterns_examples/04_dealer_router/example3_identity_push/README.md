# 示例 3：基于 Identity 的服务端主动推送

ROUTER 天生知道每个连接的身份（Identity），因此可以在**没有收到任何请求**的情况下，主动往指定的客户端推送消息——这是 REP/PULL 做不到的（它们只能"被问了才回答"）。这个示例模拟一个通知服务器：客户端先注册自己的名字，服务端之后可以随时按名字主动推送通知。

## 场景

服务端主动通知的场景非常常见：后台任务完成通知、实时告警推送、游戏服务器广播事件给指定玩家等——本质都是"服务端知道该发给谁，且不需要对方先问"。

## 心智模型

```
DEALER Client "A" ──connect+REGISTER A──▶┐
DEALER Client "B" ──connect+REGISTER B──▶┼── ROUTER Server
                                          │     维护: A -> identity_A
                                          │           B -> identity_B
                                          │
                                          │  每隔2秒主动推送通知给A或B
                                          └──────────────────────────▶ 对应客户端
```

关键心智：ROUTER 每次 `recv()` 得到的第一帧就是"发送者身份"，这个身份**在整个连接生命周期内不变**；服务端只要把它存起来，之后随时可以拿出来当作"回复的目标地址"去 `send()`，即使这次 `send` 并不是在回应某次 `recv`。

## 编译 & 运行（C）

```bash
make
./server            # 终端1：先起服务端
./client A &        # 终端2：客户端A注册
./client B &        # 终端3：客户端B注册
```

## 运行（Python）

```bash
python3 server.py
python3 client.py A &
python3 client.py B &
```

## 预期输出（Server，示例）

```
[Server] ROUTER listening on tcp://*:5570
[Server] Registered client: A
[Server] Registered client: B
[Server] Pushing notification to A: "System update #0"
[Server] Pushing notification to B: "System update #1"
[Server] Pushing notification to A: "System update #2"
...
```

预期输出（Client A）：

```
[Client A] Registered with server
[Client A] Notification: System update #0
[Client A] Notification: System update #2
...
```

## 关键点

- 客户端的"注册"消息只是**应用层协议**（不是 ZMQ 强制要求的），目的是让服务端把"业务名字"（比如用户名、设备ID）和 ZMQ 底层自动分配的 identity 关联起来。
- 服务端用 `zmq_poll` 加超时而不是纯阻塞 `zmq_recv`，才能"既能处理收到的注册消息，又能按时间到点主动推送"——这是实现"双向异步"的标准写法。
- 如果客户端断线重连，ZMQ 通常会分配一个新的 identity（除非客户端显式设置了固定 identity），服务端需要重新收到 REGISTER 消息才能更新映射——这是使用这种模式时要注意的坑。
