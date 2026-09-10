# 07. Workqueue

> 03 篇讲了 Context Injection（`work_struct` 内嵌 + `container_of`）的机制本身。
> 本篇聚焦"任务级别的注入"如何与"异步执行、并发调度、延迟触发"结合成完整的 workqueue 子系统。

---

## 1. Why

中断处理函数（ISR）、某些持锁的临界区、原子上下文，都有一个共同限制：**不能睡眠、不能做耗时操作**。
但很多工作本质上就是耗时的（刷盘、发网络包、内存回收），必须被"推迟"到一个可以睡眠的执行环境
（进程上下文）里去做。

如果没有 workqueue，替代方案是：

1. **在中断里直接做耗时操作**——违反"中断处理要快"的铁律，会导致其它中断被长时间阻塞、系统响应变差。
2. **每个子系统自己维护一个内核线程**——需要各自处理线程创建/销毁/调度，代码重复，
   且线程数量会随子系统数量线性增长，浪费资源。

Workqueue 的设计目标：

> **提供一个通用的、可复用的"延迟执行 + 异步执行"基础设施：任何代码只需要把"要做什么"
> （一个函数 + 它需要的数据）打包成一个 `work_struct`，"扔"进队列，就可以立刻返回；
> 具体"什么时候、由哪个线程"执行，交给 Core 决定。**

这是 03 篇 Context Injection 在"任务调度"场景下的应用：`work_struct` 就是被注入的"任务"，
worker 线程池就是"运行这些任务的通用执行环境"。

---

## 2. Architecture

```
   生产者（中断处理函数 / 系统调用 / 另一个 work）
        |
        | INIT_WORK(&obj->work, my_fn); queue_work(wq, &obj->work);
        v
   +--------------------------------------------------+
   |                workqueue_struct                   |
   |  +----------------+                                |
   |  | 待处理 work 队列 | <-- Core 数据结构               |
   |  +----------------+                                |
   |        |                                           |
   |        | 唤醒                                        |
   |        v                                           |
   |  worker 线程池（N 个 kthread，可动态增减）             |
   |     worker1  worker2  worker3 ...                   |
   +--------------------------------------------------+
        |  每个空闲 worker 各自取走一个 work
        v
   work->func(work)   <-- 盲调，进入具体业务逻辑（进程上下文，可以睡眠）
```

Kernel 真实结构（[kernel/workqueue.c:223](../../kernel/workqueue.c) 附近，本仓库为
"cmwq"—— concurrency-managed workqueue 架构，比传统单线程 workqueue 更复杂，但核心概念一致）：

```
struct workqueue_struct { ... };     /* Core 持有的队列抽象 */
struct work_struct { work_func_t func; struct list_head entry; ... };  /* 被注入的任务 */
struct delayed_work { struct work_struct work; struct timer_list timer; }; /* 延迟版本 */
```

---

## 3. Lifecycle

```
   allocate
   (业务代码定义 struct my_obj { ...; struct work_struct work; };
    分配一个 my_obj 实例)
        |
        v
   initialize
   (INIT_WORK(&obj->work, my_work_fn))
        |
        v
   inject
   (schedule_work(&obj->work) 或 queue_work(wq, &obj->work)
    —— 可能立即执行，也可能先等一个 delay：queue_delayed_work())
        |
        v
   framework owns dispatch
   (Core 把 work 放进内部队列，唤醒某个空闲 worker 线程；
    生产者这行代码立即返回，不等待 work 执行完毕——"fire-and-forget")
        |
        v
   callback
   (某个 worker 线程从队列取出 work，调用 work->func(work)，
    在进程上下文里安全地做耗时操作)
        |
        v
   destroy
   (work 执行完毕后，work_struct 本身通常复用或随业务对象一起释放；
    workqueue 自身的销毁用 destroy_workqueue()，会先 flush 再回收 worker 线程)
```

---

## 4. Relationship

```
   业务对象（内嵌 work_struct，Context Injection，见 03 篇）
        |
        | queue_work() 注入
        v
   workqueue_struct（Core，管理任务队列 + worker 线程池）
        |
        | 唤醒
        v
   worker 线程（Core 管理的执行体，不知道具体任务内容）
        |
        | container_of() 反推
        v
   业务对象（work->func 内部还原出完整上下文，回到调用发起时的业务语义）
```

---

## 5. Data Flow

以 `workqueue_from_scratch.c` 为例：

```
main()
  |
  +- create_workqueue("thumbnail-wq", 3)     // 创建 3 个 worker 线程的池
  |
  +- queue_work(wq, &jobs[i].work)  x 6        // 6 次注入，立即返回，不阻塞 main()
  |
  +- flush_workqueue(wq)                       // 阻塞直到 6 个任务全部执行完毕
        |
        v（并发发生在 3 个 worker 线程里）
   worker_thread_main() 循环
        |
        v
   w = 从队列取出一个 work
        |
        v
   w->func(w)  =>  thumbnail_work_fn(w)
        |
        | container_of(w, struct thumbnail_job, work)
        v
   还原出 job->filename / job->width / job->height，执行耗时的"缩略图生成"
```

对照真实 Kernel（`kernel/workqueue.c` `process_one_work()`，本仓库真实代码）：

```
schedule_work(&dev->irq_work)
  |
  v
queue_work_on() -> __queue_work()          // 塞进 cpu_workqueue_struct 的 worklist
  |
  v
worker_thread() -> process_scheduled_works()
  |
  v
process_one_work(worker, work)
  |
  v
work_func_t f = work->func;
f(work);                                    // <-- 盲调
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| workqueue 内部队列（待处理 work 列表） | **高度动态** | 随 `queue_work()`/worker 取走而随时变化 |
| worker 线程数量 | 创建时固定（简单实现），真实 cmwq **可动态增减** | 本仓库真实 `kernel/workqueue.c` 有"manage_workers"逻辑，按负载动态调整并发 worker 数 |
| `work_struct.func` | 一旦 `INIT_WORK` 后基本不变 | 同一个 `work_struct` 理论上可以被多次复用（重新 `INIT_WORK` + 再次入队），但需要确保上一次已执行完 |
| `busy_workers`/`pending` 计数 | **运行期实时变化** | `flush_workqueue()` 依赖这两个计数判断"是否已经清空" |
| 业务对象自身状态 | 由业务代码维护，Core 完全不感知 | 如 `thumbnail_job.filename` |

---

## 7. Failure

1. **同一个 `work_struct` 被重复入队**：如果一个 work 还没执行完就再次被 `queue_work()`，
   在没有防重入队保护的简化实现里会破坏链表结构（两次入队指向同一块内存的 `next` 字段互相覆盖）——
   真实 Kernel 用 `test_and_set_bit(WORK_STRUCT_PENDING_BIT, ...)` 原子操作防止。
2. **`flush_workqueue()` 与新注入的竞争**：如果在 `flush_workqueue()` 等待期间，另一个线程又
   `queue_work()` 了新任务，flush 应该只保证"flush 调用时刻已经在队列里的任务被处理完"，
   还是"一直等到队列真正清空（包括新来的）"？这是设计上必须明确的语义边界，`workqueue_from_scratch.c`
   的实现属于后一种（简单地等 `pending==0 && busy_workers==0`），真实 Kernel `flush_workqueue()`
   用"flush 时打一个标记（work color）"的机制精确解决这个歧义。
3. **delayed_work 的取消竞态**：`delayed_work_timer.c` 演示的 `cancel_delayed_work()` 存在一个真实的
   竞态窗口——如果取消发生在定时器"刚好触发、但还没检查 `cancelled` 标志"的瞬间，取消可能"来不及"，
   work 依然会被执行；真实 Kernel 的 `cancel_delayed_work_sync()` 通过"取消定时器 + 等待可能已经在跑的
   work 执行完"两步来消灭这个竞态窗口（`del_timer_sync()` + `flush_work()`）。
4. **worker 线程数量与并发上限不匹配**：如果 workqueue 只有 1 个 worker，但提交的任务彼此之间有
   隐式的执行顺序依赖（A 必须在 B 之后才能开始），单 worker 场景下"看起来"总是对的，一旦运维/开发者
   把 worker 数量调大来"提升性能"，隐藏的顺序依赖 bug 就会暴露——这是 Kernel 里
   `WQ_UNBOUND`/`alloc_ordered_workqueue()` 等"保证顺序"变体存在的原因。
5. **销毁 workqueue 时还有未完成的 work**：`destroy_workqueue()` 前如果没有先 `flush`，
   正在队列里但还未执行的 work 会被直接丢弃——如果这些 work 携带的业务对象已经被释放，
   worker 线程即将执行的 `container_of` 还原会访问已释放内存。
6. **调试方法**：
   - 用户态：给 `queue_work`/worker 循环加日志（如三个示例中的 `tid=` 打印），观察任务是否被
     正确分散到多个线程、`flush_workqueue()` 是否真的等待到了全部完成。
   - Kernel：`/sys/kernel/debug/workqueue/wq_dump`（`CONFIG_WQ_WATCHDOG`）可以查看 workqueue
     积压情况；`workqueue.watchdog_thresh` 参数用于检测"work 执行时间过长导致的假死"；
     `ps -eLf | grep kworker` 查看当前有多少 worker 线程在跑；`perf sched` 可以观察 worker
     线程的调度延迟。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `INIT_WORK` + `schedule_work`：最基础的异步注入

```c
struct my_device { struct work_struct irq_work; void __iomem *regs; };

static void my_work_fn(struct work_struct *work)
{
	struct my_device *dev = container_of(work, struct my_device, irq_work);
	slow_path_handle(dev->regs);
}

INIT_WORK(&dev->irq_work, my_work_fn);
schedule_work(&dev->irq_work);   /* 使用系统默认的 system_wq */
```

### Example 2 — `__queue_work()`：Core 内部真正的入队逻辑

真实位置：[kernel/workqueue.c:982](../../kernel/workqueue.c)

```c
static void __queue_work(unsigned int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
	struct global_cwq *gcwq;
	struct cpu_workqueue_struct *cwq;

	debug_work_activate(work);
	/* ... 选择合适的 gcwq/cwq，插入 worklist ... */
	insert_work(cwq, work, worklist, work_flags);
	if (likely(cwq->nr_active < cwq->max_active))
		wake_up_worker(gcwq);   /* 唤醒一个空闲 worker */
}
```

### Example 3 — `process_one_work()`：worker 真正调用业务函数的地方

真实位置：[kernel/workqueue.c:1797](../../kernel/workqueue.c)

```c
static void process_one_work(struct worker *worker, struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq = get_work_cwq(work);
	work_func_t f = work->func;
	/* ... 设置 worker->current_work，处理 lockdep/加锁保护 ... */
	f(work);   /* <-- 盲调 */
	/* ... 清理 worker->current_work，更新 work_color ... */
}
```

### Example 4 — `queue_delayed_work()` + `delayed_work_timer_fn()`：延迟注入

真实位置：[kernel/workqueue.c:1102](../../kernel/workqueue.c)

```c
static void delayed_work_timer_fn(unsigned long __data)
{
	struct delayed_work *dwork = (struct delayed_work *)__data;
	struct cpu_workqueue_struct *cwq = get_work_cwq(&dwork->work);
	__queue_work(smp_processor_id(), cwq->wq, &dwork->work);   /* 定时器到期才真正入队 */
}

int queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork, unsigned long delay)
{
	struct timer_list *timer = &dwork->timer;
	if (delay == 0)
		return queue_work(wq, &dwork->work);
	timer->expires = jiffies + delay;
	add_timer(timer);   /* 03 篇讲的 timer_list 机制，在此复用 */
	return 1;
}
```
为什么这个例子特别值得注意：**delayed_work 本身就是 Context Injection（03 篇 timer_list）
和 Workqueue（本篇）两种手法的组合**——`delayed_work` 内嵌 `work_struct` 和 `timer_list`，
定时器到期的回调（`delayed_work_timer_fn`）不是直接执行业务逻辑，而是把 `work` 转手"再注入"给
workqueue，这是两层 DI 的叠加。

### Example 5 — `flush_workqueue()`：等待队列清空的同步原语

真实位置：[kernel/workqueue.c:2402](../../kernel/workqueue.c) 附近

```c
void flush_workqueue(struct workqueue_struct *wq)
{
	struct wq_flusher this_flusher = { .flush_color = ... };
	/* 给当前队列里的 work 打上一个"flush color"标记，
	 * 只要所有旧 color 的 work 都执行完，就认为这次 flush 完成，
	 * 不会被"flush 调用之后才提交的新 work"无限期阻塞 */
	wait_for_completion(&this_flusher.done);
}
```

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/07-workqueue/`](c/07-workqueue/)

### 9.1 `workqueue_from_scratch.c` — 多 worker 线程 workqueue

- 场景：3 个 worker 线程并发处理 6 个缩略图生成任务，验证 `flush_workqueue()` 正确等待全部完成。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o workqueue_from_scratch workqueue_from_scratch.c`
- 实测输出：
  ```
  === workqueue_from_scratch demo ===

  [wq:thumbnail-wq] creating workqueue with 3 worker thread(s)

  -- 提交 6 个缩略图生成任务到 3 个 worker 的 workqueue --

  -- flush_workqueue()：阻塞直到全部 6 个任务处理完毕 --
    [worker tid=140041944274496] generating 128x128 thumbnail for 'photo_00.jpg'
    [worker tid=140041935881792] generating 128x128 thumbnail for 'photo_01.jpg'
    [worker tid=140041927489088] generating 128x128 thumbnail for 'photo_02.jpg'
    [worker tid=140041944274496] done with 'photo_00.jpg'
    [worker tid=140041944274496] generating 128x128 thumbnail for 'photo_03.jpg'
    [worker tid=140041935881792] done with 'photo_01.jpg'
    [worker tid=140041935881792] generating 128x128 thumbnail for 'photo_04.jpg'
    [worker tid=140041927489088] done with 'photo_02.jpg'
    [worker tid=140041927489088] generating 128x128 thumbnail for 'photo_05.jpg'
    [worker tid=140041944274496] done with 'photo_03.jpg'
    [worker tid=140041935881792] done with 'photo_04.jpg'
    [worker tid=140041927489088] done with 'photo_05.jpg'
  [main] flush returned, all work confirmed done

  -- 销毁 workqueue --
  [wq] destroyed, total_processed=6

  === done ===
  ```
  （可以清楚看到只有 3 个不同的 `tid`，验证确实是 3 个 worker 线程在并发处理 6 个任务，
  每个 worker 处理 2 个；`flush_workqueue()` 返回之后 `total_processed` 精确等于 6。）
- 为什么是 DI：`worker_thread_main()`（Core）从未 `#include` `thumbnail_job` 的定义，
  只操作 `struct work_struct`；新增一种任务类型不需要修改 workqueue 的任何代码。

### 9.2 `delayed_work_timer.c` — 延迟工作 + 取消竞态

- 场景：一个 delayed_work 正常 100ms 后触发；另一个 200ms 的 delayed_work 在 50ms 时被取消，
  验证"取消及时生效，work 从未被执行"。已连续运行 3 次验证结果稳定。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o delayed_work_timer delayed_work_timer.c`
- 实测输出（3 次运行结果一致）：
  ```
  === delayed_work_timer demo ===

  -- 场景1：正常触发 —— 100ms 后 flush 'app.log' --
  [schedule_delayed_work] armed timer for 100ms

  -- 场景2：安排 200ms 后过期缓存，但 50ms 后就取消它 --
  [schedule_delayed_work] armed timer for 200ms
  [cancel_delayed_work] marked cancelled (race: timer thread may still be sleeping)

  -- 等待所有定时器/工作完成 --
    [timer] delay of 100ms elapsed, now queue_work() into wq
      [flush_work_fn] flushing 'app.log' to disk now
    [timer] delayed_work was cancelled before firing, skipping queue_work()

  -- 销毁 workqueue --

  === done: flush_ctx fired=1, cache_ctx fired=0 (expected 0, cancelled) ===
  ```
  （`cache_expire_fn` 的 "evicting cache key" 那行日志**从未出现**，证明取消确实生效，
  `cache_ctx.dwork.fired` 始终为 0。）
- 为什么是 DI：`schedule_delayed_work()`（Core）不知道 `flush_work_fn`/`cache_expire_fn`
  具体做什么，只负责"定时器到期后把 work 转发进 workqueue"；这是 Context Injection（timer）
  与 Workqueue（异步执行）两层机制的组合。

### 9.3 `thread_pool_task_queue.c` — 通用任务队列线程池（带返回值）

- 场景：4 个 worker 线程处理 fib 计算、字符串反转、校验和计算三类不同任务，
  且每个任务可以被"等待结果"（类似简化版 future），与 Kernel workqueue 的"fire-and-forget"
  语义形成对比。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o thread_pool_task_queue thread_pool_task_queue.c`
- 实测输出：
  ```
  === thread_pool_task_queue demo ===

  -- 提交 3 个 CPU 密集型 fib 任务（会分散到不同线程并行计算）--
    [tid=140522781943360] fib(20) = 6765
  [main] fib_tasks[0] result = 6765
    [tid=140522790336064] fib(25) = 75025
  [main] fib_tasks[1] result = 75025
    [tid=140522773550656] fib(28) = 317811
  [main] fib_tasks[2] result = 317811

  -- 提交字符串反转任务 --
    [tid=140522765157952] reversed string -> "noitcejnIycnednepeD" (len=19)
  [main] reverse task result (length) = 19

  -- 批量提交 4 个校验和任务，汇总结果 --
  [main] checksum('kernel') = 641
  [main] checksum('workqueue') = 1000
  [main] checksum('container_of') = 1271
  [main] checksum('notifier') = 864
  [main] total_checksum across 4 words = 3776

  -- 销毁线程池 --
  [pool] destroyed, total_run=8

  === done ===
  ```
  （`"DependencyInjection"` 反转后是 `"noitcejnIycnednepeD"`，可以手动验证字符级正确性；
  `total_run=8` 精确等于 3 个 fib + 1 个反转 + 4 个校验和。）
- 为什么是 DI：`threadpool_worker()`（Core）只认识 `struct task` 契约（`run`/`arg`/`result`），
  完全不知道 `fib_task_fn`/`reverse_task_fn`/`checksum_task_fn` 的具体计算逻辑；
  `task_wait()` 提供的"等待结果"语义是对 Kernel workqueue "fire-and-forget"模型的用户态扩展，
  说明同一套 DI 骨架可以按需叠加不同的同步语义。

一次性编译并运行全部三个示例：`cd c/07-workqueue && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Workqueue | Context Injection | Function Pointer Callback | Notifier Chain |
|---|---|---|---|---|
| 执行时机 | **异步**，由 worker 线程决定何时执行 | 取决于宿主机制（可能同步可能异步） | 通常同步（调用即执行） | 同步（广播时立即逐一调用） |
| 执行者 | Core 管理的线程池 | 不涉及执行者概念，只是数据携带机制 | 调用者所在线程 | 调用者所在线程 |
| 是否支持延迟 | **是**，`delayed_work` | 否 | 否 | 否 |
| 是否支持并发 | **是**，多 worker 并行处理不同任务 | 不涉及并发 | 不涉及 | 通常顺序调用（除非订阅者自己开线程） |
| 何时用 Workqueue | 需要"推迟到进程上下文异步执行"的耗时操作，尤其是从中断/原子上下文触发的场景 | 任何需要"回调带上下文"的场景（几乎所有异步机制的基础） | 需要同步、立即执行的单一动作 | 需要同步广播给多个订阅者 |

一句话：**Workqueue 是"Context Injection + Callback"在时间维度上的延伸**——
它不仅解决"回调怎么拿到上下文"，还解决"回调应该在什么时间、什么执行环境（线程/CPU）下运行"，
是 Kernel 里"异步化"的默认工具箱。
下一篇 [08-security-hooks-lsm.md](08-security-hooks-lsm.md) 转向一个特殊场景：
**当"策略"需要在系统的每一个关键操作路径上被强制检查、且必须支持多个策略模块同时叠加时，
用什么手法？**
