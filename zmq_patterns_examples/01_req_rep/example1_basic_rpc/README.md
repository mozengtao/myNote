# 示例 1：基础 Ping-Pong RPC

最简单的 REQ/REP：客户端连续发 5 个 "Hello N"，服务端每次回 "World N"。

## 场景

模拟一个最基础的 RPC 调用：客户端发出请求，等待服务端处理完并返回结果，然后再发下一个请求。

## 心智模型

```
Client (REQ)                    Server (REP)
     │                                │
     │  connect                       │  bind
     │───── "Hello 0" ───────────────▶│
     │◀──── "World 0" ────────────────│
     │───── "Hello 1" ───────────────▶│
     │◀──── "World 1" ────────────────│
     │            ... x5 ...          │
```

## 编译 & 运行（C）

```bash
make
# 终端 1
./server
# 终端 2
./client
```

## 运行（Python）

```bash
# 终端 1
python3 server.py
# 终端 2
python3 client.py
```

C 和 Python 两端可以任意混搭（比如 `./server` + `python3 client.py`），因为协议是语言无关的。

## 预期输出

Server：

```
[Server] Listening on tcp://*:5555
[Server] Received: Hello 0
[Server] Received: Hello 1
...
```

Client：

```
[Client] Sending: Hello 0
[Client] Received reply: World 0
[Client] Sending: Hello 1
[Client] Received reply: World 1
...
```

## 关键点

- `zmq_send`/`zmq_recv` 必须严格交替，否则会返回 `EFSM`（错误状态机）错误。
- REQ 端一旦 `connect`，即使 server 还没起来，请求也会先进队列，server 起来后自动发出（第十二章"常见错误③"提到的现象）。
