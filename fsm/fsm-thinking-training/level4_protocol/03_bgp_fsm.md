# Lesson 03: BGP FSM (Level 4 - 网络协议)

## Step 1: 是否适合FSM

逐条检查BGP邻居会话(session)管理这个问题的"FSM特征"。

- **是否存在有限状态?**
  是。RFC4271第8节定义了6个状态:Idle、Connect、Active、OpenSent、OpenConfirm、Established。和OSPF一样,这也是"每一个BGP对等体(peer)独立维护一台状态机"的场景,但这一课要聚焦的重点不是"多实例"(上一课已经训练过),而是**状态机与多个计时器协同工作**这件事。

- **是否依赖历史状态?**
  是。比如"TCP连接失败"这个事件,在Connect状态下的反应是退到Active(意味着"曾经尝试主动连接过,但失败了,现在换成被动等待/重试策略");但同样的"TCP连接失败",如果发生在OpenSent/OpenConfirm阶段(TCP已经建立好、正在交换BGP协议报文时连接突然断了),则要退回到更早的Active而不是停留在原地——具体退到哪一步,取决于"之前到底走到了哪一步",不是无条件重置。

- **是否由事件驱动?**
  是,而且这一课的事件构成最能体现"计时器超时本身就是一种事件"这个道理:BGP至少涉及ConnectRetryTimer(连接重试计时器)、HoldTimer(保持定时器,超时判定对端失联)、KeepaliveTimer(周期性发送保活报文的计时器)三种独立计时器,每一种的到期都会驱动明确的状态转移或动作,和"收到一个BGP报文"这种网络事件在转移表里的地位完全相同。

- **是否同一个事件在不同状态行为不同?**
  是。"TCP连接建立成功"这个事件,在Connect状态下意味着可以发送OPEN报文、进入OpenSent;但如果在Active状态下收到同样的事件(此前尝试连接失败后正在重试),同样是进入OpenSent——这里反而是"不同状态、相同事件、相同目标状态"的例子,提醒我们"同一事件不同状态不同行为"不是绝对规律,有的协议里几个状态会收敛到同一个后续处理,这也是要学会辨识的一种正常情况,不必强行制造"不同行为"。真正体现差异的是"收到HoldTimer超时"——在Established状态下意味着"会话保持失败,邻居关系整个断开,退回Idle";但如果发生在更早的OpenSent/OpenConfirm阶段,同样退回更早状态,只是"退多远"依赖当前处于哪个阶段。

- **是否存在明确状态切换?**
  是,每一步(尝试建立TCP连接、交换OPEN报文协商参数、交换Keepalive确认双方都认可参数、进入正常路由交换)都有明确边界。

**适合FSM: ★★★★★**

理由:BGP作为互联网骨干路由协议,其会话管理本身就是RFC明确定义的正式状态机。这一课的独特价值在于强调**"状态机不是孤立运行的,它经常需要和若干个独立的计时器协同工作,计时器既是转移的触发者,也可能是转移动作要重新设置的对象"**——这是很多人初学FSM时容易忽略的一个维度:教材举的例子常常只有"外部事件"驱动转移,但真实系统里,状态机自己启动/停止/重置计时器,计时器超时又反过来驱动状态机,这是一种"状态机与计时器互相喂养"的协同结构。

## Step 2: 发现状态

**引导性问题:**
- 建立一个BGP会话,需要先后完成哪几件"性质完全不同"的事情(网络层连接、协议层参数协商、正式确认)?
- "TCP连接已经建立"和"BGP会话已经建立"是不是一回事?中间还差什么?
- 为什么协商阶段要拆成OpenSent和OpenConfirm两步,而不是"发完OPEN报文,收到对方OPEN报文,直接进入Established"?
- Idle和Connect、Active之间的区别是什么?看起来都是"还没连上",为什么要分成三个状态?

**思考过程:**

1. **Idle** —— 初始状态,也是任何一次异常之后最彻底的"回到起点"。这个状态下,BGP进程不会主动做任何连接尝试,处于"完全静止"的等待状态,通常需要一个`ManualStart`(管理员启用)或自动重启逻辑才会离开。值得注意:Idle不等于"从没配置过",它也是发生严重错误(比如收到格式错误的报文、认证失败)后的"安全着陆点"——协议设计者选择让几乎所有异常路径都收敛回Idle,而不是设计成"从异常发生的那个状态原地修复",这是一种"出错就退回最干净的起点,而不是尝试在原地打补丁"的稳健性设计,值得作为一种通用协议设计智慧记下来。

2. **Connect** —— 已经决定要主动发起TCP连接,正在等待底层TCP三次握手完成。这个状态下已经启动了ConnectRetryTimer——如果长时间连不上,定时器超时会触发重试(而不是无限等待或直接放弃)。

3. **Active** —— **这是最容易被误解的一个状态名**。名字叫"Active"听起来像是"活跃/已连接",但恰恰相反,它的真实含义是"之前尝试连接失败了,现在正处于不断重试建立TCP连接的过程中"——某种意义上它和Connect很像(都是在等TCP连接建立),区别在于**历史**:Connect是"第一次尝试,还没失败过";Active是"已经至少失败过一次,现在在重试"。这正好呼应Step 1的判断——区分Connect和Active,本质上是在用状态名字记录"是否已经经历过一次失败"这段历史。（RFC4271原文对这个容易混淆的命名也有专门说明,这是历史遗留的协议术语,不代表字面直觉。）

4. **OpenSent** —— TCP连接已经建立成功,并且本地已经发送了BGP的OPEN报文(里面包含AS号、Hold Time建议值、BGP版本号、Router ID等参数),正在等待对方的OPEN报文。**这里必须区分清楚:"TCP连接建立"只代表两台设备之间有了一条可靠字节流通道,和"两边同意用BGP这套参数对话"完全是两回事**——这也是本课要强调的另一层"半信半疑"语义:通道有了,但协议层的握手才刚开始。

5. **OpenConfirm** —— 双方的OPEN报文都已经收到并且本地校验通过(AS号、能力协商等没有冲突),本地也回复了Keepalive报文,正在等待对方的Keepalive确认。**为什么需要OpenConfirm这一步,而不是收到对方OPEN报文合法就直接进Established?** 因为"我认可了你发来的OPEN报文参数"和"你也确认收到并认可了我的确认"是两个方向的事情——如果没有OpenConfirm这一步,任何一方都无法确定"对方是否真的准备好开始正常的路由更新交换了",OpenConfirm本质上是协议层握手的最后一次双向确认,类似TCP三次握手里第三个ACK的作用,只是发生在应用层协议自己的报文交换上。

6. **Established** —— 双方都确认完毕,进入正常工作状态,可以开始交换UPDATE报文(路由通告/撤销),期间靠周期性的Keepalive报文和HoldTimer维持会话存活判定。

**为什么要严格区分OpenSent和OpenConfirm,而不是合并?** 因为它们对"异常"的容忍策略不同:OpenSent阶段如果TCP连接中断,直接退回Active重新尝试建连;而OpenConfirm阶段如果发生同样的中断,同样退回Active——虽然目标状态相同,但两者各自还要处理"收到格式错误的OPEN报文"(OpenSent阶段特有,直接发Notification后退Idle)和"HoldTimer在还没进入Established前就先超时"(OpenConfirm阶段更需要提防的情况,因为这时对方理论上应该很快回复Keepalive)这些细节不同的失败模式,拆开建模能让每个状态的失败处理更精确。

**确定的状态清单:**

| 状态 | 说明 | 互斥性 | 持续性 |
|---|---|---|---|
| Idle | 初始/安全着陆点,未做任何连接尝试 | 与其余状态互斥 | 直到管理员启动或自动重启逻辑触发 |
| Connect | 首次尝试建立TCP连接,等待完成 | 互斥 | 直到TCP连接成功/失败或ConnectRetryTimer超时 |
| Active | 之前连接尝试已失败,正在重试建立TCP连接 | 互斥 | 直到TCP连接成功或彻底放弃退回Idle |
| OpenSent | TCP已连接,已发送OPEN报文,等待对方OPEN | 互斥 | 直到收到合法/非法OPEN或连接中断 |
| OpenConfirm | 双方OPEN已互换且校验通过,等待对方Keepalive确认 | 互斥 | 直到收到Keepalive、HoldTimer超时或连接中断 |
| Established | BGP会话完全建立,正常交换路由信息 | 互斥 | 直到HoldTimer超时、收到Notification或连接中断 |

6个状态两两互斥,其中Connect/Active这一对的区分完全依赖"是否已经历过失败"这段历史,是本课理解上的一个重点。

## Step 3: 寻找事件

**引导性问题:**
- BGP会话涉及几种独立的计时器?它们各自的超时分别驱动什么转移或动作?
- "收到一个报文"和"这个报文携带的类型/内容"分别对应什么粒度的事件?
- 计时器超时算事件吗?如果算,它和"收到网络报文"这类事件在转移表里应该被同等对待吗?
- Keepalive报文本身除了用来触发OpenConfirm到Established的转移之外,在Established状态下还扮演什么角色?

**分析过程:**

BGP事件可以清楚地分成三类,这也是这一课最重要的分类训练:

**第一类:管理动作类事件** —— `ManualStart`(管理员启用该邻居会话)、`ManualStop`(管理员停用)。这些是"人为触发"的事件,任何状态下收到`ManualStop`都应该走向Idle(和OSPF/TCP里的RST/KillNbr类似,是一种通用退化事件)。

**第二类:网络/TCP层事件** —— `TcpConnectionSucceeds`(TCP连接建立成功)、`TcpConnectionFails`(TCP连接失败或中断)。这两个事件和"BGP协议报文内容"无关,纯粹是底层传输层的状态反馈。

**第三类:BGP协议报文事件** —— `RcvOpenValid`(收到格式和参数都合法的OPEN报文)、`RcvOpenInvalid`(收到但校验失败,比如AS号不匹配)、`RcvKeepalive`(收到Keepalive报文)、`RcvUpdate`(收到路由更新报文,仅在Established态有意义)、`RcvNotification`(收到对方主动发送的错误通知,几乎总是导致退回Idle)。

**第四类,也是本课的重点:计时器超时事件。** BGP维护三种独立计时器,分别驱动不同性质的转移:

- **ConnectRetryTimer超时** —— 发生在Connect或Active状态,表示"这一轮TCP连接尝试花了太久还没成功",触发的动作是"重新发起一次TCP连接尝试"(状态可能保持在Connect/Active不变,也可能在实现里被建模为"重新进入Connect",取决于具体实现选择——本课统一简化为"停留原地、重新发起连接"这一动作,不产生状态跳转,这是"事件驱动动作但不驱动状态改变"的又一个例子)。
- **HoldTimer超时** —— 这是"判定对端失联"的核心计时器,每收到一次对方的Keepalive或Update报文就会被重置。一旦在规定的Hold Time内没有收到任何来自对方的报文,判定会话已经不健康,直接退回Idle——不管当前处于OpenSent、OpenConfirm还是Established,这都是一条"通用退化事件"。
- **KeepaliveTimer超时** —— 这个计时器和前两个性质不同,它不驱动任何状态转移,它的唯一作用是"提醒本地该发一个Keepalive报文给对方了,以避免对方的HoldTimer超时"。**这提醒我们:不是所有计时器超时都必然导致状态跳转,有些计时器的存在纯粹是为了驱动一个周期性动作,这也应该被建模为事件,只是它在转移表里对应的"下一状态"就是"原地不动"。**

区分总结:
- **动作(不改变状态)**:KeepaliveTimer超时后发送一个Keepalive报文、ConnectRetryTimer超时后重新发起TCP连接尝试、Established状态下收到UPDATE报文并更新路由表。
- **真正触发跨状态转移的事件**:`ManualStart`、`ManualStop`、`TcpConnectionSucceeds`、`TcpConnectionFails`、`RcvOpenValid`、`RcvOpenInvalid`、`RcvKeepaliveConfirm`(OpenConfirm阶段收到的Keepalive,专门用于确认握手完成)、`RcvNotification`、`HoldTimerExpires`。
- **应忽略的输入**:在不该出现某类报文的状态下收到该报文(比如Idle状态下收到路由更新报文),生产实现通常会丢弃并记录日志,教学模型简化为忽略。

**确定的事件清单:**

| 事件 | 说明 |
|---|---|
| ManualStart | 管理员启用该邻居会话 |
| ManualStop | 管理员停用该邻居会话(通用退化事件) |
| TcpConnectionSucceeds | 底层TCP连接建立成功 |
| TcpConnectionFails | 底层TCP连接失败或中途断开 |
| ConnectRetryTimerExpires | 连接重试计时器到期,重新发起TCP连接(不跳转状态) |
| RcvOpenValid | 收到合法的OPEN报文 |
| RcvOpenInvalid | 收到但校验失败的OPEN报文 |
| RcvKeepaliveConfirm | OpenConfirm阶段收到对方Keepalive,握手正式完成 |
| RcvNotification | 收到对方的错误通知报文(通用退化事件) |
| HoldTimerExpires | 保持计时器到期,判定对端失联(通用退化事件) |
| KeepaliveTimerExpires | 保活计时器到期,提醒本地发送Keepalive(不跳转状态) |

## Step 4: 状态转移表

| Current State | Event | Next State | Action |
|---|---|---|---|
| Idle | ManualStart | Connect | 启动ConnectRetryTimer,发起TCP连接 |
| Connect | TcpConnectionSucceeds | OpenSent | 发送本地OPEN报文 |
| Connect | TcpConnectionFails | Active | 记录失败,准备重试 |
| Connect | ConnectRetryTimerExpires | Connect | 重新发起TCP连接尝试(状态不变) |
| Active | TcpConnectionSucceeds | OpenSent | 发送本地OPEN报文 |
| Active | ConnectRetryTimerExpires | Active | 重新发起TCP连接尝试(状态不变) |
| OpenSent | RcvOpenValid | OpenConfirm | 发送Keepalive,等待对方确认 |
| OpenSent | RcvOpenInvalid | Idle | 发送Notification后退回Idle |
| OpenSent | TcpConnectionFails | Active | 连接中断,回退重试 |
| OpenConfirm | RcvKeepaliveConfirm | Established | 握手完成,启动HoldTimer/KeepaliveTimer |
| OpenConfirm | HoldTimerExpires | Idle | 对方迟迟未确认,判定超时失败 |
| OpenConfirm | TcpConnectionFails | Active | 连接中断,回退重试 |
| Established | KeepaliveTimerExpires | Established | 发送Keepalive报文(状态不变) |
| Established | RcvKeepaliveConfirm / RcvOpenValid(收到对端报文即视为活跃) | Established | 重置HoldTimer(状态不变) |
| Established | HoldTimerExpires | Idle | 判定会话失效,清空已学到的路由 |
| Established | TcpConnectionFails | Idle | 连接中断,会话彻底失效 |
| (任意状态) | ManualStop | Idle | 管理员手动停用 |
| (任意状态) | RcvNotification | Idle | 收到对方错误通知,安全退回起点 |

表中省略了RFC4271附录里针对"Delay Open"、"Passive TCP Establishment"等可选行为的细节分支,教学目的是覆盖"Connect/Active重试建连 → OpenSent/OpenConfirm协议层握手 → Established"这条主干路径,以及HoldTimer超时和Notification这两类关键的失效路径。

## Step 5: ASCII状态图

```
+------+  ManualStart   +---------+  TcpConnectionSucceeds  +----------+
| Idle | -------------> | Connect | ----------------------> | OpenSent |
+------+                +---------+                          +----------+
   ^  ^                   |    ^                                |    |
   |  |    TcpConnectionFails  | ConnectRetryTimerExpires        |    | RcvOpenInvalid
   |  |                   v    | (状态不变,重新发起连接)          |    v
   |  |               +--------+                                 |  +------+
   |  |               | Active | <-------------------------------+  | Idle |(同上)
   |  |               +--------+   TcpConnectionFails               +------+
   |  |                   |    ^
   |  |   TcpConnectionSucceeds| ConnectRetryTimerExpires(状态不变)
   |  |                   v    |
   |  |               +----------+   RcvOpenValid   +------------+
   |  +---------------| OpenSent |----------------->| OpenConfirm|
   |   TcpConnectionFails(此箭头示意从OpenSent到Active,已在上方画出)
   |                                                       |    |
   |                                    RcvKeepaliveConfirm |    | HoldTimerExpires
   |                                                       v    v
   |                                              +-------------+  +------+
   |                                              | Established |  | Idle |
   |                                              +-------------+  +------+
   |                                                  |   ^
   |                    HoldTimerExpires / TcpConnectionFails   KeepaliveTimerExpires
   |                                                  |   (状态不变,发送Keepalive)
   +--------------------------------------------------+
   ManualStop / RcvNotification (可从任意状态直接回到Idle,图中未逐条画出)
```

说明:ConnectRetryTimerExpires和KeepaliveTimerExpires这两个"状态不变、只触发动作"的自环事件容易让图显得凌乱,图中用文字标注代替画自环箭头;`ManualStop`/`RcvNotification`这两个通用退化事件同样只用文字说明,不在图上为每个状态单独画一条回Idle的箭头。

## Step 6: Python实现

完整代码见同目录下的 `03_bgp_fsm.py`。这一课的代码重点是"状态机和计时器数据如何放在一起"——用`dataclass`携带每种计时器的剩余时间,而不是用裸的浮点数散落在类的各处属性里。

**v1:Enum + match-case版**
`BGPPeerV1`内部除了`state`字段,还有`hold_time`、`connect_retry_time`等计时器配置,`handle`方法用`match`直接处理转移,计时器超时以"事件"的形式喂给`handle`,不做真实的wall-clock计时(教学模拟,不需要真实网络I/O)。

**v2:Transition Table版**
把主干转移整理成字典,`ConnectRetryTimerExpires`/`KeepaliveTimerExpires`这类"状态不变"的转移也老老实实作为表里的一行(`next_state`等于`current_state`),而不是用特殊分支逃避——这样"这台状态机到底认识哪些事件"这个问题,查一遍字典的key就有完整答案,不会有事件"藏"在特殊分支里被漏掉。

**v3:State Pattern版**
用一个`@dataclass`表示"计时器组"(`TimerBank`,携带`connect_retry_remaining`、`hold_remaining`、`keepalive_remaining`),每个状态类在被"tick"（模拟时间推进）时decrement计时器并在归零时产生对应事件,体现"状态机和计时器协同工作"的具体代码形态。

**v4:通用FSM框架 + 计时器管理**
复用通用FSM引擎,额外加一个简单的"事件队列 + tick驱动"的模拟循环:每次tick模拟计时器递减,归零则注入对应超时事件给FSM处理,收到外部报文事件时重置相应计时器——这是最接近真实实现思路的一版:状态机不是被动等待外部调用,而是有一个持续运行的"时间推进 + 事件处理"主循环。

运行`python3 03_bgp_fsm.py`可以看到一次从Idle到Established的完整会话建立过程,以及一次HoldTimer超时导致会话跌回Idle的演示。

## Step 7: 复杂度分析

**为什么FSM比if-else更好?**
BGP会话管理涉及至少3种计时器和10种以上事件的组合,如果不用状态机,状态本身还是需要用某种方式表达(哪怕只是字符串),但转移逻辑会散落成大量"如果当前状态是X且事件是Y且计时器Z没有超时"这样的多条件判断,尤其是"计时器超时后要不要跳转状态"这种规则,如果不集中在一张转移表里管理,极容易出现"某个状态忘记处理某个计时器超时"的遗漏——生产环境里BGP会话卡死或者错误地保持在不该保持的状态,很多时候正是这类遗漏的转移分支导致的。

**状态机与计时器协同工作,给设计带来了什么额外复杂度?**
计时器本身也是有状态的(距离下次超时还剩多久),这意味着"进入某个状态"这个动作往往不只是切换state字段,还要连带"启动/停止/重置某个计时器"这个副作用,这要求转移表的Action列不能只写"发送某个报文",还要清楚地写明"顺带启动/取消了哪个计时器"——这是本课在Step 4表格里刻意保留Action列细节的原因:少了这一列,读者会误以为BGP状态机只是"报文驱动"的,而遗漏计时器这个同样重要的驱动源。

**什么情况下FSM反而是负担?**
如果只是写一个"心跳检测"脚本(定期ping一次,超过N次没回应就报警),这种"只有一个计时器、状态最多两三个(健康/不健康)"的场景,不需要套用BGP这种6状态+3计时器的复杂结构——是否需要"协商阶段"这种精细状态,取决于你的协议是否真的需要在正式工作前完成一轮参数协商;纯粹的健康检测通常不需要。

**什么时候不应该使用FSM?**
如果计时器超时后的处理逻辑,不管当前处于哪个状态,永远是同一个动作(比如"不管什么状态,超时就直接报错退出"),那就不需要为每个状态单独定义"计时器超时该怎么办",一个全局的超时处理器就够了,没必要把这条规则拆散到状态机的转移表里重复。

**协议标准文档几乎总是直接给出状态图和状态表,这一现象说明了什么?**
RFC4271附录里甚至给出了一张比正文更详细的伪代码状态机实现建议(哪些事件在哪些状态下该被忽略、该被记录日志),这说明协议设计者非常清楚"如果不把所有(状态,事件)组合都显式定义清楚,不同厂商的实现之间就会出现行为不一致",这正是"表驱动/转移表"这种设计相比零散if-else最大的工程价值:它天然逼迫设计者穷举所有组合,而不是只覆盖"想得到的那几种"。

## Step 8: FSM思维总结

```
==========
FSM识别口诀(BGP篇)
==========
✓ 计时器超时和外部报文到达,在转移表里应该被同等对待——都是"事件"
✓ 有些事件触发的是动作而非状态跳转(自环转移),要如实写进表里,不要用特殊分支逃避
✓ 相近的状态名字(Connect vs Active)如果语义不同,差异往往藏在"是否经历过失败"这类历史信息里
✓ 进入新状态的动作,常常伴随着"启动/取消/重置某个计时器"这个容易被遗漏的副作用
✓ 几乎所有异常路径收敛回同一个"安全起点"状态,是一种值得借鉴的稳健性设计
```

## Step 9: 迁移训练题

1. RIP(路由信息协议)虽然是距离矢量协议,结构远比BGP简单,但同样依赖"路由计时器(Route Timeout)"和"垃圾回收计时器(Garbage-Collection Timer)"来判定一条路由是否失效。如果为一条具体的路由表项(而不是整个协议进程)建模一台FSM,你会设计哪些状态?这两个计时器分别对应哪些转移?
2. MQTT协议的客户端连接同样有"Connecting → Connected → Disconnected"这样的会话状态,并且依赖`Keep Alive`间隔配合PINGREQ/PINGRESP报文判断连接是否存活。对照本课的BGP KeepaliveTimer/HoldTimer机制,MQTT的Keep Alive机制和它是同一种设计思路吗?如果要你画MQTT客户端连接的FSM,状态数量预期比BGP多还是少?
3. 一个分布式系统里常见的"租约(lease)续约"机制(比如Kubernetes里的Lease对象、ZooKeeper的会话租约)本质上也是"靠计时器超时判定对方是否还活着,靠周期性心跳/续约来维持"。这种"续约类"状态机和BGP的会话保持机制相比,是否存在结构上的相似性?如果相似,它们各自的"HoldTimer等价物"和"KeepaliveTimer等价物"分别是什么?

## 今天真正学会的一句话

当一台状态机不是只被"外部事件"推着走,而是自己启动、维护、依赖若干个独立计时器的到期时间时,不要把计时器超时和其他事件区别对待——它就是一种事件,应该被放进同一张转移表里,唯一需要额外留心的是:进入某个状态时,往往也需要顺带启动或重置某个计时器,这是状态切换动作里最容易被遗漏的一环。
