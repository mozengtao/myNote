# 14. Tracepoint

> 05 篇 Notifier Chain 和本篇都是"一对多广播"结构，但设计目标截然不同：
> Notifier Chain 是"业务逻辑需要被通知"（有人注册就一定有真实的业务意义）；
> Tracepoint 是"诊断/调试用途的探测点"，**绝大多数时间根本没人订阅**，
> 必须做到"没人订阅时开销趋近于零"，否则所有从不使用 tracing 的用户都要
> 白白为"万一有人订阅"付出代价。这是本篇要讲清楚的核心矛盾。

---

## 1. Why

内核到处都需要"观测点"：每次调度切换、每次系统调用进出、每次内存分配、每次
中断处理……如果要调试一个复杂的性能问题或竟态条件，往往需要在这些点上临时
"挂一个探针"，记录发生了什么、什么时候发生、带着什么参数。但这类观测点有一个
和前面 13 篇讲过的所有机制都不同的核心约束：

1. **默认状态下几乎永远没有人订阅**：一台生产环境的服务器，99% 的运行时间里
   不会有人在跑 `ftrace`/`perf record`，但内核里几万个可能的探测点依然
   编译在里面、依然会被执行路径经过。如果每次经过探测点都要付出哪怕是"检查一个
   链表是否为空"的开销，乘以每秒可能触发几百万次的高频路径（调度切换、
   系统调用），总开销会显著拖慢完全不需要 tracing 的普通场景。
2. **一旦订阅，往往需要同时支持多个独立的消费者**：`ftrace` 的事件跟踪、
   `perf` 的采样、某个自定义内核模块的调试代码，可能同时挂在同一个探测点上，
   彼此互不知道对方存在，也不应该互相干扰。
3. **探测点本身不能改变被观测代码的行为**：这是 Tracepoint 与 05 篇 Notifier
   Chain 的关键差异——Notifier Chain 的订阅者可以合法地影响业务流程
   （比如 `NETDEV_UNREGISTER` 通知里某个订阅者可能需要做清理工作，这是
   业务逻辑的一部分）；Tracepoint 的探测点如果崩溃、如果耗时过长，都不应该
   影响被观测代码的正确性——它是纯粹的"旁观者"。

如果不用专门的 Tracepoint 机制，而是简单地在每个观测点写一个函数指针字段判空
后调用（08 篇之前几种手法的思路），在"没人订阅"这个绝大多数时间成立的场景下，
仍然要付出"读取一个指针、和 NULL 比较"的开销——这在调度器热路径这种每秒几十万次
调用的场景下，即使每次只有几个纳秒，累积起来也是可观测的。

Tracepoint 的设计目标：

> **在代码里插入一个静态、编译期确定、永远存在的"插入点"，这个插入点在没有 probe
> 挂载时的开销必须被压到硬件能提供的最低水平（理想情况下是零，一次代码路径都不
> 会真正执行），挂载了 probe 之后又能支持任意多个互相独立的消费者同时观测同一个
> 插入点，且消费者之间、消费者与被观测代码之间完全解耦。**

本篇讲清楚三件事：（1）"静态插入点"这个契约本身是什么样子；（2）"多个 probe
同时挂载"是怎么实现的；（3）"没人订阅时开销趋近零"这个承诺在本仓库版本里
具体是怎么兑现的，以及和更现代内核版本的差异。

---

## 2. Architecture

```
   业务热路径代码（Core，如调度器、系统调用入口、内存分配器）
        |
        | 无条件执行到这一行（这一行本身永远存在，不会被条件编译移除）
        v
   trace_xxx(...)（静态插入点，由 DECLARE_TRACE/TRACE_EVENT 宏生成）
        |
        | 检查 jump_label_key（本仓库支持 asm-goto 时是代码打洞的 NOP/JMP，
        | 不支持时退化为 atomic_read()，见 include/linux/jump_label.h）
        v
   +---------------------------+          +---------------------------+
   | key 未启用（没有 probe）      |          | key 已启用（至少一个 probe） |
   | 直接 return，几乎零开销       |          | 进入 __DO_TRACE()           |
   +---------------------------+          +---------------------------+
                                                      |
                                                      v
                                    +--------------------------------------+
                                    | tracepoint_func 数组（NULL 结尾）        |
                                    | [ftrace probe][perf probe][自定义 probe] |
                                    | 逐一盲调，互不知道对方存在                |
                                    +--------------------------------------+
```

真实定义：[include/linux/tracepoint.h:25](../../include/linux/tracepoint.h)、
[include/linux/tracepoint.h:30](../../include/linux/tracepoint.h)

```c
struct tracepoint_func {
	void *func;
	void *data;
};

struct tracepoint {
	const char *name;
	struct jump_label_key key;
	void (*regfunc)(void);
	void (*unregfunc)(void);
	struct tracepoint_func __rcu *funcs;
};
```

`__DECLARE_TRACE()` 宏（[include/linux/tracepoint.h:142](../../include/linux/tracepoint.h)）
展开出的静态插入点函数：

```c
static inline void trace_##name(proto)
{
	if (static_branch(&__tracepoint_##name.key))
		__DO_TRACE(&__tracepoint_##name, TP_PROTO(data_proto), TP_ARGS(data_args), cond);
}
```

---

## 3. Lifecycle

```
   allocate（编译期，静态数据）
   (TRACE_EVENT(page_fault, TP_PROTO(...), ...) 或 DECLARE_TRACE(name, proto, args)
    在头文件里声明；DEFINE_TRACE(name) 在某个 .c 文件里实例化出
    __tracepoint_##name 这个全局静态对象，落在 "__tracepoints" section)
        |
        v
   initialize（链接期）
   (链接器把所有 __tracepoints section 的对象收集到一起，
    __start___tracepoints_ptrs ~ __stop___tracepoints_ptrs 圈定边界，
    这是 13 篇讲的"链接器 section 自注册"元模式的又一个真实实例)
        |
        v
   dormant（绝大多数运行时间处于这个状态）
   (探测点在代码路径里被无条件执行到，但 key 未启用，
    trace_xxx() 内部的分支判断为假，直接返回，业务逻辑完全不受影响)
        |
        v
   inject（运行期，用户主动触发）
   (register_trace_xxx(probe, data) -> tracepoint_probe_register()
    -> 数组追加 probe -> jump_label_inc() 把 key 从 0 变为非 0/patch 代码)
        |
        v
   active（callback，被观测代码继续无条件执行到同一行代码，
   行为发生变化）
   (trace_xxx() 内部分支判断为真，进入 __DO_TRACE()，
    遍历 tracepoint_func 数组，逐一盲调所有已挂载的 probe)
        |
        v
   destroy（运行期，用户主动触发）
   (unregister_trace_xxx() -> tracepoint_probe_unregister()
    -> 数组移除该 probe -> 如果数组变空，jump_label_dec() 把 key 变回禁用状态
    -> tracepoint_synchronize_unregister() 等待所有正在进行中的探测调用完成
    （RCU grace period），再真正释放旧的 probe 数组内存)
```

**与前面几篇最大的生命周期差异**：01-05 篇的"注册"通常意味着"从此这个功能点
恒定生效直到注销"；本篇的插入点代码本身**不区分"注册前"和"注册后"**——它是
永远存在、永远会被执行到的一行代码，唯一变化的是这行代码内部"该不该往下走"
的判断结果。这与 06/13 篇"链接器 section 决定谁参与"的静态性也不同：
tracepoint 的"是否参与"（是否有 probe）是纯运行期概念，"插入点本身存在"才是
编译期/链接期概念——两个维度叠加，构成了本篇独有的生命周期形态。

---

## 4. Relationship

```
   业务热路径代码（Core）
        |
        | 持有（内嵌）
        v
   struct tracepoint __tracepoint_xxx（静态对象，随二进制/模块常驻）
        |
        | .key 字段决定要不要往下走
        | .funcs 字段指向
        v
   struct tracepoint_func 数组（NULL 结尾，运行期可增减）
        |
        | 每个元素关联
        v
   [ftrace 的 probe][perf 的 probe][自定义模块的 probe]……
        |
        | 每个 probe 通过 .data 字段携带（Context Injection，见 03 篇）
        v
   属于该 probe 自己的私有状态（本文档 tracepoint_multi_probe.c 里的
   struct ftrace_ctx/struct perf_ctx/struct custom_mod_ctx）
```

与 06 篇 initcall、13 篇 `__setup`/PCI fixup 的关系：`__tracepoint_xxx` 对象
本身落在 `__tracepoints` section 里，这一层的"收集"用的是 13 篇讲的通用元模式；
但 tracepoint 特有的是**每个收集到的对象自己内部又维护了一份运行期可变的
子链表（`funcs` 数组）**——13 篇的两个例子（`__setup`/PCI fixup）里，
每个收集到的对象都是"原子的、不可再分的"一条规则，而 tracepoint 的每个对象
是"一个可以挂载任意数量 probe 的插座"，这是本篇在"链接器 section 自注册"
这棵大树上长出的一个新分支。

---

## 5. Data Flow

以本文档 `tracepoint_multi_probe.c` 复刻的 `sched_switch` 场景为例，对照真实路径：

```
真实内核路径：
  kernel/sched.c:3160  trace_sched_switch(prev, next);
        |
        v
  trace_sched_switch()（由 TRACE_EVENT(sched_switch, ...) 展开，
  见 include/trace/events/sched.h:113）
        |
        v
  if (static_branch(&__tracepoint_sched_switch.key))
        | 真（至少一个 probe 挂载）
        v
  __DO_TRACE(&__tracepoint_sched_switch, ...)（include/linux/tracepoint.h:117）
        |
        v
  it_func_ptr = rcu_dereference_sched(tp->funcs)
  do {
      it_func = it_func_ptr->func;
      __data  = it_func_ptr->data;
      ((void(*)(proto))it_func)(args);      <-- 盲调，可能是 ftrace 也可能是 perf
  } while ((++it_func_ptr)->func);
```

对照本文档 `tracepoint_multi_probe.c` 的真实运行轨迹：

```
main()
  |
  +- trace_sched_switch("idle", "bash") x2   <-- 此刻 enabled=0，静默返回
  |
  +- tp_register(probe_ftrace, &ftrace_data)      <-- 对应 register_trace_sched_switch()
  +- tp_register(probe_perf, &perf_data)
  +- tp_register(probe_custom_module, &custom_data)
  |
  +- trace_sched_switch("idle", "nginx") x4   <-- 每次都依次触发 3 个 probe
  |     [ftrace ] #1 ...
  |     [perf   ] sample #1 ...
  |     [custom ] watched task 'nginx' involved ...
  |
  +- tp_unregister(probe_perf, &perf_data)   <-- 对应 unregister_trace_sched_switch()
  |
  +- trace_sched_switch("nginx", "idle")     <-- 只剩 ftrace 和 custom 收到事件
        [ftrace ] #5 ...
        [custom ] watched task 'nginx' involved ..., hit #4
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `struct tracepoint` 对象本身（地址、`.name`） | **编译期/链接期确定，运行期地址不变** | 与 06/13 篇的 section 收集对象性质相同 |
| `.key`（jump_label_key / 简化版 `enabled` 整数） | **运行期可变，且是本篇性能承诺的核心** | 从 0 到非 0 的转变触发的可能是一次原子操作，也可能是真实的机器码修改（asm-goto 架构） |
| `.funcs` 数组内容 | **运行期可增减**，真实实现用 RCU 保证"读者永远看到一份完整、一致的数组" | 本文档用户态示例用普通 `realloc` 简化，牺牲了并发安全性，仅用于演示单线程下的行为逻辑 |
| 每个 probe 的私有数据（`.data`） | 由 probe 自己管理，tracepoint 机制本身不关心内容 | 典型的 Context Injection（03 篇）应用 |
| 插入点在业务代码里的位置 | **编译期固定，不可运行期移动** | `trace_sched_switch(prev, next)` 这一行代码在 `kernel/sched.c` 里的位置一旦确定，就不会因为有没有人订阅而改变——这与 08 篇 LSM 的"强制检查点不可绕过"是同一种"位置固定性"，但目的不同（LSM 是安全要求，tracepoint 是可观测性要求） |

---

## 7. Failure

1. **probe 函数耗时过长拖慢热路径**：Tracepoint 的"零开销承诺"只覆盖"没人订阅"
   这一种状态；一旦订阅，`__DO_TRACE()` 是同步阻塞调用所有 probe，如果某个
   probe 实现里做了昂贵的操作（本文档 `tracepoint_enable_disable_runtime.c`
   的 `probe_realistic` 用 `snprintf` 格式化字符串，实测比禁用状态慢了约 40
   倍），会直接拖慢被观测的业务路径本身——这是"观测行为改变被观测系统性能"
   的经典陷阱（有时被称为"观测者效应"），真实 `ftrace`/`perf` 的实现极度关注
   probe 本身的执行效率，通常直接写入无锁的 per-CPU ring buffer 而不是做
   任何格式化/加锁操作。
2. **注销 probe 后立刻释放内存，但可能有并发调用者仍在使用旧数组**：这正是
   `tracepoint_synchronize_unregister()`（include/linux/tracepoint.h:82）
   存在的原因——真实实现用 `synchronize_sched()` 等待一个 RCU grace period，
   确保所有"可能正持有旧 `funcs` 数组指针、正在遍历中"的 CPU 都已经退出临界区，
   才能安全释放。本文档用户态示例是单线程程序，没有这个并发问题，但文档里
   明确指出这是真实实现里必须处理、而演示代码里被简化掉的一环。
3. **忘记维护 NULL 结尾哨兵**：`__DO_TRACE()` 的遍历写法是
   `do { ...; } while ((++it_func_ptr)->func);`——这个写法**依赖数组末尾
   必须有一个 `func == NULL` 的哨兵元素**，如果扩容/移除逻辑出现 bug 忘记
   正确维护这个哨兵，遍历会越界读到不属于这个数组的内存，调用一个随机地址
当作函数指针——这是本文档 `tracepoint_multi_probe.c`/`static_tracepoint_sim.c`
里 `tp_register()`/`tp_unregister()` 每次修改数组后都要重新写一个
`NULL` 哨兵的原因，也是真实 `tracepoint_entry_add_probe()`
（kernel/tracepoint.c:126）注释里"+ 2 : one for new probe, one for
NULL func"这句话在强调的事情。
4. **误以为 jump label 在所有架构/配置下都是真正的零开销**：本篇 Why 小节
   已经说明——只有在编译器支持 `asm goto` **且** `CONFIG_JUMP_LABEL=y` 时，
   `static_branch()` 才是真正的代码打洞机制；否则会退化成
   `include/linux/jump_label.h:71` 的 `atomic_read()` fallback，仍然是
   一次内存读 + 一次分支判断，虽然很便宜但绝不是字面意义上的"零"。
   本文档 `tracepoint_enable_disable_runtime.c` 如实测量的正是这条 fallback
   路径，并在代码注释和输出说明里明确标注了这一版本/配置差异。
5. **同名 tracepoint 重复定义**：真实 `DEFINE_TRACE(name)` 只能在一个编译单元
   里出现一次（否则链接期会出现重复符号错误），`DECLARE_TRACE` 可以在头文件里
   被多次 `#include`，这组"声明可以重复、定义只能一次"的约定与 02 篇 Ops Table
   的"契约可以被多处引用，实现只能有一份"是同一种设计思路的复用。
6. **调试方法**：
   - 用户态：本文档三个示例都在关键路径打印详细日志，`tracepoint_enable_disable_runtime.c`
     还给出了真实测量的纳秒级数字，可以直接对照"启用前后差异有多大"。
   - Kernel：`cat /sys/kernel/debug/tracing/available_events` 列出所有已编译进
     内核的 tracepoint；`echo sched:sched_switch > /sys/kernel/debug/tracing/set_event`
     启用一个具体的 tracepoint；`cat /sys/kernel/debug/tracing/trace` 查看已经
     记录的事件；`perf list` 列出 `perf` 能看到的 tracepoint 事件，
     `perf stat -e sched:sched_switch ...` 用某个 tracepoint 做统计；
     `cat /proc/sys/kernel/ftrace_enabled` 检查 ftrace 整体是否开启；
     如果怀疑某个 tracepoint 拖慢了系统，`perf record -e <event> -a` 加上
     `perf report` 可以观测该事件触发频率与开销。

---

## 8. Kernel Core Pseudocode（6 个）

### Example 1 — `struct tracepoint`/`struct tracepoint_func`：插座与插件的契约

真实定义：[include/linux/tracepoint.h:25](../../include/linux/tracepoint.h)、
[include/linux/tracepoint.h:30](../../include/linux/tracepoint.h)

```c
struct tracepoint_func {
	void *func;
	void *data;
};

struct tracepoint {
	const char *name;
	struct jump_label_key key;
	void (*regfunc)(void);
	void (*unregfunc)(void);
	struct tracepoint_func __rcu *funcs;
};
```
为什么是 DI：`struct tracepoint` 本身既是"契约"（`key` 决定要不要往下走）
又是"注册表头"（`funcs` 指向具体订阅者数组）——这与 05 篇 Notifier Chain
的 `notifier_block` 链表头结构角色相同，但多了 `key` 这个前置性能开关，
是本篇独有的结构。

### Example 2 — `struct jump_label_key`/`static_branch()`：性能开关的真实实现

真实定义：[include/linux/jump_label.h:9](../../include/linux/jump_label.h)、
[include/linux/jump_label.h:71](../../include/linux/jump_label.h)

```c
/* 支持 asm-goto 且 CONFIG_JUMP_LABEL=y 时：*/
struct jump_label_key {
	atomic_t enabled;
	struct jump_entry *entries;
};
static __always_inline bool static_branch(struct jump_label_key *key)
{
	return arch_static_branch(key);   /* 内联汇编，运行期代码打洞，真零开销 */
}

/* !HAVE_JUMP_LABEL fallback（本文档三个示例实际复刻的路径）：*/
static __always_inline bool static_branch(struct jump_label_key *key)
{
	if (unlikely(atomic_read(&key->enabled)))
		return true;
	return false;
}
```
为什么值得单列：这是本篇 Why 小节"零开销承诺"的技术根基，也是本篇与
06/13 篇最大的技术新颖点——前面所有手法的"判断要不要往下走"都至少要付出
一次内存读；jump label 在支持的架构上把这次内存读也省掉了，判断本身在机器码
层面消失，替换成一条可以被运行期改写的无条件跳转/空操作指令。

### Example 3 — `__DECLARE_TRACE()`：静态插入点函数的生成模板

真实定义：[include/linux/tracepoint.h:142](../../include/linux/tracepoint.h)

```c
#define __DECLARE_TRACE(name, proto, args, cond, data_proto, data_args)	\
	extern struct tracepoint __tracepoint_##name;				\
	static inline void trace_##name(proto)					\
	{									\
		if (static_branch(&__tracepoint_##name.key))			\
			__DO_TRACE(&__tracepoint_##name,			\
				TP_PROTO(data_proto), TP_ARGS(data_args), TP_CONDITION(cond)); \
	}									\
	static inline int register_trace_##name(void (*probe)(data_proto), void *data) \
	{ return tracepoint_probe_register(#name, (void *)probe, data); }	\
	static inline int unregister_trace_##name(void (*probe)(data_proto), void *data) \
	{ return tracepoint_probe_unregister(#name, (void *)probe, data); }
```
为什么是 DI：这一个宏同时生成了三样东西——插入点函数 `trace_##name()`、
注册函数 `register_trace_##name()`、注销函数 `unregister_trace_##name()`——
业务代码只需要在头文件里写一行 `DECLARE_TRACE(my_event, TP_PROTO(...), TP_ARGS(...))`，
剩下的"契约生成"完全由宏在编译期自动展开，本文档三个示例把这个宏的效果手工
展开成了具体的 `trace_page_fault()`/`trace_sched_switch()`/`trace_hot_path()`。

### Example 4 — `__DO_TRACE()`：遍历 probe 数组的真正实现

真实定义：[include/linux/tracepoint.h:117](../../include/linux/tracepoint.h)

```c
#define __DO_TRACE(tp, proto, args, cond)				\
	do {								\
		struct tracepoint_func *it_func_ptr;			\
		void *it_func;						\
		if (!(cond))						\
			return;						\
		rcu_read_lock_sched_notrace();				\
		it_func_ptr = rcu_dereference_sched((tp)->funcs);	\
		if (it_func_ptr) {					\
			do {						\
				it_func = (it_func_ptr)->func;		\
				__data = (it_func_ptr)->data;		\
				((void(*)(proto))(it_func))(args);	\
			} while ((++it_func_ptr)->func);		\
		}							\
		rcu_read_unlock_sched_notrace();			\
	} while (0)
```
为什么值得单列：`rcu_read_lock_sched_notrace()`/`rcu_dereference_sched()`
这一对操作，是真实内核在"多个 CPU 可能同时触发同一个 tracepoint，同时另一个
CPU 可能正在注册/注销 probe"这种并发场景下的正确性保证——本文档用户态示例
略去了这一层（单线程，没有并发），但文档在 Failure 小节明确指出了这个简化。

### Example 5 — `tracepoint_probe_register()`/`set_tracepoint()`：注册与 key 联动

真实定义：[kernel/tracepoint.c:377](../../kernel/tracepoint.c)、
[kernel/tracepoint.c:254](../../kernel/tracepoint.c)

```c
int tracepoint_probe_register(const char *name, void *probe, void *data)
{
	mutex_lock(&tracepoints_mutex);
	old = tracepoint_add_probe(name, probe, data);   /* 数组追加，见 kernel/tracepoint.c:112 */
	tracepoint_update_probes();   /* 遍历所有 tracepoint，同步 key 状态 */
	mutex_unlock(&tracepoints_mutex);
	release_probes(old);   /* RCU 延迟释放旧数组 */
	return 0;
}

static void set_tracepoint(struct tracepoint_entry **entry, struct tracepoint *elem, int active)
{
	rcu_assign_pointer(elem->funcs, (*entry)->funcs);
	if (active && !jump_label_enabled(&elem->key))
		jump_label_inc(&elem->key);        /* <-- 真正打开这个探测点的性能开关 */
	else if (!active && jump_label_enabled(&elem->key))
		jump_label_dec(&elem->key);
}
```
为什么是 DI：`jump_label_inc()`/`jump_label_dec()` 这一对调用，才是本篇
"性能开关"真正被打开/关闭的时刻——`rcu_assign_pointer(elem->funcs, ...)`
和 `jump_label_inc()` 的顺序很关键：**先把新的 probe 数组挂上去，再打开 key**，
确保 key 一旦被打开，任何并发读者看到的 `funcs` 一定是完整的新数组，不会出现
"key 已启用但 funcs 还是空指针"的中间态——这是并发场景下 DI 装配顺序的
一个真实的正确性约束案例。

### Example 6 — 真实调用点：`trace_sched_switch()` 在调度器里的位置

真实定义：[kernel/sched.c:3160](../../kernel/sched.c)、
[include/trace/events/sched.h:113](../../include/trace/events/sched.h)

```c
/* kernel/sched.c 的 context_switch() 附近 */
trace_sched_switch(prev, next);

/* include/trace/events/sched.h */
TRACE_EVENT(sched_switch,
	TP_PROTO(struct task_struct *prev, struct task_struct *next),
	TP_ARGS(prev, next),
	TP_STRUCT__entry(
		__array(char, prev_comm, TASK_COMM_LEN)
		__field(pid_t, prev_pid)
		__array(char, next_comm, TASK_COMM_LEN)
		__field(pid_t, next_pid)
	),
	TP_fast_assign(
		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
		__entry->prev_pid = prev->pid;
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->next_pid = next->pid;
	),
	TP_printk("prev_comm=%s prev_pid=%d ==> next_comm=%s next_pid=%d",
		  __entry->prev_comm, __entry->prev_pid, __entry->next_comm, __entry->next_pid)
);
```
为什么值得单列：这是内核调度器**每一次上下文切换**都会经过的一行代码——
一台繁忙的多核服务器每秒可能触发几十万次上下文切换，`trace_sched_switch()`
这一行必须在"没人订阅"时开销趋近零，这正是本篇 Why 小节开篇提出问题的
最真实、最高频的例证；本文档 `tracepoint_multi_probe.c` 的
`trace_sched_switch("idle", "bash")` 就是对这一行真实代码的直接建模。
（注意：本仓库版本的 `TRACE_EVENT` 宏在 `include/linux/tracepoint.h:366`
最终展开为 `DECLARE_TRACE(name, ...)`，`TP_STRUCT__entry`/`TP_fast_assign`/
`TP_printk` 这些字段在本仓库版本里被忽略（用于生成 `/sys/kernel/debug/tracing/
events/.../format` 的二进制记录格式，本仓库版本的宏定义为空），
这是本篇 Architecture 小节之外的另一处如实注明的版本差异。）

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/14-tracepoint/`](c/14-tracepoint/)

### 9.1 `static_tracepoint_sim.c` — 静态插入点 + probe 动态注册/注销

- 场景：`handle_page_fault()` 内部无条件调用 `trace_page_fault()`，全程从
  注册前到注册后到注销后，这一行代码一个字节都不变，验证"插入点固定、
  行为由运行期状态决定"这一核心承诺。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o static_tracepoint_sim static_tracepoint_sim.c`
- 实测输出：
  ```
  === static_tracepoint_sim demo ===

  -- 阶段1：没有任何 probe 挂载，触发 3 次 page fault --
    (预期：trace_page_fault() 内部的 if 判断为假，什么都不会打印)

  -- 阶段2：注册一个 probe，再触发 3 次 page fault --
    [tracepoint_probe_register] 'page_fault' now has 1 probe(s) attached
      [probe:ftrace] page_fault on cpu=0 addr=0x00004000
      [probe:ftrace] page_fault on cpu=0 addr=0x00005000
      [probe:ftrace] page_fault on cpu=0 addr=0x00006000

  -- 阶段3：注销 probe，再触发 2 次 page fault --
    [tracepoint_probe_unregister] probe detached from 'page_fault', 0 remaining
    (预期：注销之后又恢复到"静默"状态)

  === done ===
  总共触发 8 次 page fault，handle_page_fault() 里那一行 trace_page_fault(0, addr)
  从阶段1到阶段3从未被修改过一个字节——这就是 Tracepoint 作为 DI 手法的核心：
  静态插入点的源代码是固定不变的契约，探测行为完全由运行期挂载的 probe 决定。
  ```
  （阶段1和阶段3的"静默"是真实运行验证到的结果——`if (__tracepoint_page_fault.enabled)`
  在这两个阶段确实为假，没有任何打印，与预期完全一致。）
- 为什么是 DI：`handle_page_fault()`（Core）从未 `#include` 任何 probe 实现的
  头文件，`trace_page_fault()` 这个插入点函数在链接时就已经完全确定，
  运行期变化的只是 `__tracepoint_page_fault.enabled`/`.funcs` 两个数据字段。

### 9.2 `tracepoint_multi_probe.c` — 一个探测点挂载多个互不相识的 probe

- 场景：`ftrace` 风格、`perf` 风格、自定义模块风格三个 probe 同时挂载在
  `sched_switch` 风格的探测点上，各自维护独立的私有上下文（Context Injection），
  验证多消费者互不干扰、可以独立注销。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o tracepoint_multi_probe tracepoint_multi_probe.c`
- 实测输出：
  ```
  === tracepoint_multi_probe demo ===

  -- 触发 2 次 sched_switch，此刻没有任何 probe，Core 仍然无条件调用 trace_sched_switch() --
    (预期：静默，enabled=0)

  -- 依次挂载 3 个互不相识的 probe --
    [register] probe_ftrace attached, total probes = 1
    [register] probe_perf attached, total probes = 2
    [register] probe_custom_module attached, total probes = 3

  -- 触发 4 次 sched_switch，三个 probe 应该按注册顺序依次收到同一份事件 --
      [ftrace ] #1   sched_switch: 'idle' -> 'nginx'
      [perf   ] sample #1 written to ring buffer 'cpu0_ring' (prev='idle' next='nginx')
      [custom ] watched task 'nginx' involved in switch (idle -> nginx), hit #1
      [ftrace ] #2   sched_switch: 'nginx' -> 'sshd'
      [perf   ] sample #2 written to ring buffer 'cpu0_ring' (prev='nginx' next='sshd')
      [custom ] watched task 'nginx' involved in switch (nginx -> sshd), hit #2
      [ftrace ] #3   sched_switch: 'sshd' -> 'idle'
      [perf   ] sample #3 written to ring buffer 'cpu0_ring' (prev='sshd' next='idle')
      [ftrace ] #4   sched_switch: 'idle' -> 'nginx'
      [perf   ] sample #4 written to ring buffer 'cpu0_ring' (prev='idle' next='nginx')
      [custom ] watched task 'nginx' involved in switch (idle -> nginx), hit #3

  -- 注销中间的 perf probe，只留下 ftrace 与 custom_module --
      [ftrace ] #5   sched_switch: 'nginx' -> 'idle'
      [custom ] watched task 'nginx' involved in switch (nginx -> idle), hit #4

  === done ===
  统计：ftrace 记录了 5 个事件，custom_module 命中 nginx 相关切换 4 次，
        perf 在被注销前只记录了它存在期间发生的样本，注销之后的最后一次
        切换完全不会再进入 probe_perf()——三个 probe 全程互不知道对方存在，
        trace_sched_switch() 这个 Core 函数也完全不知道挂了几个、是谁。
  ```
  （`probe_custom_module` 只在 `nginx` 参与切换时打印，4 次触发里恰好命中了
  4 次——第 3 次切换 `sshd -> idle` 不涉及 `nginx`，`custom` 那一行确实没有
  打印，精确验证了这个 probe 内部自己的过滤逻辑与 tracepoint 分发机制本身
  是两层独立的判断。）
- 为什么是 DI：`__do_trace_sched_switch()`（Core）用一个 `while` 循环无差别
  遍历 `funcs` 数组逐一调用，完全不知道背后是 `ftrace`、`perf` 还是自定义
  模块——这是 05 篇 Notifier Chain"链式遍历、各自独立处理"思想在"探测点"
  场景下的直接复用，唯一的区别是这里的调用是同步阻塞的，且没有"短路终止"
  的语义（一个 probe 出问题不应该阻止后面的 probe 继续执行——本例简化实现
  没有做异常隔离，真实内核里探测点的 probe 一般被要求绝对不能崩溃或阻塞太久）。

### 9.3 `tracepoint_enable_disable_runtime.c` — 真实测量启用前后的性能差异

- 场景：1 亿次调用 `trace_hot_path()`，分别在"无 probe"、"挂载最小化 probe"、
  "挂载接近真实场景的 probe（字符串格式化）"三种状态下用 `clock_gettime()`
  测量真实耗时，验证"没人订阅时开销趋近零"以及"一旦订阅，probe 本身的实现
  质量才是决定性因素"这两个论点。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o tracepoint_enable_disable_runtime tracepoint_enable_disable_runtime.c`
- 实测输出：
  ```
  === tracepoint_enable_disable_runtime demo ===

  每组测试跑 100000000 次 trace_hot_path() 调用，测量 CLOCK_MONOTONIC 下的真实耗时。
  数字会随机器负载/CPU 型号波动，但相对大小关系（disabled < minimal probe
  < realistic probe）应该在任何机器上都成立。

  -- 场景1：没有任何 probe 挂载（enabled=0，只经过一次分支判断） --
    [disabled     ] 100000000 次调用，总耗时 188.052 ms，平均每次调用 1.881 ns

  -- 场景2：挂载 1 个最小化 probe（只做一次自增） --
    [minimal probe] 100000000 次调用，总耗时 187.275 ms，平均每次调用 1.873 ns

  -- 场景3：挂载 1 个更接近真实场景的 probe（格式化字符串） --
    [realistic probe] 100000000 次调用，总耗时 7549.800 ms，平均每次调用 75.498 ns

  === done ===
  汇总（真实测得，非估算）：
    disabled        :    1.881 ns/call
    minimal probe   :    1.873 ns/call  (1.0x disabled)
    realistic probe :   75.498 ns/call  (40.1x disabled)

  说明：g_sink=38250000000（防止循环被优化掉的哨兵变量，无实际意义）；
        probe_minimal 触发了 100000000 次，probe_realistic 触发了 100000000 次，
        两者都应该正好等于 ITERATIONS=100000000，证明每次挂载期间的调用
        全部被正确送达 probe，没有丢事件。真实 Kernel 用 asm-goto
        jump label 把 disabled 场景的开销进一步降到"零"（连这里
        测得的一次分支判断都不需要），本文件如实测量的是同一头文件里
        !HAVE_JUMP_LABEL fallback 路径的开销，而不是编造 asm-goto
        版本的具体数字。
  ```
  （这组真实数字讲了一个比"开销为零"更细致、更诚实的故事：`disabled`
  与 `minimal probe` 几乎完全相等（1.881ns vs 1.873ns，差异在测量噪声范围内），
  证明"是否有 probe"本身在这条 fallback 路径上几乎不影响耗时——真正的开销
  差异出现在 probe **内部做了什么**：`realistic probe` 因为多了一次
  `snprintf` 格式化，耗时暴涨到 40 倍。这个测量结果本身就是对"Tracepoint
  分发机制极其廉价，性能瓶颈永远在 probe 实现质量"这一设计哲学的直接证据，
  运行环境不同这个具体倍数会变化，但"disabled ≈ minimal ≪ realistic"这个
  相对关系在任何机器上重新跑都应该成立。）
- 为什么是 DI：这个示例证明了"机制的开销"（tracepoint 分发本身）与
  "策略的开销"（probe 内部具体做什么）是两个独立可测量的维度——这与
  12 篇 Policy Injection"机制固定、策略可换"的分离思想是一致的，只是本篇
  多了一个"必须能测量、必须能证明机制侧开销趋近于零"的额外工程要求。

一次性编译并运行全部三个示例：`cd c/14-tracepoint && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Tracepoint | Notifier Chain（05 篇） | Security Hooks / LSM（08 篇） | Linker Section Registration（13 篇） |
|---|---|---|---|---|
| 设计目标 | **诊断/调试可观测性**，绝大多数时间无人订阅 | **业务逻辑需要被通知**，订阅通常有明确业务意义 | **强制安全检查**，业务路径必经 | 静态候选集合的收集，与"何时/是否调用"完全解耦 |
| 未被订阅时的开销要求 | **必须趋近于零**（理想是零，本仓库 fallback 路径是一次分支判断） | 没有这个要求（链表为空时判断一次指针本身开销可接受，因为触发频率通常远低于调度切换级别） | 不适用，检查点总是"生效"，不存在"未订阅"状态 | 不适用，是编译期/链接期概念，不谈运行期开销 |
| 是否允许 probe/订阅者影响业务流程 | **不允许**，probe 崩溃或过慢被视为工程事故，不是业务逻辑的一部分 | **允许**，订阅者的处理结果/耗时是业务逻辑设计要考虑的一部分 | 允许且是核心机制（返回值直接决定业务放行与否） | 不适用 |
| 多消费者关系 | 多个 probe 完全独立、互不知道对方，同步顺序调用 | 多个订阅者独立处理，支持优先级排序 | 多个 LSM 链式叠加，一票否决 | 不适用（每个收集到的对象通常是独立规则，不是"消费者"概念） |
| 典型触发频率 | **极高**（调度切换、系统调用，每秒可达百万级） | 中低（设备热插拔、网络状态变化，通常每秒远少于千次） | 中（文件打开、进程信号，取决于工作负载） | 不适用（编译期/启动期） |
| 何时用 Tracepoint | 需要在极高频路径上插入一个"可能永远不会被使用"的观测点，且必须保证不使用时几乎零成本 | 需要在事件发生时让所有关心的一方都执行一段有业务意义的逻辑 | 需要在关键路径上做不可绕过的强制访问控制 | 需要静态收集一批候选对象，但何时/是否使用它们完全交给运行期条件决定 |
| 何时不要用 | 如果这个"通知"本身就是业务逻辑的一部分（不是纯诊断用途），应该用 Notifier Chain，因为 Tracepoint 的语义暗示"不订阅是完全合法的默认状态" | 如果触发频率极高（百万级/秒）且大多数时间没有订阅者，应该考虑 Tracepoint 的零开销设计，而不是让所有调用者都付出"检查链表是否为空"的代价 | — | — |

一句话总结本篇与 05 篇的关系：**Notifier Chain 回答的是"发生了一件业务上大家
都关心的事，如何通知所有关心的人"；Tracepoint 回答的是一个更苛刻的问题——
"如何在一条极高频路径上放一个诊断用的观测点，且保证这个观测点在 99% 的时间里
（没人订阅时）完全不影响性能"**。两者共享"一对多分发"的表层结构，但
Tracepoint 为了达成"零开销默认状态"这个额外目标，在 Notifier Chain 的基础上
叠加了 jump label 这一层专门的性能优化设施——这正是本系列教程反复强调的
"DI 手法的选择本质是设计目标驱动的权衡"的又一个具体例证：多花一层机制复杂度
（jump label），换来"默认状态零开销"这个在高频路径上至关重要的性质。

至此，第二批教程（12~14 号主题）全部完成。回顾一下这三篇之间的递进关系：
12 篇讲"同一时刻只有一个策略生效"（互斥选择），13 篇讲"链接器 section 自注册"
作为脱离 initcall 的通用元模式（收集手段与消费策略解耦），14 篇讲"一对多广播"
在"诊断可观测性、零开销默认状态"这一特殊约束下如何演化——三篇合起来，
把 06 篇 Initcall 和 08 篇 LSM 里"顺手带到但没有展开"的三个话题，用真实存在于
本仓库的代码路径补全成了同样深度的九段式内容。
