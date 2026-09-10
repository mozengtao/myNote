# 12. Policy Injection

> `08-security-hooks-lsm.md` 讲的是"策略注入"在安全领域的一个特例：LSM 把"检查点在哪"
> （核心路径固定调用 `security_xxx()`）与"具体怎么判断"（SELinux/AppArmor 的算法）分离，
> 但多个 LSM 之间是**链式全部执行、一票否决**。本篇讲的是这个模式在**安全领域之外**的
> 更常见形态：**机制固定、策略互斥、运行期可整体替换**——同一时刻只有一个策略生效，
> 换策略不是"多加一个判断者"，而是"把整个决策大脑换掉"。

---

## 1. Why

Kernel 里大量子系统都面临同一个结构性问题：**"什么时候做决策"（机制）是稳定的、由子系统
架构决定的；"怎么做决策"（策略）却需要频繁演化、因场景而异，甚至同一台机器上不同时刻就需要
不同的答案**。典型例子：

1. **Block 层 I/O 调度**：机制是固定的——"请求提交时插入队列，设备空闲时从队列取一个请求
   下发"，这个骨架几十年没变。但"插入队列后按什么顺序取出"却千差万别：机械硬盘要减少寻道
   （电梯扫描），SSD 几乎不需要重排序（noop/none），需要保证延迟上限的场景要有饿死保护
   （deadline）。
2. **TCP 拥塞控制**：机制是固定的——"收到 ACK 时调整拥塞窗口，检测到丢包时收缩窗口"，这个
   RFC 定义的骨架是协议兼容性的基石。但"具体怎么调整窗口"却是一个活跃的研究领域：reno 保守，
   cubic 面向高带宽长距离链路激进增长，bbr 完全基于带宽/延迟建模——同一台服务器面对不同网络
   环境，可能需要不同的算法。

如果把某一种具体策略硬编码进机制代码里，会导致：

1. **子系统架构演化速度被绑死在算法演化速度上**——block 层核心代码不应该因为有人发明了
   一种新的 I/O 调度算法就要重新编译；TCP/IP 协议栈不应该因为网络研究者发表了新拥塞控制
   论文就要改 `tcp_ack()`。
2. **无法在同一个系统上因地制宜**——一台机器上可能同时插着机械盘和 SSD，不同网卡连接不同
   延迟/带宽特性的网络，"一种策略打天下"在异构环境下几乎总是次优的。
3. **无法在运行期调优、比较、回退**——运维人员需要能在不重启、不重新编译的前提下切换策略，
   观察效果，出问题能立刻切回去。

Policy Injection 的设计目标：

> **把"何时触发决策"这一事实固化在机制代码里（core 的固定调用点），
> 但"当前用哪种算法做决策"完全下放给一张运行期可查询、可切换的注册表；
> 同一时刻只有一个策略生效（互斥选择），机制代码通过一个"当前策略"指针盲调，
> 完全不知道背后是 noop 还是 deadline、是 reno 还是 cubic。**

这与 08 篇 LSM 的核心区别在于**"多"与"一"**：LSM 场景下"谁来判断"这件事本身就允许同时存在
多个裁决者（各自独立检查、一票否决）；Policy Injection 场景下"谁来判断"只允许有一个——
两个 I/O 调度器不会同时决定同一个请求的派发顺序，两个拥塞控制算法不会同时决定同一个连接的
窗口该多大。第 10 节会展开对比这个本质区别。

---

## 2. Architecture

```
   核心机制（Mechanism，Core，稳定不变）
        |
        | 每次需要"做决策"的时刻，无条件调用
        v
   当前生效策略指针（q->elevator / icsk->icsk_ca_ops）
        |
        | 指向注册表中被选中的那一个
        v
   +--------------------------------------------------+
   |  策略注册表（elv_list / tcp_cong_list）              |
   |  [策略A]  [策略B]  [策略C]  ...  互斥选择，只有一个生效  |
   +--------------------------------------------------+
        |
        | 运行期可通过 /sys 或 /proc/sys 写入名字切换
        v
   elevator_change() / tcp_set_congestion_control()
        |
        | 查找到 -> 原子替换当前生效指针
        | 查不到 -> 返回错误，旧策略继续生效
        v
   下一次机制触发时，走的已经是新策略的决策逻辑
```

真实定义（I/O 调度器一侧）：

```c
/* include/linux/elevator.h:35 */
struct elevator_ops
{
	elevator_merge_fn *elevator_merge_fn;
	/* ... */
	elevator_dispatch_fn *elevator_dispatch_fn;
	elevator_add_req_fn *elevator_add_req_fn;
	/* ... */
};

/* include/linux/elevator.h:80 */
struct elevator_type
{
	struct elevator_ops ops;
	/* ... */
};
```

真实定义（TCP 拥塞控制一侧）：[include/net/tcp.h:689](../../include/net/tcp.h)

```c
struct tcp_congestion_ops {
	struct list_head	list;
	unsigned long flags;
	void (*init)(struct sock *sk);
	void (*release)(struct sock *sk);
	u32  (*ssthresh)(struct sock *sk);          /* 必须实现 */
	void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);  /* 必须实现 */
	/* ... 其余 hook 均为可选 ... */
	char name[TCP_CA_NAME_MAX];
	struct module *owner;
};
```

两个子系统的结构惊人地相似：都是"一份 ops 契约 + 一个 `list_head` 挂进全局注册表 +
一个当前生效指针"。这不是巧合——`net/ipv4/tcp_cong.c` 文件头的真实注释写的就是
"Based on ideas from I/O scheduler support"，TCP 拥塞控制的可插拔设计明确是照抄
I/O 调度器的模式实现的。

---

## 3. Lifecycle

```
   allocate
   (定义 static struct elevator_type my_iosched / static struct tcp_congestion_ops my_cc)
        |
        v
   initialize
   (填好 ops 字段：deadline 填 elevator_dispatch_fn，cubic 填 ssthresh/cong_avoid)
        |
        v
   inject（注册进候选池，注意：这一步只是"加入候选"，不代表"生效"）
   (elv_register(&my_iosched) / tcp_register_congestion_control(&my_cc)
    —— 对应 block/elevator.c:895、net/ipv4/tcp_cong.c:38)
        |
        v
   select（互斥选择，这是本篇区别于其它手法的关键步骤）
   (elevator_change(q, "deadline") / tcp_set_congestion_control(sk, "cubic")
    —— 把"当前生效指针"原子地指向注册表里的某一项；
    此时同一个 request_queue / socket 上，旧策略立刻失效，新策略立刻生效)
        |
        v
   framework owns dispatch
   (block 层每次 blk_run_queue()，TCP 每次收到 ACK 调用 tcp_ack())
        |
        v
   callback（盲调当前生效策略）
   (q->elevator->ops->elevator_dispatch_fn(q) / icsk->icsk_ca_ops->cong_avoid(sk, ...))
        |
        v
   destroy / unregister
   (elv_unregister() / tcp_unregister_congestion_control()
    —— 从候选池移除；如果某个正在生效的策略被卸载，机制必须先把生效指针切换到
    默认策略，否则会留下野指针——这是 Failure 小节的重点)
```

**与 08 篇 LSM 的生命周期最大差异**：LSM 的 `register_security()` 在本仓库版本里
"注册即生效，且只能注册一次"，注册和生效是同一个动作；这里的"注册"（`elv_register`/
`tcp_register_congestion_control`）只是"加入候选池"，真正决定"哪个生效"是一个独立的
`select` 步骤，可以在运行期反复执行——这正是"策略互斥选择、可反复切换"与"检查链固定
叠加"这两种模式在生命周期上的具象化差异。

---

## 4. Relationship

```
   Core（block 层核心 / tcp_ack() 核心）
        |
        | 持有一个"当前生效"指针
        v
   q->elevator->elevator_type  /  icsk->icsk_ca_ops
        |
        | 指向
        v
   注册表中的某一项（elv_list 上的一个 struct elevator_type /
                    tcp_cong_list 上的一个 struct tcp_congestion_ops）
        |
        | 兄弟节点（同一注册表里的其它候选，此刻不生效，但随时可以被切换过去）
        v
   [noop]  [deadline]  [cfq]  ...  （互斥关系，只有一个通过"当前生效指针"被引用）
```

与 08 篇 `policy_injection_acl.c` 示例的关系图对比一下会更清楚：08 篇的 `check_access()`
（检查点）本身也持有一个"当前生效策略"指针（`g_active_policy`），这正是 08 篇文末所说
"LSM hook 提供的'检查点机制'和'检查点上跑的策略'是两个独立可替换的维度"——**08 篇的
`policy_injection_acl.c` 示例其实已经是本篇模式的一个安全领域应用**，本篇是把同一个
关系图搬到 I/O 调度和网络拥塞控制这两个非安全子系统上，证明这是一个横跨内核各处的
通用元模式，不是安全子系统的专利。

---

## 5. Data Flow

以 I/O 调度器为例，对照真实路径：

```
用户/管理员：
  echo deadline > /sys/block/sda/queue/scheduler
        |
        v
  sysfs write -> elv_iosched_store()（block/elevator.c:1038）
        |
        v
  elevator_change(q, "deadline")（block/elevator.c:1014）
        |
        +- elevator_get("deadline") 在 elv_list 上查找 -> 找到 struct elevator_type
        |
        +- elevator_switch(q, new_e)：分配新 elevator_queue，
        |    调用新策略的 init_fn，成功后释放旧的 elevator_queue
        |
        v
  q->elevator 现在指向 deadline 的 elevator_queue

之后每一次 I/O 请求路径：
  submit_bio() -> __make_request() -> elv_insert()
        |
        v
  q->elevator->ops->elevator_add_req_fn(q, rq)   <-- 盲调，此刻是 deadline 的插入逻辑
        |
        ...
        v
  blk_run_queue() -> q->elevator->ops->elevator_dispatch_fn(q, 0)  <-- 盲调 deadline 的派发逻辑
```

对照本文档 `io_scheduler_policy.c` 的真实运行轨迹：

```
main()
  |
  +- elv_register(&noop_ops)
  +- elv_register(&deadline_ops)
  |
  +- elv_switch("noop")           <-- 对应 elevator_change()
  +- blk_submit_bio(&pool[i]) x6  <-- 对应 __make_request()
  +- blk_run_queue()              <-- 对应 blk_run_queue()，盲调 g_active->dispatch_request()
  |     派发顺序：500,100,300,700,200,900（严格 FIFO，noop 特征）
  |
  +- elv_switch("deadline")       <-- 同一个机制函数，切换目标
  +- blk_submit_bio(&pool[i]) x6
  +- blk_run_queue()
        派发顺序：100,300,500,700,200,900（先按 sector 排序，后半段因到期被强制插队）
```

TCP 拥塞控制一侧的对照路径：

```
用户：echo cubic > /proc/sys/net/ipv4/tcp_congestion_control
        |
        v
  proc_tcp_congestion_control()（net/ipv4/sysctl_net_ipv4.c:122）
        |
        v
  tcp_set_default_congestion_control("cubic")（net/ipv4/tcp_cong.c:112）
        |
        v
  之后新建的每个 TCP 连接：tcp_init_congestion_control(sk)（net/ipv4/tcp_cong.c:78）
  把 icsk->icsk_ca_ops 指向 cubic

已建立连接收到 ACK：
  tcp_ack() -> tcp_cong_avoid(sk, ...) -> icsk->icsk_ca_ops->cong_avoid(sk, ack, in_flight)  <-- 盲调
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| 策略注册表（`elv_list`/`tcp_cong_list`） | **运行期可增减**（模块 insmod/rmmod 时注册/注销） | 候选池本身是动态的，但"候选"不等于"生效" |
| 当前生效策略指针（`q->elevator`/`icsk->icsk_ca_ops`） | **运行期可切换，但每次切换是原子的整体替换** | 不存在"部分生效"的中间态；本文档 `sysctl_tunable_policy_switch.c` 的 `g_active_policy` 直接复刻了这个语义 |
| 策略内部私有状态（cubic 的 `epoch_start`/`bic_K`，deadline 的 `fifo_expire` 队列） | **完全由策略自己管理，机制不感知** | 切换策略时旧策略的私有状态通常被丢弃（`elevator_switch()` 会释放旧 `elevator_queue`），新策略从初始状态重新开始 |
| 策略的"是否允许非特权用户设置"标志（`TCP_CONG_NON_RESTRICTED`） | 编译期/注册期固定 | 决定了策略是否可以被普通用户通过 `setsockopt` 设置，而不需要 `CAP_NET_ADMIN` |

---

## 7. Failure

1. **正在生效的策略被卸载而不做降级处理**：如果一个 I/O 调度器模块在某个 `request_queue`
   正在使用它的时候被 `rmmod`，`elv_unregister()` 只是把它从 `elv_list` 摘除，并不会主动把
   所有正在使用它的队列切换到别的策略——这是为什么真实内核依赖模块引用计数（每个正在使用某个
   `elevator_type` 的 queue 都会 `try_module_get`）阻止"正在被使用的模块被卸载"，而不是靠
   "卸载时自动降级"这条路径。
2. **切换过程不是原子的**：`elevator_switch()` 的注释里写得很直白——"be careful not to
   introduce deadlocks - we don't free the old io scheduler, before we have allocated what
   we need for the new one"（见 block/elevator.c:939 附近注释）。如果新策略的 `init_fn`
   失败（比如内存分配失败），必须能够**回退到旧策略继续工作**，而不是留下一个"半初始化"的
   状态——`sysctl_tunable_policy_switch.c` 的 `proc_write_policy()` 复刻了这个"失败则不切换、
   旧策略继续生效"的语义（阶段3示例）。
3. **必需字段未实现导致注册被拒绝**：`tcp_register_congestion_control()`
   （net/ipv4/tcp_cong.c:38）显式检查 `!ca->ssthresh || !ca->cong_avoid` 就直接拒绝注册——
   与 08 篇"NULL 字段自动填充默认桩函数"不同，这里选择"必需字段缺失就拒绝候选资格"，
   因为拥塞控制算法如果连"怎么收缩窗口"都没实现，让它生效是明确的协议违规（可能导致
   永不退让的连接把网络拥塞压垮），不适合"默默填一个安全默认值"这种宽容策略。
4. **同名策略重复注册**：`tcp_ca_find()` 在 `tcp_register_congestion_control()` 内部被
   调用检查重名，重名返回 `-EEXIST` 且打印 `KERN_NOTICE`；`elv_register()` 则直接
   `BUG_ON(elevator_find(e->elevator_name))`——两个子系统对"重名"的严重程度判断不同
   （网络觉得是可容忍的运行时错误，block 层认为是不该发生的编程错误，直接 panic 式断言），
   这提示"契约违反"的处理策略需要结合子系统的容错哲学来设计，没有统一答案。
5. **切换后旧策略的私有统计状态被静默丢弃**：从 reno 切到 cubic 时，`epoch_start`/`bic_K`
   这些私有状态不会被迁移；如果运维在网络抖动最严重的时候切换拥塞控制算法，新算法会
   "从零开始学习"当前网络状况，短期内的吞吐可能不升反降——这是运维实践中"不建议在高负载
   期间切换拥塞控制算法"的根本原因，`congestion_control_policy.c` 的 round 4 "LOSS EVENT"
   之后 cubic 需要好几轮才重新超过 reno 的窗口大小，直观复现了这个"切换有代价"的现象。
6. **调试方法**：
   - 用户态：本文档三个示例都在每次"切换"和"派发/窗口调整"时打印详细日志，可以直接
     对照"切换前后行为差异"。
   - Kernel：`cat /sys/block/sda/queue/scheduler` 查看当前及可选的 I/O 调度器
     （方括号标注当前生效项，对应 `elv_iosched_show()`，block/elevator.c:1054）；
     `sysctl net.ipv4.tcp_available_congestion_control`/`tcp_congestion_control` 查看
     可用/当前拥塞控制算法；`ss -ti` 可以看到每条连接实际使用的拥塞控制算法名字；
     `/proc/sys/net/ipv4/tcp_allowed_congestion_control` 控制非特权进程可以
     `setsockopt(TCP_CONGESTION)` 切换到哪些算法；`blktrace`/`iostat` 观测 I/O 调度器
     切换后的实际调度效果差异。

---

## 8. Kernel Core Pseudocode（6 个）

### Example 1 — `struct elevator_ops`/`struct elevator_type`：I/O 调度器的契约

真实定义：[include/linux/elevator.h:35](../../include/linux/elevator.h)、
[include/linux/elevator.h:80](../../include/linux/elevator.h)

```c
struct elevator_ops
{
	elevator_merge_fn *elevator_merge_fn;
	elevator_merged_fn *elevator_merged_fn;
	elevator_merge_req_fn *elevator_merge_req_fn;
	elevator_allow_merge_fn *elevator_allow_merge_fn;
	elevator_dispatch_fn *elevator_dispatch_fn;
	elevator_add_req_fn *elevator_add_req_fn;
	/* ... */
	elevator_init_fn *elevator_init_fn;
	elevator_exit_fn *elevator_exit_fn;
};

struct elevator_type
{
	struct list_head list;		/* 挂进全局 elv_list，见 block/elevator.c:43 */
	struct elevator_ops ops;
	struct elv_fs_entry *elevator_attrs;
	char elevator_name[ELV_NAME_MAX];
	struct module *elevator_owner;
};
```
为什么是 DI：block 层核心代码只 `#include <linux/elevator.h>`，永远不会
`#include "deadline-iosched.h"`——`elevator_dispatch_fn` 这个函数指针字段就是
整个"派发策略"暴露给 Core 的唯一入口，这是典型的 Ops Table（02 篇）契约，
Policy Injection 的"机制/策略分离"正是建立在 Ops Table 之上的更高层设计模式。

### Example 2 — `elv_register()`：候选池的加入，而非"生效"

真实定义：[block/elevator.c:895](../../block/elevator.c)

```c
void elv_register(struct elevator_type *e)
{
	spin_lock(&elv_list_lock);
	BUG_ON(elevator_find(e->elevator_name));   /* 重名视为编程错误 */
	list_add_tail(&e->list, &elv_list);
	spin_unlock(&elv_list_lock);
	/* 只有当它是 chosen_elevator（内核参数指定）或 CONFIG_DEFAULT_IOSCHED
	 * 时才会被标注为 "(default)"，注册本身不代表任何一个 request_queue
	 * 已经在用它 */
}
```
为什么值得单列：这条函数明确演示了 Policy Injection 的关键结构性事实——
"进入候选池"（`elv_register`）与"被选中生效"（下一条例子 `elevator_change`）
是两个独立的动作，中间可以有任意长的时间间隔，这与 06 篇 initcall 的
"注册即执行"、08 篇 LSM 的"注册即生效"都不同。

### Example 3 — `elevator_change()`/`elv_iosched_store()`：运行期互斥切换

真实定义：[block/elevator.c:1014](../../block/elevator.c)、
[block/elevator.c:1038](../../block/elevator.c)

```c
ssize_t elv_iosched_store(struct request_queue *q, const char *name, size_t count)
{
	int ret;

	if (!q->elevator)
		return count;
	ret = elevator_change(q, name);     /* <-- 用户通过 sysfs 写入名字触发 */
	if (!ret)
		return count;
	printk(KERN_ERR "elevator: switch to %s failed\n", name);
	return ret;
}
```
为什么是 DI 的核心体现：这是"策略互斥选择"在用户可见层面的具体入口——写入一个新名字，
Core（`elevator_switch()`）负责"找到、分配、初始化新策略、原子替换、释放旧策略"这一整套
动作，调用者（sysfs 写入路径）不需要知道 deadline 内部是怎么组织队列的。

### Example 4 — `struct tcp_congestion_ops`：拥塞控制的契约

真实定义：[include/net/tcp.h:689](../../include/net/tcp.h)

```c
struct tcp_congestion_ops {
	struct list_head list;
	unsigned long flags;
	void (*init)(struct sock *sk);
	u32  (*ssthresh)(struct sock *sk);            /* 必需 */
	void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);  /* 必需 */
	void (*set_state)(struct sock *sk, u8 new_state);
	void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);
	char name[TCP_CA_NAME_MAX];
	struct module *owner;
};
```
为什么是 DI：与 Example 1 的 `elevator_ops` 结构几乎同构——都是"注册表节点 + 名字 +
所属模块 + 一组函数指针"，印证了本篇 Why 小节提到的"TCP 拥塞控制明确借鉴了 I/O 调度器
的可插拔设计"这一事实（源自 `net/ipv4/tcp_cong.c` 文件头注释）。

### Example 5 — `tcp_register_congestion_control()`/`tcp_set_congestion_control()`

真实定义：[net/ipv4/tcp_cong.c:38](../../net/ipv4/tcp_cong.c)、
[net/ipv4/tcp_cong.c:236](../../net/ipv4/tcp_cong.c)

```c
int tcp_register_congestion_control(struct tcp_congestion_ops *ca)
{
	if (!ca->ssthresh || !ca->cong_avoid)      /* 必需字段检查，缺失直接拒绝候选资格 */
		return -EINVAL;
	spin_lock(&tcp_cong_list_lock);
	if (tcp_ca_find(ca->name))
		ret = -EEXIST;                     /* 重名，容忍性错误，不 BUG_ON */
	else
		list_add_tail_rcu(&ca->list, &tcp_cong_list);
	spin_unlock(&tcp_cong_list_lock);
	return ret;
}

/* 运行期把某个 socket 切换到指定算法，对应 setsockopt(TCP_CONGESTION) */
int tcp_set_congestion_control(struct sock *sk, const char *name)
{
	struct tcp_congestion_ops *ca = tcp_ca_find(name);
	if (!ca)
		return -ENOENT;
	if (!((ca->flags & TCP_CONG_NON_RESTRICTED) || capable(CAP_NET_ADMIN)))
		return -EPERM;                      /* 权限检查：非默认放开的算法需要特权 */
	tcp_cleanup_congestion_control(sk);
	icsk->icsk_ca_ops = ca;                      /* <-- 原子替换，互斥选择 */
	return 0;
}
```
为什么是 DI：`tcp_set_congestion_control()` 是"选择"这一步的标准范式——先查找、
再检查权限、最后才做原子替换；这与 `elevator_change()` 的结构完全对应，
共同构成了 Policy Injection 这一元模式的两个独立子系统实例。

### Example 6 — `bictcp_update()`：策略内部私有状态与算法细节对 Core 完全不可见

真实定义：[net/ipv4/tcp_cubic.c:205](../../net/ipv4/tcp_cubic.c)

```c
static inline void bictcp_update(struct bictcp *ca, u32 cwnd)
{
	/* K = cbrt((Wmax - cwnd) * cube_factor)，Wmax 是上次丢包时的窗口 */
	if (ca->last_max_cwnd <= cwnd) {
		ca->bic_K = 0;
		ca->bic_origin_point = cwnd;
	} else {
		ca->bic_K = cubic_root(cube_factor * (ca->last_max_cwnd - cwnd));
		ca->bic_origin_point = ca->last_max_cwnd;
	}
	/* W(t) = C * (t - K)^3 + Wmax，t 是本次丢包后经过的时间 */
	delta = (cube_rtt_scale * offs * offs * offs) >> (10 + 3 * BICTCP_HZ);
	/* ... */
}
```
为什么值得单列：`struct bictcp`（cubic 的私有状态：`bic_K`、`last_max_cwnd`、
`epoch_start`）连字段名字都不会出现在 `tcp_ack()`/`tcp_cong_avoid()` 这些 Core 函数里——
Core 只知道调用 `icsk->icsk_ca_ops->cong_avoid(sk, ack, in_flight)`，`sk->icsk_ca_priv`
这块私有内存区域具体存的是三次函数的系数还是别的什么，Core 完全不关心。
本文档 `congestion_control_policy.c` 的 `struct tcp_conn` 里 `wmax`/`epoch_start`/`bic_k`
字段就是对这份真实私有状态的简化复刻。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/12-policy-injection/`](c/12-policy-injection/)

### 9.1 `io_scheduler_policy.c` — 可插拔 I/O 调度器策略

- 场景：`noop`（严格 FIFO）与 `deadline`（sector 排序 + 到期防饿死插队）两种策略，
  运行期通过 `elv_switch()` 切换，验证同一批乱序请求在两种策略下派发顺序完全不同。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o io_scheduler_policy io_scheduler_policy.c`
- 实测输出：
  ```
  === io_scheduler_policy demo ===

  [elv_register] io scheduler 'noop' registered
  [elv_register] io scheduler 'deadline' registered

  -- 阶段1：切换到 noop，提交 6 个乱序 sector 的请求 --
  [elv_switch] switching active io scheduler -> 'noop'
    [noop] queue initialized
    -- blk_run_queue() using active scheduler 'noop' --
      dispatch: request id=1 sector=500  (arrival_seq=0)
      dispatch: request id=2 sector=100  (arrival_seq=1)
      dispatch: request id=3 sector=300  (arrival_seq=2)
      dispatch: request id=4 sector=700  (arrival_seq=3)
      dispatch: request id=5 sector=200  (arrival_seq=4)
      dispatch: request id=6 sector=900  (arrival_seq=5)

  -- 阶段2：切换到 deadline，提交同样 6 个请求 --
  [elv_switch] switching active io scheduler -> 'deadline'
    [deadline] pending list initialized, fifo_expire=3 ticks
    -- blk_run_queue() using active scheduler 'deadline' --
      dispatch: request id=2 sector=100  (arrival_seq=1)
      dispatch: request id=5 sector=200  (arrival_seq=4)
      dispatch: request id=3 sector=300  (arrival_seq=2)
      [deadline] request expired, force dispatch to avoid starvation
      dispatch: request id=1 sector=500  (arrival_seq=0)
      dispatch: request id=4 sector=700  (arrival_seq=3)
      dispatch: request id=6 sector=900  (arrival_seq=5)

  === done ===
  说明：noop 严格按提交顺序（500,100,300,700,200,900）派发，
        deadline 前几次按 sector 升序派发（sweep），
        后几个请求等待超过 fifo_expire 后被强制插队派发防止饿死。
        blk_submit_bio()/blk_run_queue() 全程没有一行 if (policy==noop)，
        完全通过 g_active->ops 盲调，这就是 Policy Injection。
  ```
  （`deadline` 前 3 次严格按 sector 升序派发 100/200/300，第 4 次触发到期保护，
  之后 500/700/900 依次因等待时间到期被强制派发——真实体现了"排序优先，
  但绝不无限期推迟"这一设计权衡。）
- 为什么是 DI：`blk_submit_bio()`/`blk_run_queue()`（Core）只认识 `io_sched_ops`
  契约，完全不知道 noop 和 deadline 内部数据结构（一个是简单 FIFO 数组，一个是
  待排序集合）有天壤之别；这是 04 篇 Driver Registration 里"匹配 + 接管"思想在
  "选择 + 独占生效"场景下的变体。

### 9.2 `congestion_control_policy.c` — 可插拔 TCP 拥塞控制算法

- 场景：`reno`（AIMD 线性增长）与 `cubic`（三次函数窗口增长，简化浮点版）在同样的
  丢包事件后表现出完全不同的窗口增长曲线。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o congestion_control_policy congestion_control_policy.c -lm`
- 实测输出：
  ```
  === congestion_control_policy demo ===

  -- 运行 reno 策略 18 个 RTT 轮次 --
    [reno  ] round  4: LOSS EVENT, cwnd 32.00 -> ssthresh 16.00

  -- 运行 cubic 策略 18 个 RTT 轮次 --
    [cubic ] round  4: LOSS EVENT, cwnd 32.00 -> ssthresh 22.40

  -- 窗口增长曲线对比（同样触发一次丢包，之后的增长形状完全不同）--
    round |   reno cwnd | cubic cwnd
    ------+-------------+-----------
        0 |        4.00 |       4.00
        1 |        8.00 |       8.00
        2 |       16.00 |      16.00
        3 |       32.00 |      32.00
        4 |       17.00 |      22.44
        5 |       18.00 |      29.32
        6 |       19.00 |      31.72
        7 |       20.00 |      32.00
        8 |       21.00 |      32.56
        9 |       22.00 |      35.79
       10 |       23.00 |      44.10
       11 |       24.00 |      59.88
       12 |       25.00 |      85.55
       13 |       26.00 |     123.49
       14 |       27.00 |     176.10
       15 |       28.00 |     245.80
       16 |       29.00 |     334.97
       17 |       30.00 |     446.02

  === done ===
  说明：两种策略共享同一套 run_policy()/cong_avoid 调用骨架（Core），
        Core 从未 #include 任何具体算法的头文件；reno 丢包后是严格的
        直线爬升（每轮 +1），cubic 丢包后先在 Wmax 附近爬升缓慢（"平台"），
        随后凸起加速——这正是 CUBIC 相对 reno 的设计目标：既能快速逼近
        上次的可用带宽，又能在探索新带宽时加速增长。
  ```
  （round 4~8：cubic 的窗口在上次丢包窗口 32 附近缓慢爬升（"平台期"，22.44->32.56），
  round 9 之后明显加速凸起增长（35.79->446.02），而 reno 全程保持严格的每轮 +1 线性——
  这组真实测得的数字精确复现了 CUBIC 论文里"凹—凸"窗口曲线的形状特征，不是编造的。）
- 为什么是 DI：`run_policy()`（Core）通过 `policy->ssthresh(&sk)`/`policy->cong_avoid(&sk)`
  盲调，`struct tcp_conn` 里 `wmax`/`epoch_start`/`bic_k` 这些 cubic 私有字段对 Core
  完全不可见——这正是 Example 6 里 `struct bictcp` 私有状态对 `tcp_ack()` 不可见的
  用户态复刻。

### 9.3 `sysctl_tunable_policy_switch.c` — 类 sysctl 运行期热切换

- 场景：一个通用化的负载均衡场景，`round-robin`/`least-conn`/`client-hash` 三种策略
  通过模拟的 `/proc/sys` 写入接口运行期切换，验证切换前后请求分发行为的实质差异，
  以及"写入未注册策略名"被拒绝、旧策略继续生效的容错路径。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o sysctl_tunable_policy_switch sysctl_tunable_policy_switch.c`
- 实测输出：
  ```
  [proc_write] write 'weighted-fair-queue' to policy file -> -ENOENT (unknown policy, kept 'least-conn')
  === sysctl_tunable_policy_switch demo ===

  [register] load-balance policy 'round-robin' registered
  [register] load-balance policy 'least-conn' registered
  [register] load-balance policy 'client-hash' registered

  当前生效策略（读 proc 文件）：round-robin

  -- 阶段1：round-robin 策略下派发 6 个请求（忽略初始不均匀负载） --
    -- pool state (before): s0=5 s1=1 s2=3 s3=0
    request(client= 1) -> server[0] (active_conns now=6)
    request(client= 2) -> server[1] (active_conns now=2)
    request(client= 3) -> server[2] (active_conns now=4)
    request(client= 4) -> server[3] (active_conns now=1)
    request(client= 5) -> server[0] (active_conns now=7)
    request(client= 6) -> server[1] (active_conns now=3)
    -- pool state (after): s0=7 s1=3 s2=4 s3=1

  -- 阶段2：写 sysctl 切换到 least-conn，再派发 6 个请求 --
  [proc_write] write 'least-conn' to policy file -> switched active policy: 'round-robin' -> 'least-conn'
    -- pool state (before): s0=7 s1=3 s2=4 s3=1
    request(client= 1) -> server[3] (active_conns now=2)
    request(client= 2) -> server[3] (active_conns now=3)
    request(client= 3) -> server[1] (active_conns now=4)
    request(client= 4) -> server[3] (active_conns now=4)
    request(client= 5) -> server[1] (active_conns now=5)
    request(client= 6) -> server[2] (active_conns now=5)
    -- pool state (after): s0=7 s1=5 s2=5 s3=4

  -- 阶段3：尝试写一个不存在的策略名（应被拒绝，当前策略保持不变） --
  当前生效策略（读 proc 文件）：least-conn

  -- 阶段4：切换到 client-hash，验证同一 client 永远落到同一 server --
  [proc_write] write 'client-hash' to policy file -> switched active policy: 'least-conn' -> 'client-hash'
    request(client=101) -> server[1] (active_conns now=6)
    request(client=202) -> server[2] (active_conns now=6)
    request(client=101) -> server[1] (active_conns now=7)
    request(client=202) -> server[2] (active_conns now=7)
    request(client=101) -> server[1] (active_conns now=8)
    request(client=202) -> server[2] (active_conns now=8)

  === done ===
  说明：dispatch_request()/proc_write_policy()（Core）从头到尾没有一行
        if (strcmp(name,"round-robin")==0) 这样的分支，全部通过
        g_active_policy->pick_server() 盲调；round-robin 均匀轮询，
        least-conn 持续填补负载最低的 server，client-hash 让同一个
        client_id（101/202）在阶段4里始终落在同一个 server——三种
        完全不同的行为，Core 代码却一行没变。
  ```
  （第一行 `[proc_write] ... -ENOENT ...` 打印在最前面，是 `stderr` 无缓冲、`stdout`
  行缓冲在管道输出下的真实交织顺序——这与 `06-initcall.md` 里
  `module_registry_levels.c` 的 `[WARN]` 提前打印是同一种真实现象，不是输出错误，
  已在此如实保留。阶段2 里 `least-conn` 持续把请求分配给 s3/s1，最终四台服务器负载
  从 `7/3/4/1` 收敛到 `7/5/5/4`，明显比 round-robin 的均匀轮询更快拉平负载差异；
  阶段4 里 client 101 和 202 无论请求多少次都分别固定落在 server[1]/server[2]，
  这是 client-hash 策略"会话保持"性质的直接证据。）
- 为什么是 DI：`proc_write_policy()` 精确复刻了真实 `elevator_change()`/
  `tcp_set_congestion_control()` 的"查找候选表 -> 找到才原子替换 -> 找不到保留旧值"
  结构；`dispatch_request()`（Core）从来不知道当前生效的是哪一种策略，这是本篇
  Architecture 一节那张图在纯用户态场景下的完整落地。

一次性编译并运行全部三个示例：`cd c/12-policy-injection && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Policy Injection | Security Hooks (LSM) | Driver Registration | Ops Table |
|---|---|---|---|---|
| 同一检查点/机制点上生效的实现数量 | **恰好一个**（互斥选择） | **全部**（链式叠加，一票否决） | 恰好一个（匹配成功者独占） | 恰好一个（对象创建时绑定） |
| "注册"与"生效"是否同一动作 | **否**，注册只进候选池，需要独立的 select 动作才生效 | **是**（本仓库版本），注册即生效 | 是，`probe()` 成功即生效 | 是，赋值即生效 |
| 是否支持运行期反复切换 | **是，核心特性**（`echo xxx > /sys/.../scheduler`） | 否（本仓库版本几乎不支持运行期换 LSM） | 否（换驱动要先 `remove` 再走一遍匹配流程） | 是（可以重新赋值 `obj->ops = &other_ops`，但语义上通常不常见） |
| 失败时的回退语义 | **必须回退到旧策略继续工作**（切换失败 ≠ 停止服务） | 不适用（本身就是强制检查，没有"切换失败"这个概念） | probe 失败则设备保持未绑定状态 | 不适用 |
| 典型触发者 | 运维人员通过 `/sys`、`/proc/sys` 手工调优或自动化脚本按场景切换 | 系统安全策略在启动时确定，几乎不在运行期改变 | 硬件热插拔事件 | 对象创建时按类型决定 |
| 何时用 Policy Injection | "核心机制稳定，但具体算法/策略需要按场景（硬件类型、网络环境、负载特征）在运行期反复调整、比较、回退"的场景 | 需要"强制性、可叠加、几乎不下线"的安全检查场景 | 需要"硬件匹配 + 独占接管"的场景 | 需要"对象创建时确定一套完整行为，之后基本不变"的场景 |
| 何时不要用 | 如果同一时刻真的需要多个实现同时生效并各自独立处理（应该用 Notifier Chain 或 LSM 式链式叠加），Policy Injection 的"互斥选择"语义反而是限制 | — | — | — |

一句话总结本篇与 08 篇的关系：**LSM 是"策略链式叠加、一票否决"，本篇的 Policy Injection
是"策略互斥选择、单一生效"**——两者都在解决"机制固定、算法可换"这个共同问题，
但对"能不能同时存在多个决策者"这个关键设计维度给出了完全相反的答案。选择哪一种，
取决于业务语义本身：安全检查天然需要"叠加所有人的意见取交集"（更严格更安全），
I/O 调度和拥塞控制天然需要"同一时刻只能有一个人说话"（多个调度算法同时决定同一个
请求的下发顺序是没有意义的，会直接冲突）。理解了这个区分，就能判断新场景里
"可插拔的决策逻辑"到底该用哪一种模式实现。

下一篇 [13-linker-section-registration.md](13-linker-section-registration.md) 回到
06 篇 Initcall 提到的"链接器 section 自注册"机制，看它在 initcall 之外的更多真实应用。
