# 18. Tasklet（`struct tasklet_struct` / `tasklet_schedule`）

> 上一批 [07-workqueue.md](07-workqueue.md) 讲的是"可以睡眠的延后任务"；本批 [17-softirq.md](17-softirq.md)
> 讲的是"编译期固定编号、可以在不同 CPU 上并发执行"的软中断。本篇讲的 Tasklet 正好卡在两者中间：
> **动态注册（不占用固定编号），但仍在软中断上下文执行（不可睡眠），且同一个 tasklet 绝不会并发**。

---

## 1. Why

Softirq（17 篇）解决了"中断处理函数不能做太多事，把耗时工作 defer 到中断返回之后"的问题，但它有一个先天限制：

> `NR_SOFTIRQS` 是编译期就固定的一个小整数（本仓库里是 10 个：`HI_SOFTIRQ`、`TIMER_SOFTIRQ`、
> `NET_TX_SOFTIRQ`……），每一个编号对应且只对应一个处理函数，用 `open_softirq(nr, action)` 在初始化期注册。

这意味着：**如果每个驱动实例都需要一个自己的"下半部"，softirq 完全不够用**——一台机器上可能插着
5 张网卡、3 个 MMC 控制器，它们各自都想要"延后到中断上下文之外，但仍然快速执行"的机制，
不可能每种设备类型都去内核申请一个全局唯一的 softirq 编号（而且 `NR_SOFTIRQS` 本来就只有 10 个）。

如果没有 Tasklet，驱动作者只有两种糟糕的选择：

1. **复用一个已有的 softirq 编号，自己在 handler 里再做一次分发**：等价于自己重新发明 tasklet，
   还要自己处理"这个设备的处理函数会不会在多个 CPU 上并发执行"这个棘手问题。
2. **直接甩给 workqueue**：但 workqueue 的任务在独立内核线程里执行，可以被调度器换出、可以睡眠，
   对于"网卡收到一个包，要尽快、低延迟地处理，但又不适合放在硬中断里"这种场景，
   thread 调度的延迟和开销都比软中断上下文高得多。

Tasklet 的设计目标：

> **提供一种"驱动可以随时动态创建、随时调度、不需要争抢固定编号资源"的下半部机制，
> 同时保留 softirq 的"软中断上下文执行、延迟可预测"特性，
> 并且额外保证：同一个 tasklet 实例绝不会被两个 CPU 同时执行。**

`kernel/softirq.c:494` 的原始注释把这条设计说得非常直接：

```
/* Tasklets --- multithreaded analogue of BHs.

   Main feature differing them of generic softirqs: tasklet
   is running only on one CPU simultaneously.

   Main feature differing them of BHs: different tasklets
   may be run simultaneously on different CPUs.
```

一句话：**Tasklet 是"动态注册版的 softirq"，代价是每个 tasklet 内部自己串行化，
换来的好处是驱动可以随时创建任意多个独立的下半部，互不干扰。**

---

## 2. Architecture

```
   驱动 / 子系统（Core 不知道具体是谁）
        |
        | 持有若干个 struct tasklet_struct 实例（可以是任意多个，运行期创建）
        v
  +-----------------------+
  | struct tasklet_struct |    include/linux/interrupt.h:515
  |  next                 |    <-- 侵入式单链表指针
  |  state                |    <-- bit0=SCHED bit1=RUN
  |  count                |    <-- >0 表示 disable
  |  func                 |    <-- 回调（Function Pointer Callback，01篇）
  |  data                 |    <-- 上下文（Context Injection，03篇的"手段A"）
  +-----------------------+
        |
        | tasklet_schedule() 时被挂入
        v
  +---------------------------+
  | per-cpu tasklet_vec       |   kernel/softirq.c:397 struct tasklet_head
  | (每个 CPU 一条独立的       |   DEFINE_PER_CPU(tasklet_vec)
  |  pending 单链表)           |
  +---------------------------+
        |
        | raise_softirq(TASKLET_SOFTIRQ)
        v
  +---------------------------+
  | softirq_vec[TASKLET_SOFTIRQ] |  见 02/17 篇：ops table 的极简版
  |   .action = tasklet_action    |  kernel/softirq.c:736 open_softirq(TASKLET_SOFTIRQ, tasklet_action)
  +---------------------------+
        |
        | 软中断上下文里被调用
        v
  tasklet_action() 遍历本 CPU 的 pending 链表，逐个调用 t->func(t->data)
```

对照 04 篇/05 篇/07 篇：Tasklet 和 Softirq 都不需要"匹配"（不像 Driver Registration），
也不是"广播"（不像 Notifier Chain）——它是纯粹的"单点、延后、串行化"的回调注入。

---

## 3. Lifecycle

```
   allocate
   (驱动在自己的私有结构体里嵌入一个 struct tasklet_struct，运行期任意数量)
        |
        v
   initialize
   (tasklet_init(&t, func, data) 或 DECLARE_TASKLET(name, func, data)：
    state=0, count=0，只是把 func/data 填好，还没有进入任何链表)
        |
        v
   inject / schedule
   (硬件中断到来 -> ISR 里调用 tasklet_schedule(&t)：
    test_and_set_bit(TASKLET_STATE_SCHED) 成功才真正入队本 CPU 的 tasklet_vec，
    raise_softirq(TASKLET_SOFTIRQ)；如果已经在队列里，本次调用被去重)
        |
        v
   framework owns dispatch
   (中断返回路径触发 do_softirq() -> tasklet_action() 从本 CPU pending 链表取出)
        |
        v
   trylock + count 检查
   (tasklet_trylock() 用 test_and_set_bit(TASKLET_STATE_RUN) 保证互斥；
    如果 count>0（被 disable），清 RUN 位，把 t 重新排回队列，本轮不执行)
        |
        v
   callback
   (清 SCHED 位，调用 t->func(t->data)，执行期间仍处于软中断上下文——不可睡眠)
        |
        v
   unlock
   (tasklet_unlock() 清 RUN 位，允许下一次 tasklet_schedule() 重新入队执行)
        |
        v
   destroy
   (tasklet_kill(&t)：等待"当前已排队但还没跑完"的这一次执行完成，
    清掉 SCHED 位，之后驱动才能安全释放内嵌了 tasklet_struct 的结构体)
```

与 07 篇 Workqueue 的关键生命周期差异：workqueue 的 `work_struct` 执行完之后工作线程可以立刻睡眠等下一个任务；
tasklet 执行完之后如果被 `tasklet_disable()` 拦住，会被**重新排回队列**而不是丢弃——这是"暂停调度但记住请求"
语义的具体体现，第 7 节 Failure 和第 9 节示例都会验证这一点。

---

## 4. Relationship

```
   驱动私有结构体（如 struct sc92031_priv）
     |
     |  内嵌（Context Injection，03篇）
     v
   struct tasklet_struct
     |
     |  地址被交给
     v
   per-cpu tasklet_vec（Core，不知道具体是哪个驱动）
     |
     |  软中断触发时遍历
     v
   tasklet_action() —— Core 的分发循环
     |
     |  盲调 t->func(t->data)
     v
   驱动的回调函数（如 sc92031_tasklet / wbsd_tasklet_finish）
     |
     |  通过 data 字段（Context Injection）拿回驱动私有数据
     v
   驱动私有结构体（还原成功，可以安全访问硬件寄存器等状态）
```

真实驱动关系示例（`drivers/mmc/host/wbsd.h:178`）：

```c
struct wbsd_host {
	...
	struct tasklet_struct	card_tasklet;	/* 卡插拔检测 */
	struct tasklet_struct	fifo_tasklet;	/* FIFO 数据处理 */
	struct tasklet_struct	crc_tasklet;	/* CRC 校验完成 */
	struct tasklet_struct	timeout_tasklet;/* 命令超时 */
	struct tasklet_struct	finish_tasklet;	/* 命令完成 */
};
```

一个驱动可以同时拥有**5 个独立的 tasklet**，各自互相之间可以并发执行（`kernel/softirq.c:500` 明确说明
"different tasklets may be run simultaneously on different CPUs"），但**每一个 tasklet 自己内部严格串行**。

---

## 5. Data Flow

对照真实内核路径（网卡收包场景）：

```
硬件中断（网卡收到数据）
  |
  v
my_netdev_isr(irq, dev_id)                      <-- 硬中断上下文，不能睡眠
  |
  | tasklet_schedule(&priv->rx_tasklet)          include/linux/interrupt.h:561
  |   test_and_set_bit(TASKLET_STATE_SCHED)      成功 -> 入队
  v
__tasklet_schedule(t)                            kernel/softirq.c:405
  |  挂入 __this_cpu_read(tasklet_vec.tail)
  |  raise_softirq_irqoff(TASKLET_SOFTIRQ)
  v
（硬中断处理函数返回，ISR 结束，中断返回路径）
  |
  v
do_softirq() / __do_softirq()                    kernel/softirq.c:209 一带而过（本系列 17 篇详细展开）
  |  发现 TASKLET_SOFTIRQ 的 pending 位被置位
  v
tasklet_action(a)                                kernel/softirq.c:444
  |  摘走本 CPU 整条 pending 链表
  |  for each t in list:
  |      tasklet_trylock(t)                      互斥：保证串行
  |      if atomic_read(&t->count) == 0:
  |          clear_bit(TASKLET_STATE_SCHED)
  v
t->func(t->data)                                  <-- 驱动的实际处理函数，仍是软中断上下文
  |
  v
my_rx_tasklet_fn(unsigned long data)
  |  struct my_priv *priv = (struct my_priv *)data;   （手段A：03篇 Context Injection）
  v
处理 priv->rx_ring 里的数据包，最终交给网络协议栈
```

对照本文档 [c/18-tasklet/tasklet_schedule_sim.c](c/18-tasklet/tasklet_schedule_sim.c) 的真实运行输出
（第 9 节有完整粘贴）：调度、去重、执行三个阶段与上面这条真实路径完全一一对应。

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `state` 的 `TASKLET_STATE_SCHED` 位 | **运行期频繁变化** | `tasklet_schedule()` 置位，`tasklet_action()` 执行前清位；置位期间重复 schedule 会被去重 |
| `state` 的 `TASKLET_STATE_RUN` 位（仅 SMP） | **运行期频繁变化，且必须原子操作** | 这是"同一 tasklet 不并发执行"这条核心保证的唯一实现手段（`tasklet_trylock`/`tasklet_unlock`） |
| `count`（`atomic_t`） | **可变**，由 `tasklet_disable`/`tasklet_enable` 成对维护 | `>0` 时 `tasklet_action()` 会把已出队的 tasklet 重新排回，而不是丢弃调度请求 |
| `func`/`data` | **初始化后基本不变** | 一次 `tasklet_init()` 之后很少再改，与 03 篇 `container_of`/`data` 两种手段里的"手段A"完全一致 |
| `next` | **仅由 Core（tasklet_vec 相关代码）修改** | 驱动代码永远不应该直接读写这个字段，纯粹是链表侵入式指针 |
| per-cpu `tasklet_vec.head/tail` | **每个 CPU 独立，仅在本 CPU 软中断上下文/关中断区间修改** | 与 softirq（17 篇）共享"per-cpu 状态无需跨 CPU 加锁"的设计前提 |

---

## 7. Failure

1. **在 tasklet 回调里睡眠/阻塞**：tasklet 的执行环境等价于软中断上下文，调用任何可能睡眠的函数
   （`mutex_lock`、`msleep`、`kmalloc(GFP_KERNEL)`、等待自己触发的 I/O 完成）都是致命错误。
   `CONFIG_DEBUG_ATOMIC_SLEEP` 下会在 `__might_sleep()` 里检测到并打印
   `"BUG: sleeping function called from invalid context"`，本篇
   [tasklet_vs_workqueue_compare.c](c/18-tasklet/tasklet_vs_workqueue_compare.c) 用用户态代码复现了这个检测逻辑。
2. **误以为"disable 会取消已排队的调度"**：`tasklet_disable()` 只是让 `tasklet_action()`
   在遇到这个 tasklet 时把它重新排回队列，**并不会清掉 pending 状态**，`tasklet_enable()`
   之后下一轮 `tasklet_action()` 会立刻把之前被推迟的调用执行掉——如果驱动作者以为
   "disable 期间发生的调度请求会被吃掉"，据此写出依赖"错过一次调度也没关系"的逻辑，就会出现
   "明明 disable 了，回调却还是执行了"的诡异 bug。
3. **`tasklet_kill()` 在中断上下文调用**：`kernel/softirq.c:527` 的 `tasklet_kill()` 内部用
   `yield()` 忙等直到 `TASKLET_STATE_SCHED` 位被清除——如果在中断上下文调用它，`yield()`
   根本不可能让出 CPU 给正在等待的软中断处理，会造成死等；`tasklet_kill()` 内部有
   `if (in_interrupt()) printk("Attempt to kill tasklet from interrupt\n");` 这行防御性诊断
   （`kernel/softirq.c:529`），但只是打印警告，不会阻止死等发生。
4. **忘记调用 `tasklet_kill()` 就释放内嵌了 `tasklet_struct` 的对象**：如果对象释放时
   tasklet 还在某个 CPU 的 pending 链表里，`tasklet_action()` 稍后会访问一块已经释放的内存——
   这是 03 篇讲过的"悬挂指针"风险在 tasklet 场景下的具体版本。
5. **误用 `count` 做业务计数**：`count` 字段完全是"是否 disable"的内部实现细节
   （`atomic_inc`/`atomic_dec` 配对），绝不应该被驱动代码读出来当成别的业务用途，
   否则会破坏 `tasklet_action()` 里 `atomic_read(&t->count)` 的判断语义。
6. **调试方法**：
   - 用户态：本篇三个示例分别用 `printf` 打印每一步状态转换、用原子计数器
     `g_currently_running`/`g_max_concurrent_seen` 实测验证互斥性
     （见 [tasklet_disable_enable_demo.c](c/18-tasklet/tasklet_disable_enable_demo.c)），
     用 `-fsanitize=thread`（ThreadSanitizer）可以进一步检测数据竞争。
   - Kernel：`/proc/softirqs` 可以看到 `TSKLET`（tasklet 借用的 `TASKLET_SOFTIRQ`）触发次数；
     `ftrace` 的 `softirq_entry`/`softirq_exit` 事件能定位 tasklet 执行耗时是否异常；
     `CONFIG_DEBUG_ATOMIC_SLEEP` + `lockdep` 组合能在开发阶段抓到"在 tasklet 里睡眠"这类错误；
     `WARN_ON(!in_softirq())` 是很多驱动在自己的 tasklet 回调开头加的防御性断言写法。

---

## 8. Kernel Core Pseudocode（5 个，均标注本仓库真实文件/行号）

### Example 1 — `struct tasklet_struct` 本身

真实定义：[include/linux/interrupt.h:515](../../include/linux/interrupt.h)

```c
struct tasklet_struct
{
	struct tasklet_struct *next;
	unsigned long state;
	atomic_t count;
	void (*func)(unsigned long);
	unsigned long data;
};

#define DECLARE_TASKLET(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(0), func, data }
```
为什么是 DI：`func`/`data` 就是 01 篇 Function Pointer Callback + 03 篇 Context Injection
"手段A"的直接组合——`next`/`state`/`count` 是 Core（tasklet 调度机制）专属字段，
驱动代码只应该关心 `func`/`data`。

### Example 2 — `tasklet_schedule()`：调度入口

真实定义：[include/linux/interrupt.h:561](../../include/linux/interrupt.h)，
真正入队逻辑：[kernel/softirq.c:405](../../kernel/softirq.c) `__tasklet_schedule()`

```c
static inline void tasklet_schedule(struct tasklet_struct *t)
{
	if (!test_and_set_bit(TASKLET_STATE_SCHED, &t->state))
		__tasklet_schedule(t);
}

void __tasklet_schedule(struct tasklet_struct *t)
{
	unsigned long flags;
	local_irq_save(flags);
	t->next = NULL;
	*__this_cpu_read(tasklet_vec.tail) = t;
	__this_cpu_write(tasklet_vec.tail, &(t->next));
	raise_softirq_irqoff(TASKLET_SOFTIRQ);
	local_irq_restore(flags);
}
```
为什么是 DI：调用者（驱动 ISR）只知道"我要调度这个 tasklet"，完全不知道 `tasklet_vec`
这个 per-cpu 链表的存在，也不知道 `TASKLET_SOFTIRQ` 这个编号——这些都是 Core 侧的实现细节，
被 `tasklet_schedule()` 这一层薄薄的契约完全隔离。

### Example 3 — `tasklet_action()`：Core 的分发循环，互斥性的落地点

真实定义：[kernel/softirq.c:444](../../kernel/softirq.c)

```c
static void tasklet_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	local_irq_disable();
	list = __this_cpu_read(tasklet_vec.head);
	__this_cpu_write(tasklet_vec.head, NULL);
	__this_cpu_write(tasklet_vec.tail, &__get_cpu_var(tasklet_vec).head);
	local_irq_enable();

	while (list) {
		struct tasklet_struct *t = list;
		list = list->next;

		if (tasklet_trylock(t)) {
			if (!atomic_read(&t->count)) {
				if (!test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
					BUG();
				t->func(t->data);      /* <-- 盲调，Core 不知道具体是谁 */
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}
		/* 拿不到锁或被 disable：重新排回队列，下一轮再处理 */
		local_irq_disable();
		t->next = NULL;
		*__this_cpu_read(tasklet_vec.tail) = t;
		__this_cpu_write(tasklet_vec.tail, &(t->next));
		__raise_softirq_irqoff(TASKLET_SOFTIRQ);
		local_irq_enable();
	}
}
```
为什么是 DI：这是本篇最核心的一段——`tasklet_trylock()`/`atomic_read(&t->count)` 这两行判断
共同实现了"同一个 tasklet 绝不并发、disable 期间调度请求不丢失"两条设计承诺，而 `t->func(t->data)`
这一行盲调完全不知道 `t` 具体是网卡驱动的 tasklet 还是 MMC 驱动的 tasklet。

### Example 4 — `tasklet_disable()`/`tasklet_enable()`：暂停调度但不取消已调度的执行

真实定义：[include/linux/interrupt.h:590-607](../../include/linux/interrupt.h)

```c
static inline void tasklet_disable_nosync(struct tasklet_struct *t)
{
	atomic_inc(&t->count);
	smp_mb__after_atomic_inc();
}

static inline void tasklet_disable(struct tasklet_struct *t)
{
	tasklet_disable_nosync(t);
	tasklet_unlock_wait(t);   /* 等待"正在跑的这一次"结束，而不是等待"还未发生的调度" */
	smp_mb();
}

static inline void tasklet_enable(struct tasklet_struct *t)
{
	smp_mb__before_atomic_dec();
	atomic_dec(&t->count);
}
```
真实驱动用法：[drivers/net/ethernet/silan/sc92031.c:370,378](../../drivers/net/ethernet/silan/sc92031.c)

```c
static void sc92031_disable_interrupts(struct net_device *dev)
{
	...
	synchronize_irq(dev->irq);
	tasklet_disable(&priv->tasklet);   /* line 370 */
}

static void sc92031_enable_interrupts(struct net_device *dev)
{
	struct sc92031_priv *priv = netdev_priv(dev);
	tasklet_enable(&priv->tasklet);    /* line 378 */
	...
}
```
为什么是 DI：`count` 这个字段把"是否允许执行"这条策略从 `tasklet_action()`（Core）身上
抽出来，交给驱动通过 `disable`/`enable` 一对函数动态控制——Core 侧的分发逻辑完全不用
关心"为什么这次不能执行"，只需要机械地检查 `count` 是否为 0。

### Example 5 — `tasklet_init()`/`tasklet_kill()`：生命周期两端

真实定义：[kernel/softirq.c:515,527](../../kernel/softirq.c)，
真实驱动用法：[drivers/mmc/host/wbsd.c:1462,1494](../../drivers/mmc/host/wbsd.c)

```c
void tasklet_init(struct tasklet_struct *t,
		  void (*func)(unsigned long), unsigned long data)
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func = func;
	t->data = data;
}

void tasklet_kill(struct tasklet_struct *t)
{
	if (in_interrupt())
		printk("Attempt to kill tasklet from interrupt\n");
	while (test_and_set_bit(TASKLET_STATE_SCHED, &t->state)) {
		do { yield(); } while (test_bit(TASKLET_STATE_SCHED, &t->state));
	}
	tasklet_unlock_wait(t);
	clear_bit(TASKLET_STATE_SCHED, &t->state);
}
```

```c
/* drivers/mmc/host/wbsd.c:1462 —— 初始化 5 个独立 tasklet */
tasklet_init(&host->card_tasklet, wbsd_tasklet_card, (unsigned long)host);
tasklet_init(&host->fifo_tasklet, wbsd_tasklet_fifo, (unsigned long)host);
...
/* drivers/mmc/host/wbsd.c:1494 —— 卸载路径逐一 kill，保证没有残留调度 */
tasklet_kill(&host->card_tasklet);
tasklet_kill(&host->fifo_tasklet);
```
为什么是 DI：`tasklet_init()` 把"契约字段"（`func`/`data`）填好，`tasklet_kill()`
保证对象销毁前不会再有 Core 侧的悬挂引用——这正是 03 篇 Failure 里提到的
"业务对象生命周期必须覆盖注入到回调执行完成的整个区间"这条纪律的标准落地方式。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/18-tasklet/`](c/18-tasklet/)

### 9.1 `tasklet_schedule_sim.c` — 调度、去重、执行三阶段完整模拟

- 场景：单线程模拟 `tasklet_schedule()` 入队 + `tasklet_action()` 出队执行的完整流程，
  重点验证"同一 tasklet 还未执行完时重复调度会被去重"这条 `kernel/softirq.c:504` 注释里写明的性质。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o tasklet_schedule_sim tasklet_schedule_sim.c`
- 实测输出：
  ```
  === tasklet_schedule_sim ===

  -- 阶段1：模拟中断处理函数（ISR）里调用 tasklet_schedule --
    [tasklet_schedule] 'net_rx_t' 入队 pending 链表，raise_softirq(TASKLET_SOFTIRQ)
    [tasklet_schedule] 'mmc_finish_t' 入队 pending 链表，raise_softirq(TASKLET_SOFTIRQ)
    [tasklet_schedule] 'net_rx_t' 已经在 pending 链表里，本次调度被合并（去重）

  -- 阶段2：模拟软中断上下文执行 tasklet_action（遍历 pending 链表）--
    [tasklet_action] 执行 'net_rx_t'->func(data=0)
      [net_rx] 处理网卡 #0 的收包 tasklet（模拟 drivers/net/usb/usbnet.c 的 dev->bh）
    [tasklet_action] 执行 'mmc_finish_t'->func(data=1)
      [mmc_finish] 处理 MMC 主机 #1 的命令完成 tasklet（模拟 wbsd.c finish_tasklet）

  -- 阶段3：net_rx tasklet 再次被调度（模拟第二次网卡中断）--
    [tasklet_schedule] 'net_rx_t' 入队 pending 链表，raise_softirq(TASKLET_SOFTIRQ)
    [tasklet_action] 执行 'net_rx_t'->func(data=0)
      [net_rx] 处理网卡 #0 的收包 tasklet（模拟 drivers/net/usb/usbnet.c 的 dev->bh）

  -- 阶段4：验证 SCHED 位已清除，两个 tasklet 都处于空闲状态 --
    net_rx_t.state=0 mmc_finish_t.state=0 (0 表示空闲，可再次调度)

  === done, softirq raised 3 time(s) total ===
  ```
- 为什么是 DI：`mini_tasklet_action()`（Core）从未 `#include` `net_rx_tasklet_fn`/`mmc_finish_tasklet_fn`
  的任何具体逻辑，只通过 `t->func(t->data)` 盲调；新增第三种 tasklet 类型不需要修改调度循环一行代码。

### 9.2 `tasklet_disable_enable_demo.c` — 互斥性 + disable/enable 语义的并发实测

- 场景：4 个"虚拟 CPU"线程并发争抢同一个 tasklet 的执行权，用原子计数器实测验证
  "同一 tasklet 任意时刻只在一个 CPU 上运行"；同时验证 `tasklet_disable()` 期间的调度请求
  会被推迟而不是丢弃。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o tasklet_disable_enable_demo tasklet_disable_enable_demo.c`
- 实测输出：
  ```
  === tasklet_disable_enable_demo ===

  -- 启动 4 个模拟 CPU worker 线程，反复执行 pending tasklet --

  -- 阶段1：正常调度，观察多个 CPU 线程互斥执行同一个 tasklet --
      [cpu3] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu2] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu3] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu2] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu1] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu2] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu0] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu3] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu0] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu2] 执行 'log_flush_tasklet'->func(data=57005)
      [cpu1] 执行 'log_flush_tasklet'->func(data=57005)
    执行次数=11，观测到的最大并发数=1（预期恒为 1）

  -- 阶段2：tasklet_disable()，模拟驱动关中断期间暂停调度执行 --
    disable 完成，count=1
    disable 期间连续调度 10 次（模拟 10 次网卡中断到来）...
    disable 期间 exec_count 变化: 11 -> 11（预期不变，调度请求被推迟而非丢弃）

  -- 阶段3：tasklet_enable()，之前被推迟的调度请求应该立刻被执行 --
      [cpu2] 执行 'log_flush_tasklet'->func(data=57005)
    enable 后 exec_count 变化: 11 -> 12（预期立刻追上，说明请求没有丢失）

  === done: 总执行次数=12, 观测到的最大并发执行数=1 ===
  验证通过：同一 tasklet 在任意时刻只在一个 CPU 上运行。
  ```
  （连续运行多次，`执行次数`/`cpuN` 的具体分布因线程调度而略有波动，但
  `观测到的最大并发数` 每一次都稳定为 `1`，这正是本示例要验证的核心断言；
  `disable` 期间 `exec_count` 恒定不变、`enable` 后立刻追上，多次实测同样稳定。）
- 为什么是 DI：`count` 字段把"要不要执行"这条策略从并发调度框架（Core）身上剥离出来，
  交给驱动通过 `tasklet_disable`/`tasklet_enable` 动态控制，Core 侧的互斥逻辑
  (`tasklet_trylock`) 完全不关心"为什么现在不能跑"，只负责机械地检查和重试。

### 9.3 `tasklet_vs_workqueue_compare.c` — 上下文约束的对照实验

- 场景：同一个"刷日志"任务分别用"tasklet 风格"（模拟软中断上下文，禁止阻塞）和
  "workqueue 风格"（独立线程，process context，允许阻塞）实现，验证两者能力边界的真实差异。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o tasklet_vs_workqueue_compare tasklet_vs_workqueue_compare.c`
- 实测输出：
  ```
    [BUG] 'sda1' 试图在软中断/tasklet 上下文里阻塞 20ms —— 等价于真实内核 CONFIG_DEBUG_ATOMIC_SLEEP 检测到的 "scheduling while atomic"，本次操作被拒绝执行
  === tasklet_vs_workqueue_compare ===

  -- (a) tasklet 风格：直接在软中断上下文里调用 --
    [tasklet] 开始处理 device='sda1' dirty_pages=128（假设身处软中断上下文）
    结果: result=-1 (0=成功 -1=被拒绝), dirty_pages剩余=0, 耗时=0.049ms

  -- (b) workqueue 风格：独立 worker 线程里调用 --
    [workqueue] 开始处理 device='sdb1' dirty_pages=128（独立线程，process context）
    结果: result=0 (0=成功 -1=被拒绝), dirty_pages剩余=0, 耗时=20.311ms

  === 对比总结 ===
    tasklet   风格: 失败——正如预期，验证了约束生效（软中断上下文里的阻塞操作被设计上禁止）
    workqueue 风格: 成功（process context 下可以安全阻塞）

  断言通过：tasklet 上下文正确地拒绝了阻塞操作，workqueue 上下文正确地允许了阻塞操作。
  ```
  （首行 `[BUG]` 是 stderr 无缓冲提前刷出，对应"软中断上下文尝试阻塞"这一预期路径被拒绝；
  tasklet 风格耗时 0.049ms、workqueue 风格耗时 20.311ms——这个耗时差正好等于模拟阻塞的 20ms，
  直观验证了"tasklet 必须快进快出，workqueue 可以老老实实等"这条设计边界。）
- 为什么是 DI：`in_softirq()` 这个上下文标记本身就是一种隐式的"运行环境契约"——
  Core（`tasklet_action`/`worker_thread`）向回调注入的不仅是"什么时候调用"，还隐含"在什么执行环境下调用"，
  这是 Tasklet 与 Workqueue 这两种 DI 手法除了"是否可以睡眠"之外，更本质的契约差异。

一次性编译并运行全部三个示例：`cd c/18-tasklet && make run`

---

## 10. 与其它 DI 手法比较

Softirq（17）、Tasklet（18）、Workqueue（07）构成一个清晰的"灵活性 / 并发性 / 上下文约束"谱系：

| 对比 | Softirq（17篇） | Tasklet（本篇） | Workqueue（07篇） |
|---|---|---|---|
| 注册方式 | 编译期固定编号，`open_softirq()` 在初始化期绑定 | **运行期任意时刻**动态创建，无编号限制 | 运行期任意时刻动态创建 |
| 同类型实例数量上限 | `NR_SOFTIRQS`（本仓库为 10），全局共享 | **无上限**，每个驱动可以有任意多个 | 无上限 |
| 执行上下文 | 软中断上下文，**不可睡眠** | 软中断上下文，**不可睡眠** | 独立内核线程（process context），**可以睡眠** |
| 同一实例是否可并发 | **可以**，同一 softirq handler 可能同时在多个 CPU 上跑（自己负责加锁） | **不可以**，`TASKLET_STATE_RUN` 位强制串行 | 可以，多个 worker 线程可并发处理不同 work，但同一 work 只会被一个 worker 执行 |
| 调度延迟 | 最低（下一次中断返回路径就会检查） | 与 softirq 相同（复用 `TASKLET_SOFTIRQ`） | 更高（需要线程调度器介入） |
| 典型用途 | 网络收包（`NET_RX_SOFTIRQ`）、时钟（`TIMER_SOFTIRQ`）等内核核心路径 | 驱动私有的、数量不定的下半部（网卡/MMC/USB 驱动） | 需要睡眠、需要长时间运行、可以容忍毫秒级调度延迟的任务 |
| 是否支持"暂停但不丢失请求" | 无此语义 | **有**，`tasklet_disable`/`count` 字段 | 有类似机制（`flush_work`/`cancel_work`），但语义是"取消"而非"暂停" |
| 何时用 | 内核核心子系统自己的、数量固定、性能极限的路径 | 驱动需要"轻量、低延迟、但又不想抢占固定 softirq 编号"的场景 | 任务本身需要访问可能阻塞的资源（磁盘、锁、用户态内存） |
| 何时不要用 | 不要在这里做任何可能阻塞或耗时较长的工作 | 不要在 tasklet 回调里睡眠/阻塞（本篇 9.3 示例的核心断言） | 不要用在需要纳秒级响应的硬实时路径 |

一句话：**Tasklet 是"Softirq 的动态注册版，同时比 Softirq 多了一条'自己不并发'的安全网”**——
它把"编号资源有限"这个 Softirq 的短板补上了，但没有解决"不能睡眠"这个更根本的上下文限制，
真正需要睡眠的场景仍然要交给 [07-workqueue.md](07-workqueue.md)。三者放在一起看，
正是 Kernel 在"中断上下文 -> 软中断上下文 -> 进程上下文"这条延迟处理链条上，
针对不同的灵活性/并发性/延迟要求给出的三种精确权衡。
