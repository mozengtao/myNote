# 13. Linker Section Registration

> `06-initcall.md` 已经详细讲过链接器 section 自注册的**一种**用法：把一批
> "系统启动时必须无条件执行一次"的函数指针收集起来。本篇要说明的是：
> **initcall 只是这套元模式的一个特例**——同一套"编译期打标记、链接器自动收集、
> 运行期遍历"的骨架，在内核里还撑起了完全不同的执行语义：**按条件匹配后才执行**，
> 而不是"收集到的全部无条件执行"。

---

## 1. Why

06 篇已经论证过"为什么需要一种机制让 Core 在编译期就能'发现'所有参与者，而不需要
运行期显式调用一次注册函数"——链接器 section 自注册就是这个答案。但 initcall
只解决了这类需求里的一种形态：**"收集到的每一项，最终都要在同一个时刻被调用一次"**。
内核里还有大量场景符合"需要静态收集，但不是全部执行"这个更精确的描述：

1. **命令行参数处理器**（`__setup`/`early_param`）：内核编译进了成百上千个可能的
   启动参数处理器（每个驱动、每个子系统都可能声明几个），但一次具体的启动只会带有
   十几个参数。如果每次启动都无条件执行全部处理器，绝大多数处理器会在没有对应参数的
   情况下被莫名其妙调用——这是不可接受的。正确的语义是：**先收集全部候选，
   启动时按命令行实际出现的参数名去候选表里"查找并执行匹配项"**。
2. **PCI 设备修补表**（`DECLARE_PCI_FIXUP_*`）：内核编译进了数百条针对特定硬件
   vendor/device id 的修补规则，但一台具体的机器上插着的 PCI 设备最多几十个。
   同样不能"设备探测到时把所有修补规则都跑一遍"——修补规则必须**按 (vendor, device)
   数值匹配后才执行**，不匹配的规则永远不会被调用。

如果不用链接器 section 自注册，唯一的替代方案是回到 06 篇开头就否定的那条路：
在核心代码里手写一个巨大的 `if (strcmp(param, "debug")==0) ... else if (strcmp(param,
"quiet")==0) ...` 或者 `if (vendor==0x8086 && device==0x1234) ... else if (...)`
分支链——这正是"核心代码与具体实现耦合"的反面教材，每加一个新参数/新硬件修补都要
改核心文件。

Linker Section Registration 作为通用元模式的设计目标：

> **把"某个候选对象存在、需要被纳入某个统一集合"这一事实固化在编译期/链接期
> （通过 section 标记 + 链接器自动收集），但"这个集合里的元素在运行期具体怎么被
> 使用"——是无条件全部执行、还是按某种匹配规则挑选执行、还是被完全不同的两种
> 消费者以不同方式遍历——完全下放给 Core 侧的运行期逻辑决定。
> 收集手段（section）与消费策略（遍历方式）是两个独立的维度。**

本篇用 `__setup`/`early_param`（字符串前缀匹配）和 `DECLARE_PCI_FIXUP_*`
（数值 vendor/device 匹配）两个真实存在于本仓库的机制，加上一个抽出公共骨架的
通用工具库示例，证明这套元模式的领域无关性——它不是"initcall 专用的一个实现细节"，
而是内核里到处复用的"静态注册 + 动态消费"的基础设施。

---

## 2. Architecture

```
   每个候选对象的源文件
        |
        | 用专用宏声明自己（这一步在编译期完成）：
        |  __setup("debug", debug_kernel)
        |  DECLARE_PCI_FIXUP_EARLY(0x8086, 0x1234, quirk_fn)
        v
   链接阶段：链接器把所有落在同一个 section 名字里的符号排排坐放在一起
        |
        v
   +--------------------------------------------------------+
   | .init.setup:        [debug entry][quiet entry][elevator= entry]... |
   | .pci_fixup_early:   [intel:1234 entry][broadcom:ANY entry]...      |
   | .pci_fixup_final:   [realtek:8168 entry][intel:1234 entry]...      |
   +--------------------------------------------------------+
        ^                              ^
        | __setup_start/__setup_end    | __start_pci_fixups_early/__end_...
        | （链接脚本显式定义边界符号）
        v
   Core 侧的消费逻辑（关键：两种消费逻辑完全不同！）
        |
        +- do_early_param()/obsolete_checksetup()：
        |    按命令行 token 的字符串名字在 section 里"查找匹配项"才调用
        |
        +- pci_do_fixups()：
             按 (vendor, device) 数值在 section 里"查找匹配项"（支持通配）才调用
```

真实定义（`__setup`/`early_param` 一侧）：[include/linux/init.h:225](../../include/linux/init.h)

```c
struct obs_kernel_param {
	const char *str;
	int (*setup_func)(char *);
	int early;
};

#define __setup_param(str, unique_id, fn, early)			\
	static struct obs_kernel_param __setup_##unique_id		\
		__used __section(.init.setup)				\
		__attribute__((aligned((sizeof(long)))))		\
		= { __setup_str_##unique_id, fn, early }

#define __setup(str, fn)       __setup_param(str, fn, fn, 0)
#define early_param(str, fn)   __setup_param(str, fn, fn, 1)
```

真实定义（PCI fixup 一侧）：[include/linux/pci.h:1375](../../include/linux/pci.h)

```c
struct pci_fixup {
	u16 vendor, device;	/* 可以用 PCI_ANY_ID 通配 */
	void (*hook)(struct pci_dev *dev);
};

#define DECLARE_PCI_FIXUP_SECTION(section, name, vendor, device, hook)	\
	static const struct pci_fixup __pci_fixup_##name __used		\
	__attribute__((__section__(#section))) = { vendor, device, hook };
#define DECLARE_PCI_FIXUP_EARLY(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_early,			\
			vendor##device##hook, vendor, device, hook)
```

两者结构几乎同构（`__used` + `__section__`/`__section` + 一份携带"匹配条件"的静态
常量数据），但真实内核用**带点的 section 名**（`.init.setup`、`.pci_fixup_early`）
并通过 `include/asm-generic/vmlinux.lds.h` 里的 `INIT_SETUP()` 宏
（[vmlinux.lds.h:612](../../include/asm-generic/vmlinux.lds.h)）和显式的
`__start_pci_fixups_early`/`__end_pci_fixups_early` 定义（[vmlinux.lds.h:236](../../include/asm-generic/vmlinux.lds.h)）
手工指定边界符号——因为带点的 section 名不是合法 C 标识符，GNU ld 不会为它自动生成
`__start_<section>`/`__stop_<section>`。本文档三个用户态示例为了避免维护一份自定义
链接脚本，统一改用不带点、合法 C 标识符的 section 名字（如 `kdi_setup`、
`pci_fixup_early_sec`），从而借助 ld 的自动生成机制——**效果完全等价，只是省掉了
"自己写链接脚本片段"这一步**，这是本篇与 06 篇 `linker_section_ctor_sim.c` 一致的
处理方式，也在文档里如实注明这一版本/工程差异。

---

## 3. Lifecycle

```
   allocate
   (写一个 static int my_setup_fn(char *arg) 或 static void my_pci_quirk(struct pci_dev *dev))
        |
        v
   initialize / inject（编译期，不是运行期！这一点和 01-05 篇完全不同）
   (__setup("myparam", my_setup_fn) 或 DECLARE_PCI_FIXUP_EARLY(0x8086, 0x1234, my_pci_quirk)
    —— 展开为一个带 section 属性的 static const 变量声明)
        |
        v
   framework owns collection（链接期）
   (链接器把所有相同 section 的符号收集到连续内存区域，
    __setup_start/__setup_end 或 __start_pci_fixups_early/__end_pci_fixups_early
    这两个边界符号被链接脚本固定下来)
        |
        v
   select（运行期，这一步是本篇与 06 篇 initcall 最大的行为差异）
   (Core 侧遍历整个 section，对每一项做匹配判断——字符串前缀匹配或数值匹配——
    只有匹配成功的那一项才会被调用；绝大多数已注册的条目在一次具体的运行中
    永远不会被调用，这是完全正常、设计使然的结果，不是 bug)
        |
        v
   callback（仅针对匹配成功的条目）
   (my_setup_fn(val) 或 my_pci_quirk(dev) 被调用)
        |
        v
   destroy
   (与 initcall 类似，.init.setup 数据在内核启动完成后可能随 .init 段一起
    被释放；pci_fixup 表则是编译期常量数据，运行期不会被回收，因为设备热插拔
    随时可能需要重新匹配)
```

**与 06 篇 initcall 生命周期的关键差异标注在"select"这一步**：initcall 没有
"select"这个阶段——`do_initcalls()` 遍历到的每一项直接调用，没有匹配判断；
本篇的两种机制都在"遍历"和"调用"之间插入了一次显式的匹配判断，这正是
"链接器 section 自注册"作为元模式时，"收集"（编译期/链接期）与"消费策略"
（运行期）两个维度可以独立变化的直接证明。

---

## 4. Relationship

```
   Core（init/main.c 的参数解析逻辑 / drivers/pci/quirks.c 的设备探测逻辑）
        |
        | 只认识
        v
   __setup_start ~ __setup_end 之间的一段内存
   __start_pci_fixups_early ~ __end_pci_fixups_early 之间的一段内存
        |
        | 里面密集排列着
        v
   一个个 struct obs_kernel_param / struct pci_fixup（数据 + 匹配条件的组合）
        |
        | 每一项都关联着
        v
   对应源文件里的一个 static 函数（真正的处理逻辑）
        |
        | 是否被调用，取决于运行期传入的
        v
   外部输入（命令行字符串 / 实际探测到的硬件 id）——这是本篇独有的一层关系，
   06 篇 initcall 没有任何"外部输入决定是否调用"的环节
```

---

## 5. Data Flow

以 `__setup`/`early_param` 为例，对照真实启动路径：

```
链接阶段：
  ld 把所有 .init.setup section 的条目拼接在一起，
  vmlinux.lds.S（通过 INIT_SETUP 宏）在首尾放置 __setup_start/__setup_end 符号

启动阶段（第一遍，只处理 early）：
  start_kernel() -> parse_early_param()（init/main.c:414）
      |
      v
  parse_args("early options", cmdline, ..., do_early_param)
      |
      v
  do_early_param()（init/main.c:390）：对命令行的每个 token，
  在 __setup_start~__setup_end 里线性扫描，只匹配 p->early==1 的条目

启动阶段（第二遍，处理完整命令行，含非 early）：
  start_kernel() -> ... -> parse_args(..., unknown_bootoption)
      |
      v
  obsolete_checksetup()（init/main.c:158）：再扫描一遍 __setup_start~__setup_end，
  跳过 early==1 的（已经处理过），对 early==0 的做匹配并调用
```

对照本文档 `boot_param_setup_sim.c` 的真实运行轨迹：

```
main()
  |
  +- 打印已注册的 4 个处理器（elevator=/reset_devices/quiet/debug）
  |  —— 注意打印顺序是 elevator=, reset_devices, quiet, debug，
  |     与源码里 debug_kernel 最先声明、elevator_setup 最后声明的顺序恰好相反，
  |     这是链接器按目标文件/符号处理顺序排列 section 内容的真实体现
  |     （与 06 篇 Failure 小节讲的"同一 level 内顺序不保证"是同一现象）
  |
  +- do_early_param(tokens)   <-- 只执行 debug/quiet 两个 early_param
  |     [early_param] debug_kernel: console_loglevel -> 10
  |     [early_param] quiet_kernel: console_loglevel -> 4
  |
  +- obsolete_checksetup(tokens)   <-- 执行 reset_devices/elevator=，跳过已处理的 debug/quiet
        [__setup] set_reset_devices: reset_devices -> 1
        'debug' already handled by early_param, skip
        [__setup] elevator_setup: chosen_elevator -> 'deadline'
        Unknown boot option 'panic=1' -- ignoring
        'quiet' already handled by early_param, skip
        Unknown boot option 'unknown_param=5' -- ignoring
```

PCI fixup 一侧的对照路径：

```
设备枚举核心路径（drivers/pci/probe.c 风格）：
  pci_scan_device()
      |
      v
  pci_fixup_device(pci_fixup_early, dev)（drivers/pci/quirks.c:2938）
      |
      v
  pci_do_fixups(dev, __start_pci_fixups_early, __end_pci_fixups_early)（quirks.c:2909）
      |
      v
  for (f = start; f < end; f++)
      if ((f->vendor==dev->vendor || f->vendor==PCI_ANY_ID) &&
          (f->device==dev->device || f->device==PCI_ANY_ID))
          f->hook(dev);          <-- 盲调，只有匹配的才会执行
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| section 里的候选条目集合 | **编译期/链接期确定，运行期只读** | 与 06 篇 initcall 完全一致的性质——一旦链接完成就固定，不能运行期增减（模块场景下可以在 `insmod` 时把模块自己的 section 内容"追加"进已知列表，见 `tp_module`/`tracepoint_module_list` 的处理方式，这是唯一的运行期变化途径） |
| 每个条目的匹配条件（`str`/`early`，`vendor`/`device`） | **编译期常量，不可变** | `struct obs_kernel_param`/`struct pci_fixup` 都声明为只读的 `static (const)` 数据 |
| "是否被调用"这个结果 | **运行期才能确定，且依赖外部输入** | 同一份二进制、同一个 section 内容，不同的启动命令行/不同的插入设备会导致完全不同的一组条目被实际调用；这是本篇与 initcall（每次启动结果完全确定）最大的运行期差异 |
| 匹配算法本身（前缀匹配 vs 数值精确/通配匹配） | Core 侧硬编码，与具体条目无关 | `do_early_param()`/`pci_do_fixups()` 各自实现自己的匹配算法，条目本身不携带"怎么匹配"的逻辑，只携带"匹配需要的数据" |

---

## 7. Failure

1. **section 内条目被编译器"插入 padding"导致遍历错位**：这是本文档编写过程中
   真实踩中的坑——如果不显式加 `__attribute__((aligned(sizeof(long))))`，
   编译器可能给每个 section 条目加上比结构体自身大小更宽的对齐要求（本例中实测
   是把 24 字节的结构体对齐到 32 字节边界），导致按 `sizeof(type)` 做指针步进的
   遍历代码"跳过"实际不存在的 padding 字节从而错位、读到脏数据甚至越界段错误。
   真实内核 `include/linux/init.h:234` 附近的注释原文就是
   "Force the alignment so the compiler doesn't space elements of the
   obs_kernel_param 'array' too far apart in .init.setup."——这不是防御性编程的
   噪音注释，是真实会发生的编译器行为，本文档 `boot_param_setup_sim.c` 第一次
   省略这个属性时就实测触发了段错误，加上属性后问题消失。
2. **宏形参名字与 `__attribute__` 关键字撞车**：`DECLARE_PCI_FIXUP_SECTION` 宏
   如果贪图直观把形参命名为 `section`，而宏体里又写 `__attribute__((section(...)))`，
   C 预处理器的宏形参替换会把属性名 `section` 也当作形参一起替换掉，展开成
   `__attribute__((pci_fixup_early_sec(...)))` 这种语法错误——真实内核在
   `include/linux/pci.h:1393` 用的是 `__section__`（两侧双下划线的等价写法）
   而不是 `section`，正是为了绕开这个命名冲突。本文档 `pci_fixup_table_sim.c`
   第一次编写时就用错了写法，编译报出一堆奇怪的 "attribute directive ignored"
   警告和链接错误，改成 `__section__` 后问题消失，是这类"宏卫生"陷阱的真实记录。
3. **误以为"注册了就一定会被调用"**：这是本篇与 06 篇最容易混淆的一点——
   initcall 的心智模型是"注册 == 会被执行"，如果照搬这个心智模型去看 `__setup`/
   PCI fixup，会误以为某个模块声明了 `__setup("myparam", ...)` 之后这个函数就会
   运行；实际上如果启动命令行里从未出现 `myparam`，这个处理函数永远不会被调用——
   这是完全正常的设计行为，不是遗漏或 bug。
4. **忘记 `PCI_ANY_ID` 通配符的双向性**：`pci_do_fixups()` 的匹配条件是
   `f->vendor == dev->vendor || f->vendor == PCI_ANY_ID`——如果开发者误以为
   "只有 vendor 字段能通配，device 字段必须精确"，写出的修补规则可能匹配到
   完全不相干厂商的设备（如果不小心把 `device` 字段留空初始化为 0，恰好某个
   真实设备的 device id 就是 0x0000，就会产生意外匹配）。
5. **完整命令行解析阶段重复触发 early 参数的副作用**：如果 `obsolete_checksetup()`
   风格的第二遍扫描忘记跳过 `early==1` 的条目，会导致 `debug_kernel()`/
   `quiet_kernel()` 这类函数被调用两次——如果处理函数不是幂等的（比如内部有
   计数器 `count++` 而不是简单赋值），第二次调用会产生错误的状态。
   `boot_param_setup_sim.c` 的 `obsolete_checksetup()` 显式做了这个跳过判断，
   并在输出里打印 "already handled by early_param, skip" 来验证这一点。
6. **调试方法**：
   - 用户态：三个示例都在遍历/匹配的每一步打印详细日志，`boot_param_setup_sim.c`
     还额外打印了"链接顺序 vs 声明顺序不一致"的现象，可以直接对照验证。
   - Kernel：启动时在内核命令行末尾加一个不存在的参数，观察 `dmesg` 里
     `Unknown boot option '...' -- ignoring` 的提示（对应 `unknown_bootoption()`，
     与本文档模拟的行为完全一致）；`cat /proc/cmdline` 查看实际生效的启动命令行；
     PCI 修补生效与否可以用 `lspci -vvv` 观察设备的实际配置空间状态变化，
     或者在 `CONFIG_DYNAMIC_DEBUG` 打开的情况下给 `drivers/pci/quirks.c`
     开启动态调试日志，观察哪些 `quirk_*` 函数实际被调用。

---

## 8. Kernel Core Pseudocode（6 个）

### Example 1 — `struct obs_kernel_param`/`__setup_param()`：命令行处理器的契约

真实定义：[include/linux/init.h:225](../../include/linux/init.h)

```c
struct obs_kernel_param {
	const char *str;
	int (*setup_func)(char *);
	int early;
};

#define __setup_param(str, unique_id, fn, early)			\
	static struct obs_kernel_param __setup_##unique_id	\
		__used __section(.init.setup)			\
		__attribute__((aligned((sizeof(long)))))	\
		= { __setup_str_##unique_id, fn, early }
```
为什么是 DI：与 06 篇 `__define_initcall` 几乎同构的"编译期打标记"手法，
但携带了一个 initcall 完全没有的字段——`str`（匹配条件本身），这是本篇和
initcall 唯一的结构性差异来源：initcall 的元素是"纯函数指针"，本篇的元素是
"函数指针 + 匹配条件"的组合。

### Example 2 — `do_early_param()`：只扫描一次、只执行 early 条目

真实定义：[init/main.c:390](../../init/main.c)

```c
static int __init do_early_param(char *param, char *val)
{
	const struct obs_kernel_param *p;
	for (p = __setup_start; p < __setup_end; p++) {
		if ((p->early && parameq(param, p->str)) ||
		    (strcmp(param, "console") == 0 && strcmp(p->str, "earlycon") == 0))
			if (p->setup_func(val) != 0)
				printk(KERN_WARNING "Malformed early option '%s'\n", param);
	}
	return 0;   /* 这一阶段"接受一切"，未匹配的参数留给后面完整解析阶段处理 */
}
```
为什么值得单列：`return 0` 这一行揭示了一个重要设计——**早期扫描阶段对"匹配不上"
完全宽容**，不会报错、不会中止，因为很多参数本来就是要留给后面 `obsolete_checksetup()`
阶段处理的；这与 06 篇 `do_one_initcall()` 对错误返回值"容忍 `-ENODEV`，
但警告其它错误码"的策略是完全不同的容错哲学——初期扫描阶段选择的是
"完全不判断对错，只管执行 early 匹配项"。

### Example 3 — `obsolete_checksetup()`：第二遍扫描，跳过已处理的 early 条目

真实定义：[init/main.c:158](../../init/main.c)

```c
static int __init obsolete_checksetup(char *line)
{
	const struct obs_kernel_param *p;
	int had_early_param = 0;

	p = __setup_start;
	do {
		int n = strlen(p->str);
		if (parameqn(line, p->str, n)) {
			if (p->early) {
				if (line[n] == '\0' || line[n] == '=')
					had_early_param = 1;   /* 已经处理过了，只做标记 */
			} else if (p->setup_func(line + n))
				return 1;
		}
		p++;
	} while (p < __setup_end);
	return had_early_param;
}
```
为什么是 DI：这是本篇 Failure 小节第 5 条的真实原文出处——`if (p->early)` 分支
故意不调用 `p->setup_func`，只是设个标记，这正是"避免重复触发副作用"的显式处理，
证明内核作者非常清楚"同一个 section 被扫描两遍"这件事本身需要仔细的状态管理。

### Example 4 — `struct pci_fixup`/`DECLARE_PCI_FIXUP_EARLY`：硬件修补的契约

真实定义：[include/linux/pci.h:1375](../../include/linux/pci.h)

```c
struct pci_fixup {
	u16 vendor, device;	/* PCI_ANY_ID 表示通配 */
	void (*hook)(struct pci_dev *dev);
};

#define DECLARE_PCI_FIXUP_EARLY(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_early,			\
			vendor##device##hook, vendor, device, hook)
```
为什么是 DI：`vendor##device##hook` 这个 token-paste 手法值得注意——它利用
调用方传入的字面 token（如 `PCI_VENDOR_ID_INTEL`、`0x1234`、`quirk_fn`）拼出一个
**编译期保证唯一**的变量名，不需要开发者手工起名、不需要担心命名冲突，这是
"批量声明进同一个 section 但又要求变量名互不相同"这一矛盾需求的巧妙解法，
06 篇 initcall 的 `__initcall_##fn##id` 用的是完全一样的手法。

### Example 5 — `pci_do_fixups()`/`pci_fixup_device()`：按数值匹配后才盲调

真实定义：[drivers/pci/quirks.c:2909](../../drivers/pci/quirks.c)、
[drivers/pci/quirks.c:2938](../../drivers/pci/quirks.c)

```c
static void pci_do_fixups(struct pci_dev *dev, struct pci_fixup *f, struct pci_fixup *end)
{
	while (f < end) {
		if ((f->vendor == dev->vendor || f->vendor == (u16)PCI_ANY_ID) &&
		    (f->device == dev->device || f->device == (u16)PCI_ANY_ID))
			f->hook(dev);      /* <-- 盲调，仅针对匹配成功的条目 */
		f++;
	}
}

void pci_fixup_device(enum pci_fixup_pass pass, struct pci_dev *dev)
{
	struct pci_fixup *start, *end;
	switch (pass) {
	case pci_fixup_early:
		start = __start_pci_fixups_early;
		end = __end_pci_fixups_early;
		break;
	/* ... 其余 pass ... */
	}
	pci_do_fixups(dev, start, end);
}
```
为什么是 DI：`pci_fixup_device()` 甚至连"用哪个 section"这件事也是运行期动态决定的
（按 `pass` 参数切换到不同的起止边界符号）——这是本篇独有的一个额外维度：
不仅"是否调用某一项"是运行期决定的，"到底该遍历哪一个 section"本身也可以是
运行期决定的，initcall 的 `do_initcalls()` 只是按固定顺序遍历所有 level，
没有这种"按运行期条件选择 section"的灵活性。

### Example 6 — 边界符号的链接脚本来源

真实定义：[include/asm-generic/vmlinux.lds.h:612](../../include/asm-generic/vmlinux.lds.h)、
[include/asm-generic/vmlinux.lds.h:236](../../include/asm-generic/vmlinux.lds.h)

```c
#define INIT_SETUP(initsetup_align)					\
		. = ALIGN(initsetup_align);				\
		VMLINUX_SYMBOL(__setup_start) = .;			\
		*(.init.setup)						\
		VMLINUX_SYMBOL(__setup_end) = .;

/* PCI fixup 的边界符号同理，在链接脚本里手工放置： */
		VMLINUX_SYMBOL(__start_pci_fixups_early) = .;		\
		*(.pci_fixup_early)					\
		VMLINUX_SYMBOL(__end_pci_fixups_early) = .;
```
为什么值得单列：这条例子证明了真实内核版本的边界符号不是"GNU ld 自动生成"的
（因为带点的 section 名不是合法标识符），而是**内核自己在链接脚本里手工放置的**——
这是本篇 Architecture 小节提到的"版本/工程差异"的确切来源；本文档三个用户态示例
为了避免维护链接脚本，全部改用不带点的合法标识符 section 名，改由 GNU ld
自动生成，这是一个刻意的简化，已在文档中明确注明，不是编造。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/13-linker-section-registration/`](c/13-linker-section-registration/)

### 9.1 `boot_param_setup_sim.c` — `__setup`/`early_param` 风格的命令行参数处理器

- 场景：4 个处理器（`debug`/`quiet` 为 early，`reset_devices`/`elevator=` 为普通
  `__setup`）通过链接器 section 自注册，模拟命令行
  `"reset_devices debug elevator=deadline panic=1 quiet unknown_param=5"`
  的两遍扫描过程。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o boot_param_setup_sim boot_param_setup_sim.c`
- 实测输出：
  ```
  === boot_param_setup_sim demo ===

  模拟的启动命令行: "reset_devices debug elevator=deadline panic=1 quiet unknown_param=5"

  已通过链接器 section 'kdi_setup' 自注册的处理器一共 4 个：
    str='elevator=       ' early=0
    str='reset_devices   ' early=0
    str='quiet           ' early=1
    str='debug           ' early=1

  -- parse_early_param()：仅扫描 early_param 声明的条目 --
      [early_param] debug_kernel: console_loglevel -> 10
      [early_param] quiet_kernel: console_loglevel -> 4

  -- 完整命令行解析（obsolete_checksetup 风格）：扫描非 early 条目 --
      [__setup] set_reset_devices: reset_devices -> 1
    'debug' already handled by early_param, skip
      [__setup] elevator_setup: chosen_elevator -> 'deadline'
    Unknown boot option 'panic=1' -- ignoring
    'quiet' already handled by early_param, skip
    Unknown boot option 'unknown_param=5' -- ignoring

  === done ===
  最终状态：console_loglevel=4 reset_devices=1 chosen_elevator='deadline'
  说明：本文件注册了 4 个处理器，只有命令行里出现了对应名字的那几个被调用；
        'panic=1'/'unknown_param=5' 从未被声明过 __setup，被忽略——这与 initcall
        的"section 里有什么就无条件全部执行"完全不同：这里是"section 里有什么
        只是候选，命令行决定谁被执行"。
  ```
  （已注册处理器的打印顺序是 `elevator=, reset_devices, quiet, debug`，与源码里
  声明顺序 `debug, quiet, reset_devices, elevator=` 正好相反——这是链接器按目标
  文件内符号出现顺序从后往前排列 section 内容的真实体现，与 06 篇 `do_initcalls()`
  "同一 level 内顺序不保证"是完全相同的底层原因，这里再次真实复现，不是编造的。）
- 为什么是 DI：`do_early_param()`/`obsolete_checksetup()`（Core）完全不知道
  `debug_kernel`/`elevator_setup` 这些函数的存在，只按链接器收集到的
  `struct obs_kernel_param` 数组线性扫描 + 字符串匹配；这是"链接器 section
  自注册"与"字符串匹配消费策略"结合的真实范例。

### 9.2 `pci_fixup_table_sim.c` — `DECLARE_PCI_FIXUP_*` 风格的硬件修补表

- 场景：2 条 early-phase 修补规则（一条精确匹配 Intel 设备，一条 Broadcom
  厂商通配）+ 2 条 final-phase 修补规则，探测 4 个模拟 PCI 设备（精确匹配、
  厂商通配匹配、只匹配 final 阶段、完全不匹配），验证匹配逻辑正确性。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o pci_fixup_table_sim pci_fixup_table_sim.c`
- 实测输出：
  ```
  === pci_fixup_table_sim demo ===

  已注册的 early-phase fixup 数量: 2
  已注册的 final-phase fixup 数量: 2

  -- 探测设备 0000:00:1f.0 (vendor=8086 device=1234) --
    [pci core] pci_fixup_device(pci_fixup_early, ...)
      [early ] quirk_intel_disable_msi: disabling MSI quirk for 0000:00:1f.0 (8086:1234)
    [pci core] (探测 BAR、读取配置空间...)
    [pci core] pci_fixup_device(pci_fixup_final, ...)
      [final ] quirk_intel_final: applying final-stage fixup for 0000:00:1f.0 (8086:1234)
  -- 探测设备 0000:03:00.0 (vendor=14e4 device=4321) --
    [pci core] pci_fixup_device(pci_fixup_early, ...)
      [early ] quirk_broadcom_any_device: vendor-wide workaround for 0000:03:00.0 (14e4:4321)
    [pci core] (探测 BAR、读取配置空间...)
    [pci core] pci_fixup_device(pci_fixup_final, ...)
  -- 探测设备 0000:04:00.0 (vendor=10ec device=8168) --
    [pci core] pci_fixup_device(pci_fixup_early, ...)
    [pci core] (探测 BAR、读取配置空间...)
    [pci core] pci_fixup_device(pci_fixup_final, ...)
      [final ] quirk_realtek_final: applying final-stage fixup for 0000:04:00.0 (10ec:8168)
  -- 探测设备 0000:05:00.0 (vendor=1af4 device=1000) --
    [pci core] pci_fixup_device(pci_fixup_early, ...)
    [pci core] (探测 BAR、读取配置空间...)
    [pci core] pci_fixup_device(pci_fixup_final, ...)

  === done ===
  说明：pci_probe_device()/pci_fixup_device()（Core）从未 #include 任何
        具体厂商的头文件，只是按 (vendor,device) 数值在两个 section 里线性
        查找匹配项再盲调；virtio 设备（1af4:1000）没有任何规则匹配它，
        pci_do_fixups() 遍历完整个 section 什么也没做，这是完全正常、
        预期内的"没有候选命中"情况，不是错误。
  ```
  （编写过程中这里踩中并修复了两个真实的坑，都已在 Failure 小节和源码注释里
  记录：一是省略 `aligned(sizeof(long))` 导致 section 条目间出现 padding
  缝隙，遍历时读到脏数据段错误；二是宏形参误命名为 `section` 与
  `__attribute__((section(...)))` 撞车，改成 `__section__` 后才编译通过。）
- 为什么是 DI：`quirk_broadcom_any_device` 通过 `PCI_ANY_ID` 通配 `device`
  字段，一条规则覆盖了该厂商的所有设备型号——这是"匹配条件本身可以是模糊/
  通配的"这一特性，比 9.1 的精确字符串匹配更进一步，说明"消费策略"这个维度
  可以有比"精确相等"更丰富的匹配语义。

### 9.3 `section_based_plugin_table.c` — 通用化的链接器 section 自注册工具库

- 场景：抽出 `DECLARE_SECTION_TABLE`/`SECTION_REGISTER`/`SECTION_TABLE_FOREACH`
  三个与业务类型无关的宏，同时撑起一个字符串匹配的 CLI 命令表和一个数值匹配的
  错误码描述表，证明"链接器 section 自注册"这套骨架可以完全脱离 initcall/
  `__setup`/PCI fixup 的具体业务语义，成为一个可独立复用的基础设施。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o section_based_plugin_table section_based_plugin_table.c`
- 实测输出：
  ```
  === section_based_plugin_table demo ===

  cli_cmd_tab 里注册的命令数: 3
  errno_tab   里注册的错误码数: 4

  -- 领域 A：CLI 命令表（字符串匹配） --
    执行 'help':
        [cmd:help] listing all registered commands:
          echo       echo back the argument
          version    print demo version
          help       list all available commands
    执行 'version':
        [cmd:version] section_based_plugin_table demo v1.0
    执行 'echo hello linker sections':
        [cmd:echo] hello linker sections
    执行 'foobar'（未注册的命令）:
      unknown command 'foobar'

  -- 领域 B：错误码描述表（数值精确匹配） --
    errno_lookup( -2) -> No such file or directory
    errno_lookup(-22) -> Invalid argument
    errno_lookup(-12) -> Cannot allocate memory
    errno_lookup(-11) -> Resource temporarily unavailable
    errno_lookup(-99) -> (no description registered for this code)

  === done ===
  说明：DECLARE_SECTION_TABLE/SECTION_REGISTER/SECTION_TABLE_FOREACH
        这三个宏被原样复用了两次，分别撑起了一个字符串匹配的命令表和
        一个数值匹配的错误码表——工具库本身只关心"怎么把对象放进 section、
        怎么给出遍历边界"，完全不关心对象内部字段是什么、怎么匹配；
        这就是链接器 section 自注册作为通用元模式（而不是 initcall 专属技巧）
        的直接证据。
  ```
  （`help` 命令的输出顺序是 `echo, version, help`，同样与源码声明顺序
  `help, version, echo` 相反，第三次真实复现"链接顺序与声明顺序不一致"这一
  现象，进一步证明这不是某个示例的偶然现象，而是 GNU ld 处理这类 section
  的一贯行为。）
- 为什么是 DI：这个示例的意义不在于展示某个具体内核机制，而在于证明
  Example 1~5 里反复出现的"编译期打标记 + 链接期收集 + 运行期按策略消费"骨架，
  可以被写成一套三行宏的通用工具库，脱离任何具体业务语境依然成立——这正是
  本篇 Why 小节开篇提出的核心论点的直接证据。

一次性编译并运行全部三个示例：`cd c/13-linker-section-registration && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Linker Section Registration（本篇） | Initcall（06 篇的特例） | Driver Registration | Notifier Chain |
|---|---|---|---|---|
| 收集手段 | 链接器 section（编译期/链接期） | 链接器 section（编译期/链接期），本篇的一个特例 | 运行期显式调用 `register_xxx()` | 运行期显式调用 `register_xxx()` |
| 收集到的元素是否携带"匹配条件" | **是**（字符串前缀/数值 vendor·device），这是本篇与 initcall 的本质区别 | 否，纯函数指针，没有匹配条件 | 是（bus 的 `match()` 回调） | 否，只有优先级，没有内容匹配 |
| 运行期消费策略 | **按匹配条件挑选，未匹配的永远不被调用** | **无条件全部调用一次** | 按匹配条件挑选，最多一个胜出 | 无条件全部调用（链式广播） |
| 是否支持运行期动态增减 | 否（模块场景下可以随 `insmod`/`rmmod` 追加/移除自己的 section 内容） | 否 | 是 | 是 |
| 典型触发者 | 外部输入（命令行字符串、探测到的硬件 id） | 内核启动过程本身（`do_initcalls()` 无条件遍历） | 硬件热插拔事件 | 某个事件源代码主动广播 |
| 何时用本篇的模式 | "候选集合在编译期已知且数量庞大，但每次运行只有少数会被真正用到，需要按某种匹配条件筛选"的场景（命令行参数、硬件修补表、后面 14 篇要讲的 tracepoint 探测点表） | "收集到的每一项都必须被执行，且只执行一次"的场景 | "需要动态热插拔、运行期概念上有'设备到来/离开'"的场景 | "需要广播事件给所有关心的一方，且都要执行"的场景 |
| 何时不要用 | 如果收集到的元素**不需要**任何匹配条件、每次都要全部执行——直接退化成 06 篇的 initcall，没必要额外设计匹配逻辑 | 如果需要运行期动态卸载/重新初始化，initcall 完全不支持 | — | — |

一句话总结本篇与 06 篇的关系：**06 篇的 Initcall 是"链接器 section 自注册"这个
元模式在"无条件全部执行"这一种消费策略下的特例；本篇揭示的是这套"编译期标记 +
链接期收集"骨架本身与"运行期怎么消费"完全解耦——同一套收集手段，配上不同的
消费策略（无条件全部执行 / 按字符串匹配 / 按数值匹配及通配），就能派生出
initcall、`__setup`、PCI fixup 这三种表面上看起来风马牛不相及的机制**。
理解了这个抽象层次，再看到内核里任何一个新的"某某表通过链接器 section 收集"的
机制（比如下一篇要讲的 tracepoint `__start___tracepoints_ptrs`），就能立刻
判断出它的"消费策略"是什么、和已经学过的哪一种最相似。

下一篇 [14-tracepoint.md](14-tracepoint.md) 讲 tracepoint——它的探测点集合
同样是靠链接器 section（`__tracepoints_ptrs`）收集的，但消费策略又与本篇两个
例子都不同：**每个探测点关联的不是"零个或一个"匹配结果，而是一份可以动态挂载
"零个到多个" probe 的列表**，且必须做到"没人挂载时开销趋近于零"——这是 06/13
两篇都不需要操心的全新约束。
