# Lesson 04: Kubernetes Pod状态 (Level 5 - Linux系统)

## Step 1: 是否适合FSM

前三课的"是否适合FSM"最终都给出了肯定回答,这一课要认真对待一个不一样的可能性:**传统的"事件驱动FSM"思维,套在Kubernetes Pod上会出现明显的裂缝,需要修正之后才适用**。逐条检查,但这次每一条都要多问一句"这条特征在这里成立到什么程度"。

- **是否存在有限状态?**
  是。Pod的`status.phase`只有五个取值:`Pending`/`Running`/`Succeeded`/`Failed`/`Unknown`,加上更细粒度的容器状态`Waiting`/`Running`/`Terminated`。数量有限,这条完全成立。

- **是否依赖历史状态?**
  是,但这里已经能看出第一处裂缝:Pod的历史不只体现在"上一个phase是什么",还体现在一份持续存在的**`spec`(期望状态)**上——Pod对象从创建到删除,`spec`(要跑哪些容器、用什么镜像)基本不变,变化的是`status`在不断尝试"追上"`spec`所描述的期望。这种"存在一份独立于当前观测值、持续起作用的期望状态"的设定,在前三课(systemd的Job、NetworkManager的ActiveConnection、容器runtime的操作调用)里都没有以这种"常驻声明"的形式出现过。

- **是否由事件驱动?**
  这里裂缝更明显了。传统FSM期待"收到一个事件,查表,执行一次转移"。但Kubernetes的核心组件——**kubelet**——不是被动等待事件的:它在自己的节点上以一个**周期性循环(sync loop)**的方式,反复对比"这个Pod的spec要求跑成什么样"和"容器runtime里现在实际跑成什么样",每一轮循环都会重新计算"现在该做什么"。真正的外部事件(比如"用户改了spec"、"容器进程退出了")确实存在,也确实会触发一次同步,但**即使什么外部事件都没发生,kubelet依然会定期主动重新核对一遍**——这是前三课都没有的行为模式:一个"没有收到任何新事件,却依然会重新评估状态"的循环。

- **是否同一个事件在不同状态行为不同?**
  是,这条依然成立,不受影响——比如"容器进程退出"这件事,在`restartPolicy: Always`下会导致重启(phase保持`Running`),在`restartPolicy: Never`下如果退出码非0会导致phase变成`Failed`。

- **是否存在明确状态切换?**
  从`status.phase`的取值本身看,是的(切换点离散、可枚举)。但从"这个切换是怎么被触发的"看,不完全是——很多时候不是"某个事件直接触发了这次切换",而是"某一轮reconcile循环观察到现状和期望的差距,算出了一个新的status该写成什么样"。

**修正点是什么?**

传统FSM思维的核心假设是:**"下一个状态 = f(当前状态, 收到的事件)"**,转移是"事件驱动、一次性执行"的。Kubernetes Pod(以及绝大多数Kubernetes资源)更接近:**"下一个要执行的动作 = reconcile(期望状态spec, 当前观测状态status)"**,而`status`本身则是"当前观测到的事实的记录",不是"由某次转移直接产生的、驱动下一步判断的核心依据"。

用更具体的话说,修正需要做两件事:

1. **把"status"从"驱动下一步判断的状态"降级为"观测结果的记录"**。传统FSM里,"current state"既是"系统现在处于什么阶段",也是"下一步转移判断要用的输入"。Kubernetes里,`status.phase`更多是"给外部观察者看的摘要",kubelet做决策时更依赖"直接检查容器runtime的真实情况",而不是完全信任上一次写入的`status`字段(因为`status`可能滞后、可能在中途被别的组件改过)。
2. **引入"控制循环(reconcile)"作为和"事件"平级、甚至更基础的驱动力**。事件(比如`spec`被用户更新)只是"提前触发一次reconcile,不用等到下一个周期",本质上还是回到"再算一次reconcile"这条统一路径,而不是像传统FSM那样直接查一张"(状态,事件)->下一状态"的表。

做完这两个修正后,"FSM思维"依然有用,但用法变了:**"phase的五个取值"依然是一个有效的、值得枚举的有限状态集合,但描述"如何从一个phase变成另一个phase"这件事,不应该继续用"事件驱动转移表"去套,而应该用"reconcile函数在当前观测和期望状态之间的差距下会计算出什么动作"去描述**。这一课会把两种视角都做出来对比(Step 6的v1~v3还是传统FSM式的教学演进,v4会加入reconcile函数作为修正后的版本),而不是简单地说"这里不适合FSM"就回避掉复杂性。

**适合FSM: ★★★☆☆(传统事件驱动FSM需要修正为"状态枚举+reconcile循环"的混合模式才适用)**

理由:五个phase值本身完全满足"有限、互斥、可枚举"的门槛,单纯当作一个"有限状态集合"去理解和查文档是没问题的、也是必要的。但如果照搬前三课"事件->查表->转移"的思路去写实现代码,会发现处处对不上号——kubelet真正的实现逻辑(`SyncPod`)是一个反复运行的对比-决策循环,不是一个等事件的状态机。三星星级不是否定FSM思维的价值,而是标记"这里的FSM思维需要和控制循环模式结合着用,单用一种都不够"。

## Step 2: 发现状态

**引导性问题:**
- `kubectl get pod`的`STATUS`列显示的值(比如`Running`、`CrashLoopBackOff`、`ImagePullBackOff`)都是`phase`吗?
- Pod有`status.phase`,里面每个容器还有自己的状态,这是两层独立的状态机,还是一层是另一层的汇总?
- Pod的`spec`算状态吗?
- `Conditions`(比如`PodScheduled`、`Ready`、`Initialized`)是不是也应该被当成状态?

**思考过程:**

先把候选项摆出来:

1. **`status.phase`** —— 官方文档明确定义为"Pod生命周期的一个高层次总结",取值`Pending`/`Running`/`Succeeded`/`Failed`/`Unknown`,互斥,合格,是这一课的主状态。

2. **每个容器的`status.containerStatuses[].state`(`Waiting`/`Running`/`Terminated`)** —— 这是**另一层独立的状态**,不是phase的子集,而是"phase是怎么被计算出来的"的输入材料之一。一个Pod可以处于`Running`phase,而其中某一个容器处于`Waiting`(比如`CrashLoopBackOff`,正在等待下一次重启的backoff计时结束)——只要Pod里至少有一个容器在跑或正在启动/重启,phase就还是`Running`,不会因为某一个容器暂时不在跑就掉到别的phase。这提示我们:**这是两层状态机(容器级、Pod级),Pod级的phase是容器级状态的一种"汇总判断结果"，不是简单的"一一对应"**。

3. **`kubectl get pod`里显示的`CrashLoopBackOff`/`ImagePullBackOff`/`ContainerCreating`** —— 容易被误认为是独立的phase值,但官方文档说得很清楚:**这些是容器状态`Waiting`附带的`reason`字段的取值,不是phase,也不是一个独立的状态维度**,是"Waiting"这一个状态内部"为什么在等"的详情数据。

4. **`spec`** —— 不是状态,是**期望**。这是Step1修正点里提到的核心概念,值得单独强调:`spec`一旦创建基本不变(除非用户主动`kubectl apply`更新),它扮演的角色更接近"目标",不是"随时间经历各个阶段的东西"。把`spec`塞进状态清单是最容易犯的错误——它没有"互斥性"(不存在"spec A和spec B谁生效"的问题,每个Pod只有一份spec),也不满足"持续到某事件才改变"(它是外部输入,不是系统内部经历的阶段)。

5. **`Conditions`(`PodScheduled`/`Initialized`/`ContainersReady`/`Ready`)** —— 这是本课最值得展开辨析的一项。它们看起来很像"状态"(每个都有`True`/`False`/`Unknown`三种取值,且和phase的变化明显相关),但它们的本质是**"关于当前status的一组独立布尔判断结果,可以同时成立、不互斥"**——比如`PodScheduled=True`和`Initialized=True`可以同时为真,这和"状态互斥"的定义直接冲突。更准确的理解是:**Conditions是"细粒度的观测事实清单",phase是"综合这些事实之后给出的一个高层摘要",两者是不同粒度、不同性质(一组独立布尔量 vs 一个枚举值)的东西,不应该混在一张"状态清单"里**。

**确定的状态清单(Pod级,status.phase):**

| 状态 | 说明 | 互斥性 | 持续性 |
|---|---|---|---|
| Pending | 已被集群接受,但还有容器未就位(调度中/拉镜像中/初始化中) | 与其他phase互斥 | 持续到至少一个主容器成功启动,或彻底失败 |
| Running | 已绑定到节点,至少有一个容器在运行/启动/重启中 | 同上 | 持续到所有容器终止(成功或失败) |
| Succeeded | 所有容器都以退出码0终止,且不会再被重启 | 同上 | 终态 |
| Failed | 所有容器都已终止,且至少一个以失败告终 | 同上 | 终态 |
| Unknown | 无法获取Pod状态(通常是节点失联) | 同上 | 持续到与节点的通信恢复或Pod被判定驱逐 |

**确定的容器级状态清单(status.containerStatuses[].state,附属于Pod级状态,不是同一层):**

| 状态 | 说明 |
|---|---|
| Waiting | 尚未进入Running(拉镜像、等待重启backoff等),reason字段说明具体原因 |
| Running | 容器进程正在执行 |
| Terminated | 容器进程已退出,reason/exitCode记录详情 |

这两层状态是"Pod phase由容器状态的组合规则推导出来"的关系,而不是"平级独立的两套状态机各管各的"——这本身就是"层次化但又相互依赖"的又一个例子(和第2课NetworkManager的Device/ActiveConnection分层不同,那里是"协作的两台独立FSM",这里是"下层状态的组合直接定义了上层状态的取值",耦合程度更紧)。

## Step 3: 寻找事件

**引导性问题:**
- "kubelet的sync loop每隔几秒醒来看一眼容器状态",这算不算一个"事件"?
- "用户执行`kubectl apply`改了Pod的镜像版本"是事件吗?这种改动实际上在Pod上会怎么生效?
- "容器探针(liveness/readiness probe)检测失败"是事件吗,它直接改变phase,还是先改变别的东西?
- "节点失联(kubelet心跳超时)"这件事,是Pod自己的FSM里的一个事件,还是完全是外部强加给它的判断?

**分析过程:**

- **"kubelet的周期性sync loop"** —— 这是Step1提到的核心裂缝,这里正式给它一个名字:**它不是传统意义上的"外部事件",但在"驱动系统重新评估状态"这个作用上,扮演了和事件类似的角色**。可以把它抽象成一个特殊的伪事件`RECONCILE_TICK`,区别在于:普通事件通常携带具体信息("进程退出,退出码是137"),而`RECONCILE_TICK`不携带任何新信息,只是"提醒系统:不管有没有新消息,都再检查一次现状和期望的差距"。这个区分很重要:**普通事件回答"发生了什么新情况",RECONCILE_TICK只回答"到时间了,再看一眼"**。
- **"用户更新spec(比如`kubectl apply`改镜像)"** —— 这里有个容易被误解的细节:**Pod对象的绝大多数`spec`字段实际上是不可变的**(比如`spec.containers[].image`在Kubernetes较早版本里创建后不能直接改;更常见的做法是通过Deployment/ReplicaSet控制器,创建一个新Pod、删除旧Pod,而不是"就地修改这一个Pod object的spec")。所以"更新spec直接让同一个Pod对象走一遍新的reconcile"这种直觉,在Pod这一层大多数场景下并不准确——真正频繁经历"reconcile"的是**Deployment/ReplicaSet这些更上层的控制器对象**,它们的spec变化确实会持续触发协调,Pod本身更多是"被创建出来后走完自己有限的生命周期,而不是反复被reconcile成不同的东西"。这提示一个重要区分:**"声明式期望+reconcile"这套模式,在Kubernetes里更适合用来描述Deployment/ReplicaSet/StatefulSet这些控制器管理的"期望副本数、期望版本"层面,Pod自身的phase转移相对更接近一次性的、单向的生命周期推进**。这一课选Pod作为案例,重点不是说"Pod也频繁被reconcile成不同东西",而是说"哪怕Pod的phase转移相对单向,支撑它运作的机制底层依然是kubelet的持续sync loop,不是事件回调"。
- **容器探针检测失败(liveness/readiness probe failed)** —— 真正的事件,但它不直接改变phase,而是先改变一个更细粒度的东西:`Ready` Condition(readiness probe失败)或者触发kubelet杀死并重启容器(liveness probe失败)。只有当"容器被重启的次数和策略最终导致所有容器都终止且不会再被重启"时,才会间接影响到phase。这印证了Step2的分层结论:**探针结果直接影响的是容器级状态和Conditions,Pod phase是这些更细粒度事实的滞后汇总,不是每个细粒度事件都会直接、立刻改写phase**。
- **"容器进程退出"** —— 真正的事件,导致该容器的状态从`Running`变成`Terminated`,具体退出码/reason记录下来,然后kubelet根据`restartPolicy`决定要不要重启这个容器(重启的话该容器状态会经过`Waiting`回到`Running`)。
- **"节点失联(kubelet与API Server失去心跳)"** —— 真正的事件,但它不是"这个Pod自己经历的事情",而是**外部对这个Pod可观测性的判断**:控制平面（比如node controller）无法再确认这个Pod的真实状态,于是把phase标记为`Unknown`。这是一个很好的例子说明**"状态机自己也可能因为外部原因变得不可观测,`Unknown`状态存在的意义就是老实承认这一点,而不是硬猜一个`Running`或`Failed`**。
- **"Pod被调度到某个节点(scheduling完成)"** —— 真正的事件,触发`Pending`阶段内部一个关键的Condition(`PodScheduled=True`)达成,但phase本身依然停留在`Pending`,直到容器真正开始创建/运行。

区分总结:
- **携带具体新信息的真正事件**:容器进程退出、探针失败、调度完成。
- **不携带新信息但驱动重新评估的特殊"伪事件"**:kubelet的周期性sync(`RECONCILE_TICK`)。
- **描述"系统对自身失去观测能力"的特殊事件**:节点失联,导向`Unknown`而不是硬猜其他phase。

**确定的事件清单:**

| 事件 | 说明 |
|---|---|
| SCHEDULED | Pod被调度到某个节点 |
| CONTAINER_CREATING_DONE | 容器镜像拉取/创建完成,即将启动 |
| CONTAINER_STARTED | 容器进程开始运行 |
| CONTAINER_EXITED_ZERO | 容器进程以退出码0终止 |
| CONTAINER_EXITED_NONZERO | 容器进程以非零退出码终止 |
| PROBE_FAILED | 探针检测失败(细化影响Condition/容器重启,不直接决定phase) |
| NODE_UNREACHABLE | 节点失联,无法确认Pod真实状态 |
| RECONCILE_TICK | kubelet周期性sync循环(不携带新信息的"伪事件") |

## Step 4: 状态转移表

传统"Current State | Event | Next State | Action"的表格在这一课需要加一列说明:**这条转移是"某个具体事件直接触发的",还是"reconcile循环基于当前汇总观测算出来的"**,否则会掩盖Step1讨论过的核心裂缝。

| Current State | Event | Next State | Action | 驱动方式 |
|---|---|---|---|---|
| Pending | SCHEDULED | Pending | 记录PodScheduled=True Condition,不改变phase | 事件直接触发Condition更新 |
| Pending | CONTAINER_STARTED (至少一个主容器) | Running | phase由Pending变为Running | reconcile汇总容器状态后计算得出 |
| Pending | (长期无法调度/拉镜像失败且无重试余地) | Failed | 记录失败原因 | reconcile判断"已无法达成期望" |
| Running | CONTAINER_EXITED_ZERO (全部容器,且不会重启) | Succeeded | 标记为成功终态 | reconcile汇总所有容器终态后计算得出 |
| Running | CONTAINER_EXITED_NONZERO (全部容器终止,至少一个失败) | Failed | 记录失败详情 | reconcile汇总后计算得出 |
| Running | CONTAINER_EXITED_NONZERO (restartPolicy允许重启) | Running | 触发该容器重新进入Waiting->Running,phase不变 | reconcile决定重启动作,phase不变 |
| (任意phase) | NODE_UNREACHABLE | Unknown | 停止信任现有status,标记不可观测 | 外部判断,非本Pod内部事件 |
| Unknown | (节点重新上报心跳) | (恢复为重新计算出的真实phase) | 重新汇总容器状态 | reconcile重新计算 |
| (任意phase) | RECONCILE_TICK | (通常不变,除非发现遗漏的差距) | 重新对比spec与容器runtime实际情况 | 纯reconcile,可能什么都不做 |

这张表故意留了几行"下一状态取决于汇总判断,不是查表能直接得出的单一值"(比如`CONTAINER_EXITED_NONZERO`要看restartPolicy和其它容器的情况),这正是Step1修正点1提到的:**status不再是简单地被一次转移直接赋值,而是reconcile综合多个输入计算出来的结果**,转移表能作为"参考、帮助理解常见路径",但不能再被当作"权威的、完备的查表依据"。

## Step 5: ASCII状态图

```
                     SCHEDULED                CONTAINER_STARTED
   (创建Pod对象) --------------> +---------+ -------------------> +---------+
                                  | Pending |                      | Running |
                                  +---------+                      +---------+
                                    |    ^                          |  |   ^
                        拉镜像/初始化失败  |                CONTAINER_ |  |   | 重启
                        且无法恢复    |    | RECONCILE_TICK   EXITED_  |  |   | (restartPolicy
                                    v    | (通常原地不动)      ZERO   |  |   |  允许)
                              +--------+ |                    (全部) |  |   |
                              | Failed | <-------------------------- +  |   |
                              +--------+   CONTAINER_EXITED_NONZERO     |   |
                                   ^        (全部容器终止,含失败)         |   |
                                   |                                    v   |
                                   |                              +-----------+
                                   +----------------------------- | Succeeded |
                                       (若全部容器退出码为0且      +-----------+
                                        不再重启)

        NODE_UNREACHABLE(可能发生在Pending/Running/上方任意状态,虚线表示"横切"关系)
   +---------+                                                    +---------+
   | Unknown | <--------------------------------------------------| 任意phase|
   +---------+  --------------------------------------------------> +---------+
                节点重新上报心跳后,reconcile重新计算出真实phase(可能回到原状态或Failed)
```

这张图和前三课最大的不同在于:图上大部分箭头旁边其实应该标注的不是一个具体的事件名,而是"reconcile发现了XX差距",画成传统事件箭头是为了保持视觉上的一致性和可读性,但读图时要提醒自己——**这些箭头背后是一次对比计算,不是一次查表跳转**。

## Step 6: Python实现

完整代码见同目录下的`04_kubernetes_pod.py`。

**v1:简单版(Enum + match-case硬编码)**
先用最传统的方式写一版:`PodPhase`枚举 + `match`语句,故意让读者先体会"如果强行按事件驱动FSM来写会怎样"——写完会发现`CONTAINER_EXITED_NONZERO`这个分支里必须塞进"这是不是全部容器都退出了""这个容器还能不能重启"这些额外判断,代码里开始出现"不是查表就能解决,需要额外逐一检查所有容器状态"的味道,这是这一课要让你亲身感受到的裂缝,而不是一开始就用reconcile版本掩盖它。

**v2:Transition Table版(数据驱动)**
尝试把v1的判断抽成转移表,会发现**大部分转移的"next_state"其实没法只由`(phase, event)`这一对唯一决定**,必须传入"当前所有容器的状态列表"作为额外参数,表本身的价值被削弱了不少。这一版故意保留这个"不太好用"的过程,用来对比前三课转移表用得很顺畅的情形——**转移表数据驱动这个技巧,在"下一状态高度依赖一份需要汇总的外部数据"时,收益会明显下降**,这是一个值得记住的边界信号。

**v3:State Pattern版**
同样按传统方式实现,每个phase是一个类,`handle`方法接收"当前所有容器状态的快照"而不是单一事件,内部做汇总判断。写到这里应该已经能感觉到:**State Pattern本身没有错,但传入给它的"输入"已经从"一个离散事件"变成了"一份需要汇总的状态快照"，这已经不是标准State Pattern教科书描述的用法**,是在往reconcile的方向靠。

**v4:可复用FSM框架 + reconcile函数(修正后的版本)**
这一版不再勉强套用前三课的`GenericFSM`,而是新增一个独立的函数:

```python
def reconcile(desired: PodSpec, current: PodStatusSnapshot) -> PodAction
```

它接收"期望状态(spec)"和"当前观测到的状态快照(容器状态列表+已有phase)",返回"接下来应该采取的动作"(比如"启动某个容器""标记phase为Failed""什么都不做"),而不是"下一个phase"本身——**phase是reconcile执行完动作、观测到新事实之后,由一个单独的"汇总函数"重新计算出来的,不是reconcile直接返回的**。这个设计有意区分了三件事:期望(spec)、观测事实(容器状态)、摘要标签(phase),对应Step1提出的两个修正点。

**这算不算"FSM"、算不算"FSM的变体"?**

这是这一课要正面回答的问题,不回避:

- **不算传统意义上的FSM**,因为核心驱动力不是"(当前状态, 事件) -> 下一状态"的查表,而是"(期望, 观测) -> 该做的动作"的持续比较,并且这个比较即使没有新事件也会定期重新执行。
- **可以算是FSM的一种变体或者说"扩展形态"**——阮一峰、Kubernetes社区的一些资料把这种模式称为"Level-based(基于水平面/状态量)"而不是"Edge-triggered(基于边沿/事件触发)"的控制逻辑,这其实是控制理论里"电平触发 vs 边沿触发"这对概念在软件架构上的类比:传统FSM是边沿触发(只在状态变化的"边沿"上响应),reconcile循环是电平触发(只要"现状"和"期望"不一致这个条件成立,就会持续被处理,不依赖某一次事件是否被精确捕获到)。**枚举出来的"有限状态集合"(phase的五个值)依然是FSM思维贡献的部分,但"如何在状态之间移动"这件事已经被reconcile循环取代**。这正是为什么Step1给出的评级是三星而不是一星或五星——FSM思维的"识别有限状态"这一半依然成立且必要,"事件驱动转移"这一半需要替换成"持续协调"才能对上真实的实现。

## Step 7: 复杂度分析

**为什么"reconcile循环"比"事件驱动FSM"更适合这个场景?**
Kubernetes要处理的现实是:事件可能会丢失(网络分区、组件重启导致某条消息没送到)、系统里有大量独立运行的Pod/Node/Controller,谁都无法保证"我一定收到了所有相关事件"。如果完全依赖"事件驱动直接转移",一旦漏掉一个事件,系统状态就会永久卡在错误的地方,没有自我纠正的机会。**reconcile循环的核心价值就是"自我纠正":不管之前漏掉了什么事件,下一轮循环总会重新对比现状和期望,把差距找出来再处理一次**,这是分布式系统里应对"消息可能丢失、组件可能重启"这类不确定性的标准解法,不是Kubernetes的独创,而是从更早的运维自动化工具(比如Puppet/Chef这类"声明式配置管理"工具的收敛式apply循环)沿用过来的思路。

**这一模式什么时候是负担?**
如果一个系统里"事件不会丢、组件不会随意重启、状态转移的路径本身很短很确定"(比如第1课的systemd管理本机的一个服务,消息就是函数调用,不存在网络分区),那么持续跑一个"每隔几秒重新比对一遍"的循环,相比"收到事件就处理一次"纯属浪费CPU和引入不必要的延迟(reconcile周期决定了"最坏情况下要多久才能发现并纠正一个问题")。**reconcile模式的价值,只有在"系统必须假设消息会丢失/组件会重启/需要自我纠正"这个前提成立时才划得来**——这也是为什中心化单机的systemd不需要这套机制,而跨节点分布式协调的Kubernetes需要。

**什么时候不该用这种混合模式?**
如果你在写一个单机小工具,不涉及分布式协调、不需要应对"消息丢失"这类问题,那么直接照搬"reconcile循环"是过度设计——传统的事件驱动FSM(前三课的模式)已经足够,而且响应更即时(不需要等下一个reconcile周期)、代码更直观。判断标准可以归纳为:**只有当"状态可能因为你控制不了的原因(网络、其它组件、多副本竞争)而偏离期望,且系统需要能自动发现并纠正这种偏离"时,才需要引入reconcile循环这一层**。

## Step 8: FSM思维总结

```
==========
FSM识别口诀(Kubernetes Pod篇 —— 修正版)
==========
✓ 先分清"有限状态集合"(phase的五个值)和"驱动状态变化的机制"(事件 vs reconcile)是两件事
✓ 出现一份独立存在、持续起作用的"期望状态(spec)"时,传统"事件->转移"思路需要让位给"期望-观测对比"
✓ 下一状态如果要靠"汇总一堆细粒度状态"才能算出来,说明该转的是reconcile模式,不是查表模式
✓ Conditions(可以同时为真的一组布尔量)不是状态,不要和互斥的phase枚举混为一谈
✓ 判断该用事件驱动FSM还是reconcile循环:系统会不会丢事件/组件会不会重启,是关键分水岭
```

## Step 9: 迁移训练题

1. Kubernetes的`Deployment`资源同时有`spec.replicas`(期望副本数)和`status.replicas`/`status.availableReplicas`(观测到的实际副本数),ReplicaSet控制器不断reconcile让两者接近。这个案例比Pod本身更适合用来讨论"声明式期望+reconcile"吗?为什么Deployment层面这套模式体现得比Pod本身更"典型"?
2. Terraform这类基础设施即代码(IaC)工具的`plan`/`apply`流程(对比配置文件描述的期望状态和云资源的实际状态,计算出要新增/修改/删除哪些资源),和这一课讨论的reconcile循环有什么相似和不同?Terraform的"apply"是一次性的,Kubernetes的reconcile是持续的,这个差异重要吗?
3. 数据库主从复制里,从库不断追赶主库的最新binlog位置(可以理解为从库的"当前状态"持续向主库的"期望状态/最新进度"靠近,类似一个持续的追赶循环),这和Kubernetes的reconcile模式是不是同一类问题?如果要给"主从复制的追赶过程"建模,状态该怎么定义,还需要事件驱动FSM吗?

## 今天真正学会的一句话

当你发现一个系统里存在一份"独立于当前观测、持续起作用的期望"(spec),并且驱动状态变化的不是"收到事件就查表转移一次",而是"不管有没有新消息都会定期重新比较现状和期望、并自动纠正差距",这时候不要勉强把它塞进"事件驱动FSM"的模子——有限状态集合的枚举依然有用,但描述"如何移动"这部分要换成"reconcile(期望,观测)->动作"的持续协调思路,这才是这类系统真正的运作方式。
