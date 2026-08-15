# Lesson 03: 容器生命周期 (Level 5 - Linux系统)

## Step 1: 是否适合FSM

先按标准五条检查,再单独厘清一个容易踩坑的点——"容器状态"到底说的是哪一层。

- **是否存在有限状态?**
  是,但要先确定"在哪一层看"。这一课聚焦**runtime层**(比如`runc`这种直接实现OCI Runtime Specification的低层运行时),OCI Runtime Spec的`state.json`里`status`字段规定只能是`creating`/`created`/`running`/`stopped`四个值之一。`runc`在这四个之上,自己的实现(libcontainer)又额外定义了一个`paused`状态(规范允许runtime自行扩展,但要求新增状态必须代表"规范未定义的新运行时状态",不能和已有四个冲突)。这一课的状态清单以`runc`/libcontainer的五个状态(`created`/`running`/`paused`/`stopped`,外加最初的`creating`过渡态)为准,数量有限、可枚举。

- **是否依赖历史状态?**
  是,而且是"强依赖"。`created`状态下不能直接`pause`(必须先`start`到`running`才能`pause`),`stopped`状态下也不能`resume`——这些限制在libcontainer源码里通过每个状态类自己的`transition()`方法显式拒绝非法跳转来实现,不是靠外部标志位拼凑判断的。

- **是否由事件驱动?**
  是。事件主要来自"运维操作"(`create`/`start`/`pause`/`resume`/`kill`/`delete`),以及"容器内部进程自己退出"这一事实。事件种类不多,但每一种都对应OCI规范里明确定义的一个runtime操作。

- **是否同一个事件在不同状态行为不同?**
  是,但主要体现为"合法/非法"的区分,而不是"走向不同的下一状态"。比如`start`操作:在`created`状态下合法,会转移到`running`;在`running`/`paused`/`stopped`状态下都是非法操作,规范要求"必须报错、且不能对容器产生任何效果"。这和前两课"同一事件走向不同下一状态"略有差异,这里更多是"同一事件在错误状态下被规范明确要求拒绝",这本身也是一种需要在转移表里体现的行为。

- **是否存在明确状态切换?**
  是。`runc list`能实时显示每个容器的当前status,切换点(创建完成、用户程序开始执行、进程退出)都是离散的系统调用/进程事件驱动的,没有中间模糊态。

**这一课的关键辨析:"你说的容器状态,到底是哪一层的状态?"**

这是这一课真正要训练的直觉,比五条特征本身更重要。至少有三层容易被混在一起说:

1. **OCI Runtime Spec层(最小公约数)**:只定义`creating`/`created`/`running`/`stopped`四个,这是"运行一个容器"这件事本身、脱离任何具体引擎的最基本描述。
2. **具体runtime实现层(如runc/libcontainer)**:在规范允许的范围内扩展,加入了`paused`(通过Linux cgroup freezer机制冻结容器内所有进程,不终止),这是"某一个具体runtime多做了什么"。
3. **上层容器引擎层(如Docker/containerd)**:在runtime之上又包了一层自己的状态机(`docker inspect`能看到`created`/`running`/`paused`/`restarting`/`removing`/`exited`/`dead`),这一层还要处理"重启策略""容器对象被删除后如何清理"这些runtime本身不关心的问题,这一层的状态机比runtime层更丰富,也是下一课Kubernetes Pod状态机的直接下游。

用户口语里常说的"容器有created/running/paused/stopped/removed这几个状态",其实是把第2层(paused来自这里)和第3层(removed这个概念,更准确说是"被删除、不再作为一个对象存在"来自这里)混在一起说的。这一课先把地基(第1、2层,runtime视角)讲清楚,"removed"作为一个特殊的讨论点单独处理(它更像是"FSM实例被销毁"而不是"进入了某个新状态"),第3层和更上层的编排视角(K8s Pod)留给第4课专门展开对比。

**适合FSM: ★★★★★**

理由:OCI Runtime Specification原文直接用"lifecycle"章节以编号步骤描述了这套状态转移,`runc`的`libcontainer/state_linux.go`更是直接用Go的接口`containerState`加`transition()`方法实现了一个教科书式的State Pattern——这是本教程里少数"连真实生产代码结构都和State Pattern几乎一一对应"的案例,非常适合用来建立"FSM在系统编程里长什么样"的具体印象。

## Step 2: 发现状态

**引导性问题:**
- OCI规范定义的四个状态和`runc`实际观察到的状态,为什么数量不一样?
- "容器被删除(delete)"之后,还有状态吗,还是这个FSM实例本身就不存在了?
- 容器里主进程的PID、退出码、创建时间,算状态吗?
- "runtime执行`create`操作的过程中"和"runtime执行完`create`、等待`start`"是不是应该算同一个状态?

**思考过程:**

先把候选项摆出来:

1. **`creating`** —— OCI规范里明确列出的第一个状态,对应"lifecycle"里的"步骤2:正在执行create操作"。这是一个短暂的过渡态,持续时间通常很短(准备namespace、mount、cgroup等),但确实是规范承认的独立阶段,合格。

2. **`created`** —— create操作完成、用户程序还没开始执行。这个状态存在的意义很关键:**它让"资源已经准备好"和"用户进程真正开始跑"这两件事被明确切开**,给了上层编排系统一个"确认容器已经就位、可以安全做进一步准备(比如网络配置、挂钩hook)再决定要不要真正start"的窗口,合格。

3. **`running`** —— 用户指定的程序已经在执行且未退出,合格。

4. **`paused`** —— libcontainer的扩展状态,通过cgroup freezer把容器内所有进程冻结(不是杀死,是暂停调度),常用于容器迁移/快照场景中"先冻住再拷贝内存"这类操作。要注意:**OCI规范原文的四个状态里没有`paused`**,它是runc在规范"允许runtime自行定义新状态,但必须代表未定义过的新运行时状态"这一条款下加的合法扩展,合格,但要标注"这是runtime层的扩展,不是最小公约数"。

5. **`stopped`** —— 容器进程已经退出(无论是正常退出、被kill、还是crash)。合格。

6. **"容器被删除(delete)"** —— 这是最容易引发争议的一项。`runc delete`执行后,这个容器对象在`runc list`里就彻底消失了,不再有任何status可查。它不满足"状态"的定义,因为状态要求"这个FSM实例还存在、只是处于某个稳定阶段",而delete之后FSM实例本身的生命周期结束了。更准确的描述是:**"delete"是一个让FSM整体终结的操作,产生的不是"进入removed状态",而是"这个状态机对象被销毁"**——这和第1课"failed状态被reset-failed清空"不同(那是回到inactive,FSM实例继续存在),这里是实例本身消亡。教学上,可以把它画成一个"终态之外的销毁箭头",而不是新增一个跟其他四个平级的状态节点。

7. **主进程PID(`state.json`里的`pid`字段)、创建时间、退出码** —— 这些是状态内携带的数据,不满足"状态"定义。比如`pid`只在`created`/`running`时必填,`stopped`状态下这个字段的意义变成"最后一次的pid"而不代表"当前有效进程",这恰好印证它是数据而不是状态本身。

8. **"容器被判定为不健康(unhealthy,如果配置了HEALTHCHECK)"** —— 这是上层引擎(Docker)才有的概念,OCI Runtime Spec本身完全不管"健康检查",这一课聚焦runtime层,不纳入,留给读者自己判断这属于第几层。

筛选标准不变:(a)对外行为稳定、(b)持续到明确事件才结束、(c)与其他候选互斥。"删除"不满足(a)(b)(它不是一种"稳定阶段",而是终结动作本身)。

**确定的状态清单(runtime层,以runc/libcontainer为参照):**

| 状态 | 说明 | 互斥性 | 持续性 |
|---|---|---|---|
| creating | create操作执行中(namespace/mount/cgroup准备) | 与其他状态互斥 | 持续到create操作完成或失败 |
| created | 环境已就位,用户程序尚未执行 | 同上 | 持续到收到start操作 |
| running | 用户指定程序正在执行 | 同上 | 持续到进程退出或被pause |
| paused | 容器进程被cgroup freezer冻结,可恢复 | 同上 | 持续到收到resume操作 |
| stopped | 容器进程已退出(不代表对象被删除) | 同上 | 持续到被delete(FSM实例终结,非"转移到新状态") |

五个状态两两互斥,`runc list`任意时刻每个容器只显示一个status。"delete"作为终结整个FSM实例的操作单独讨论,不计入这五个状态。

## Step 3: 寻找事件

**引导性问题:**
- `create`和`start`是不是可以合并成一个事件(反正很多人直接用`docker run`,感觉是"一步到位")?
- 容器内部主进程自己退出(比如脚本执行完毕)和外部用`kill`杀死容器,应该算同一种事件还是两种?
- OCI规范里提到的`prestart`/`createRuntime`/`createContainer`/`startContainer`各种hooks失败,应该怎么建模?
- runtime有没有类似"定期巡检容器状态"的循环?

**分析过程:**

- **`create`(runtime创建容器,对应`runc create`)** —— 真正的事件,触发"不存在 → creating → created"。之所以`create`和`start`必须是两个独立事件而不是一步到位,是因为OCI lifecycle明确把它们分成了两个可以分别调用的操作(`runc create`和`runc start`是两条独立命令),中间可以插入外部系统做准备工作(比如CNI插件在`created`状态下配置容器网络)——**这正是"created"这个状态存在的意义**,合并成一个事件会丢失这个可插入的窗口。`docker run`看起来"一步到位"只是上层封装帮你连续调用了这两步,不代表底层只有一步。
- **`start`(对应`runc start`)** —— 真正的事件,触发`created → running`,执行用户指定的程序(`process`字段)。
- **`pause`(对应`runc pause`)** —— 真正的事件,触发`running → paused`,底层通过写cgroup freezer的`freezer.state`为`FROZEN`实现。
- **`resume`(对应`runc resume`)** —— 真正的事件,触发`paused → running`,写freezer状态为`THAWED`。
- **"容器进程退出"** —— 这是本课需要重点厘清的一条:不管是进程自己正常退出、崩溃退出、还是被外部signal(比如`kill -9`)终止,从runtime状态机的角度看,**这些都归入同一种事件语义——"init进程终止"**,统一触发`running → stopped`,区别只在于`state.json`里记录的退出码/终止原因(数据层面的差异),不需要为"自己退出"和"被杀死"建模成两个不同的转移目标事件。这个归纳思路和第1课"三个定时器到时归纳成一个TIMER_EXPIRED"是同一种手法。
- **`delete`(对应`runc delete`)** —— 特殊事件,不指向"下一个状态",而是终结整台FSM。规范要求只有`stopped`状态才能被直接delete(`created`状态在某些实现里也允许强制delete,但`running`/`paused`状态必须先转成`stopped`才能删除,否则要报错)。
- **hooks失败(`prestart`/`createRuntime`/`createContainer`/`startContainer`等)** —— OCI lifecycle文档规定得很明确:任何一个hook失败,runtime必须报错、停止容器、并让容器直接走向销毁流程("continue the lifecycle at destroy步骤"),不是"停留在当前状态"也不是"转到stopped慢慢来"——这是一种"hook失败直接导致创建/启动流程整体失败并清理"的强约束事件,建模时可以统一抽象成`HOOK_FAILED`,触发"creating/created → (清理并销毁)"。
- **周期性巡检?** —— OCI runtime本身没有"定期检查容器状态"这种循环,`runc list`只是"查询"操作,不会主动改变任何容器的状态。这一点和第1课systemd(有Job调度但不算持续reconcile)、以及第4课要讨论的K8s(真正持续reconcile)形成一条清晰的复杂度递增线索,值得在这里先记一笔留给后面对比。

区分总结:
- **应归纳为同一事件的不同来源**:进程正常退出/崩溃/被kill,统一视为"init进程终止"事件,细节记录在数据里。
- **改变FSM实例生命周期而非"转移到某状态"的特殊事件**:`delete`。
- **强约束型失败事件**:hook失败,直接导向清理销毁,不停留在中间态。

**确定的事件清单:**

| 事件 | 说明 |
|---|---|
| CREATE | 请求创建容器(准备namespace/mount/cgroup) |
| CREATE_DONE / CREATE_FAILED | create操作完成/失败 |
| START | 请求执行用户指定程序 |
| PAUSE | 请求冻结容器进程 |
| RESUME | 请求解冻容器进程 |
| PROCESS_EXITED | init进程终止(涵盖正常退出/崩溃/被signal杀死) |
| HOOK_FAILED | 生命周期钩子执行失败 |
| DELETE | 请求销毁容器对象(终结FSM实例,非转移到新状态) |

## Step 4: 状态转移表

| Current State | Event | Next State | Action |
|---|---|---|---|
| (不存在) | CREATE | creating | 分配容器ID,准备namespace/mount/cgroup |
| creating | CREATE_DONE | created | 写入state.json,pid字段记录init进程pid(尚未exec用户程序) |
| creating | CREATE_FAILED / HOOK_FAILED | (销毁) | 清理已分配资源,不进入created |
| created | START | running | 执行startContainer hooks,exec用户指定程序 |
| created | HOOK_FAILED | (销毁) | 清理资源,不进入running |
| running | PAUSE | paused | 写cgroup freezer.state=FROZEN |
| paused | RESUME | running | 写cgroup freezer.state=THAWED |
| running | PROCESS_EXITED | stopped | 记录退出码/终止原因,回收部分资源(保留可查询的state.json) |
| stopped | DELETE | (销毁) | 清理cgroup/namespace残留,移除容器对象 |
| created | DELETE | (销毁) | (多数实现允许:尚未start,直接清理) |

这张表里"(销毁)"出现了三次而不是统一成一个状态,是故意的:它提醒我们**"终结"不是一个状态,而是转移表里可以合法出现的"目标是让FSM实例本身消失"这一类特殊结果**,不需要为了让表格"看起来完整"而硬造一个`removed`状态节点。

## Step 5: ASCII状态图

```
                CREATE
    (不存在) --------------> +----------+
                              | creating |
                              +----------+
                               |        |
                    CREATE_DONE|        | CREATE_FAILED / HOOK_FAILED
                               v        v
                         +---------+  (销毁,资源被清理)
                         | created |
                         +---------+
                          |       |
                     START|       | DELETE / HOOK_FAILED
                          v       v
                    +---------+ (销毁)
                    | running |
                    +---------+
                     ^      |
               RESUME|      | PAUSE
                     |      v
                  +--------+     PROCESS_EXITED
                  | paused | -----------------+  (paused状态下容器进程被冻结,
                  +--------+                  |   不会自己退出,此箭头仅示意
                                               |   若外部强制kill整个cgroup的
                                               v   极端路径,非常规路径)
                                          +---------+   DELETE
                                          | stopped | ----------> (销毁)
                                          +---------+
```

这张图的主干是一条"creating → created → running → stopped → (销毁)"的单向链,`paused`是`running`上的一个可逆旁路(`pause`/`resume`互相往返),而`(销毁)`不是图里的一个方框节点,是刻意画成"链的终点、不代表继续存在的状态"来强调Step 4提到的设计取舍。

## Step 6: Python实现

完整代码见同目录下的`03_container_lifecycle.py`。

**v1:简单版(Enum + match-case硬编码)**
用`Enum`定义`CREATING`/`CREATED`/`RUNNING`/`PAUSED`/`STOPPED`,加一个特殊的哨兵值表示"已销毁"(不能用`None`,因为`None`和"FSM还没初始化"容易混淆;用一个显式的`DESTROYED`状态放进同一个Enum里,虽然它在教学上"不是真正的状态",但代码实现上把它当成一个终态处理最简单直接——这是"教学模型的清晰度"和"实现的简洁性"之间一个常见的妥协)。

**v2:Transition Table版(数据驱动)**
这一版专门演示"如何忠实还原OCI规范里`start`操作在非法状态下必须报错"这条约束:字典里查不到`(state, event)`对应的转移时,`v1`只是"忽略并打印一条日志",但`v2`改成**抛出异常**(`InvalidTransitionError`)。这个改动看似很小,但呼应了Step1提到的"OCI规范要求非法操作必须报错,不能对容器产生任何效果"——**教学案例要不要把"非法输入"设计成"忽略"还是"报错",取决于这个真实系统的规范怎么要求,不是随便选一种就行**。第1课交通灯选择"忽略"(安全关键系统,维持现状更安全),这一课选择"报错"(操作型系统,调用者需要立刻知道自己的调用不合法)。

**v3:State Pattern版(对照真实的libcontainer实现)**
这一版故意贴近`runc`源码`state_linux.go`的实际结构:每个状态是一个类,类里有一个`transition(self, next_state)`方法,判断"我能不能变成这个新状态",不能就抛异常。这和前两课的State Pattern实现方式略有不同——前两课的状态类是"收到事件,决定下一个状态",这一课额外展示了libcontainer的真实设计:**状态类直接判断"目标状态是否被允许",而不是"某个事件是否被允许"**。这是两种同样合理的State Pattern实现角度(以事件为输入 vs 以目标状态为输入),读者可以对比感受在什么场景下哪种更贴近真实需求(runtime这种"调用者明确知道自己想让容器变成什么状态"的场景,后一种角度反而更直白)。

**v4:可复用FSM框架**
延续前两课的`GenericFSM`,但这一版增加"终结(terminate)"这个框架层面的新概念,和"转移到某个状态"区分开:当规则的`next_state`是特殊标记`None`时,框架把它理解为"这台FSM的生命周期到此结束",后续任何`dispatch()`调用都会被明确拒绝并提示"FSM已终结",而不是静默无视。这是这一课暴露给框架的新需求——前两课的`GenericFSM`没有"实例可以被销毁"这个概念,遇到这个新案例就应该扩展框架本身。

## Step 7: 复杂度分析

**为什么FSM比if-else更好?**
容器runtime是一个会被无数上层系统(Docker、containerd、Kubernetes的kubelet、CI流水线)反复调用的基础组件,任何一次"允许了不该允许的转移"(比如允许对一个`running`状态的容器直接执行`create`)都可能导致资源泄漏或者更严重的安全问题。用显式的状态类+转移检查(像`runc`真实做的那样),任何非法调用都会在早期被拒绝并有明确报错,而不是执行到一半才发现出了问题——这是"fail fast"原则在FSM设计里的直接体现,比起if-else里散落的"检查一下现在是不是running"式判断,更容易保证覆盖所有非法组合。

**这一课"多层状态机"的辨析什么时候是负担?**
如果你只是写一个简单的本地开发工具(比如给自己写个脚本管理几个测试容器),完全不需要区分"OCI层/runc层/Docker引擎层"这三层概念,直接调用`docker`命令行、只关心"容器现在是不是在跑"这一个布尔值即可。这三层区分的价值,只有在你真的需要**直接对接runtime层API(比如自己写一个符合OCI标准的容器管理工具,或者需要理解Kubernetes到底怎么通过CRI跟runtime打交道时)**才会体现出来。

**什么时候不该用FSM?**
如果一个"容器"在你的场景里只是"运行一次就完事的短命任务"(类似CI里跑一个测试脚本,跑完立刻销毁,不会被pause、不会被长期观察),那么完整建模五个状态+多层辨析是浪费——你只需要关心"启动成功了吗"和"退出码是多少"这两个数据点,这跟第1课"洗衣机暂停按钮"讨论里的判断标准一致:**如果一个系统里几乎不会用到某些中间状态(这里是paused),且没有人需要长期观察和响应这些中间状态,那么简化模型(甚至完全不建模FSM)可能更合适**。

## Step 8: FSM思维总结

```
==========
FSM识别口诀(容器runtime篇)
==========
✓ 先分清你说的"状态"到底是哪一层(规范最小定义 / 具体runtime扩展 / 上层引擎扩展)
✓ "对象被销毁/删除"不是一个新状态,是终结整台FSM实例的特殊转移结果
✓ 多种不同来源但语义等价的输入(进程退出/被杀/崩溃)可以归纳成同一个事件
✓ 非法输入该"忽略"还是"报错",要看这个系统本身的设计目标(安全关键 vs 操作型API)
✓ 只有真的需要理解/对接某一层的实现细节时,才需要把那一层的完整状态机建出来
```

## Step 9: 迁移训练题

1. 一个虚拟机的生命周期(比如通过libvirt管理的KVM虚拟机:`shut off`/`running`/`paused`/`in shutdown`/`crashed`/`pmsuspended`)和这一课的容器runtime状态机有很多相似之处,但虚拟机多了一个`pmsuspended`(通过ACPI挂起到内存)。这个状态该怎么归入本课"三层辨析"的框架里(它更接近哪一层的扩展)?
2. Linux的`cgroup v2`本身有一个"进程组"的概念,一个进程可以从一个cgroup迁移到另一个cgroup。如果要给"一个进程在cgroup树里的归属变化"建模FSM,状态该怎么定义(cgroup路径本身是状态,还是只是数据)?这和本课"容器状态"是同一类问题吗?
3. 快照/检查点技术(比如CRIU,Checkpoint/Restore In Userspace)可以把一个`running`的容器完整地序列化到磁盘,之后在另一台机器上`restore`回`running`。如果把"checkpoint"和"restore"也纳入这一课的状态机,应该新增什么状态、什么事件?这个新增会不会破坏"delete是终结而非转移"这条设计原则?

## 今天真正学会的一句话

当别人说"这个系统有created/running/paused/stopped/removed这几个状态"时,先别急着照单全收建模——问一句"这些状态是同一层定义的吗,还是把规范层、实现层、上层引擎层的概念混在一起说了",而"removed"这种描述"对象不再存在"的词,往往根本不是一个状态,而是终结FSM实例的特殊转移结果。
