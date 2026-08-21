# XState v5 FSM 常用关键字完整心智模型

## 目录
- [核心认知：从关键字到系统思维](#核心认知从关键字到系统思维)
- [Machine 架构层级](#machine-架构层级)
- [Machine 基础配置](#machine-基础配置)
- [状态管理核心概念](#状态管理核心概念)
- [事件驱动机制](#事件驱动机制)
- [状态迁移控制](#状态迁移控制)
- [副作用与生命周期](#副作用与生命周期)
- [时间驱动机制](#时间驱动机制)
- [自动迁移机制](#自动迁移机制)
- [数据管理](#数据管理)
- [外部任务管理](#外部任务管理)
- [关键组合模式](#关键组合模式)
- [最终心智模型](#最终心智模型)

---

## 核心认知：从关键字到系统思维

### 最重要的认知转换

学习 XState 时，不要把 `on`、`after`、`guard`、`entry` 等关键字当成一堆需要死记的 API。

更好的理解方式是：

> **XState 是在描述一个"状态 → 事件/时间 → 条件判断 → 状态迁移 → 副作用"的系统。**

### XState 核心流水线

可以把一个状态机理解成下面这条流水线：

```text
                    ┌──────────────┐
                    │ 当前 State    │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
           Event          Time        条件检查
              │            │            │
              ▼            ▼            ▼
             on          after        guard
              │            │            │
              └────────────┼────────────┘
                           │
                           ▼
                       Transition
                           │
                           ▼
                    ┌──────────────┐
                    │ 目标 State    │
                    └──────┬───────┘
                           │
                     entry / exit
                           │
                           ▼
                         action
```

### 关键字心智模型速查表

| XState 关键字 | 你应该把它理解成                 |
| ---------- | ------------------------ |
| `id`       | 这个状态机叫什么                 |
| `initial`  | 状态机从哪里开始                 |
| `states`   | 状态机有哪些状态                 |
| `on`       | **收到某个事件怎么办**            |
| `after`    | **经过一段时间怎么办**            |
| `target`   | **跳到哪个状态**               |
| `guard`    | **允许不允许跳**               |
| `actions`  | **跳转过程中做什么副作用**          |
| `entry`    | **进入状态时做什么**             |
| `exit`     | **离开状态时做什么**             |
| `context`  | **状态机需要保存哪些数据**          |
| `invoke`   | **启动一个持续运行的外部任务/服务**     |
| `always`   | **不需要事件，自动判断是否迁移**       |
| `onDone`   | **某个 invoke/任务正常完成后怎么办** |
| `onError`  | **某个 invoke/任务失败后怎么办**   |

---

## Machine 架构层级

### Machine 基础结构

一个 XState Machine 最核心的结构：

```yaml
id: device_machine
initial: IDLE
context:
  retryCounter: 0
states:
  IDLE:
    # ...
  CONNECTING:
    # ...
```

可以理解成：

```text
Machine
│
├── id
├── initial  
├── context
└── states
```

### 关键字分类体系

按照作用层级，XState 关键字可以分为三类：

#### Machine-level（机器层级）
- `id` - 状态机身份标识
- `initial` - 初始状态
- `description` - 机器描述
- `context` - 全局数据
- `actions` - 动作定义
- `guards` - 守卫定义
- `states` - 状态定义

#### State-level（状态层级）
- `description` - 状态描述
- `entry` - 进入动作
- `exit` - 退出动作
- `on` - 事件监听
- `after` - 时间监听
- `always` - 自动迁移
- `invoke` - 外部任务

#### Transition-level（迁移层级）
- `target` - 目标状态
- `guard` - 迁移守卫
- `actions` - 迁移动作
- `reenter` - 重入控制

---

## Machine 基础配置

### `id` —— 状态机的身份

```yaml
id: device_machine
```

**目的：** 给整个状态机一个唯一身份。

**心智模型：**
```text
Machine
   │
   └── id = device_machine
```

**主要用途：**
- 标识状态机
- 调试
- 状态路径引用
- Actor 系统中的识别

**重要理解：**
- `id` 是 **Machine ID**，不是 **Current State**
- 例如：`id: tcp_connection_machine`

### `initial` —— 从哪个状态开始

```yaml
initial: IDLE
```

**意思：**
```text
Machine Start
      │
      ▼
    IDLE
```

**心智模型：** `initial = 出生点`

**示例：**
```yaml
initial: CONNECTING
```

启动状态机后直接进入：`CONNECTING`

### `states` —— 定义所有状态

```yaml
states:
  IDLE:
  CONNECTING:
  AUTHENTICATING:
  DATA_TRANSFER:
```

**等价于：**
```text
State Space
│
├── IDLE
├── CONNECTING  
├── AUTHENTICATING
└── DATA_TRANSFER
```

**心智模型：** `states = 状态机可以处于哪些状态`

**状态迁移图示例：**
```text
              ┌─────────────┐
              │    IDLE     │
              └─────────────┘
                     │
                     ▼
              ┌─────────────┐
              │ CONNECTING  │
              └─────────────┘
                     │
                     ▼
              ┌─────────────┐
              │AUTHENTICATING│
              └─────────────┘
                     │
                     ▼
              ┌─────────────┐
              │DATA_TRANSFER│
              └─────────────┘
```

### `description` —— 人类可读的说明

```yaml
description: TCP连接建立阶段
```

**目的：**
- 描述这个状态/机器是干什么的
- 不会决定状态迁移逻辑

**注意：** 从状态机逻辑角度，以下两者没有区别：
```yaml
description: TCP连接建立阶段
```
```yaml  
description: 哈哈哈
```

---

## 状态管理核心概念

### State 和 Context 的根本区别

这是理解 XState 的关键概念之一：

#### State - 离散阶段
```text
State = "我现在处于什么阶段？"
```

例如：`CONNECTING` 是 State

#### Context - 动态数据  
```text
Context = "我现在有哪些数据？"
```

例如：`retryCounter = 3` 是 Context

#### 错误的设计模式
❌ 不要设计成：
```yaml
states:
  CONNECTING_3:
  CONNECTING_4: 
  CONNECTING_5:
```

#### 正确的设计模式
✅ 应该设计成：
```yaml
# State
states:
  CONNECTING:

# Context  
context:
  retryCounter: 3
```

**核心原则：**
> **有限状态负责描述离散流程，Context 负责保存动态数据。**

---

## 事件驱动机制

### `on` —— 收到事件以后怎么办

这是 XState 最重要的关键字之一。

#### 基本语法
```yaml
on:
  CONNECT_OK:
    target: AUTHENTICATING
```

**意思：**
```text
当前状态 = CONNECTING
         ↓
   收到 CONNECT_OK  
         ↓
进入 AUTHENTICATING
```

**心智模型：** `on = Event-driven transition（事件驱动的状态迁移）`

#### 最简单的 on
```yaml
on:
  CONNECT_OK:
    target: AUTHENTICATING
```

**等价于：**
```text
CONNECTING
     │
     │ CONNECT_OK
     ▼
AUTHENTICATING
```

#### 一个状态监听多个事件
```yaml
on:
  CONNECT_OK:
    target: AUTHENTICATING
  CONNECT_FAIL:
    target: RETRY_WAIT
  NETWORK_DOWN:
    target: ERROR_STATE
```

**形成：**
```text
                  CONNECT_OK
                      │
                      ▼
               AUTHENTICATING

CONNECTING ──── CONNECT_FAIL ────> RETRY_WAIT

                  NETWORK_DOWN
                      │
                      ▼
                 ERROR_STATE
```

**核心理解：** `on` 可以理解成状态机的"事件入口"

---

## 状态迁移控制

### `target` —— 迁移到哪里

```yaml
target: CONNECTING
```

**目的：** 指定 Transition 的目标状态

**示例：**
```yaml
on:
  CONNECT_REQ:
    target: CONNECTING
```

**意思：**
```text
IDLE
 │
 │ CONNECT_REQ
 ▼
CONNECTING
```

**核心组合：**
```text
on     = 什么情况下迁移？
target = 迁移到哪里？
```

这是理解 XState 的第一个核心组合：
```yaml
on:
  EVENT:
    target: STATE
```

**翻译成中文：**
```text
发生 EVENT
    ↓
进入 STATE  
```

### `guard` —— 允许不允许迁移

这是第二个最重要的概念。

#### 基本用法
```yaml
on:
  CONNECT_REQ:
    target: CONNECTING
    guard: canAllowConnect
```

**不要理解成：** `guard = 一个普通函数`

**应该理解成：** `Guard 是 Transition 的门卫`

#### 执行流程
```text
                 CONNECT_REQ
                      │
                      ▼
                ┌───────────┐
                │   Guard   │
                │允许连接？   │
                └─────┬─────┘
                  YES │ NO
                      │
              ┌───────┘
              ▼
         CONNECTING
```

#### 条件判断逻辑
**如果：** `canAllowConnect() == true`
```text
IDLE
 ↓
CONNECTING
```

**如果：** `canAllowConnect() == false`  
```text
IDLE
 ↓
仍然 IDLE
```

### guard + target 的完整心智模型

看到：
```yaml
on:
  CONNECT_REQ:
    target: CONNECTING
    guard: canAllowConnect
```

**直接翻译成：**
```text
收到 CONNECT_REQ
       │
       ▼
   canAllowConnect?
      /      \
    YES       NO
     │         │
     ▼         ▼
CONNECTING   不迁移
```

**核心组合：**
```text
on     = 触发条件
guard  = 是否允许  
target = 去哪里
```

**这是学习 XState 最值得记住的组合。**

---

## 副作用与生命周期

### `actions` —— 做一个副作用

#### 基本概念
```yaml
actions:
  recordError
```

**意思：** 发生某个状态迁移时，执行一个动作。

#### 实际应用
```yaml
on:
  CONNECT_FAIL:
    target: RETRY_WAIT
    actions: recordError
```

**可以理解成：**
```text
CONNECT_FAIL
      │
      ├── recordError()
      │
      ▼
 RETRY_WAIT
```

#### 典型 Action 用途
- 打开 socket
- 关闭 socket  
- 记录日志
- 更新 context
- 发送消息
- 启动 timer
- 停止 timer
- 通知应用程序

### actions 和 target 的根本区别

这是非常容易混淆的地方：

```yaml
target: RETRY_WAIT
actions:
  - recordError
```

**两者完全不是一回事：**

```text
target
  ↓
决定"状态去哪儿"

actions
  ↓  
决定"顺便做什么事情"
```

**示例：**
```text
CONNECTING
    │
    │ CONNECT_FAIL
    │
    ├──── recordError()
    │
    ▼
RETRY_WAIT
```

**核心区别：**
```text
target = State Transition
action = Side Effect
```

### `entry` —— 进入状态时做什么

#### 基本用法
```yaml
CONNECTING:
  entry:
    - openSocket
    - start_connect_timeout
```

**意思：** 每次进入 CONNECTING 时执行这些 Action

#### 执行流程
```text
IDLE
 │
 │ CONNECT_REQ
 ▼
CONNECTING
 │
 ├── openSocket()
 │
 └── start_connect_timeout()
```

**心智模型：** `entry = 进入这个状态时自动执行`

### `exit` —— 离开状态时做什么

#### 基本用法
```yaml
DATA_TRANSFER:
  exit:
    - stop_heartbeat_timer
```

**意思：** 离开 DATA_TRANSFER 时执行

#### 示例流程
```text
DATA_TRANSFER
      │
      │ NETWORK_DOWN
      ▼
 ERROR_STATE
```

**在离开 DATA_TRANSFER 时：** `stop_heartbeat_timer()`

**核心概念：**
```text
entry = 进入时
exit  = 离开时
```

### entry / exit / actions 三者区别

这是 XState 最重要的几个概念之一。

#### 完整示例
```yaml
DATA_TRANSFER:
  entry:
    - startHeartbeat
  exit:
    - stopHeartbeat
  on:
    DISCONNECT:
      target: DISCONNECTING
      actions:
        - logDisconnect
```

#### 执行顺序
```text
进入 DATA_TRANSFER
        │
        ▼
  startHeartbeat()
        │
        ▼
   DATA_TRANSFER
        │
        │ DISCONNECT
        │
        ▼
  logDisconnect()
        │
        ▼
离开 DATA_TRANSFER
        │
        ▼
  stopHeartbeat()
        │
        ▼
 DISCONNECTING
```

#### 执行时机对比

| 机制 | 什么时候执行 |
|------|-------------|
| `entry` | 进入状态 |
| `exit` | 离开状态 |
| `transition actions` | 发生特定迁移 |
| `always + actions` | 自动迁移时 |
| `invoke` | 启动/管理外部任务 |

---

## 时间驱动机制

### `after` —— 时间到了以后怎么办

这是 XState 第二个非常重要的触发机制。

#### 基本语法
```yaml
after:
  5000:
    target: RETRY_WAIT
```

**意思：** 进入当前状态以后，等待 5000ms，然后进行迁移

#### 示例流程
```text
CONNECTING
     │
     │
     │ 5 seconds
     │
     ▼
RETRY_WAIT
```

### on 和 after 的本质区别

#### Event-driven vs Time-driven
```text
on
 ↓
Event-driven

after
 ↓  
Time-driven
```

**这是一个非常重要的理解。**

#### 具体对比
```yaml
on:
  CONNECT_OK:
    target: AUTHENTICATING
```

**表示：** "有人告诉我 CONNECT_OK 了"

```yaml
after:
  5000:
    target: RETRY_WAIT
```

**表示：** "没人告诉我任何东西，但时间到了"

#### 触发机制图解
```text
              State
                │
       ┌────────┴────────┐
       │                 │
     Event              Time
       │                 │
       ▼                 ▼
      on               after
       │                 │
       ▼                 ▼
 Transition         Transition
```

### after + guard 组合

```yaml
after:
  1200:
    target: CONNECTING
    guard: hasRetryLeft
```

#### 执行流程
```text
进入 RETRY_WAIT
      │
      │ 1200ms
      ▼
hasRetryLeft?
   /       \
 YES        NO
  │          │
  ▼          ▼
CONNECTING  不迁移
```

**重要理解：** guard 并不是只能用于 on，它可以用于：
- Event transition
- Time transition  
- Always transition

### 多个 after 定时器

#### 示例配置
```yaml
after:
  1200:
    target: CONNECTING
    guard: hasRetryLeft
  30000:
    target: DISCONNECTED
```

#### 执行逻辑
```text
RETRY_WAIT
    │
    ├── 1.2s ──> 如果还有重试次数 ──> CONNECTING
    │
    └── 30s ───────────────────────> DISCONNECTED
```

**这是一个典型的：** `Timeout + Retry FSM`

---

## 自动迁移机制

### `always` —— 不需要事件，自动判断

这是另一个非常重要的 XState 概念。

#### 基本用法
```yaml
always:
  - guard: hasRetryLeft
    target: CONNECTING
  - target: DISCONNECTED
```

#### 执行流程
```text
进入 RETRY_WAIT
      │
      ▼
   立即判断
      │
   hasRetryLeft?
    /       \
  YES        NO
   │          │
   ▼          ▼
CONNECTING DISCONNECTED
```

### on 和 always 的根本区别

```text
on
↓
需要 Event

always
↓
不需要 Event
```

#### 心智模型对比
**`on` 理解成：** "等别人通知我"

**`always` 理解成：** "我自己马上判断"

---

## 数据管理

### `context` —— 状态机的数据

```yaml
context:
  retryCounter: 0
  lastError: null
  heartbeatCount: 0
```

**Context 是：** FSM 的运行时数据

#### 状态与数据的关系
```text
State:
RETRY_WAIT

Context:
retryCounter = 3
lastError = timeout
heartbeatCount = 10
```

**这两个概念必须分开：**
```text
State = "我现在处于什么阶段？"
Context = "我现在有哪些数据？"
```

### guards 通常读取 context

#### 示例配置
```yaml
guards:
  hasRetryLeft:
    type: retry_remain_times_gt0
```

#### 实际实现
```javascript
function hasRetryLeft(context) {
    return context.retryCounter < MAX_RETRY
}
```

#### 执行流程
```text
Context
   │
   ▼
 Guard
   │
   ▼
 true / false
   │
   ▼
是否允许 Transition
```

**核心流程：** `Context → Guard → Transition`

### context + actions 协作

Action 经常修改 Context。

#### 示例
```yaml
actions:
  incrementRetryCounter:
    type: increment_retry_counter
```

#### 执行流程
```text
RETRY_WAIT
    │
    │ entry
    ▼
incrementRetryCounter()
    │
    ▼
context.retryCounter += 1
```

#### 形成反馈循环
```text
Action
  │
  ▼
修改 Context
  │
  ▼
Guard读取 Context
  │
  ▼
决定下一步
```

**这是实际项目中非常常见的模式。**

---

## 外部任务管理

### `invoke` —— 启动一个持续运行的外部任务

这是 XState 中非常重要、但初学者容易忽略的概念。

#### 基本用法
```yaml
invoke:
  src: connectTcp
```

#### 执行模型
```text
进入 CONNECTING
       │
       ▼
启动 connectTcp()
       │
       ├── 成功
       │
       └── 失败
```

#### invoke 特别适合
- Promise
- HTTP 请求
- TCP/网络操作
- 子状态机
- 长时间运行的服务
- Actor

#### 核心心智模型
```text
State
  │
  │ invoke
  ▼
External Activity
  │
  ├── done
  └── error
```

### invoke 和 action 的根本区别

这个区别非常重要：

#### Action - 执行一下
```yaml
actions:
  openSocket
```

**更像：** `do something`

#### Invoke - 启动并管理
```yaml
invoke:
  src: connectTcp
```

**更像：**
```text
start something
    │
    ├── running
    ├── done
    └── error
```

#### 记忆方法
```text
Action = 做一下
Invoke = 启动并管理一个任务
```

### `onDone` —— invoke 成功完成

#### 配合 invoke 使用
```yaml
invoke:
  src: connectTcp
onDone:
  target: AUTHENTICATING
```

#### 心智模型
```text
CONNECTING
     │
     │ invoke
     ▼
 connectTcp()
     │
     │ success
     ▼
AUTHENTICATING
```

**核心概念：** `onDone = "这个被 invoke 的任务完成了以后怎么办？"`

### `onError` —— invoke 失败

#### 基本用法
```yaml
onError:
  target: RETRY_WAIT
```

#### 执行流程
```text
connectTcp()
     │
     │ error
     ▼
RETRY_WAIT
```

#### 概念对比
```text
onDone  = 成功完成
onError = 执行失败
```

### `type` —— Action / Guard 的类型标识

#### 示例配置
```yaml
actions:
  resetSession:
    type: reset_session_context
```

#### 目的说明
这里的 `type: reset_session_context` **不是状态机逻辑本身**。

它通常表示：**告诉 XState / 宿主程序，这个 Action 使用哪一个实现。**

#### 外部实现
```javascript
const actions = {
    reset_session_context: (...) => {
        // ...
    },
    close_socket: (...) => {
        // ...  
    }
}
```

#### 引用关系
```yaml
type: close_socket
```

**就相当于引用：** `close_socket()`

#### 重要区分
```text
type 在这里主要是：
"这个 Action / Guard 是什么类型？"
```

---

## 关键组合模式

### 最重要的组合：on + target + guard + actions

#### 完整示例
```yaml
on:
  CONNECT_REQ:
    target: CONNECTING
    guard: canAllowConnect
    actions:
      - resetSession
```

#### 不要逐字翻译 YAML，应该转换成思维流程

```text
             CONNECT_REQ
                  │
                  ▼
        ┌──────────────────┐
        │ canAllowConnect? │
        └────────┬─────────┘
             YES │
                 ▼
          resetSession()
                 │
                 ▼
           CONNECTING
```

#### 核心流程图解
```text
on      ↓ 什么时候？
guard   ↓ 能不能？
actions ↓ 做什么？  
target  ↓ 去哪儿？
```

**这是 XState FSM 最核心的语法模式。**

### 第二个最重要的组合：entry + exit

#### 示例配置
```yaml
CONNECTING:
  entry:
    - openSocket
  exit:
    - closeSocket
```

#### 心智模型
```text
                 CONNECTING
                     │
        ┌────────────┴────────────┐
        │                         │
      entry                       exit
        │                         │
        ▼                         ▼
   openSocket()              closeSocket()
```

#### 核心概念
```text
entry = 进入时初始化
exit  = 离开时清理
```

#### RAII 思想对应
这实际上非常类似 RAII 的思想：
```text
进入资源作用域 → 获得/启动资源 → 使用 → 离开作用域 → 释放/清理资源
```

**因此对于网络 FSM：**
```text
CONNECTING
    entry → open socket
    exit  → close socket
```

是非常自然的设计。

### 第三个最重要的组合：after + guard

#### 示例配置
```yaml
after:
  1200:
    target: CONNECTING
    guard: hasRetryLeft
  30000:
    target: DISCONNECTED
```

#### 执行流程
```text
进入 RETRY_WAIT
       │
       ├──── 1.2秒 ────> 还有重试次数？
       │                      │
       │                 YES  │  NO
       │                  │   │
       │                  ▼   └──> 留在这里
       │              CONNECTING
       │
       └──── 30秒 ─────────────> DISCONNECTED
```

#### 设计模式
这是：
```text
Timeout + Guard + Retry
```

**非常典型的网络状态机设计。**

---

## 最终心智模型

### 从"关键字思维"升级为"FSM 思维"

最终不要停留在：
- on 是什么？
- after 是什么？  
- guard 是什么？

应该升级成：
```text
                    Event
                      │
                      ▼
Current State ──── Transition ────> Target State
                      │
                 ┌────┴────┐
                 │         │
               guard     actions
                 │         │
              能不能？    做什么？
```

同时还有两条独立触发路径：
```text
                 Current State
                  /         \
                 /           \
              Event          Time
                │              │
               on            after
                │              │
                └──────┬───────┘
                       ▼
                  Transition
                       │
                  ┌────┴────┐
                  │         │
                guard     actions
                  │         │
                  ▼         ▼
              是否允许？   副作用
                  │
                  ▼
              target
                  │
                  ▼
             Next State
              /       \
           entry       exit
```

### 核心压缩公式

把 XState v5 压缩成一句话：

> **XState = State + Event + Transition + Guard + Action + Context**

### 关键问题导向学习

```text
State     ↓ 我现在在哪里？
Event/Time ↓ 什么事情发生了？
Guard     ↓ 条件允许吗？
Transition ↓ 我要不要迁移？
Target    ↓ 迁移到哪里？
Action    ↓ 迁移过程中做什么？
Entry/Exit ↓ 进入/离开状态时做什么？
Context   ↓ 我需要记住哪些运行时数据？
Invoke    ↓ 我要管理什么外部任务？
```

### 完整系统图

真正应该形成的 XState 心智模型不是：**背关键字**

而是：
```text
             ┌──────────────┐
             │    State     │
             │  当前阶段     │
             └──────┬───────┘
                    │
          Event / Time / Always
                    │
                    ▼
             ┌──────────────┐
             │  Transition  │
             └──────┬───────┘
                    │
                 Guard?
                /      \
              NO        YES
              │          │
              │          ├── Actions
              │          │
              │          ▼
              │       Target
              │          │
              │          ▼
              │     ┌──────────┐
              └────>│Next State│
                    └────┬─────┘
                         │
                    entry / exit
```

### 推荐学习顺序

建议按这个顺序学习：

1. **State** - 理解状态概念
2. **Event** - 理解事件驱动
3. **Transition** - 理解状态迁移  
4. **target** - 理解迁移目标
5. **guard** - 理解条件控制
6. **actions** - 理解副作用
7. **entry / exit** - 理解生命周期
8. **after** - 理解时间驱动
9. **context** - 理解数据管理
10. **always** - 理解自动迁移
11. **invoke** - 理解外部任务
12. **onDone / onError** - 理解任务完成处理

### 最值得掌握的 12 个关键字

如果按照二八原则学习，不建议一开始把 XState 所有 API 都背下来。

先掌握这 12 个：

| 优先级 | Keyword | 心智模型 |
|--------|---------|----------|
| ⭐⭐⭐⭐⭐ | `states` | 有哪些状态 |
| ⭐⭐⭐⭐⭐ | `initial` | 从哪里开始 |
| ⭐⭐⭐⭐⭐ | `on` | 收到事件怎么办 |
| ⭐⭐⭐⭐⭐ | `target` | 去哪个状态 |
| ⭐⭐⭐⭐⭐ | `guard` | 允许不允许 |
| ⭐⭐⭐⭐⭐ | `actions` | 做什么副作用 |
| ⭐⭐⭐⭐⭐ | `entry` | 进入时做什么 |
| ⭐⭐⭐⭐⭐ | `exit` | 离开时做什么 |
| ⭐⭐⭐⭐⭐ | `after` | 时间到了怎么办 |
| ⭐⭐⭐⭐ | `context` | 保存什么数据 |
| ⭐⭐⭐⭐ | `always` | 自动判断迁移 |
| ⭐⭐⭐ | `invoke` | 启动外部任务 |

**掌握这些，已经可以读懂绝大多数基础 XState FSM。**

### 一句话记住核心关键字

建议直接记这一张表：

```text
┌────────────┬────────────────────────────┐
│ id         │ 我是谁？                    │
│ initial    │ 我从哪里开始？               │
│ states     │ 我有哪些状态？               │
│ on         │ 收到事件怎么办？             │
│ after      │ 时间到了怎么办？             │
│ always     │ 不等事件，马上判断怎么办？     │
│ target     │ 我要去哪里？                 │
│ guard      │ 我允许不允许去？              │
│ actions    │ 顺便做什么？                 │
│ entry      │ 进入状态做什么？              │
│ exit       │ 离开状态做什么？              │
│ context    │ 我需要保存什么数据？          │
│ invoke     │ 我要启动什么外部任务？        │
│ onDone     │ 外部任务成功后怎么办？        │
│ onError    │ 外部任务失败后怎么办？        │
└────────────┴────────────────────────────┘
```

### 重要区分：YAML 关键字 vs XState 关键字

在配置里面存在两种东西：

#### XState 关键字
```yaml
on:
  CONNECT_REQ:
    target: CONNECTING
```

这里的 `on`、`target` 是 **XState 配置概念**

#### 用户定义名字  
```yaml
on:
  CONNECT_REQ:    # 用户定义的事件名
    target: CONNECTING  # 用户定义的状态名
```

这里的 `CONNECT_REQ`、`CONNECTING` 是**你自己定义的名字**

#### 组成关系
```text
XState Keyword + User-defined Name = Machine Configuration
```

### 实际设备 FSM 完整翻译示例

#### 配置代码
```yaml
CONNECTING:
  entry:
    - openSocket
  after:
    5000:
      target: RETRY_WAIT
      actions: recordError
  on:
    CONNECT_OK:
      target: AUTHENTICATING
    CONNECT_FAIL:
      target: RETRY_WAIT
      actions: recordError
```

#### 心智模型转换

不要把它看成 YAML，应该在脑中转换成：

```text
                    CONNECTING
                         │
                 entry: openSocket()
                         │
                         ▼
                    等待事件
                         │
             ┌───────────┼───────────┐
             │           │           │
       CONNECT_OK   CONNECT_FAIL    5秒
             │           │           │
             ▼           │           │
       AUTHENTICATING    │           │
                         │           │
                  recordError()      │
                         │           │
                         ▼           ▼
                    RETRY_WAIT <─────┘
```

**这时候你已经不是在"学习 YAML"，而是在理解：状态机在什么时候被什么东西驱动，然后如何做决策并产生状态迁移。**

---

**如果你真正掌握了这套心智模型，XState 的大部分常用关键字就会变得非常自然！**