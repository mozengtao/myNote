# 示例 2：结构化计算服务（多部分消息）

一个更"真实"的 RPC：客户端用**多部分消息（multipart）**发送结构化请求 `[operator, operand1, operand2]`，服务端解析、计算，返回结果或错误信息。

## 场景

真实业务里请求从来不是一个裸字符串，通常需要拆分成"操作类型 + 参数"。这个示例演示：

- 用 `ZMQ_SNDMORE` 发送多段消息（对应第十章"多段消息"）。
- 服务端如何用 `ZMQ_RCVMORE` 循环接收所有分段。
- 出错时如何设计响应协议（`OK|<result>` / `ERR|<reason>`），而不是让协议本身崩掉。

## 心智模型

```
Client                          Server
  │  send("div", SNDMORE)          │
  │  send("10",  SNDMORE)          │
  │  send("0")                     │
  │─────────────────────────────▶  │  recv 3 frames
  │                                │  10 / 0 -> 除零
  │◀───────────────────────────    │  send("ERR|division by zero")
```

## 支持的操作

- `add a b`  ->  `OK|a+b`
- `sub a b`  ->  `OK|a-b`
- `mul a b`  ->  `OK|a*b`
- `div a b`  ->  `OK|a/b`，若 b=0 返回 `ERR|division by zero`
- 未知操作   -> `ERR|unknown operator: xxx`

## 编译 & 运行（C）

```bash
make
./server        # 终端1
./client        # 终端2
```

## 运行（Python）

```bash
python3 server.py   # 终端1
python3 client.py   # 终端2
```

## 预期输出（Client）

```
[Client] add 3 5 -> OK|8
[Client] div 10 0 -> ERR|division by zero
[Client] mod 7 2 -> ERR|unknown operator: mod
```

## 关键点

- Request 和 Reply 都是**独立的多帧消息**，但这个示例里 Reply 只用了单帧（`OK|8`），因为响应结构更简单；多帧不是必须对称的。
- 服务端必须把一个请求的**所有分段全部收完**才能再 `send`，否则状态机会出错。
