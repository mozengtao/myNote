# FSM 的核心心智模式：FSM 的主要工作就是「判定（Decision）」

> **一句话总结：**
>
> **FSM（Finite State Machine）并不是负责"完成工作"，而是负责"决定下一步应该做什么"。**
>
> 它是整个系统的**决策中心（Decision Engine）**，而不是执行中心（Execution Engine）。

---

# 一、为什么说 FSM 的主要工作就是「判定」

很多初学者容易产生一个误区：

> **FSM 控制整个系统。**

实际上，更准确的理解应该是：

```
FSM
        ↓
判断现在应该做什么
        ↓
通知其他模块去执行
```

也就是说：

FSM 并不负责：

- 数据计算（Compute）
- 数据存储（Storage）
- 网络发送（Transmit）
- DMA 操作
- IO 操作
- 数据库访问

FSM 负责的是：

```
Decision（决策）
```

因此：

> **FSM = Decision Engine（决策引擎）**

而不是：

```
Worker（执行者）
```

---

# 二、FSM 每一次工作的完整流程

FSM 每收到一个事件，都会经历同样的流程：

```
             Event
               │
               ▼
      Current State
               │
               ▼
        FSM 做判定（Decision）
               │
      ┌────────┴────────┐
      ▼                 ▼
Next State         Action List
                       │
                       ▼
             Worker / Service
                       │
                       ▼
               Real Execution
```

FSM 真正工作的只有中间这一层：

```
Decision
```

后面的动作都是别人完成的。

---

# 三、FSM 的核心公式

FSM 可以抽象成两个函数：

```
NextState = f(CurrentState, Event)

Action = g(CurrentState, Event)
```

也就是：

```
当前状态
+
当前事件

↓

FSM进行判定

↓

得到：

下一状态

+

需要执行哪些动作
```

整个 FSM 的本质，就是维护这样一张映射关系：

```
(State, Event)
        │
        ▼
(NextState, Action)
```

---

# 四、经典示例

---

## 示例一：自动门

状态：

```
CLOSED
OPENING
OPEN
CLOSING
```

事件：

```
有人靠近(Person)
```

FSM：

```
Current State：

CLOSED

收到：

Person

↓

Decision：

Next State = OPENING

Action：

motor_open()
```

真正开门的是：

```
Motor
```

FSM 并没有开门。

FSM 只是决定：

```
现在应该开门。
```

---

## 示例二：电梯

状态：

```
Idle
Moving
DoorOpening
DoorClosing
```

收到：

```
Button Press
```

FSM：

```
Idle

+

Button

↓

Moving

↓

Motor.start()
```

FSM 并没有控制电机。

只是：

```
告诉电机：

可以开始。
```

---

## 示例三：TCP 状态机

例如：

```
Current State：

LISTEN
```

收到：

```
SYN
```

FSM：

```
LISTEN

+

SYN

↓

Decision

↓

SYN_RCVD

↓

Send SYN+ACK
```

真正发送的是：

```
tcp_transmit_skb()

↓

IP

↓

Driver

↓

NIC
```

FSM 没有发送任何数据。

只是：

```
决定：

应该发送。
```

---

## 示例四：Linux Driver

例如：

网卡：

```
DOWN
```

收到：

```
ifconfig up
```

FSM：

```
DOWN

+

UP

↓

RUNNING

↓

调用：

driver_open()
```

真正初始化：

```
DMA

RX Queue

TX Queue

Interrupt
```

FSM 不负责。

FSM 只是：

```
决定：

现在开始初始化。
```

---

## 示例五：订单系统

订单：

```
Created
```

收到：

```
Pay Success
```

FSM：

```
Created

↓

Paid
```

通知：

```
库存系统

物流系统

积分系统

短信系统
```

FSM 没有：

```
扣库存

发物流

加积分
```

只是：

```
决定：

订单已经进入 Paid 状态。
```

---

# 五、FSM 为什么叫「状态机」

FSM 最大特点：

**同一个事件，在不同状态下，意义完全不同。**

例如：

按钮：

```
Button Press
```

如果：

```
OFF

↓

ON
```

如果：

```
ON

↓

OFF
```

所以：

```
Button Press
```

没有意义。

真正有意义的是：

```
State

+

Event
```

因此：

状态(State)

就是：

```
上下文(Context)
```

FSM 本质就是：

```
Context

+

Event

↓

Decision
```

---

# 六、FSM 与 Worker 的职责划分

整个系统通常采用如下架构：

```
              Event
                │
                ▼
         +--------------+
         |     FSM      |
         |  Decision    |
         +--------------+
          │          │
          ▼          ▼
   Next State     Action
                      │
                      ▼
              Worker / Service
                      │
                      ▼
              Real Execution
```

职责划分如下：

| 模块 | 职责 |
|------|------|
| FSM | 决定应该做什么（Decision） |
| Dispatcher | 将动作分发出去 |
| Worker | 真正完成工作（Execution） |

一句话：

```
FSM

↓

Should Do

Worker

↓

How To Do
```

---

# 七、经典架构模式中的 FSM 思想

## 1）MVC

```
HTTP Request

↓

Controller

↓

Decision

↓

Service

↓

Execution
```

Controller 本质上就是一个 FSM。

负责：

```
判断

↓

调用哪个 Service
```

---

## 2）State Pattern（状态模式）

GoF 状态模式：

```
Current State Object

↓

handle(event)

↓

Next State

↓

Action
```

其实就是：

```
FSM 的面向对象实现。
```

每一个 State 对象负责：

```
收到事件

↓

做判定

↓

决定下一状态
```

---

## 3）Actor 模型

Actor：

```
Receive Message

↓

Current Behavior

↓

Decision

↓

Next Behavior
```

Behavior：

就是：

```
State
```

所以：

Actor 本质也是 FSM。

---

## 4）CPU 指令译码（Instruction Decode）

CPU：

```
Instruction

↓

Decode

↓

Execute
```

Decode 做的事情：

```
判断：

这是什么指令？

↓

应该交给哪个执行单元？
```

Decode：

就是一个 Decision Engine。

---

## 5）网络协议栈

几乎所有协议：

```
Packet

↓

Parser

↓

FSM

↓

Handler

↓

Execution
```

FSM：

负责：

```
收到ACK

↓

当前状态？

↓

应该：

重传？

发送？

关闭？

等待？
```

真正完成的是：

```
Driver

DMA

NIC
```

---

# 八、统一的软件架构模式

几乎所有大型系统都符合这一模式：

```
              Event
                │
                ▼
         Current State
                │
                ▼
        +----------------+
        |      FSM       |
        | Decision Engine|
        +----------------+
          │           │
          ▼           ▼
   Next State     Action List
                        │
                        ▼
              Dispatcher
                        │
                        ▼
          Worker / Service
                        │
                        ▼
             Real Execution
```

其中：

FSM：

负责：

```
Decision
```

Worker：

负责：

```
Execution
```

Dispatcher：

负责：

```
Routing
```

这是经典的软件设计思想：

> **职责分离（Separation of Concerns）**

---

# 九、形成统一心智模型

以后看到任何 FSM，都可以套入下面这个思维框架：

```
             Event
               │
               ▼
      Current State
               │
               ▼
        Decision（FSM）
               │
      ┌────────┴────────┐
      ▼                 ▼
Next State         Action List
                        │
                        ▼
          Worker / Service
                        │
                        ▼
               Real Execution
```

因此：

FSM 永远不会：

```
自己完成工作
```

FSM 永远只是：

```
决定：

应该做什么。
```

---

# 十、最终形成的心智模式（建议牢记）

## 一句话

> **FSM 的本质不是执行，而是基于当前状态（State）和输入事件（Event）进行判定（Decision），然后决定下一状态（Next State）以及需要触发的动作（Action）。**

---

## 四步口诀

```
看状态（State）

        ↓

收事件（Event）

        ↓

做判定（Decision）

        ↓

发指令（Dispatch）
```

---

## 三层职责

```
Decision

↓

Dispatch

↓

Execution
```

分别对应：

| 层次 | 负责内容 |
|------|----------|
| FSM | 判定（Decision） |
| Dispatcher | 调度（Dispatch） |
| Worker / Service | 执行（Execution） |

---

## 最终统一架构图

```
          +---------------------+
          |     Current State   |
          +---------------------+
                     │
                     ▼
                Receive Event
                     │
                     ▼
          +---------------------+
          |        FSM          |
          | Decision Engine     |
          +---------------------+
             │              │
             ▼              ▼
      Next State       Action List
                             │
                             ▼
                  Dispatcher / Router
                             │
                             ▼
                  Worker / Service
                             │
                             ▼
                    Real Execution
```

---

# 核心总结（最终心智模式）

可以把 FSM 理解成系统中的**交通警察**：

- **事件（Event）**：路口来的车辆。
- **状态（State）**：当前红灯、绿灯或黄灯。
- **FSM**：根据当前信号灯和来车情况，判断下一步应该放行、等待还是切换信号。
- **执行者（Worker）**：真正驾驶车辆通过路口的人。

因此：

- **FSM 不负责"干活"（Execution）**；
- **FSM 负责"判断该让谁干、什么时候干"（Decision + Dispatch）**。

这一思想广泛存在于 TCP 协议栈、Linux 驱动、GUI 事件循环、订单系统、工作流引擎、Actor 模型、状态模式（State Pattern）等系统中，其共同遵循的架构原则就是：

> **Decision（决策） 与 Execution（执行）分离，通过 State + Event 驱动系统行为。**

---

# 十一、可运行对照示例

上面的心智模型可以落到同一套代码骨架上：

```
(State, Event) -> fsm_decide() -> (NextState, ActionList) -> Dispatcher -> Worker
```

Event / Action 都是 **kind + 数据**：转移表按 kind 判定，Worker 是带参函数（例如 `lamp_set(color)`、`inventory_deduct(sku, qty)`）。跨事件数据放在 Ctx，不塞进 State 枚举。细节见 [总览里的 kind 与数据](decision-examples/README.md)。

五个由浅入深的场景，每种都有 Python / C / Go 三份可运行实现，trace 格式统一，方便对照：

- [FSM Decision 可运行对照示例（总览）](decision-examples/README.md)
- [01 交通灯](decision-examples/01-traffic-light/README.md) — 同一事件在不同状态下意义不同
- [02 旋转门](decision-examples/02-turnstile/README.md) — 非法转移必须 REJECT
- [03 自动门](decision-examples/03-auto-door/README.md) — FSM 只发 `MOTOR_*`，电机才开门
- [04 TCP 三次握手](decision-examples/04-tcp-handshake/README.md) — `SEND_*` 是 Action，NIC 才发包
- [05 订单履约](decision-examples/05-order/README.md) — 一次判定产出 Action List，分发给多个 Worker

建议按编号顺序跑一遍：先看 Python 的转移表和 trace，再对照 C / Go 确认三层职责没有被语法差异打乱。