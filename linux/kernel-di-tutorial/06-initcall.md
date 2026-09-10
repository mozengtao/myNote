# 06. Initcall

> 前面几篇都在讲"注册接口 `register_xxx()` 被调用之后会发生什么"。
> 本篇回答一个更底层的问题：**这些注册调用本身，是谁负责在什么时候触发的？**

---

## 1. Why

Kernel 里到处都是 `xxx_init()` 函数：`mm_init()`、`net_dev_init()`、某个驱动的模块级 `my_driver_init()`。
这些函数必须在系统启动的**某个确定阶段**被调用一次，且：

1. **调用顺序有讲究**：内存管理必须先初始化好，才能轮到网络子系统；文件系统子系统要先于 rootfs 挂载。
2. **调用者（`init/main.c`）不应该知道每一个 `xxx_init()` 函数的存在**——否则每新增一个子系统/驱动，
   都要回去改 `init/main.c`，这是典型的"核心代码与具体实现耦合"。
3. **模块化构建（built-in vs 可加载模块）要用同一套源代码**：同一个 `my_driver_init()`，
   编译成内置时要在启动阶级某个阶段被自动调用；编译成 `.ko` 模块时要在 `insmod` 时被调用——
   两种场景必须用同一份 `module_init(fn)` 声明。

如果不用 Initcall 机制，唯一替代方案是在 `init/main.c` 里手写一份长长的调用列表：

```c
void start_kernel(void)
{
	mm_init();
	net_dev_init();
	my_driver_init();   /* 每加一个子系统/驱动就要回来加一行 */
	...
}
```

Initcall 的设计目标：

> **让每个子系统/驱动的初始化函数"自己声明自己属于哪个启动阶段"，
> 完全不需要把自己的名字写进任何中心化的列表里；
> Core（`do_initcalls()`）在启动时只管"按阶段遍历所有声明过的函数"，
   完全不知道、也不需要知道具体有哪些函数存在。**

这是"注册"这件事本身的 DI 化：**注册动作从"运行期显式调用 `register_xxx()`"，
下放给"编译期用特殊 linker section 打标记，链接器自动把它们收集到一起"**。

---

## 2. Architecture

```
   每个子系统/驱动的源文件
        |
        |  module_init(my_fn)  展开为：
        |  static initcall_t __initcall_my_fn __attribute__((section(".initcallN.init"))) = my_fn;
        v
   链接阶段：链接器把所有落在同一个 section 名字里的符号"排排坐"放在一起
        |
        v
   +------------------------------------------------+
   | .initcall1.init: [core_fn_A][core_fn_B]...       |  <-- core_initcall
   | .initcall4.init: [subsys_fn_A][subsys_fn_B]...   |  <-- subsys_initcall
   | .initcall7.init: [late_fn_A]...                  |  <-- late_initcall
   +------------------------------------------------+
        ^                        ^
        | __initcall_start       | __initcall_end
        | （链接脚本 vmlinux.lds.S 显式定义）
        v
   do_initcalls()（init/main.c）
        |
        | 按 section 顺序遍历，逐一 (*fn)()
        v
   进入具体子系统/驱动的初始化逻辑
```

真实定义：[include/linux/init.h:177](../../include/linux/init.h)

```c
#define __define_initcall(level,fn,id) \
	static initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" level ".init"))) = fn

#define core_initcall(fn)		__define_initcall("1",fn,1)
#define subsys_initcall(fn)		__define_initcall("4",fn,4)
#define late_initcall(fn)		__define_initcall("7",fn,7)
```

（本仓库里 `module_init(x)` 实际展开为 `__initcall(x)` 即 `device_initcall(x)`，见
[include/linux/init.h:266](../../include/linux/init.h) 和 212 行附近；当代码作为可加载模块编译时，
`module_init` 会被重定义为直接生成 `init_module()` 符号，由 `insmod` 调用——**同一个宏，
两种展开方式，取决于编译时是 built-in 还是 module**，这正是"零成本抽象 + 编译期决定形状"的又一例证）。

---

## 3. Lifecycle

```
   allocate
   (写一个 static int my_fn(void) { ...; return 0; })
        |
        v
   initialize / inject（这一步与前面几篇完全不同！）
   (module_init(my_fn) —— 不是"调用注册函数"，而是"声明性地打一个 section 标记"，
    这一步发生在编译期，不是运行期！)
        |
        v
   framework owns dispatch（链接期）
   (链接器把所有相同 section 的符号收集到连续的内存区域，
    这一步发生在链接期，仍然不是运行期！)
        |
        v
   callback（内核启动的某一时刻）
   (do_initcalls() 遍历 __initcall_start 到 __initcall_end，逐一调用；
    这才是运行期真正发生调用的唯一时刻)
        |
        v
   destroy
   (initcall 函数指针所在的内存区域（.init.text/.init.data）在启动完成后
    整块被内核释放/标记为可回收 —— free_initmem()，因为它们保证只会被调用一次)
```

**这是本篇最反直觉、也最值得记住的一点**：01-05 篇的"注入"都发生在**运行期**（调用一次
`register_xxx()` 函数）；Initcall 的"注入"发生在**编译期/链接期**（一个特殊的变量声明），
运行期只有"遍历 + 调用"这一步，且保证每个 initcall **只执行一次**。

---

## 4. Relationship

```
   Core（init/main.c do_initcalls()）
        |
        | 只认识
        v
   __initcall_start ... __initcall_end 之间的一段内存
        |
        | 里面密集排列着
        v
   一个个 initcall_t（函数指针）
        |
        | 每一个都
        v
   对应到某个子系统/驱动源文件里的一个 static 函数
```

关键点：Core 和具体实现之间，**没有任何一个运行期数据结构（没有链表、没有数组变量）**——
两者的"契约"完全靠"大家都同意把自己的函数指针放进特定命名的 section"这个编译期/链接期约定来维系。

---

## 5. Data Flow

```
编译阶段：
  my_driver.c:  module_init(my_driver_init)
       |
       v （宏展开）
  static initcall_t __initcall_my_driver_init
      __attribute__((section(".initcall6.init"))) = my_driver_init;

链接阶段：
  ld 把所有 .initcallN.init section 按名字排序、拼接，
  vmlinux.lds.S 在其首尾放置 __initcall_start / __initcall_end 符号

运行阶段（内核启动）：
  start_kernel()
      |
      v
  do_basic_setup() -> do_initcalls()
      |
      v
  for (fn = __initcall_start; fn < __initcall_end; fn++)
        do_one_initcall(*fn)      <-- 盲调，Core 不知道这是哪个子系统
      |
      v
  my_driver_init() 被执行，完成该驱动的全局初始化
```

对照本文档 `linker_section_ctor_sim.c` 示例的真实运行轨迹：

```
main() 之前：
  __attribute__((constructor)) 系列函数自动运行（另一种"链接器自动调用"机制，见 7.）

main() 内部：
  do_initcalls()
      |
      v
  do_one_level("core", ...) -> 遍历 .initcall1 section
      |
      v
  do_one_level("subsys", ...) -> 遍历 .initcall4 section
      |
      v
  do_one_level("late", ...) -> 遍历 .initcall7 section
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| initcall 函数指针本身 | **编译期确定，运行期只读** | 与前几篇"运行期动态注册/注销"完全不同 |
| section 里"有哪些函数" | **链接期确定，运行期不可变** | 一旦链接完成，`__initcall_start`~`__initcall_end` 之间的内容就固定了 |
| 执行状态（是否已经跑过） | **运行期变化一次** | 每个 initcall 保证只被调用一次，`do_initcalls()` 完成后这段代码/数据可能被释放 |
| 返回值处理策略 | Kernel 约定"容忍 `-ENODEV`" | 见 `module_registry_levels.c` 的 `do_one_initcall()` 复刻 |

---

## 7. Failure

1. **同一 level 内部的顺序不保证**：本文档 `linker_section_ctor_sim.c` 实测中，`core_subsystem_init`
   和 `mm_subsystem_init` 同属 level 1，但**实际执行顺序是先 `mm_subsystem_init` 后 `core_subsystem_init`**——
   与源码里声明顺序（`core_subsystem_init` 先声明）恰好相反！这是因为链接器按"目标文件/符号处理顺序"
   排列同 section 内容，**不保证与源码文本顺序一致**。真实 Kernel 里如果两个 `core_initcall` 之间有隐式的
   顺序依赖，是一个真实存在过的 bug 类型（历史上有通过 `initcall_sync` 变体、显式依赖声明来解决的案例）。
2. **忘记选择正确的 level**：如果一个驱动的初始化依赖某个子系统，但把自己声明成比该子系统更早的 level
   （比如某驱动依赖网络栈却声明成 `core_initcall`），会在早于依赖就绪的阶段被调用，访问未初始化的资源。
3. **initcall 函数在错误的上下文假设下运行**：initcall 阶段通常是单线程、抢占关闭或部分关闭的早期启动环境，
   如果函数里调用了假设"已经有完整调度器/完整内存分配子系统"的接口，可能直接死锁或崩溃。
4. **built-in vs module 的行为差异被忽略**：同一份用 `module_init()` 声明的代码，编译成模块后是在
   `insmod` 时执行（可能在系统运行很久之后），编译成内置时是在启动早期执行——如果代码里有"假设自己
   一定在启动早期运行"的逻辑（比如访问只在早期可用的 bootmem allocator），模块化编译后会出问题。
5. **`.init.text`/`.init.data` 释放后仍被引用**：initcall 函数体本身在执行完 `do_initcalls()` 后可能被
   释放（`free_initmem()`），如果某处还保留着指向这段代码的函数指针并试图在之后调用，就是经典的
   "调用已释放代码段"崩溃。这要求 `__init` 标记的函数**绝对不能**被非 `__init` 代码保存指针并延迟调用。
6. **调试方法**：
   - 用户态：本文档三个示例分别打印了"登记表内容"（`module_registry_levels.c`）和实际执行序列，
     可以直接对比"声明顺序"与"实际执行顺序"，具象化"顺序不保证"这一事实。
   - Kernel：启动时加 `initcall_debug` 内核参数，会为每个 initcall 打印耗时和返回值
     （对应本仓库 [init/main.c:662](../../init/main.c) 的 `do_one_initcall_debug()`）；
     `initcall_blacklist=` 参数可以在调试时跳过某个 initcall；`System.map`/`/proc/kallsyms`
     配合 `initcall_debug` 输出的函数名可以定位具体是哪个子系统的初始化出了问题。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `__define_initcall` 宏展开

真实定义：[include/linux/init.h:177](../../include/linux/init.h)

```c
#define __define_initcall(level,fn,id) \
	static initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" level ".init"))) = fn

#define core_initcall(fn)   __define_initcall("1", fn, 1)
```
为什么是 DI 的编译期版本：这一行代码把"某个函数应该在哪个阶段被调用"这个信息，
编码进了"这个函数指针变量放在哪个 section"这一事实里，Core 不需要任何运行期登记动作就能"发现"它。

### Example 2 — `do_initcalls()`：按 section 顺序遍历

真实定义：[init/main.c:713](../../init/main.c)

```c
static void __init do_initcalls(void)
{
	initcall_t *fn;
	for (fn = __early_initcall_end; fn < __initcall_end; fn++)
		do_one_initcall(*fn);   /* <-- 盲调 */
}
```

### Example 3 — `do_one_initcall()`：错误码的容忍策略

真实定义：[init/main.c:680](../../init/main.c)

```c
int __init_or_module do_one_initcall(initcall_t fn)
{
	int count = preempt_count();
	int ret = fn();

	if (ret && ret != -ENODEV && initcall_debug)
		sprintf(msgbuf, "error code %d ", ret);   /* -ENODEV 被认为是正常情况 */
	if (preempt_count() != count) {
		/* 检测 initcall 是否忘记恢复 preempt count —— 一种典型的"契约违反"检测 */
		preempt_count() = count;
	}
	return ret;
}
```
为什么这里值得单独列一条：`do_one_initcall()` 不仅"盲调"，还**验证被调用方是否遵守了契约**
（不能留下不平衡的 preempt count、不能留下未恢复的中断状态）——这是 DI 场景下 Core 侧
"契约防御性检查"的典范。

### Example 4 — `module_init()` 在"内置"与"可加载模块"两种场景下的不同展开

真实定义：[include/linux/init.h:266,295](../../include/linux/init.h)

```c
/* 场景 A：built-in（编译进 vmlinux） */
#define module_init(x)	__initcall(x);   /* 展开成 device_initcall，进 .initcall6.init section */

/* 场景 B：可加载模块（编译成 .ko），__init.h 在模块编译单元里重新定义 */
#define module_init(initfn)					\
	static inline initcall_t __inittest(void)		\
	{ return initfn; }					\
	int init_module(void) __attribute__((alias(#initfn)));
/* init_module() 是 insmod 时用户态 modutils/kmod 通过 sys_init_module 系统调用查找并调用的固定符号名 */
```
为什么是 DI：**同一份驱动源代码，同一行 `module_init(my_probe)`**，在两种构建模式下被完全不同的
机制"发现并调用"——built-in 靠链接器 section，module 靠固定符号名 + 系统调用——但对驱动作者而言，
写法完全一样，这是"注入机制对使用者透明"的极致体现。

### Example 5 — `console_initcall`/`early_param`：比 `core_initcall` 更早的特殊 level

场景：某些子系统（如串口控制台）必须在**几乎所有**其它 initcall 之前就能工作，否则连
`printk` 都无法输出，无法调试后续的启动过程。

```c
#define early_initcall(fn)		__define_initcall("early", fn, early)

/* drivers/tty/serial 风格 */
static int __init early_serial_setup(void)
{
	/* 在其它所有 core/subsys/late initcall 之前完成，保证尽早可用的控制台输出 */
	return 0;
}
early_initcall(early_serial_setup);
```
为什么是 DI：即便是"谁最优先运行"这种极端需求，Kernel 依然用同一套"section 分组"机制解决，
而不是给"特别特殊的初始化"开一个特例通道——这正是"统一机制覆盖所有场景"的设计哲学。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/06-initcall/`](c/06-initcall/)

### 9.1 `linker_section_ctor_sim.c` — 用 linker section 模拟 initcall + 对比 constructor

- 场景：真实复刻 Kernel `__define_initcall` 的技巧——自定义 section + GNU ld 自动生成的
  `__start_<section>`/`__stop_<section>` 边界符号；同时实现 `__attribute__((constructor))`
  作对比（另一种"链接器/运行时自动触发"机制，但语义不同：无需显式调用 do_initcalls）。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o linker_section_ctor_sim linker_section_ctor_sim.c`
- 实测输出：
  ```
  [ctor priority=101] runs first among prioritized constructors
  [ctor priority=200] runs after priority=101
  [ctor default priority] early constructor ran (before main)
  === linker_section_ctor_sim demo ===
  (constructor 输出应该已经出现在上面，因为它们在 main() 之前运行)

  -- 显式调用 do_initcalls()，模拟内核启动时 init/main.c 的行为 --
  [do_initcalls] running level 'core (level 1)' (2 entries)
    [core_initcall] mm-like subsystem initialized
    [core_initcall] core subsystem initialized
  [do_initcalls] running level 'subsys (level 4)' (1 entries)
    [subsys_initcall] net-like subsystem initialized
  [do_initcalls] running level 'late (level 7)' (1 entries)
    [late_initcall] a late-registered driver probes hardware

  === done ===
  说明：constructor 是"运行时机确定但顺序需要显式指定优先级"；
        initcall 是"运行时机由 Core 显式决定 (do_initcalls())，
        顺序由 section 分组（level）决定，同一 level 内顺序不保证"。
  ```
  **关键观察**：`core_subsystem_init` 在源码里声明在 `mm_subsystem_init` **之前**，
  但实际输出顺序是 `mm_subsystem_init` 先执行——这正是 Failure 小节第 1 条讲的"同一 level
  内顺序不保证"的直接实证，不是编造的，是这次真实编译运行观察到的结果。
- 为什么是 DI：`do_initcalls()`（Core）从未 `#include` 任何具体子系统的头文件，
  只遍历 `__start_initcallN`~`__stop_initcallN` 之间的内存区域盲调；这批函数指针是**链接器**
  而非任何运行期代码"收集"到一起的。

### 9.2 `module_registry_levels.c` — 分级初始化（可移植版）

- 场景：不依赖编译器 section 技巧，用 `__attribute__((constructor))` + 显式登记表实现同样的
  "分级初始化"效果，并复刻 `do_one_initcall()` 对 `-ENODEV` 的特殊容忍策略。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o module_registry_levels module_registry_levels.c`
- 实测输出：
  ```
    [WARN] initcall 'broken_subsystem_init' returned error code -22
  === module_registry_levels demo ===

  -- 登记表内容（构造函数已在 main() 之前把 7 个 initcall 登记进来）--
    entry[0]: name='late_driver_probe       ' level=late
    entry[1]: name='mm_init                 ' level=core
    entry[2]: name='net_stack_init          ' level=subsys
    entry[3]: name='rootfs_mount_init       ' level=fs
    entry[4]: name='optional_device_init    ' level=device
    entry[5]: name='broken_subsystem_init   ' level=subsys
    entry[6]: name='early_percpu_init       ' level=early

  -- 执行 do_initcalls()，按 level 顺序执行，与声明顺序无关 --
  [do_initcalls] entering level 'early'
    [early   ] early_percpu_init: setting up per-CPU areas
  [do_initcalls] entering level 'core'
    [core    ] mm_init: setting up memory zones
  [do_initcalls] entering level 'subsys'
    [subsys  ] net_stack_init: registering protocol families
    [subsys  ] broken_subsystem_init: something really went wrong
  [do_initcalls] entering level 'fs'
    [fs      ] rootfs_mount_init: mounting rootfs
  [do_initcalls] entering level 'device'
    [device  ] optional_device_init: device not present
  [do_initcalls] entering level 'late'
    [late    ] late_driver_probe: probing optional hardware

  === done, executed 7 initcalls across 8 levels ===
  ```
  （`optional_device_init` 返回 `-ENODEV`，Core **没有**打印 WARN；`broken_subsystem_init`
  返回 `-EINVAL`，Core **打印了** WARN 但继续执行后续 initcall——精确复刻了真实 Kernel
  "容忍 -ENODEV，但对其它错误码发出警告且不中止启动"的策略。登记表顺序也验证了：
  尽管源码声明顺序是 late->core->subsys->fs->device->subsys->early，`do_initcalls()`
  仍严格按 level 数值顺序执行。）
- 为什么是 DI：`register_initcall()`/`do_initcalls()`（Core）不知道 `mm_init`/`net_stack_init`
  等函数的存在，`DECLARE_INITCALL` 宏让每个"模块"在 `main()` 之前自己把自己塞进登记表——
  这是"用可移植的运行期机制（constructor）模拟编译期机制（linker section）"的典型手法。

### 9.3 `self_registering_tests.c` — 自注册测试框架

- 场景：`TEST_CASE(name) { ... }` 定义的测试用例通过 linker section 自动被框架发现，
  运行后输出 TAP（Test Anything Protocol）风格结果，故意包含一个失败用例验证报告路径正确。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o self_registering_tests self_registering_tests.c`
- 实测输出：
  ```
  === self_registering_tests demo ===

  TAP version 13
  1..5
  ok 1 - stack_overflow_guard (1 assertion(s))
  not ok 2 - stack_intentional_failure_demo (1/1 assertion(s) failed)
    # self_registering_tests.c:145: expected 999 got 100
  ok 3 - stack_empty_after_init (1 assertion(s))
  ok 4 - stack_lifo_order (3 assertion(s))
  ok 5 - stack_push_pop_basic (2 assertion(s))

  === summary: 5 total, 4 passed, 1 failed ===
  ```
  进程退出码为 `1`（失败用例数 > 0），符合 CI 场景下"有失败测试则整体失败"的约定。
  开发过程中这里还真实踩中并修复了一个经典陷阱：`EXPECT_EQ` 宏最初直接在宏体里重复展开
  `actual` 表达式，导致带副作用的 `stack_pop(&s)` 被求值了两次（一次用于比较，一次用于格式化
  错误信息），第二次求值时栈已经空了，报出了错误的"got -1"。修复方式是让宏先把 `actual`/
  `expected` 存入局部变量各求值一次——这本身就是 Context Injection（03 篇）里提到的"宏参数
  多次求值"陷阱在测试框架场景下的真实重现，保留在这里作为活教材。
- 为什么是 DI：测试运行器（`main()`）在编译期完全不知道有 `stack_push_pop_basic` 等 5 个
  测试用例存在，全靠 `TEST_CASE` 宏把每个测试函数指针放进 `kdi_tests` section，链接器负责
  "发现"它们；新增一个测试用例只需要写一个新的 `TEST_CASE(...)` 块，不需要触碰任何"测试列表"。

一次性编译并运行全部三个示例：`cd c/06-initcall && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Initcall | Function Pointer Callback | Driver Registration | Notifier Chain |
|---|---|---|---|---|
| "注册"发生的阶段 | **编译期/链接期**（section 标记） | 运行期（赋值给字段） | 运行期（调用 `register_xxx()`） | 运行期（调用 `register_xxx()`） |
| Core 侧持有什么 | 一段内存区域的起止地址 | 一个函数指针字段 | 一份动态表/链表 | 一份动态链表 |
| 是否支持运行期动态增减 | **否**，一旦链接完成就固定 | 是 | 是 | 是 |
| 执行次数保证 | **恰好一次**（且执行完可能释放代码段） | 任意次数 | probe 一次，remove 一次 | 任意次数（每次事件触发一次） |
| 何时用 Initcall | "系统/模块启动时必须执行且只执行一次的初始化逻辑"，且不需要运行期再变化 | 需要在对象生命周期内被反复调用 | 需要处理运行期硬件热插拔 | 需要响应运行期反复发生的事件 |
| 何时不要用 | 如果需要支持"运行期动态卸载再重新初始化"，Initcall 机制完全不适合（它没有"注销"概念） | — | — | — |

一句话：**Initcall 是"把注册动作本身也做成零运行时开销"的极致**——前面几篇的"注入"都要付出
"运行期调用一次注册函数"的代价（尽管很小），Initcall 把这个代价也优化掉了，用编译期/链接期
的元数据取代运行期调用，代价是"失去了运行期动态性"（不能注销、不能重复执行）。这解释了
为什么 Initcall 只适合"启动阶段一次性初始化"，而不适合本系列其它场景。
下一篇 [07-workqueue.md](07-workqueue.md) 回到运行期，看"任务级别的注入"如何与"异步执行"结合。
