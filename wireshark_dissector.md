# Wireshark Lua Dissector 三大核心对象：Tvb、Pinfo、TreeItem

## 一、Mental Model：一次 Dissection 本质上在做什么？

一个 Lua dissector 可以理解为：

> **从字节流(Data)中读取协议字段，在当前包的上下文(Context)中解释它们，并将解析结果展示到协议树(View)中。**

整个流程可以抽象成：

```text
                Wireshark
                     │
      ┌──────────────┼──────────────┐
      │              │              │
   Packet Data   Packet Context   Packet View
      │              │              │
     Tvb           Pinfo        TreeItem
      │              │              │
      └───────┬──────┴──────┬───────┘
              │             │
              ▼             ▼
          Protocol Parser (Dissector)
```

因此：

- **Tvb**：输入（Input）
- **Pinfo**：运行环境（Context）
- **TreeItem**：输出（Output）

三者分别属于三个完全不同的世界，而不是一个 Packet 对象的三个成员。

---

# 二、Tvb —— Data World（我要解析什么）

## Why

协议解析器最重要的任务就是：

> 从字节流中提取字段。

Tvb 就是 dissector 当前能够看到的那段字节。

例如：

```
Ethernet
    ↓
IP
    ↓
TCP
    ↓
HTTP
```

HTTP dissector 收到的 tvb：

```
GET /index.html HTTP/1.1
```

它不会看到：

```
Ethernet Header
IP Header
TCP Header
```

这些已经由父 dissector 消费掉了。

因此：

> **Tvb = 当前协议的数据窗口（Logical Packet Buffer）**

不是整个 Frame。

---

## Tvb 解决的问题

回答唯一的问题：

> **我要解析哪些字节？**

---

## 常见操作

### 获取长度

```lua
tvb:len()
tvb:captured_len()
tvb:reported_len()
```

模式：

```
Packet
 │
 └── 长度是多少？
```

---

### 获取一个范围（TvbRange）

```lua
local range = tvb(offset, length)
```

例如：

```lua
local header = tvb(0,8)
```

模式：

```
Tvb
 │
 ├── Header
 ├── Payload
 └── Footer
```

---

### 提取整数

```lua
tvb(0,1):uint()

tvb(2,2):uint()

tvb(4,4):le_uint()
```

模式：

```
Bytes
 ↓
Integer
```

---

### 提取字符串

```lua
tvb(8,16):string()

tvb(8,16):stringz()
```

模式：

```
Bytes
 ↓
Text
```

---

### 提取 ByteArray

```lua
tvb(0,16):bytes()
```

模式：

```
Bytes
 ↓
Binary Object
```

---

### 获取剩余 Payload

```lua
local payload = tvb(offset)
```

表示：

```
offset
 │
 ▼
+---------------------+
| Remaining Payload   |
+---------------------+
```

---

## Tvb 的典型使用模式

几乎所有 dissector 都遵循：

```lua
offset = 0

version = tvb(offset,1):uint()
offset = offset + 1

type = tvb(offset,1):uint()
offset = offset + 1

length = tvb(offset,2):uint()
offset = offset + 2
```

这是典型的：

> **Sequential Parsing（顺序解析）**

---

# 三、Pinfo —— Context World（我正在解析谁）

## Why

解析协议不仅需要字节。

还需要知道：

- 当前包来自哪里？
- 属于哪个 TCP Stream？
- 当前协议叫什么？
- Info 列显示什么？
- 是否需要 TCP 重组？

这些都不是数据。

因此 Wireshark 把这些放进：

> **Packet Information（Pinfo）**

---

## Pinfo 解决的问题

回答：

> **当前正在解析哪个 Packet？**

它保存的是：

- Packet Metadata
- Dissection State
- Conversation State

---

## 常见成员

### Source / Destination

```lua
pinfo.src
pinfo.dst
```

模式：

```
Packet

Source
Destination
```

---

### 协议列

```lua
pinfo.cols.protocol = "MYPROTO"
```

结果：

```
Protocol

MYPROTO
```

---

### Info 列

```lua
pinfo.cols.info = "Login Request"
```

结果：

```
Info

Login Request
```

---

### TCP 重组

```lua
pinfo.desegment_len

pinfo.desegment_offset
```

告诉 TCP：

```
数据还不完整

↓

请继续拼包
```

---

### Frame 信息

例如：

```
Frame Number

Visited

Timestamp

TCP Stream
```

都属于 Pinfo。

---

## Pinfo 的典型使用模式

### 更新 UI

```lua
pinfo.cols.protocol = "ABC"

pinfo.cols.info = "Heartbeat"
```

---

### 请求 TCP Reassembly

```lua
pinfo.desegment_len = needed

pinfo.desegment_offset = offset
```

---

### 根据上下文解析

例如：

```
TCP Stream 1

↓

登录协议

TCP Stream 2

↓

文件协议
```

Context 决定解释方式。

---

# 四、TreeItem —— View World（我要展示什么）

## Why

解析完成以后，需要告诉 Wireshark：

```
Packet Details
```

应该显示什么。

TreeItem 就是：

> **Packet Details Pane 上的一棵树。**

---

## TreeItem 解决的问题

回答：

> **我要向用户展示什么？**

---

## Tree 的层次

例如：

```
MyProto

    Header

        Version

        Length

    Payload

        Command

        Data
```

就是：

```
TreeItem

└── TreeItem

    └── TreeItem
```

---

## 常见操作

### 创建协议根节点

```lua
local subtree = tree:add(proto, tvb())
```

模式：

```
Tree

↓

MyProto
```

---

### 添加字段

```lua
subtree:add(f.version, tvb(0,1))

subtree:add(f.length, tvb(2,2))
```

结果：

```
MyProto

 Version

 Length
```

---

### 创建子树

```lua
local hdr = subtree:add(f.header, tvb(0,8))
```

结果：

```
MyProto

 Header

     ...
```

---

### 修改标题

```lua
node:set_text("Header")
```

模式：

```
默认名字

↓

新的名字
```

---

### 设置长度

```lua
node:set_len(length)
```

表示：

```
这个节点覆盖哪些字节
```

---

### 添加专家信息（Expert Info）

```lua
node:add_expert_info(...)
```

结果：

```
⚠ Warning

Malformed Packet
```

---

## Tree 的典型模式

几乎所有 dissector：

```lua
root

↓

Protocol

↓

Header

↓

Fields

↓

Payload
```

即：

```
Root

└── Protocol

    ├── Header

    │    ├── Version

    │    └── Length

    └── Payload
```

---

# 五、三者之间的关系

可以理解为 MVC：

```
             Packet

                │

    ┌───────────┼───────────┐

    │           │           │

  Data      Context      Display

    │           │           │

   Tvb       Pinfo     TreeItem
```

或者：

```
               Parser

Tvb --------------------------->

           读取字段

Pinfo ------------------------->

           提供上下文

TreeItem <----------------------

           输出解析结果
```

更完整一点：

```
                 Packet

                    │

         +----------+-----------+

         │                      │

      Packet Bytes        Packet Metadata

         │                      │

         ▼                      ▼

        Tvb                  Pinfo

         │                      │

         └────────┬─────────────┘

                  ▼

          Protocol Dissector

                  │

                  ▼

             TreeItem

                  │

                  ▼

        Packet Details Pane
```

---

# 六、三者的职责总结

| 对象 | 心智模型 | 解决的问题 | 常用操作 | 输出 |
|------|----------|------------|----------|------|
| **Tvb** | Data（输入） | 我要解析哪些字节？ | `tvb(offset,len)`、`uint()`、`string()`、`bytes()`、`len()` | 协议字段值 |
| **Pinfo** | Context（环境） | 当前解析的是哪个 Packet？ | `cols.protocol`、`cols.info`、`src`、`dst`、`desegment_len` | Packet 元信息、TCP 重组状态、UI 列信息 |
| **TreeItem** | View（展示） | 我要向用户展示什么？ | `add()`、`add_packet_field()`、`set_text()`、`set_len()` | Packet Details 协议树 |

---

# 七、一个完整 Dissector 的固定思维流程

几乎所有 Lua dissector 都可以套入下面这个模板：

```
Dissector()

│
├── ① Tvb
│      │
│      ├── 检查长度
│      ├── 顺序解析字段
│      └── 提取 Payload
│
├── ② Pinfo
│      │
│      ├── 设置 Protocol 列
│      ├── 设置 Info 列
│      ├── 根据上下文解析
│      └── 必要时请求 TCP 重组
│
└── ③ TreeItem
       │
       ├── 创建协议节点
       ├── 创建 Header 子树
       ├── 添加字段
       ├── 添加 Payload
       └── 添加 Expert Info
```

因此，可以把整个 Wireshark Lua dissector 浓缩成一句话：

> **Tvb 决定“读什么”（Data）；Pinfo 决定“如何理解”（Context）；TreeItem 决定“如何展示”（View）。一个 dissector 的工作，就是从 Tvb 中解析协议字段，借助 Pinfo 提供的上下文进行解释，再通过 TreeItem 构建最终呈现给用户的协议树。**