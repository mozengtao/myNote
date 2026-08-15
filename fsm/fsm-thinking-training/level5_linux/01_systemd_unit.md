# Lesson 01: systemd Unit状态 (Level 5 - Linux系统)

## Step 1: 是否适合FSM

先别急着下结论,逐条检查"systemd管理一个Unit(比如一个.service)"这个案例的FSM特征。这一课比前面几个Level要复杂一点,因为systemd不是一个"纯粹"的事件驱动FSM——它内部还套了一层"任务调度"。我们先按标准五条检查,最后再单独讨论这个"调度"的特殊之处。

- **是否存在有限状态?**
  是。`systemctl status` 里能看到的 `Active:` 字段(ActiveState)只有几个固定取值:`inactive`、`activating`、`active`、`deactivating`、`failed`、`reloading`(还有一个很少见的 `maintenance`,用于mount/automount类单元的特殊故障场景,这一课先不展开)。数量有限、可枚举,第一条满足。

- **是否依赖历史状态?**
  是,但"浅依赖"——下一步能不能转移,主要取决于"现在是什么ActiveState",而不需要知道"三次启动之前发生了什么"。不过这里有个例外要留意:`Restart=on-failure` 这类策略会让systemd"记住"重启次数(`StartLimitBurst`/`StartLimitIntervalSec`),超过阈值会拒绝再次重启、直接判定为`failed`。这是一点"跨越单次转移的历史依赖",但没有严重到需要抛弃FSM模型,只是提示我们:这个状态机需要在状态之外携带一个"重启计数"这种辅助数据。

- **是否由事件驱动?**
  是,但事件的来源比交通灯复杂得多:有外部请求(`systemctl start/stop/restart/reload`,即"派发一个Job"),也有系统内部产生的事实(进程退出、进程被信号杀死、启动超时、依赖单元失败)。这些都是离散、可枚举的输入,依然满足"事件驱动"这一条。

- **是否同一个事件在不同状态行为不同?**
  是,而且非常典型。比如"进程退出(exit)"这个事实:在`activating`状态下如果是"启动脚本正常退出且类型是oneshot",会走向`active`;但如果退出码非0,则走向`failed`;而在`active`状态下如果主进程意外退出,走向`failed`或者(配置了自动重启的话)先经过`(auto-restart)`再回到`activating`。同一个"进程退出"事实,在不同状态下语义完全不同。

- **是否存在明确状态切换?**
  是。`inactive → activating → active` 这些切换点都是离散的、可以在日志里精确打点的(`journalctl -u <unit>` 能看到每一次ActiveState变化的时间戳),不存在"正在从active慢慢变成inactive"这种连续模糊态。

**特殊之处:"目标状态(want)vs当前状态(current)"**

前四条看起来和交通灯没什么本质区别,但systemd有一个别的教学案例都没强调过的机制:**Job队列**。当你执行`systemctl start foo.service`,systemd不是直接把ActiveState从inactive"拍"成active,而是先创建一个`Job`(类型是`JOB_START`),放进事务(transaction)队列,然后manager才根据依赖关系(`Requires=`、`After=`、`Wants=`等)决定执行顺序,一步步调用`unit_start()`让Unit的实际状态逼近"这个Job想要达成的状态"。如果中途依赖的单元失败,或者启动超时(`TimeoutStartSec`),这个"逼近"过程会中止,最终落在`failed`。

也就是说,systemd的模型更像是:**"current"是被观察到的ActiveState,而Job代表一次"让current朝want方向移动"的调度意图,manager不断尝试执行这个意图直到成功、失败或被取消**。这和"收到一个事件就直接查表切换到下一个状态"的简单FSM有微妙差异——Job不是一次性的转移触发器,而是一个"持续尝试达成目标"的过程(尽管systemd不会像Kubernetes那样无限期重试,详见第4课的对比)。这一课我们会先按"简单FSM"的方式建一版转移表(因为对使用者/运维视角,ActiveState确实是离散地一步步跳的),再在Step 7回过头讨论这个"调度式"差异什么时候重要。

**适合FSM: ★★★★★**

理由:systemd源码里`src/basic/unit-def.h`和`src/core/unit.c`确实就是用`UnitActiveState`枚举 + 一张转移合法性检查的方式实现的,这是一个真实存在、久经生产考验的FSM。它比交通灯复杂的地方在于引入了"Job/调度"这一层,但这恰恰是这一课的教学重点——让你看到"事件驱动FSM"这套思维模型在真实系统里会被怎样地扩展和包装,而不是被推翻。

## Step 2: 发现状态

**引导性问题:**
- `systemctl status`里能看到哪些"字段",这些字段是不是都是"状态"?
- 一个Unit在任意时刻,它的ActiveState一定是唯一确定的一个值吗?
- "这个服务配置了要开机自启"和"这个服务现在正在运行"是同一件事吗?
- 进程的PID、重启次数、上次退出码,这些算状态,还是算状态里携带的数据?

**思考过程:**

`systemctl status nginx.service` 输出里一次能看到好几个"看起来像状态"的字段,把它们都摆出来筛一遍:

1. **`Loaded:`(LoadState)** —— 这是"单元文件本身有没有被systemd成功加载"的状态,取值有`loaded`/`not-found`/`bad-setting`/`error`/`masked`等。这是一台**完全独立的状态机**,回答的问题是"这个.service文件存在吗、语法对吗、有没有被mask",和"服务现在有没有在跑"是两个不同维度。一个单元完全可以是`loaded`但`inactive`,也可以是`masked`(等价于被强制禁止启动,不管ActiveState是什么)。这一课聚焦的是第二条——ActiveState,LoadState作为"前提条件"提一下即可,不纳入本课状态清单。

2. **`Active:`(ActiveState)** —— 这才是本课的主角:`inactive`/`activating`/`active`/`deactivating`/`failed`/`reloading`。这是真正的"运行时状态",互斥、离散、有明确转移点,合格。

3. **`SubState`** —— 容易被误认为"更细的状态",比如同样是`active`,一个`Type=simple`的服务SubState是`running`,一个`Type=oneshot`执行完的服务SubState是`exited`。SubState其实是"针对不同Unit类型的实现细节补充说明",不同类型的Unit(service/socket/mount/timer)有各自不同的SubState取值集合,不是一套统一跨类型的状态机。这一课只建模跨所有Unit类型通用的ActiveState,SubState作为"状态内的补充细节"提及,不单独展开。

4. **`UnitFileState`(是否enabled)** —— 容易和ActiveState混淆,但这是"这个单元有没有被配置成开机自启"(`enabled`/`disabled`/`static`/`masked`),回答的是"下次开机会不会自动启动",跟"现在有没有在运行"完全是两件事——一个服务可以是`enabled`但`inactive`(还没到开机那一下,或者手动停掉了),也可以是`disabled`但`active`(手动启动过,只是没配置自启)。这是状态之外的"配置属性",不是运行时状态。

5. **主进程PID(MainPID)、上次退出码(ExecMainStatus)、重启次数** —— 这些是"状态内携带的数据",不是状态本身。同样处于`active`状态,PID每次启动都不同,这不影响"active"这个状态标签的语义,只是状态内的一个属性值。

筛选标准和第1课一样:看是否满足(a) 对外行为在该阶段内稳定、(b) 会持续到某个明确事件才结束、(c) 与其他候选状态互斥。LoadState和UnitFileState满足(a)(b)(c)但回答的是不同的问题维度(不是运行时FSM),SubState是ActiveState内部按类型细分的实现细节,PID/退出码/重启计数不满足(a)(它们本身不代表一种"稳定行为模式",只是数据)。

**确定的状态清单(ActiveState):**

| 状态 | 说明 | 互斥性 | 持续性 |
|---|---|---|---|
| inactive | 未运行,处于初始/停止后的静止态 | 与其他状态互斥 | 持续到收到start类Job或依赖被拉起 |
| activating | 正在执行启动流程(ExecStartPre/ExecStart等) | 同上 | 持续到启动流程完成或失败/超时 |
| active | 正常运行中 | 同上 | 持续到收到stop/restart类Job,或主进程意外退出 |
| deactivating | 正在执行停止流程(ExecStop等) | 同上 | 持续到停止流程完成或超时被强杀 |
| failed | 上一次启动或运行以失败告终 | 同上 | 持续到被重置(`systemctl reset-failed`)或重新收到start Job |
| reloading | 正在处理reload请求(通常对应SIGHUP) | 同上 | 持续到reload完成,之后回到active |

六个状态两两互斥(`systemctl status`任意时刻只显示一个ActiveState),每个都有清晰的"进入条件"和"退出条件",符合状态的定义。

## Step 3: 寻找事件

**引导性问题:**
- 用户在终端敲的`systemctl start/stop/restart/reload`,这些命令本身是事件,还是"发出了一个调度请求"?
- 进程退出这件事,是一个事件,还是要看"退出码"才能决定算哪种事件?
- "依赖的另一个单元失败了",这算不算一种事件?
- Unit配置里的`ConditionPathExists=`之类的前置条件检查失败,应该建模成"转移到failed",还是"这次Job直接被跳过,状态不变"?

**分析过程:**

- **`systemctl start`(或DBus等价调用)** —— 真正的事件,对应"派发一个`JOB_START`"。触发`inactive`/`failed → activating`。
- **`systemctl stop`** —— 真正的事件,派发`JOB_STOP`,触发`active → deactivating`。
- **`systemctl restart`** —— 严格来说不是一个原子事件,而是"先stop、等到位、再start"的组合(`JOB_RESTART`背后其实按顺序处理)。但对状态机来说,可以先把它看成"两个事件的组合"来理解,不需要单独建模成第三种转移箭头。
- **`systemctl reload`(SIGHUP语义)** —— 真正的事件,派发`JOB_RELOAD`,触发`active → reloading`,完成后自动回到`active`。注意:大多数Unit只有在`active`状态下才接受reload,如果单元当前是`inactive`,reload请求会直接失败并报错,而不是产生任何状态转移——这是"应被拒绝的非法输入",不是一次成功的转移。
- **"启动流程执行完毕且成功"** —— 这不是外部输入,而是systemd内部对`ExecStart`进程行为的判定结果(比如`Type=notify`的服务调用了`sd_notify(READY=1)`,或者`Type=oneshot`的进程正常退出码为0)。这是真正驱动`activating → active`的事件,但它的"事件源"是进程状态变化,不是用户命令。
- **"启动流程失败或超时(`TimeoutStartSec`到)"** —— 真正的事件,触发`activating → failed`。
- **"进程意外退出(在active状态下)"** —— 真正的事件,但下一步走向要看`Restart=`策略:如果是`Restart=no`(默认),直接`active → failed`;如果是`Restart=on-failure`/`always`,则会先经过`deactivating`收尾清理,再重新派发一次隐含的start Job回到`activating`(前提是没有触发`StartLimitBurst`重启风暴保护,否则最终落在`failed`并不再自动重试)。这正是Step1提到的"内部产生的自动重试事件",要在事件清单里单独标注它是"系统自己生成的事件",而不是外部输入。
- **"依赖单元失败(比如`Requires=`的另一个服务挂了)"** —— 真正的事件,通过systemd的依赖传播机制(`OnFailure=`、`PropagatesReloadTo=`等)间接触发本单元的stop或直接标记failed,建模时可以抽象成"收到一个`DependencyFailed`事件"。
- **`ConditionPathExists=`等前置条件检查失败** —— 这是"动作被跳过"而不是"转移到failed"。systemd文档里对这类Condition的定义是:如果条件不满足,Job会被跳过(skipped),Unit保持原ActiveState不变,日志会记录"Condition check failed",不产生失败状态。这是一个容易被教学案例忽略、但生产环境经常踩到的细节——"跳过"和"失败"是两种不同的语义,不能混为一谈。
- **"用户手动执行`systemctl reset-failed`"** —— 真正的事件,把`failed`清空为`inactive`,不做任何实际的启停操作,纯粹是"清除历史失败标记"。

区分总结:
- **应被视为"调度请求"而非直接转移的输入**:`systemctl start/stop/restart`——它们产生Job,Job的执行结果才是真正驱动转移的东西。
- **真正触发转移的事件**:启动成功/失败/超时、进程意外退出、reload请求被接受、reset-failed。
- **应忽略或不产生转移的输入**:在非法状态下的reload请求(报错但不转移)、Condition检查失败(跳过但不转移)。

**确定的事件清单:**

| 事件 | 说明 |
|---|---|
| START_REQUESTED | 收到start类Job(用户/依赖单元触发) |
| STOP_REQUESTED | 收到stop类Job |
| RELOAD_REQUESTED | 收到reload类Job(仅active状态接受) |
| START_SUCCEEDED | 启动流程判定成功 |
| START_FAILED | 启动流程失败或超时 |
| STOP_COMPLETED | 停止流程执行完毕 |
| RELOAD_COMPLETED | reload流程执行完毕 |
| PROCESS_CRASHED | 运行中主进程意外退出 |
| AUTO_RESTART | 系统根据`Restart=`策略自动生成的重试事件(区别于用户请求) |
| RESET_FAILED | 用户清除failed标记 |

## Step 4: 状态转移表

| Current State | Event | Next State | Action |
|---|---|---|---|
| inactive | START_REQUESTED | activating | 执行ExecStartPre/ExecStart,启动计时器TimeoutStartSec |
| activating | START_SUCCEEDED | active | 记录MainPID,清除重启计数(视配置) |
| activating | START_FAILED | failed | 记录失败原因,若配置Restart=on-failure则触发AUTO_RESTART |
| active | STOP_REQUESTED | deactivating | 执行ExecStop,启动计时器TimeoutStopSec |
| active | RELOAD_REQUESTED | reloading | 向主进程发送SIGHUP或调用reload命令 |
| active | PROCESS_CRASHED | deactivating / failed | 若配置Restart策略,先清理再触发AUTO_RESTART;否则直接判定failed |
| reloading | RELOAD_COMPLETED | active | 清除reloading标记,恢复正常 |
| deactivating | STOP_COMPLETED | inactive | 清理MainPID,进入静止态 |
| deactivating | (超时未退出) | failed | 强制SIGKILL后仍判定为异常收尾 |
| failed | START_REQUESTED | activating | 重新走启动流程(如未超过StartLimitBurst) |
| failed | AUTO_RESTART | activating | 系统自动重试(受StartLimitBurst约束) |
| failed | RESET_FAILED | inactive | 仅清除失败标记,不执行任何启停动作 |
| (任意状态) | Condition检查失败 | 保持原状态 | 跳过本次Job,记录日志,不算失败 |

值得强调的一条设计取舍:`PROCESS_CRASHED`在"active"状态下的下一状态不是唯一确定的,而是**取决于Restart=配置这个"状态外的策略参数"**。这提示我们:纯粹的状态转移表有时不足以完整描述行为,还需要一个"策略配置"作为转移函数的额外输入——这在真实工程FSM里很常见,不代表模型错了,而是提醒我们转移表要配合文字说明才完整。

## Step 5: ASCII状态图

```
                         START_REQUESTED
        +------------------------------------------+
        |                                            v
   +---------+                                 +------------+
   | inactive|<------------STOP_COMPLETED------ |deactivating|
   +---------+                                 +------------+
        |                                            ^
        | START_REQUESTED                            | STOP_REQUESTED /
        v                                            | PROCESS_CRASHED(有Restart策略)
   +----------+   START_SUCCEEDED   +--------+--------+
   |activating| ------------------> | active |
   +----------+                     +--------+
        ^   |                        |    ^
        |   | START_FAILED           |    | RELOAD_COMPLETED
        |   v                        |    |
        |  +--------+   RELOAD_REQUESTED  |
        |  | failed |                +---------+
        |  +--------+ <--------------|reloading|
        |     |   ^   PROCESS_CRASHED +---------+
        |     |   |   (无Restart策略)
        |     |   +---------------------------+
        |     |  AUTO_RESTART (受StartLimitBurst约束)
        +-----+
        RESET_FAILED (-> inactive, 图中未画出以保持简洁)
```

这张图比第1课的单向环复杂,因为systemd确实存在"回头路"(`failed → activating`通过重启或手动start)、"临时分支"(`active → reloading → active`是一个不改变主状态的短暂旁路)。这种"主环+旁路+失败回环"的结构,在生产系统FSM里比"纯单向环"更常见,值得留意。

## Step 6: Python实现

完整代码见同目录下的`01_systemd_unit.py`。

**v1:简单版(Enum + match-case硬编码)**
用`Enum`定义六个ActiveState,用一个`SystemdUnit`类里的`match`语句直接写"如果当前是inactive,收到START_REQUESTED就切到activating"。这一版故意先忽略"Restart策略"这个复杂度,只做最基础的单一路径转移,让读者先建立"这确实是个FSM"的直觉,再在v2里补上策略参数。

**v2:Transition Table版(数据驱动)**
把转移规则抽成`dict[(State, Event), State]`,但这一版遇到了第1课没有的问题:`(active, PROCESS_CRASHED)`的下一状态不是唯一的,取决于`restart_policy`。解决方式是把"动作函数"里包含一段小判断逻辑(读取`restart_policy`字段决定要不要经过AUTO_RESTART),而转移表本身只负责"给定(state, event)->下一步大方向"。这体现了一个重要取舍:**转移表数据驱动不代表"零逻辑",遇到需要外部配置参与决策的转移,合理的做法是把"决策"留在动作函数里,而不是试图把所有配置组合都拆成独立的事件类型**(否则事件数量会因为配置组合而爆炸)。

**v3:State Pattern版(每个状态是一个类)**
六个状态各自成类,每个类知道"我在这个状态下,收到什么事件该怎么办"。这一版特别适合展示`failed`状态的特殊性——`FailedState`类里可以清晰地封装"重启计数检查、StartLimitBurst判断"这类只跟这个状态相关的复杂逻辑,不会污染其他状态类的代码。这是State Pattern相比转移表字典的优势场景:某个状态的处理逻辑明显比其他状态复杂时,单独成类能让复杂度局部化。

**v4:可复用FSM框架**
沿用第1课抽象出的`State`/`Event`/`Transition`/`FSM`通用构件,额外加一个"guard"(转移前置条件判断函数)的概念:一条转移除了`(state, event) -> next_state`,还可以带一个"这个转移是否被允许发生"的判断函数,用来表达"reload只在active状态被接受""Condition检查失败则跳过"这类"输入合法但当前不满足条件"的场景,和"输入本身未定义"区分开。这是从第1课v4基础框架上的一次真实扩展——遇到新案例暴露出旧框架的不足,就应该扩展框架,而不是在案例代码里各自打补丁。

## Step 7: 复杂度分析

**为什么FSM比if-else更好?**
systemd要管理的不是一个Unit,而是一台机器上可能几百个Unit,每个Unit有自己的依赖关系、重启策略、超时配置。如果用if-else,每个Unit类型(service/socket/mount/timer/device等)、每种Restart策略组合都要写一遍完整的判断逻辑,状态泄露和遗漏转移的风险极高(比如忘记处理"reload请求但当前是inactive"这种边界)。真实systemd源码(`src/core/unit.c`)确实是用状态枚举+严格的转移合法性检查函数实现的,任何试图执行"不合法转移"的代码路径都会被断言拦住——这正是FSM思维在C语言工程里的典型落地方式。

**这一课比前几课多出来的复杂度值得吗?**
"Job队列/调度"这层复杂度不是systemd过度设计,而是真实存在的工程需求:一个服务启动往往依赖好几个其他服务先就位(`After=`、`Requires=`),如果没有Job队列做统一调度和依赖排序,每个Unit自己判断"我现在能不能启动"会导致大量重复逻辑和竞态问题。这提示一个更普遍的原则:**当多个FSM实例之间存在相互依赖时,单靠每个实例自己的转移表是不够的,通常需要一个更上层的调度器来协调"谁先谁后"——这个调度器本身也可以用FSM或者队列模型来实现,但它和单个实例的FSM是两个不同层次的问题**,不要混在一张转移表里。

**什么时候不该用这么复杂的模型?**
如果你只是写一个简单的后台脚本管理工具,管理的进程之间没有依赖关系、不需要开机自启、失败了也不需要自动重启策略,那么"activating/active/failed"三个状态 + 一个转移表可能就足够,不需要重新发明"Job队列"和"依赖事务"——那是分布式init系统才需要解决的问题规模。判断标准依然是:**你的系统里有没有真实存在"多个FSM实例互相依赖、需要统一调度先后顺序"这个问题**,没有就不需要引入调度层。

## Step 8: FSM思维总结

```
==========
FSM识别口诀(systemd Unit篇)
==========
✓ ActiveState数量有限且互斥(inactive/activating/active/deactivating/failed/reloading)
✓ 同一事件(如进程退出)在不同状态、不同配置(Restart=)下走向完全不同
✓ 存在"回头路"(failed可以重新变成activating),不是单向环
✓ 当转移结果依赖"状态外的配置参数"时,把决策放进动作函数,而不是让事件类型爆炸
✓ 多个FSM实例互相依赖时,考虑引入独立的调度/协调层,而不是塞进单个转移表
```

## Step 9: 迁移训练题

1. Linux的`udev`在处理设备热插拔时,一个设备节点会经历"探测中/已就位/正在被规则处理/已移除"这类阶段——这和systemd Unit的模型有什么相似之处?udev有没有类似"Job队列"的调度层?
2. `cron`/`systemd-timer`触发的定时任务,每一次触发算是"新建一个FSM实例",还是"同一个Unit反复经历activating→active→inactive这个循环"?这个判断会不会影响你怎么设计代码?
3. 一个反向代理(比如nginx/haproxy)的"配置热加载(reload)"过程,通常是"起一个新worker、等它就位、再优雅关闭旧worker",这比systemd的reload(往往只是发个SIGHUP)复杂。这算不算需要单独建模的一个"reload子状态机",而不是像本课这样简单标一个`reloading`状态就完事?

## 今天真正学会的一句话

当一个系统里"下一步该做什么"不仅取决于当前状态和收到的事件,还取决于一份"外部策略配置"(比如Restart=)时,不要试图把这份配置也拆成无穷多种事件类型——把状态机负责的"我该到哪个状态"和策略负责的"要不要走这条路"分开处理,状态机才不会因为配置组合的膨胀而失控。
