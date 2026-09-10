# 01. Function Pointer Callback

> 系列索引见 [00-mental-model.md](00-mental-model.md)。本篇是全系列"原子"手法：
> 其余所有手法（ops table、notifier、workqueue、LSM hook...）都是"一堆 callback 的组织方式"。

---

## 1. Why

C 语言没有接口（interface）、没有虚函数、没有 lambda 闭包捕获（C99 之前甚至没有匿名函数）。
但 Kernel Core（比如 VFS、调度器）必须能够：

> "在写这段代码的时候，还不知道将来会调用谁的实现，但要求编译期就能类型检查、运行期能正确跳转过去。"

如果不用 function pointer callback，唯一的替代方案是：

```c
switch (device_type) {
case DEV_EXT4:  ext4_do_something();  break;
case DEV_NFS:   nfs_do_something();   break;
case DEV_PROC:  proc_do_something();  break;
/* 新增一种文件系统，就要回来改这个 switch */
}
```

这是**开闭原则的反面**：核心代码要为每一种新实现反复修改、反复重新编译、反复重新测试整个内核。
Function Pointer Callback 把这个 `switch` **换成一次间接跳转**，新增实现变成"纯粹的加法"：

```c
void (*handler)(void);
handler();     /* 不关心 handler 到底指向谁 */
```

设计目标：

- **开闭原则**：新增实现不修改 Core 代码。
- **可分离编译**：Core 和实现可以在不同的 `.c` 文件、甚至不同的编译单元（模块）里独立编译。
- **零运行时开销**：一次指针解引用 + 一次 `CALL`，没有反射、没有虚函数表查找的额外跳转层。

---

## 2. Architecture

```
                 Core (Framework)
             (不知道也不关心具体实现)
                       |
                       | 持有一个函数指针字段
                       v
              +-----------------+
              |  void (*fn)(…)  |   <-- 契约：类型签名
              +-----------------+
                       |
             在初始化/注册时被"注入"
                       |
                       v
              +-----------------+
              | 具体实现 A / B / C|
              +-----------------+
```

对应到本文档的三个用户态示例：

```
plugin_system:        framework.plugins[i]->on_event      (数组表 + 回调)
event_loop_reactor:    reactor.handlers[i]->on_readable     (fd -> 回调映射)
command_dispatcher:    dispatcher.table[i].handler          (name -> 回调映射)
```

---

## 3. Lifecycle

```
   allocate                 (定义一个 struct，里面有函数指针字段)
      |
      v
   initialize                (把具体函数地址赋值给该字段)
      |
      v
   inject                    (把这个 struct 交给 Core：注册/挂到链表/存进数组)
      |
      v
   framework owns dispatch   (Core 决定"什么时候"调，调用者/实现者不控制时机)
      |
      v
   callback                  (Core 执行 obj->fn(...)，进入具体实现)
      |
      v
   destroy                   (从 Core 注销，释放/清空该函数指针，防止 dangling call)
```

三个示例里分别是：

- `plugin_system`：`framework_register_plugin()` -> `framework_dispatch()` -> `framework_unregister_plugin()`
- `event_loop_reactor`：`reactor_add_handler()` -> `select()` 就绪后调用 -> `reactor_remove_handler()`
- `command_dispatcher`：`register_command()` -> `dispatcher_execute()`（进程结束前无显式注销，等价于"静态注册，永不注销"）

---

## 4. Relationship

```
   Core（framework / reactor / dispatcher）
        |  持有
        v
   一份"表"（数组 / 链表 / hash）
        |  每一项是
        v
   一个"契约对象"（struct plugin / struct fd_handler / struct cmd_entry）
        |  包含
        v
   一个或多个函数指针字段（on_event / on_readable / handler）
        |  在运行期指向
        v
   具体实现函数（logger_on_event / echo_on_readable / cmd_sum）
```

关键点：**"表"和"契约对象"由 Core 定义类型，但由外部实例创建并填充** —— 这就是 DI 的落地方式。

---

## 5. Data Flow

以 `event_loop_reactor.c` 为例，完整调用链：

```
main()
  |
  +- reactor_add_handler(&h_echo)     // 注入：把 echo_on_readable 塞进 reactor 的表
  |
  +- reactor_run(5)
        |
        v
     select(...)                      // Core 自己的调度逻辑，与实现无关
        |  fd 就绪
        v
     h->on_readable(h, h->fd)         // <-- DI 发生的确切位置：运行期盲调
        |
        v
     echo_on_readable(self, fd)       // 具体实现，reactor 完全不知道这里在干什么
        |
        v
     read(fd, ...)  / printf(...)
```

对照 Kernel 真实调用链（VFS 读文件，下一篇 `02-ops-table.md` 详解，这里先建立直觉）：

```
sys_read()
  |
  v
vfs_read()
  |
  v
file->f_op->read(file, buf, count, pos)   // 与 h->on_readable(...) 是同一种结构
  |
  v
ext4_file_read() / pipe_read() / ...
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| 函数指针字段本身（如 `on_event`） | **注册后基本只读** | 一旦注入，Core 不会再修改它，只会读取并调用 |
| Core 的"表"（数组/链表长度） | **可变** | 注册/注销会改变表的内容和大小 |
| `private_data` / `ctx` | **可变** | 具体实现自己维护的状态（如 `logger_state.event_count`），Core 完全不感知 |
| 注册顺序 | **影响执行顺序** | `framework_dispatch()` 按数组下标顺序调用，顺序敏感场景要格外小心 |

不变的是"契约"本身——即函数指针的类型签名。这也是为什么 Kernel 修改一个 `struct xxx_operations`
的字段类型是"ABI 破坏性变更"：所有实现都要同步改。

---

## 7. Failure

1. **NULL callback 未检查**：注册了一个 `on_event == NULL` 的插件，Core 调用时直接 crash。
   -> 示例中 `framework_register_plugin()` 显式检查并拒绝，这是必须做的防御。
2. **回调生命周期比 Core 短**：如果实现所在的模块被卸载（`rmmod`），但 Core 里还留着指向它的函数指针，
   下次调用就是**悬挂指针调用**（等价于跳到一段已经被释放/被其它代码覆盖的内存）。Kernel 靠
   `module_put()`/引用计数确保"模块使用中不能被卸载"。
3. **回调内部修改正在被遍历的表**：`event_loop_reactor.c` 里 `echo_on_readable()` 在被调用时可能调用
   `reactor_remove_handler()` 修改 `g_reactor.handlers[]`，如果 `reactor_run()` 不拷贝快照直接遍历原表，
   会导致数组错位或重复调用——示例中用 `snapshot[]` 规避了这个问题。
4. **重复注册/命名冲突**：`command_dispatcher.c` 演示了注册前检查重名，避免两个实现"抢占"同一个分发入口。
5. **返回值语义不一致**：多个实现如果对返回值的约定不统一（有的返回 0/1，有的返回 errno 负数），
   Core 侧的错误处理会出错——这正是 Kernel 全局统一"负数 errno" 约定的原因。
6. **调试方法**：
   - 用户态：`gdb` 对函数指针变量 `p/a handler` 可以直接看到指向哪个符号；`objdump -d` 找到函数地址对应关系。
   - Kernel：`WARN_ON(!ops->fn)` 防御性检查；`ftrace` 的 `function_graph` 可以看到实际跳入了哪个符号；
     `/proc/kallsyms` 配合崩溃时的返回地址可以定位是哪个模块的回调出了问题。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `schedule_work()` -> workqueue -> `work->func(work)`

场景：驱动想把一段耗时操作丢到进程上下文异步执行。
核心数据结构：`struct work_struct { work_func_t func; struct list_head entry; ... }`
（真实定义与调用见 [kernel/workqueue.c](../../kernel/workqueue.c) `process_one_work()`，本系列 07 篇详细展开）。

```
/* driver 侧：注入 */
static void my_work_fn(struct work_struct *work) { ... }
INIT_WORK(&dev->my_work, my_work_fn);
schedule_work(&dev->my_work);

/* core 侧：kernel/workqueue.c process_one_work() 简化 */
static void process_one_work(struct worker *worker, struct work_struct *work)
{
	work_func_t f = work->func;      /* 取出被注入的函数指针 */
	...
	f(work);                          /* <-- 盲调，worker 不知道 f 是谁 */
}
```
为什么是 DI：`process_one_work()` 从不 `#include` 任何具体驱动，只认 `work_func_t` 这个签名。

### Example 2 — VFS `file->f_op->read`

场景：`read(2)` 系统调用要读一个 fd，但 fd 后面可能是 ext4 文件、管道、`/proc` 文件……
核心数据结构：`struct file { const struct file_operations *f_op; ... }`
（真实定义见 [include/linux/fs.h:1583](../../include/linux/fs.h)）。

```
/* core 侧：fs/read_write.c vfs_read() 简化 */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	if (file->f_op->read)
		return file->f_op->read(file, buf, count, pos); /* <-- 盲调 */
	return -EINVAL;
}
```
为什么是 DI：`vfs_read()` 编译时根本不知道 ext4/nfs 是否存在；`f_op` 在 `open()` 时才被具体文件系统注入。

### Example 3 — 中断处理 `irq_desc->action->handler(irq, dev_id)`

场景：`request_irq()` 注册一个中断处理函数，硬件触发中断后，中断分发核心逐一调用挂在该 IRQ 上的所有 handler。

```
/* driver 侧：注入 */
request_irq(irq_num, my_irq_handler, IRQF_SHARED, "mydev", dev);

/* core 侧：kernel/irq/handle.c handle_irq_event_percpu() 简化 */
for (action = desc->action; action; action = action->next) {
	ret = action->handler(irq, action->dev_id);   /* <-- 盲调 */
	if (ret == IRQ_HANDLED)
		break;
}
```
为什么是 DI：中断核心不知道 `my_irq_handler` 内部做什么，只关心它遵守 `irq_handler_t` 签名和返回值约定。

### Example 4 — 定时器 `timer_list->function(timer->data)`

场景：`add_timer()` 注册一个超时回调，时钟中断触发后由 `run_timers()` 逐一调用到期的定时器函数。

```
/* driver 侧：注入 */
setup_timer(&dev->watchdog, watchdog_timeout, (unsigned long)dev);
mod_timer(&dev->watchdog, jiffies + HZ);

/* core 侧：kernel/timer.c __run_timers() 简化 */
while ((timer = next_expired_timer(base, jiffies))) {
	void (*fn)(unsigned long) = timer->function;
	unsigned long data = timer->data;
	detach_timer(timer);
	fn(data);      /* <-- 盲调，data 就是 Context Injection，见 03 篇 */
}
```
为什么是 DI：`__run_timers()` 从不知道 `watchdog_timeout` 是谁写的，只按照签名 `void (*)(unsigned long)` 调用。

### Example 5 — `notifier_call_chain()`

场景：系统发生某个"事件"（如 CPU 上线/下线），多个子系统都想被通知，但不能让核心代码写死"谁要被通知"。
真实实现见 [kernel/notifier.c:74](../../kernel/notifier.c)（第 05 篇会完整展开，这里先看回调调用点）。

```
/* 具体子系统侧：注入 */
static int my_cpu_callback(struct notifier_block *nb, unsigned long action, void *data)
{ ... return NOTIFY_OK; }
static struct notifier_block my_nb = { .notifier_call = my_cpu_callback };
register_cpu_notifier(&my_nb);

/* core 侧：kernel/notifier.c notifier_call_chain() 简化 */
static int notifier_call_chain(struct notifier_block **nl, unsigned long val, void *v, ...)
{
	struct notifier_block *nb = *nl;
	while (nb) {
		ret = nb->notifier_call(nb, val, v);   /* <-- 盲调，链上每一个都调 */
		nb = nb->next;
	}
	return ret;
}
```
为什么是 DI：`notifier_call_chain()` 遍历的链表是运行期动态构建的，核心代码在编译期完全不知道链上会挂多少个、是谁。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/01-function-pointer-callback/`](c/01-function-pointer-callback/)

### 9.1 `plugin_system.c` — 插件系统

- 场景：框架加载多个插件，插件通过 `on_event` 回调接收框架广播的事件；同时演示生命周期回调
  （`init`/`exit`）与 `private_data`（Context Injection 的雏形）。
- 编译：
  ```
  cd c/01-function-pointer-callback
  gcc -Wall -Wextra -O2 -std=gnu11 -o plugin_system plugin_system.c
  ```
- 实测输出：
  ```
  [framework] plugin 'broken' has NULL on_event, reject
  === plugin_system demo ===

  -- 注册阶段 --
  [framework] registered plugin 'logger' (slot 0)
    [logger] init, private_data allocated at 0x92842b0
  [framework] registered plugin 'alert' (slot 1)

  -- 运行阶段：框架广播事件，不知道插件内部实现 --
  [framework] dispatching event 'startup' to 2 plugin(s)
    [logger] event #1: 'startup'
    [alert] ignoring non-error event 'startup'
  [framework] dispatching event 'error' to 2 plugin(s)
    [logger] event #2: 'error'
    [alert] !!! got an error event, paging on-call !!!
  [framework] dispatching event 'shutdown' to 2 plugin(s)
    [logger] event #3: 'shutdown'
    [alert] ignoring non-error event 'shutdown'

  -- 卸载阶段 --
    [logger] exit, saw 3 events total, freeing 0x92842b0
  [framework] unregistered plugin 'logger'
  [framework] unregistered plugin 'alert'

  === done, remaining plugins: 0 ===
  ```
  （第一行 `[framework] plugin 'broken' has NULL on_event, reject` 因 stderr 无缓冲先于 stdout 刷出，
  属于终端缓冲交错，不影响逻辑：`broken_plugin` 确实被拒绝注册。）
- 为什么是 DI：`framework_dispatch()` 永远不知道 `logger_on_event`/`alert_on_event` 内部逻辑，
  它只调用契约里约定的函数指针；新增一个插件（比如 `metrics_plugin`）**不需要修改 framework 任何代码**。

### 9.2 `event_loop_reactor.c` — Reactor 事件循环

- 场景：一个基于 `select()` 的 Reactor，fd 就绪后回调对应 handler；模拟 Kernel 中断/IRQ 分发模型。
- 编译：
  ```
  gcc -Wall -Wextra -O2 -std=gnu11 -o event_loop_reactor event_loop_reactor.c
  ```
- 实测输出：
  ```
  === event_loop_reactor demo ===

  [reactor] added handler 'echo-handler' on fd=5
  [reactor] added handler 'counter-handler' on fd=7

  -- 运行事件循环 --
    [echo] (#1) got 14 bytes: hello reactor
    [counter] fd=7 fired 1 time(s), consumed 2 bytes
  [reactor] event loop exiting after 5 iterations

  === done, echo saw 1 line(s), counter fired 1 time(s) ===
  ```
- 为什么是 DI：`reactor_run()` 的 `select()` 调度逻辑与 `echo_on_readable`/`counter_on_readable` 的具体
  实现完全解耦；给 reactor 加一种新 handler 类型，不需要改 `reactor_run()` 一行代码。

### 9.3 `command_dispatcher.c` — 命令分发器

- 场景：字符串命令名到处理函数的映射表，模拟 syscall table / ioctl cmd 分发。
- 编译：
  ```
  gcc -Wall -Wextra -O2 -std=gnu11 -o command_dispatcher command_dispatcher.c
  ```
- 实测输出：
  ```
  [dispatcher] command 'echo' already registered
    [fail] intentionally returning error
  [dispatcher] unknown command 'nosuchcmd'
  === command_dispatcher demo ===

  [dispatcher] registered command 'echo' -> 0x401430
  [dispatcher] registered command 'sum' -> 0x4014c0
  [dispatcher] registered command 'fail' -> 0x401490

  [dispatcher] 3 command(s) registered:
      echo       : print arguments back
      sum        : sum integer arguments, keep running total
      fail       : always fail, for error-path demo

  -- 执行脚本 --
  > echo hello from dispatcher
  [dispatcher] dispatch 'echo' -> handler=0x401430
    [echo] hello from dispatcher 
    (exit code = 0)
  > sum 1 2 3
  [dispatcher] dispatch 'sum' -> handler=0x4014c0
    [sum] this call = 6, running total across 1 call(s) = 6
    (exit code = 0)
  > sum 10 20
  [dispatcher] dispatch 'sum' -> handler=0x4014c0
    [sum] this call = 30, running total across 2 call(s) = 36
    (exit code = 0)
  > fail
  [dispatcher] dispatch 'fail' -> handler=0x401490
    (exit code = -1)
  > nosuchcmd foo bar
    (exit code = -1)

  === done, sum called 2 time(s), final total = 36 ===
  ```
  （同样，前三行是 stderr 提前刷出的告警，对应"重复注册 echo 被拒绝"、"fail 命令执行"、"未知命令"三个
  校验点，均符合预期。）
- 为什么是 DI：`dispatcher_execute()` 只依赖 `cmd_handler_fn` 签名和名字查找表；`cmd_sum` 通过
  `ctx` 参数（`struct sum_ctx *`）携带私有状态，这正是下一篇 `03-context-injection.md` 的雏形。

一次性编译并运行全部三个示例：`cd c/01-function-pointer-callback && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Function Pointer Callback | Ops Table | Notifier Chain | Driver Registration |
|---|---|---|---|---|
| 注入粒度 | 单个函数指针 | 一组相关函数指针（结构体） | 单个函数指针 + 优先级 | 一整个"对象族"（含 ops + 元数据） |
| 典型场景 | "某一件事发生时调用我" | "这一整套操作我都要实现" | "多个人都想知道这件事发生了" | "我是一种新硬件/新设备类型" |
| 支持多实现同时存在 | 视 Core 设计（表/链表可支持） | 否——一个对象绑定一个 ops | **是**，天然设计成链表 | 是，按总线匹配挑一个 |
| 何时用 Callback 而不是 Ops Table | 只需要注入"一个动作"，不需要一整套相关行为 | — | — | — |
| 何时不要用 | 如果一个对象要实现 5+ 个相关操作，应该升级为 ops table（见 02 篇），否则字段爆炸、语义分散 | — | — | — |

一句话：**Callback 是原子操作，Ops Table 是"一组 Callback 的结构化打包”，Notifier 是"支持多订阅者的 Callback”，
Driver Registration 是"Callback + Ops Table + 元数据的完整对象化”**。后面几篇会逐层往上叠加复杂度。
