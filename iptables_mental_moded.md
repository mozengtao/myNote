# Linux iptables 命令使用及心智模型

---

# 1. iptables 到底是什么

很多人学习 iptables 时陷入两个误区：

- 死记硬背命令
- 死记硬背 INPUT/FORWARD/OUTPUT

结果：

```text
会写规则
不会分析问题

会复制配置
不会定位故障
```

真正应该建立的是：

```text
Packet Traversal Model
（数据包流转模型）
```

因为：

```text
iptables 本质上不是规则系统

而是：

Netfilter 数据包处理流水线的配置接口
```

---

# 2. Netfilter 与 iptables 的关系

架构：

```text
+----------------------+
|      User Space      |
|      iptables        |
+----------+-----------+
           |
           v
+----------------------+
|      Netfilter       |
|  (Kernel Framework)  |
+----------+-----------+
           |
           v
+----------------------+
|    Network Stack     |
+----------------------+
```

理解为：

```text
iptables
    ↓
配置 Netfilter

Netfilter
    ↓
真正执行规则
```

即：

```text
iptables = CLI工具

Netfilter = Linux内核中的报文处理框架
```

---

# 3. 最重要的心智模型

永远先问：

```text
这个包现在在哪？
```

而不是：

```text
应该写什么规则？
```

---

# 4. 数据包流转模型

## 4.1 收到外部数据包

```text
eth0
  |
  v
PREROUTING
  |
  +-------> 路由判断
                |
        +-------+-------+
        |               |
        v               v
      INPUT         FORWARD
        |               |
        v               v
     Local          POSTROUTING
     Process           |
                        v
                     eth1
```

这里发生两种情况：

---

### 情况1：目标是本机

例如：

```text
Client ---> Server
```

路径：

```text
eth0
  |
PREROUTING
  |
Routing
  |
INPUT
  |
Process
```

---

### 情况2：目标不是本机

例如：

```text
PC1 ---> Linux Router ---> Internet
```

路径：

```text
eth0
  |
PREROUTING
  |
Routing
  |
FORWARD
  |
POSTROUTING
  |
eth1
```

---

## 4.2 本机发出的包

```text
Process
   |
   v
OUTPUT
   |
   v
POSTROUTING
   |
   v
eth0
```

例如：

```bash
ping 8.8.8.8

curl google.com

ssh remote_host
```

---

# 5. 五大 Chain

Chain 可以理解为：

```text
Packet Processing Stage
（报文处理阶段）
```

---

# INPUT

发给本机的流量。

路径：

```text
PREROUTING
    |
    v
 INPUT
    |
    v
 Process
```

典型场景：

```text
SSH
HTTP Server
DNS Server
SNMP Agent
```

例如：

```bash
iptables -A INPUT \
         -p tcp \
         --dport 22 \
         -j ACCEPT
```

---

# OUTPUT

本机主动发出的流量。

路径：

```text
Process
   |
   v
OUTPUT
   |
POSTROUTING
```

例如：

```bash
curl https://example.com

ping 8.8.8.8

ssh remote_server
```

---

# FORWARD

经过本机转发的流量。

路径：

```text
PREROUTING
    |
 FORWARD
    |
POSTROUTING
```

例如：

```text
LAN
 |
 v
Linux Router
 |
 v
Internet
```

Linux 只是路由器。

---

# PREROUTING

路由判断之前。

路径：

```text
Packet
  |
PREROUTING
  |
Routing Decision
```

特点：

```text
路由尚未发生
```

常见用途：

```text
DNAT

Port Forward

Transparent Proxy
```

例如：

```text
公网IP:80
      ↓
内网服务器:8080
```

因为：

```text
必须先修改目标地址

然后才能进行路由判断
```

---

# POSTROUTING

路由之后。

路径：

```text
Routing Decision
      |
POSTROUTING
      |
 Outgoing Interface
```

特点：

```text
已经知道出口接口
```

常见用途：

```text
SNAT

MASQUERADE
```

例如：

```text
192.168.1.10
       |
       v
 Public IP
```

因为：

```text
知道从哪个出口发出后
才能决定源地址转换
```

---

# 6. Table 的真正含义

很多人把：

```text
Table
```

和：

```text
Chain
```

混淆。

实际上：

```text
Table
    ↓
做什么

Chain
    ↓
在哪里做
```

---

# 7. 四大 Table

---

# Filter Table

默认表。

负责：

```text
Allow

Drop

Reject
```

即：

```text
访问控制
```

例子：

```bash
iptables -A INPUT \
         -p tcp \
         --dport 22 \
         -j ACCEPT
```

---

# NAT Table

负责：

```text
地址转换
```

包括：

```text
DNAT

SNAT

MASQUERADE
```

查看：

```bash
iptables -t nat -L -n -v
```

---

# Mangle Table

负责：

```text
修改报文属性
```

例如：

```text
TTL

TOS

DSCP

MARK
```

经常用于：

```text
QoS

Policy Routing

Traffic Engineering
```

---

# Raw Table

最早进入的表。

主要用于：

```text
NOTRACK
```

绕过连接跟踪。

用于：

```text
高性能转发

特殊场景优化
```

---

# 8. 整体 Hook 顺序

完整视图：

```text
                Incoming Packet

                        |
                        v

                 Raw PREROUTING
                        |
                        v

               Mangle PREROUTING
                        |
                        v

                 NAT PREROUTING
                        |
                        v

                  Routing Decision
                 /                \
                /                  \
               v                    v

          Local Packet         Forward Packet

               |                    |
               v                    v

        Mangle INPUT         Mangle FORWARD
               |                    |
               v                    v

        Filter INPUT         Filter FORWARD
               |                    |
               v                    v

          Local App        Mangle POSTROUTING
                                    |
                                    v

                           NAT POSTROUTING
                                    |
                                    v

                              Out Interface
```

## 各步骤详细解释

### 1. Incoming Packet（数据包进入）
**作用**：数据包从网络接口进入系统
**目的**：这是所有网络处理的起点，无论是本地应用的数据还是需要转发的数据包都从这里开始

### 2. Raw PREROUTING（原始预路由）
**作用**：最早的处理点，主要用于连接跟踪相关操作
**目的**：
- 设置连接跟踪标记（NOTRACK）
- 跳过连接跟踪以提高性能
- 最原始的数据包处理，不做修改

### 3. Mangle PREROUTING（修改预路由）
**作用**：修改数据包的头部字段
**目的**：
- 修改 TOS/DSCP 字段进行 QoS 控制
- 设置数据包标记（MARK）用于后续处理
- 修改 TTL 值
- 为策略路由做准备

### 4. NAT PREROUTING（网络地址转换预路由）
**作用**：进行目标地址转换（DNAT）
**目的**：
- 端口转发（将外部访问转发到内部服务器）
- 负载均衡（将流量分发到多个后端）
- 重定向到本地端口
- 透明代理设置

### 5. Routing Decision（路由决策）
**作用**：内核根据目标地址决定数据包的去向
**目的**：
- 判断数据包是发给本机的还是需要转发的
- 这是整个流程的关键分岔点
- 基于路由表进行决策

### 6. Local Packet 分支（本地数据包）

#### 6.1 Mangle INPUT（修改输入）
**作用**：对发往本机的数据包进行修改
**目的**：
- 为本地应用设置特殊标记
- 修改 QoS 相关字段
- 数据包的最后修改机会

#### 6.2 Filter INPUT（过滤输入）
**作用**：决定是否允许数据包进入本机
**目的**：
- **防火墙的核心**：阻止不想要的连接
- 保护本机服务的安全
- 实现访问控制策略

#### 6.3 Local App（本地应用）
**作用**：数据包最终到达本机的应用程序
**目的**：数据包处理的终点，应用程序获得数据

### 7. Forward Packet 分支（转发数据包）

#### 7.1 Mangle FORWARD（修改转发）
**作用**：对需要转发的数据包进行修改
**目的**：
- 为转发数据包设置 QoS 标记
- 修改数据包优先级
- 流量整形准备

#### 7.2 Filter FORWARD（过滤转发）
**作用**：决定是否允许数据包通过本机转发
**目的**：
- **路由器/网关的核心安全控制**
- 控制哪些流量可以通过
- 实现网络间的访问策略

#### 7.3 Mangle POSTROUTING（修改后路由）
**作用**：在数据包离开前的最后修改机会
**目的**：
- 最终的 QoS 设置
- 出接口相关的修改
- 流量标记的最终处理

#### 7.4 NAT POSTROUTING（网络地址转换后路由）
**作用**：进行源地址转换（SNAT/MASQUERADE）
**目的**：
- **共享上网的关键**：将内网地址转换为公网地址
- 隐藏内网结构
- 实现网络地址复用

#### 7.5 Out Interface（输出接口）
**作用**：数据包从网络接口发出
**目的**：数据包处理的终点，发送到下一跳或目标网络

## 关键理解点

1. **顺序很重要**：每个步骤都有其特定的位置和作用
2. **分岔是核心**：路由决策将本地处理和转发处理分开
3. **安全在中间**：Filter 表负责安全控制，位于 Mangle 和 NAT 之间
4. **NAT 成对出现**：PREROUTING 做 DNAT，POSTROUTING 做 SNAT
5. **Mangle 灵活性最高**：可以在多个点进行数据包修改

---

# 9. 连接跟踪（Conntrack）

这是 Netfilter 最强大的能力之一。

查看：

```bash
cat /proc/net/nf_conntrack
```

或者：

```bash
conntrack -L
```

---

# 为什么需要 Conntrack

如果没有状态跟踪：

```text
每个包都必须重新判断
```

有状态后：

```text
连接建立一次

后续包自动识别
```

---

# Conntrack 状态机

```text
NEW
ESTABLISHED
RELATED
INVALID
```

---

## NEW

新连接。

例如：

```text
TCP SYN
```

---

## ESTABLISHED

已经建立连接。

例如：

```text
TCP ACK

TCP DATA
```

---

## RELATED

与现有连接相关。

例如：

```text
FTP Data Channel

ICMP Error
```

---

## INVALID

无法识别。

例如：

```text
损坏报文

异常状态
```

---

# 最经典规则

```bash
iptables -A INPUT \
         -m conntrack \
         --ctstate ESTABLISHED,RELATED \
         -j ACCEPT
```

含义：

```text
允许已建立连接的返回流量
```

这是绝大多数防火墙配置中的基础规则。

---

# 10. Rule 的四要素

任何规则都可以拆成：

```text
Table
Chain
Match
Target
```

---

# 示例

```bash
iptables -t nat \
         -A POSTROUTING \
         -s 192.168.1.0/24 \
         -o eth0 \
         -j MASQUERADE
```

拆解：

```text
Table:
    nat

Chain:
    POSTROUTING

Match:
    -s 192.168.1.0/24
    -o eth0

Target:
    MASQUERADE
```

---

# 心智模型

```text
iptables
    =
Table
    +
Chain
    +
Match
    +
Target
```

---

# 11. 常用命令

---

## 查看规则

```bash
iptables -L -n -v
```

参数：

```text
-L  List

-n  Numeric

-v  Verbose
```

---

## 查看 NAT 规则

```bash
iptables -t nat -L -n -v
```

---

## 查看规则序号

```bash
iptables -L --line-numbers
```

---

## 追加规则

```bash
iptables -A INPUT \
         -p tcp \
         --dport 22 \
         -j ACCEPT
```

追加到链尾。

---

## 插入规则

```bash
iptables -I INPUT 1 \
         -p tcp \
         --dport 22 \
         -j ACCEPT
```

插入到第一条。

---

## 删除规则

按序号：

```bash
iptables -D INPUT 3
```

---

## 清空规则

```bash
iptables -F
```

仅清空规则。

不删除 Chain。

---

# 12. Target 常见动作

---

## ACCEPT

允许通过。

```bash
-j ACCEPT
```

---

## DROP

直接丢弃。

```bash
-j DROP
```

特点：

```text
无响应
```

---

## REJECT

拒绝。

```bash
-j REJECT
```

特点：

```text
主动返回错误
```

例如：

```text
ICMP Unreachable
TCP RST
```

---

## LOG

记录日志。

```bash
-j LOG
```

通常与 DROP 联用。

---

## MASQUERADE

动态源地址转换。

```bash
-j MASQUERADE
```

常用于：

```text
家庭路由器

云主机
```

---

## SNAT

静态源地址转换。

```bash
-j SNAT
```

例如：

```bash
-j SNAT --to-source 1.2.3.4
```

---

## DNAT

目标地址转换。

```bash
-j DNAT --to-destination 192.168.1.10
```

---

# 13. 故障定位模型

不要先看规则。

按照报文路径分析。

---

## Step 1

包在哪？

```text
INPUT ?

OUTPUT ?

FORWARD ?
```

---

## Step 2

走哪个出口？

```bash
ip route get <ip>
```

例如：

```bash
ip route get 8.8.8.8
```

查看：

```text
next hop

outgoing interface

source address
```

---

## Step 3

连接状态是什么？

```bash
conntrack -L
```

查看：

```text
NEW

ESTABLISHED

RELATED
```

---

## Step 4

规则是否命中？

```bash
iptables -L -n -v
```

重点：

```text
pkts

bytes
```

例如：

```text
pkts 10000
bytes 5M
```

如果持续增长：

```text
规则被命中
```

如果始终为：

```text
0
```

说明：

```text
流量根本没经过这里
```

---

## Step 5

抓包验证

```bash
tcpdump -ni any host 1.1.1.1
```

观察：

```text
是否进入系统

是否离开系统

在哪个接口消失
```

---

# 14. 网络工程视角的最终模型

不要把 iptables 看成：

```text
规则列表
```

应该看成：

```text
Packet Processing Pipeline
```

即：

```text
Packet
  |
  v

[Raw]
  |
  v

[Mangle]
  |
  v

[NAT]
  |
  v

[Routing]
  |
  +------> INPUT
  |
  +------> FORWARD
  |
  +------> OUTPUT
  |
  v

[POSTROUTING]
  |
  v

NIC
```

---

# 15. 一句话总结

记住下面四句话：

```text
Table 决定做什么
Chain 决定在哪里做
Match 决定匹配什么
Target 决定最终动作
```

```text
iptables
=
Table
+
Chain
+
Match
+
Target
```

```text
先分析数据包路径
再分析规则
```

```text
不要记规则
要记 Packet Traversal
```

掌握这套模型之后：

- nftables
- Docker 网络
- Kubernetes Service
- kube-proxy
- Calico
- Conntrack
- Policy Routing
- FRR PBR
- Linux Router

本质上都会回到同一个核心：

```text
Linux Netfilter Packet Processing Pipeline
```