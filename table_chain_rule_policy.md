# iptables 心智模型：Table、Chain、Rule、Policy 完整解析

> **一句话总结：**
>
> **Table 决定"做什么"，Chain 决定"什么时候做"，Rule 决定"哪些包这样做"，Policy 决定"没有任何 Rule 命中怎么办"。**

---

# 1. 整体架构

iptables 可以看成一个四层结构：

```text
                iptables

          ┌──────────────────┐
          │      Table       │   ← 功能分类（做什么）
          └────────┬─────────┘
                   │
          ┌────────▼─────────┐
          │      Chain       │   ← 数据包经过的位置（什么时候）
          └────────┬─────────┘
                   │
          ┌────────▼─────────┐
          │      Rule        │   ← 匹配条件 + 动作（哪些包）
          └────────┬─────────┘
                   │
          ┌────────▼─────────┐
          │     Target       │   ← ACCEPT / DROP / DNAT ...
          └──────────────────┘

Policy = Chain 中所有 Rule 都未命中时执行的默认动作
```

四者分别回答四个不同的问题：

| 概念 | 回答的问题 |
|------|------------|
| Table | 我要做什么？（过滤、NAT、修改包……） |
| Chain | 数据包现在走到哪里？ |
| Rule | 什么样的数据包？ |
| Target | 匹配以后怎么办？ |
| Policy | 如果没有任何 Rule 匹配怎么办？ |

---

# 2. Table：按功能分类

## 本质

**Table = 功能模块（Department）**

不同 Table 负责不同类型的数据包处理。

例如公司：

```text
公司
├── 安检部门
├── 转运部门
├── 地址修改部门
└── 统计部门
```

iptables：

```text
iptables
├── filter
├── nat
├── mangle
├── raw
└── security
```

---

## 常见 Table

### filter

负责：

- ACCEPT
- DROP
- REJECT

也就是：

> 是否允许数据包继续。

例如：

```text
Internet
      │
      ▼
允许？
      │
 ┌────┴────┐
 │         │
允许      丢弃
```

---

### nat

负责地址转换：

- SNAT
- DNAT
- MASQUERADE
- Port Forwarding

例如：

```text
192.168.1.10

↓

203.0.113.20
```

或者：

```text
80

↓

8080
```

---

### mangle

修改 IP Header：

例如：

- TTL
- TOS
- DSCP
- MARK

通常用于：

QoS、策略路由等。

---

### raw

最早处理 Packet。

通常：

- NOTRACK

很少直接使用。

---

### security

主要配合 SELinux。

普通服务器基本不用。

---

## 心智模式

```
Table = "按功能分类"

Filter
↓

负责过滤

NAT
↓

负责改地址

Mangle
↓

负责修改数据包
```

---

# 3. Chain：数据包经过的位置

很多人误认为：

> Chain = 一堆 Rule

其实：

**Chain 更重要的意义是：**

> Packet Flow 中固定的处理阶段。

例如：

```
Internet

↓

PREROUTING

↓

Routing Decision

↓

INPUT

↓

Application
```

Chain 表示：

> 数据包现在走到了哪里。

---

## 常见 Chain

### INPUT

数据包：

```
别人

↓

本机
```

例如：

SSH：

```
Internet

↓

INPUT

↓

sshd
```

---

### OUTPUT

数据包：

```
Application

↓

外面
```

例如：

```
curl

↓

OUTPUT

↓

Internet
```

---

### FORWARD

数据包：

```
别人

↓

经过我

↓

别人
```

例如：

路由器：

```
PC1

↓

Router

↓

PC2
```

Router 上经过：

```
FORWARD
```

---

### PREROUTING

路由决定之前。

典型：

DNAT。

例如：

```
80

↓

8080
```

需要在：

```
PREROUTING
```

完成。

---

### POSTROUTING

路由之后。

准备离开机器。

典型：

SNAT：

```
192.168.1.10

↓

203.0.113.20
```

---

## 心智模式

```
Chain

不是

Rule List

而是

Packet Processing Stage
```

---

# 4. Rule：真正的规则

Rule：

其实就是：

```
if (条件)
    执行动作
```

例如：

```bash
iptables -A INPUT \
    -p tcp \
    --dport 22 \
    -j ACCEPT
```

翻译：

```
if TCP
and Port == 22

↓

ACCEPT
```

等价：

```c
if (tcp && dport == 22)
    ACCEPT;
```

---

## Rule = Match + Target

```
条件

+

动作
```

例如：

```
TCP 22

↓

ACCEPT
```

或者：

```
ICMP

↓

DROP
```

---

# 5. Target：Rule 的动作

Target 是 Rule 的处理结果。

例如：

```
-j ACCEPT
```

表示：

允许。

---

常见 Target：

| Target | 含义 |
|---------|------|
| ACCEPT | 放行 |
| DROP | 静默丢弃 |
| REJECT | 拒绝并返回错误 |
| LOG | 写日志 |
| DNAT | 修改目标地址 |
| SNAT | 修改源地址 |
| MASQUERADE | 动态 SNAT |
| RETURN | 返回上一层 Chain |

---

# 6. Policy：默认动作

Policy：

不是 Rule。

它是：

```
Default Action
```

例如：

```
Rule1

↓

Rule2

↓

Rule3

↓

全部没匹配

↓

Policy
```

例如：

```bash
iptables -P INPUT DROP
```

表示：

```
INPUT

默认

DROP
```

所以：

Policy 就像：

```c
switch (...)

default:
    DROP;
```

---

# 7. Rule 的执行顺序

iptables：

采用：

```
Linear Scan
```

即：

```
Rule1

↓

Rule2

↓

Rule3

↓

Policy
```

一旦命中：

立即停止。

例如：

```
Rule1

DROP ALL

Rule2

ALLOW SSH
```

SSH 永远不会执行到。

所以：

**Rule 顺序十分重要。**

---

# 8. Chain 本质就是一串 if

例如：

```
INPUT
```

可以理解为：

```c
if (tcp && port == 22)
    ACCEPT;

else if (tcp && port == 80)
    ACCEPT;

else if (icmp)
    DROP;

else
    Policy;
```

所以：

```
Chain

=

Rule1

↓

Rule2

↓

Rule3

↓

Policy
```

---

# 9. 一个 Table 包含多个 Chain

例如：

```
filter

├── INPUT
├── OUTPUT
└── FORWARD
```

每个 Chain：

都有自己的 Rule。

例如：

```
filter

├── INPUT
│      Rule1
│      Rule2
│
├── OUTPUT
│      Rule1
│      Rule2
│
└── FORWARD
       Rule1
       Rule2
```

因此：

```
Table

↓

Chain

↓

Rule
```

---

# 10. Packet Flow：数据包如何经过各个 Table

## 入站（Incoming）

```
                    Packet
                       │
                       ▼
            raw PREROUTING
                       │
                       ▼
          mangle PREROUTING
                       │
                       ▼
             nat PREROUTING
                       │
                       ▼
              Routing Decision
            ┌──────────┴──────────┐
            ▼                     ▼
       发给本机                转发
            │                     │
            ▼                     ▼
    mangle INPUT         mangle FORWARD
            │                     │
            ▼                     ▼
     filter INPUT        filter FORWARD
            │                     │
            ▼                     ▼
      Local Process      nat POSTROUTING
```

---

## 出站（Outgoing）

```
Application
      │
      ▼
 raw OUTPUT
      │
      ▼
mangle OUTPUT
      │
      ▼
 nat OUTPUT
      │
      ▼
filter OUTPUT
      │
      ▼
mangle POSTROUTING
      │
      ▼
 nat POSTROUTING
      │
      ▼
Network Interface
```

注意：

> **Packet Flow 是 Linux 内核固定好的。**

iptables 只是：

> 在这些固定位置提供 Hook。

---

# 11. 四者之间的关系

```
iptables
│
├── Table（按功能分类）
│      │
│      ├── filter
│      ├── nat
│      └── mangle
│
├── 每个 Table
│      │
│      ├── Chain（处理阶段）
│      │      │
│      │      ├── INPUT
│      │      ├── OUTPUT
│      │      ├── FORWARD
│      │      ├── PREROUTING
│      │      └── POSTROUTING
│      │
│      └── 每个 Chain
│              │
│              ├── Rule
│              ├── Rule
│              ├── Rule
│              └── Policy（默认动作）
```

---

# 12. 公司类比（最容易理解）

```
公司
│
├── 部门（Table）
│
├── 工作站（Chain）
│
├── 操作规范（Rule）
│
└── 默认规定（Policy）
```

对应关系：

| iptables | 公司类比 | 回答的问题 |
|-----------|----------|------------|
| Table | 部门 | 做什么？ |
| Chain | 工作站 | 在什么时候处理？ |
| Rule | 操作规范 | 哪些包这样处理？ |
| Target | 执行动作 | 怎么处理？ |
| Policy | 默认规定 | 都没命中怎么办？ |

---

# 13. 最佳实践

## ① 先理解 Packet Flow，再学习 iptables

不要死记：

```
INPUT
OUTPUT
FORWARD
```

而要先回答：

> Packet 现在在哪里？

---

## ② 牢记 Table 是功能，不是流程

错误理解：

```
Packet

↓

filter

↓

nat
```

正确理解：

```
Packet

↓

某个 Hook

↓

该 Hook 上有哪些 Table

↓

按固定顺序执行
```

---

## ③ Rule 顺序十分重要

例如：

```
DROP ALL

↓

ALLOW SSH
```

SSH 永远不会命中。

正确：

```
ALLOW SSH

↓

DROP ALL
```

---

## ④ Policy 应该尽量简单

推荐：

```
允许需要的

↓

默认 DROP
```

例如：

```
ACCEPT SSH

↓

ACCEPT HTTP

↓

Policy = DROP
```

---

# 14. 核心心智模型总结

## 四层模型

```
Table
↓

Chain
↓

Rule
↓

Target
```

---

## Packet Flow 模型

```
Packet

↓

经过固定 Hook

↓

每个 Hook

↓

执行对应 Table

↓

执行对应 Chain

↓

依次检查 Rule

↓

命中立即结束

↓

没有命中

↓

Policy
```

---

## 一句话总结

> **Table 决定功能（做什么），Chain 决定处理阶段（什么时候），Rule 决定匹配条件（哪些包），Target 决定处理动作（怎么办），Policy 决定所有 Rule 都未命中时的默认行为。**