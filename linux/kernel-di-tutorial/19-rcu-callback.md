# 19. RCU Callback（`struct rcu_head` / `call_rcu`）

> 07 篇 [07-workqueue.md](07-workqueue.md) 讲的"延迟执行"是为了**异步**——把耗时工作挪出关键路径；
> 本篇讲的 RCU Callback 的"延迟执行"是为了**安全性证明**——把释放动作推迟到"确定没有人还在用旧数据"
> 的那一刻。同样是"回调不会立刻执行"，但延迟的理由、延迟的判定者完全不同，这是本篇要讲清楚的核心。

---

## 1. Why

一个链表节点（或任何被多个读者并发访问的数据）要被摘除并释放，传统做法是"引用计数 + 锁"：

```
读者：lock(); use(node); unlock();          // 或 atomic_inc(&node->refcount) 再用
写者：lock(); unlink(node); unlock(); free_if_refcount_zero(node);
```

问题：

1. **读者也要付出同步开销**：哪怕只是"读一下"，也要么争抢一把锁，要么做一次原子递增/递减——
   在"读多写少"的场景下（路由表查找、`dentry` 查找、`file_operations` 分派……），
   这份开销会被读路径放大成整个系统的瓶颈。
2. **锁把读者和写者绑在一起**：写者要更新数据，必须等所有正在读的读者退出——如果读者数量很多、
   持有时间不确定，写者可能等很久，读者之间也可能互相阻塞。
3. **判断"何时可以安全释放"这件事，被下放给每一个具体场景自己发明轮子**：有的用引用计数，
   有的用"两阶段删除"，各自实现细节不同，容易出错（忘记递减计数、忘记判空）。

如果没有 RCU，Kernel 里大量"读多写少、性能极限"的路径（网络收包时查路由表、VFS 路径查找时查 dentry
cache）都必须付出"读者也要同步"的代价。RCU 的设计目标：

> **让读者完全不需要修改任何共享状态就能安全读取，把"何时可以释放旧数据"这个决策，
> 交给一个全局的、跨 CPU 协调的 grace period 机制去证明，而不是靠读者自己维护的引用计数。**

`include/linux/rcupdate.h:644` 的注释把这条设计哲学写得非常直接：

```
/*
 * So where is rcu_write_lock()?  It does not exist, as there is no
 * way for writers to lock out RCU readers.  This is a feature, not
 * a bug -- this property is what provides RCU's performance benefits.
 */
```

`call_rcu()` 正是这套机制里"回调何时被调用"这一环的具体实现：**它不是"事件发生就立刻调用"，
而是"注册后，必须等到一个全局的 grace period 结束才会被调用"**——这与 01~08 篇讲过的所有回调注入手法
（事件驱动、立刻盲调）在设计哲学上是根本不同的一类。

---

## 2. Architecture

```
   写者（要摘除/替换一份数据）
        |
        | 1. 分配一份全新的、完整的数据（绝不就地修改旧数据）
        | 2. rcu_assign_pointer(shared_ptr, new_data)   <-- 一次原子指针切换
        | 3. call_rcu(&old_data->rcu_head, free_callback)
        v
  +------------------------+
  | struct rcu_head        |   include/linux/types.h:246
  |   next                 |   <-- 侵入式单链表指针（Context Injection，03篇）
  |   func                 |   <-- 回调（Function Pointer Callback，01篇）
  +------------------------+
        |
        | 挂入本 CPU 的 callback 队列（kernel/rcutree.c:1589 __call_rcu）
        v
  +------------------------+
  | rdp->nxtlist（per-CPU）|   真实实现是分段链表（RCU_DONE_TAIL/RCU_WAIT_TAIL/...）
  +------------------------+
        |
        | grace period 结束后才被取出（这是与其它 DI 手法最大的区别）
        v
  +------------------------------+
  | grace period 检测器            |  跨越所有 CPU 判断"是否所有 CPU
  | （kernel/rcutree.c 的 rcu_state） |  都已经经过至少一次 quiescent state」
  +------------------------------+
        |
        v
  rcu_do_batch()（kernel/rcutree.c:1257）依次调用每个 head->func(head)
        |
        v
  读者（rcu_read_lock() / rcu_dereference() / rcu_read_unlock()）
  全程不需要修改任何共享状态，只做一次指针读 + 内存序保证
```

与 02/03 篇对照：`rcu_head` 本身是"通用契约字段"，几乎总是以 Context Injection（03篇）
的方式内嵌在业务对象里；`call_rcu()` 传入的回调是标准的 Function Pointer Callback（01篇）；
真正独特的是"谁来决定调用时机"——不是 Core 在事件发生时盲调，而是一个跨 CPU 的 grace period
检测器在"证明安全"之后才触发。

---

## 3. Lifecycle

```
   [读者侧]                                [写者侧]
   rcu_read_lock()                          分配新版本数据（完整构造，
        |                                    绝不修改已发布的旧版本）
        |                                       |
   rcu_dereference(ptr)                         v
        |                                  rcu_assign_pointer(ptr, new)
        |                                  （原子指针切换 = "发布"）
   使用数据（可能耗时）                            |
        |                                       v
   rcu_read_unlock()                        call_rcu(&old->rcu, free_fn)
   （这一刻 == 一次                          （登记回调，立刻返回，不等待）
    quiescent state）                              |
                                                   v
                                          grace period 检测器持续判断：
                                          "从 call_rcu() 那一刻起，
                                           是否所有 CPU 都至少经过
                                           一次 quiescent state？"
                                                   |
                                          （期间任何读者的
                                           rcu_read_lock/unlock
                                           都可能构成这个判断的依据）
                                                   v
                                          grace period 结束
                                                   |
                                                   v
                                          rcu_do_batch()：真正调用 free_fn(head)
                                                   |
                                                   v
                                          free(old)（此刻绝对安全，
                                          因为没有任何读者可能还持有指向
                                          old 的、在 rcu_assign_pointer
                                          之后才发生的引用）
```

关键差异 vs 07 篇 Workqueue：Workqueue 的 `work_struct` 一旦入队，worker 线程"随时可以"取出执行
（只要排到它）；RCU 的 `rcu_head` 入队后，**必须等待一个与它无关的、由其它 CPU 的行为决定的条件**
（grace period 结束）才能被取出执行——延迟的时长不是"排队等待"，而是"等待一个安全性证明"。

---

## 4. Relationship

```
   业务对象（如 struct config_node / struct inode）
     |
     | 内嵌（Context Injection，03篇）
     v
   struct rcu_head
     |
     | 地址被 call_rcu() 交给
     v
   per-CPU callback 队列（Core，不知道具体业务类型）
     |
     | grace period 检测器（跨 CPU 协调，Core 的另一部分）
     v
   rcu_do_batch()：grace period 结束后盲调 head->func(head)
     |
     v
   业务的释放回调（如 bdev_i_callback）
     |
     | container_of() 还原（03篇的机制在这里被复用）
     v
   业务对象（安全释放）

   （与此完全并行、互不阻塞的另一条关系链）
   读者线程 ---- rcu_dereference() ----> 共享指针 ----> 业务对象当前版本
   （读者从头到尾不出现在上面这条"写者->callback"关系链里，这正是 RCU 的核心卖点）
```

真实内核关系示例（`fs/block_dev.c:422`）：

```c
static void bdev_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	struct bdev_inode *bdi = BDEV_I(inode);
	INIT_LIST_HEAD(&inode->i_dentry);
	kmem_cache_free(bdev_cachep, bdi);
}

static void bdev_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, bdev_i_callback);
}
```

`inode->i_rcu`（`include/linux/fs.h:807`）与 `inode->i_dentry` 是**同一块内存的 union**——
这是 03 篇 Context Injection 的一个巧妙延伸：反正 `i_dentry` 在 inode 被销毁流程里已经不需要用了，
就把这块内存挪来存 `rcu_head`，省下额外的内存开销。

---

## 5. Data Flow

对照真实内核路径（网络设备接收处理钩子的注册与查找，`net/core/dev.c`）：

```
【注册期，process context，持有 rtnl_mutex】
netdev_rx_handler_register(dev, rx_handler, rx_handler_data)      net/core/dev.c:3191
  |
  +- rcu_assign_pointer(dev->rx_handler_data, rx_handler_data)    net/core/dev.c:3200
  +- rcu_assign_pointer(dev->rx_handler, rx_handler)              net/core/dev.c:3201
        （两次原子指针写，配合 smp_wmb()，保证 data 先于 handler 可见）

【收包热路径，softirq context，每个包都会执行】
__netif_receive_skb(skb)
  |
  +- rx_handler = rcu_dereference(skb->dev->rx_handler)           net/core/dev.c:3287
  |     （无锁读取，哪怕另一个 CPU 正在 register/unregister 也不会读到撕裂状态）
  v
  if (rx_handler) rx_handler(&skb)                                 <-- 盲调，Function Pointer Callback

【注销期】
netdev_rx_handler_unregister(dev)
  |
  +- rcu_assign_pointer(dev->rx_handler, NULL)
  +- synchronize_rcu()   // 或 call_rcu() 风格的延迟释放
        （等待 grace period，确保所有"看到旧 rx_handler 非 NULL"的收包路径
         都已经执行完毕，此后才能安全释放 rx_handler_data 指向的内存）
```

对照本文档 [c/19-rcu-callback/call_rcu_grace_period_sim.c](c/19-rcu-callback/call_rcu_grace_period_sim.c)
的真实运行输出（第 9 节完整粘贴）：`call_rcu` 立即返回、grace period 检测器轮询等待、
`rcu_do_batch` 最终调用回调，三个阶段与上面这条真实路径的"注册 -> 热路径读 -> 注销 + 延迟释放"
完全对应。

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `rcu_head.func`/`next` | **注册后不再修改**，由 Core 在 grace period 结束后消费 | 一旦 `call_rcu()` 传入就不应该再被业务代码触碰 |
| 被保护的数据本身（如 `config_node` 的字段） | **一旦发布（`rcu_assign_pointer`）后绝不就地修改** | 这是 RCU 一致性保证的根基——写者永远"构造新版本+切指针"，绝不"改旧版本的字段" |
| 共享指针（`shared_ptr`/`dev->rx_handler`） | **可以频繁原子切换**，但每次切换必须是"整体替换" | 切换本身只需要一次原子写（`rcu_assign_pointer`），比锁便宜得多 |
| grace period 的"进行中/已结束"状态 | **全局的、跨 CPU 协调的状态**，任何单个 CPU 都不能单方面决定 | 这是 RCU 与本系列其它手法最大的不同：状态判定权不在注册方也不在 Core 的某个固定位置，而在一个分布式的协调机制里 |
| 读者的"是否处于 read-side critical section" | **每个 CPU/线程独立**，仅由该 CPU 自己的 `rcu_read_lock`/`unlock` 维护 | 这正是 grace period 判定所依赖的"quiescent state"信号来源 |

---

## 7. Failure

1. **在 RCU 读临界区里睡眠**：非 preemptible RCU（本仓库默认配置）下，`rcu_read_lock()` 和
   `rcu_read_unlock()` 之间不允许睡眠——如果睡眠，调度器切换到的下一个任务可能永远不会让出 CPU
   回到这个读者，导致 grace period 永远无法结束（因为这个 CPU 一直"卡"在一次读临界区里，
   无法产生新的 quiescent state）。
2. **忘记 `rcu_assign_pointer`，直接赋值指针**：普通的 `p = v` 在某些架构和优化级别下不保证
   "先写完新对象的所有字段，再让指针可见"这个顺序（编译器/CPU 可能重排），读者可能通过
   `rcu_dereference` 拿到一个字段还没写完的半成品对象——这是本篇 9.2 示例用 checksum 专门检测的问题。
3. **把 `rcu_dereference` 的结果在临界区外继续使用**：一旦 `rcu_read_unlock()`，这份数据随时可能
   进入下一个 grace period 后被释放——如果读者保存了指针，退出临界区后才使用，就是典型的
   "读取已释放内存"，等价于 03 篇讲过的悬挂指针问题在 RCU 场景下的版本。
4. **就地修改一个已经发布的、可能正被读者访问的节点**：这是最隐蔽的一类 bug——表面上看起来
   "只是改了个字段"，但如果这份数据已经被 `rcu_assign_pointer` 发布过，任何就地修改都可能
   被并发读者读到"修改中"的中间状态；正确做法永远是"构造新版本 + 切换指针"。
5. **误以为 `call_rcu()` 会等待 grace period 结束才返回**：`call_rcu()` 是异步的，注册完立刻返回；
   如果调用者需要"确定回调已经执行完"（比如要 `rmmod` 一个模块），必须用 `rcu_barrier()`
   或改用同步版本 `synchronize_rcu()`，不能想着"call_rcu 后面紧接着的代码肯定在回调之后执行"。
6. **调试方法**：
   - 用户态：本篇 9.2 示例用 checksum 字段实测验证"永远不会读到撕裂状态"；
     ThreadSanitizer（`-fsanitize=thread`）能检测数据竞争，验证 `rcu_assign_pointer`/`rcu_dereference`
     配对使用（acquire/release 语义）是否真的建立了正确的内存序。
   - Kernel：`lockdep` 配合 `CONFIG_PROVE_RCU` 能检测"在 RCU 读临界区里睡眠"、
     "rcu_dereference 用在没有持有 rcu_read_lock 的地方"这类误用；
     `/proc/sys/kernel/`下没有直接的 RCU 状态文件，但 `ftrace` 的 `rcu_grace_period`/`rcu_callback`
     事件可以追踪 grace period 的开始/结束和每个回调的调用时刻；
     `rcu_barrier()` 常在模块卸载路径里用来"排空"所有未执行的回调，避免模块代码段被卸载后
     残留的回调函数指针变成悬空跳转。

---

## 8. Kernel Core Pseudocode（5 个，均标注本仓库真实文件/行号）

### Example 1 — `struct rcu_head` 本身

真实定义：[include/linux/types.h:246](../../include/linux/types.h)

```c
struct rcu_head {
	struct rcu_head *next;
	void (*func)(struct rcu_head *head);
};
```
为什么是 DI：与 18 篇 `tasklet_struct`、03 篇 `work_struct` 一样，是"最小化的通用契约字段"——
只包含 Core（RCU 回调调度机制）需要的两个字段，业务上下文靠内嵌（Context Injection）携带。

### Example 2 — `call_rcu()`：注册回调，立刻返回

真实原型：[include/linux/rcupdate.h:85](../../include/linux/rcupdate.h)，
真正入队实现：[kernel/rcutree.c:1589](../../kernel/rcutree.c) `__call_rcu()`

```c
extern void call_rcu(struct rcu_head *head,
		      void (*func)(struct rcu_head *head));

static void
__call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *rcu),
	   struct rcu_state *rsp)
{
	unsigned long flags;
	struct rcu_data *rdp;

	head->func = func;
	head->next = NULL;
	smp_mb(); /* Ensure RCU update seen before callback registry. */

	local_irq_save(flags);
	rdp = this_cpu_ptr(rsp->rda);
	*rdp->nxttail[RCU_NEXT_TAIL] = head;         /* 挂入本 CPU 的分段链表 */
	rdp->nxttail[RCU_NEXT_TAIL] = &head->next;
	rdp->qlen++;
	...
	local_irq_restore(flags);
}
```
为什么是 DI：调用者只知道"我要注册一个回调，将来会在安全的时候被调用"，完全不知道
`rdp->nxttail[]` 这个分段链表、`rcu_state` 这个跨 CPU 协调结构的存在——这些都是 Core
侧被完全隔离的实现细节。

### Example 3 — `rcu_process_callbacks()`/`__rcu_process_callbacks()`：grace period 检测的入口

真实定义：[kernel/rcutree.c:1519](../../kernel/rcutree.c) `__rcu_process_callbacks()`，
[kernel/rcutree.c:1555](../../kernel/rcutree.c) `rcu_process_callbacks()`

```c
static void
__rcu_process_callbacks(struct rcu_state *rsp, struct rcu_data *rdp)
{
	/* 推进"上一个 grace period 结束"带来的状态变化 */
	rcu_process_gp_end(rsp, rdp);
	/* 根据本 CPU 最近的 quiescent state 更新全局判定 */
	rcu_check_quiescent_state(rsp, rdp);
	/* 如果本 CPU 有新回调在等待、且还没有 grace period 在跑，启动一个 */
	if (cpu_needs_another_gp(rsp, rdp))
		rcu_start_gp(rsp, flags);
	/* 如果已经有回调"熬过"了一个完整 grace period，调用它们 */
	if (cpu_has_callbacks_ready_to_invoke(rdp))
		invoke_rcu_callbacks(rsp, rdp);
}

static void rcu_process_callbacks(struct softirq_action *unused)
{
	__rcu_process_callbacks(&rcu_sched_state, &__get_cpu_var(rcu_sched_data));
	__rcu_process_callbacks(&rcu_bh_state, &__get_cpu_var(rcu_bh_data));
}
```
真实注册：`open_softirq(RCU_SOFTIRQ, rcu_process_callbacks)`（[kernel/rcutree.c:2156](../../kernel/rcutree.c)）——
RCU 本身也是靠 17 篇 Softirq 这个更底层的机制驱动的。

为什么是 DI：这一层完全体现"调用时机由 Core 决定"——业务代码在 `call_rcu()` 那一刻
对"什么时候会被真正调用"没有任何控制权，调用时机取决于一个它看不见、管不着的
跨 CPU 状态机（`rcu_state`）。

### Example 4 — `rcu_do_batch()`：grace period 结束后真正调用回调

真实定义：[kernel/rcutree.c:1257](../../kernel/rcutree.c)

```c
static void rcu_do_batch(struct rcu_state *rsp, struct rcu_data *rdp)
{
	struct rcu_head *next, *list;

	if (!cpu_has_callbacks_ready_to_invoke(rdp))
		return;

	list = rdp->nxtlist;
	rdp->nxtlist = *rdp->nxttail[RCU_DONE_TAIL];
	...
	while (list) {
		next = list->next;
		prefetch(next);
		debug_rcu_head_unqueue(list);
		__rcu_reclaim(rsp->name, list);   /* 内部会调用 list->func(list) */
		list = next;
	}
}
```
为什么是 DI：`rcu_do_batch()`（Core）从未 `#include` 任何具体业务类型（`config_node`/
`inode`/`btrfs_device`），只通过 `rcu_head->func` 盲调——这与 02 篇 ops table 的
"Core 只认识契约类型"哲学完全一致，只是多了"必须先证明安全"这一道额外的前置条件。

### Example 5 — `rcu_assign_pointer()`/`rcu_dereference()`：读写两端的契约宏

真实定义：[include/linux/rcupdate.h:760](../../include/linux/rcupdate.h)（`rcu_assign_pointer`），
[include/linux/rcupdate.h:576](../../include/linux/rcupdate.h)（`rcu_dereference`），
真实驱动用法：[net/core/dev.c:3200-3201](../../net/core/dev.c)（写）、
[net/core/dev.c:3287](../../net/core/dev.c)（读）

```c
#define __rcu_assign_pointer(p, v, space) \
	({ smp_wmb(); (p) = (typeof(*v) __force space *)(v); })
#define rcu_assign_pointer(p, v) __rcu_assign_pointer((p), (v), __rcu)

#define __rcu_dereference_check(p, c, space) \
	({ typeof(*p) *_________p1 = (typeof(*p)*__force)ACCESS_ONCE(p); \
	   smp_read_barrier_depends(); \
	   ((typeof(*p) __force __kernel *)(_________p1)); })
#define rcu_dereference(p) rcu_dereference_check(p, 0)

/* net/core/dev.c:3200 注册收包回调 */
rcu_assign_pointer(dev->rx_handler_data, rx_handler_data);
rcu_assign_pointer(dev->rx_handler, rx_handler);

/* net/core/dev.c:3287 收包热路径读取 */
rx_handler = rcu_dereference(skb->dev->rx_handler);
```
为什么是 DI：`rx_handler` 本身就是一个标准的 Function Pointer Callback（01篇），
但它的"注入"（注册）和"消费"（查找后调用）两端由 `rcu_assign_pointer`/`rcu_dereference`
这一对内存序原语保护——这是 RCU 与"普通函数指针字段"的关键差异：读者不需要锁，
但依然被保证"要么看到完整旧值，要么看到完整新值"。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/19-rcu-callback/`](c/19-rcu-callback/)

### 9.1 `call_rcu_grace_period_sim.c` — `call_rcu` + grace period 检测 + 延迟释放

- 场景：4 个 reader 线程持续做"心跳"（模拟 `rcu_read_lock`/`unlock`），
  writer 注册 `call_rcu` 回调，grace period 检测器等待所有 reader 心跳前进后才放行回调执行。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o call_rcu_grace_period_sim call_rcu_grace_period_sim.c`
- 实测输出：
  ```
  === call_rcu_grace_period_sim ===

  -- 启动 4 个 reader 线程，持续做 rcu_read_lock/unlock --

  -- 场景：writer 摘除一个旧的 config_node，注册 call_rcu 延迟释放 --
    (摘除动作：假设已经有另一个线程把全局指针指向了新节点，
     old_node 现在对新的 reader 已经不可达，但可能还有正在读它的 reader)
    [call_rcu] 注册回调 'old-config'，立刻返回（不等待 grace period）

  -- 等待 grace period（所有 reader 至少完成一次心跳）--
    [grace_period] 记录快照: r0=82 r1=81 r2=81 r3=81 
    [grace_period] 所有 reader 都已经至少经过一次 quiescent state，grace period 结束

  -- grace period 结束，执行 rcu_do_batch 风格的回调调用 --
    [rcu_do_batch] grace period 已结束，安全调用回调 'old-config'
      [free] 真正释放 config_node 'old-config' (value=42) 的内存

  -- 再注册两个回调，验证批量场景 --
    [call_rcu] 注册回调 'cfg-1'，立刻返回（不等待 grace period）
    [call_rcu] 注册回调 'cfg-2'，立刻返回（不等待 grace period）
    [grace_period] 记录快照: r0=83 r1=82 r2=82 r3=82 
    [grace_period] 所有 reader 都已经至少经过一次 quiescent state，grace period 结束
    [rcu_do_batch] grace period 已结束，安全调用回调 'cfg-1'
      [free] 真正释放 config_node 'cfg-1' (value=1) 的内存
    [rcu_do_batch] grace period 已结束，安全调用回调 'cfg-2'
      [free] 真正释放 config_node 'cfg-2' (value=2) 的内存

  === done, 总共安全释放了 3 个节点 ===
  ```
  （`r0`~`r3` 的具体心跳计数因线程调度而每次运行略有不同，但"记录快照后所有值都会前进"
  这个断言每次都成立，这正是 grace period 判定逻辑的核心。）
- 为什么是 DI：`mini_call_rcu()`/`run_ready_callbacks()`（Core）从未 `#include` `config_node`
  的定义，只通过 `head->func(head)` 盲调；而"什么时候调用"这个时机完全由一个与调用者无关的、
  跨线程协调的 grace period 检测器决定——这是本篇要强调的、与前 18 篇所有回调注入手法都不同的地方。

### 9.2 `rcu_read_lock_replace_demo.c` — 一致性实测：永不读到撕裂状态

- 场景：6 个 reader 线程持续读取一个多字段配置节点，1 个 writer 线程做 2000 次"构造新版本 +
  原子指针切换"式的替换，reader 用 checksum 实时验证自己读到的字段组合是否"整体一致"。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o rcu_read_lock_replace_demo rcu_read_lock_replace_demo.c`
- 实测输出：
  ```
  === rcu_read_lock_replace_demo ===

  -- 启动 6 个 reader 线程持续读取，1 个 writer 线程持续替换（共 2000 次替换）--

  -- writer 完成全部 2000 次替换，最终版本 version=2000 --
  -- reader 总读取次数: 120000 --
  -- 检测到撕裂读次数: 0（预期恒为 0）--

  === done: 共发布 2001 个版本，全部安全回收 ===
  验证通过：全程没有任何 reader 读到不一致的中间状态。
  ```
- 为什么是 DI：写者从未"就地修改"任何已经发布给读者的节点——每次替换都是"分配全新对象
  +一次原子指针切换"，这正是 `rcu_assign_pointer` 的核心约束；读者完全不知道也不需要知道
  "现在是第几个版本"，只管读当前指针指向的数据，Core（这里是共享指针本身）替读者屏蔽了
  所有关于"更新如何发生"的细节。

### 9.3 `rcu_vs_refcount_compare.c` — 读写锁 vs RCU 的真实吞吐对比

- 场景：8 个 reader 线程 + 1 个 writer 线程，分别用传统 `pthread_rwlock_t` 方案和
  RCU 风格无锁方案实现同一个"多读者、偶尔写者"场景，各自测量 1 秒内的读取吞吐量。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o rcu_vs_refcount_compare rcu_vs_refcount_compare.c`
- 实测输出：
  ```
  === rcu_vs_refcount_compare ===
  (reader 线程数=8, 每种方案测量 1 秒)

  -- 方案 (a)：pthread_rwlock 读写锁，读者也要 rdlock/rdunlock --
    总读取吞吐: 49541120 次/秒

  -- 方案 (b)：RCU 风格，读者只做一次原子指针读，完全无锁 --
    总读取吞吐: 14400065536 次/秒

  === 对比结果 ===
    rwlock 方案: 49541120 次/秒
    RCU    方案: 14400065536 次/秒
    RCU 方案吞吐倍数: 290.67x

  (实测数字受运行环境 CPU 核数/负载影响会有波动，但 RCU 方案读吞吐显著更高这一趋势应当稳定重现，因为读者完全不需要和写者/其它读者竞争任何锁。)
  ```
  （连续多次运行，RCU 方案的吞吐倍数在 290x~341x 之间波动，具体数字受测试机负载影响，
  但"RCU 方案显著更高"这个趋势在所有实测中都稳定重现——根本原因是 rwlock 方案的读者之间
  以及读者与写者之间存在真实的缓存行竞争与锁开销，而 RCU 方案的读者只做一次原子读，
  且不同 CPU 核心之间没有任何共享的可写状态需要同步。）
- 为什么是 DI：两种方案实现的是完全相同的"业务契约"（读者读取当前值，写者可以更新值），
  但"读者如何与写者协调"这条隐藏的同步契约完全不同——RCU 把这份协调工作从"读者必须参与"
  变成了"读者完全不参与，由 Core 的 grace period 机制在事后证明安全性"，这正是本篇第 1 节
  Why 里论证的核心收益，本示例用真实测量数据把这份收益量化了出来。

一次性编译并运行全部三个示例：`cd c/19-rcu-callback && make run`

---

## 10. 与其它 DI 手法比较

### RCU 方案 vs 传统"引用计数 + 锁"方案

| 对比 | 传统引用计数/锁 | RCU |
|---|---|---|
| 读者开销 | 至少一次原子操作（`atomic_inc`）或锁获取/释放 | **零**——只有一次指针读 + 内存屏障 |
| 读者与写者是否互相阻塞 | 是，写者通常要等所有读者释放引用 | **否**，写者从不等待读者，读者也从不等待写者 |
| 何时可以安全释放旧数据的判定者 | 每个读者自己维护的引用计数 | 一个跨 CPU 协调的 grace period 检测器 |
| 实现复杂度 | 相对简单，但每个场景要自己保证"计数不会漏加/漏减" | 更复杂（需要理解 grace period、内存序），但一旦掌握可以复用到任意场景 |
| 典型性能特征 | 读多写少场景下，读者的同步开销随并发度增长而放大（见 9.3 实测） | 读多写少场景下几乎线性扩展，几乎不受并发读者数量影响 |
| 适用边界 | 通用，任何读写模式都能work，但性能不是最优 | 只适合"读多写少 + 能接受延迟释放"的场景；不适合"读者需要修改数据"的场景 |

### RCU Callback vs Workqueue（07篇）："延迟"的目的完全不同

| 对比 | Workqueue（07篇） | RCU Callback（本篇） |
|---|---|---|
| 延迟执行的**目的** | 异步——把耗时工作挪出关键路径（中断/热路径），不阻塞调用者 | 安全性证明——确保"没有人还在用旧数据"之后才释放，防止 use-after-free |
| 延迟的时长由谁决定 | worker 线程池的调度（相对确定，通常是"下一个空闲 worker") | grace period 检测器（不确定，取决于所有 CPU 的行为，可能很快也可能较慢） |
| 调用者是否关心"什么时候执行" | 通常关心（可以 `flush_work` 等待完成） | 通常不关心具体时刻，只关心"最终一定会安全执行"，需要同步等待时才用 `rcu_barrier`/`synchronize_rcu` |
| 是否可以在回调里睡眠 | **可以**，运行在独立内核线程里 | **不可以**（默认 RCU 语境下），回调通常运行在软中断上下文（`RCU_SOFTIRQ`） |
| 是否需要"多个 CPU 协调"才能触发 | 不需要，工作线程池内部调度即可 | **需要**，grace period 本质就是"跨 CPU 协调"的产物 |

一句话：**RCU Callback 把"延迟执行回调"这个手法用到了一个前 18 篇都没有触及的维度——
延迟不是为了让调用者轻松、也不是为了排队等资源，而是为了让一个分布式的、跨 CPU 的协调机制
有机会"证明"释放动作是安全的。** 它与传统"引用计数+锁"解决的是同一个问题（安全释放共享数据），
但把同步开销从"每一次读"转移到了"每一次写之后的一次等待"，这正是"读多写少"场景下
RCU 相比锁/引用计数具有压倒性性能优势的根本原因，也是本教程 20 种手法里，
唯一一种"回调调用时机由全局分布式状态而非单一注册点决定"的设计。
