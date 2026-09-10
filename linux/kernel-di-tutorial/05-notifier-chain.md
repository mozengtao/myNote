# 05. Notifier Chain

> 04 篇讲的是"精确匹配一个实现"（driver registration）。本篇讲相反的场景：
> **一个事件发生，Core 不知道、也不关心有多少个订阅者，必须能广播给所有人。**

---

## 1. Why

考虑一个真实场景：CPU 要下线（`CPU_DOWN_PREPARE`）。至少下面这些子系统都需要知道并做出反应：

- 调度器：把该 CPU 上的任务迁走
- perf 子系统：停止该 CPU 的性能计数器
- RCU：处理该 CPU 的 RCU callback
- workqueue：迁移该 CPU 上未完成的 work
- ……未来可能还会有更多子系统关心这个事件

如果用 04 篇的"driver registration"模式（一个事件精确路由给一个实现），根本无法表达"广播给所有关心的人"。
如果不用 Notifier Chain，唯一的替代方案是在 CPU 下线代码里手写：

```c
void cpu_down_prepare(int cpu)
{
	sched_cpu_down_prepare(cpu);
	perf_cpu_down_prepare(cpu);
	rcu_cpu_down_prepare(cpu);
	workqueue_cpu_down_prepare(cpu);
	/* 每加一个新子系统都要回来改这里，core 与所有子系统紧耦合 */
}
```

Notifier Chain 的设计目标：

> **把"广播"本身做成一个通用机制：任意数量的订阅者可以在任意时刻挂上/摘下，
> Core 触发事件时只管遍历链表逐一调用，完全不知道链上有谁、有几个。**

同时还要解决两个 01 篇 Callback 不需要考虑的新问题：

1. **顺序**：多个订阅者响应同一事件，谁先谁后有时候很关键（比如"安全校验"必须比"业务处理"先跑）——
   用 `priority` 字段解决。
2. **短路**：某个订阅者可以"拒绝"这个事件并要求终止后续传播——用返回值里的 `NOTIFY_STOP_MASK` 解决。

---

## 2. Architecture

```
   事件源（Core：CPU hotplug / 网络设备状态变化 / OOM ...）
        |
        | notifier_call_chain(head, action, data)
        v
   +----------------------------------------------------+
   |              notifier_head（链表头）                  |
   |                                                      |
   |  nb1(priority=1000) -> nb2(priority=100) -> nb3(...) |   <-- 按 priority 排序的链表
   +----------------------------------------------------+
        |  逐一调用
        v
   nb->notifier_call(nb, action, data)
        |
        | 返回值 & NOTIFY_STOP_MASK ?
        v
   是 -> 停止遍历，立即返回      否 -> 继续下一个
```

真实定义：[include/linux/notifier.h:50](../../include/linux/notifier.h)

```c
struct notifier_block {
	int (*notifier_call)(struct notifier_block *, unsigned long, void *);
	struct notifier_block __rcu *next;
	int priority;
};
```

Kernel 里四种变体（本仓库 `kernel/notifier.c` 全部实现），区别仅在"注册/调用期的同步机制"：

```
atomic_notifier_chain    -> spinlock 保护，回调运行在原子上下文（不能睡眠）
blocking_notifier_chain  -> rwsem 保护，回调运行在进程上下文（可以睡眠）
raw_notifier_chain       -> 无锁，调用者自己负责同步
srcu_notifier_chain      -> SRCU 保护，读端几乎零开销，适合极热路径
```

---

## 3. Lifecycle

```
   allocate
   (定义 static struct notifier_block my_nb = { .notifier_call = my_fn, .priority = N })
        |
        v
   initialize
   (填好 notifier_call 和 priority)
        |
        v
   inject（注册）
   (xxx_notifier_chain_register(&head, &my_nb) —— 按 priority 插入链表)
        |
        v
   framework owns dispatch
   (Core 完全不主动"通知"任何一个订阅者，只在事件发生时遍历链表)
        |
        v
   callback（广播时被调用）
   (my_fn(&my_nb, action, data) —— 可能被调用零次、一次、无数次，取决于事件发生频率)
        |
        v
   destroy（注销）
   (xxx_notifier_chain_unregister(&head, &my_nb) —— 从链表摘除，防止悬挂调用)
```

---

## 4. Relationship

```
   notifier_head（Core 持有）
        |
        | 链表头指向
        v
   notifier_block A --next--> notifier_block B --next--> notifier_block C --> NULL
        |                          |                          |
        | 通常内嵌于               |                          |
        v                          v                          v
   子系统 A 的私有结构体      子系统 B 的私有结构体       子系统 C 的私有结构体
   （Context Injection，见 03 篇）
```

---

## 5. Data Flow

以 `notifier_chain_impl.c` 为例：

```
main()
  |
  +- notifier_chain_register(&head, &perf_nb)    // priority=50，乱序注册
  +- notifier_chain_register(&head, &guard_nb)    // priority=1000
  +- notifier_chain_register(&head, &sched_nb)    // priority=100
        |
        |  registration 内部按 priority 重新排序，链变成：
        |  guard(1000) -> sched(100) -> perf(50)
        v
  notifier_call_chain(&head, CPU_ONLINE, &cpu=99)
        |
        v
  guard_cpu_notifier(...)  返回 NOTIFY_BAD (0x8002，含 STOP_MASK)
        |
        | ret & NOTIFY_STOP_MASK != 0
        v
  立即停止，sched_cpu_notifier / perf_cpu_notifier 完全不会被调用
```

对照真实 Kernel（CPU hotplug，`kernel/cpu.c` 风格调用 `kernel/notifier.c` 的机制）：

```
_cpu_down(cpu)
  |
  v
cpu_notify(CPU_DOWN_PREPARE, hcpu)
  |
  v
__raw_notifier_call_chain(&cpu_chain, CPU_DOWN_PREPARE, hcpu, -1, NULL)
  |
  v
逐一调用链上每个 notifier_call —— sched/perf/rcu/workqueue 各自的回调
  |
  | 任意一个返回 NOTIFY_BAD
  v
_cpu_down() 得知失败，中止下线流程（"一票否决"机制）
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| 链表本身（谁在链上） | **运行期高度动态** | 任意时刻可以注册/注销，这是与 Ops Table（创建时固定）最大的区别 |
| `priority` 字段 | 注册时确定，一般不再改 | 决定链上位置，间接决定响应顺序 |
| `data` 参数指向的数据 | **每次广播都可能不同** | 由触发广播的一方（事件源）临时构造，通常是栈上局部变量的地址 |
| 广播返回值的语义（`NOTIFY_STOP_MASK`） | 每次调用独立判断 | 不是持久状态，是"这一次广播"的临时控制流决策 |

---

## 7. Failure

1. **注册顺序 vs 优先级混淆**：`notifier_chain_impl.c` 特意乱序注册（perf -> guard -> sched），
   验证真正决定调用顺序的是 `priority` 字段而非注册顺序——如果不理解这一点，容易写出"以为先注册的先执行"
   的错误代码。
2. **STOP 语义被滥用或被忽略**：如果某个 notifier 不恰当地返回 `NOTIFY_STOP_MASK`，会"误杀"链上后续
   本应执行的订阅者（本文 guard 示例故意演示了这个"一票拒绝"场景，用来说明这既是特性也是风险点）。
3. **回调里注册/注销自身或其它节点**：在遍历链表的过程中修改链表结构（尤其是无锁的 `raw_notifier_chain`）
   会导致遍历指针失效——真实 Kernel 靠 RCU（`notifier_block __rcu *next`）来允许"读的时候可能有并发的写"，
   但要求写端遵守 RCU 更新协议（`rcu_assign_pointer` + 延迟释放）。
4. **忘记注销导致悬挂调用**：模块被卸载但忘了 `unregister_xxx_notifier()`，之后事件广播时链表遍历会跳到
   已经被释放/被其它数据覆盖的内存——这是 Kernel 里"模块卸载必须清理所有已注册的 notifier"这条铁律的来源。
5. **多个 notifier 之间的隐式顺序依赖被打破**：如果 A 的处理逻辑隐含假设"B 已经先跑过"，
   一旦优先级数值被后来的人不小心改动，顺序反转会引入极难调试的 bug——这是 notifier chain
   相比"精确一对一调用"所付出的额外复杂度代价。
6. **调试方法**：
   - 用户态：遍历链表时打印每个节点的 `priority`（如示例中的"链上顺序"打印），配合日志观察实际调用序列。
   - Kernel：`notify_die`、`panic_notifier_list` 等链在系统崩溃路径上极其关键，通常配合
     `printk` 时序、`ftrace` 的 `trace_printk` 在关键回调里打点；`CONFIG_DEBUG_NOTIFIERS`
     （见 `kernel/notifier.c` 里的 `func_ptr_is_kernel_text` 检查）用于捕捉"notifier 函数指针
     根本不指向有效内核代码"这种极端的悬挂调用场景。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `notifier_chain_register()`：按优先级插入

真实定义：[kernel/notifier.c:21](../../kernel/notifier.c)

```c
static int notifier_chain_register(struct notifier_block **nl, struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	rcu_assign_pointer(*nl, n);   /* RCU 安全地插入 */
	return 0;
}
```

### Example 2 — `notifier_call_chain()`：广播 + STOP 语义

真实定义：[kernel/notifier.c:74](../../kernel/notifier.c)

```c
static int notifier_call_chain(struct notifier_block **nl, unsigned long val, void *v, ...)
{
	int ret = NOTIFY_DONE;
	struct notifier_block *nb = rcu_dereference_raw(*nl);

	while (nb && nr_to_call) {
		struct notifier_block *next_nb = rcu_dereference_raw(nb->next);
		ret = nb->notifier_call(nb, val, v);   /* <-- 盲调 */
		if (ret & NOTIFY_STOP_MASK)
			break;                             /* <-- 短路 */
		nb = next_nb;
	}
	return ret;
}
```

### Example 3 — `atomic_notifier_chain_register/call`：原子上下文的通知

```c
/* kernel/notifier.c 简化 */
int atomic_notifier_chain_register(struct atomic_notifier_head *nh, struct notifier_block *n)
{
	unsigned long flags;
	spin_lock_irqsave(&nh->lock, flags);      /* 保证注册操作在中断上下文也安全 */
	notifier_chain_register(&nh->head, n);
	spin_unlock_irqrestore(&nh->lock, flags);
	return 0;
}

int atomic_notifier_call_chain(struct atomic_notifier_head *nh, unsigned long val, void *v)
{
	rcu_read_lock();
	ret = notifier_call_chain(&nh->head, val, v, -1, NULL);
	rcu_read_unlock();
	return ret;
}
```
为什么是 DI：`die_chain`（`register_die_notifier()`，本仓库 `kernel/notifier.c:544` 附近）就是用
`atomic_notifier_head`实现的——内核 oops/panic 时需要在原子上下文（可能已经关中断）通知调试子系统，
不能用会睡眠的锁。

### Example 4 — `blocking_notifier_call_chain`：进程上下文，可以睡眠

```c
int __blocking_notifier_call_chain(struct blocking_notifier_head *nh, unsigned long val, void *v, ...)
{
	if (rcu_dereference_raw(nh->head)) {
		down_read(&nh->rwsem);              /* rwsem 允许睡眠，适合较重的回调逻辑 */
		ret = notifier_call_chain(&nh->head, val, v, nr_to_call, nr_calls);
		up_read(&nh->rwsem);
	}
	return ret;
}
```
为什么用 blocking 而不是 atomic：像 `reboot_notifier_list`、`usermodehelper_notifier_list`
这类事件的处理逻辑往往需要分配内存/等待 I/O，必须允许睡眠。

### Example 5 — `panic_notifier_list`：系统崩溃时的最后一次广播

场景：`panic()` 被调用时，Kernel 需要给尽可能多的子系统一个"做最后清理"的机会
（刷写日志、通知看门狗、保存诊断信息），这些子系统彼此毫无关系，只能靠通用广播机制串起来。

```c
/* kernel/panic.c 风格 */
void panic(const char *fmt, ...)
{
	...
	atomic_notifier_call_chain(&panic_notifier_list, 0, buf);
	/* 每个关心 panic 的子系统（看门狗驱动、crash dump、远程日志...）
	 * 各自注册了一个 notifier_block，此处统一广播 */
	...
}
```

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/05-notifier-chain/`](c/05-notifier-chain/)

### 9.1 `notifier_chain_impl.c` — 从零实现 notifier chain

- 场景：完整复刻 `priority` 排序 + `NOTIFY_STOP_MASK` 短路语义，模拟 CPU 上线/下线通知。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o notifier_chain_impl notifier_chain_impl.c`
- 实测输出：
  ```
    [guard] CPU 99 exceeds supported max, rejecting (STOP chain)
  === notifier_chain_impl demo ===

  -- 链上顺序（按 priority 从高到低）--
    priority=1000 
    priority=100  
    priority=50   

  -- 广播 CPU_ONLINE for cpu=3（合法）--
    [sched] rebalancing runqueues for new CPU 3
    [perf] initializing PMU counters for CPU 3
    final ret = 0x1

  -- 广播 CPU_ONLINE for cpu=99（超出上限，guard 会 STOP 链）--
      [chain] notifier returned STOP (0x8002), halting propagation
    final ret = 0x8002 (sched/perf 不应该被调用到)

  -- 注销 perf 订阅，再广播一次 CPU_DOWN_PREPARE --
    [sched] migrating tasks off CPU 3
    final ret = 0x1

  === done ===
  ```
  （链上顺序确实是 1000 -> 100 -> 50，与乱序注册顺序 perf(50)->guard(1000)->sched(100) 完全不同，
  证明排序逻辑按 priority 生效；cpu=99 时 guard 返回 STOP 后 sched/perf 确实未出现在输出中。）
- 为什么是 DI：`notifier_call_chain()`（Core）遍历一个运行期动态构建的链表，完全不知道链上挂的是
  调度器、perf 还是 guard；新增一个订阅者只需要 `register`，不需要改 Core 一行代码。

### 9.2 `pubsub_event_bus.c` — 发布订阅事件总线

- 场景：把 notifier chain 的思想横向扩展成"多 topic 独立订阅"，验证按 topic 隔离广播、
  取消订阅后不再收到消息。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o pubsub_event_bus pubsub_event_bus.c`
- 实测输出：
  ```
  === pubsub_event_bus demo ===

  [bus] created new topic 'system.error'
  [bus] 'audit-logger' subscribed to topic 'system.error'
  [bus] 'pager-duty' subscribed to topic 'system.error'
  [bus] created new topic 'order.created'
  [bus] 'audit-logger' subscribed to topic 'order.created'
  [bus] 'metrics-collector' subscribed to topic 'order.created'

  -- 发布事件 --
  [bus] publishing to topic 'order.created' (2 subscriber(s)): "order#1001 placed by user42"
    [audit-logger] AUDIT LOG: topic='order.created' payload="order#1001 placed by user42" (total seen=1)
    [metrics-collector] incrementing counter for topic='order.created' (count=1)

  [bus] publishing to topic 'system.error' (2 subscriber(s)): "disk /dev/sda1 read error"
    [audit-logger] AUDIT LOG: topic='system.error' payload="disk /dev/sda1 read error" (total seen=2)
    [pager-duty] !! PAGE ON-CALL !! topic='system.error' payload="disk /dev/sda1 read error"

  [bus] publish to unknown topic 'unknown.topic' (no subscribers, dropped)

  -- 取消订阅 alert_sub，再发一次 error --
  [bus] 'pager-duty' unsubscribed from topic 'system.error'
  [bus] publishing to topic 'system.error' (1 subscriber(s)): "second disk error, no paging this time"
    [audit-logger] AUDIT LOG: topic='system.error' payload="second disk error, no paging this time" (total seen=3)

  === done ===
  audit_sub total messages   = 3
  alert_sub total messages   = 1
  metrics_sub total messages = 1
  ```
- 为什么是 DI：`bus_publish()`（Core）不知道 `audit_sub`/`alert_sub`/`metrics_sub` 具体做什么，
  只按 topic 找到订阅链并逐一盲调 `on_message`；`audit_sub` 同时订阅两个 topic，验证了同一个订阅者
  可以出现在多条独立的通知链上。

### 9.3 `gui_event_broadcast.c` — GUI 多监听者广播 + 事件消费截断

- 场景：三个 widget 按 `z_order`（对应 notifier `priority`）响应同一个点击事件，
  演示"消费后停止传播"（等价于 `NOTIFY_STOP_MASK`）。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o gui_event_broadcast gui_event_broadcast.c`
- 实测输出：
  ```
  === gui_event_broadcast demo ===

  [dispatcher] widget 'log-panel' registered (z_order=0)
  [dispatcher] widget 'status-bar' registered (z_order=50)
  [dispatcher] widget 'confirm-dialog' registered (z_order=100)

  -- 事件1：点在对话框范围内，应该被 dialog 消费，statusbar/logpanel 不会收到 --
  [dispatcher] dispatching 'click' event at (50,30) to 3 widget(s)
      [dialog] click inside dialog bounds, handling 'OK' button
    [dispatcher] event consumed by 'confirm-dialog' (z=100), stop propagation

  -- 事件2：点在状态栏区域，dialog 忽略，statusbar 消费 --
  [dispatcher] dispatching 'click' event at (400,550) to 3 widget(s)
      [dialog] click outside dialog bounds, ignoring
      [statusbar] click in status bar, toggling menu
    [dispatcher] event consumed by 'status-bar' (z=50), stop propagation

  -- 事件3：点在空白区域，谁都不消费，最终 logpanel 只是观察到但不处理 --
  [dispatcher] dispatching 'click' event at (400,300) to 3 widget(s)
      [dialog] click outside dialog bounds, ignoring
      [logpanel] observed event 'click' at (400,300), not consuming
    [dispatcher] no widget consumed the event, it falls through

  -- 关闭对话框（注销），重新触发事件1的坐标 --
  [dispatcher] widget 'confirm-dialog' removed
  [dispatcher] dispatching 'click' event at (50,30) to 2 widget(s)
      [logpanel] observed event 'click' at (50,30), not consuming
    [dispatcher] no widget consumed the event, it falls through

  === done ===
  dialog handled=1, statusbar handled=1, logpanel handled=0
  ```
  （事件2的输出里 `status-bar` 消费之后，`log-panel`（z_order 更低）确实没有出现在这次 dispatch 的日志里，
  验证了"消费即停止传播"生效；对话框被移除后，同样坐标的事件1改为"落到底"不被任何 widget 消费。）
- 为什么是 DI：`dispatcher_dispatch()`（Core）按 `z_order` 排好的顺序盲调每个 widget 的 `on_event`，
  完全不知道 dialog/statusbar/logpanel 各自的业务逻辑；GUI 框架里新增一种 widget 类型不需要修改
  dispatch 逻辑。

一次性编译并运行全部三个示例：`cd c/05-notifier-chain && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Notifier Chain | Function Pointer Callback | Driver Registration | Ops Table |
|---|---|---|---|---|
| 响应者数量 | **任意多个**，同时响应 | 固定一个 | 精确匹配后固定一个 | 固定一个（一个对象一份 ops） |
| 是否需要"匹配"逻辑 | 否，全部无条件广播（但可以在回调内部自行判断是否处理） | 否 | **是**，需要 `match()` | 否 |
| 是否支持"短路"/"否决" | **是**，`NOTIFY_STOP_MASK` | 通常不需要 | 是（`probe()` 返回非 0 表示放弃，尝试下一个候选） | 不适用 |
| 顺序控制 | `priority` 字段 | 由 Core 的表/数组顺序决定 | `priority`（04 篇示例） | 不适用（一对一） |
| 何时用 Notifier Chain | 一个事件需要通知**不确定数量**的、**互相独立**的订阅者 | 只有一个确定的处理者 | 需要"从多个候选中选出恰好一个"来精确处理 | 需要"一整套确定的行为集合" |

一句话：**Notifier Chain 是"一对多"的 DI**（一个事件，N 个订阅者都要响应），
而 Driver Registration 是"多对一"的 DI（N 个候选驱动，精确选出 1 个来处理）——
两者互补，共同覆盖了"广播"和"路由"这两类完全不同的解耦需求。
下一篇 [06-initcall.md](06-initcall.md) 回答另一个问题：**这些注册动作（`register_xxx()`）本身，
是谁、在什么时候负责调用的？**
