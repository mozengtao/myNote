# 03. Context Injection（container_of / private_data / work_struct）

> 上一篇 [02-ops-table.md](02-ops-table.md) 留下一个问题：ops table 是 `static const`，
> 天然无状态、被所有实例共享。那"这个具体对象自己的私有数据"该放在哪？本篇给出答案。

---

## 1. Why

回调函数的签名是**契约的一部分，一旦定下来不能随便改**（改了所有实现都要跟着改，等价于破坏 ABI）。
但具体实现往往需要访问"自己的私有数据"——比如 `flush_task_fn(struct mini_work *w)` 内部需要知道
"该刷哪个设备、有多少脏页"，可这些信息不在 `work_struct` 的固定字段里。

如果没有 Context Injection，只有两种糟糕的选择：

1. **全局变量**：`flush_task_fn` 里写 `extern struct flush_task g_the_one_task`——只能支持一个实例，
   完全不可扩展，多线程下还有竞争问题。
2. **每加一种用途就改回调签名**：`work_func_t` 变成 `void (*)(int device_id, int pages, ...)`——
   契约爆炸，Core 代码要跟着改，破坏了"契约稳定"的前提。

Context Injection 的设计目标：

> **在不改变回调签名（契约）的前提下，让每一次注册都能携带"专属于这次注册"的私有数据。**

Kernel 演进出了两种主流手段：

```
手段 A：通过一个 void* / unsigned long 参数携带上下文
        （早期 timer_list.data、tasklet_struct.data、大量 __user 回调的 void *priv）

手段 B：把"通用结构体"直接嵌入到"业务结构体"内部，
        回调拿到通用结构体指针后用 container_of() 反推出整个业务对象
        （struct list_head、struct work_struct、struct kobject、现代 timer_list）
```

---

## 2. Architecture

```
   Core（不知道具体业务类型）
        |
        | 只操作/只调用这个"通用契约字段"
        v
  +------------------+
  | 通用结构体         |   如 struct work_struct / struct list_head / struct timer_list
  | (只含 Core 需要的  |
  |  最小字段)          |
  +------------------+
        ^
        | 被"嵌入"进
        |
  +------------------------------+
  | struct my_business_object {  |
  |     ... 业务字段 ...          |
  |     struct work_struct work; |  <-- 嵌入位置任意，不要求是第一个字段
  | };                            |
  +------------------------------+
        |
        | 回调内部用 container_of(ptr, type, member) 反推
        v
  拿到完整的 struct my_business_object *
```

对比"手段 A"（`void *data` 参数）：

```
   Core 调用：       fn(data)
   回调内部：         struct my_obj *obj = (struct my_obj *)data;
   缺点：             类型不安全（编译器不检查 data 到底是不是指向 my_obj）
                      只能传一个指针大小的数据，无法直接"作为对象自身的一部分"存在
```

---

## 3. Lifecycle

```
   allocate
   (业务对象被创建，内部包含一个通用契约字段，如 struct work_struct work)
        |
        v
   initialize
   (INIT_WORK(&obj->work, my_fn) / setup_timer(&obj->timer, fn, (unsigned long)obj))
        |
        v
   inject
   (把这个通用字段的地址交给 Core：schedule_work(&obj->work) / add_timer(&obj->timer))
        |
        v
   framework owns dispatch
   (Core 只保存/操作通用字段的指针，决定何时触发)
        |
        v
   callback
   (Core 调用 my_fn(&obj->work) 或 my_fn(obj->data)；
    回调内部用 container_of() 或强转拿回整个 obj)
        |
        v
   destroy
   (业务对象释放时，通用字段随之释放；若 Core 还持有指向它的指针 —— 悬挂指针风险，见 Failure)
```

---

## 4. Relationship

```
   业务对象（struct flush_task / struct modern_watchdog）
        |
        | 内嵌
        v
   通用契约字段（struct mini_work / struct timer_list）
        |
        | 地址被传给
        v
   Core（workqueue / timer 核心）
        |
        | 回调时把该地址传回
        v
   回调函数
        |
        | container_of() 还原
        v
   业务对象（拿回了完整的、类型安全的指针）
```

这是一个"**去了又回来**"的环形关系：业务对象 -> 注入通用字段地址给 Core -> Core 回调时把地址还给业务代码
-> 业务代码用 `container_of` 还原回业务对象。全程 Core 都不知道"业务对象"这个类型存在。

---

## 5. Data Flow

以 `worker_thread_pool.c` 的 `flush_task` 为例：

```
main()
  |
  +- INIT_WORK(&ft1.work, flush_task_fn)     // 初始化：把回调塞进内嵌的 work 字段
  |
  +- queue_work(&g_wq, &ft1.work)            // 注入：只把 &ft1.work 这个地址交给 Core
        |
        v
   worker_thread() 循环
        |  取出 w = ft1.work 的地址
        v
   w->func(w)                                 // Core 盲调，参数只是 work 指针
        |
        v
   flush_task_fn(struct mini_work *w)
        |
        | struct flush_task *ft = container_of(w, struct flush_task, work);
        v
   完整还原出 ft->device_name / ft->dirty_pages   // Context Injection 生效的确切位置
```

对照真实 Kernel（现代 `timer_setup()` 风格，`kernel/time/timer.c`）：

```
call_timer_fn(timer, timer->function)
  |
  v
timer->function(timer)          // 只传 timer_list 指针
  |
  v
my_timer_fn(struct timer_list *t)
  |
  | struct my_dev *dev = container_of(t, struct my_dev, timer);
  v
拿到完整的 my_dev
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| 通用契约字段本身（`work.func`、`timer.function`） | 注册后基本不变 | 一旦 `INIT_WORK`/`timer_setup` 后很少再改 |
| 内嵌字段在业务对象里的**偏移量**（offset） | **编译期固定，运行期绝对不变** | `container_of` 的正确性完全依赖这个偏移量在编译期就确定，这是它比"猜测/假设首字段"更安全的原因 |
| 业务对象的其它字段（`dirty_pages`、`timeout_count`） | **可变**，且只有业务代码自己会修改 | Core 完全不感知，也不允许感知 |
| "旧式" `data` 字段（`unsigned long`） | 值不变，但**类型信息在编译期就丢失了** | 这是它相比 `container_of` 方式的根本劣势 |

---

## 7. Failure

1. **`container_of` 的类型假设被打破**：如果错误地对一个"根本没有内嵌该字段"的指针调用 `container_of`，
   编译器不会报错（`offsetof` 依然能算出一个数），但运行期会算出一个完全错误的地址——这是最隐蔽的一类 bug，
   通常表现为"看起来正常但字段值全是垂圾"。
2. **悬挂指针（dangling pointer）**：业务对象被释放后，如果 Core 侧还留有指向其内嵌字段的指针
   （比如 `worker_thread_pool.c` 里如果 `flush_task` 在 `queue_work()` 后立刻被 `free`），
   worker 线程稍后访问 `w->func` 就是"读已释放内存"——**这是 Context Injection 最大的生命周期风险**：
   业务对象的生命周期必须覆盖"从注入到回调执行完成"的整个区间。
3. **`unsigned long data` 方式的类型不安全**：`legacy_watchdog_fn` 把 `data` 强转成
   `struct watchdog_device *`——如果有人注册时不小心传了别的指针类型，编译器完全不会警告，
   只有运行期解释数据时才会发现字段对不上，这正是 Kernel 后来把 `timer_list.function` 签名从
   `(unsigned long)` 改成 `(struct timer_list *)` + `container_of` 的原因（4.14->4.15 大规模重构，
   见 `9c62a68d1317` 系列 commit）。
4. **多次注入同一个内嵌字段**：如果同一个 `work_struct` 被 `queue_work()` 两次（还没执行完就再入队），
   在没有 `WORK_STRUCT_PENDING` 一类标志位保护的自制实现里，会导致链表被破坏（两次入队指向同一块内存的
   `next` 字段互相覆盖）。真实 Kernel 用 `test_and_set_bit` 类原子操作防止重复入队。
5. **偏移量在联合体/多重继承式嵌入下变得复杂**：如果业务对象里嵌入了多个同类型的通用字段
   （比如两个 `struct list_head`，分别挂在两个不同的链表上），必须保证 `container_of` 第三个参数
   （member 名）与实际触发回调的那个字段完全对应，写错字段名会得到错误但"看起来合理"的地址。
6. **调试方法**：
   - 用户态：`gdb` 里可以直接 `print &((struct flush_task *)0)->work` 手动核对 offset 是否符合预期；
     `AddressSanitizer`（`-fsanitize=address`）能第一时间抓到"业务对象已释放但仍被访问"的悬挂指针问题。
   - Kernel：`CONFIG_DEBUG_OBJECTS_WORK` 专门用来检测 `work_struct` 生命周期错误（重复入队、
     对象释放前未 flush 完成的 work）；`slub_debug` 可以在对象释放后填充毒化字节（poison pattern），
     一旦回调再访问就会读到明显异常的值，便于定位。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `container_of()` 本身

真实定义：[include/linux/kernel.h:659](../../include/linux/kernel.h)

```c
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
```
为什么是 DI 的基础设施：这一个宏是"Core 只认识通用字段，实现却能拿回完整业务对象"的全部魔法所在，
后面 4 个例子都在用它。

### Example 2 — `struct work_struct` 与 `container_of`

场景：驱动的中断处理函数不能睡眠，把耗时操作 defer 到 workqueue，需要携带"是哪个设备"的上下文。

```c
struct my_device {
	void __iomem *regs;
	struct work_struct irq_work;   /* 内嵌 */
};

static void my_irq_work_fn(struct work_struct *work)
{
	struct my_device *dev = container_of(work, struct my_device, irq_work);
	handle_slow_path(dev->regs);   /* 拿回了完整设备上下文 */
}

/* 硬件中断上下文（不能睡眠）：*/
irqreturn_t my_isr(int irq, void *dev_id)
{
	struct my_device *dev = dev_id;   /* 这里是"手段 A"：request_irq 的 dev_id 参数 */
	schedule_work(&dev->irq_work);     /* 注入：只交出内嵌字段的地址 */
	return IRQ_HANDLED;
}
```

### Example 3 — `struct kobject` 与设备模型

场景：`struct device`/`struct kset` 等对象都内嵌 `struct kobject`，sysfs core 只操作 `kobject`，
需要反查回具体是哪个 `device` 时用 `container_of`。

```c
struct device {
	...
	struct kobject kobj;
};

#define to_dev(obj) container_of(obj, struct device, kobj)

/* drivers/base/core.c 风格的 sysfs 属性读取回调 */
static ssize_t dev_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct device *dev = to_dev(kobj);   /* sysfs core 不知道 device，只知道 kobject */
	return dev->show(dev, attr, buf);
}
```

### Example 4 — `struct list_head` 侵入式链表

场景：几乎所有 Kernel 双向链表都不是"链表节点包裹用户数据"，而是"用户数据里嵌一个 `list_head`"，
这样一个对象可以同时挂在多条链表上（比如一个 `task_struct` 同时在运行队列和某个等待队列里）。

```c
struct task_struct {
	struct list_head run_list;   /* 挂在调度队列 */
	struct list_head tasks;      /* 挂在全局任务链表 */
};

list_for_each_entry(p, &runqueue, run_list) {   /* 这个宏内部就是 container_of */
	schedule_task(p);
}
```

### Example 5 — 现代 `timer_setup()` + `container_of`（对照本仓库旧式 `data` 参数）

场景：本仓库（较旧内核）用 `void (*function)(unsigned long data)`；4.15+ 的内核改为
`void (*function)(struct timer_list *)`，靠 `container_of` 取代 `unsigned long data`，
用户态示例 `timer_context_demo.c` 里两种写法都实现了，可以直接对照。

```c
/* 新式，type-safe */
struct my_dev { struct timer_list timer; int count; };

static void my_timer_fn(struct timer_list *t)
{
	struct my_dev *dev = container_of(t, struct my_dev, timer);
	dev->count++;
}

timer_setup(&dev->timer, my_timer_fn, 0);
mod_timer(&dev->timer, jiffies + HZ);
```
为什么是 DI：无论新旧写法，本质都是"用一个 Core 认识的通用字段作为运输载体，把业务上下文注入到回调调用现场"。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/03-context-injection/`](c/03-context-injection/)

### 9.1 `container_of_demo.c` — 手写 `container_of` + 侵入式链表

- 场景：一份通用 `list_node` 遍历代码（Core），同时服务 `task` 和 `device` 两种完全不同的业务对象。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o container_of_demo container_of_demo.c`
- 实测输出（节选）：
  ```
  === container_of_demo ===

  [core] scanning task queue via generic list_node...
    [core] running task id=1 name='flush-cache' priority=5 (node@0x7fff28ec7d88 task@0x7fff28ec7d60, offset=40)
    [core] running task id=2 name='sync-disk' priority=1 (node@0x7fff28ec7dc8 task@0x7fff28ec7da0, offset=40)
    [core] running task id=3 name='network-poll' priority=9 (node@0x7fff28ec7e08 task@0x7fff28ec7de0, offset=40)

  [core] scanning device list via the *same* generic list_node code...
    [core] device bus='pci' vendor_id=0x8086
    [core] device bus='usb' vendor_id=0x1d6b

  -- 演示：从队列中移除 t2，验证链表结构完整性 --
  [core] scanning task queue via generic list_node...
    [core] running task id=1 name='flush-cache' priority=5 (node@0x7fff28ec7d88 task@0x7fff28ec7d60, offset=40)
    [core] running task id=3 name='network-poll' priority=9 (node@0x7fff28ec7e08 task@0x7fff28ec7de0, offset=40)

  === done: 同一份 list_node 遍历代码 (Core)，
      通过 container_of() 服务了两种完全不同的对象类型 (task/device) ===
  ```
  注意输出里打印的 `offset=40`：这是 `struct task` 里 `node` 字段相对对象起始地址的真实偏移量
  （`name[32]` + `id`/`priority` 两个 `int` 共 40 字节），直观验证了 `container_of` 是"减去一个编译期常量"。
- 为什么是 DI：`run_task_queue`/`scan_device_list` 里的遍历逻辑（Core）完全类型无关，
  只有 `task_from_node`/`device_from_node` 这两行 `container_of` 调用知道真实类型——这就是
  "Core 只认识通用字段，业务代码负责还原上下文"的直接体现。

### 9.2 `worker_thread_pool.c` — 线程池模拟 `work_struct`

- 场景：pthread 实现的 workqueue，`flush_task`/`retransmit_task` 两种业务对象内嵌 `mini_work`，
  worker 线程盲调 `w->func(w)`，回调内部用 `container_of` 取回完整业务对象。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o worker_thread_pool worker_thread_pool.c`
- 实测输出：
  ```
  === worker_thread_pool demo ===

  -- 主线程提交 4 个任务到 workqueue --
    [flush] device='sda1' flushing 128 dirty pages (tid=139863526876736)
    [flush] device='sda1' done
    [retransmit] seq=1001 attempt=1
    [flush] device='sdb1' flushing 64 dirty pages (tid=139863526876736)
    [flush] device='sdb1' done
    [retransmit] seq=1002 attempt=3

  -- 停止 workqueue --
  [worker] exiting, processed 4 work item(s) total

  === done, total processed = 4 ===
  ```
  （worker 线程 tid 在多次运行中保持稳定但值本身依赖系统，属于正常现象；任务按 FIFO 顺序处理，
  与提交顺序一致，验证了队列语义正确。）
- 为什么是 DI：`worker_thread()`（Core）从未 `#include` `flush_task`/`retransmit_task` 的定义，
  只操作 `struct mini_work`；新增第三种任务类型（比如 `struct compress_task`）不需要修改 worker 逻辑一行代码。

### 9.3 `timer_context_demo.c` — 新旧两代 timer context 携带方式对照

- 场景：同时实现"旧式 `unsigned long data`"和"新式内嵌 `timer_list` + `container_of`"两种上下文注入方式，
  直观对比类型安全性差异。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o timer_context_demo timer_context_demo.c`
- 实测输出：
  ```
  === timer_context_demo ===

  -- 模拟时钟推进，每次 tick 检查所有定时器 --
  tick 1:
  tick 2:
  tick 3:
    [modern] watchdog 'eth1-wd' fired, timeout_count=1 (timer@0x7ffd63ba10d8 wd@0x7ffd63ba10b0)
  tick 4:
  tick 5:
    [legacy] watchdog 'eth0-wd' fired, timeout_count=1 (data=0x7ffd63ba1080)
  tick 6:
  tick 7:
    [delayed_work] timer expired -> would schedule_work() to flush 'log-buffer' now
  tick 8:

  === done ===
  legacy watchdog fired 1 time(s)
  modern watchdog fired 1 time(s)
  delayed_flush fired = 1
  ```
- 为什么是 DI：`legacy_run_timers`/`modern_run_timers`（Core）都不知道 `watchdog_device`/
  `modern_watchdog`/`delayed_flush` 这些具体类型；两种方式的差别仅在于"上下文以什么形式被注入"，
  这正是本篇要凸显的"同一设计目标，两种实现手段"的对比。

一次性编译并运行全部三个示例：`cd c/03-context-injection && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Context Injection | Function Pointer Callback | Ops Table | Driver Registration |
|---|---|---|---|---|
| 解决的问题 | "回调怎么访问自己的私有数据" | "谁来实现这个动作" | "谁来实现这一整套动作" | "谁是这一整个设备/驱动实例" |
| 是否需要额外的宏/惯例 | **是**，`container_of` 或 `void *` 传参 | 否 | 否 | 通常配合 Context Injection 一起用 |
| 类型安全性 | `container_of` 方式安全；`void *`/`unsigned long` 方式不安全 | 由函数签名保证 | 由函数签名保证 | 由 `struct device_driver` 内嵌字段保证 |
| 何时用"内嵌 + container_of" | 通用字段本身就要被 Core 持有/挂链表时（`work_struct`/`list_head`/`timer_list`） | — | — | — |
| 何时用"void * / data 参数" | 只需要传一个简单指针，不需要把通用字段挂到 Core 的数据结构里 | — | — | — |

一句话：**Context Injection 是"给回调装上眼睛”**——没有它，Callback/Ops Table 只能操作全局状态或者
硬编码的单一实例；有了它，同一份回调代码可以服务任意多个、互相独立的对象实例。下一篇
[04-driver-registration.md](04-driver-registration.md) 会展示：**Callback + Ops Table + Context Injection
三者合体**，就是完整的"驱动注册"范式。
