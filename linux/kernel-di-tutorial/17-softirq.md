# 17. Softirq

> 系列索引见 [00-mental-model.md](00-mental-model.md)。上一篇 16 讲了"硬中断触发的回调"，
> 本篇讲的是硬中断之后紧接着的"第二级、更轻量的推迟执行机制”——与 05 篇 notifier chain
> （动态链表、任意数量订阅者）、07 篇 workqueue（动态提交、可睡眠）都不同，
> softirq 走的是 DI 谱系里**最不灵活、但性能开销最小**的一端：编号在编译期就固定死了。

---

## 1. Why

16 篇讲过：硬中断的 primary handler 必须极快返回。但"极快"到什么程度？
真正意义上，**任何"批量处理"（哪怕只是遍历一个链表处理几个元素）都不应该放在硬中断里做**——
因为硬中断处理期间，本 CPU 上所有其它中断都被屏蔽（或至少同类中断被屏蔽），
处理时间越长，其它硬件事件的响应延迟越差。

16 篇的 threaded IRQ 方案（创建一个专属内核线程）解决了"允许睡眠、允许做重活"的问题，
但线程本身有成本：**上下文切换**。对于网络收包这种极高频率（每秒可能几十万次中断）
的场景，如果每次硬中断都要唤醒一个线程、等待调度器把它排上 CPU，这个切换开销本身
就可能超过实际处理这批数据所需的时间。

Kernel 需要一种"比线程更轻"的延后执行机制：

> **不创建新的执行体（线程/进程），复用当前 CPU 正在进行的"中断返回路径"这个天然的
> 时间点，把"批量处理"推迟到硬中断真正处理完毕、马上要开中断之前那一刻集中执行——
> 仍然禁止睡眠（因为还没真正回到进程上下文），但至少不再是"逐个硬件事件都必须立刻处理”，
> 而是可以"攒一批再处理”。**

这就是 softirq。它的设计还有一个更深层的约束：

> **不像 notifier chain 或 workqueue 那样支持运行期任意数量的动态注册——
> softirq 的"种类"（`NET_RX`、`TIMER`、`BLOCK`...）在内核编译时就已经全部列举完毕，
> 是一个固定大小、按下标直接索引的数组，不是链表。**

为什么要这么"不灵活"？因为**这张表在收包、时钟中断这些内核最热的路径上被访问**，
一次数组下标索引 + 一次位测试，比一次链表遍历要快得多、缓存行为也更可预测——
这是用"牺牲运行期可扩展性"换取"极限性能"的典型设计权衡。

---

## 2. Architecture

```
        CPU (任意来源的硬件中断，如网卡收包中断)
              |
              v
    +-------------------------------+
    |   硬中断 handler (16 篇内容)    |
    |   只做: 入队 + raise_softirq()  |
    +-------------------------------+
              |  设置本 CPU 的 pending 位
              v
    +-------------------------------+
    |  irq_stat[cpu].__softirq_pending    <-- 每 CPU 独立位图（本篇 Example 3）|
    +-------------------------------+
              |  硬中断处理完毕，返回前检查
              v
    +----------------------------------------------------+
    |               do_softirq() / __do_softirq()          |
    |                                                       |
    |   softirq_vec[NR_SOFTIRQS]  <-- 全局共享、固定大小的表   |
    |   +----+----+----+----+----+----+----+----+          |
    |   | HI |TMR |NTX |NRX |BLK |... |TSK |RCU |          |
    |   +----+----+----+----+----+----+----+----+          |
    |     |                                                 |
    |     按位遍历 pending，命中的下标才调用对应 action        |
    +----------------------------------------------------+
              |  逐个盲调
              v
    action(h)   -->  net_rx_action() / timer_action() / tasklet_action() / ...
```

对应到本文档的三个用户态示例：

```
softirq_fixed_slots:       softirq_vec[SIM_NR_SOFTIRQS]         （固定表 + open/raise_softirq）
softirq_vs_hardirq_demo:   nic_hardirq_handler() -> irq_exit_sim() -> action  （两级拆分）
percpu_softirq_pending:    percpu[NUM_CPUS].pending               （每 CPU 独立位图）
```

---

## 3. Lifecycle

```
   compile time               (enum { HI_SOFTIRQ, TIMER_SOFTIRQ, ..., NR_SOFTIRQS }
                                在 include/linux/interrupt.h 里固定写死，
                                这一步不是"运行期"发生的，是本手法最特殊的地方)
        |
        v
   open_softirq (boot 期一次性) (子系统在内核启动早期调用 open_softirq(nr, action)，
                                往 softirq_vec[nr] 这个固定槎位填函数指针——
                                之后基本不会再变，也没有 close_softirq() 这种注销接口)
        |
        v
   raise_softirq (随时)        (硬中断 handler / 其它代码路径调用，
                                只是把本 CPU 的 pending 位图某一位置 1，
                                不涉及内存分配、不涉及链表操作)
        |
        v
   framework owns dispatch     (Core: 硬中断返回路径/ksoftirqd 决定何时调用 do_softirq()，
                                触发方无法控制"什么时候真正执行”，只能控制"标记待执行")
        |
        v
   callback                    (__do_softirq() 遍历 pending 位图，
                                命中的下标调用 softirq_vec[nr].action(h)，
                                仍然禁止睡眠——这一步与硬中断 handler 共享同一条约束)
        |
        v
   (没有真正的 destroy 阶段)     (softirq 槎位一旦 open，通常伴随内核生命周期始终存在，
                                这与 01/04/05 篇里"注册/注销"成对出现的模式不同)
```

三个示例里分别是：

- `softirq_fixed_slots`：`open_softirq()`（一次性）-> `raise_softirq()`（多次）-> `do_softirq_sim()`（多次）
- `softirq_vs_hardirq_demo`：`open_softirq()` -> `nic_hardirq_handler()`（多次，快速）-> `irq_exit_sim()`（批量执行）
- `percpu_softirq_pending`：`open_softirq()`（全局一次）-> 每个模拟 CPU 独立 `raise_softirq_on_cpu()`/`do_softirq_on_cpu()`

---

## 4. Relationship

```
   子系统初始化代码（网络、定时器子系统等，boot 期）
        |  调用一次
        v
   open_softirq(nr, action)（Core 提供的极简注册接口）
        |  填入
        v
   softirq_vec[nr]（全局共享、只读、固定大小数组——所有 CPU 共用同一份表）
        |
        |                    每 CPU 各自独立
        |                          |
        v                          v
   硬中断路径 raise_softirq(nr)   本 CPU 的 pending 位图（irq_stat[cpu]）
        |                          |
        +------------+-------------+
                      |
                      v
            do_softirq()（Core，每个 CPU 各自执行自己的一份，互不通信）
                      |  按位遍历，命中才盲调
                      v
            softirq_vec[nr].action(h)（真正的业务逻辑）
```

关键点：**函数指针表是"全局共享"的，但"要不要执行、执行到哪一步"的状态
（pending 位图）是"每 CPU 私有"的**——这是 04 篇 driver registration、
05 篇 notifier chain 里都没有的"数据布局按 CPU 拆分"设计，是从"多核并发"
这个维度反推出来的架构决策，也是本篇 Example 3（`percpu_softirq_pending.c`）
要重点展示的部分。

---

## 5. Data Flow

以 `softirq_vs_hardirq_demo.c` 为例，完整调用链：

```
main()
  |
  |-- open_softirq(SIM_NET_RX_SOFTIRQ, net_rx_softirq_action)   // boot 期注册，一次性
  |
  `-- for each packet:
        nic_hardirq_handler(packet_id)      // 模拟硬中断 handler
          |
          |-- rx_ring[rx_ring_count++] = packet_id   // 硬中断能做的极少量工作：入队
          `-- raise_softirq(SIM_NET_RX_SOFTIRQ)      // <-- DI 发生点 1：只标记，不执行
                |
                v (硬中断返回前)
        irq_exit_sim()
          |
          v
        softirq_vec[SIM_NET_RX_SOFTIRQ].action()   // <-- DI 发生点 2：批量盲调
          |
          v
        net_rx_softirq_action()   // 一次性把 ring 里累积的所有包处理掉
```

对照真实 Kernel（`kernel/softirq.c`，本仓库真实代码）：

```
硬件收包中断 -> 驱动的 hard irq handler -> netif_rx() -> raise_softirq(NET_RX_SOFTIRQ)
  |
  v
irq_exit()（架构相关的中断返回路径，最终调用到 kernel/softirq.c 附近逻辑）
  |
  v
do_softirq()                          // kernel/softirq.c:272-289
  |
  v
__do_softirq()                        // kernel/softirq.c:207-267
  |
  |  pending = local_softirq_pending();
  |  h = softirq_vec;
  |  do { if (pending & 1) h->action(h); h++; pending >>= 1; } while (pending);
  v
h->action(h)  =>  net_rx_action(h)    // <-- 盲调，真正的网络收包批处理逻辑
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `softirq_vec[]`（函数指针表） | **boot 期填一次，之后只读** | 真实内核 `kernel/softirq.c:55` 声明为 `static`，没有导出任何"注销"接口，生命周期与内核本身相同 |
| `irq_stat[cpu].__softirq_pending`（每 CPU pending 位图） | **极其频繁地读写** | 每次 `raise_softirq()`/`__do_softirq()` 都会修改，但只在本 CPU 内部修改，不需要跨 CPU 同步原语 |
| softirq 编号本身（`enum` 常量） | **编译期常量，运行期完全不可变** | 与 05 篇 notifier chain 的"运行期动态数量"形成最尖锐的对比 |
| `ksoftirqd` 线程 | 每 CPU 一个，**内核启动时创建，几乎不销毁** | 真实内核 `kernel/softirq.c:740`（`run_ksoftirqd`）+ `kernel/softirq.c:856-869`（热插拔 CPU 时创建/绑定），当 softirq 负载过高、`__do_softirq()` 达到重试上限时才会唤醒它退到进程上下文继续处理 |
| `preempt_count()` / `SOFTIRQ_OFFSET` | **执行 softirq 期间临时改变** | 用来标记"当前处于 softirq 上下文"，`in_softirq()` 等判断都依赖这个计数 |

---

## 7. Failure

1. **在 softirq action 里睡眠**：与硬中断 handler 一样，softirq action 仍然不允许睡眠——
   它运行在"中断返回路径"或 `ksoftirqd` 内核线程里（`ksoftirqd` 本身是进程上下文，
   但为了避免破坏"softirq 应该快"的假设，实践上依然要求 action 保持轻量）。
2. **单个 softirq action 执行时间过长**：因为 `__do_softirq()` 是"逐位遍历、同步执行"，
   一个 action 跑得慢会拖慢同一批里排在它后面的所有其它 softirq，也会拖慢
   `irq_exit()` 之后本该尽快恢复的正常调度——这是为什么网络驱动的 NAPI 机制会给
   `net_rx_action` 设置"预算"（budget），一次最多处理固定数量的包就主动让出。
3. **`__do_softirq()` 的重试上限与 `ksoftirqd`**：真实内核 `kernel/softirq.c:258-262`
   （`if (pending && --max_restart) goto restart;` / `if (pending) wakeup_softirqd();`）
   —— 如果 softirq 被触发的速度远超处理速度，一直原地重试会导致"softirq 风暴"，
   长时间不给用户态进程调度机会；超过 `MAX_SOFTIRQ_RESTART`（10 次）后会唤醒
   `ksoftirqd`，把剩下的负载移交给一个普通调度实体，这样至少能被调度器公平对待。
   `softirq_vs_hardirq_demo.c` 的场景 B 用"5 次硬中断只触发 1 次批量处理"演示了
   这种"攒批"思路，但没有模拟真正的重试上限（那需要更复杂的压力测试）。
4. **每 CPU 位图被误当成全局共享状态来访问**：如果代码错误地用"某个固定 CPU 的
   pending 位图"去判断"系统里是否还有 softirq 要处理"，会得出完全错误的结论——
   `percpu_softirq_pending.c` 显式展示了 4 个 CPU 各自独立的 `pending`/统计字段，
   任何跨 CPU 的全局判断都必须遍历所有 CPU 各自的状态，不能只看一个。
5. **固定编号耗尽**：`NR_SOFTIRQS` 是编译期常量，如果需要新增一种 softirq 类型，
   必须修改内核源码里的 `enum` 并重新编译整个内核——这是"极限性能"换来的代价，
   与 15 篇 BPF、05 篇 notifier chain "运行期任意扩展"的能力正好相反。
6. **调试方法**：
   - `/proc/softirqs`：查看每个 CPU 上每种 softirq 的触发次数统计，用来判断
     "是不是某个 CPU 的某类 softirq 异常频繁"。
   - `/proc/stat` 里的 `softirq` 字段：系统级别的总计数。
   - `cat /proc/interrupts` 结合 `mpstat -P ALL 1`：观察 `%soft` 列，判断
     softirq 占用 CPU 时间是否过高（常见于网络高负载场景）。
   - `ftrace` 的 `softirq_entry`/`softirq_exit` tracepoint（真实内核
     `kernel/softirq.c` 里 `trace_softirq_entry(vec_nr)`/`trace_softirq_exit(vec_nr)`
     调用点）：精确测量每种 softirq 单次执行耗时。
   - `ps -eLf | grep ksoftirqd`：查看 `ksoftirqd/N` 线程的 CPU 占用，
     异常高说明存在长期的 softirq 风暴。

---

## 8. Kernel Core Pseudocode（5 个，均取材自本仓库 `/home/morrism/linux/linux` 真实代码）

### Example 1 — `struct softirq_action` 与固定编号 `enum`

真实定义：[include/linux/interrupt.h:450-453](../../include/linux/interrupt.h)

```c
struct softirq_action
{
	void	(*action)(struct softirq_action *);
};
```

真实定义：[include/linux/interrupt.h:425-438](../../include/linux/interrupt.h)

```c
enum
{
	HI_SOFTIRQ=0,
	TIMER_SOFTIRQ,
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	BLOCK_SOFTIRQ,
	BLOCK_IOPOLL_SOFTIRQ,
	TASKLET_SOFTIRQ,
	SCHED_SOFTIRQ,
	HRTIMER_SOFTIRQ,
	RCU_SOFTIRQ,    /* Preferable RCU should always be the last softirq */

	NR_SOFTIRQS
};
```
为什么是 DI：这个契约本身就体现了"编号固定"这条核心约束——`struct softirq_action`
只有一个函数指针字段，比 16 篇 `irqaction`（还有 `next`/`dev_id`/`thread_fn` 等字段）
简单得多，因为它根本不需要支持链表和 Context Injection：**每个编号只能对应一个实现**，
这是 softirq 与 notifier chain/共享中断线最大的结构性区别。

### Example 2 — `open_softirq()`：注册就是"填一个数组元素"

真实位置：[kernel/softirq.c:55](../../kernel/softirq.c)（表本身）、
[kernel/softirq.c:388-391](../../kernel/softirq.c)（注册函数）

```c
static struct softirq_action softirq_vec[NR_SOFTIRQS] __cacheline_aligned_in_smp;

void open_softirq(int nr, void (*action)(struct softirq_action *))
{
	softirq_vec[nr].action = action;
}
```

真实调用点（`kernel/softirq.c:736-737`）：

```c
open_softirq(TASKLET_SOFTIRQ, tasklet_action);
open_softirq(HI_SOFTIRQ, tasklet_hi_action);
```
为什么是 DI：与 01 篇"注册"最大的不同是**没有插入/查找逻辑**——`open_softirq()`
本质就是 `arr[nr] = fn` 这一行赋值，因为 `nr` 在编译期就已知、且保证唯一，
不需要像链表那样考虑"位置"或"冲突"，这是"编号固定"换来的极简实现。

### Example 3 — 每 CPU 独立的 pending 位图

真实定义：[include/linux/irq_cpustat.h:20-26](../../include/linux/irq_cpustat.h)、
[arch/x86/include/asm/hardirq.h:8](../../arch/x86/include/asm/hardirq.h)

```c
/* include/linux/irq_cpustat.h */
extern irq_cpustat_t irq_stat[];		/* defined in asm/hardirq.h */
#define __IRQ_STAT(cpu, member)	(irq_stat[cpu].member)
#define softirq_pending(cpu)	__IRQ_STAT((cpu), __softirq_pending)

/* arch/x86/include/asm/hardirq.h */
typedef struct {
	unsigned int __softirq_pending;
	...
} irq_cpustat_t;

#define local_softirq_pending()	percpu_read(irq_stat.__softirq_pending)
#define set_softirq_pending(x)		percpu_write(irq_stat.__softirq_pending, (x))
#define or_softirq_pending(x)		percpu_or(irq_stat.__softirq_pending, (x))
```

真实调用点（[include/linux/interrupt.h:459-463](../../include/linux/interrupt.h)）：

```c
static inline void __raise_softirq_irqoff(unsigned int nr)
{
	trace_softirq_raise(nr);
	or_softirq_pending(1UL << nr);   /* 只操作当前 CPU 自己的位图 */
}
```
为什么是 DI：这是本篇最独特的部分——"要执行哪个 action"这份状态被**按 CPU 拆分**，
`percpu_softirq_pending.c` 里每个模拟 CPU 拥有自己独立的 `pending` 字段，
正是这个 `percpu_read`/`percpu_write` 机制的用户态还原（只是用了显式的数组+锁
代替真实内核的 per-CPU 变量+关中断，效果等价：每个 CPU 只操心自己的状态）。

### Example 4 — `__do_softirq()`：批量执行的核心循环

真实位置：[kernel/softirq.c:207-267](../../kernel/softirq.c)

```c
asmlinkage void __do_softirq(void)
{
	struct softirq_action *h;
	__u32 pending;
	int max_restart = MAX_SOFTIRQ_RESTART;

	pending = local_softirq_pending();
	...
restart:
	set_softirq_pending(0);       /* 先清空快照，再开中断处理 */
	local_irq_enable();

	h = softirq_vec;
	do {
		if (pending & 1) {
			unsigned int vec_nr = h - softirq_vec;   /* 指针差即下标 */
			...
			h->action(h);          /* <-- 盲调 */
			...
		}
		h++;
		pending >>= 1;
	} while (pending);

	local_irq_disable();
	pending = local_softirq_pending();
	if (pending && --max_restart)
		goto restart;             /* 处理期间又有新的 pending，重试 */
	if (pending)
		wakeup_softirqd();        /* 超过重试上限，移交给 ksoftirqd */
}
```
为什么是 DI：`__do_softirq()`（Core）用**指针运算 + 位移**遍历固定数组，
从不知道 `h->action` 具体是网络驱动还是定时器子系统的实现；这也是
`softirq_fixed_slots.c` 里 `do_softirq_sim()` 用 `for (i = 0; i < NR; i++)`
+ 位测试还原的真实原型，只是真实代码用指针步进（`h++`）而不是数组下标，
两者等价。

### Example 5 — `raise_softirq()` 与 `ksoftirqd` 唤醒判断

真实位置：[kernel/softirq.c:362-391](../../kernel/softirq.c)

```c
inline void raise_softirq_irqoff(unsigned int nr)
{
	__raise_softirq_irqoff(nr);

	/* 如果当前已经处于中断/softirq 上下文，不需要主动唤醒 ksoftirqd，
	 * 因为很快就会自然地走到 do_softirq() 那一步；
	 * 但如果是从普通进程上下文调用 raise_softirq()（比较少见），
	 * 就必须显式唤醒 ksoftirqd，否则这次标记可能永远等不到执行 */
	if (!in_interrupt())
		wakeup_softirqd();
}

void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}
```
为什么是 DI：即便是"标记 pending 这么简单的一步"，也要考虑"谁、在什么上下文
调用"——这正是本篇 Failure 一节里"softirq 仍然假设自己会被及时执行"这条隐含契约
的兜底机制：`raise_softirq()`（Core）不知道调用者是谁，但必须保证"不管谁调用，
这次标记最终都会被处理”，这是"轻量级 DI"背后仍然需要的一点"契约完整性保证"。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/17-softirq/`](c/17-softirq/)

### 9.1 `softirq_fixed_slots.c` — 固定编号表 + open/raise_softirq

- 场景：5 个固定编号的模拟槎位（`HI`/`TIMER`/`NET_TX`/`NET_RX`/`TASKLET`），
  只有 3 个被实际注册了 action，验证"槎位存在但未注册"和"触发多个 pending
  后一次性批量执行"两种情况。
- 编译：
  ```
  cd c/17-softirq
  gcc -Wall -Wextra -O2 -std=gnu11 -o softirq_fixed_slots softirq_fixed_slots.c
  ```
- 实测输出：
  ```
  === softirq_fixed_slots demo ===

  -- 注册阶段（编译期就已经决定有 5 个固定槎位） --
  [open_softirq] slot 2 (NET_TX) <- 0x4012b0
  [open_softirq] slot 3 (NET_RX) <- 0x401290
  [open_softirq] slot 1 (TIMER) <- 0x401270

  -- 触发阶段：模拟硬件事件/软件路径 raise_softirq() --
  [raise_softirq] marked pending: NET_RX (bit 3)
  [raise_softirq] marked pending: TIMER (bit 1)

  -- 统一执行：do_softirq() 一次性处理所有 pending 位 --
  [do_softirq] pending bitmap = 0x0a
    [do_softirq] running slot 1 (TIMER)
      [timer_action] ran expired timer list (run #1)
    [do_softirq] running slot 3 (NET_RX)
      [net_rx_action] drained incoming packet ring (run #1)

  -- 再次触发：这次三个都触发，包括一个从未注册 action 的槎位 --
  [raise_softirq] marked pending: NET_TX (bit 2)
  [raise_softirq] marked pending: NET_RX (bit 3)
  [raise_softirq] marked pending: HI (bit 0)

  -- 统一执行 --
  [do_softirq] pending bitmap = 0x0d
    [do_softirq] slot 0 (HI) pending but no action registered!
    [do_softirq] running slot 2 (NET_TX)
      [net_tx_action] flushed outgoing packet queue (run #1)
    [do_softirq] running slot 3 (NET_RX)
      [net_rx_action] drained incoming packet ring (run #2)

  -- 验证：没有 pending 时 do_softirq() 应该什么都不做 --
  [do_softirq] pending bitmap = 0x00

  === done: net_tx=1, net_rx=2, timer=1 ===
  ```
- 为什么是 DI：`do_softirq_sim()`（Core）按下标遍历固定大小的数组，
  从不知道每个槎位背后是谁；`SIM_HI_SOFTIRQ` 那一行清楚演示了"槎位存在但没有
  实现"是完全合法的状态（对应真实内核里某些 `.config` 关闭了某个子系统，
  它的 softirq 编号依然占着位置，只是永远不会被 `pending`）。

### 9.2 `softirq_vs_hardirq_demo.c` — 硬中断触发 + 批量延后执行

- 场景：稀疏到达（场景 A，每次硬中断后立即处理）与突发到达（场景 B，5 次硬中断
  密集到来才统一处理一次）两种流量模式，验证"批量攒批"确实能把多次硬件事件
  合并成一次 softirq 执行，同时硬中断 handler 本身始终保持极快返回。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o softirq_vs_hardirq_demo softirq_vs_hardirq_demo.c`
- 实测输出：
  ```
  === softirq_vs_hardirq_demo ===

  -- 场景 A：每次硬中断后立即触发一次 irq_exit()（模拟中断稀疏到达） --
    [hardirq] packet #1 arrived, enqueued + raised softirq, hardirq handler took 0 us
    [irq_exit] running pending softirq: NET_RX
      [net_rx_softirq_action] drained 1 packet(s) from ring in one pass (softirq run #1)
    [hardirq] packet #2 arrived, enqueued + raised softirq, hardirq handler took 0 us
    [irq_exit] running pending softirq: NET_RX
      [net_rx_softirq_action] drained 1 packet(s) from ring in one pass (softirq run #2)
    [hardirq] packet #3 arrived, enqueued + raised softirq, hardirq handler took 0 us
    [irq_exit] running pending softirq: NET_RX
      [net_rx_softirq_action] drained 1 packet(s) from ring in one pass (softirq run #3)

  -- 场景 B：连续 5 次硬中断密集到达（模拟突发流量），只在最后统一调用一次 irq_exit() --
    [hardirq] packet #4 arrived, enqueued + raised softirq, hardirq handler took 0 us
    [hardirq] packet #5 arrived, enqueued + raised softirq, hardirq handler took 0 us
    [hardirq] packet #6 arrived, enqueued + raised softirq, hardirq handler took 0 us
    [hardirq] packet #7 arrived, enqueued + raised softirq, hardirq handler took 0 us
    [hardirq] packet #8 arrived, enqueued + raised softirq, hardirq handler took 0 us
    (5 次硬中断已全部返回，ring 里累积了 5 个包，pending 位仍然是 1)
    [irq_exit] running pending softirq: NET_RX
      [net_rx_softirq_action] drained 5 packet(s) from ring in one pass (softirq run #4)

  === done: hardirq_total_fires=8, total_packets_drained=8, net_rx_softirq_runs=4 ===
  ```
  （8 次硬中断只触发了 4 次 softirq 执行，其中场景 B 的 5 次硬中断被合并成 1 次
  批量处理，`drained 5 packet(s) from ring in one pass` 精确验证了"攒批"确实发生。）
- 为什么是 DI：`nic_hardirq_handler()`（硬中断上下文的 Core 逻辑）与
  `net_rx_softirq_action()`（真正的业务处理）被两次独立的盲调分开——
  中间用"标记 pending + 稍后统一执行"解耦，触发次数和执行次数可以不是 1:1，
  这是本篇与 16 篇"每次硬中断触发都对应一次 primary handler 调用"最大的不同。

### 9.3 `percpu_softirq_pending.c` — 每 CPU 独立 pending 位图

- 场景：4 个模拟 CPU（pthread 线程）各自独立触发/处理 50 轮 softirq，
  偶数 CPU 偏重 `NET_RX`、奇数 CPU 偏重 `TIMER`，验证互不干扰（一个 CPU 的
  统计不会"泄漏"到另一个 CPU 头上），且所有 CPU 处理完后 pending 位图必须
  全部清零。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o percpu_softirq_pending percpu_softirq_pending.c`
- 实测输出：
  ```
  === percpu_softirq_pending demo ===

  -- 启动 4 个模拟 CPU，每个各自独立触发/处理 50 轮 softirq --

  -- 每个 CPU 的最终统计（验证互不干扰：偶数 CPU 应以 NET_RX 为主，奇数 CPU 应以 TIMER 为主） --
    CPU0: net_rx_runs=50  timer_runs=17  total_raises=67  final pending=0x00
    CPU1: net_rx_runs=17  timer_runs=50  total_raises=67  final pending=0x00
    CPU2: net_rx_runs=50  timer_runs=17  total_raises=67  final pending=0x00
    CPU3: net_rx_runs=17  timer_runs=50  total_raises=67  final pending=0x00

  -- 验证：所有 CPU 处理完后，pending 位图必须全部清零（不会有一个 CPU 的活漏到另一个 CPU 头上） --
    all_pending_cleared = YES

  === done: total_net_rx=134, total_timer=134, total_raises=268 ===
  ```
  （CPU0/CPU2 的 `net_rx_runs=50` 恰好等于每 CPU 迭代总轮数，`timer_runs=17`
  来自"每 3 轮额外触发一次另一种"的分支逻辑（`50/3` 取整 = 17），CPU1/CPU3
  的数字完全对称互换，精确验证了负载模式按 CPU 独立生效；`all_pending_cleared
  = YES` 验证了每个 CPU 各自处理完自己的 pending 后没有残留。）
- 为什么是 DI：`softirq_vec[]` 这张函数指针表是 4 个线程共享的**同一份只读数据**
  （对应真实内核所有 CPU 共用一份 `softirq_vec[]`），但 `do_softirq_on_cpu()`
  （Core）每次只读写调用者自己那个 `percpu_softirq_state`，从不触碰其它 CPU 的
  状态——这是"共享的行为契约 + 隔离的执行状态"这一设计原则的直接体现。

一次性编译并运行全部三个示例：`cd c/17-softirq && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Softirq (17) | Notifier Chain (05) | Workqueue (07) |
|---|---|---|---|
| 注册数量上限 | **编译期固定**（`NR_SOFTIRQS`，本仓库为 9 个左右） | **运行期任意数量**，链表动态增长 | 运行期任意数量的 `work_struct` |
| 注册数据结构 | **数组**（下标即编号） | 链表 | 队列（内部可能是链表/环形缓冲区） |
| 是否支持注销 | **基本没有**（一旦 `open_softirq`，通常伴随内核生命周期） | 有（`unregister_xxx_notifier`） | 有（work 执行完自然"注销"，也可显式 `cancel_work`） |
| 执行上下文 | 硬中断返回路径 / `ksoftirqd`（仍偏"原子"，不假设能睡眠） | 通常是触发广播的那段代码的上下文 | **进程上下文**，明确允许睡眠 |
| 每次触发 vs 每次执行的关系 | **可以是 N:1**（多次 raise 合并成一次 action 调用） | 通常 1:1（每次广播都完整遍历一次链） | 1:1（每个 work 独立执行一次） |
| 是否有"每 CPU 独立状态" | **是**，pending 位图按 CPU 隔离 | 否，链表本身通常全局共享（需要锁保护并发修改） | 部分是（cmwq 的 `cpu_workqueue_struct` 有 per-CPU 结构，但复杂度远高于 softirq） |
| 性能特征 | **最快**：数组下标 + 位测试，零动态内存分配 | 中等：链表遍历，通常需要锁或 RCU 保护 | 较慢：涉及线程调度、可能的睡眠等待 |
| 何时用 Softirq | 极高频率触发的场景（网络收包、每次时钟 tick），且处理逻辑本身足够轻量、不需要睡眠 | 需要支持运行期任意数量、不同子系统动态订阅同一事件 | 处理逻辑需要睡眠、或耗时较长不适合放在中断路径上 |
| 何时不要用 Softirq | 需要运行期动态增减处理逻辑的场景——softirq 编号一旦耗尽就必须改内核源码重新编译，完全不适合"插件式"需求，应该用 05 篇或 15 篇 | — | — |

一句话：**Softirq 是 DI 谱系里"灵活性"这个维度上的下限，"性能"维度上的上限**——
它用"放弃运行期动态注册/注销能力"换取"数组下标级别的分发开销"和"每 CPU 独立、
天然免锁的并发模型"。18 篇的 Tasklet 会在这张固定表的基础上，重新引入一点点
"动态数量"的灵活性（`tasklet_struct` 本身可以动态创建任意多个，但仍然统一挂在
`TASKLET_SOFTIRQ` 这一个固定槎位下面执行）——本质是"在 softirq 的固定框架内，
用链表模拟出接近 notifier chain 的动态能力，但代价是牺牲掉 softirq 本身
"直接按编号索引"的最快路径，形成本篇与 05/07 两篇之间的一个中间态。
