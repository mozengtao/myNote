# ZeroMQ（ZMQ）通信核心概念、用法与心智模式（C语言）

> 适合已经具有 Socket 编程基础，希望理解 ZMQ 本质，而不是死记 API 的开发者。

---

# 一、先建立正确的心智模式

很多人第一次接触 ZMQ，会犯一个错误：

> **把 ZMQ 当成 Socket 库。**

实际上：

> **ZeroMQ = Socket API + 通信框架 + 消息队列 + 网络中间件。**

如果 Linux Socket 是：

```
read/write
```

那么 ZMQ 更像：

```
send message
receive message
```

注意：

**ZMQ 操作的是 Message，不是 Byte Stream。**

这是第一层心智模式。

---

## Linux Socket

```
Application
     │
 send()/recv()
     │
    TCP
     │
 Network
```

应用必须自己处理：

- 半包
- 粘包
- reconnect
- retry
- queue
- 多线程同步

几乎所有事情都要自己写。

---

## ZMQ

```
Application
      │
 zmq_send()
 zmq_recv()
      │
+----------------+
| ZeroMQ Engine  |
|                |
| Queue          |
| Retry          |
| IO Thread      |
| Routing        |
| Load Balance   |
| Reconnect      |
+----------------+
      │
 TCP / IPC / INPROC
```

ZMQ 已经替你实现了：

- Queue
- Buffer
- IO线程
- 自动重连
- 消息边界
- 多连接管理

所以：

> **Socket负责"传输字节"，ZMQ负责"传输消息"。**

---

# 二、ZeroMQ 的三层对象

很多人 API 一学就乱。

其实只有三个对象。

```
Context
    │
 Socket
    │
 Message
```

---

## 第一层：Context

Context 是整个 ZMQ 世界。

```
zmq_ctx_new();
```

可以理解成：

```
Kernel of ZeroMQ
```

里面包含：

```
IO Thread

Memory Pool

Timer

Global Queue

Socket Registry
```

所有 socket 都属于某个 context。

例如：

```
Context

 ├── socket1

 ├── socket2

 ├── socket3
```

程序通常：

```
一个进程
↓

一个 Context
```

即可。

不要每个 socket 一个 Context。

---

# 第二层：Socket

注意：

ZMQ Socket 不是 Linux Socket。

Linux：

```
int fd
```

ZMQ：

```
zmq_socket()
```

得到的是：

```
Messaging Endpoint
```

它里面可能连接：

```
1 个 TCP

10 个 TCP

100 个 TCP

1000 个 TCP
```

例如：

```
Publisher

        Socket

       /  |   \

 TCP TCP TCP
```

一个 Socket 可以管理很多连接。

这是第二个重要心智模式。

---

# 第三层：Message

ZMQ 永远发送 Message。

例如：

```
Hello
```

发送的是：

```
+--------+
 Length
+--------+
 Payload
+--------+
```

不是：

```
char*
```

所以不会：

- 粘包
- 半包

这是最大的优势。

---

# 三、ZMQ 为什么不用 connect/send？

Linux：

```
socket()

connect()

send()
```

ZMQ：

```
socket()

connect()

send message
```

原因：

ZMQ 中：

Socket ≠ TCP连接

Socket 是：

```
Application Queue
```

真正 TCP 连接由后台 IO Thread 建立。

所以：

```
zmq_connect()

↓

只是告诉后台：

以后建立连接。
```

真正连接可能：

```
100ms 后

500ms 后

5 秒后
```

因此：

```
connect()

并不会阻塞。
```

---

# 四、ZMQ 最大特色：异步

Linux：

```
send()

↓

进入 TCP

↓

等待
```

ZMQ：

```
send()

↓

放进 Queue

↓

立刻返回

↓

IO Thread

↓

真正发送
```

即：

```
Application

↓

Queue

↓

Background Thread

↓

Network
```

所以：

发送速度通常很快。

---

# 五、IO Thread 是什么？

创建 Context 时：

```
zmq_ctx_new()
```

默认：

```
1 IO Thread
```

这个线程负责：

```
TCP

Reconnect

Polling

Write

Read

Heartbeat

Routing
```

应用线程：

```
只负责：

send()

recv()
```

真正网络线程：

```
IO Thread
```

所以：

应用线程几乎不用碰 epoll。

---

# 六、Queue 是 ZMQ 的灵魂

每个 Socket 都有 Queue。

例如：

```
Producer

↓

Queue

↓

TCP
```

网络慢：

```
Producer

↓↓↓↓↓↓↓↓

Queue越来越长
```

网络恢复：

```
Queue

↓↓↓↓↓

全部发送
```

因此：

ZMQ 天然具有：

```
Buffer
```

---

## HWM（High Water Mark）

Queue 不可能无限大。

所以：

```
SNDHWM

RCVHWM
```

例如：

```
1000 Message
```

超过以后：

可能：

```
阻塞

或者

丢弃
```

取决于 Socket 类型。

---

# 七、通信模式（Pattern）

真正理解 ZMQ，

其实就是理解各种 Pattern。

最重要的五种。

---

# 1）REQ / REP

请求响应。

```
Client

REQ

↓

REP

Server
```

必须：

```
Send

↓

Recv

↓

Send

↓

Recv
```

严格交替。

不能：

```
Send

Send
```

否则：

```
EFSM Error
```

---

典型代码：

Client：

```c
void *ctx = zmq_ctx_new();

void *req = zmq_socket(ctx, ZMQ_REQ);

zmq_connect(req, "tcp://127.0.0.1:5555");

zmq_send(req, "Hello", 5, 0);

char buf[100];

zmq_recv(req, buf, sizeof(buf), 0);
```

Server：

```c
void *ctx = zmq_ctx_new();

void *rep = zmq_socket(ctx, ZMQ_REP);

zmq_bind(rep, "tcp://*:5555");

char buf[100];

zmq_recv(rep, buf, sizeof(buf), 0);

zmq_send(rep, "World", 5, 0);
```

---

# 心智模式

```
HTTP

↓

REQ

↓

Server

↓

REP
```

就是 RPC。

---

# 2）PUB / SUB

发布订阅。

```
Publisher

↓

↓

↓

Subscriber1

Subscriber2

Subscriber3
```

Publisher：

不知道：

谁订阅。

Subscriber：

决定：

看哪些 Topic。

例如：

```
NEWS

SPORT

AI
```

订阅：

```
AI
```

只收到：

```
AI
```

---

# 心智模式

像：

```
微信群广播。
```

发消息的人：

不知道：

谁在看。

---

# 3）PUSH / PULL

流水线。

```
Producer

↓

↓

↓

Worker1

Worker2

Worker3
```

自动：

Round Robin。

例如：

```
Task1

↓

Worker1

Task2

↓

Worker2

Task3

↓

Worker3
```

---

适合：

```
任务分发

日志处理

计算集群
```

---

# 心智模式

像：

```
工厂流水线。
```

---

# 4）DEALER / ROUTER

这是高级版本。

REQ/REP：

限制：

```
Send

Recv

Send

Recv
```

Dealer：

没有限制。

Router：

维护：

```
Client Identity
```

例如：

```
ClientA

ClientB

ClientC
```

Router：

知道：

```
A

B

C
```

所以：

可以：

```
主动推送。
```

---

# 心智模式

Router：

像：

```
交换机。
```

知道：

每个端口是谁。

---

# 5）PAIR

仅：

```
1 对 1
```

一般：

```
线程通信
```

很少用于网络。

---

# 八、bind 和 connect 怎么理解？

很多人搞反。

实际上：

谁 bind 都行。

真正原则：

```
稳定的一端

↓

bind

变化的一端

↓

connect
```

例如：

服务器：

```
bind()
```

客户端：

```
connect()
```

因为：

服务器地址固定。

---

# 九、Transport

ZMQ 支持：

```
tcp://

ipc://

inproc://

pgm://

epgm://
```

最常用：

```
tcp://
```

---

## TCP

```
tcp://192.168.1.10:5555
```

跨机器。

---

## IPC

```
ipc:///tmp/demo.sock
```

同一机器。

Unix Domain Socket。

---

## INPROC

```
inproc://worker
```

同一 Context 内。

最快。

零拷贝。

---

# 十、多段消息（Multipart Message）

例如：

```
Topic

Payload
```

发送：

```c
zmq_send(sock, topic, len, ZMQ_SNDMORE);

zmq_send(sock, data, len, 0);
```

接收：

```
Recv()

↓

Recv()
```

组成：

```
Message

├── Part1

└── Part2
```

很适合：

```
Header

Body
```

---

# 十一、常见生命周期（C语言）

```c
void *ctx = zmq_ctx_new();

void *sock = zmq_socket(ctx, ZMQ_REQ);

zmq_connect(sock, "...");

zmq_send(...);

zmq_recv(...);

zmq_close(sock);

zmq_ctx_destroy(ctx);
```

顺序：

```
Context

↓

Socket

↓

Connect

↓

Send

↓

Recv

↓

Close

↓

Destroy
```

---

# 十二、常见错误

## ① 一个线程同时操作同一个 Socket

错误：

```
Thread1

↓

Socket

↑

Thread2
```

原则：

> **一个 Socket 只能由一个线程拥有。**

多线程：

每个线程一个 Socket。

---

## ② 一个 Socket 多次 connect()

其实可以。

```
Socket

├── Server1

├── Server2

└── Server3
```

ZMQ 自动：

```
Load Balance
```

---

## ③ connect 前发送？

可以。

```
send()

↓

Queue

↓

以后建立连接

↓

发送
```

这是 ZMQ 很神奇的地方。

---

## ④ bind 后没人连接？

没关系。

```
Queue

↓

等待客户端
```

---

# 十三、完整心智模型（建议牢记）

```
                    +-----------------------------+
                    |         Context             |
                    |  (整个 ZeroMQ 运行时环境)     |
                    +--------------+--------------+
                                   |
                        管理多个 Socket
                                   |
        +--------------------------+--------------------------+
        |                          |                          |
   +-----------+             +-----------+             +-----------+
   |  Socket   |             |  Socket   |             |  Socket   |
   | (消息端点) |             | (消息端点) |             | (消息端点) |
   +-----+-----+             +-----+-----+             +-----+-----+
         |                         |                         |
    内部消息队列               内部消息队列             内部消息队列
         |                         |                         |
         +------------ IO Thread（后台网络线程） ------------+
                                   |
                          TCP / IPC / INPROC
                                   |
                              对端 Socket
                                   |
                                Message
```

---

# 十四、一句话总结

- **Context**：ZeroMQ 的运行时环境，管理所有资源与 I/O 线程。
- **Socket**：消息端点，不等同于 TCP 连接，可同时管理多个连接。
- **Message**：通信的基本单位，天然具有消息边界，无需处理粘包/半包。
- **Queue**：每个 Socket 自带发送/接收队列，实现异步、缓冲与自动重连。
- **Pattern**：ZMQ 的真正价值所在，通过 REQ/REP、PUB/SUB、PUSH/PULL、DEALER/ROUTER 等模式表达不同通信关系。
- **心智模式**：**把 ZeroMQ 看成“消息中间件 + 通信框架”，而不是 Socket API 的简单封装。**