# FSM 思维训练系统

## 训练目标

> 思维训练 > 代码实现 > API使用

这个训练系统的目的**不是**教你FSM(有限状态机)的语法——State、Event、Transition这些概念你已经知道了。

真正的目标是训练一种直觉:

**看到一个问题,大脑能在几秒内判断"这是一个状态机问题",然后自己设计出状态、事件、状态转移,最后落地成Python代码。**

大多数工程师"学过FSM"但"不会用FSM",根本原因是:知道语法,但没有识别问题结构的直觉。这个直觉只能通过大量案例的"判断训练"积累,不能靠背状态图获得。

## 怎么用这套教材

每一课都严格按照同一个十步法结构写成,**不要跳着看**,尤其是Step 1~5(判断是否适合FSM、发现状态、发现事件、画转移表、画图),因为这几步是训练"识别直觉"的核心,代码(Step 6)只是水到渠成的产物。

推荐的自测读法:

1. 读完标题,**先自己合上文档**,尝试独立回答 Step 1(这个问题适不适合FSM?为什么?)
2. 再展开文档对照,看看自己漏掉了哪些特征
3. Step 2/Step 3 同样先自己列一遍状态和事件,再对照修正
4. Step 4/5(转移表、ASCII图)可以直接看,这是"整理"而不是"发现"的步骤
5. Step 6 的四版代码,建议先看v1能不能自己写出来,再逐版看优化点
6. Step 7/8 是复盘,重点看"什么时候不该用FSM"——这比"什么时候该用FSM"更容易被忽视
7. Step 9 的3道迁移题,**必须自己先判断完再看下一课**,这才是真正的训练,不是摆设

## 课程路线图

### Level 1 —— 生活场景(热身:直觉最明显的物理设备)

| # | 主题 | 文件 | 侧重点 |
|---|------|------|--------|
| 1 | 交通灯 | [level1_life/01_traffic_light.md](level1_life/01_traffic_light.md) | 最经典入门案例:固定周期切换,理解"状态"和"事件驱动的切换"的区别 |
| 2 | 电梯 | [level1_life/02_elevator.md](level1_life/02_elevator.md) | 引入"依赖历史状态"(当前楼层、请求队列)和状态内部数据 |
| 3 | 自动门 | [level1_life/03_auto_door.md](level1_life/03_auto_door.md) | 引入"超时事件"(定时器驱动的自动转移),而非只有外部输入 |
| 4 | 饮水机 | [level1_life/04_water_dispenser.md](level1_life/04_water_dispenser.md) | 引入"多状态并发嫌疑"辨析:哪些看起来像状态、实际只是标志位 |

### Level 2 —— 小游戏(行为逻辑:状态驱动的动作与AI)

| # | 主题 | 文件 | 侧重点 |
|---|------|------|--------|
| 1 | 人物移动 | [level2_game/01_character_move.md](level2_game/01_character_move.md) | 输入映射到状态而非直接映射到动作 |
| 2 | 角色攻击 | [level2_game/02_character_attack.md](level2_game/02_character_attack.md) | 引入"不可打断的状态"(攻击动画锁定期间事件被忽略) |
| 3 | NPC状态 | [level2_game/03_npc_state.md](level2_game/03_npc_state.md) | 引入"感知驱动的自动转移"(基于距离/视野的隐式事件) |
| 4 | Boss AI | [level2_game/04_boss_ai.md](level2_game/04_boss_ai.md) | 引入分层状态机(Hierarchical FSM)的必要性判断 |

### Level 3 —— Python工程场景(从玩具问题过渡到真实业务代码)

| # | 主题 | 文件 | 侧重点 |
|---|------|------|--------|
| 1 | 订单状态 | [level3_python/01_order_status.md](level3_python/01_order_status.md) | 业务状态机:非法转移必须被拒绝而不是静默忽略 |
| 2 | 登录流程 | [level3_python/02_login_flow.md](level3_python/02_login_flow.md) | 安全相关状态机:失败计数、锁定,状态里带业务数据 |
| 3 | TCP连接状态(应用层视角) | [level3_python/03_tcp_connection.md](level3_python/03_tcp_connection.md) | 从应用层socket使用者角度理解状态机,为Level 4铺垫 |
| 4 | HTTP请求流程 | [level3_python/04_http_request_flow.md](level3_python/04_http_request_flow.md) | 辨析"顺序流程(pipeline)"与"真正的状态机"的边界 |

### Level 4 —— 网络协议(工业级FSM:标准文档定义的状态机)

| # | 主题 | 文件 | 侧重点 |
|---|------|------|--------|
| 1 | TCP FSM | [level4_protocol/01_tcp_fsm.md](level4_protocol/01_tcp_fsm.md) | RFC793经典11状态机,理解"半关闭"等复杂状态语义 |
| 2 | OSPF Neighbor FSM | [level4_protocol/02_ospf_neighbor_fsm.md](level4_protocol/02_ospf_neighbor_fsm.md) | 理解"每个邻居独立一台状态机"的建模思路 |
| 3 | BGP FSM | [level4_protocol/03_bgp_fsm.md](level4_protocol/03_bgp_fsm.md) | 理解状态机与计时器(ConnectRetryTimer等)协同工作 |
| 4 | DHCP FSM | [level4_protocol/04_dhcp_fsm.md](level4_protocol/04_dhcp_fsm.md) | 客户端视角状态机,理解"租约"这类带时间属性的状态 |
| 5 | ARP FSM | [level4_protocol/05_arp_fsm.md](level4_protocol/05_arp_fsm.md) | 对比一个"几乎不需要FSM"的协议——训练识别边界的反例 |
| 6 | FRR源码FSM | [level4_protocol/06_frr_source_fsm.md](level4_protocol/06_frr_source_fsm.md) | 阅读真实C项目中FSM的工程实现模式(函数指针状态表) |

### Level 5 —— Linux系统(生产系统里无处不在的FSM)

| # | 主题 | 文件 | 侧重点 |
|---|------|------|--------|
| 1 | systemd Unit状态 | [level5_linux/01_systemd_unit.md](level5_linux/01_systemd_unit.md) | 理解"目标状态 vs 当前状态"的调度式状态机 |
| 2 | NetworkManager连接状态 | [level5_linux/02_networkmanager.md](level5_linux/02_networkmanager.md) | 理解设备状态机与连接状态机的分层关系 |
| 3 | 容器生命周期 | [level5_linux/03_container_lifecycle.md](level5_linux/03_container_lifecycle.md) | 理解runtime状态机与编排层状态机的区别 |
| 4 | Kubernetes Pod状态 | [level5_linux/04_kubernetes_pod.md](level5_linux/04_kubernetes_pod.md) | 理解"声明式期望状态"场景下FSM思维为何要调整 |
| 5 | DPDK Port State | [level5_linux/05_dpdk_port_state.md](level5_linux/05_dpdk_port_state.md) | 理解高性能数据面场景下状态机与资源生命周期绑定 |

## 每课固定结构

```
Step 1: 是否适合FSM        —— 判断训练,给出星级评分和理由
Step 2: 发现状态            —— 引导性问题 + 状态清单(含互斥性/持续性说明)
Step 3: 寻找事件            —— 引导性问题 + 事件清单(区分动作/事件/应忽略输入)
Step 4: 状态转移表          —— Current State / Event / Next State / Action
Step 5: ASCII状态图          —— 纯ASCII箭头图
Step 6: Python实现          —— v1(Enum+if/match) → v2(转移表) → v3(State Pattern) → v4(可复用框架)
Step 7: 复杂度分析          —— 为什么FSM更好 / 什么时候是负担 / 什么时候不该用
Step 8: FSM思维总结          —— FSM识别口诀
Step 9: 迁移训练题          —— 3道同/邻近难度判断题,不给答案
今天真正学会的一句话
```

## 代码规范

所有 `.py` 文件遵循:

- Python 3.12+
- 类型注解(type hints)
- `Enum` 表示状态/事件
- `dataclass` 表示带数据的状态(适合时)
- `match-case` 做状态转移分支(适合时)
- PEP8
- 不依赖第三方FSM库(如 `python-statemachine`、`transitions`),全部手写以强化原理理解

## 关于 Level 4/5 的内容边界

- Level 4 的协议状态机(TCP/OSPF/BGP/DHCP/ARP)按公开标准(RFC793、RFC2328、RFC4271、RFC2131、RFC826)简化建模用于教学,**不是**生产级协议栈实现,忽略了大量真实实现中的边界情况和性能优化。
- `06_frr_source_fsm` 没有独立可运行的 `.py`,因为它讨论的是"如何在真实C语言项目中识别FSM工程实现模式"(如函数指针状态转移表 `struct {state, event} -> handler`),文档中以伪代码/简化Python移植版辅助理解,不是完整移植。
- Level 5 的系统级状态机(systemd/NetworkManager/容器/K8s Pod/DPDK)同样是简化的教学模型,用来训练"从真实系统文档中提炼出FSM结构"的能力,不追求覆盖官方实现的全部细节。
