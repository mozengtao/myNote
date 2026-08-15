# Lesson 02: NetworkManager连接状态 (Level 5 - Linux系统)

## Step 1: 是否适合FSM

先套用标准五条检查,再单独处理这一课真正的重点——"分层"。

- **是否存在有限状态?**
  是。NetworkManager给每个网络设备(比如`eth0`、`wlan0`)维护一个`NM_DEVICE_STATE`,取值有限且在D-Bus接口/`nmcli`里能直接查到:`UNMANAGED`、`UNAVAILABLE`、`DISCONNECTED`、`PREPARE`、`CONFIG`、`NEED_AUTH`、`IP_CONFIG`、`IP_CHECK`、`SECONDARIES`、`ACTIVATED`、`DEACTIVATING`、`FAILED`(还有一个`UNKNOWN`用作初始/异常兜底)。十二个状态,数量比前一课多,但依然有限、可枚举。

- **是否依赖历史状态?**
  是,而且这一课比第1课更明显。设备从`DISCONNECTED`到`ACTIVATED`要经过一串固定顺序的中间态(`PREPARE → CONFIG → (NEED_AUTH) → IP_CONFIG → IP_CHECK → SECONDARIES → ACTIVATED`),"现在处于哪一步"直接决定"下一步该做什么、该等哪个信号"。这是典型的"强顺序依赖"场景,FSM的价值会比第1课更突出。

- **是否由事件驱动?**
  是。事件来源很杂:用户/`nmcli`发出的激活请求,底层链路信号(网线插拔的carrier变化、Wi-Fi关联结果),DHCP客户端的应答或超时,802.1X/WPA认证结果,IP冲突检测(ARP probe)结果等。都是离散、可枚举的事实。

- **是否同一个事件在不同状态行为不同?**
  是。比如"链路丢失(carrier down)"这个事件:在`ACTIVATED`状态下会触发`DEACTIVATING`(连接被拆除);在`UNAVAILABLE`状态下则什么都不用做(本来就没链路);在`PREPARE`/`CONFIG`这些激活过程中途,链路丢失通常直接导致激活失败,走向`FAILED`而不是慢慢退回。同一个底层事实,在不同阶段有截然不同的语义。

- **是否存在明确状态切换?**
  是。每一次`NM_DEVICE_STATE`变化,`nmcli`和D-Bus信号`StateChanged`都会精确广播,包含"旧状态、新状态、变化原因(reason code)",切换点非常清晰。

**这一课真正的复杂之处:"设备状态机"和"连接profile状态机"是两台不同的FSM**

前面四条只回答了"设备(Device)本身要不要用FSM建模",但NetworkManager还有第二层概念——**连接配置(Connection Profile)**,比如"办公室WiFi"和"家里WiFi"是两个不同的profile,都可以配置在同一块Wi-Fi网卡上。当你执行"激活连接",NetworkManager会创建一个`ActiveConnection`对象,它自己也有一套状态(`NM_ACTIVE_CONNECTION_STATE`:`ACTIVATING`/`ACTIVATED`/`DEACTIVATING`/`DEACTIVATED`),生命周期比Device短——**设备是长期存在的物理/虚拟接口,而ActiveConnection是"这一次激活某个profile的尝试",每次切换profile都会创建一个新的ActiveConnection实例,而Device对象本身持续存在,只是它的DeviceState随着当前活跃的ActiveConnection的进度而联动变化**。

也就是说,这不是"一个FSM",而是**两台耦合的FSM**:一台生命周期长(Device),一台生命周期短且可以反复创建/销毁(ActiveConnection),Device的状态在很大程度上是ActiveConnection进度的"投影",但又不完全等价(比如`UNMANAGED`/`UNAVAILABLE`跟任何ActiveConnection都无关,是Device自己独有的状态)。这一课会先建Device这台十二状态的FSM作为主线,在Step 2末尾专门讨论这种分层关系,而不回避它。

**适合FSM: ★★★★★**

理由:这是NetworkManager在`nm-device.c`/`libnm`公开API里实实在在实现的状态机,状态数量多、顺序依赖强、事件来源多样,是FSM该出场的典型场景。更重要的是,它提供了一个"单层FSM思维不够用,需要理解FSM之间的层级/协作关系"的真实教学案例,这一点比状态数量本身更值得训练。

## Step 2: 发现状态

**引导性问题:**
- `nmcli device status`一次能看到"DEVICE/TYPE/STATE/CONNECTION"这几列,它们都是"状态"吗?
- "网线有没有插上(carrier)"是设备的状态,还是设备状态机的一个输入信号?
- 激活到一半卡在"正在等WPA握手"和"正在等DHCP",这算两个不同状态,还是同一个状态的两种"原因"?
- 一块网卡同时"被NetworkManager管理"和"当前没有插网线",这两件事是不是同一维度?

**思考过程:**

先把`nmcli`/D-Bus里能看到的候选项摆出来:

1. **`STATE`列(NM_DEVICE_STATE)** —— 这是本课的主角,十二个取值全部满足"互斥、持续到明确事件才改变"的标准,合格。

2. **`CONNECTION`列(当前绑定的profile名字,如果有)** —— 容易被误认为"状态的一部分",但它其实是"当前ActiveConnection对象指向的是哪个profile"这一信息,是Device状态机之外、由ActiveConnection这台协作FSM提供的数据。同一个DeviceState(比如`ACTIVATED`),`CONNECTION`列的值可以是"办公室WiFi"或者"家里WiFi"完全取决于用户激活了哪个profile,不影响DeviceState的判定逻辑本身,所以不纳入Device状态清单。

3. **`carrier`(链路层是否检测到信号,比如网线插好、光收发正常)** —— 容易被当成状态,但它更准确的角色是"一个持续变化的外部信号/事件源",而不是DeviceState本身的一个取值。`carrier`从"无"变"有"会触发`UNAVAILABLE → DISCONNECTED`这样的转移,但`carrier`本身不是DeviceState枚举里的一项——把它当成"事件"(Step 3详细展开)更贴合真实实现。

4. **"是否被NetworkManager托管(managed)"** —— 这其实直接对应`UNMANAGED`这个DeviceState本身(设备被排除在NM管理范围之外,比如通过udev规则、`nmcli device set eth0 managed no`,或者设备被其它工具如网络命名空间接管)。所以这一条不是独立的新维度,而是已经包含在状态清单第一项里了。

5. **"NEED_AUTH期间具体缺的是WiFi密码还是802.1X证书"、"IP_CONFIG期间具体是DHCP还是静态IP还是IPv6 SLAAC"** —— 这些是"同一个状态下,不同的实现路径/原因",不是需要拆分成新状态的东西。`NM_DEVICE_STATE_REASON`枚举(比如`NM_DEVICE_STATE_REASON_SUPPLICANT_TIMEOUT`、`NM_DEVICE_STATE_REASON_NO_SECRETS`)专门用来携带"为什么发生了这次转移"的详情,它是转移事件的附加数据,不是新的状态节点。

筛选标准不变:看是否满足(a)对外行为稳定、(b)持续到明确事件才结束、(c)与其他候选互斥。`carrier`不满足(a)(它是瞬时信号而非稳定阶段),`CONNECTION`列和"REASON"都是"状态之外的关联数据"。

**确定的状态清单(NM_DEVICE_STATE,设备层):**

| 状态 | 说明 | 互斥性 | 持续性 |
|---|---|---|---|
| UNMANAGED | 设备存在但不受NM管理 | 与其他互斥 | 持续到被设为managed |
| UNAVAILABLE | 受管但当前不可用(无carrier、驱动未就位等) | 同上 | 持续到carrier出现/设备就位 |
| DISCONNECTED | 可用但未激活任何连接 | 同上 | 持续到收到激活请求 |
| PREPARE | 激活流程开始,设备特定准备工作 | 同上 | 持续到准备完成 |
| CONFIG | 进行L2层配置(如WiFi关联) | 同上 | 持续到L2配置完成 |
| NEED_AUTH | 等待用户/系统提供密钥或证书 | 同上 | 持续到secrets被提供 |
| IP_CONFIG | 进行L3层配置(DHCP/静态IP/SLAAC) | 同上 | 持续到IP配置完成 |
| IP_CHECK | 检测IP冲突(ARP probe等) | 同上 | 持续到检测完成 |
| SECONDARIES | 等待依赖的次级连接(如VPN)就位 | 同上 | 持续到次级连接就位或判定不需要 |
| ACTIVATED | 连接完全建立,可正常使用 | 同上 | 持续到收到停用请求或链路/配置出问题 |
| DEACTIVATING | 正在拆除当前连接 | 同上 | 持续到清理完成 |
| FAILED | 激活过程失败 | 同上 | 持续到被重新尝试激活或设备被重置 |

十二个状态两两互斥,`nmcli device status`任意时刻每个设备只显示一个STATE,持续性和转移边界都很清晰。

**分层关系补充说明:**

上面这十二个状态属于**Device这台FSM**。与它协作的**ActiveConnection这台FSM**只有四个状态(`ACTIVATING`/`ACTIVATED`/`DEACTIVATING`/`DEACTIVATED`),生命周期是"每次激活一个profile就新建一个实例,停用后销毁"。两者的关系可以概括为:**Device的`PREPARE`到`SECONDARIES`这几个中间态,基本是ActiveConnection从`ACTIVATING`推进到`ACTIVATED`过程的细化展开;但Device还有`UNMANAGED`/`UNAVAILABLE`这类跟"当前有没有在激活某个profile"完全无关的状态,这是ActiveConnection这台FSM根本不需要关心的**。同一块网卡在一天之内可能激活"办公室WiFi"失败(产生一个ActiveConnection走到DEACTIVATED)、再激活"手机热点"成功(产生另一个新的ActiveConnection走到ACTIVATED),Device对象本身只有一个,但背后驱动它状态变化的ActiveConnection实例换了好几个——这正是"分层FSM"要训练的直觉:**先分清"谁的生命周期更长、谁的生命周期短且可反复创建",再看长生命周期的状态机在多大程度上是由短生命周期状态机的进度所驱动的**。

## Step 3: 寻找事件

**引导性问题:**
- 网线被拔掉这件事,应该在哪个状态下才会引发转移?在其它状态下是不是应该被忽略?
- DHCP超时和DHCP失败(比如收到NAK)是同一种事件,还是要分开建模?
- "用户在NEED_AUTH阶段提供了密码"和"用户一开始激活连接时就带上了保存好的密码"是两种不同的事件路径吗?
- NetworkManager会不会自己"定期检查"设备状态,类似第4课要讨论的reconcile循环?

**分析过程:**

- **carrier变化(网线插入/拔出、光模块信号出现/消失)** —— 真正的事件,但语义因当前状态而完全不同:`UNAVAILABLE`状态下carrier出现,触发`UNAVAILABLE → DISCONNECTED`;`ACTIVATED`状态下carrier消失,触发`ACTIVATED → DEACTIVATING → DISCONNECTED`(有线网卡);对Wi-Fi设备,"carrier"的等价物是"是否处于某个可关联的射频环境"这类间接信号,道理相通。
- **激活请求(用户/`nmcli`/dispatcher脚本触发`ActivateConnection`)** —— 真正的事件,触发`DISCONNECTED → PREPARE`。
- **L2配置完成(比如WiFi关联成功、以太网无需额外L2步骤直接视为完成)** —— 真正的事件,触发`CONFIG → IP_CONFIG`(不需要认证)或`CONFIG → NEED_AUTH`(需要密钥但尚未提供)。
- **secrets(密码/证书)被提供** —— 真正的事件,触发`NEED_AUTH → CONFIG`(回去重新走L2认证流程)。这里有个容易被误解的地方:`NEED_AUTH`不是"CONFIG"的并列分支,而更像是"CONFIG流程中途因为缺东西被挂起,补上以后要重新走一次CONFIG",这提示我们转移图里会有"回头看似退步、实际是补齐条件后重试"的边。
- **DHCP获得租约 / 静态IP配置生效 / IPv6 SLAAC完成** —— 真正的事件,触发`IP_CONFIG → IP_CHECK`。
- **DHCP超时或收到否定应答(NAK)** —— 真正的事件,但要看有没有备用方案(比如同时配置了IPv6或者有fallback静态地址),没有备用方案则触发`IP_CONFIG → FAILED`。DHCP超时和收到NAK在NetworkManager看来都归入"IP配置失败"这一类事件语义,细分原因由`NM_DEVICE_STATE_REASON`记录,不需要建模成不同的转移目标。
- **IP冲突检测完成且无冲突 / 检测到冲突** —— 真正的事件,前者触发`IP_CHECK → SECONDARIES`,后者触发`IP_CHECK → FAILED`(或按配置重新走IP_CONFIG,取决于策略,这一课按最常见情况建模为FAILED)。
- **次级连接(如VPN)就位,或者判定本次激活不需要任何次级连接** —— 真正的事件,触发`SECONDARIES → ACTIVATED`。这是一个"大多数情况下瞬间通过"的状态,因为大多数连接根本没有配置次级依赖,但它确实是一个独立的、真实存在的检查点。
- **停用请求(用户主动断开,或者上层应用请求切换profile)** —— 真正的事件,触发`ACTIVATED → DEACTIVATING`。
- **激活超时(任意中间状态卡太久)** —— 真正的事件,触发对应状态直接进入`FAILED`。
- **"设备被设为unmanaged/managed"** —— 真正的事件,直接进出`UNMANAGED`状态,优先级很高(不管当前处于激活流程的哪一步,一旦被设为unmanaged都会中断转到UNMANAGED)。

**关于"周期性检查"是不是事件:** 和第1课的Job调度不同,NetworkManager的Device状态机整体上是**纯事件驱动**的,没有类似"控制器每隔几秒重新核对一次期望状态"的显式循环(那是第4课Kubernetes的模式)。但要注意两个例外:一是`autoconnect`机制——如果设备处于`DISCONNECTED`且有标记了`autoconnect=yes`的profile,NetworkManager会在合适的时机(比如设备变为可用、或者定期的connectivity检查失败后)尝试自动激活,这可以理解成"系统生成的隐式ActivateConnection事件",和第1课的`AUTO_RESTART`是同一类"内部生成事件"的思路;二是`connectivity check`(定期访问一个已知URL判断是否真的能上网)属于ACTIVATED状态下的一个持续监控动作,它的结果会影响`nmcli`显示的连接质量提示,但不会直接把DeviceState拉出ACTIVATED——这是一个"看起来像周期性reconcile,但实际不改变本课定义的核心状态"的旁支,值得知道但不需要纳入转移表。

**确定的事件清单:**

| 事件 | 说明 |
|---|---|
| CARRIER_UP / CARRIER_DOWN | 链路层信号出现/消失 |
| MANAGE / UNMANAGE | 设备被纳入/移出NM管理 |
| ACTIVATE_REQUESTED | 收到激活某个连接profile的请求 |
| L2_CONFIG_DONE | L2层配置(如WiFi关联)完成 |
| SECRETS_NEEDED | L2或L3认证缺少密钥/证书 |
| SECRETS_PROVIDED | 密钥/证书已提供 |
| L3_CONFIG_DONE | IP配置(DHCP/静态/SLAAC)完成 |
| L3_CONFIG_FAILED | IP配置失败(超时/NAK且无备用方案) |
| IP_CHECK_PASSED / IP_CHECK_CONFLICT | IP冲突检测通过/发现冲突 |
| SECONDARIES_READY | 次级连接就位或判定无需等待 |
| DEACTIVATE_REQUESTED | 收到停用请求 |
| ACTIVATION_TIMEOUT | 激活流程某一步超时 |
| AUTO_ACTIVATE (隐式) | autoconnect机制自动生成的激活请求 |

## Step 4: 状态转移表

| Current State | Event | Next State | Action |
|---|---|---|---|
| UNAVAILABLE | CARRIER_UP | DISCONNECTED | 标记链路可用,评估autoconnect profile |
| DISCONNECTED | ACTIVATE_REQUESTED / AUTO_ACTIVATE | PREPARE | 创建ActiveConnection对象,开始激活流程 |
| PREPARE | (设备特定准备完成) | CONFIG | 进行L2层配置(如发起WiFi关联) |
| CONFIG | L2_CONFIG_DONE | IP_CONFIG | 若无需认证,直接进入IP配置 |
| CONFIG | SECRETS_NEEDED | NEED_AUTH | 请求上层(agent)提供密钥 |
| NEED_AUTH | SECRETS_PROVIDED | CONFIG | 用新提供的密钥重试L2配置 |
| IP_CONFIG | L3_CONFIG_DONE | IP_CHECK | 进行IP冲突检测(ARP probe) |
| IP_CONFIG | L3_CONFIG_FAILED | FAILED | 记录失败原因,清理已分配的部分资源 |
| IP_CHECK | IP_CHECK_PASSED | SECONDARIES | 检查是否有次级连接需要等待 |
| IP_CHECK | IP_CHECK_CONFLICT | FAILED | 记录IP冲突,放弃本次激活 |
| SECONDARIES | SECONDARIES_READY | ACTIVATED | 标记ActiveConnection状态为ACTIVATED |
| ACTIVATED | DEACTIVATE_REQUESTED / CARRIER_DOWN | DEACTIVATING | 拆除路由/IP配置,通知ActiveConnection进入DEACTIVATING |
| DEACTIVATING | (清理完成) | DISCONNECTED | 销毁ActiveConnection对象 |
| (PREPARE/CONFIG/NEED_AUTH/IP_CONFIG/IP_CHECK/SECONDARIES) | ACTIVATION_TIMEOUT | FAILED | 中止激活流程,记录超时原因 |
| FAILED | ACTIVATE_REQUESTED | PREPARE | 重新尝试激活(可能换一个profile) |
| (任意状态) | UNMANAGE | UNMANAGED | 中止任何进行中的激活,交出管理权 |
| UNMANAGED | MANAGE | UNAVAILABLE / DISCONNECTED | 视carrier情况决定落到哪个状态 |

## Step 5: ASCII状态图

```
   +-----------+  MANAGE(无carrier)   +-------------+  CARRIER_UP   +--------------+
   | UNMANAGED | --------------------> | UNAVAILABLE | -------------> | DISCONNECTED |
   +-----------+                       +-------------+                +--------------+
        ^  ^                                                                 |
        |  |                                                    ACTIVATE_REQUESTED /
        |  |                                                        AUTO_ACTIVATE
    UNMANAGE (任意状态,虚线省略画出)                                        v
        |  |                                                          +---------+
        |  +---------------------------- ACTIVATION_TIMEOUT --------- | PREPARE |
        |                                        ^                    +---------+
        |                                        |                         |
        |                                        |                         v
        |                                   +--------+  L2_CONFIG_DONE  +--------+
        |                                   | FAILED | <--------------- | CONFIG |
        |                                   +--------+                 +--------+
        |                                     ^     |                    |    ^
        |                       ACTIVATE_REQUESTED  |          SECRETS_NEEDED |
        |                     (回到PREPARE,图中箭头  |                    v    | SECRETS_PROVIDED
        |                      从FAILED再出发,未重画) L3_CONFIG_FAILED  +-----------+
        |                                            |                | NEED_AUTH |
        |                                            |                +-----------+
        v                                            |
   +--------------+  DEACTIVATE_REQUESTED /   +-----------+  L3_CONFIG_DONE
   | DEACTIVATING | <----- CARRIER_DOWN ------ |ACTIVATED  | <---------------+
   +--------------+                            +-----------+                |
        |                                            ^                      |
        | (清理完成)                       SECONDARIES_READY           +----------+
        v                                            |                 | IP_CONFIG|
   +--------------+                          +--------------+          +----------+
   | DISCONNECTED | -------------------------> | SECONDARIES | <--------------+
   +--------------+                          +--------------+   IP_CHECK_PASSED
                                                     ^
                                                     |  IP_CHECK_CONFLICT (-> FAILED, 图中未画出)
                                              +-----------+
                                              | IP_CHECK  |
                                              +-----------+
```

这张图故意留了几处"箭头未画全"的注释,提醒一个真实工程里常见的情况:**当一个状态有"多个不同来源都能指向同一个失败终点(FAILED)"时,完整画出每一条箭头会让图变得难以阅读,教学/文档里常见做法是集中在转移表里列全,图里只画主干路径**——这也是"什么时候该用表、什么时候该用图"的实际取舍。

## Step 6: Python实现

完整代码见同目录下的`02_networkmanager.py`。

**v1:简单版(Enum + match-case硬编码)**
用两个Enum分别表示`DeviceState`(12个状态)和`Event`,一个类里用`match`硬编码全部转移。这一版会明显感觉到"分支数量比第1课多得多"——这正是这一课想让你亲身体会的:**状态数量从6个涨到12个,`match`分支的行数不是线性增长,而是接近"状态数×平均每状态可能事件数"地增长**,直观感受到为什么后面几版数据驱动的写法会更重要。

**v2:Transition Table版(数据驱动)**
转移表变成字典后,新增/修改一条转移只需要改一行数据,而不用在一大段`match`里定位到正确的分支。这一版专门加入"表驱动带来的一个新收益":可以直接把转移表**打印/导出成文档表格**(比如遍历字典生成Markdown表),这正是Step 4那张转移表能够"从代码自动生成、保持代码和文档同步"的现实途径,而不是靠人工同步维护两份。

**v3:State Pattern版(两台FSM协作)**
这一版是这一课真正的重点,不只是重复"每个状态一个类"的套路,而是引入**第二个FSM类**:`ActiveConnectionFSM`,状态只有`ACTIVATING`/`ACTIVATED`/`DEACTIVATING`/`DEACTIVATED`四个,由`DeviceFSM`在激活开始时创建一个新实例、在停用完成后销毁。`DeviceFSM`的部分转移动作里会调用`ActiveConnectionFSM`的方法来同步推进后者的状态,体现"一个协调者FSM驱动一个附属FSM"的协作模式,这是分层FSM在代码里最直接的落地方式。

**v4:可复用FSM框架**
复用第1课的`GenericFSM`/`TransitionRule`框架,但这一版额外展示"如何用同一套引擎，同时跑两个不同规模的FSM实例并让它们联动"——`DeviceFSM`的某些转移动作(`action`)里直接调用`ActiveConnectionFSM`实例的`dispatch()`。这验证了通用框架的可复用性,同时也暴露了它的边界:**框架本身不负责"两台FSM之间怎么互相通知",这部分协作逻辑仍然需要业务代码自己编排**,框架只解决"单台FSM内部怎么查表、怎么执行动作"。

## Step 7: 复杂度分析

**为什么FSM比if-else更好?**
十二个状态、十几种事件,如果用嵌套if-else,大概会退化成一层套一层的"如果当前是CONFIG且事件是XXX,再判断有没有配置认证……"——每加一个中间状态,外层判断的分支数就要跟着涨,而且没有办法一眼看出"从DISCONNECTED到ACTIVATED一共要经过哪几步、每一步失败会去哪"。转移表把这条主干链路以及每一步的失败出口都列成了可查的数据,新增一个中间检查步骤(比如未来NetworkManager真的加了新的检测环节)只需要在链路中间插入一个新状态和对应的两条转移,不需要重新梳理整条if-else链。

**这一课的"分层FSM"什么时候是负担?**
如果你只是写一个简单脚本,一次只连一个固定的WiFi、不需要处理profile切换、也不关心VPN次级连接,那么维护"Device FSM + ActiveConnection FSM"两套状态机确实是过度设计——用一个单层的、七八个状态的转移表描述"从断开到连上"的过程就够了。分层的价值出现在**"同一个物理设备上,存在多个profile会被反复切换激活,且这些profile的生命周期管理(创建、销毁、失败重试)需要和设备本身的可用性状态分开追踪"**这个真实需求出现之后。

**什么时候不该用FSM?**
如果一块网卡永远只用一种连接方式(比如一台专用网关设备,出厂配置好一个静态IP,永不切换),那么这一整套十二状态机对这个场景是巨大的浪费——一个"已连接/未连接"的布尔值加上开机自检脚本就足够。这提示一个通用判断:**FSM该建模到多细,取决于这个系统实际会经历多少种"真实存在、需要被分别处理"的中间状态,不是"标准规定了十二个状态"就必须照抄全部十二个**——教学案例为了完整性会展示全部状态,但实际项目应该只建模用得到的那部分。

## Step 8: FSM思维总结

```
==========
FSM识别口诀(NetworkManager篇)
==========
✓ 状态数量变多、且转移路径有强顺序依赖(PREPARE->CONFIG->...->ACTIVATED)时,FSM优势更明显
✓ carrier/链路信号这类"持续变化的外部信号",建模成触发转移的事件,而不是状态本身
✓ 同一事件(如链路丢失)在不同阶段的语义不同,不能只用一条全局规则处理
✓ 发现"长生命周期对象的状态"和"短生命周期、可反复创建销毁的对象的状态"同时存在时,
  考虑拆成两台协作的FSM,而不是塞进一张表
✓ 只建模真实用得到的状态子集,不必照抄标准定义的全部状态
```

## Step 9: 迁移训练题

1. `systemd-networkd`(NetworkManager的一个轻量替代品)对每个网络接口也维护类似的`LinkOperationalState`(如`off`/`no-carrier`/`carrier`/`degraded`/`routable`)。这套状态和本课的`NM_DEVICE_STATE`相比,状态粒度更粗还是更细?这种差异反映了两个项目设计目标上的什么不同?
2. 一台笔记本同时连着有线网卡(插了网线)和无线网卡(连着WiFi),操作系统要决定"默认路由走哪个接口"(路由优先级/metric)。这个"选路"逻辑,应该建模成第三台独立的FSM,还是应该看作是两台Device FSM各自状态变化后触发的一个"外部决策函数"(不是FSM)?为什么?
3. 手机的移动网络(蜂窝网络)有一个和WiFi概念上很像但实现完全不同的连接状态机(涉及APN配置、注册网络、建立PDP上下文/PDU会话)。如果你要给"手机的网络连接管理"整体建模,是应该让"WiFi FSM"和"蜂窝网络FSM"完全独立,还是需要一个更上层的"当前活跃网络选择"FSM来协调二者?这个上层协调者本身满不满足"适合FSM"的五条特征?

## 今天真正学会的一句话

当你发现系统里同时存在"一个长期存在、状态变化较慢的对象"和"依附在它上面、可以被反复创建和销毁、状态变化更快的对象"时,不要试图用一张表同时描述两者——先分别识别这两台FSM各自的状态和事件,再单独想清楚"谁的转移会触发谁的转移",这比强行把所有状态塞进同一个枚举更接近系统的真实结构。
