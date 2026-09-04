# Wireshark 解析 Packet 的通用流程与心智模型

无论是 Ethernet、DOCSIS、PPP、USB、CAN、802.11、Bluetooth，Wireshark 的解析流程本质上都是一致的。

可以把整个过程理解为：

```
                +----------------------+
                |      Capture File    |
                |   (*.pcap/*.pcapng)  |
                +----------+-----------+
                           |
                           | 读取 Global Header
                           |
                           ▼
                 获取 Link-Layer Type (DLT)
                           |
                           ▼
             选择第一个 Dissector（入口协议）
                           |
                           ▼
                  开始解析 Packet Bytes
                           |
                           ▼
                每层协议决定下一层协议
                           |
                           ▼
             构建 Protocol Tree（协议树）
                           |
                           ▼
      Packet List / Packet Details / Hex View
```

整个过程可以浓缩成一句话：

> **LinkType 决定从哪里开始解析；每一层协议决定下一层协议；最终生成 Protocol Tree。**

---

# 第一阶段：读取 Capture File

Wireshark 首先不会解析协议。

它先读取：

```
pcap
│
├── Global Header
├── Packet Header
└── Packet Data
```

例如：

```
+----------------+
| Global Header  |
+----------------+
| LinkType=1     |   <-- Ethernet
+----------------+
| Packet #1      |
+----------------+
| Packet #2      |
+----------------+
```

其中最重要的是：

```
LinkType（DLT）
```

例如：

```
DLT_EN10MB
```

表示：

```
这是 Ethernet 抓包
```

又例如：

```
DLT_DOCSIS
```

表示：

```
这是 DOCSIS 抓包
```

注意：

> LinkType 只是告诉 Wireshark **应该先调用哪个 dissector**。

它**不会证明数据一定是合法协议**。

---

# 第二阶段：根据 LinkType 找入口 Dissector

Wireshark 内部维护一张映射表：

```
LinkType
      │
      ▼
Dissector
```

例如：

```
DLT_EN10MB
        │
        ▼
Ethernet dissector
```

或者

```
DLT_DOCSIS
        │
        ▼
DOCSIS dissector
```

或者

```
DLT_USB
      │
      ▼
USB dissector
```

所以真正开始工作的第一个解析器叫：

```
Entry Dissector
```

它就是协议树的根。

---

# 第三阶段：解析 Header

假设：

```
LinkType = Ethernet
```

Wireshark 就会认为：

```
offset 0
│
▼
Ethernet Header
```

于是开始读取：

```
Destination MAC
Source MAC
EtherType
```

例如：

```
00 11 22 33 44 55
66 77 88 99 aa bb
08 00
```

解析得到：

```
Ethernet
├── Dst MAC
├── Src MAC
└── EtherType = IPv4
```

注意：

这里还不知道 IP。

它只是知道：

```
EtherType
```

---

# 第四阶段：决定下一层协议

Ethernet Header 最后两个字节：

```
EtherType
```

例如：

```
0800
```

Wireshark 查询：

```
0800
   │
   ▼
IPv4 dissector
```

于是：

```
Ethernet
        │
        ▼
IPv4
```

如果：

```
86DD
```

就变成：

```
Ethernet
       │
       ▼
IPv6
```

如果：

```
0806
```

就是：

```
ARP
```

所以：

> 每一层协议都会决定下一层协议是谁。

---

# 第五阶段：递归调用 Dissector

解析 IPv4：

```
IPv4 Header
```

里面有：

```
Protocol
```

例如：

```
6
```

表示：

```
TCP
```

于是：

```
IPv4
    │
    ▼
TCP
```

TCP 又有：

```
Dst Port
```

例如：

```
80
```

于是：

```
TCP
   │
   ▼
HTTP
```

整个调用过程：

```
Ethernet
      │
      ▼
IPv4
      │
      ▼
TCP
      │
      ▼
HTTP
```

---

# Wireshark 内部真正的工作方式

整个解析过程其实就是：

```
Dissector A
      │
      │ 解析 Header
      │
      ▼
找到 Next Protocol
      │
      ▼
调用 Dissector B
```

例如：

```
Ethernet dissector

↓

发现 EtherType=0800

↓

调用 IPv4 dissector

↓

发现 Protocol=6

↓

调用 TCP dissector

↓

发现 Port=80

↓

调用 HTTP dissector
```

所以：

> Wireshark 本质上就是一个 **Dissector 调度器（Dispatcher）**。

---

# Protocol Tree 是如何构建的？

每个 dissector 都会向树中添加节点。

例如：

```
Ethernet
├── Destination
├── Source
└── Type
```

IPv4 dissector：

```
Ethernet
│
└── IPv4
    ├── Version
    ├── TTL
    └── Protocol
```

TCP：

```
Ethernet
│
└── IPv4
    │
    └── TCP
        ├── Src Port
        ├── Dst Port
        └── Flags
```

最终得到：

```
Frame
└── Ethernet
    └── IPv4
        └── TCP
            └── HTTP
```

这就是 Packet Details。

---

# 如果解析失败，会发生什么？

例如：

```
LinkType = Ethernet
```

但是：

```
Packet Bytes

01 02 03 04 ...
```

并不是合法 Ethernet Header。

那么：

```
Ethernet dissector
        │
        ▼
无法识别 Header
        │
        ▼
停止解析
```

Protocol Tree：

```
Frame
```

不会继续。

这就是很多人看到：

```
Protocols in frame:
```

为空的原因。

---

# 为什么 "Treat all frames as DOCSIS" 不一定有效？

例如：

```
LinkType = Ethernet
```

用户：

```
Decode As DOCSIS
```

实际上就是告诉 Wireshark：

```
offset 0

这里就是 DOCSIS Header
```

如果真实数据：

```
Vendor Header

↓

DOCSIS
```

那么：

```
offset 0
```

根本不是 DOCSIS Header。

DOCSIS dissector：

```
读取 Header

↓

发现字段非法

↓

停止
```

于是：

```
Frame
```

不会出现：

```
DOCSIS
```

所以：

> **Dissector 能否成功，取决于 Header 是否真的位于当前 Offset。**

---

# Wireshark 的核心心智模型

可以记住下面这张图：

```
Capture File
      │
      ▼
LinkType
      │
      ▼
Entry Dissector
      │
      ▼
Header
      │
      ▼
Next Protocol
      │
      ▼
Next Dissector
      │
      ▼
Header
      │
      ▼
Next Protocol
      │
      ▼
......
      │
      ▼
Protocol Tree
```

每一层都重复同一个循环：

```
读取 Header
      │
      ▼
解析字段
      │
      ▼
确定下一层协议
      │
      ▼
调用下一层 Dissector
```

---

# 一句话总结

Wireshark 的协议解析可以抽象成下面这个统一公式：

```
Capture File
      │
      ▼
LinkType（决定入口）
      │
      ▼
Dissector
      │
      ▼
解析 Header
      │
      ▼
确定 Next Protocol
      │
      ▼
调用下一个 Dissector
      │
      ▼
递归解析
      │
      ▼
构建 Protocol Tree
```

牢记三个关键词即可：

1. **入口（Entry）**：由 LinkType 决定第一个 Dissector。
2. **递归（Recursive Dissection）**：每一层协议通过协议字段（如 EtherType、IP Protocol、TCP Port 等）选择并调用下一层 Dissector。
3. **协议树（Protocol Tree）**：每个 Dissector 将解析结果追加到树中，最终形成 Wireshark 的 Packet Details 视图。