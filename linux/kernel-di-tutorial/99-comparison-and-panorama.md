# 第三部分：所有 DI 技术横向比较 / 第四部分：Kernel DI 全景图 / 第五部分：设计思想总结

> 本文汇总本系列教程涉及的全部 20 种 Kernel DI 手法，**全部已完成**，均在对应的
> `01`~`20` 号文档中有完整的 Why/Architecture/Lifecycle/Relationship/Data Flow/State/
> Failure/Kernel Core Pseudocode/User-space Examples/与其它手法比较十段式展开，并配有
> 实测编译运行的用户态 C 示例（`c/01-.../` ~ `c/20-.../`）。文档编号与下表的编号一一对应。

---

## 第三部分：所有 DI 技术横向比较

| # | 手法 | 注入对象 | 生命周期 | 谁拥有对象 | 谁负责调用 | 是否支持多个实现 | 是否支持热插拔 | 是否属于多态 | Kernel 示例 | 用户空间对应 |
|---|------|----------|----------|------------|------------|------------------|----------------|--------------|-------------|--------------|
| [01](01-function-pointer-callback.md) | Function Pointer Callback | 单个函数指针 | 注册->调用->注销，运行期任意时刻 | 注册方（Core 的表/字段） | Core，在事件发生时盲调 | 视 Core 设计（表/链表可支持） | 是 | 弱多态（单点替换） | `timer_list->function`、`irq action->handler` | [`c/01-function-pointer-callback/`](c/01-function-pointer-callback/) 插件系统/Reactor/命令分发器 |
| [02](02-ops-table.md) | Operations Table (ops) | 一组函数指针（结构体） | 对象创建时注入，随对象销毁 | 对象自身持有指针，指向 `static const` 共享数据 | Core，通过 `obj->ops->fn()` | 否，一对一（但可运行期切换指向） | 部分（可切换 ops 指针） | **强多态**，等价手写 vtable | `file_operations`、`net_device_ops`、`vm_operations_struct` | [`c/02-ops-table/`](c/02-ops-table/) 形状多态/迷你VFS/策略模式 |
| [03](03-context-injection.md) | Context Injection | 私有数据（`void*`或内嵌结构体） | 与宿主业务对象同生命周期 | 业务对象自身 | Core 传回指针，业务代码 `container_of` 还原 | 是（每个实例独立上下文） | 是 | 不涉及多态，是多态的"配套设施" | `work_struct`+`container_of`、`kobject` | [`c/03-context-injection/`](c/03-context-injection/) container_of/线程池/timer |
| [04](04-driver-registration.md) | Driver Registration | 整个"驱动实体"（ops+元数据+匹配规则） | register->match->probe->...->remove->unregister | 总线（bus）持有注册表，驱动/设备自己持有绑定关系 | bus core，匹配成功后调用 `probe`/`remove` | **是**，多驱动可竞争同一设备 | **是**，核心特性 | 强多态 + 动态路由 | `pci_driver`、`platform_driver`、`usb_driver` | [`c/04-driver-registration/`](c/04-driver-registration/) bus匹配/热插拔注册表/多驱动probe |
| [05](05-notifier-chain.md) | Notifier Chain | 单个回调 + 优先级 | register->（任意次广播）->unregister | 事件源持有链表头 | Core，事件发生时遍历链表逐一盲调 | **是**，天然设计为链表 | 是 | 不算典型多态，是"一对多分发" | `notifier_block`、`atomic/blocking/raw/srcu_notifier_chain` | [`c/05-notifier-chain/`](c/05-notifier-chain/) notifier实现/pubsub/GUI广播 |
| [06](06-initcall.md) | Initcall | 一批函数指针（`initcall_t`） | 编译期声明，链接期收集，运行期恰好执行一次 | 无运行期"拥有者"，靠 linker section | `do_initcalls()`，按 level 顺序遍历 | 是（每个 level 内可以有任意多个） | **否**，无注销概念 | 不涉及多态，是"零成本注册" | `core_initcall`/`subsys_initcall`/`module_init` | [`c/06-initcall/`](c/06-initcall/) linker section模拟/分级初始化/自注册测试 |
| [07](07-workqueue.md) | Workqueue | 任务（数据+回调）的打包体 `work_struct` | alloc->INIT_WORK->queue->（worker 异步执行）->（复用或释放） | 业务对象自己持有（内嵌） | worker 线程池，异步取出并执行 | 是，多个 work 并发处理 | 是（workqueue 可创建/销毁） | 不涉及多态 | `struct work_struct`、`schedule_work`、`delayed_work` | [`c/07-workqueue/`](c/07-workqueue/) 从零实现workqueue/delayed_work/线程池 |
| [08](08-security-hooks-lsm.md) | Security Hooks (LSM) | 一组安全检查回调（ops 或 hook 链） | register（本仓库单例，现代多例可堆叠）->（每次敏感操作强制调用）->（几乎不注销） | Core 持有 `security_ops`/hook 链表 | Core 的 `security_xxx()` 包装函数，敏感操作路径强制调用 | 现代 Kernel 是，本仓库版本否（单例） | 否（安全模块通常不支持运行期热插拔） | 强多态 + "一票否决"语义 | `security_operations`、`security_file_permission` | [`c/08-security-hooks-lsm/`](c/08-security-hooks-lsm/) LSM hook链/策略注入/审计管道 |
| [09](09-bus-device-driver-injection.md) | Bus/Device/Driver Object Injection | `device`/`device_driver`/`bus_type` 三元对象 | 与 04 相同的调用时序，但这一篇讲的是其**总线无关**的底层通用基础设施 | `bus_type` 持有 device/driver 双向列表 | `drivers/base/{bus,dd,core}.c` 核心逻辑，总线无关 | 是，任意多种总线类型可并存 | 是 | 强多态 | `struct bus_type`、`struct device`、`device_driver` | [`c/09-bus-device-driver-injection/`](c/09-bus-device-driver-injection/) 通用bus核心/多总线共存/uevent热插拔通知 |
| [10](10-scheduler-class.md) | Scheduler Class | 一整套调度策略 ops（`enqueue`/`dequeue`/`pick_next`） | 内核编译期固定（`fair_sched_class`等），任务运行期"归属"某个 class | 调度核心持有 class 链（按优先级 `next` 指针串联，本仓库为 stop->rt->fair->idle） | `schedule()` 核心，按 class 优先级依次询问 | 是，多个 class 共存 | 否（class 本身编译期固定，任务在 class 间切换是运行期的） | 强多态 | `struct sched_class`（`kernel/sched.c`） | [`c/10-scheduler-class/`](c/10-scheduler-class/) 调度类链式回退/可插拔策略/enqueue-dequeue生命周期 |
| [11](11-allocator-injection.md) | Allocator Injection | 内存/资源分配策略（`ctor` 回调 + 编译期可选后端） | 通常随子系统/cache 生命周期 | 具体分配器实现（SLAB/SLUB） | 调用方通过统一接口（`kmalloc`/`kmem_cache_alloc`），内部路由到具体分配器 | 是（SLAB/SLUB 二选一编译，`ctor` 可运行期注入） | 否（后端选择是编译期，`ctor` 本身是运行期注入） | 强多态（编译期多态） | `kmem_cache_create`（`ctor`）、`mm/slab.c` vs `mm/slub.c` | [`c/11-allocator-injection/`](c/11-allocator-injection/) kmem_cache构造回调/可插拔分配后端/自定义arena |
| [12](12-policy-injection.md) | Policy Injection | 决策逻辑（函数指针/ops），与 08 的"链式否决"不同，这里是"互斥单选" | 与所属子系统同生命周期，运行期可整体切换 | 核心持有"当前生效策略"指针 | Core 检查点调用当前唯一生效的策略 | 是（多个策略实现，但同一时刻只有一个生效） | 是（可运行期切换） | 强多态 | `struct elevator_type`、`struct tcp_congestion_ops` | [`c/12-policy-injection/`](c/12-policy-injection/) IO调度策略/拥塞控制算法/sysctl运行期切换 |
| [13](13-linker-section-registration.md) | Linker Section Registration | 任意"编译期已知、运行期按需匹配"的数据（不局限于 06 的"全部执行"模式） | 编译期声明，链接期收集，运行期按条件匹配执行（而非无条件全部执行） | 链接器（无运行期对象） | 遍历 `__setup_start`~`__setup_end` 等 section 并按条件匹配 | 是 | 否 | 不涉及多态 | `__setup()`/`early_param()`、`DECLARE_PCI_FIXUP` | [`c/13-linker-section-registration/`](c/13-linker-section-registration/) 启动参数自注册/PCI fixup表/通用section插件表 |
| [14](14-tracepoint.md) | Tracepoint | 一个"探测点"上的多个 probe 函数 | 静态插入代码中，probe 运行期动态挂接/摘除 | tracepoint 结构本身是静态数据，probe 链表运行期变化 | `__DO_TRACE()` 宏展开的代码，探测点被执行到时遍历 probe 链 | 是，多个 probe 可以挂同一个 tracepoint | 是，`tracepoint_probe_register`/`_unregister` | 类似 Notifier Chain 的特化，但以"近零开销"为首要设计目标 | `DECLARE_TRACE`、`kernel/tracepoint.c` | [`c/14-tracepoint/`](c/14-tracepoint/) 静态插入点模拟/多probe挂载/运行期启停性能对比 |
| [15](15-bpf-program-registration.md) | BPF Program Registration | 一段被验证过的字节码程序 + attach 点 | load->verify->attach->（执行）->detach->unload | 内核持有已加载程序的引用，attach 点持有挂接关系 | 内核在对应 hook 点执行验证过的程序 | 是，一个 hook 点可以挂多个程序 | **是**，可以动态 attach/detach 而不重启内核 | 类似 Notifier Chain，但执行体是字节码而非直接函数指针 | 现代内核的 `bpf_prog_run`/`cgroup_bpf`（**本仓库 3.2.0 不含 eBPF，本篇为概念性说明**） | [`c/15-bpf-program-registration/`](c/15-bpf-program-registration/) 玩具字节码验证器/按类型分发/hook点挂接 |
| [16](16-irq-handler.md) | IRQ Handler | 单个中断处理函数 + `dev_id` | `request_irq()`->（任意次中断触发）->`free_irq()` | `irq_desc->action` 链表 | 中断分发核心（`handle_irq_event`），硬件中断触发时盲调 | 是，`IRQF_SHARED` 支持多个 handler 共享同一条 IRQ 线 | 是 | 不算多态，是"共享中断线上的链式尝试" | `request_irq`、`struct irqaction` | [`c/16-irq-handler/`](c/16-irq-handler/) 共享中断链式分发/线程化IRQ/free_irq竞态与同步 |
| [17](17-softirq.md) | Softirq | 一个固定编号对应一个函数指针 | 编译期/启动期通过 `open_softirq()` 固定注册，几乎不再变化 | 全局数组 `softirq_vec[NR_SOFTIRQS]`（每 CPU 独立 pending 位图） | `do_softirq()`/`__do_softirq()`，在中断返回路径检查 pending 位并调用 | 否，编号固定，每个编号只有一个处理函数 | 否 | 不涉及多态 | `open_softirq`、`raise_softirq`、`NET_RX_SOFTIRQ` | [`c/17-softirq/`](c/17-softirq/) 固定编号表/硬中断两级处理/每CPU独立pending位图 |
| [18](18-tasklet.md) | Tasklet | 单个函数指针 + `data`（动态注册，但仍在软中断上下文执行） | `tasklet_init()`->`tasklet_schedule()`->（软中断上下文执行，保证同一 tasklet 不并发）->`tasklet_kill()` | 驱动自己持有 `struct tasklet_struct` | softirq 机制（`TASKLET_SOFTIRQ`）在软中断上下文调用 | 是，多个 tasklet 可以并存 | 是（动态注册/调度） | 不涉及多态 | `struct tasklet_struct`、`tasklet_schedule` | [`c/18-tasklet/`](c/18-tasklet/) tasklet调度去重/disable-enable交互/与workqueue对比 |
| [19](19-rcu-callback.md) | RCU Callback | 一个回调函数指针 `rcu_head->func` | `call_rcu()`->（等待所有 reader 经过一次 grace period）->回调执行->释放 | 通常内嵌在被保护的数据结构里（Context Injection 的应用） | RCU 核心，grace period 结束后调用 | 是，大量并发的 `call_rcu()` 请求 | 不适用（RCU 本身是机制不是可插拔模块） | 不涉及多态 | `struct rcu_head`、`call_rcu`、`synchronize_rcu` | [`c/19-rcu-callback/`](c/19-rcu-callback/) grace period模拟/无锁读替换/与传统refcount性能对比 |
| [20](20-filesystem-registration.md) | Filesystem Registration | 一整套文件系统类型定义 `file_system_type` | `register_filesystem()`->（任意次 `mount()`）->`unregister_filesystem()` | 全局链表 `file_systems`（新注册项追加到**尾部**） | VFS `mount` 路径按名字查找并调用 `mount`/`get_sb` | 是，任意多种文件系统类型可注册 | 是，文件系统模块可 `insmod`/`rmmod` | 强多态（每种文件系统是一种实现） | `struct file_system_type`、`register_filesystem` | [`c/20-filesystem-registration/`](c/20-filesystem-registration/) 注册表管理/mount分发/多模块动态注册注销 |

（共 20 行，覆盖全部 DI 手法，均已完成；`09`/`12`/`13` 分别是 `04`/`08`/`06` 在更通用层面的展开，
已在各自文档的"与其它 DI 手法比较"一节中交叉说明两者的差异，不是重复内容。）

---

## 第四部分：Kernel DI 全景图

```
                                Kernel Core
                     (VFS / 调度器 / 总线 / 中断分发 / 安全框架 ...)
                                     |
              +-----------------------+-----------------------+
              |                       |                       |
          callback               registration              broadcast
              (原子调用单元)           (对象化注册)             (一对多分发)
              |                       |                       |
   +-----+----+----+----+   +----+----+----+----+   +----+----+----+
   |     |    |    |    |   |    |    |    |    |   |    |    |    |
 timer  irq tasklet softirq RCU driver bus/dev sched fs  notifier tp LSM
 [03]  [16]  [18]   [17]  [19] [04]  [09]  [10] [20]  [05]    [14][08]
   |     |    |    |    |     |    |    |    |    |    |     |    |
   +--+--+    |    |    |     +-+--+    |    +-+--+    +--+--+     |
      |       |    |    |       |       |      |          |        |
      v       v    v    v       v       v      v          v        v
   Context  softirq 机制  RCU 核心    ops table (02)   一对多分发    一票否决
  Injection  分发机制  grace period  (强多态/vtable)   (可选广播,   (强制策略,
    (03)       |     等待读者退出    |                无短路要求)   本仓库单例)
      |        |         |          |                    |          |
      +--------+---------+----------+--------------------+----------+
                                     |
                                     v
                      +--------------+---------------+
                      |                               |
              时间延迟异步/运行期分派              编译期/链接期静态收集
              (workqueue [07])                  (initcall [06] /
                      |                          linker section [13])
                      v                               |
                runtime dispatch <----------------------+
        (最终都归结为：Core 通过一次指针跳转,
         调用一个它在编译期完全不认识的具体实现)
```

（方括号内数字为对应文档编号，全部 20 种手法均已完成，`tp` 为 Tracepoint [14] 的缩写。）

全景图的阅读方式：从上往下看，越靠上越接近"Core 主动发起调用"（callback/registration/broadcast
三大范式），越靠下越接近"最终统一的运行期分派动作"。三条主干互相交织：

- **callback 主干**：timer（03 的例子）/ irq（16）/ tasklet（18）/ softirq（17）/ RCU（19）都是
  "单一回调 + 携带上下文"的变体，都要靠 Context Injection（03）才能访问业务数据，其中 softirq/
  tasklet/RCU 进一步都是"延迟到某个安全或合适的时刻才执行"的具体形态，只是"何时安全/合适"的判定
  条件不同（softirq 是"中断返回路径"，tasklet 是"下一次软中断循环且保证不并发"，RCU 是"所有读者
  都经过一次 grace period"）。
- **registration 主干**：driver（04）/ bus-device（09）/ scheduler class（10）/ filesystem（20）
  都是"对象化注册 + 匹配/路由"的变体，内部无一例外用到 ops table（02）打包一整套操作；09 是 04
  的通用基础设施层，10 把 ops table 和 notifier chain 的"链式尝试直到有人接受"结合在一起。
- **broadcast 主干**：notifier（05）/ tracepoint（14）/ LSM hook（08）都是"一对多分发"的变体，
  区别在于"是否允许否决、是否要求近零开销"（LSM 是强制的一票否决，notifier 是可选广播，tracepoint
  是以诊断为目的、对未挂载 probe 时的开销近乎零有极致要求）。

三条主干最终都汇入同一个"runtime dispatch"节点——这正是 `00-mental-model.md` 开篇那张
总纲架构图的具体展开：**Kernel 的 DI 无论表面形态多么不同，底层动作永远是"Core 持有一个契约类型的指针，
在运行期对它做一次盲调"**。

---

## 第五部分：设计思想总结

### 解耦

所有 20 种手法的共同目标都是切断"Core 知道具体实现"这条依赖边。解耦的直接收益是：VFS 团队和
文件系统团队、调度器团队和某个新调度算法的研究者、总线维护者和驱动作者，彼此都不需要读对方的代码，
只需要读懂共享的"契约"（一个头文件里的结构体/函数指针签名）。

### 可扩展

开闭原则的具体落地：新增一种文件系统（20）、新增一个 tracepoint 探测点（14）、新增一种调度类（10）、
新增一种 IO 调度/拥塞控制策略（12），都是"纯增量"操作——写新文件、调用一个注册函数（或打一个
section 标记），不修改任何已有代码。这也是为什么 Kernel 能够在几十年里持续吸收数以千计的驱动、
文件系统、网络协议而核心架构基本稳定。

### 可测试

Ops Table（02）、Policy Injection（12，`elevator_type`/`tcp_congestion_ops` 这类互斥可切换策略）
都天然支持"用一个 mock 实现替换真实实现"进行单元测试——这正是本系列全部 60 个用户态示例能够脱离
内核环境、用普通 gcc 编译运行并断言结果的根本原因：DI 让"被测逻辑"和"具体实现"解耦到可以在用户态
复刻整个交互模式。

### 多态

C 语言没有语言级多态，Kernel 用 Ops Table（02）手工实现了等价于虚函数表的效果，`sched_class`（10）
更进一步把多个 ops table 串成链、依次尝试直到有一个接受，这些都是手写的、编译期确定布局的实现，
比语言原生虚函数表更透明、更可控（没有隐藏的运行期类型信息开销）。

### 插件化

Driver Registration（04）+ Bus/Device/Driver Object Injection（09）、Filesystem Registration（20）、
BPF Program Registration（15）都是"插件化"的具体形态：运行中的系统可以在不重启的前提下接纳全新的、
编译时未知的行为单元；BPF 更进一步在插件化之上叠加了"运行期验证"这一安全层。

### 生命周期管理

Context Injection（03）、Driver Registration 里的 refcount（04）、RCU Callback（19）都在解决同一个
根本问题：**当一个对象的"使用"和"释放"分别由不同主体控制时，如何保证不会在还有人用的时候释放、
也不会永远不释放造成泄漏**——RCU（19）给出了这个问题在"读多写少、无锁读"场景下最精巧的答案：把"释放"
推迟到"所有可能持有旧指针的读者都已经离开临界区"这一刻才发生。这是所有异步/解耦系统里最容易出错、
也最需要纪律的部分。

### 模块化

Initcall（06）+ Linker Section Registration（13）让"内置"和"可加载模块"两种构建形态共享同一套源码
约定（`module_init()`/`__setup()`），这是 Kernel 模块化构建体系的基石。

### ABI 稳定性

Ops Table（02）的字段一旦定下来，新增字段必须放在结构体末尾（否则所有二进制模块的字段偏移全部错位）；
这一约束直接决定了 Kernel 对外模块 ABI（`vmlinux` 符号版本、`struct` 布局）的演进策略——
"契约"不仅是编译期的类型检查，也是运行期二进制兼容性的基石。

### 性能

零反射、零虚拟机、零额外内存分配（`static const` ops table 被所有实例共享）——`00-mental-model.md`
第 5 节详细论证过，Kernel 的 DI 本质是"一次指针间接跳转"，与直接函数调用相比只多了 1-2 次内存读取，
现代 CPU 的间接分支预测器对这种"稳定指向同一目标"的调用模式命中率很高。Softirq（17）把这个理念推到
另一个极端：连"表"都不需要动态分配，编号在编译期固定、数组下标直接索引，是全系列 20 种手法里开销最低
的一种，代价是完全牺牲了运行期的灵活性。

### Cache Friendly

`static const struct xxx_operations` 通常很小（几十到几百字节），会被大量对象实例共享，
反复访问同一份只读数据天然具有极佳的 cache 局部性；相比每个对象各自拷贝一份函数指针数组，
共享 ops table 显著减少了内存占用和 cache 压力。

### Zero-cost Abstraction

Initcall（06）+ Linker Section Registration（13）把这个理念推到极致：**连"注册"这个动作本身的运行期
开销都消除了**——不是"运行期调用一次注册函数"，而是"编译期打一个 section 标记，链接器自动收集"；
Tracepoint（14）则展示了这个理念的另一种变体：插入点本身永远存在，但通过"未挂载 probe 时开销降到
最低"的设计，让"诊断能力"这个横切关注点几乎不侵蚀核心路径的性能。

这是 C 语言在没有任何运行时基础设施的前提下，实现"关注点分离"与"零/近零运行期代价"同时成立的终极手法，
也是全系列教程希望读者带走的最核心 mental model：
**Kernel 的每一种 DI 手法，都是在"解耦的收益"与"运行期/内存开销的代价"之间，
针对具体场景做出的精确权衡——理解了这套权衡逻辑，就能看懂 Kernel 里任何一个新的、
本教程未覆盖的注册/回调机制，并将同样的思路迁移到自己的 C 项目设计中。**
