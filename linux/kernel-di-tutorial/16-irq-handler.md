# 16. IRQ Handler

> 系列索引见 [00-mental-model.md](00-mental-model.md)。本篇讲的是 01 篇 function pointer
> callback 手法在**最苛刻的执行环境**（硬件中断上下文）里的具体应用——同样是"注册一个函数指针，
> 让 Core 到时候盲调”，但调用发生的环境施加了极强的约束，这是本篇要讲清楚的核心差异。

---

## 1. Why

外设（网卡、磁盘、串口、定时器芯片）需要一种方式，在"某个硬件事件发生"（收到一个包、
一次 DMA 完成、一个计时器到期）的那一刻，**立刻**通知 CPU 去处理它，而不是让 CPU
不停地轮询（polling）每一个设备的状态寄存器——轮询在设备很多、事件很稀疏的场景下
是纯粹的 CPU 浪费。

如果没有中断处理这套机制，只能靠：

1. **忙等轮询**：CPU 在一个死循环里反复检查所有设备的状态寄存器，事件响应延迟不可控，
   且严重浪费 CPU 时间——现代系统里外设数量动辄几十个，轮询开销无法接受。
2. **每个设备驱动自己实现一套私有的事件通知机制**：完全没有统一性，每加一种硬件平台，
   都要重新设计一套"如何让 CPU 及时响应"的机制。

Kernel 的设计目标是：

> **提供一套统一的、体系结构无关的"硬件事件 -> 软件回调"注入接口
> （`request_irq`/`request_threaded_irq`/`free_irq`），驱动只需要提交一个函数指针，
> 中断控制器/CPU 中断向量表的细节完全被 Core 屏蔽；同一个 IRQ 号还可以被多个设备共享，
> 由 Core 负责依次分发，每个 handler 自己判断"这次中断是不是我的”。**

与 01 篇最原始的 callback 相比，中断处理的特殊之处在于**执行环境的硬约束**：
handler 是在**硬件中断上下文**（禁止睡眠、禁止调用可能阻塞的接口、栈空间极小、
必须尽快返回）里被调用的——这对回调实现方式施加了远比"普通函数指针回调"更严格的规则，
这正是本篇要重点展开的部分。

---

## 2. Architecture

```
                         硬件设备（网卡/串口/磁盘...）
                                    |
                                    | 电平/边沿变化，触发中断线
                                    v
                        中断控制器 (APIC/GIC/PLIC...)
                                    |
                                    | CPU 收到中断，跳转到中断向量
                                    v
        +--------------------------------------------------------------+
        |                  Core: irq_desc / irqaction 链                |
        |                                                                |
        |   struct irq_desc {                                           |
        |       struct irqaction *action;   <-- 契约：单链表头            |
        |       ...                                                     |
        |   }                                                           |
        |                                                                |
        |   struct irqaction {                                         |
        |       irq_handler_t handler;      <-- 硬中断上下文回调           |
        |       irq_handler_t thread_fn;    <-- 可选：进程上下文回调        |
        |       void *dev_id;               <-- Context Injection        |
        |       struct irqaction *next;     <-- 共享中断线                |
        |   }                                                           |
        +--------------------------------------------------------------+
                                    |
                     handle_irq_event_percpu() 遍历链表逐一盲调
                                    v
        +----------------+   +----------------+   +----------------+
        |  驱动 A handler |   |  驱动 B handler |   |  驱动 C handler |
        |  (硬中断上下文) |   |  (硬中断上下文) |   |  (硬中断上下文) |
        +----------------+   +----------------+   +----------------+
              |  返回 IRQ_WAKE_THREAD（可选）
              v
        +----------------+
        |  irq_thread()   |   <-- 独立的 kthread，进程上下文，可以睡眠
        |  thread_fn(...) |
        +----------------+
```

对应到本文档的三个用户态示例：

```
irq_action_chain_sim:      irq4.action -> ttyS0.action -> ttyS1.action  （共享中断线链式分发）
threaded_irq_demo:         nic_primary_handler (快) -> nic_threaded_handler (慢，独立线程)
irq_free_and_race_demo:    handle_irq_event_sim() + synchronize_irq_sim()  （卸载竞态）
```

---

## 3. Lifecycle

```
   allocate                  (driver 在 probe() 里准备好 dev_id 私有数据)
        |
        v
   initialize                (设置好 handler/thread_fn 函数指针，flags 如 IRQF_SHARED)
        |
        v
   inject (request_irq /      request_threaded_irq(irq, handler, thread_fn,
           request_threaded_irq)  flags, name, dev) -> 内部 __setup_irq() 插入 desc->action 链表)
        |
        v
   framework owns dispatch    (硬件触发中断，CPU 跳转到 Core 的中断入口，
                                Core 决定"何时"调用，driver 不控制时机)
        |
        v
   callback (硬中断上下文)     (handle_irq_event_percpu() 遍历链表，逐个调用
                                action->handler(irq, dev_id)，必须极快返回)
        |
        v
   [可选] callback (进程上下文) (若 handler 返回 IRQ_WAKE_THREAD，唤醒专属 kthread，
                                在其中调用 thread_fn(irq, dev_id)，可以睡眠)
        |
        v
   destroy (free_irq)          (从链表摘除 action；调用 synchronize_irq() 等待
                                当前正在执行的 handler/thread 结束；
                                停止并回收 kthread；driver 之后才能安全释放 dev_id)
```

三个示例里分别是：

- `irq_action_chain_sim`：`setup_irq_sim()` -> `handle_irq_event_percpu_sim()`（可能多次）-> `free_irq_sim()`
- `threaded_irq_demo`：`request_threaded_irq_sim()` -> `fire_hardware_irq()`（多次，primary 触发线程）-> `free_threaded_irq_sim()`
- `irq_free_and_race_demo`：`handle_irq_event_sim()`（正在执行）与 `buggy_free_irq()`/`safe_free_irq()` 并发竞争

---

## 4. Relationship

```
   驱动 probe()（注册方）
        |  持有
        v
   struct irqaction（Core 定义类型，driver 分配并填充字段）
        |  包含
        v
   handler / thread_fn 函数指针 + dev_id（Context Injection）
        |  被插入
        v
   irq_desc->action 单链表（Core 拥有，支持多个 irqaction 挂在同一个 IRQ 号上）
        |  在硬件触发时
        v
   handle_irq_event_percpu()（Core，遍历链表，逐一盲调，不知道具体是谁的实现）
        |  handler 返回 IRQ_WAKE_THREAD 时
        v
   irq_thread()（Core 为每个 irqaction 创建的专属 kthread）
        |  盲调
        v
   thread_fn（真正的"重活"实现，进程上下文）
```

关键点：**同一个 IRQ 号上可以同时存在多个 irqaction（共享中断线），
但每个 irqaction 自己的 handler/thread_fn 对是完全独立的一对函数指针**——
这与 05 篇 notifier chain "一对多广播"结构相似，但触发源是硬件而不是软件事件，
且返回值语义（`IRQ_HANDLED`/`IRQ_NONE`）承担着"认领"这次中断的作用，
这是 notifier chain 没有的语义。

---

## 5. Data Flow

以 `threaded_irq_demo.c` 为例，完整调用链：

```
main()
  |
  |-- request_threaded_irq_sim(&act)     // 注入：primary + thread_fn 两个回调
  |     `-- pthread_create(irq_thread_main)   // 对应真实 kthread_create(irq_thread, ...)
  |
  `-- for i in 1..5:
        fire_hardware_irq(&act)           // 模拟硬件触发
          |
          v
        act->primary(irq, dev_id)         // <-- DI 发生点 1：硬中断上下文盲调，必须极快
          |  返回 IRQ_WAKE_THREAD
          v
        pthread_cond_signal(&act->cond)   // 对应 wake_up_process(action->thread)
          |
          v （异步，在 irq_thread_main 线程里）
        act->thread_fn(irq, dev_id)       // <-- DI 发生点 2：进程上下文盲调，可以睡眠
```

对照真实 Kernel 调用链（`kernel/irq/handle.c`/`kernel/irq/manage.c`，本仓库真实代码）：

```
硬件触发 -> CPU 中断向量 -> 通用中断入口
  |
  v
handle_irq_event(desc)                         // kernel/irq/handle.c:166
  |
  v
handle_irq_event_percpu(desc, action)          // kernel/irq/handle.c:117
  |
  |  do { ... action = action->next; } while (action);
  v
res = action->handler(irq, action->dev_id)     // kernel/irq/handle.c:126   <-- 盲调 1
  |
  |  if (res == IRQ_WAKE_THREAD)
  v
irq_wake_thread(desc, action)                  // kernel/irq/handle.c:54，内部 wake_up_process()
  |
  v （在 irq_thread() 内核线程里异步发生）
handler_fn = irq_thread_fn;                    // kernel/irq/manage.c:750-760
action_ret = handler_fn(desc, action)
  |
  v
action->thread_fn(action->irq, action->dev_id) // kernel/irq/manage.c:757   <-- 盲调 2
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `irqaction.handler`/`thread_fn` | **注册后只读** | `request_irq()` 之后 Core 只读取并调用，不会修改 |
| `irqaction.dev_id` | **驱动自己维护的私有状态可变，指针本身注册后不变** | Context Injection 的典型用法 |
| `irq_desc->action` 链表 | **高度动态** | `request_irq()`/`free_irq()` 随时增删链表节点，且必须在持锁的情况下操作（真实内核 `desc->lock`） |
| `IRQD_IRQ_INPROGRESS`（本文示例里的 `in_progress`） | **每次进入/退出 handler 时翻转** | `synchronize_irq()` 正是靠这个标志位判断"现在能不能安全释放资源" |
| `desc->threads_active` | **每个线程化 handler 执行期间自增，结束自减** | `synchronize_irq()` 在硬中断部分完成后，还要等这个计数归零才返回 |

不变的是"契约"本身：`irq_handler_t` 的函数签名 `irqreturn_t (*)(int, void *)`，
以及"硬中断上下文里绝不能睡眠"这条铁律——这条铁律不是靠类型系统强制的，
而是靠**约定 + `lockdep`/`might_sleep()` 之类的运行期检测**来保证的。

---

## 7. Failure

1. **在硬中断上下文里睡眠**：如果 handler（不是 thread_fn）里调用了会睡眠的函数
   （比如获取一个可能阻塞的 mutex、调用 `kmalloc(GFP_KERNEL)`），会导致
   `BUG: scheduling while atomic` 或者更隐蔽的死锁——这是本篇与 07 篇 workqueue
   最大的区别：workqueue 的业务函数运行在进程上下文，允许睡眠；IRQ 的 primary handler
   绝对不允许。
2. **共享中断线上返回值语义不一致**：如果某个 handler 明明不是自己的中断却错误地返回
   `IRQ_HANDLED`，会让其它真正应该处理这次中断的 handler 被跳过（虽然真实内核不会因为
   一个 `IRQ_HANDLED` 就提前退出循环，但错误的"认领"会污染 `note_interrupt()` 的统计，
   掩盖真正的硬件故障）——`irq_action_chain_sim.c` 里每个 handler 严格依赖自己的硬件寄存器
   （`rx_pending`）判断，绝不臆测。
3. **`free_irq()` 时序竞态**：如果驱动在 `free_irq()` 返回后立即释放 `dev_id` 指向的内存，
   但 `free_irq()` 内部没有正确等待"正在执行中的 handler/thread"结束，就会出现
   use-after-free——`irq_free_and_race_demo.c` 用 magic 校验值实测复现了这个竞态
   （`buggy_free_irq` 版本 100% 检测到 `USE-AFTER-FREE`，`safe_free_irq` 版本因为
   调用了 `synchronize_irq_sim()` 而 100% 安全）。
4. **`IRQF_SHARED` 标志不匹配**：真实内核 `__setup_irq()` 要求共享同一 IRQ 号的所有
   `irqaction` 必须都设置 `IRQF_SHARED`、触发方式（level/edge）必须一致，否则拒绝注册
   （对应 `kernel/irq/manage.c` 里的 `mismatch` 分支）——这是"契约"里除了函数签名之外，
   还必须满足的一组"协议级"前提条件。
5. **虚假中断（spurious interrupt）风暴**：如果硬件本身有问题，反复触发中断但没有一个
   handler 认领（全部返回 `IRQ_NONE`），真实内核的 `note_interrupt()` 会在超过阈值后
   自动关闭这个 IRQ，防止系统被中断风暴拖死——`irq_action_chain_sim.c` 用
   `spurious_count` 简化模拟了这个统计。
6. **调试方法**：
   - `/proc/interrupts`：查看每个 IRQ 号触发次数、绑定的 CPU、共享该 IRQ 的所有设备名，
     判断"中断是否真的按预期触发"。
   - `cat /proc/irq/<N>/spurious`：查看某个 IRQ 号的虚假中断统计。
   - `lockdep`：检测在硬中断上下文里误用了可能睡眠的锁（`might_sleep()` 断言）。
   - `WARN_ONCE(!irqs_disabled(), ...)`：真实内核 `handle_irq_event_percpu()` 里就有
     这样一个检查，确保 handler 返回时中断确实还是关闭状态（`kernel/irq/handle.c:129`）。
   - `ftrace` 的 `irq_handler_entry`/`irq_handler_exit` tracepoint：可以精确测量每个
     handler 的执行耗时，识别"硬中断上下文里跑得太久"的问题驱动。

---

## 8. Kernel Core Pseudocode（5 个，均取材自本仓库 `/home/morrism/linux/linux` 真实代码）

### Example 1 — `struct irqaction` 与 `request_irq()`

真实定义：[include/linux/interrupt.h:110-123](../../include/linux/interrupt.h)

```c
struct irqaction {
	irq_handler_t		handler;
	unsigned long		flags;
	void			*dev_id;
	void __percpu		*percpu_dev_id;
	struct irqaction	*next;
	int			irq;
	irq_handler_t		thread_fn;
	struct task_struct	*thread;
	unsigned long		thread_flags;
	unsigned long		thread_mask;
	const char		*name;
	struct proc_dir_entry	*dir;
} ____cacheline_internodealigned_in_smp;
```

`request_irq()` 是 `request_threaded_irq()` 的简化包装（[include/linux/interrupt.h:133-138](../../include/linux/interrupt.h)）：

```c
static inline int __must_check
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
	    const char *name, void *dev)
{
	return request_threaded_irq(irq, handler, NULL, flags, name, dev);
}
```
为什么是 DI：`irqaction` 就是被注入的"契约对象"，`handler`/`thread_fn` 是两个独立的
函数指针字段，`dev_id` 是 Context Injection；`next` 字段让它天然支持"共享中断线"
这种一对多结构，不需要额外的数据结构。

### Example 2 — `request_threaded_irq()`：把 handler/thread_fn 填进新分配的 irqaction

真实位置：[kernel/irq/manage.c:1317-1385](../../kernel/irq/manage.c)

```c
int request_threaded_irq(unsigned int irq, irq_handler_t handler,
			 irq_handler_t thread_fn, unsigned long irqflags,
			 const char *devname, void *dev_id)
{
	struct irqaction *action;
	struct irq_desc *desc;
	...
	action = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
	...
	action->handler = handler;
	action->thread_fn = thread_fn;
	action->flags = irqflags;
	action->name = devname;
	action->dev_id = dev_id;

	chip_bus_lock(desc);
	retval = __setup_irq(irq, desc, action);   /* 真正插入链表在这里 */
	chip_bus_sync_unlock(desc);
	...
	return retval;
}
```
为什么是 DI：这个函数（Core）从不知道 `handler`/`thread_fn` 具体做什么，
只负责把驱动填好的 `irqaction` 交给 `__setup_irq()` 去挂到 `irq_desc` 上。

### Example 3 — `__setup_irq()`：共享中断线的挂载与线程创建

真实位置：[kernel/irq/manage.c:879-992](../../kernel/irq/manage.c)（节选）

```c
static int
__setup_irq(unsigned int irq, struct irq_desc *desc, struct irqaction *new)
{
	...
	/* 如果提供了 thread_fn，且不是 nested irq，就为这个 irqaction
	 * 创建一个专属的处理线程 —— 这正是 threaded_irq_demo.c 里
	 * pthread_create(irq_thread_main, act) 的原型 */
	if (new->thread_fn && !nested) {
		struct task_struct *t;

		t = kthread_create(irq_thread, new, "irq/%d-%s", irq, new->name);
		...
		get_task_struct(t);
		new->thread = t;
	}
	...
	raw_spin_lock_irqsave(&desc->lock, flags);
	old_ptr = &desc->action;
	old = *old_ptr;
	if (old) {
		/* 共享中断线校验：flags 必须都带 IRQF_SHARED，触发类型必须一致 */
		if (!((old->flags & new->flags) & IRQF_SHARED) ||
		    ((old->flags ^ new->flags) & IRQF_TRIGGER_MASK) ||
		    ((old->flags ^ new->flags) & IRQF_ONESHOT)) {
			old_name = old->name;
			goto mismatch;
		}
		/* add new interrupt at end of irq queue */
		do {
			thread_mask |= old->thread_mask;
			old_ptr = &old->next;
			old = *old_ptr;
		} while (old);
		shared = 1;
	}
	...
}
```
为什么是 DI：这就是 `irq_action_chain_sim.c` 里 `setup_irq_sim()`
（"插入链表末尾"）的真实原型；更进一步，`__setup_irq()` 还在"注册时机"
（而不是"调用时机"）就完成了一次策略校验（共享标志/触发类型必须一致），
这是"契约"不仅约束函数签名、还约束运行期协议参数的例子。

### Example 4 — `handle_irq_event_percpu()`：硬中断上下文里的链式盲调

真实位置：[kernel/irq/handle.c:117-164](../../kernel/irq/handle.c)（节选）

```c
irqreturn_t
handle_irq_event_percpu(struct irq_desc *desc, struct irqaction *action)
{
	irqreturn_t retval = IRQ_NONE;
	unsigned int random = 0, irq = desc->irq_data.irq;

	do {
		irqreturn_t res;

		trace_irq_handler_entry(irq, action);
		res = action->handler(irq, action->dev_id);      /* <-- 盲调 */
		trace_irq_handler_exit(irq, action, res);

		if (WARN_ONCE(!irqs_disabled(), "irq %u handler %pF enabled interrupts\n",
			      irq, action->handler))
			local_irq_disable();

		switch (res) {
		case IRQ_WAKE_THREAD:
			if (unlikely(!action->thread_fn)) {
				warn_no_thread(irq, action);
				break;
			}
			irq_wake_thread(desc, action);   /* wake_up_process() */
			/* fall through */
		case IRQ_HANDLED:
			random |= action->flags;
			break;
		default:
			break;
		}

		retval |= res;
		action = action->next;    /* 遍历共享中断线上所有 irqaction */
	} while (action);
	...
	return retval;
}
```
为什么是 DI：这就是 `irq_action_chain_sim.c` 里 `handle_irq_event_percpu_sim()`
的真实原型——注意它是 `do { ... } while (action)`，**不会因为某个 action 返回了
`IRQ_HANDLED` 就提前退出**，必须让链上每一个都有机会认领，这是共享中断线的核心语义，
本篇示例严格照抄了这个行为。

### Example 5 — `irq_thread()` / `irq_thread_fn()`：进程上下文里的第二段盲调

真实位置：[kernel/irq/manage.c:750-826](../../kernel/irq/manage.c)（节选）

```c
static irqreturn_t irq_thread_fn(struct irq_desc *desc, struct irqaction *action)
{
	irqreturn_t ret;

	ret = action->thread_fn(action->irq, action->dev_id);   /* <-- 盲调，进程上下文 */
	irq_finalize_oneshot(desc, action, false);
	return ret;
}

static int irq_thread(void *data)
{
	struct irqaction *action = data;
	struct irq_desc *desc = irq_to_desc(action->irq);
	irqreturn_t (*handler_fn)(struct irq_desc *, struct irqaction *);
	...
	handler_fn = irq_thread_fn;
	...
	while (!irq_wait_for_interrupt(action)) {     /* 对应示例里的 pthread_cond_wait */
		...
		action_ret = handler_fn(desc, action);
		if (!noirqdebug)
			note_interrupt(action->irq, desc, action_ret);
		...
	}
	return 0;
}
```
为什么是 DI：`irq_thread()`（Core 为每个 threaded irqaction 创建的通用线程主体）
完全不知道 `action->thread_fn` 具体做什么，只负责"等待唤醒 -> 盲调 -> 循环"这套
通用骨架——这正是 `threaded_irq_demo.c` 里 `irq_thread_main()` 的真实原型，
且这里的"等待唤醒"（`irq_wait_for_interrupt`）与硬中断的"触发/合并"语义
（多次唤醒被合并成一次线程执行）在真实内核里也是同样的行为。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/16-irq-handler/`](c/16-irq-handler/)

### 9.1 `irq_action_chain_sim.c` — 共享中断线的链式分发

- 场景：`ttyS0`/`ttyS1` 两个模拟串口共享 IRQ 4，每次"硬件触发"都遍历整条链，
  每个 handler 根据自己的 `rx_pending` 硬件寄存器判断是否认领，未被任何人认领时
  触发 spurious 警告；`free_irq_sim()` 卸载后验证已卸载设备不再被分发到。
- 编译：
  ```
  cd c/16-irq-handler
  gcc -Wall -Wextra -O2 -std=gnu11 -o irq_action_chain_sim irq_action_chain_sim.c
  ```
- 实测输出：
  ```
  === irq_action_chain_sim demo ===

  [setup_irq] registered 'ttyS0' on IRQ 4
  [setup_irq] registered 'ttyS1' on IRQ 4

  -- 场景1：只有 ttyS0 有数据到达 --
  [core] hardware IRQ 4 fired, walking action chain:
      [ttyS0] servicing RX interrupt, clearing pending bit
    [1] 'ttyS0' returned IRQ_HANDLED
    [2] 'ttyS1' returned IRQ_NONE

  -- 场景2：只有 ttyS1 有数据到达 --
  [core] hardware IRQ 4 fired, walking action chain:
    [1] 'ttyS0' returned IRQ_NONE
      [ttyS1] servicing RX interrupt, clearing pending bit
    [2] 'ttyS1' returned IRQ_HANDLED

  -- 场景3：两个设备同时有数据到达（真实硬件上很常见） --
  [core] hardware IRQ 4 fired, walking action chain:
      [ttyS0] servicing RX interrupt, clearing pending bit
    [1] 'ttyS0' returned IRQ_HANDLED
      [ttyS1] servicing RX interrupt, clearing pending bit
    [2] 'ttyS1' returned IRQ_HANDLED

  -- 场景4：虚假中断（两个设备都没有 pending，触发 spurious 警告） --
  [core] hardware IRQ 4 fired, walking action chain:
    [1] 'ttyS0' returned IRQ_NONE
    [2] 'ttyS1' returned IRQ_NONE

  -- 卸载 ttyS0 驱动 --
  [free_irq] removed 'ttyS0' from IRQ 4

  -- 场景5：ttyS0 已卸载，即使它的硬件寄存器意外置位也不会再被处理到 --
  [core] hardware IRQ 4 fired, walking action chain:
      [ttyS1] servicing RX interrupt, clearing pending bit
    [1] 'ttyS1' returned IRQ_HANDLED

  === done, spurious_count=1 ===
  ```
  （stderr 的 spurious 警告行在真实终端上会与 stdout 交错先输出，属于缓冲交错，
  不影响逻辑：`spurious_count` 最终精确为 1，对应场景 4。）
- 为什么是 DI：`handle_irq_event_percpu_sim()`（Core）永远不知道链上挂的是
  串口还是网卡，只认 `irq_handler_fn` 这个签名和 `IRQ_HANDLED`/`IRQ_NONE`
  这套返回值协议；新增第三个共享设备只需要 `setup_irq_sim()` 多插一个节点，
  不需要改分发逻辑一行代码。

### 9.2 `threaded_irq_demo.c` — 两段式处理：primary（硬中断）+ thread_fn（进程上下文）

- 场景：模拟网卡 `eth0` 在 100ms 内密集触发 5 次中断（每 10ms 一次），
  `primary handler` 每次都在微秒级返回（硬中断上下文的铁律），真正耗时的收包处理
  （40ms/次）被丢给独立线程异步执行，硬件触发速度超过线程处理速度时自动"合并"
  （coalesce）多次唤醒为一次线程执行——这正是电平触发中断"服务前一直有效"的直觉。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o threaded_irq_demo threaded_irq_demo.c`
- 实测输出：
  ```
  === threaded_irq_demo ===

  [request_threaded_irq] registered 'eth0' on IRQ 11 (primary=0x401460, thread_fn=0x401470)

  -- 模拟硬件在 100ms 内密集触发 5 次中断（每 10ms 一次），但每次线程处理需要 40ms，必然发生合并 --
    [hw] fired interrupt #1, primary handler returned in 2 us (hard-irq context must be fast)
    [thread:eth0] woke up, servicing (coalesced 1 hardware fire(s) into 1 pass)
    [hw] fired interrupt #2, primary handler returned in 0 us (hard-irq context must be fast)
    [hw] fired interrupt #3, primary handler returned in 0 us (hard-irq context must be fast)
    [hw] fired interrupt #4, primary handler returned in 0 us (hard-irq context must be fast)
      [eth0] finished processing packet batch #1
    [thread:eth0] woke up, servicing (coalesced 3 hardware fire(s) into 1 pass)
    [hw] fired interrupt #5, primary handler returned in 0 us (hard-irq context must be fast)

  -- 等待线程处理完所有已排队的工作 --
      [eth0] finished processing packet batch #2
    [thread:eth0] woke up, servicing (coalesced 1 hardware fire(s) into 1 pass)
      [eth0] finished processing packet batch #3

  -- 卸载中断线程 --
  [free_irq] 'eth0' thread stopped (fired=5, serviced=3, coalesced=2)

  === done, eth0.packets_processed=3 ===
  ```
  （`primary handler` 每次返回都是 0~2 微秒级别，验证了"硬中断上下文必须极快"；
  5 次硬件触发只对应 3 次真正的线程处理，`coalesced=2` 精确记录了被合并掉的 2 次
  唤醒——这就是"硬件触发速率 > 软件处理速率"时的真实行为。）
- 为什么是 DI：`irq_thread_main()`（Core）从不知道 `nic_threaded_handler` 具体
  怎么处理包，只负责"等待唤醒 -> 调用 -> 循环"；`fire_hardware_irq()`（Core）
  也从不知道 `nic_primary_handler` 的判断逻辑，只认 `IRQ_WAKE_THREAD` 这个返回值——
  两段回调各自独立注入，各自独立运行在不同的执行上下文里。

### 9.3 `irq_free_and_race_demo.c` — `free_irq()` 与竞态的实测复现

- 场景：分别实现"不调用 `synchronize_irq()`"和"调用 `synchronize_irq()`"两个版本的
  `free_irq()`，用一个 `magic` 校验值量化检测"释放操作是否与正在执行的 handler 产生竞态”，
  已连续运行 3 次验证结果稳定（错误版本 100% 检测到 use-after-free，正确版本 100% 安全）。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o irq_free_and_race_demo irq_free_and_race_demo.c`
- 实测输出：
  ```
  === irq_free_and_race_demo ===

  == 场景1：buggy_free_irq（不调用 synchronize_irq，存在竞态） ==
      [handler] start, dev='demo-dev', magic=0xa1a1a1a1 (expect ALIVE=0xa1a1a1a1)
    [buggy_free_irq] unlinking action WITHOUT calling synchronize_irq()
    [buggy_free_irq] freeing dev_id memory NOW (handler may still be running!)
      [handler] !!! USE-AFTER-FREE DETECTED !!! magic is now 0xdeaddead, memory was freed while handler was still running
  == 场景1：buggy_free_irq（不调用 synchronize_irq，存在竞态）: scenario complete ==

  == 场景2：safe_free_irq（调用 synchronize_irq，竞态被消灭） ==
      [handler] start, dev='demo-dev', magic=0xa1a1a1a1 (expect ALIVE=0xa1a1a1a1)
    [safe_free_irq] unlinking action
    [safe_free_irq] calling synchronize_irq(), blocking until in-flight handler done...
      [handler] finished safely, magic still 0xa1a1a1a1
    [safe_free_irq] synchronize_irq() returned, now safe to free dev_id memory
  == 场景2：safe_free_irq（调用 synchronize_irq，竞态被消灭）: scenario complete ==

  === done ===
  ```
- 为什么是 DI：这个示例演示的不是"注入本身”，而是"注入的生命周期管理"——
  `handle_irq_event_sim()`（Core）盲调 `action->handler`，同时用 `in_progress`
  标志把"这次调用还没返回"这个事实暴露给 `synchronize_irq_sim()`；
  `free_irq()` 必须先查询这个状态、等待它清零，才能安全地让调用方回收被注入的
  `dev_id` 内存——这正是 03 篇 Context Injection 提到的"回调生命周期比 Core 短"
  这类 Failure 场景在 IRQ 场景下的具体、可复现的实例。

一次性编译并运行全部三个示例：`cd c/16-irq-handler && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | IRQ Handler (16) | Workqueue (07) | Notifier Chain (05) |
|---|---|---|---|
| 触发源 | **硬件**（外设电气信号） | 软件（`schedule_work()` 调用点） | 软件（`notifier_call_chain()` 调用点） |
| 执行上下文 | **硬中断上下文**（primary handler）+ 可选进程上下文（thread_fn） | 进程上下文（worker 线程） | 调用者所在上下文（通常是触发事件的那段代码） |
| 是否允许睡眠 | primary handler **绝对不允许**；thread_fn 允许 | **允许**（这是它存在的核心原因） | 视调用者上下文而定，通常不假设可以睡眠 |
| 多实现是否可以同时挂载 | **是**，共享中断线，链式遍历 | 不适用（work 是一次性任务，不是"多个实现挂在一个点") | **是**，天然设计成链表 |
| "认领"语义 | **有**：`IRQ_HANDLED`/`IRQ_NONE` 决定是否认领这次中断 | 无——work 提交后必然会被执行一次 | 部分场景有"短路"语义（如 `NOTIFY_STOP_MASK`），但没有"认领"概念 |
| 生命周期管理关键点 | `synchronize_irq()`：等待正在执行的 handler/thread 结束才能安全释放资源 | `flush_workqueue()`：等待队列清空 | 通常靠调用者自己保证订阅者存活周期覆盖广播窗口 |
| 何时用 IRQ Handler | 需要响应真实硬件事件，且响应延迟必须尽可能低 | 需要把耗时操作推迟到可睡眠的执行环境 | 需要在软件事件发生时同步通知多个订阅者 |
| 何时不要用 IRQ Handler | 任何不是由硬件直接触发的场景——软件内部的"事件通知"应该用 05 篇 notifier chain 或自定义回调，绝不应该借用中断机制 | — | — |

一句话：**IRQ Handler 是"硬件强制的、带执行上下文硬约束的、支持共享认领语义的回调注入”**——
它是 01 篇 callback 手法里约束最强的一个特化：不仅要满足"函数指针签名契约”，
还必须满足"执行环境契约"（能不能睡眠）和"协议语义契约"（`IRQ_HANDLED`/`IRQ_NONE`
如何影响共享链上其它 handler）。下一篇 [17-softirq.md](17-softirq.md) 会展示
"把硬中断里不能做的重活往后推”的另一种手法——与本篇 threaded IRQ 的"线程化"不同，
softirq 走的是"编译期固定编号、每 CPU 独立执行"的路线，灵活性更低但性能更高。
