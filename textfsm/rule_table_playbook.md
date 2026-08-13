# 规则表分析实战：从场景到规则表的完整思维过程

## 说明

[state_ruletable.md](state_ruletable.md) 已经给出了 13 个场景**完整可运行的 Python 代码**（状态图、规则表、代码、输出）。但那份文档回答的是"规则表长什么样、代码怎么写"，没有回答一个更关键的问题：

> **拿到一个新问题时，这张规则表到底是怎么分析出来的？**

这份文档就是专门补这一课的。同样的 13 个场景，这里不再重复贴可运行代码，而是把"分析过程"完整摊开——怎么找候选状态、怎么判断该合并还是拆分、事件和动作从哪里来、什么时候需要 Guard、什么时候需要通配符。目标是让你以后遇到全新的问题，也能自己走一遍这个流程，而不是背下 13 个例子的答案。

每个场景需要的完整可运行代码，都可以在 [state_ruletable.md](state_ruletable.md) 里找到对应编号的示例。

---

# 一、通用分析方法

在看 13 个例子之前，先把方法论讲清楚。下面 5 步是所有例子共用的分析框架：

```
问题描述
   |
   v
第1步：列出候选状态（不做取舍，先罗列）
   |
   v
第2步：用三原则过滤/合并（互斥、穷尽、有限）
   |
   v
第3步：列出事件（Event）
   |
   v
第4步：列出动作（Action）
   |
   v
第5步：处理特殊情况（Guard / 非法转换 / 通配符 ANY）
   |
   v
规则表
```

## 第1步：列出候选状态

方法很朴素：把问题描述读一遍，圈出所有"名词化的阶段/位置/模式"，先不做任何取舍地罗列出来。例如"门打开一半""正在收钱""等待经理审批"，都是候选。这一步的目标不是精确，而是**不遗漏**。

## 第2步：用三原则过滤/合并

对着候选列表，逐个做三个检验（详见 [state_ruletable.md 第二节](state_ruletable.md)）：

- **互斥性**：两个候选是否可能同时为真？如果可能，说明它们其实是两个正交的维度（比如"是否付款"和"是否发货"），不应该合并进同一个状态变量，而应该分别处理——但也不能各自拆成布尔变量堆在一起（那是第二节讲的误区），正确做法是想清楚它们组合出的"业务上真正存在的阶段"，把这些阶段本身列为状态。
- **穷尽性**：能不能想出一个不属于任何候选状态的情况？如果能，说明还缺状态，或者需要一个显式的"未知/其他"兜底状态。
- **有限性**：这个候选是不是一个连续变化的数字（金额、次数、剩余时间、坐标）？如果是，它应该是**数据**，而不是状态——数据会在第 5 步以 Guard 的形式重新出现。

这一步做完，候选列表会明显收敛，剩下的就是真正的状态集合。

## 第3步：列出事件（Event）

问自己：什么会让系统离开当前状态？把这些"会触发变化的输入"列出来。如果一个输入无论如何都不会引起状态变化，它甚至不需要出现在规则表里。

有一种常见情况是：原始输入不是干净的事件符号，而是一行文本、一个字符、一个数据包，需要先做一次**归一化**（分类）才能变成事件——示例5、示例13都是这种情况。

## 第4步：列出动作（Action）

每一次迁移，系统需要做什么？把这些动作列出来，动作应该是"做一件具体的事"，而不是"判断该做什么"——判断逻辑属于第 5 步的 Guard，不属于 Action。

## 第5步：处理特殊情况

三个信号，出现任何一个都要特别处理：

- **需要 Guard**：发现"同一个状态收到同一个事件，结果却要看情况"——这时候不要加新状态，也不要写 if，而是给这个 `(State, Event)` 配上一组按优先级排列的候选规则，每条带一个 Guard 条件。
- **需要通配符 ANY**：发现"某个事件不管当前处于哪个状态，处理方式都完全一样"——这时候不要给每个状态复制一条规则，而是用 `(ANY, event)` 写一条。
- **需要明确"非法转换"**：把规则表里没覆盖的 `(state, event)` 组合过一遍，为每一个空白格子想清楚：这是"业务上不可能发生"，还是"目前故意不处理"，还是"真的漏掉了"。分不清的，先当作非法转换处理（拒绝/报错），而不是静默忽略。

下面 13 个场景，全部严格按这 5 步展开。

---

# 二、示例1：自动门

**场景**：一扇自动感应门。收到开门信号后电机开始转动，门缓缓打开；电机检测到门已经完全开启后停止转动；开启一段时间没有新的触发后开始关门；电机检测到门已经完全关闭后停止转动。

**分析过程**：

1. **候选状态**：从描述里能圈出的阶段有——"门关着""正在开门（电机转动中）""门开着""正在关门（电机转动中）"。
2. **三原则检验**：
   - 互斥：门在任一时刻只能处于这四种物理状态之一，满足。
   - 穷尽：门的物理位置只有"关/开/两者之间"，而"两者之间"又要区分方向（开门中还是关门中），四个状态覆盖了全部可能，满足。
   - 有限：4 个，满足。
   - 关键判断点：**"开门中"和"开着"能不能合并成一个"开"状态？** 不能——如果合并，电机还在转动时收到"再次触发"信号，程序无法区分"门刚开始动"和"门已经到位在等待关门"，会导致误判（比如门还没开到位就提前开始关门计时）。这就是为什么运动过程本身也要单独建状态的原因：**只要"正在做一件事"和"已经做完这件事"会引发不同的处理逻辑，它们就必须是两个状态。**
3. **事件列表**：`button`（收到开门触发信号）、`opened`（电机检测到已到位）、`timeout`（开着超过一定时间无新触发）、`closed`（电机检测到已关闭到位）。
4. **动作列表**：`open()` 启动电机正转、`stop()` 停止电机、`close()` 启动电机反转。
5. **特殊情况**：本例没有 Guard、没有通配符，是最基础的线性循环。但值得留一个思考题：`(Closing, button)` 这个组合——正在关门时又有人靠近——规则表里没有覆盖，属于"真的漏掉了"还是"故意不处理"？工程上通常会补一条 `(Closing, button) -> Opening`，这正是"规则表完整性自检"要帮你发现的那类问题（见 [state_ruletable.md 第十三节](state_ruletable.md)）。

**规则表**：

|State|Event|Action|Next|
|------|------|------|------|
|Closed|button|open()|Opening|
|Opening|opened|stop()|Opened|
|Opened|timeout|close()|Closing|
|Closing|closed|stop()|Closed|

**核心处理伪代码**：

```
state = Closed
for event in 事件序列:
    if (state, event) not in table:
        处理非法转换（报错 / 忽略）
        continue
    action, next_state = table[(state, event)]
    执行 action
    state = next_state
```

---

# 三、示例2：TCP 三次握手（服务端视角）

**场景**：服务端 socket 处于监听状态，等待客户端发起连接；收到 SYN 后回复 SYN+ACK；收到最后的 ACK 后连接建立。只考虑正常路径，不含超时重传、RST 等异常分支。

**分析过程**：

1. **候选状态**：从协议描述里能直接抠出——"监听中""收到 SYN 等待 ACK""连接已建立"。
2. **三原则检验**：互斥（握手过程任一时刻只处于一个阶段）、穷尽（正常路径被完整覆盖）、有限（3 个，真实 TCP 完整状态机是 11 个）——全部满足。这个例子的特殊之处在于：**状态名不是我们发明的，而是协议标准里现成的**——LISTEN、SYN_RCVD、ESTABLISHED。这反过来验证了这套分析方法：真正的工业级协议设计，走的也是同一套"识别互斥、穷尽、有限的阶段"的思路。
3. **事件列表**：`SYN`（收到 SYN 包）、`ACK`（收到 ACK 包）。
4. **动作列表**：`Send SYN+ACK`、`Connection Established`（回调通知上层连接已建立）。
5. **特殊情况**：本例没有 Guard。但"非法转换默认值"这个原则在网络协议里有非常现实的意义——`LISTEN` 状态收到一个 `ACK`（不该在此时出现），规则表里没有这一项，内核网络栈会直接丢弃或回复 RST，而不需要额外写检查代码。**状态机本身就是一层免费的输入合法性校验**，这也是为什么协议解析代码天生适合用状态机实现，能天然抵御大量畸形报文攻击。

**规则表**：

|State|Event|Action|Next|
|------|------|------|------|
|LISTEN|SYN|Send SYN+ACK|SYN_RCVD|
|SYN_RCVD|ACK|Connection Established|ESTABLISHED|

**核心处理伪代码**：

```
state = LISTEN
for packet in 收到的报文序列:
    if (state, packet.type) not in table:
        丢弃报文 / 回复 RST（非法转换）
        continue
    action, next_state = table[(state, packet.type)]
    执行 action
    state = next_state
```

---

# 四、示例3：订单系统

**场景**：一个简化的电商订单，从下单到完成，依次经过支付、发货、收货三个动作。

**分析过程**：

1. **候选状态**：口头描述里能圈出——"新建的""付过款的""在路上的""用户收到货的"。
2. **三原则检验 + 常见误区对照**：这里最容易踩的坑是用多个布尔变量表达（`is_paid`、`is_shipped`），而不是一个状态变量。判断方法：先问"这两个布尔量是不是相互独立的两个维度"——如果 `is_shipped=True` 而 `is_paid=False` 在业务上根本不可能出现，说明它们不是独立维度，而是**同一条业务时间线上的先后阶段**，理应合并成一个状态变量。用 `OrderState` 校验三原则：互斥（订单不可能同时是 NEW 和 SHIPPED）、穷尽（覆盖订单生命周期全部阶段）、有限（4 个）——全部满足，因此确定为 `NEW / PAID / SHIPPED / FINISHED`。
3. **事件列表**：`pay`、`ship`、`receive`。
4. **动作列表**：更新订单记录、发送通知。
5. **特殊情况**：本例简化版没有 Guard、没有通配符。但可以预告一个思考钩子：真实业务里"取消订单"应该能在 `NEW`、`PAID` 等多个未完成状态下触发，且处理逻辑几乎一样（回滚库存、退款），这正是示例8、示例11里"通配符 ANY"要解决的问题的雏形。

**规则表**：

|State|Event|Action|Next|
|------|------|------|------|
|NEW|pay|支付成功|PAID|
|PAID|ship|开始发货|SHIPPED|
|SHIPPED|receive|订单完成|FINISHED|

**核心处理伪代码**：

```
state = NEW
for event in 事件序列:
    if (state, event) not in table:
        处理非法转换
        continue
    action, next_state = table[(state, event)]
    执行 action
    state = next_state
```

---

# 五、示例4：TextFSM 最小实现

**场景**：解析网络设备命令行输出，每一行形如"接口名 up/down"，需要把纯文本转换成结构化记录列表。

**分析过程**：

1. **候选状态**：这里状态不再是"业务阶段"，而是"当前正在解析文本的哪个区域"。对于这个最简单的输入格式（每一行都是独立、同构的接口状态行），全程只存在一种解析上下文，所以候选状态只有一个：`Start`。
2. **三原则检验**：只有一个候选，互斥/穷尽自动满足，有限（1 个）也满足。这是一个很好的"最小状态集合"范例——**不要为了套用模式而生造不需要的状态**。只有当文本格式变复杂（比如出现分段的"Interface 块"和"IP 块"，不同区域需要不同的正则去匹配）时，才需要拆出 `Interface`、`IP` 等新状态（[mental_model.md](mental_model.md) 里有更复杂的例子）。
3. **事件列表**：这里的"事件"不是命名好的符号，而是"当前这一行文本是否匹配某个正则模式"——即行是否匹配 `(\S+)\s+(up|down)`。
4. **动作列表**：`Record`（提取字段，追加到结果列表）。
5. **特殊情况**：不匹配任何规则的行该怎么处理？本例简化为"跳过"；真实 TextFSM 里有专门的 `Error` 动作，能让"不认识的行"直接终止解析并报错——这也是"非法转换默认值不应该被静默忽略"这一原则的具体体现。

**规则表**：

|State|Match（正则）|Action|Next|
|------|------|------|------|
|Start|`(\S+)\s+(up\|down)`|Record|Start|

**核心处理伪代码**：

```
state = Start
records = []
for line in text_lines:
    for (s, pattern, action, next_state) in rules:
        if s != state:
            continue
        m = pattern.match(line)
        if m:
            执行 action(m, records)
            state = next_state
            break
    # 循环走完没有 break，说明这一行不匹配任何规则（可选择跳过或报错）
```

---

# 六、示例5：HTTP 请求解析器（极简版）

**场景**：解析一个 HTTP 请求报文，按行处理：请求行、若干 Header、一个空行、Body 数据。

**分析过程**：

1. **候选状态**：请求报文天然分成几个物理分段——"正在解析请求行""正在解析 Header""正在读 Body"。空行本身只是触发迁移的事件，不单独作为一个状态。
2. **三原则检验**：互斥（当前只能处于其中一个分段的解析上下文）、穷尽（HTTP 报文结构固定就这几段）、有限（3 个）——满足，因此确定为 `REQUEST / HEADER / BODY`。
3. **事件列表**：`line`（请求行）、`header`（一行 Header）、`blank`（空行）、`data`（Body 数据）——这里的事件是"行的类型"，需要先对原始文本行做一次分类（归一化）才能得到。
4. **动作列表**：解析请求行、保存 Header、开始 Body、读取 Body。
5. **特殊情况**：`HEADER` 状态收到 `line` 事件，或者 `REQUEST` 状态收到 `header` 事件，都是不应该出现的组合——这意味着报文格式错误，应该报错而不是静默处理，这也是"非法转换默认值"原则的又一次体现。

**规则表**：

|State|Event|Action|Next|
|------|------|------|------|
|REQUEST|line|解析请求行|HEADER|
|HEADER|header|保存Header|HEADER|
|HEADER|blank|开始Body|BODY|
|BODY|data|读取Body|BODY|

**核心处理伪代码**：

```
state = REQUEST
for line in lines:
    event = classify(line)             # 归一化：原始文本行 -> 事件类型
    if (state, event) not in table:
        报错（报文格式错误）
        continue
    action, next_state = table[(state, event)]
    执行 action(line)
    state = next_state
```

---

# 七、示例6：交通信号灯

**场景**：十字路口信号灯，按固定周期红灯、绿灯、黄灯循环，不考虑行人按钮、车流传感器等复杂输入。

**分析过程**：

1. **候选状态**：灯本身只能呈现三种颜色——红、绿、黄。
2. **三原则检验**：互斥（灯不能同时亮两种颜色）、穷尽（这就是全部可能的颜色）、有限（3 个）——满足，这是三原则最直白的教科书案例。
3. **事件列表**：只有一个事件——`timer`（定时器到时）。这是"事件种类最少"的极端情况，用来说明**事件数量少不代表不需要规则表**——哪怕只有一种事件，只要状态之间的迁移关系需要被清楚记录下来（谁的下一个是谁），规则表依然是最简洁的表达方式，比写三个 if 更直观。
4. **动作列表**：切换到下一个颜色（点亮新颜色、熄灭旧颜色）。
5. **特殊情况**：无 Guard、无通配符、无需要特别处理的非法转换——状态空间是一个纯循环，任一时刻都只有唯一合法的"下一个状态"。

**规则表**：

|State|Event|Action|Next|
|------|------|------|------|
|Red|timer|绿灯亮起|Green|
|Green|timer|黄灯亮起|Yellow|
|Yellow|timer|红灯亮起|Red|

**核心处理伪代码**：

```
state = Red
loop 每次定时器触发:
    action, next_state = table[(state, "timer")]
    执行 action
    state = next_state
```

---

# 八、示例7：自动售货机

**场景**：顾客先选择商品，然后投币；投够钱后按出货键才能出货；钱不够会提示还差多少。

**分析过程**：

1. **候选状态**：售货机的两个操作上下文——"空闲，还没选任何商品""已经选定商品，正在收钱"。
2. **三原则检验**：互斥（不可能同时"还没选"和"已经选了在收钱"）、穷尽（这两个阶段覆盖了单次购买流程的全部阶段）、有限（2 个）——满足，确定为 `Idle / Selecting`。这里要特别注意区分"状态"和"数据"：**已投入金额（inserted）是一个连续变化的数字**，不满足有限性，所以它不应该被做成状态（不应该有"已投1元/已投2元/已投3元"这种状态），而应该作为上下文数据挂在 `Selecting` 状态之外单独维护。
3. **事件列表**：`select`（选择商品）、`insert_coin`（投币）、`dispense`（按下出货键）。
4. **动作列表**：记录单价、累加金额、出货找零、提示余额不足。
5. **特殊情况**：`(Selecting, dispense)` 这一个 `(状态, 事件)` 组合，会因为"金额是否够"产生两种完全不同的结果——**这正是需要 Guard 的信号：当你发现同一个状态收到同一个事件，结果却要"看情况"，说明这里缺一个 Guard，而不是缺一个新状态**。把 Guard 条件写成"金额 >= 单价"，两条候选规则按优先级排列即可，完全不需要为"钱不够"单独发明一个新状态。

**规则表**：

|State|Event|Guard|Action|Next|
|------|------|------|------|------|
|Idle|select|-|记录单价|Selecting|
|Selecting|insert_coin|-|累计金额|Selecting|
|Selecting|dispense|金额 >= 单价|出货找零|Idle|
|Selecting|dispense|金额 < 单价|提示不足|Selecting|

**核心处理伪代码**：

```
state = Idle
for (event, ctx) in 事件流:
    candidates = table.get((state, event))
    if candidates is None:
        处理非法转换
        continue
    for (guard, action, next_state) in candidates:
        if guard is None or guard(ctx):
            执行 action(ctx)
            state = next_state
            break
    else:
        处理"所有 Guard 都不满足"
```

---

# 九、示例8：登录失败锁定

**场景**：登录系统在连续失败达到阈值后锁定账户，需要额外的解锁操作才能恢复；无论账户当前是否已锁定，管理员随时都可以触发解锁。

**分析过程**：

1. **候选状态**："正常，可以登录""已锁定，不可登录"——两个候选。
2. **三原则检验**：互斥、穷尽、有限（2 个）——满足，确定为 `Normal / Locked`。同样要区分状态和数据：**失败次数（count）是一个持续变化的整数**，不应该拆分成"失败1次/失败2次/失败3次"三个状态（那会导致状态数量随阈值增长而爆炸），而应该作为 Guard 判断用的上下文数据。
3. **事件列表**：`login_fail`、`login_success`、`unlock`。
4. **动作列表**：记录失败次数并提示、锁定账户、登录成功清零计数、解锁清零计数。
5. **特殊情况**：
   - **Guard**：`(Normal, login_fail)` 要不要真正锁定，取决于失败次数是否达到阈值——判断标准与示例7一致。
   - **通配符**：`unlock` 这个事件的语义是"不管你现在是不是锁定状态，只要触发解锁就回到 Normal"。判断一个事件是否适合用 `ANY` 通配符的标准是：**这个事件的处理逻辑与"当前具体是哪个状态"无关，只与"发生了这件事"本身有关**。如果为 `Normal` 和 `Locked` 分别写一条 `unlock` 规则，两条规则的 Action 和 NextState 会完全一样——这种"重复"就是通配符该出场的信号。

**规则表**：

|State|Event|Guard|Action|Next|
|------|------|------|------|------|
|Normal|login_fail|失败次数 >= 阈值|锁定账户|Locked|
|Normal|login_fail|失败次数 < 阈值|提示失败次数|Normal|
|Normal|login_success|-|登录成功、清零计数|Normal|
|ANY|unlock|-|解锁、清零计数|Normal|

**核心处理伪代码**：

```
state = Normal
for (event, ctx) in 事件流:
    candidates = table.get((state, event)) or table.get((ANY, event))
    if candidates is None:
        处理非法转换（例如 Locked 状态收到 login_success）
        continue
    for (guard, action, next_state) in candidates:
        if guard is None or guard(ctx):
            执行 action(ctx)
            state = next_state
            break
    else:
        处理"所有 Guard 都不满足"
```

---

# 十、示例9：电梯调度器

**场景**：电梯接到呼叫后前往目标楼层；如果呼叫的楼层恰好就是当前楼层，直接开门，不需要移动。

**分析过程**：

1. **候选状态**："待命""正在移动""门开着"——三个候选。
2. **三原则检验**：互斥（电梯不可能同时待命又在移动）、穷尽（覆盖一次调度的全部物理阶段）、有限（3 个）——满足，确定为 `Idle / Moving / DoorOpen`。
3. **事件列表**：`call`（呼叫）、`arrived`（到达目标楼层）、`door_timeout`（开门等待超时）。
4. **动作列表**：开始移动、到达开门、直接开门（跳过移动）、关门。
5. **特殊情况**：**Guard——`(Idle, call)` 要不要经过 `Moving`，取决于"目标楼层是否等于当前楼层"**。这是一个典型的"同一事件，根据数据决定是否需要跳过某个中间状态"的场景，说明 Guard 不仅能决定"结果状态是什么"，还能决定"要不要走完整条路径"。

**规则表**：

|State|Event|Guard|Action|Next|
|------|------|------|------|------|
|Idle|call|目标楼层 == 当前楼层|直接开门|DoorOpen|
|Idle|call|目标楼层 != 当前楼层|开始移动|Moving|
|Moving|arrived|-|到达、开门|DoorOpen|
|DoorOpen|door_timeout|-|关门、待命|Idle|

**核心处理伪代码**：

```
state = Idle
for (event, ctx) in 事件流:
    candidates = table.get((state, event))
    for (guard, action, next_state) in candidates:
        if guard is None or guard(ctx):
            执行 action(ctx)
            state = next_state
            break
```

---

# 十一、示例10：断路器模式（Circuit Breaker）

**场景**：调用下游服务时，如果连续失败次数过多就熔断（直接拒绝后续请求，不再浪费资源等待超时）；一段冷却时间后放行一个试探请求，试探成功则恢复正常，试探失败则继续熔断。

**分析过程**：

1. **候选状态**："正常放行""熔断拒绝""半开试探"——三个候选。
2. **三原则检验**：互斥、穷尽（覆盖熔断器所有可能所处的阶段）、有限（3 个）——满足，确定为 `Closed / Open / HalfOpen`。这几乎是业界统一的命名，说明这套分析方法和真实工程实践是一致的。
3. **事件列表**：`failure`（请求失败）、`success`（请求成功）、`timeout`（冷却时间到）。
4. **动作列表**：记录失败、熔断打开、保持关闭、进入半开、恢复关闭、重新打开。
5. **特殊情况**：
   - **Guard**：`(Closed, failure)` 是否要跳到 `Open`，取决于累计失败次数是否达到阈值——与示例8、示例7是同一类判断。
   - **完整性思考**：`(Open, success)`、`(Open, failure)`、`(Closed, timeout)`、`(HalfOpen, timeout)` 这几个组合在规则表里是空白的，但**这不是遗漏，而是刻意设计**——熔断打开时请求根本不会被真正发出，所以不可能收到 success/failure 事件；正常关闭和半开状态也不需要"冷却计时"。这提前示范了 [state_ruletable.md 第十三节](state_ruletable.md) 要解决的问题：**一张规则表里"缺失"的格子，必须是团队主动确认"不可能发生"或"不需要处理"，而不是没想到。**

**规则表**：

|State|Event|Guard|Action|Next|
|------|------|------|------|------|
|Closed|failure|失败次数 >= 阈值|熔断打开|Open|
|Closed|failure|失败次数 < 阈值|记录失败|Closed|
|Closed|success|-|保持关闭|Closed|
|Open|timeout|-|进入半开、放行试探请求|HalfOpen|
|HalfOpen|success|-|恢复关闭|Closed|
|HalfOpen|failure|-|重新打开|Open|

**核心处理伪代码**：

```
state = Closed
for (event, ctx) in 事件流:
    candidates = table.get((state, event))
    if candidates is None:
        请求直接被拒绝（例如 Open 状态下，根本不发起调用）
        continue
    for (guard, action, next_state) in candidates:
        if guard is None or guard(ctx):
            执行 action(ctx)
            state = next_state
            break
```

---

# 十二、示例11：CI/CD 流水线

**场景**：流水线开始执行后可能通过、可能失败；失败时如果重试次数没用完就自动重试，用完了就判定失败；任何阶段都可以被人工取消。

**分析过程**：

1. **候选状态**："等待执行""正在执行""执行成功""执行失败""被取消"——五个候选。
2. **三原则检验**：互斥、穷尽（覆盖一次流水线运行的全部结局）、有限（5 个）——满足，确定为 `Pending / Running / Success / Failed / Cancelled`。其中 `Success / Failed / Cancelled` 都是"终止状态"（没有任何事件能再让它们离开）——终止状态在规则表里表现为"没有任何一行以它为 State"，这也是一种经过深思熟虑的、合法的"空白"，不需要专门为它们写规则。
3. **事件列表**：`start`、`pass`、`fail`、`cancel`。
4. **动作列表**：开始执行、判定成功、准备重试、判定失败、终止任务。
5. **特殊情况**：
   - **Guard**：`(Running, fail)` 走"重试"还是"判定失败"，取决于已重试次数是否达到上限。
   - **通配符**：`cancel` 在 `Pending`、`Running`（以及未来可能新增的任何中间状态）下语义完全一样——"不管现在在哪一步，直接终止"。这是通配符 `ANY` 最典型的应用场景：**事件语义与具体状态无关，并且希望以后新增状态时，不需要记得回来给这个事件补规则。**

**规则表**：

|State|Event|Guard|Action|Next|
|------|------|------|------|------|
|Pending|start|-|开始执行|Running|
|Running|pass|-|流水线成功|Success|
|Running|fail|重试次数 < 上限|准备重试|Running|
|Running|fail|重试次数 >= 上限|判定失败|Failed|
|ANY|cancel|-|终止任务|Cancelled|

**核心处理伪代码**：

```
state = Pending
for (event, ctx) in 事件流:
    candidates = table.get((state, event)) or table.get((ANY, event))
    if candidates is None:
        处理非法转换（例如 Success 状态之后不应该再收到任何事件）
        continue
    for (guard, action, next_state) in candidates:
        if guard is None or guard(ctx):
            执行 action(ctx)
            state = next_state
            break
```

---

# 十三、示例12：请假审批工作流

**场景**：员工提交请假申请；天数较短时经理可以直接审批通过；天数较长则需要转交 HR 审批；经理和 HR 都可以拒绝申请。

**分析过程**：

1. **候选状态**："草稿""待经理审批""待 HR 审批""已批准""已拒绝"——五个候选。
2. **三原则检验**：互斥、穷尽（覆盖一次审批流程从提交到终审的所有阶段）、有限（5 个）——满足，确定为 `Draft / PendingManager / PendingHR / Approved / Rejected`。这里能看到多角色工作流建模的一个常见手法：**状态名可以直接编码"当前球在谁手里"**（`PendingManager`、`PendingHR`）。
3. **事件列表**：`submit`、`approve`、`reject`。
4. **动作列表**：提交申请、经理直接批准、转交 HR、经理拒绝、HR 批准、HR 拒绝。
5. **特殊情况**：**Guard——`(PendingManager, approve)` 要不要直接进入 `Approved`，还是要转给 `PendingHR`，取决于"请假天数是否超过经理审批权限（3 天）"**。这是业务规则（审批权限阈值）直接翻译成 Guard 表达式的典型例子——业务文档里那句"3 天以内经理可直接批准"，翻译过来就是一行 Guard 代码。

**规则表**：

|State|Event|Guard|Action|Next|
|------|------|------|------|------|
|Draft|submit|-|提交申请|PendingManager|
|PendingManager|approve|天数 <= 3|经理直接批准|Approved|
|PendingManager|approve|天数 > 3|转交 HR|PendingHR|
|PendingManager|reject|-|经理拒绝|Rejected|
|PendingHR|approve|-|HR 批准|Approved|
|PendingHR|reject|-|HR 拒绝|Rejected|

**核心处理伪代码**：

```
state = Draft
for (event, ctx) in 审批操作流:
    candidates = table.get((state, event))
    for (guard, action, next_state) in candidates:
        if guard is None or guard(ctx):
            执行 action(ctx)
            state = next_state
            break
```

---

# 十四、示例13：表达式词法分析器（字符级状态机）

**场景**：把一个算术表达式字符串（如 `"12 + 34 * 5"`）逐字符扫描，切分成数字 Token 和运算符 Token。

**分析过程**：

1. **候选状态**："正在扫描一个数字的中途""不在数字中途（刚结束一个数字，或者还没开始）"——两个候选。
2. **三原则检验**：互斥（当前字符要么正处于某个数字内部，要么不是）、穷尽（覆盖了整个扫描过程的两种上下文）、有限（2 个）——满足，确定为 `Number / Start`。这也说明"字符级"状态机可以用极少的状态覆盖整个问题——**不需要为每一位数字单独建一个状态**，也不需要按数字长度区分。
3. **事件列表**：这里的"事件"是"当前字符的类型"：`digit`、`space`、`op`、`end`——需要先对字符做一次分类，分类结果才是真正的 Event。这一步叫**输入的归一化**，示例5的"行分类"是同一个道理：**原始输入往往不能直接当事件用，需要先经过一层分类函数。**
4. **动作列表**：开始记录数字、继续记录数字、产出 NUMBER Token 并清空缓冲、产出 NUMBER + OP 两个 Token。
5. **特殊情况**：`(Number, space)` 和 `(Number, op)` 都会触发"结束当前数字"，但后者还要多产出一个 OP Token——这是"同一大类结果（结束数字），根据事件细节追加不同副作用"的例子。这里没有用 Guard，而是直接写成了两个不同的 Action 函数——提醒一点：**Guard 不是分支的唯一手段，当"要不要执行"没有疑问、只是"具体做什么"不同时，直接用不同的 Action 更简单。**

**规则表**：

|State|CharType|Action|Next|
|------|------|------|------|
|Start|digit|开始记录数字|Number|
|Number|digit|继续记录数字|Number|
|Number|space|产出 NUMBER，清空缓冲|Start|
|Number|op|产出 NUMBER + OP，清空缓冲|Start|
|Start|space|忽略|Start|
|Start|op|产出 OP|Start|

**核心处理伪代码**：

```
state = Start
buf, tokens = [], []
for ch in 表达式字符串:
    event = char_type(ch)              # 归一化：字符 -> digit/space/op/end
    action, next_state = table[(state, event)]
    执行 action(ch, buf, tokens)
    state = next_state
```

---

# 十五、总结：如何把这套方法应用到新问题

以后遇到一个新场景，按下面的检查清单走一遍，就能自己推导出状态和规则表，而不用现找例子照抄：

```
[ ] 第1步：读一遍问题描述，圈出所有"名词化的阶段"，先不做取舍地全部列出来

[ ] 第2步：对每个候选做三个检验
    - 互斥：两个候选会不会同时为真？会 -> 它们是正交维度，需要重新想清楚真正的业务阶段
    - 穷尽：能不能想出一个不属于任何候选的情况？能 -> 缺状态，或者需要一个兜底状态
    - 有限：这是不是一个连续变化的数字？是 -> 它是数据，不是状态，留到 Guard 里用

[ ] 第3步：列出所有会让系统离开当前状态的事件
    - 如果原始输入不是干净的符号（一行文本/一个字符/一个数据包），先写一个分类函数做归一化

[ ] 第4步：列出每次迁移要执行的具体动作（只做事，不做判断）

[ ] 第5步：检查三个信号
    - 同一状态同一事件，结果要看情况 -> 加 Guard，不要加状态、不要写 if
    - 某个事件不管当前状态是什么，处理方式都一样 -> 用通配符 ANY，不要每个状态复制一条
    - 过一遍规则表里所有空白格子 -> 明确是"不可能发生"还是"故意不处理"还是"真的漏掉了"
```

13 个场景，从最简单的三色循环（交通信号灯）到带多重分支的工作流（请假审批），用的都是同一张检查清单。状态数量、规则复杂度会变，但分析的步骤永远是这五步——这正是"状态 + 规则表"能成为一种**普适**心智模式的原因：它提供的不是某个具体系统的答案，而是一套拿到任何新系统都能重复使用的分析流程。
