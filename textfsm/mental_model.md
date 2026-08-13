# TextFSM 实现的基本思想、底层原理、设计哲学与心智模式

## 一、TextFSM 是什么？

TextFSM 可以看作是一个**基于有限状态机（Finite State Machine，FSM）的文本解析器**。

它的目标只有一句话：

> **把非结构化文本（CLI 输出）转换成结构化数据（表格）。**

例如：

```text
Interface        Status    Protocol
GigabitEthernet0 up        up
GigabitEthernet1 down      down
```

最终得到

| Interface | Status | Protocol |
| ---------- | ------ | -------- |
| Gig0       | up     | up       |
| Gig1       | down   | down     |

输出通常就是：

```python
[
    ["Gig0","up","up"],
    ["Gig1","down","down"]
]
```

或者

```python
[
    {
        "interface":"Gig0",
        "status":"up",
        "protocol":"up"
    }
]
```

---

# 二、为什么需要 TextFSM？

网络设备（Cisco、Juniper、Huawei、Arista……）大量接口都是：

```text
show ip route
show interface
show arp
show version
```

这些输出都是**给人看的**。

而程序需要的是：

```
JSON
Dict
List
Table
```

于是出现一个问题：

```
CLI Text
        │
        ▼
Regex?
        │
        ▼
Python Object
```

如果全部写 Regex，会出现：

- 正则越来越长
- 不同设备格式不同
- 很难维护
- 每个 show 命令都重新写

所以 Google 提出了 TextFSM：

> **把 Regex 提升到"模板(template)"层面。**

因此：

```
CLI
 ↓
Template
 ↓
FSM
 ↓
Record
 ↓
Table
```

---

# 三、底层原理

TextFSM 底层其实非常简单。

可以抽象成：

```
文件
 ↓
按行读取
 ↓
当前状态(State)
 ↓
拿这一状态所有Regex尝试匹配
 ↓
匹配成功？
 ↓
执行动作(Action)
 ↓
状态迁移
 ↓
继续下一行
```

也就是说：

TextFSM 从来不是：

```
整个文本一起Regex
```

而是

```
Line by Line
```

逐行扫描。

这一点非常重要。

---

# 四、为什么采用逐行扫描？

因为 CLI 本质就是：

```
line1
line2
line3
line4
line5
...
```

例如：

```text
Interface Gi0
  status up
  mtu 1500

Interface Gi1
  status down
  mtu 1500
```

真正决定上下文的是：

> 当前正在解析哪一段。

因此：

```
当前状态
+
当前这一行
```

就足够决定下一步。

这就是 FSM 最擅长解决的问题。

---

# 五、FSM（有限状态机）的思想

所谓 FSM：

就是：

> **程序任何时候只处于一个状态。**

例如：

```
Start

↓

Interface

↓

IP

↓

Done
```

例如：

```
Start

读到 Interface

↓

进入 Interface State

↓

继续读取

↓

读到 IP

↓

进入 IP State

↓

记录完成

↓

Start
```

整个程序始终只有一个 Current State。

这就是状态机。

---

# 六、TextFSM 的组成

一个模板主要包含三部分：

```
Value

State

Rule
```

例如：

```text
Value IF (\S+)
Value STATUS (up|down)

Start
  ^${IF}\s+${STATUS} -> Record
```

可以理解成：

```
变量定义

↓

状态

↓

规则
```

非常像：

```
变量
↓

if
↓

动作
```

---

# 七、真正的数据流

例如输入：

```text
Gi0 up
Gi1 down
```

FSM 流程：

```
开始

↓

读第一行

↓

Regex匹配成功

↓

IF=Gi0
STATUS=up

↓

Record

↓

保存

↓

读第二行

↓

Regex匹配

↓

Record

↓

结束
```

整个过程中：

```
Current State
↓

Regex

↓

Capture

↓

Output
```

---

# 八、为什么叫 FSM，而不是 Regex？

很多人误以为：

```
TextFSM = Regex
```

其实不是。

Regex 只是：

```
识别一行
```

FSM 才负责：

```
什么时候匹配

什么时候跳转

什么时候结束

什么时候保存
```

也就是说：

Regex 负责：

> **Recognition（识别）**

FSM 负责：

> **Control（控制流程）**

二者分工明确。

---

# 九、状态迁移（State Transition）

例如：

```text
Interface Gi0

IP 10.0.0.1

Description Uplink
```

可以设计：

```
Start

↓

看到 Interface

↓

InterfaceState

↓

看到 IP

↓

IPState

↓

看到 Description

↓

DescState
```

状态图：

```
          +---------+
          | Start   |
          +---------+
                |
                |
        Interface
                |
                ▼
        +---------------+
        | Interface     |
        +---------------+
                |
             IP Line
                |
                ▼
        +---------------+
        | IP State      |
        +---------------+
                |
        Description
                |
                ▼
        +---------------+
        | Desc State    |
        +---------------+
```

FSM 的精髓就在于：

> **不同状态使用不同规则解析同一行。**

---

# 十、为什么这种设计非常优雅？

传统 Regex：

```
Regex1
Regex2
Regex3
Regex4
Regex5
```

每一个 Regex 都不知道：

```
上一行是什么

下一行是什么
```

所以：

Regex 没有上下文。

FSM 则不同：

```
State + Regex
```

例如：

```
当前状态：

Interface

↓

Regex A

↓

匹配成功

↓

进入 IP

↓

Regex B
```

Regex 本身无需了解上下文，状态机负责上下文管理。

---

# 十一、TextFSM 的核心思想

可以概括成一句话：

> **用状态保存上下文，用正则提取信息，用动作驱动解析流程。**

对应关系：

```
文本
↓

Regex
↓

提取字段

↓

Action

↓

State

↓

下一行
```

整个解析过程形成一个闭环。

---

# 十二、底层算法（伪代码）

```python
state = Start

for line in lines:

    rules = state.rules

    for rule in rules:

        if regex_match(rule.regex, line):

            save_variable()

            if rule.action == Record:
                output()

            state = rule.next_state

            break
```

真正复杂的地方只有：

```
当前状态
+
当前规则
+
当前输入
```

没有递归，没有回溯，也没有复杂的数据结构。

---

# 十三、时间复杂度分析

假设：

```
N 行文本

M 条规则
```

那么：

```
每一行

↓

尝试 M 条规则
```

复杂度约为：

```
O(N × M)
```

由于：

```
M 很小（通常几十条）

N 一般几百行
```

因此性能非常高。

---

# 十四、TextFSM 对我们的启发

## 启发一：不要直接处理数据，要先建模

很多程序员：

```
输入

↓

Regex

↓

结束
```

而 TextFSM：

```
输入

↓

抽象状态

↓

规则

↓

输出
```

先建立模型，再处理数据，系统会更加稳定且易维护。

---

## 启发二：上下文比算法更重要

很多解析问题并不难。

真正困难的是：

```
当前解析到了哪里？
```

例如：

```
Interface

IP

Route

ACL
```

不同位置意味着不同语义。

因此：

> **上下文决定解释方式。**

这正是状态机存在的意义。

---

## 启发三：把流程和规则分离

TextFSM：

```
FSM
```

负责：

```
流程
```

Regex：

```
负责规则
```

因此：

```
流程

↓

稳定
```

```
规则

↓

可配置
```

这就是经典的：

> **控制流（Control Flow）与业务规则（Business Rules）分离。**

这一思想同样适用于工作流引擎、规则引擎、编译器前端等系统。

---

## 启发四：声明式优于命令式

普通 Python：

```python
if ...
elif ...
elif ...
```

TextFSM：

```
描述规则

而不是写流程
```

也就是说：

程序员回答的是：

> **"是什么（What）"**

而不是：

> **"怎么做（How）"**

这是典型的声明式（Declarative）设计思想。

---

## 启发五：数据驱动程序，而不是程序驱动数据

传统程序：

```
代码决定解析
```

TextFSM：

```
模板决定解析
```

于是：

```
新增设备

↓

新增模板

↓

无需改程序
```

这体现了**配置驱动（Configuration Driven）**和**数据驱动（Data Driven）**的设计理念。

---

# 十五、对应的心智模式

TextFSM 背后体现了多个经典的软件工程心智模型：

| 心智模式 | TextFSM 中的体现 | 更广泛的应用 |
|----------|------------------|--------------|
| 状态机思维（State-based Thinking） | 用状态保存上下文，而不是依赖复杂条件判断 | TCP、协议解析、编译器、UI、工作流 |
| 分层思维（Separation of Concerns） | Regex 负责识别，FSM 负责控制 | MVC、微服务、分层架构 |
| 声明式思维（Declarative Thinking） | 描述匹配规则，而非编写控制逻辑 | SQL、Terraform、Kubernetes、Ansible |
| 数据驱动思维（Data-driven Design） | 模板定义行为，程序负责执行 | 配置中心、规则引擎、DSL |
| 模式匹配思维（Pattern Matching） | 从文本中识别稳定模式 | 编译器词法分析、日志解析、事件处理 |
| 有限复杂度思维（Bounded Complexity） | 任一时刻只关注"当前状态 + 当前输入" | 流处理、事件循环、嵌入式系统 |

这些模式共同传递了一个重要理念：

> **不要一次解决整个问题，而是把问题限制在"当前上下文"中，使每一步都简单、确定、可验证。**

---

# 十六、总结

TextFSM 并不仅仅是一个文本解析工具，它体现了一套成熟的软件设计哲学：

1. **把复杂流程拆解为有限状态（FSM）。**
2. **把识别逻辑（Regex）与控制逻辑（State）解耦。**
3. **把解析规则外置为模板，实现声明式配置。**
4. **按"逐行输入 → 状态迁移 → 字段提取 → 记录输出"的流水线工作。**
5. **通过"状态 + 当前输入"局部决策，避免全局复杂性。**

因此，从更高的视角来看，TextFSM 不只是一个 CLI 解析器，它是**有限状态机（FSM）+ 正则表达式（Regex）+ 声明式 DSL（模板）+ 数据驱动架构**四者结合的典型实践。这种设计思想广泛存在于协议解析、编译器、日志分析、事件驱动系统、工作流引擎以及现代基础设施自动化中，其核心价值在于：**以明确的状态建模上下文，以可配置的规则表达行为，以简单的局部决策构建可扩展、可维护的复杂系统。**