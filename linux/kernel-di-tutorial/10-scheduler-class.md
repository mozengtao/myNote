# 10. Scheduler Class

> 02 篇讲了 Ops Table——一组回调打包成结构体，等价手写 vtable。
> 05 篇讲了 Notifier Chain——多个订阅者串成链，依次尝试直到"短路"。
> 本篇要讲的 `struct sched_class` 是两者的结合体：**它本身是一份 Ops Table
> （`enqueue_task`/`pick_next_task`/...），但多个 Ops Table 实例又按优先级串成一条链，
> 调度器沿着链依次询问"你这里有没有活要干"，直到有人接受为止。**

---

## 1. Why

一个 CPU 在任意时刻要从"可能存在的所有可运行任务"里选出下一个该运行的任务。但 Linux 从来
不是只有一种任务类型：

- 需要绝对优先、不能被任何普通任务打断的任务（`stop_machine` 这类特殊内核任务）
- 硬实时任务（`SCHED_FIFO`/`SCHED_RR`，需要严格的优先级保证，绝不能被"公平"调度稀释）
- 绝大多数普通任务（`SCHED_OTHER`，需要在多个任务间"公平"分配 CPU 时间）
- CPU 上没有任何任务可运行时的兜底（`idle` 任务，不能不存在）

如果不用某种可插拔机制，调度器核心代码必须写成：

```c
if (has_stop_task())      pick_stop_task();
else if (has_rt_task())    pick_rt_task_by_priority();
else if (has_fair_task())  pick_fair_task_by_vruntime();
else                        pick_idle_task();
/* 以后想加一个新的调度策略（比如 Deadline），又要回来改这段 if-else */
```

这不仅违反开闭原则，更严重的是：**"实时任务用什么算法选择"和"公平任务用什么算法选择"
是两个完全独立、由不同团队维护、复杂度都很高的算法（RT 用位图找最高优先级，
CFS 用红黑树找最小 vruntime）**，写在一起会让调度器核心变成一个不可维护的巨石函数。

设计目标：

> **把"一种调度策略"抽象成一份完整的 Ops Table（`sched_class`），多个策略按固定优先级
> 用 `next` 指针串成一条链；调度核心（`pick_next_task()`）不实现任何具体调度算法，
> 只负责沿链依次询问每个策略"你要不要接管这次调度"，第一个愿意接管的就赢。**

这正是本系列前两个手法的合体：**Ops Table 提供"一种策略的完整行为集合"，
Notifier Chain 提供"依次尝试直到有人处理"的链式结构**——只是 sched_class 链上的语义
是"接受即停止"（类似 05 篇的 `NOTIFY_STOP_MASK`，但语义反过来：这里是"谁先说 YES 就归谁"，
而非"谁说 NO 就否决全部"）。

---

## 2. Architecture

```
                       __schedule() / pick_next_task()
                    （Core，完全不知道 RT/CFS 内部算法）
                               |
                               | for_each_class(class)
                               v
   sched_class_highest
   (&stop_sched_class)
        |
        | .next
        v
   +-------------------+     +-------------------+     +-------------------+     +-------------------+
   |  stop_sched_class |---->|   rt_sched_class   |---->|  fair_sched_class |---->|  idle_sched_class |--> NULL
   |  (最高优先级)       |.next|  (SCHED_FIFO/RR)  |.next|  (SCHED_OTHER/CFS)|.next|   (兜底)           |
   |                    |     |                    |     |                    |     |                    |
   | enqueue_task       |     | enqueue_task       |     | enqueue_task       |     | enqueue_task       |
   | dequeue_task       |     | dequeue_task       |     | dequeue_task       |     | dequeue_task       |
   | pick_next_task     |     | pick_next_task     |     | pick_next_task     |     | pick_next_task     |
   | put_prev_task      |     | put_prev_task      |     | put_prev_task      |     | put_prev_task      |
   +-------------------+     +-------------------+     +-------------------+     +-------------------+
        |  pick_next_task() 返回 NULL 才继续下一个，返回非 NULL 立即停止（"谁先接受就归谁"）
        v
   任务被选中，成为 rq->curr
```

真实定义：[include/linux/sched.h:1084](../../include/linux/sched.h)

```c
struct sched_class {
	const struct sched_class *next;    /* 链式指针，链的顺序编译期固定 */

	void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
	void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
	struct task_struct * (*pick_next_task) (struct rq *rq);
	void (*put_prev_task) (struct rq *rq, struct task_struct *p);
	void (*task_tick) (struct rq *rq, struct task_struct *p, int queued);
	void (*switched_to) (struct rq *this_rq, struct task_struct *task);
	/* ... 共约 20 个回调，篇幅原因不全部列出 ... */
};
```

链的顺序由每个具体调度类**自己在静态初始化时**填好 `.next`（本仓库真实代码）：

```
kernel/sched_stoptask.c:84   stop_sched_class.next = &rt_sched_class;
kernel/sched_rt.c:1804       rt_sched_class.next   = &fair_sched_class;
kernel/sched_fair.c:5045     fair_sched_class.next = &idle_sched_class;
kernel/sched_idletask.c      idle_sched_class 没有 next（链尾，等价 NULL）
```

链头由宏固定：[kernel/sched.c:1909](../../kernel/sched.c) `#define sched_class_highest (&stop_sched_class)`。

---

## 3. Lifecycle

```
   allocate
   (每种调度类在编译期定义一个 static const struct sched_class xxx_sched_class = {...}；
    这一步与 02 篇 Ops Table 完全一致——ops 本身没有运行期分配)
        |
        v
   initialize
   (.next 字段在源码里静态指定好链的顺序：stop -> rt -> fair -> idle)
        |
        v
   inject（任务归属某个 class）
   (task_struct->sched_class = &fair_sched_class 这一行，在 sched_fork()/
    sched_setscheduler() 时被赋值——"注入"的对象不是调度类本身，而是
    "一个任务归属于哪个调度类"这条绑定关系)
        |
        v
   framework owns dispatch
   (__schedule() 调用 pick_next_task(rq)，沿链依次调用每个 class->pick_next_task(rq)，
    Core 完全不知道具体是哪个 class 最终接管了这次调度)
        |
        v
   callback（enqueue/dequeue/pick_next/put_prev 陆续被调用）
   (任务的整个"活跃期"内，enqueue_task/dequeue_task/task_tick 等回调
    被 Core 反复盲调，具体行为完全由任务当前所属的 sched_class 决定)
        |
        v
   [可选：switched_to/switched_from —— 任务的 sched_class 指针在运行期被重新赋值，
    比如 sched_setscheduler() 把一个 SCHED_OTHER 任务改成 SCHED_FIFO]
        |
        v
   destroy
   (任务退出前必须先被 dequeue_task() 从当前 class 的运行队列摘除；
    调度类本身（stop/rt/fair/idle）是编译期常量，永不销毁——
    这与 04 篇"驱动可以运行期注册/注销"是本质区别)
```

---

## 4. Relationship

```
   struct rq（Core，每个 CPU 一个运行队列）
        |
        | sched_class_highest 指向链头（编译期固定）
        v
   stop_sched_class --.next--> rt_sched_class --.next--> fair_sched_class --.next--> idle_sched_class
        ^                          ^                          ^
        |                          |                          |
        +--------------+-----------+--------------+----------+
                        |                          |
                struct task_struct           struct task_struct
                （.sched_class 指向          （.sched_class 指向
                  其中某一个 class）            其中某一个 class，
                                                 可运行期改变指向）
```

与 04 篇 `device->driver` 的关系对比：`device->driver` 是"一对一的运行期绑定"，
一旦 `probe()` 成功基本不变（除非设备被拔出）；`task_struct->sched_class` 也是
"一对一绑定"，但**任务可以在生命周期内多次改变绑定**（`chrt`/`sched_setscheduler()`
系统调用），这是 sched_class 场景特有的"运行期重新注入"。

---

## 5. Data Flow

以 `sched_class_chain.c` 为例：

```
core_pick_next_task(rq)
  |
  +- class = sched_class_highest (&stop_sched_class)
  |     |
  |     v
  |  stop_sched_class.pick_next_task(rq)  ->  NULL（没有 stop 任务在跑）
  |     |
  |     v class = class->next
  +- rt_sched_class.pick_next_task(rq)    ->  NULL（没有可运行的 RT 任务）
  |     |
  |     v class = class->next
  +- fair_sched_class.pick_next_task(rq)  ->  非 NULL，选中 'bash'
  |     |
  |     v 立即返回，停止遍历（rt/idle 完全没被问到）
  +- 返回 'bash'
```

对照真实 Kernel（本仓库真实代码，`kernel/sched.c:4368` `pick_next_task()`）：

```
__schedule()
  |
  v
put_prev_task(rq, prev)          // prev->sched_class->put_prev_task(rq, prev)
  |
  v
pick_next_task(rq)
  |
  +- 快路径优化：if (rq->nr_running == rq->cfs.h_nr_running)
  |     直接调用 fair_sched_class.pick_next_task(rq)，跳过链遍历
  |     （绝大多数系统绝大多数时刻只有 CFS 任务在跑，这是一个纯性能优化，
  |      不影响"链式依次尝试"这个语义模型，只是给最常见情况开了个后门）
  |
  +- for_each_class(class)       // #define for_each_class(class) \
        |                        //   for (class = sched_class_highest; class; class = class->next)
        v
     class->pick_next_task(rq)   // <-- 盲调，逐一尝试
        |  非 NULL
        v
     BUG()（如果连 idle 都返回 NULL，说明调度器出现了严重 bug）
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `sched_class` 结构体本身（`fair_sched_class` 等） | **编译期常量，`const`，永不改变** | 与 04 篇"驱动可以热插拔"是本质区别——调度类不能运行期新增/移除 |
| `sched_class.next`（链的顺序） | **编译期固定，运行期永不改变** | 04 篇/05 篇的链表节点都支持运行期插入/摘除，本篇的链是全系列里最"静态"的一种 |
| `task_struct->sched_class`（任务归属哪个类） | **运行期可变**，`sched_setscheduler()`触发 | 这是本篇真正动态的状态维度，10.3 示例的"迁移"场景演示的就是这个 |
| 每个 class 内部的运行队列（`cfs_rq`/`rt_rq` 等） | **运行期高度动态** | 任务持续 enqueue/dequeue，具体数据结构由各 class 自己选择（CFS 用红黑树，RT 用优先级位图+链表） |
| `rq->nr_running`/`rq->cfs.h_nr_running` 等计数 | **运行期实时变化** | `pick_next_task()` 里的快路径优化正是依赖这两个计数的差值判断 |

---

## 7. Failure

1. **链的顺序被意外打乱**：如果某个调度类初始化代码手误把 `.next` 指错（比如把
   `fair_sched_class.next` 指回 `rt_sched_class` 造成环），`for_each_class()` 会死循环——
   真实 Kernel 里这几个 `.next` 赋值都是极少改动的核心代码，任何改动都会经过严格 review。
2. **某个 class 的 `pick_next_task()` 忘记返回 NULL**：如果一个 class 内部没有可运行任务，
   但 `pick_next_task()` 因为实现 bug 返回了一个非法指针而不是 NULL，Core 会"提前停止"链遍历，
   导致优先级更低的 class 本该被跳过的一次调度机会被这个 bug"偷走"或"提前终止"——
   这是 10.1 示例特意用显式 `runnable` 标志位来强调"必须返回 NULL 才代表放弃"的原因。
3. **idle class 也返回 NULL**：真实 Kernel 用 `BUG()` 而不是继续容错——这是一个刻意的设计
   决策，"CPU 上永远至少有一个可运行的 idle 任务"是调度器的基本不变量，一旦被打破，
   继续运行只会导致更难诊断的后续故障，宁可立即 panic。
4. **任务迁移 class 时忘记先 dequeue**：`sched_setscheduler()` 如果直接改 `sched_class`
   指针而不先从旧 class 的运行队列摘除，旧 class 的内部数据结构（红黑树/链表）会残留一个
   指向"已经不属于自己"的任务节点，后续旧 class 的 `pick_next_task()` 可能选中一个
   `sched_class` 字段已经指向别处的任务，引发状态不一致——10.3 示例的 `core_migrate_class()`
   严格按"先 dequeue，改指针，再 enqueue"的顺序执行，正是为了避免这个问题。
5. **快路径优化掩盖了 bug**：`pick_next_task()` 里 `if (rq->nr_running == rq->cfs.h_nr_running)`
   这一优化在"混合负载"（同时有 RT 和 CFS 任务）时不会触发，只有在"纯 CFS 负载"时生效——
   如果开发者只在纯 CFS 场景下测试过某个调度改动，混合负载下走到完整 `for_each_class()`
   路径时才可能暴露问题，这是性能优化路径与通用路径行为不一致导致的经典"测试盲区"。
6. **调试方法**：
   - 用户态：像 10.1/10.3 示例那样为每个 class 的回调加日志，观察"谁被跳过、谁被选中"，
     以及迁移场景下 `on_rq`/`nr_queued` 计数是否始终守恒。
   - Kernel：`/proc/sched_debug`（`CONFIG_SCHED_DEBUG`）可以查看每个 CPU 运行队列里
     各个 sched_class 的详细状态（`cfs_rq`/`rt_rq` 内容、`nr_running`）；
     `chrt -p <pid>` 查看/修改任务当前的调度策略（对应 `sched_class` 绑定关系）；
     `perf sched record`/`perf sched latency` 可以还原真实的调度决策时序；
     `ftrace` 的 `sched_switch`/`sched_wakeup` tracepoint 能精确看到每次
     `pick_next_task()` 的结果。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `struct sched_class` 定义与链式初始化

真实定义：[include/linux/sched.h:1084](../../include/linux/sched.h)，
链式初始化见 [kernel/sched_stoptask.c:83](../../kernel/sched_stoptask.c)、
[kernel/sched_rt.c:1803](../../kernel/sched_rt.c)、
[kernel/sched_fair.c:5044](../../kernel/sched_fair.c)

```c
static const struct sched_class stop_sched_class = {
	.next			= &rt_sched_class,
	.pick_next_task		= pick_next_task_stop,
	/* ... */
};

static const struct sched_class rt_sched_class = {
	.next			= &fair_sched_class,
	.pick_next_task		= pick_next_task_rt,
	/* ... */
};

static const struct sched_class fair_sched_class = {
	.next			= &idle_sched_class,
	.pick_next_task		= pick_next_task_fair,
	/* ... */
};
```
为什么是 DI：每个 `.c` 文件（`sched_stoptask.c`/`sched_rt.c`/`sched_fair.c`/`sched_idletask.c`）
独立编译，互相不 `#include` 对方的实现细节，只通过 `extern const struct sched_class xxx_sched_class;`
的前置声明知道"链上下一环是谁"——这是 02 篇 Ops Table 与 05 篇链式结构结合后的编译期体现。

### Example 2 — `for_each_class` 宏与 `pick_next_task()` 遍历

真实定义：[kernel/sched.c:1909](../../kernel/sched.c)、[kernel/sched.c:4368](../../kernel/sched.c)

```c
#define sched_class_highest (&stop_sched_class)
#define for_each_class(class) \
	for (class = sched_class_highest; class; class = class->next)

static inline struct task_struct *pick_next_task(struct rq *rq)
{
	const struct sched_class *class;
	struct task_struct *p;

	if (likely(rq->nr_running == rq->cfs.h_nr_running)) {
		p = fair_sched_class.pick_next_task(rq);   /* 快路径优化 */
		if (likely(p))
			return p;
	}

	for_each_class(class) {
		p = class->pick_next_task(rq);              /* <-- 盲调，链式依次尝试 */
		if (p)
			return p;
	}
	BUG(); /* idle class 保证兜底，理论上永远不会走到这里 */
}
```

### Example 3 — `enqueue_task()`/`dequeue_task()`：class 无关的总入口

真实定义：[kernel/sched.c:1943](../../kernel/sched.c)（本仓库行号，函数体极简）

```c
static void enqueue_task(struct rq *rq, struct task_struct *p, int flags)
{
	update_rq_clock(rq);
	sched_info_queued(p);
	p->sched_class->enqueue_task(rq, p, flags);   /* <-- 盲调 */
}

static void dequeue_task(struct rq *rq, struct task_struct *p, int flags)
{
	update_rq_clock(rq);
	sched_info_dequeued(p);
	p->sched_class->dequeue_task(rq, p, flags);   /* <-- 盲调 */
}
```
为什么是 DI：这两个函数从未出现任何"if 是 RT 就……否则……"的分支，`p->sched_class`
这个字段本身就是任务的"契约指针"——它决定了 `enqueue_task`/`dequeue_task` 这次调用
最终落到 `rt_sched_class` 的红黑树实现还是 `fair_sched_class` 的红黑树实现（CFS 也是红黑树，
但组织方式和排序键完全不同）。

### Example 4 — `put_prev_task()`：任务让出 CPU 时的回调

真实定义：[kernel/sched.c:4357](../../kernel/sched.c) 附近

```c
static void put_prev_task(struct rq *rq, struct task_struct *prev)
{
	if (prev->on_rq || rq->skip_clock_update < 0)
		update_rq_clock(rq);
	prev->sched_class->put_prev_task(rq, prev);   /* <-- 盲调 */
}
```
这一行在 `__schedule()` 里紧跟在选出 `next` 任务之前被调用——`prev`（当前正在运行、
即将被切换出去的任务）用它自己所属的 `sched_class` 来做"让出前的收尾"
（比如 CFS 要更新 `prev` 的 `vruntime`，RT 可能要检查是否用尽了时间片）。

### Example 5 — `__sched_setscheduler()` + `check_class_changed()`：class 迁移

真实定义：[kernel/sched.c:5245](../../kernel/sched.c)（`__sched_setscheduler()`）、
[kernel/sched.c:2290](../../kernel/sched.c)（`check_class_changed()`）

```c
/* kernel/sched.c:5245 附近，节选聚焦 DI 相关的核心动作 */
static int __sched_setscheduler(struct task_struct *p, int policy,
				const struct sched_param *param, bool user)
{
	const struct sched_class *prev_class;
	int oldprio, on_rq = p->on_rq, running = task_current(rq, p);

	if (on_rq)
		deactivate_task(rq, p, 0);          /* 先从旧 class 的队列摘除 */
	if (running)
		p->sched_class->put_prev_task(rq, p);

	oldprio = p->prio;
	prev_class = p->sched_class;
	__setscheduler(rq, p, policy, param->sched_priority);
	/* __setscheduler() 内部执行 p->sched_class = policy_to_class(policy);
	 * <-- DI 的核心动作：把任务运行期重新绑定到另一套完整的行为契约 */

	if (running)
		p->sched_class->set_curr_task(rq);
	if (on_rq)
		activate_task(rq, p, 0);            /* 加入新 class 的队列 */

	check_class_changed(rq, p, prev_class, oldprio);
	return 0;
}

/* kernel/sched.c:2290 */
static inline void check_class_changed(struct rq *rq, struct task_struct *p,
				       const struct sched_class *prev_class, int oldprio)
{
	if (prev_class != p->sched_class) {
		if (prev_class->switched_from)
			prev_class->switched_from(rq, p);   /* 通知旧 class："它被抢走了" */
		p->sched_class->switched_to(rq, p);        /* 通知新 class："它归你了" */
	} else if (oldprio != p->prio)
		p->sched_class->prio_changed(rq, p, oldprio);
}
```
为什么是 DI：`p->sched_class = policy_to_class(policy)` 这一行是全篇最核心的一次"注入"——
它把一个运行中的任务，从一套完整的行为契约（如 CFS 的所有回调）**运行期重新绑定**到
另一套完全不同的行为契约（如 RT 的所有回调），且前后两次绑定期间任务本身
（`task_struct` 除 `sched_class` 外的字段）完全不需要重新分配或拷贝。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/10-scheduler-class/`](c/10-scheduler-class/)

### 9.1 `sched_class_chain.c` — stop->rt->fair->idle 链式回退

- 场景：完整复刻 `for_each_class()` 遍历逻辑，用 4 种可运行状态组合验证
  "更高优先级 class 一旦有可运行任务，永远抢先于更低优先级 class"，以及
  "所有 class 都没有任务时兜底到 idle"。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o sched_class_chain sched_class_chain.c`
- 实测输出：
  ```
  === sched_class_chain demo ===

  链的顺序（编译期固定）：stop -> rt -> fair(CFS) -> idle

  -- 场景 1：所有 class 都没有可运行任务，只有 fair 里的 'bash' 可运行 --
    [core] trying class 'stop'... declined (no runnable task), trying next
    [core] trying class 'rt'... declined (no runnable task), trying next
    [core] trying class 'fair(CFS)'... accepted
    => picked task: 'bash'

  -- 场景 2：rt 里的 irq-handler 变为可运行，应该抢先于 fair 被选中 --
    [core] trying class 'stop'... declined (no runnable task), trying next
    [core] trying class 'rt'... accepted
    => picked task: 'irq-handler'

  -- 场景 3：stop 里的 migration 也变为可运行，应该抢先于 rt/fair --
    [core] trying class 'stop'... accepted
    => picked task: 'migration/0'

  -- 场景 4：所有 class 都没有可运行任务，最终兜底到 idle --
    [core] trying class 'stop'... declined (no runnable task), trying next
    [core] trying class 'rt'... declined (no runnable task), trying next
    [core] trying class 'fair(CFS)'... declined (no runnable task), trying next
    [core] trying class 'idle'... accepted
    => picked task: 'swapper/0'

  === done ===
  ```
  （场景 2/3 分别验证了"更高优先级 class 有任务时会抢先"；场景 4 验证了 idle 兜底，
  与真实 Kernel `BUG()`前置条件一致——如果这里也返回 NULL 才是真正的 bug。）
- 为什么是 DI：`core_pick_next_task()`（Core）不知道 `pick_next_task_stop`/`_rt`/`_fair`/`_idle`
  内部任何实现细节，只机械地沿 `.next` 链盲调，第一个返回非 NULL 的就是结果。

### 9.2 `pluggable_scheduler_policies.c` — FIFO vs 加权公平策略对比

- 场景：同一套"策略即 Ops Table"骨架（`pick_next`/`on_ran`），分别接入 FIFO
  （严格按优先级，同优先级连续独占）和加权公平（按累计 vruntime，权重越大越常被选中但
  会与其它任务交替）两种完全不同的算法，验证调度顺序的显著差异。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o pluggable_scheduler_policies pluggable_scheduler_policies.c`
- 实测输出：
  ```
  === pluggable_scheduler_policies demo ===

  -- 策略 'fifo'，运行 6 轮调度 --
    round  1: picked 'irq-relay ' (priority=1)
    round  2: picked 'irq-relay ' (priority=1)
    round  3: picked 'irq-relay ' (priority=1)
    round  4: picked 'audio-out ' (priority=5)
    round  5: picked 'audio-out ' (priority=5)
    round  6: picked 'audio-out ' (priority=5)

  -- 策略 'weighted-fair'，运行 8 轮调度 --
    round  1: picked 'video-enc ' (vruntime=0)
    round  2: picked 'browser   ' (vruntime=0)
    round  3: picked 'idle-timer' (vruntime=0)
    round  4: picked 'video-enc ' (vruntime=250)
    round  5: picked 'video-enc ' (vruntime=500)
    round  6: picked 'browser   ' (vruntime=500)
    round  7: picked 'video-enc ' (vruntime=750)
    round  8: picked 'video-enc ' (vruntime=1000)

  === 结果对比 ===
  FIFO 策略：irq-relay(priority=1) 连续跑 3 轮后让出，才轮到 audio-out
    irq-relay finished_rounds=3, audio-out finished_rounds=3, logger finished_rounds=0
  加权公平策略：weight 越大，被选中次数越多，且分布是交替的而非连续独占
    video-enc(weight=4) finished_rounds=5, browser(weight=2) finished_rounds=2, idle-timer(weight=1) finished_rounds=1
  ```
  （FIFO 下 `irq-relay` 连续独占 3 轮直到"让出"才轮到下一个，`logger` 优先级最低
  始终没机会运行（`finished_rounds=0`）；加权公平下 `video-enc` 权重是 `idle-timer` 的 4 倍，
  最终 `finished_rounds` 之比恰好是 5:1，且从输出序列可见它是"分散穿插"而非连续独占，
  这正是"公平"与"FIFO 独占"两种策略的本质差异。）
- 为什么是 DI：`run_scheduler_rounds()`（Core）不知道"按优先级"还是"按 vruntime"排序，
  只调用 `pc->pick_next(pc)` 和 `pc->on_ran(pc, t)`；新增第三种调度策略
  （比如"轮询/Round-Robin"）完全不需要改这个函数一行代码。

### 9.3 `scheduler_enqueue_dequeue.c` — 入队/出队 + 跨 class 迁移

- 场景：完整演示 `enqueue_task`/`dequeue_task`/`put_prev_task` 的调用时机，
  以及一个任务从 `fair` class 被"迁移"到 `rt` class 时（对应 `sched_setscheduler()`）
  必须遵守的"先 dequeue、改指针、再 enqueue"顺序。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o scheduler_enqueue_dequeue scheduler_enqueue_dequeue.c`
- 实测输出（节选）：
  ```
  === scheduler_enqueue_dequeue demo ===

  -- alloc/init：worker1/worker2 初始归属 fair class --

  -- enqueue：两个任务加入 fair 运行队列 --
    [core] enqueue_task('worker1') -> sched_class='fair'
        [fair] 'worker1' added to CFS runqueue (nr_queued=1)
    [core] enqueue_task('worker2') -> sched_class='fair'
        [fair] 'worker2' added to CFS runqueue (nr_queued=2)

  -- 迁移：chrt 把 worker1 改成 SCHED_FIFO，从 fair 迁移到 rt --
  [core] migrating 'worker1' from class 'fair' to class 'rt'
    [core] dequeue_task('worker1') -> sched_class='fair'
        [fair] 'worker1' removed from CFS runqueue (nr_queued=0)
    [core] enqueue_task('worker1') -> sched_class='rt'
        [rt] 'worker1' added to RT runqueue (nr_queued=1)
        [rt] 'worker1' switched_to RT: boosting priority bookkeeping
    fair_cls.nr_queued (预期 0) = 0, rt_cls.nr_queued (预期 1) = 1

  === done ===
  worker1.run_count=1 (在 fair 里跑过一次), worker1 最终所属 class = rt
  worker2.run_count=0, rt_cls.nr_queued=0, fair_cls.nr_queued=0 (全部应为 0)
  ```
  （迁移前后 `fair_cls.nr_queued`/`rt_cls.nr_queued` 的计数变化精确验证了"先摘除旧队列，
  再加入新队列"的顺序没有产生任何"重复计数"或"遗漏摘除"的问题；最终两个队列都清零，
  证明 destroy 阶段的 dequeue 也全部正确执行。）
- 为什么是 DI：`core_migrate_class()`（Core）的核心动作 `p->sched_class = new_cls`
  就是运行期把一个对象从一套契约实现重新绑定到另一套契约实现——这是 sched_class
  相比 04 篇"绑定后基本不变"的 `device->driver` 更加动态的地方。

一次性编译并运行全部三个示例：`cd c/10-scheduler-class && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Scheduler Class | Ops Table | Notifier Chain | Driver Registration |
|---|---|---|---|---|
| 单个节点的形态 | **一份完整 Ops Table**（20+ 个回调） | 一份 Ops Table | 单个回调 + priority | 整个"驱动实体"（ops+元数据+匹配规则） |
| 多节点组织方式 | **链表**（`.next` 指针，编译期固定顺序） | 不适用（一对一） | 链表（运行期动态排序） | 总线持有的注册表（运行期动态增删） |
| 遍历语义 | **依次尝试，第一个"接受"（返回非 NULL）就停止** | 不适用 | 依次调用**全部**订阅者，除非显式 STOP | 依次尝试 `match`，第一个匹配成功就 `probe` |
| 链的动态性 | **编译期固定，运行期不可增删节点** | 不适用 | 运行期可任意注册/注销 | 运行期可任意注册/注销（热插拔） |
| 什么是"动态"的部分 | **对象（task）与节点（class）的绑定关系** | 对象与 ops 的绑定（运行期可切换指向） | 订阅关系本身 | 设备与驱动的绑定关系 |
| 何时用这种模式 | 需要"有限的几种固定策略，按优先级依次尝试接管同一份工作"的场景 | 只需要固定绑定一份实现 | 需要广播给不确定数量的独立订阅者 | 需要支持运行期任意组合的多对多热插拔匹配 |

一句话：**Scheduler Class 是 Ops Table（02 篇）与"链式依次尝试"（05 篇 Notifier Chain
的结构，但语义相反——"接受即停"而非"否决即停"）的结合体，外加一个 04/09 篇都没有的特性：
链本身编译期固定、绝不热插拔，但链上每个节点服务的"对象"（task）可以在运行期自由地
重新绑定到链上任意一个节点。** 这解释了为什么 Kernel 调度器几十年来只新增过少数几种
调度类（新增 `stop_sched_class`、`dl_sched_class` 都是极其罕见的大改动），
但 `chrt`/`sched_setscheduler()` 这种"任务在几种调度类之间来回切换"的操作却是日常发生的
轻量级动作——**"策略的种类"是重决策、慢变化的，"策略的绑定关系"是轻量级、可以随时调整的**，
这正是 sched_class 这套 DI 设计权衡的核心洞察。

下一篇 [11-allocator-injection.md](11-allocator-injection.md) 转向一个全新的时间维度：
**当"选择哪种实现"这件事不是在运行期决定，而是在编译期就已经通过 Kconfig 二选一决定死了时，
DI 应该怎么设计？**
