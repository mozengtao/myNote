# TextFSM 核心概念：Value、State、Rule

> **一句话理解**
>
> TextFSM = **有限状态机（FSM） + 正则表达式（Regex）**
>
> 它的工作过程可以理解为：
>
> ```
> 输入文本（逐行）
>        │
>        ▼
>     当前 State
>        │
>        ▼
>   Rule 逐条匹配
>        │
>        ├──匹配成功
>        │      │
>        │      ├──提取 Value
>        │      ├──执行 Action
>        │      └──切换 State（可选）
>        │
>        └──匹配失败
>               │
>               ▼
>         尝试下一条 Rule
> ```
>
> 所以：
>
> - **Value = 我想保存什么**
> - **Rule = 我如何识别它**
> - **State = 我现在正在解析哪里**

---

# 一、Value —— 我想保存的数据

## 心智模式

把 Value 想成数据库的一列（Column）。

例如：

| Interface | IP | Status |
|-----------|----|--------|
| Gi0/0 | 192.168.1.1 | up |

这里：

- Interface
- IP
- Status

都是 Value。

TextFSM 最终输出：

```python
[
    {
        "Interface":"Gi0/0",
        "IP":"192.168.1.1",
        "Status":"up"
    }
]
```

所以：

> **Value 定义的是"我要保存哪些字段"。**

---

## Value 的语法

```text
Value NAME (Regex)
```

例如

```text
Value Interface (\S+)
```

表示：

发现符合 `\S+` 的内容，

保存到变量

```
Interface
```

里面。

---

# 示例1：解析接口名称

输入

```text
Interface Gi0/0
```

模板

```text
Value IFACE (\S+)

Start
    ^Interface ${IFACE} -> Record
```

结果

```
IFACE
-----
Gi0/0
```

---

# 示例2：解析IP

输入

```text
IP address: 192.168.1.1
```

模板

```text
Value IP (\d+\.\d+\.\d+\.\d+)

Start
    ^IP address:\s+${IP}
```

结果

```
IP
----------
192.168.1.1
```

---

# 示例3：多个Value

输入

```text
Gi0/0 up
```

模板

```text
Value IFACE (\S+)
Value STATUS (\S+)

Start
    ^${IFACE}\s+${STATUS}
```

输出

| IFACE | STATUS |
|--------|---------|
| Gi0/0 | up |

---

# 二、Rule —— 如何识别数据

## 心智模式

Rule 就是

> "如果当前这一行长这样，就做某件事情。"

Rule 本质就是：

```
Regex
      +
Action
```

语法：

```
Regex -> Action
```

例如

```
^Interface ${IFACE}
```

就是 Rule。

---

# 示例4：普通匹配

```
^Hostname ${HOST}
```

含义：

如果这一行以 Hostname 开头

提取 HOST。

---

# 示例5：匹配数字

```
^\s+Packets:\s+${PACKETS}
```

输入

```
Packets: 1024
```

得到

```
PACKETS=1024
```

---

# 示例6：Rule 决定 Record

```
^${IFACE}\s+${STATUS} -> Record
```

意思：

这一行结束以后

输出一条记录。

---

# 三、State —— 当前解析位置

## 心智模式

State 就像程序里的：

```
switch
```

或者

```
if (phase == ...)
```

解析 CLI 时，

不同区域需要不同规则。

例如：

```
show bgp

Neighbor ...

Route ...

Statistics ...
```

Neighbor 部分

和

Statistics 部分

完全不同。

于是：

```
State = 当前解析区域
```

---

## State 定义

```
Start
```

或者

```
Neighbor
```

或者

```
Route
```

每个 State 都拥有自己的 Rule。

---

# 示例7：两个State

```
Start

Neighbor
```

表示：

```
Start

↓

发现Neighbor

↓

进入Neighbor状态
```

---

# 示例8：状态切换

```
Start

^Neighbor -> Neighbor
```

表示：

遇到

```
Neighbor
```

以后

开始使用

```
Neighbor
```

状态里面的规则。

---

例如

```
Neighbor 1.1.1.1
  AS 65001
  State Established
```

模板

```
Start

^Neighbor ${IP} -> Neighbor

Neighbor

^\s+AS ${ASN}

^\s+State ${STATE}
```

只有进入 Neighbor 后，

下面两条 Rule 才有效。

---

# 示例9：多个Section

CLI

```
Interface

Gi0/0

Gi0/1

Route

10.1.1.0

20.1.1.0
```

模板

```
Start

^Interface -> InterfaceState

^Route -> RouteState
```

不同区域，

使用不同 Rule。

这就是 State 的作用。

---

# 示例10：状态返回

```
Neighbor

^$ -> Start
```

意思：

遇到空行，

退出 Neighbor，

回到 Start。

这是大型模板最常见的写法。

---

# 深入阅读：彻底搞懂「切换 State」

上面几个示例只说明「State 用来划分解析区域」。
切换的时机、和 Record / Continue / Clear 的组合、EOF / End 这些保留状态，
会在专题里用两组共 20 个示例展开：

[TextFSM 专题：彻底搞懂「切换 State」](state_switching_examples.md)

---

# 三者关系

假设 CLI

```
Interface Gi0/0 up
```

模板

```
Value IFACE (\S+)

Value STATUS (\S+)

Start

^Interface ${IFACE} ${STATUS} -> Record
```

执行流程

```
读取一行

↓

当前State=Start

↓

Rule开始匹配

↓

Regex成功

↓

保存IFACE

↓

保存STATUS

↓

Record

↓

输出
```

得到

```
[
 {
   IFACE:"Gi0/0",
   STATUS:"up"
 }
]
```

---

# 再看一个稍复杂一点

```
show ip interface brief

Interface

Gi0/0 up

Gi0/1 down

Statistics

Packets 100
```

模板

```
Value IFACE (\S+)
Value STATUS (\S+)
Value PACKETS (\d+)

Start

^Interface -> Interface

^Statistics -> Statistics

Interface

^${IFACE} ${STATUS} -> Record

Statistics

^Packets ${PACKETS}
```

执行流程

```
Start

↓

看到 Interface

↓

进入 Interface State

↓

解析接口

↓

看到 Statistics

↓

进入 Statistics State

↓

解析统计信息
```

---

# 三者之间的关系（最重要）

```
                TextFSM

            +------------+
            |   State    |
            +------------+
                   │
         包含很多 Rule
                   │
                   ▼
        +------------------+
        |      Rule        |
        | Regex + Action   |
        +------------------+
                   │
          Regex命中以后
                   │
                   ▼
        +------------------+
        |      Value       |
        | 保存匹配的数据    |
        +------------------+
```

可以记住下面这三个问题：

- **Value：我要保存什么数据？**
- **Rule：我如何识别这一行？**
- **State：我现在正在解析哪一部分？**

---

# 一个更容易记忆的现实类比

假设你在整理一本书。

**State（章节）**

```
第一章
第二章
第三章
```

表示你当前读到哪个章节。

---

**Rule（阅读规则）**

例如：

```
如果这一行以 "作者：" 开头，
就提取作者。
```

或者

```
如果这一行为空，
进入下一章。
```

Rule 就是你的"阅读规则"。

---

**Value（笔记）**

最终记录下来：

```
书名
作者
出版社
ISBN
```

这些就是 Value。

---

因此可以形成一个简单而稳定的心智模型：

```
Value
    = 我要记录的数据（What）

Rule
    = 我如何识别这些数据（How）

State
    = 我现在正在解析哪个区域（Where）
```

> **最终记忆口诀**
>
> - **Value：像数据库字段（Column）**
> - **Rule：像 `if + regex`（条件匹配）**
> - **State：像程序中的 `switch/case` 或解析阶段（Parsing Context）**
>
> **TextFSM 的本质就是：在不同的 State 中，用不同的 Rule 去识别文本，并把匹配到的内容保存到 Value 中，最终输出结构化记录。**