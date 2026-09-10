# 15. BPF Program Registration

> 系列索引见 [00-mental-model.md](00-mental-model.md)。本篇是第二批第一篇，聚焦
> "带沙箱验证的可加载执行体"这一手法：用户态提交的不再是原生机器码，而是一段
> **字节码数据**，内核必须先证明它"安全”才允许执行。

---

> **版本说明（务必先读）**
>
> 本仓库是 Linux 3.2.0（代号 Saber-toothed Squirrel），**不包含 BPF/eBPF 子系统**。
> eBPF（extended BPF，带寄存器式虚拟机、验证器、`bpf()` 系统调用、`BPF_PROG_LOAD`）
> 是 Linux 3.18 才引入的（`358f8be1c7` 等系列 commit，作者 Alexei Starovoitov）；
> 本仓库里能找到的只有 `arch/x86/net/bpf_jit_comp.c`、`arch/powerpc/net/bpf_jit*` ——
> 这些是**经典 BPF（classic BPF / cBPF，用于 socket filter）的 JIT 编译器**，
> 结构和现代 eBPF 的寄存器式验证器完全不是一回事，且本篇要讲的"可编程 hook 点注入"
> 场景在 3.2 里也不存在（`seccomp` 的 BPF 模式要到 3.5 才加入，`tc`/`XDP`/`kprobe`
> 挂载 BPF 程序更是 4.x 以后的事）。
>
> 因此本篇下面所有内容（第 2~8 节）都是**基于现代内核（4.x 之后）架构的概念性说明**，
> **不引用本仓库（`/home/morrism/linux/linux`）任何文件路径或行号**。第 8 节的
> "Kernel Core Pseudocode"会明确标注"概念性伪代码，参考现代内核架构（非本仓库实际代码）”，
> 绝不冒充本仓库里不存在的真实引用。第 9 节的 3 个用户态 C 示例是普通的、
> 真实可编译运行的 C 代码，不依赖任何内核头文件，只是借用"字节码 + 验证器"这个
> 核心思想来讲清楚 BPF 模式本身，与本仓库版本无关。

---

## 1. Why

前面的手法（01 篇 function pointer callback、04 篇 driver registration）都有一个共同的隐含前提：

> **注入方是可信的**。写驱动的人、写文件系统的人，代码要经过 review、要签名（或至少要有 root
> 权限才能 `insmod`），一旦注入，内核就把它当"自己的一部分”盲调，不做任何运行期检查。

但现实里出现了一类新场景：

- 一个**非特权用户**想给自己的 socket 装一个包过滤规则（早期 `tcpdump`/`libpcap` 的场景）。
- 一个**运维/可观测性工具**（`bcc`/`bpftrace`）想在生产环境的任意函数入口、任意 tracepoint
  上"挂一段小逻辑”，且这必须能在**不重新编译、不重启内核、不加载全信任模块**的前提下完成。
- 容器场景下，希望允许**租户自己**编写网络策略/过滤规则，但显然不能让租户提交任意机器码
  直接在内核态执行——那等于允许租户拿到内核权限。

如果没有 BPF 这层机制，只能有两个选择，都不理想：

1. **完全禁止用户态注入逻辑到内核路径**——退化回"只能在用户态收包后再过滤”，
   性能损失巨大（每个包都要经过一次用户态/内核态切换）。
2. **允许任意用户态代码作为内核模块加载**——等价于允许任意用户获得内核态代码执行权限，
   是一个巨大的安全漏洞。

BPF 的设计目标是：

> **在"完全不允许"和"完全信任"之间，开出第三条路：允许提交任意逻辑，
> 但强制这段逻辑必须先被静态证明"不可能作恶”，才能被解释/JIT 执行。**

这是 01 篇 callback（"要求提供者可信，无验证”）和 08 篇 LSM hook（"内核编译期决定，
不允许运行时新增逻辑”）之外的**第三种范式**：**受限的、可验证的运行时代码注入**。

---

## 2. Architecture

```
                        用户态程序 (bpftool / bcc / iproute2 / 应用自己)
                                        |
                                        |  1. 编写字节码程序 (BPF instructions)
                                        |  2. bpf(BPF_PROG_LOAD, attr)
                                        v
        +----------------------------------------------------------------+
        |                     Kernel: bpf syscall 入口                    |
        |                                                                  |
        |   +----------------+     +------------------------------+       |
        |   |  按 prog_type   | --> |  Verifier (静态验证器)         |       |
        |   |  分发对应规则   |     |  - 类型检查、边界检查           |       |
        |   +----------------+     |  - 禁止越界内存访问              |       |
        |                          |  - 禁止不可终止的循环            |       |
        |                          |  - 栈深度/复杂度上限             |       |
        |                          +---------------+--------------+       |
        |                                          | 验证通过            |
        |                                          v                      |
        |                          +------------------------------+       |
        |                          |  JIT / 解释器（生成可执行体）   |       |
        |                          +---------------+--------------+       |
        |                                          | 得到一个 fd          |
        +------------------------------------------|----------------------+
                                                    v
                              bpf_prog fd（内核持有的、已验证的可执行对象）
                                                    |
                                        3. bpf_prog_attach(fd, hook_point)
                                                    v
        +----------------------------------------------------------------+
        |                    Hook Point（内核真实的挂载位）                |
        |   网络收包路径 / syscall 入口 / tracepoint / kprobe / cgroup ...  |
        +----------------------------------------------------------------+
                                                    |
                                     4. 事件真实发生时，Core 盲调
                                                    v
                                    prog->bpf_func(ctx)  -> 返回值决定后续行为
```

对应到本文档的三个用户态示例：

```
toy_bytecode_verifier:      bpf_verify() + bpf_run()               （验证 + 单次执行）
pluggable_prog_type_ops:    g_prog_ops_table[prog_type]->verify/run （按类型分发规则）
hook_point_bpf_attach:      net_recv_hook_point.attached[]          （挂接 + 常驻策略）
```

---

## 3. Lifecycle

```
   write bytecode            (用户态：用受限指令集写一段程序，
                               或由 clang 的 BPF backend 从受限 C 子集编译出来)
        |
        v
   BPF_PROG_LOAD              (系统调用提交字节码 + 声明 prog_type)
        |
        v
   verify                     (Verifier 静态扫描：类型/边界/终止性/栈深度)
        |          \
        | 通过       \ 拒绝
        v            v
   JIT / interpret   -EINVAL / -EACCES，程序从未进入可执行状态
        |
        v
   obtain prog fd              (内核返回一个文件描述符代表这个已验证程序)
        |
        v
   attach                      (bpf_prog_attach()/setsockopt(SO_ATTACH_BPF)/
                                 kprobe 注册等——把 fd 关联到某个 hook 点)
        |
        v
   framework owns dispatch     (Core：hook 点自己的正常业务逻辑决定何时触发)
        |
        v
   run on every event          (每次事件发生，Core 盲调 prog->bpf_func(ctx)，
                                 返回值影响本次事件的处理结果——放行/丢弃/记录)
        |
        v
   detach / fd close           (显式 detach，或 fd 被 close 且无其它引用时，
                                 内核回收已验证程序占用的资源)
```

三个示例里分别是：

- `toy_bytecode_verifier`：`bpf_verify()` -> `bpf_run()`（没有 attach 阶段，单次调用即演示完整验证）
- `pluggable_prog_type_ops`：`bpf_prog_load()`（内部调用按类型选中的 verify）-> `bpf_prog_run_and_report()`
- `hook_point_bpf_attach`：`bpf_verify()` -> `bpf_attach()` -> 多次 `net_recv_hook_run()` -> `bpf_detach()`

---

## 4. Relationship

```
   用户态提交者（可能不完全可信）
        |  提交
        v
   字节码程序（数据，不是代码——这是与 01 篇最本质的区别）
        |  经过
        v
   Verifier（Core 的一部分，类型可插拔：见第 8 节 Example 2）
        |  产出
        v
   已验证程序对象（bpf_prog，内核持有，附带元数据：prog_type/复杂度统计等）
        |  被
        v
   Hook 点（Core 定义的挂载位，不知道挂的是谁）
        |  在事件发生时
        v
   JIT 后的机器码 / 解释器逐条执行（真正的执行体，由 Verifier 事先"背书”）
```

关键点：**Hook 点信任的不是"这段程序的作者”，而是"Verifier 给出的证明”**——
这是本手法与 01/04 篇最大的信任模型差异：01/04 篇信任"身份”（谁写的、谁加载的），
BPF 信任"被证明的性质”（这段程序在数学上不可能越界/不可能不终止）。

---

## 5. Data Flow

以 `hook_point_bpf_attach.c` 为例，完整调用链：

```
main()
  |
  |-- bpf_verify(&drop_udp)          // 提交阶段：静态验证，不执行任何一条指令
  |
  |-- bpf_attach(&hook_point, &drop_udp)   // 挂接：只有 verified==1 才允许挂上
  |
  |-- bpf_verify(&drop_jumbo)
  |-- bpf_attach(&hook_point, &drop_jumbo)
  |
  |-- bpf_attach(&hook_point, &unverified)  // 拒绝：unverified.verified == 0
  |
  `-- simulate_packet(&hook_point, pkt)
        |
        v
     net_recv_hook_run(hp, pkt)      // Core：不知道挂了什么程序
        |
        |  for each attached program:
        v
     bpf_run(prog, pkt)              // <-- DI 发生的确切位置：运行期盲调已验证程序
        |
        v
     逐条解释执行字节码，返回 PASS/DROP
```

对照真实 BPF 收包路径（概念性，4.x 之后架构，非本仓库实际代码）：

```
__netif_receive_skb_core()
  |
  v
bpf_prog_run_save_cb(xdp_prog, skb)   // 与 bpf_run(prog, pkt) 是同一种结构
  |
  v
BPF_PROG_RUN(prog, ctx)                // 宏展开为 prog->bpf_func(ctx, prog->insnsi)，
  |                                    // 已经是 JIT 编译后的机器码，不再逐条解释
  v
XDP_PASS / XDP_DROP / XDP_TX / ...
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| 字节码本身（`insns[]`） | **验证通过后不可变** | 真实内核里 `bpf_prog` 一旦验证完成，指令区只读，防止 TOCTOU（验证时是 A，执行时被改成 B）攻击 |
| `verified` 标志 | **一次性从 0 变 1，不可逆** | 示例中 `bpf_verify()` 通过才置位；没有"反验证"操作 |
| Hook 点的挂载列表 | **高度动态** | attach/detach 随时改变列表内容，`net_recv_hook_run()` 每次都要重新读取 |
| Verifier 的规则表（按 prog_type） | **编译期/模块加载期固定** | 见第 8 节 Example 2；新增一种 prog_type 不需要改 Core 分发逻辑，但规则本身一旦发布就相对稳定，避免"同一段程序，不同内核版本验证结果不同"带来的兼容性问题 |
| 运行期上下文（`ctx`/`packet`） | **每次调用都是新的** | Verifier 只保证"访问 ctx 的方式是安全的”，不保证 ctx 内容——这是执行期的事 |

---

## 7. Failure

1. **验证器本身的 bug（漏判）**：如果 Verifier 有漏洞，一段本应被拒绝的程序被错误接受，
   相当于沙箱被绕过——真实历史上 Linux Verifier 出现过多个可以被利用的 CVE
   （越界读写、指针泄露、整数溢出导致的边界检查绕过），这是这个机制"复杂度最高、
   风险最集中"的部分，也是为什么很多安全加固方案选择直接禁用非特权 BPF
   （`kernel.unprivileged_bpf_disabled`）。
2. **验证通过但运行期资源耗尉**：Verifier 只能证明"静态性质”（不越界、会终止），
   不能完全阻止"验证通过但实际执行很慢"的程序在高频 hook 点（比如每个包都触发）
   造成性能雪崩——这是为什么真实内核会有指令数上限、循环次数上限（bounded loop）。
3. **TOCTOU（校验后再改）**：本示例特意把 `insns[]` 和 `verified` 放在同一个
   struct 里、且验证/执行都直接读同一份数据，如果字节码所在内存在验证之后到执行之前
   被并发修改，就会出现"验证的是 A，跑的是 B”的漏洞——真实内核通过"验证后的程序只读、
   且与用户态内存完全隔离（内核自己持有一份拷贝）"来避免。
4. **prog_type 与 hook 点不匹配**：`pluggable_prog_type_ops.c` 演示了把一个
   `SOCKET_FILTER` 程序错误标记成 `SYSCALL_FILTER` 类型提交，被对应类型的验证规则
   （不允许访问 `FIELD_PKT_LEN`）拒绝——真实内核会检查 `expected_attach_type`，
   防止"给 tracepoint 写的程序被挂到网络路径上，访问了不存在的字段”。
5. **挂载链过长拖慢关键路径**：Hook 点上挂的程序越多，每次事件触发的开销越大，
   `hook_point_bpf_attach.c` 的短路逻辑（一旦 DROP 立即返回）演示了真实内核
   XDP/tc-bpf 链式调用中"提前退出"的必要性。
6. **调试方法**：
   - 用户态（本示例）：给 `bpf_verify()`/`bpf_run()` 加日志（如示例中打印 REJECTED 原因），
     模拟真实 `bpf()` 系统调用失败时 `errno` + `bpf_log_buf`（verifier log）的调试体验。
   - 概念性（现代内核）：`bpftool prog show`/`bpftool prog dump xlated` 查看已加载程序的
     指令和验证结果；`BPF_LOG_LEVEL` 打开详细验证日志；`bpftool prog tracelog` 配合
     `bpf_trace_printk()` 做运行期调试；`/proc/sys/kernel/unprivileged_bpf_disabled`
     控制非特权加载权限；`perf record`/`bpftool prog profile` 观察挂载程序对关键路径的
     性能影响。

---

## 8. Kernel Core Pseudocode（5 个，概念性伪代码，参考现代内核架构，非本仓库实际代码）

> 再次强调：以下全部为**概念性说明**，取材自公开的现代（4.x+）Linux 内核架构文档与源码
> 的通用认知，**不对应本仓库 `/home/morrism/linux/linux`（3.2.0）任何真实文件或行号**，
> 因为该子系统在本仓库版本中根本不存在。

### Example 1 — `bpf(BPF_PROG_LOAD, ...)` 系统调用入口

场景：用户态提交字节码 + 声明 `prog_type`，内核先验证再决定是否生成可执行体。

```c
/* 用户态：提交 */
union bpf_attr attr = {
	.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
	.insns     = (u64)insns,
	.insn_cnt  = insn_cnt,
	.license   = (u64)"GPL",
};
int prog_fd = bpf(BPF_PROG_LOAD, &attr, sizeof(attr));

/* 概念性核心：sys_bpf() -> bpf_prog_load() */
static int bpf_prog_load(union bpf_attr *attr)
{
	struct bpf_prog *prog = bpf_prog_alloc(attr->insn_cnt);
	bpf_prog_fill_from_user(prog, attr);

	if (bpf_check(&prog, attr))      /* <-- Verifier，见 Example 3 */
		return -EINVAL;

	prog = bpf_prog_select_runtime(prog); /* 选择 JIT 或解释器 */
	return bpf_prog_new_fd(prog);
}
```
为什么是 DI：`bpf_prog_load()`（Core）不知道字节码具体做什么，只按照统一的
"验证 + 生成可执行体"流程处理；新增一种 `prog_type` 只需要注册新的验证/执行规则
（见 Example 2），不需要修改这条主流程。

### Example 2 — 按 `prog_type` 分发的 verifier ops（概念性 `bpf_verifier_ops`）

场景：不同 `prog_type` 允许访问的上下文字段、允许调用的 helper 函数完全不同。

```c
/* 概念性：每种 prog_type 注册自己的 ops，核心分发逻辑只查表 */
struct bpf_verifier_ops {
	const struct bpf_func_proto *(*get_func_proto)(enum bpf_func_id id);
	bool (*is_valid_access)(int off, int size, enum bpf_access_type t);
};

static const struct bpf_verifier_ops sk_filter_ops = { ... };
static const struct bpf_verifier_ops xdp_ops       = { ... };

static const struct bpf_verifier_ops *bpf_prog_type_ops[BPF_PROG_TYPE_MAX] = {
	[BPF_PROG_TYPE_SOCKET_FILTER] = &sk_filter_ops,
	[BPF_PROG_TYPE_XDP]           = &xdp_ops,
	/* ... */
};

/* bpf_check() 内部按类型查表，调用对应的 is_valid_access() 检查每一次内存访问 */
ops = bpf_prog_type_ops[prog->type];
if (!ops->is_valid_access(insn->off, insn->size, access_type))
	return -EACCES;
```
为什么是 DI：这正是 02 篇 ops table 手法在 verifier 场景下的复用——
`bpf_check()`（Core）永远不 `#include` 某个具体 `prog_type` 的规则实现，
只通过 `bpf_verifier_ops` 这份契约查表分发，本文档 `pluggable_prog_type_ops.c`
是这个概念的直接用户态还原。

### Example 3 — Verifier 的静态遍历（概念性 `do_check()`）

场景：逐条"符号执行"字节码，维护每个寄存器可能的取值范围/类型，拒绝任何可能越界的路径。

```c
/* 概念性核心循环 */
static int do_check(struct bpf_verifier_env *env)
{
	for (insn_idx = 0; insn_idx < insn_cnt; insn_idx++) {
		struct bpf_insn *insn = &env->prog->insnsi[insn_idx];

		if (BPF_CLASS(insn->code) == BPF_ALU)
			check_alu_op(env, insn);
		else if (BPF_CLASS(insn->code) == BPF_LDX)
			check_mem_access(env, insn);   /* 边界检查在这里发生 */
		else if (BPF_CLASS(insn->code) == BPF_JMP)
			check_cond_jmp_op(env, insn);  /* 分支导致状态分裂，两条路径都要验证 */

		if (env->insn_processed++ > BPF_COMPLEXITY_LIMIT_INSNS)
			return -EINVAL;    /* 复杂度上限，防止验证本身被 DoS */
	}
	return 0;
}
```
为什么是 DI：`do_check()`（Core）的验证规则本身是"通用框架”，具体每条指令
"允许还是拒绝"的判断逻辑（`check_mem_access`/`check_alu_op`）按 opcode 分发到
不同的检查函数，新增一种指令类型不需要重写整个验证循环——本文档
`toy_bytecode_verifier.c` 的 `bpf_verify()` 是这个思想的极简还原（只是没有做
真正的符号执行/取值范围追踪，只做了边界和终止性检查）。

### Example 4 — Hook 点触发（概念性 `BPF_PROG_RUN`）

场景：真实事件发生时（收到一个包、进入一个 tracepoint），Core 完全不知道挂载的程序内容。

```c
/* 概念性：include/linux/filter.h 里的 BPF_PROG_RUN 宏（4.x 之后架构） */
#define BPF_PROG_RUN(prog, ctx) ({ (*(prog)->bpf_func)(ctx, (prog)->insnsi); })

/* 概念性：网络收包路径 */
static u32 netif_receive_generic_xdp(struct sk_buff *skb, struct bpf_prog *xdp_prog)
{
	struct xdp_buff xdp = { .data = skb->data, .data_end = skb->data + skb->len };
	u32 act = BPF_PROG_RUN(xdp_prog, &xdp);   /* <-- 盲调，已验证 + 已 JIT */

	switch (act) {
	case XDP_DROP: return XDP_DROP;
	case XDP_PASS: return XDP_PASS;
	default:       return XDP_ABORTED;
	}
}
```
为什么是 DI：`netif_receive_generic_xdp()`（Core）从不知道 `xdp_prog` 内部逻辑，
只依赖"输入 `ctx`、输出一个动作枚举"这份契约——与 01 篇 `f_op->read()` 的
盲调结构完全同构，唯一区别是这次的"实现"是被验证过的字节码，而不是原生编译的驱动代码。

### Example 5 — 程序引用计数与卸载（概念性 `bpf_prog_put`）

场景：多个 hook 点/多个进程可能引用同一个 `bpf_prog`（比如同一个 fd 被 `SCM_RIGHTS`
传递给另一个进程），必须等最后一个引用释放才能真正回收。

```c
/* 概念性：类似 kref 的引用计数模式（03 篇 Context Injection 里也用到） */
void bpf_prog_put(struct bpf_prog *prog)
{
	if (atomic_dec_and_test(&prog->aux->refcnt)) {
		/* 引用计数归零，才真正释放已验证程序占用的内存/JIT 代码页 */
		bpf_prog_free(prog);
	}
}

/* attach 时增加引用，detach/fd close 时减少引用 */
int bpf_prog_attach(struct bpf_prog *prog, struct hook_point *hp)
{
	bpf_prog_inc(prog);           /* refcnt++ */
	hook_point_add(hp, prog);
	return 0;
}
```
为什么是 DI：这是"验证一次，多处复用"的生命周期管理——验证的成本只需要在
`BPF_PROG_LOAD` 时付一次，之后无论挂到多少个 hook 点、被多少个进程引用，
都不需要重新验证，这是 BPF 相比"每次执行前都重新解释检查"的方案的关键性能优势，
也呼应 07 篇 workqueue 里"Core 管理生命周期，实现者只管业务逻辑"的分工模式。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/15-bpf-program-registration/`](c/15-bpf-program-registration/)

### 9.1 `toy_bytecode_verifier.c` — 极简字节码 + 验证器

- 场景：6 条指令（`LOAD`/`ADD`/`SUB`/`CMP`/`JMP`/`RET`）的字节码虚拟机，加载前先跑验证器
  （指令数上限、寄存器越界、跳转目标越界、禁止后向跳转即禁止无限循环、必须以 `RET` 结尾），
  验证通过才允许执行。
- 编译：
  ```
  cd c/15-bpf-program-registration
  gcc -Wall -Wextra -O2 -std=gnu11 -o toy_bytecode_verifier toy_bytecode_verifier.c
  ```
- 实测输出：
  ```
  === toy_bytecode_verifier demo ===

  [verifier] checking program 'good_math (compute (10+32)-7)' (6 insns)... PASSED
    [executor] program ran, return value = 35
  [verifier] checking program 'branch_final (if 5>3 return 100 else return 200)' (8 insns)... PASSED
    [executor] program ran, return value = 100
  [verifier] checking program 'evil_loop (backward jump, infinite loop attempt)' (3 insns)... REJECTED: backward jump rejected (would allow unbounded loop)
  [verifier] checking program 'evil_reg (out-of-range register access)' (2 insns)... REJECTED: register index out of range
  [verifier] checking program 'evil_jump (jump target out of program bounds)' (2 insns)... REJECTED: jump target out of bounds
  [verifier] checking program 'no_ret (missing terminating RET)' (2 insns)... REJECTED: program does not end with RET
  [verifier] checking program 'bad_op (unrecognized opcode)' (1 insns)... REJECTED: unknown opcode
  [verifier] checking program 'too_long (exceeds MAX_INSNS complexity limit)' (65 insns)... REJECTED: program too long (exceeds complexity limit)

  === done ===
  ```
- 为什么是 DI：`bpf_run()`（执行器）完全不关心字节码"从哪来”，只信任
  "已经通过 `bpf_verify()`”这一前提；`bpf_verify()` 本身也不关心字节码要"做什么”，
  只做纯粹的静态形状检查——验证器和执行器都是与具体业务逻辑解耦的通用 Core，
  真正的"业务逻辑"（算 `(10+32)-7`、算 `if 5>3`）完全是被注入的数据。

### 9.2 `pluggable_prog_type_ops.c` — 按 prog_type 分发验证/执行规则

- 场景：`PROG_TYPE_SOCKET_FILTER`（只能访问包长度，不能跳转，但有专属的
  `OP_DENY_IF_LT` 早退指令）与 `PROG_TYPE_SYSCALL_FILTER`（能访问 syscall
  号/参数，能跳转，但不能访问包长度字段）各自持有独立的 `verify`/`run` 实现，
  核心分发逻辑 `bpf_prog_load()`/`bpf_prog_run_and_report()` 按 `prog_type`
  查表调用，从不关心具体规则。
- 编译：
  ```
  gcc -Wall -Wextra -O2 -std=gnu11 -o pluggable_prog_type_ops pluggable_prog_type_ops.c
  ```
- 实测输出：
  ```
  === pluggable_prog_type_ops demo ===

  -- 加载网络过滤程序 --
  [core] load 'drop_small_packets' (type=SOCKET_FILTER): ACCEPTED
    [core] run 'drop_small_packets' -> verdict = ALLOW
    [core] run 'drop_small_packets' -> verdict = DENY

  -- 尝试把网络过滤程序错误标记为 SYSCALL_FILTER 类型加载 --
  [core] load 'drop_small_packets_mislabeled' (type=SYSCALL_FILTER): REJECTED (syscall_filter: FIELD_PKT_LEN not available in this context)

  -- 加载 syscall 过滤程序：只允许 syscall_nr <= 300（模拟白名单） --
  [core] load 'syscall_nr_whitelist' (type=SYSCALL_FILTER): ACCEPTED
    [core] run 'syscall_nr_whitelist' -> verdict = ALLOW
    [core] run 'syscall_nr_whitelist' -> verdict = DENY

  -- 尝试加载一个 syscall 过滤程序，但跳转目标非法 --
  [core] load 'syscall_bad_jump' (type=SYSCALL_FILTER): REJECTED (syscall_filter: bad jump target 99)

  -- 尝试加载一个未知 prog_type 的程序 --
  [core] load 'unknown_type_prog': unknown prog_type 999

  === done ===
  ```
  （`drop_small_packets` 对 `pkt_len=1500` 判 `ALLOW`、对 `pkt_len=20` 判 `DENY`，
  验证了 `OP_DENY_IF_LT` 分支逻辑确实按运行期数据分叉；把同一段字节码错误标记成
  另一种 `prog_type` 被拒绝，验证了"验证规则真的因类型而异”，不是摆设。）
- 为什么是 DI：新增一种 `prog_type`（比如"tracepoint 类型”）只需要在
  `g_prog_ops_table[]` 里加一行、实现一份新的 `bpf_prog_ops`，`bpf_prog_load()`
  和 `bpf_prog_run_and_report()` 两个 Core 函数一行都不用改——这是 02 篇
  ops table 手法与本篇"验证沙箱"思想的直接结合。

### 9.3 `hook_point_bpf_attach.c` — 挂接到 Hook 点的常驻策略

- 场景：`drop_udp`（丢弃 UDP 包）和 `drop_jumbo`（丢弃超过 1400 字节的包）两个已验证程序
  同时挂在模拟的 `net_recv_hook_point` 上，每次"收包"都触发两个程序依次判定，
  任意一个判 `DROP` 就短路丢弃；同时演示"未验证程序禁止挂接"和"detach 后策略立即失效"。
- 编译：
  ```
  gcc -Wall -Wextra -O2 -std=gnu11 -o hook_point_bpf_attach hook_point_bpf_attach.c
  ```
- 实测输出：
  ```
  [attach] refuse to attach 'unverified_evil': not verified
  === hook_point_bpf_attach demo ===

  -- 验证并挂接 drop_udp --
  [attach] program 'drop_udp' attached to hook point 'net_recv_hook_point' (slot 0)

  -- 验证并挂接 drop_jumbo --
  [attach] program 'drop_jumbo' attached to hook point 'net_recv_hook_point' (slot 1)

  -- 尝试直接挂接未验证的程序（应被拒绝） --

  -- 模拟收包，2 个已验证程序同时生效 --
    [recv] pkt(len=512, proto=6) [TCP, normal size] -> PASS
    [recv] pkt(len=512, proto=17) [UDP, should be dropped by drop_udp] -> DROP
    [recv] pkt(len=9000, proto=6) [TCP, jumbo, should be dropped by drop_jumbo] -> DROP
    [recv] pkt(len=64, proto=1) [ICMP, small, should pass] -> PASS

  -- detach drop_udp，之后 UDP 包应该重新放行 --
  [detach] program 'drop_udp' detached from hook point 'net_recv_hook_point'
    [recv] pkt(len=512, proto=17) [UDP, drop_udp already detached] -> PASS
  ```
  （第一行 `[attach] refuse to attach 'unverified_evil': not verified` 是 stderr
  提前刷出，对应"未验证程序被拒绝挂接"这一校验点，与 01 篇同类现象一致，不影响逻辑；
  UDP 包在 `drop_udp` 挂载期间被丢弃，detach 之后同样的包立即恢复放行，证明
  Hook 点确实是"实时读取当前挂载列表”，不是一次性快照。）
- 为什么是 DI：`net_recv_hook_run()`（Core）完全不知道挂载了几个程序、是什么逻辑，
  只认"遍历挂载列表，逐个盲调，任意 DROP 就短路”这条统一契约；新增一种过滤规则
  只需要写一段新字节码并 `bpf_attach()`，不需要修改收包路径的任何代码——这正是
  BPF 相对传统内核模块最大的敏捷性优势：策略变更不需要重新编译/加载内核模块。

一次性编译并运行全部三个示例：`cd c/15-bpf-program-registration && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Function Pointer Callback (01) | BPF Program Registration (15) | Security Hooks / LSM (08) |
|---|---|---|---|
| 注入的是什么 | 已编译的原生函数地址 | **字节码（数据）**，由内核验证后再执行/JIT | 已编译的原生函数地址（与 01 篇同构） |
| 是否需要运行期安全验证 | **否** —— 完全信任提供者 | **是** —— Verifier 静态证明安全性后才放行 | 否 —— 信任编译期决定挂载的安全模块 |
| 提供者信任等级要求 | 高（等价于内核代码本身） | **可以是非特权用户**（验证器替代了信任） | 高（LSM 模块通常需要特殊编译配置/`securityfs`） |
| 何时决定"挂哪些实现" | 编译期或加载期（`insmod`） | **运行期，用户随时可以 attach/detach**，无需重启/重新编译 | 编译期决定"有哪些 LSM 可用”，运行期只能在已启用的模块间选择（`securityfs` 部分可配置） |
| 支持多实现同时生效 | 视 Core 设计（表/链表可支持） | **是**，天然支持链式挂载（本文档示例的多程序短路） | 是，LSM stacking（多个安全模块可以同时启用） |
| 性能特征 | 一次指针跳转，零额外开销 | 验证只在加载时付一次代价，执行路径是 JIT 后的原生代码，接近零额外开销 | 一次指针跳转，零额外开销（与 01 篇一致） |
| 典型场景 | "某一件事发生时调用我"，提供者可信 | "允许不完全可信的用户注入可观测性/过滤逻辑，但必须证明安全" | "在系统关键路径上强制一票否决式的安全策略，策略来源受严格管控" |
| 何时用 BPF 而不是 Callback | 注入方不完全可信（非特权用户/租户/第三方工具），且需要运行期动态增删逻辑而不重启系统 | — | — |
| 何时不要用 BPF | 提供者本来就完全可信（比如内核自己的子系统之间），Verifier 的验证成本和复杂度反而是纯粹的负担，应该直接用 01/02 篇的原生回调/ops table | — | — |

一句话：**Callback (01) 是"信任身份，零验证”；BPF (15) 是"不信任身份，但信任被证明的性质，
用一次性静态验证换取运行期动态、低门槛的代码注入”；LSM (08) 是"信任身份，但把‘要不要生效’
的决定权下放到编译期配置”**。三者构成了 DI 谱系里"信任模型"维度上的三个典型位置：
**完全信任 -> 验证后信任 -> 编译期受控信任**。
