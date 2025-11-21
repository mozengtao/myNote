> 远程过程调用（RPC - Remote Procedure Call）是一种分布式计算的通信协议，它允许程序调用另一个地址空间（通常在另一台机器上）的函数或方法，就像调用本地函数一样简单

- RPC 让你可以像调用本地函数一样调用远程服务器上的函数
```
# 本地调用
result = calculate(5, 10)

# RPC 调用（看起来一样，但实际在远程服务器执行）
result = remote_service.calculate(5, 10)  # 实际在另一台机器上运行
```

- 工作原理
```
客户端                      网络                        服务器
   |                         |                            |
   | 1. 调用远程函数          |                            |
   |------------------------>|                            |
   |                         | 2. 发送请求                 |
   |                         |--------------------------->|
   |                         |                            | 3. 执行函数
   |                         |                            |
   |                         | 4. 返回结果                 |
   |                         |<---------------------------|
   | 5. 接收结果              |                            |
   |<------------------------|                            |
```

- example code
  assets/rpc/README_RPC.md
  assets/rpc/rpc_client.py
  assets/rpc/rpc_demo.py
  assets/rpc/rpc_server.py
  assets/rpc/RPC使用指南.md

  - gRPC example
      assets/rpc/gRPC/*

## gRPC
- 简化的层次模型
```
gRPC(应用层)
------------------------
Proto Buffers(Protobuf)(表示层)
------------------------
HTTP/2(传输层)
------------------------
TCP(网络层)
------------------------
Socket(链路层)
------------------------

序列化:将数据结构或对象转换成二进制序列的过程
反序列化:将二进制序列恢复成数据结构或对象的过程

网络传输和文件存储只能处理二进制数据，而不能直接处理复杂的数据结构
socket发送和接收的是二进制流，而不是消息流，因此发送方需要将结构化数据进行序列化之后通过socket进行发送，接收端需要反序列化二进制流进行结构化数据的恢复

// test.proto
message SimpleMessage {
   int32 id = 1;
   string name = 2;
}

// SimpleMessage 消息对象
id: 300
name: "AB"

// 序列化过程
Protobuf 使用 TLV 编码结构 (Tag-Length-Value)
Tag:
   (field_number << 3) | wire_type
      field_number: 字段编码 (例子中,id是1, name是2)
      wire_type:编码类型(0=Varint, 2=长度分割如字符串)
Length:
   值的字节长度(仅用于字符串)
Value:
   字段的实际数据

示例:
       id 字段                         name 字段
      /      \                       /      \
+-----+--------+--------+     +-----+-----+------+------+
| 08  |   AC   |  02    |     | 12  | 02  |  41  |  42  |
+-----+--------+--------+     +-----+-----+------+------+
  Tag   Varint  Varint          Tag  Length  'A'   'B'
       值(300) 的继续                 (2)     (值 "AB")


对于 id: 300 的编码
Tag = (1 << 3) | 0 = 8
Value 采用 Varint 编码， 即用可变长字节表示整数的方法，每个字节的最高位(MSB)是标志位(1表示还有后续字节, 0表示结束)，低7位是有效负载
300 的二进制: 1 0010 1100
按7位分组: 0000010 0101100
添加MSB标志位:
   第一字节: 1010 1100  (0xAC)
   第二字节: 0000 0010  (0x02)
因此 300 的 Varint 编码是 0xAC 0x02

SimpleMessage 消息对象
   id: 300
   name: "AB"
对应的最终二进制流为: 0x08 0xAC 0x02 0x12 0x02 0x41 0x42

Socket提供的是面向流的字节流服务，意味着:
1.消息边界不保留: 多个 send 操作发送的数据可能在接收端被合并为一个
2.数据可能被拆分: 一个 send 操作发送的数据可能被拆分成多个数据包到达接收端
3. 没有天然的'消息'概念: Socket只关心字节的可靠传输，不关心这些字节的语义边界

示例: 发送两个 Protobuf 消息
// 发送方
message1 = serialize(SimpleMessage(id=1, name="Alice"))  # 假设序列化后是 10 字节
message2 = serialize(SimpleMessage(id=2, name="Bob"))    # 假设序列化后是 8 字节

socket.send(message1)   # 第一次发送
socket.send(message2)   # 第二次发送

接收方可能的情况：
1.理想情况
recv1 收到: 完整的 message1 数据 (10 字节)
recv2 收到: 完整的 message2 数据 (8 字节)

2.数据合并
recv1 收到: message1 的前 5 字节
recv2 收到: message1 的后 5 字节 + 完整的 message2 (5 + 8 = 13)

3.数据拆分
recv1 收到: 完整的 message1 + message2 的前 3 字节 (10 +3 = 13)
recv2 收到: message2 的后 5 字节

由于 socket 不保留消息边界, 因此必须在应用层自己处理消息的边界， gRPC/HTTP2 采用 长度前缀法 进行处理
// 发送方 在消息前面加上长度信息
data = serialize(SimpleMessage(id=1, name="Alice"))
length = len(data).to_bytes(4, 'big')  # 4 字节表示长度
socket.sendn(length + data)   # 先发送长度，再发送数据

// 接收方先读取4字节获取长度，然后读取指定长度的数据
lenght_data = socket.recv(4)
length = int.from_bytes(length_data, 'big')
message_data = socket.recv(length)
message = deserialize(message_data)

gRPC/HTTP2的实际做法
gRPC基于HTTP/2，而 HTTP/2 使用帧(Frame)的概念
1.每个帧都有长度前缀: 帧头包含3字节的长度字段
2.多路复用: 一个TCP连接上可以交错传输多个流的帧
3.明确的边界: 每个帧都有明确的长度，接收方可以正确解析

[HTTP2 Frame]
+---------------+
| Length (24位) |  --> 告诉接收方这个帧有多长
| Type          |  --> 帧类型（HEADERS, DATA等）
| Flags         |  --> 标志位
| Stream ID     |  --> 流标识符
| Frame Payload |  --> 实际的数据（长度由Length字段指定）
+---------------+
```

## Socket 数据传输流程
   assets/Socket数据传输流程.md
   assets/socket_demo.c