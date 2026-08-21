# 示例 2：ROUTER-DEALER 中间 Broker（扩展请求-响应模式）

这是 ZMQ Guide 里经典的「Extended Request-Reply Pattern」：在 REQ 客户端和 REP Worker 之间插入一个 **Broker**（前端 ROUTER + 后端 DEALER），实现"客户端不用知道 Worker 是谁，Worker 也不用知道客户端是谁，Broker 自动做负载均衡"。

## 场景

真实系统里客户端和处理节点数量都会变化，直接让每个客户端 `connect()` 每个 Worker 不现实。引入一个**地址固定、职责单一**的 Broker：客户端只需要知道 Broker 地址，Worker 只需要知道 Broker 地址，双方数量都能独立扩缩容。

## 心智模型

```
REQ Client1 ─┐                                ┌─ REP Worker1
REQ Client2 ─┼──▶ ROUTER(前端:5568) Broker DEALER(后端:5569) ──┼─ REP Worker2
REQ Client3 ─┘                                ┴─ REP WorkerN
```

Broker 内部只做一件事：`zmq_proxy(frontend, backend, NULL)`——把前端 ROUTER 收到的消息原样转发给后端 DEALER，DEALER 天然按 Round-Robin 把它发给某个已连接的 Worker；Worker 回复时，因为 REP 会自动记住并重新贴上"信封"（client identity），消息能原路正确送回发起请求的那个 Client。

## 编译 & 运行（C）

```bash
make
./broker                 # 终端1：先起Broker
./worker 1 &             # 终端2/3：起两个Worker
./worker 2 &
./client A                # 终端4：客户端A发3个请求
./client B                # 终端5：客户端B发3个请求（可与A同时跑）
```

## 运行（Python）

```bash
python3 broker.py
python3 worker.py 1 &
python3 worker.py 2 &
python3 client.py A
python3 client.py B
```

## 预期输出（Client A，示例）

```
[Client A] Sending: req 0
[Client A] Received: Handled by worker 1: req 0
[Client A] Sending: req 1
[Client A] Received: Handled by worker 2: req 1
[Client A] Sending: req 2
[Client A] Received: Handled by worker 1: req 2
```

（具体哪个 worker 处理哪个请求取决于 Round-Robin 状态，每次运行可能不同。）

## 关键点

- Broker **完全不关心消息内容**，只做转发+负载均衡——这是"网关/代理"这一类基础设施组件的典型心智模型。
- 之所以能这样"零业务逻辑"转发，是因为 REQ 客户端和 REP Worker 各自都在消息里自动维护了一层"信封"（identity + 空分隔帧），Broker 只需要原样传递这层信封，两端协议就能自动对上。
- 如果 Worker 处理时间差异很大，某些 Worker 可能因为"总是最快确认好接收下一条"而拿到更多请求——这跟 [PUSH/PULL 的负载均衡](../../03_push_pull/example1_task_distribution/) 逻辑类似，但这里额外具备"能把结果原路送回正确客户端"的能力，这是 PUSH/PULL 做不到的。
