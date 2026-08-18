# 第 14a 课：五个代表例（Actor + Mailbox）

主例「两条教学 TCP 连接」的完整实现在 `code/`。其余四例用来对照：同一观念——每个实例一份隔离状态 + 邮箱 + 单线程 `dispatch`——换一个名词。

本课不实现 Akka，不把四例写成可运行程序。复杂度用「角色个数 × 每角色状态个数」衡量，不要用乘积状态机去算。

---

## 例 1：连接管理器（主例）

**问题**：两条连接同时活着。一条失败时，另一条必须继续收数据。任何处理函数都不得写邻居的 `state`。

**状态图**：见 `LESSON.md`。每条连接一份 `CLOSED / SYN_SENT / ESTABLISHED / ERROR`。

**状态表**：`CONNECT`、`SYNACK`、`DATA`、`FAIL`、`RESTART`；监护者只对出错的那一个入队 `RESTART`。

**实现**：`code/c/actor.c`、`code/cpp/actor.cpp`、`code/python/actor.py`。

**复杂度**：2 个 Actor × 4 个扁平状态；每角色邮箱容量 8。不是 16 个组合状态。

**工业用途**：进程内连接表、教学用会话管理器。真实栈还有套接字与定时器线程，那些是第 08 / 09 课的输入来源，本课用 `actor_send` 假装。

**常见缺陷**：`on_fail` 里把整张连接表清零；或 `fsm_dispatch(&other->fsm, ev)` 同步改邻居。

**重构**：把连接 FSM 换成第 08 课的表驱动，调度器一行都不用动。

---

## 例 2：GUI 控件角色（文字）

**问题**：输入框、按钮、列表各有焦点 / 启用 / 禁用局面。鼠标与键盘事件同时到达。若所有控件共享一个 `focused` 布尔，点击按钮会把输入框的焦点状态踩掉。

每个控件一个 Actor。邮箱里是 UI 事件：`CLICK`、`KEY`、`FOCUS`、`BLUR`、`DISABLE`。

```
+-----------+  CLICK / FOCUS   +----------+  DISABLE   +----------+
| IDLE      | ---------------> | FOCUSED  | ---------> | DISABLED |
+-----------+                  +----------+            +----------+
     ^                              | BLUR                  |
     +------------------------------+                       |
     +------------------------------------------------------+ ENABLE
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| IDLE | CLICK | （无） | take focus | FOCUSED |
| FOCUSED | BLUR | （无） | yield focus | IDLE |
| FOCUSED | KEY | （无） | insert text | FOCUSED |
| * | DISABLE | （无） | ignore input | DISABLED |
| DISABLED | ENABLE | （无） | （无） | IDLE |

控件 A **不得** 写控件 B 的 `state`。若按钮按下应让输入框失焦：按钮的动作里 `actor_send(textbox, BLUR)`。窗口级调度器轮询各控件邮箱（多数 GUI 框架本来就是单线程事件循环）。

**复杂度**：控件个数 × 约 3 态。不要做成「按钮焦点 × 输入框焦点」一张表。

**工业用途**：桌面与嵌入式 GUI 的控件树。真实框架还有布局与绘制；那些不是 FSM。

**常见缺陷**：全局 `active_widget` 指针，所有事件先改这个指针再改控件字段，两个字段不同步。

**重构**：焦点可以用「窗口角色向唯一一个控件发 `FOCUS`、向旧的发 `BLUR`」，仍然是消息，不是共享可变焦点对象。

---

## 例 3：电信呼叫会话

**问题**：每路呼叫有自己的信令局面（空闲、振铃、通话、释放）。中继上同时有许多路。一路释放不该把邻路拆掉。

每路呼叫一个 Actor。邮箱：`SETUP`、`ALERT`、`ANSWER`、`HANGUP`、`FAIL`。

```
+------+  SETUP   +--------+  ANSWER   +---------+  HANGUP   +--------+
| IDLE | -------> | RINGING| --------> | TALKING | --------> | IDLE   |
+------+          +--------+           +---------+           +--------+
                     | FAIL                 | FAIL
                     v                      v
                  +-------+ <---------------+
                  | ERROR | --RESTART--> IDLE
                  +-------+
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| IDLE | SETUP | （无） | send ringing | RINGING |
| RINGING | ANSWER | （无） | cut through | TALKING |
| TALKING | HANGUP | （无） | release | IDLE |
| RINGING 或 TALKING | FAIL | （无） | drop call | ERROR |
| ERROR | RESTART | （无） | clear call | IDLE |

监护者重启的是 **这一路** 的角色。信令点（交换机）上的别的呼叫继续 `TALKING`。

**复杂度**：呼叫路数 × 4 态。工业上还有计费、媒体、保持；那些或是正交数据，或是另一类 Actor，不要乘进信令枚举。

**工业用途**：软交换 / 呼叫处理里的每呼叫控制块。教学不实现真实 ISUP / SIP。

**常见缺陷**：一张全局 `call_state`，或失败时重启整个进程里的所有呼叫。

**重构**：媒体面可以是另一个 Actor，信令只向它 `actor_send(START_MEDIA)`，对应第 10 课「两台机」叠在本课「每路一份」上。

---

## 例 4：物联网设备影子

**问题**：云端为每台设备保留一份「期望 / 报告」影子。设备 A 离线不应把设备 B 的影子标成故障。对每台设备的命令必须串行，以免 `desired` 被两个控制台同时写乱。

每台设备一个 Actor。邮箱：`REPORT`、`DESIRE`、`TIMEOUT`、`FAIL`、`RESTART`。

```
+----------+  DESIRE   +------------+  REPORT (match)   +----------+
| IN_SYNC  | --------> | UPDATING   | ----------------> | IN_SYNC  |
+----------+           +------------+                   +----------+
     |                      | TIMEOUT / FAIL
     | FAIL                 v
     +----------------> +---------+ --RESTART--> IN_SYNC (empty desired)
                        | ERROR   |
                        +---------+
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| IN_SYNC | DESIRE | （无） | push desired | UPDATING |
| UPDATING | REPORT | 报告匹配期望 | ack | IN_SYNC |
| UPDATING | TIMEOUT | （无） | mark stale | ERROR |
| IN_SYNC 或 UPDATING | FAIL | （无） | isolate device | ERROR |
| ERROR | RESTART | （无） | clear desired | IN_SYNC |

单线程 `dispatch` 保证同一设备的 `DESIRE` 不会交错写。设备 B 的 `REPORT` 在自己的邮箱里排队，不看 A 的 `state`。

**复杂度**：设备数 × 3 态。影子文档（JSON）是数据，不是状态；状态只描述「是否在等设备确认」。

**工业用途**：设备影子 / 数字孪生的控制面。教学不实现 MQTT。

**常见缺陷**：一个全局 `devices[]` 上的锁里直接改所有影子；一台设备超时把批处理循环 `break` 掉。

**重构**：超时用第 09 课手法变成 `TIMEOUT` 消息入 **该** 设备邮箱，不要在定时器回调里写 `state`。

---

## 例 5：游戏实体

**问题**：每个可动对象（玩家、怪物、投射物）有 Idle / Move / Hit / Dead。物理与输入同时产生事件。若在怪物受击函数里写 `player.state = Dead`，一次范围伤害会把逻辑顺序搅乱，也没法单独重置一只怪。

每个实体一个 Actor。邮箱：`TICK`、`HIT`、`DIE`、`SPAWN`。

```
+------+  TICK (input)   +------+  HIT   +-----+  DIE    +------+
| IDLE | --------------> | MOVE | -----> | HIT | ------> | DEAD |
+------+                 +------+        +-----+         +------+
     ^                      | TICK (arrive)                  |
     +----------------------+                                |
     +------------------ SPAWN / RESTART --------------------+
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| IDLE | TICK | 有移动输入 | start move | MOVE |
| MOVE | TICK | 到达 | stop | IDLE |
| MOVE 或 IDLE | HIT | 仍有生命 | play hit | HIT |
| HIT | DIE | 生命 <= 0 | corpse | DEAD |
| DEAD | SPAWN | （无） | reset | IDLE |

造成伤害：攻击者 `actor_send(target, HIT)`，由目标自己减生命并决定是否 `DEAD`。范围伤害 = 对每个目标发一条消息，而不是循环写 `mobs[i].state`。

某只怪进入无法恢复的错误（寻路崩溃）时，监护者只 `SPAWN` / `RESTART` 它；玩家仍在 `MOVE`。

**复杂度**：实体数 × 4 态。位置、血量是上下文整数，不要做成 `HIT_3HP` 这种状态名（第 07 课已经禁止把计数写进枚举）。

**工业用途**：游戏对象的逻辑帧。渲染在另一条流水线读快照，不写 `state`。

**常见缺陷**：在碰撞回调里直接改双方状态；或用一个全局 `game_state` 枚举表示「整局」。

**重构**：物理引擎产出 `HIT` 消息，不拥有实体 FSM。这和第 08 课「假报文进 `dispatch`」同一边界。
