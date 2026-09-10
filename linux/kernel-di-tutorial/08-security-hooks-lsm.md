# 08. Security Hooks（LSM，Linux Security Module）

> 05 篇 Notifier Chain 是"广播、各自独立响应"；本篇是它的一个特殊变体：
> **"强制检查点"——多个安全模块必须依次给出裁决，任意一个说"不"，结果就是"不"。**

---

## 1. Why

内核需要在大量"安全敏感"的操作点（打开文件、发送信号、`ptrace` 附加、创建 socket……）上做访问控制检查。
但"用什么策略做检查"这件事，不同场景需求完全不同：

- 传统 Unix 权限位（rwx）
- SELinux 的强制访问控制（Mandatory Access Control，基于安全上下文标签）
- AppArmor 的基于路径的策略
- Yama 的 `ptrace` 限制
- 某些场景甚至完全不需要额外检查（`CONFIG_SECURITY=n`）

如果把某一种策略（比如 SELinux 的算法）硬编码进 VFS/`do_fork`/`kill` 等核心路径，会导致：

1. **不需要 SELinux 的用户被迫承担它的运行时开销和复杂度**
2. **无法同时支持多种策略叠加**（现代系统经常同时启用 SELinux + Yama）
3. **策略更新需要修改并重新编译核心代码**，不符合"策略应该能独立演化"的原则

LSM 的设计目标：

> **把"在关键操作路径上必须做安全检查"这一事实固化在核心代码里（`security_xxx()` 调用点），
> 但"具体检查逻辑是什么"完全下放给可插拔的安全模块；核心代码只知道"调用这一个（或一组）
> hook，用返回值决定是否放行"，不知道背后是 SELinux、AppArmor 还是什么都没装。**

这是"强制性"与"可插拔性"的结合：`security_file_open()` 这一行调用**不能被绕过**（这是它区别于
普通 callback 的关键——业务代码没有"不检查"的选项），但检查逻辑本身是彻底可替换的。

---

## 2. Architecture

```
   VFS / 进程管理 / 网络子系统（关键操作路径，Core）
        |
        | 每个敏感操作前，无条件调用
        v
   security_xxx()（如 security_file_open()）
        |
        | 遍历该 hook 点上挂的所有已启用 LSM
        v
   +--------------------------------------------------+
   |  LSM A -> LSM B -> LSM C -> ...（本仓库版本是单个   |
   |  security_ops 指针，只能选一个；现代 Kernel 是链表， |
   |  支持多个同时叠加）                                  |
   +--------------------------------------------------+
        |
        | 任意一个返回非 0（拒绝）
        v
   立即返回错误，操作被阻止（"一票否决"）
        | 全部返回 0
        v
   操作被允许，继续往下执行
```

真实定义（本仓库版本，单 LSM）：[include/linux/security.h:1380](../../include/linux/security.h)、
[security/security.c:29](../../security/security.c)

```c
struct security_operations {
	char name[SECURITY_NAME_MAX + 1];
	int (*ptrace_access_check)(struct task_struct *child, unsigned int mode);
	int (*capget)(struct task_struct *target, ...);
	/* ... 上百个 hook 字段 ... */
};

static struct security_operations *security_ops;   /* 全局唯一指针 */

int security_ptrace_access_check(struct task_struct *child, unsigned int mode)
{
	return security_ops->ptrace_access_check(child, mode);   /* <-- 盲调 */
}
```

（真实 Kernel 4.2+ 之后演进为 `security_hook_list` + `hlist_head`，每个 hook 点是一条链，
支持多个 LSM"堆叠"——本文档三个用户态示例中的 `lsm_hook_chain.c` 演示的正是这种"多 LSM 叠加"形态，
更贴近现代 Kernel 的真实行为，同时兼容本仓库版本"单一策略"的历史形态。）

---

## 3. Lifecycle

```
   allocate
   (定义 static struct security_operations my_lsm_ops = { .file_open = ..., ... })
        |
        v
   initialize
   (填好该 LSM 关心的 hook 字段；不关心的字段留空或指向"默认放行"实现)
        |
        v
   inject（注册，且带独占检查）
   (register_security(&my_lsm_ops) —— 本仓库版本只能注册一次，
    见 security/security.c:121 "if (security_ops != &default_security_ops) return -EAGAIN")
        |
        v
   framework owns dispatch
   (VFS/进程管理等核心路径在每次敏感操作前调用 security_xxx())
        |
        v
   callback（强制检查）
   (my_lsm_ops.file_open(...) 被调用，返回 0/非 0 决定操作是否继续)
        |
        v
   destroy
   (reset_security_ops() 恢复到 default_security_ops，通常发生在系统关闭/重新加载策略时，
    真实生产环境几乎不会在运行期"卸载"一个已生效的强制访问控制模块)
```

---

## 4. Relationship

```
   VFS/进程管理（Core）
        |
        | 调用 security_xxx()
        v
   security_ops（本仓库：全局唯一指针；现代 Kernel：hook 链表）
        |
        | 指向/链接
        v
   具体 LSM 实现（SELinux / AppArmor / Yama / 自定义模块）
        |
        | 内部维护
        v
   策略数据（安全上下文标签、规则表、白名单 —— Policy Injection，见 9.2 示例）
```

---

## 5. Data Flow

以 `lsm_hook_chain.c` 为例：

```
main()
  |
  +- security_add_lsm(&selinux_style_hooks)
  +- security_add_lsm(&yama_style_hooks)
  +- security_add_lsm(&custom_guard_hooks)
        |
        v
  security_file_open(&user_cred, &shadow_file)
        |
        v
  遍历 g_registered_lsms.lsms[]：
     selinux_style_file_open(cred, file)  -> 返回 -13（拒绝）
        |
        | ret != 0，立即停止遍历
        v
  security_file_open() 返回 -13，VFS open() 调用方得知被拒绝，
  yama-style/custom-init-guard 的 file_open 钩子（如果有）根本不会被调用
```

对照真实 Kernel VFS 路径（`do_filp_open()` 风格，调用 `security_inode_permission()` 等）：

```
do_sys_open()
  |
  v
do_filp_open() -> path_openat()
  |
  v
may_open() -> inode_permission() -> security_inode_permission()
  |
  v
security_ops->inode_permission(inode, mask)   <-- 盲调，可能是 SELinux 的强制访问控制
  |  返回非 0
  v
open() 系统调用返回 -EACCES，文件从未被真正打开
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `security_ops` 指针（本仓库版本） | **注册后基本固定**，且只能成功注册一次 | `security/security.c:121` 显式检查"已经不是 default 就拒绝再注册" |
| 每个 LSM 的内部策略数据（规则表、标签映射） | **可能运行期变化**（如 SELinux 策略热加载 `load_policy`） | LSM 内部管理，Core 不感知 |
| hook 调用的返回值 | 每次调用独立判断，不是持久状态 | 由"这一次"具体的凭证（cred）、目标对象共同决定 |
| 当前生效的策略（`policy_injection_acl.c` 里的 `g_active_policy`） | **运行期可切换** | 演示了"检查点固定，策略可替换"的核心思想 |

---

## 7. Failure

1. **Hook 遗漏（fail-open）**：如果某个新增的敏感操作路径忘记调用对应的 `security_xxx()`，
   等价于给所有 LSM 开了一个后门——这是 LSM 机制最大的风险来源，历史上出现过多次"某条路径没有
   security hook 覆盖"导致的安全绕过 CVE。防御手段：新增系统调用/敏感路径的代码 review checklist
   里必须包含"是否需要 LSM hook"这一项。
2. **"一票否决"被错误地实现成"任意一票通过"**：`lsm_hook_chain.c` 的 `security_file_open()`
   显式要求"遍历完所有 LSM 且全部放行才算放行"，如果误写成"只要有一个放行就放行"，
   会让最严格的那个 LSM 形同虚设——这是实现多 LSM 叠加时最容易犯的逻辑错误。
3. **Hook 内部有副作用但被短路跳过**：如果某个 hook 的实现在"检查"的同时还做了"记录审计日志"
   之类的副作用，一旦被前面的 LSM 短路（返回拒绝提前结束遍历），这个副作用就不会发生——
   这也是为什么本篇要把"访问控制"（8.1/8.2 示例）和"审计"（8.3 示例）设计成两条独立的处理路径：
   审计不应该依赖访问控制的遍历顺序。
4. **策略切换的原子性**：如果 `set_active_policy()` 这种"切换当前生效策略"的操作不是原子的
   （比如切换过程中有另一个线程正在调用 `check_access()`），可能读到一个"正在被替换"的中间状态。
   真实 Kernel 的策略热加载有专门的锁保护，用户态实现如果要支持并发访问也必须加锁。
5. **默认策略的选择错误**：`security/security.c:30` 里 `default_security_ops` 只有 `.name = "default"`，
   其它字段全部是 `NULL`——如果调用方忘记检查字段是否为 `NULL` 就直接调用，会崩溃；真实实现依赖
   `security_fixup_ops()`（[security/security.c:62](../../security/security.c) 附近）把所有
   未实现的字段填上"默认放行"的桩函数，这是"Ops Table 里 NULL 字段的系统性解决方案"。
6. **调试方法**：
   - 用户态：三个示例都在每个决策点打印了详细日志（谁、做什么、结果、原因），可以直接对照预期核对。
   - Kernel：`CONFIG_SECURITY_SELINUX_DEVELOP` 开启 SELinux 的"宽容模式"（denial 记录但不拒绝，
     类似本文档 `permissive_policy` 示例）；`ausearch`/`audit.log` 查看被拒绝的操作；
     `/sys/kernel/security/lsm` 可以查看当前系统实际启用的 LSM 列表及其叠加顺序；
     `dmesg | grep -i selinux` 或 `journalctl -t audit` 定位具体是哪个 LSM 拒绝了操作。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `security_ops` 单指针分派（本仓库真实版本）

真实位置：[security/security.c:29](../../security/security.c)

```c
static struct security_operations *security_ops;

int security_ptrace_access_check(struct task_struct *child, unsigned int mode)
{
	return security_ops->ptrace_access_check(child, mode);   /* <-- 盲调 */
}
```

### Example 2 — `register_security()`：只允许注册一次

真实位置：[security/security.c:117](../../security/security.c) 附近（简化）

```c
int register_security(struct security_operations *ops)
{
	if (security_ops != &default_security_ops)
		return -EAGAIN;   /* 已经有一个 LSM 生效了，拒绝第二次注册 */
	security_ops = ops;
	return 0;
}
```
为什么这条限制存在：本仓库版本的架构决定了"同一时刻只能有一个主 LSM"，这正是后续 Kernel
演进到"hook 链表 + 多 LSM 堆叠"架构的直接动因——`lsm_hook_chain.c` 演示的就是这个演进方向。

### Example 3 — `security_fixup_ops()`：给未实现的 hook 填充默认行为

真实位置：[security/security.c:62](../../security/security.c) 附近

```c
void security_fixup_ops(struct security_operations *ops)
{
	/* 对每一个 NULL 的 hook 字段，填上一个"默认放行"的桩函数指针，
	 * 避免 Core 每次调用前都要判空 —— 这是 02 篇 Ops Table Failure 里
	 * "NULL 字段导致 crash"问题的系统性解法 */
	if (!ops->ptrace_access_check)
		ops->ptrace_access_check = cap_ptrace_access_check;
	/* ... 上百个字段的 fixup ... */
}
```

### Example 4 — VFS 路径上的强制检查点

```c
/* fs/namei.c 风格 */
static int may_open(struct path *path, int acc_mode, int flag)
{
	struct inode *inode = path->dentry->d_inode;
	int error = inode_permission(inode, acc_mode);   /* 传统 DAC 权限位检查 */
	if (error)
		return error;
	return security_inode_permission(inode, acc_mode);   /* <-- LSM 强制检查，DAC 通过了也不代表 MAC 通过 */
}
```
为什么这个例子重要：它展示了"传统权限位（DAC）"和"LSM 强制访问控制（MAC）"是**两条独立的检查**，
必须都通过才能真正打开文件——DI 让这两套完全不同的安全模型可以在同一条代码路径上叠加，互不干扰。

### Example 5 — `cap_capable()`：LSM hook 也可以是"能力检查"的载体

```c
/* security/capability.c 风格：一个最基础的"LSM"实现，只做传统 capability 检查 */
int cap_capable(struct task_struct *tsk, const struct cred *cred,
		 struct user_namespace *ns, int cap, int audit)
{
	for (;;) {
		if (ns == cred->user_ns)
			return cap_raised(cred->cap_effective, cap) ? 0 : -EPERM;
		/* ... 逐级检查 user namespace 层级 ... */
	}
}
```
为什么值得单列：这说明"capability 检查"本身也是通过 LSM hook 机制实现的一个特例
（`cap_capable` 就是 `default_security_ops` 事实上依赖的基础实现），LSM 不仅是"可选的额外强制访问控制"，
连 Kernel 最基础的权限模型也复用了同一套 DI 骨架。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/08-security-hooks-lsm/`](c/08-security-hooks-lsm/)

### 9.1 `lsm_hook_chain.c` — 模拟 LSM 多模块叠加的 hook 链

- 场景：`selinux-style`/`yama-style`/`custom-init-guard` 三个"LSM"同时挂载在 `file_open`/
  `task_kill`/`ptrace_access_check` 三个 hook 点上，验证"一票否决"语义。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o lsm_hook_chain lsm_hook_chain.c`
- 实测输出：
  ```
  === lsm_hook_chain demo ===

  -- 叠加 3 个 LSM（真实 Kernel 里 SELinux/AppArmor/Yama 可以同时生效）--
  [security] LSM 'selinux-style' stacked (now 1 LSM(s) active)
  [security] LSM 'yama-style' stacked (now 2 LSM(s) active)
  [security] LSM 'custom-init-guard' stacked (now 3 LSM(s) active)

  -- 测试 file_open：alice 尝试打开 /etc/shadow（应被 selinux-style 拒绝）--
    [security] LSM 'selinux-style' DENIED file_open('/etc/shadow') for uid=1000 (ret=-13)
    result: DENIED (ret=-13)

  -- 测试 file_open：root 打开 /etc/shadow（应被允许）--
    result: ALLOWED (ret=0)

  -- 测试 file_open：alice 打开自己的普通文件（应被允许，无 LSM 关心此路径）--
    result: ALLOWED (ret=0)

  -- 测试 task_kill：alice 尝试 kill pid=1（应被 custom-init-guard 拒绝）--
    [security] LSM 'custom-init-guard' DENIED kill(pid=1, sig=9) by uid=1000
    result: DENIED (ret=-1)

  -- 测试 task_kill：alice 尝试 kill 普通进程（应被允许）--
    result: ALLOWED (ret=0)

  -- 测试 ptrace：debugger 尝试 attach 到 sensitive-vault（应被 yama-style 拒绝）--
    [security] LSM 'yama-style' DENIED ptrace('debugger' -> 'sensitive-vault')
    result: DENIED (ret=-1)

  === done ===
  ```
- 为什么是 DI：`security_file_open()`/`security_task_kill()`/`security_ptrace_access_check()`
  （Core）完全不知道 `selinux-style`/`yama-style`/`custom-init-guard` 的具体判断逻辑，
  只按注册顺序遍历并盲调；新增第 4 个 LSM 只需要 `security_add_lsm()`，不改动任何 Core 代码。

### 9.2 `policy_injection_acl.c` — 策略注入：可插拔 ACL

- 场景：同一个 `check_access()` 检查点，运行期在 `permissive`/`whitelist`/`strict-deny`
  三种策略之间切换，验证"检查点固定，策略可替换"。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o policy_injection_acl policy_injection_acl.c`
- 实测输出：
  ```
  === policy_injection_acl demo ===

  -- 阶段1：permissive 策略（一切放行）--
  [access-control] switching active policy: (none) -> permissive
    [permissive] ALLOW 'alice' access 'write' on '/data/finance.csv' -> permissive mode, would-be-audited
    [permissive] ALLOW 'alice' access 'read' on '/home/alice/notes.txt' -> permissive mode, would-be-audited
    [permissive] ALLOW 'bob' access 'write' on '/data/finance.csv' -> permissive mode, would-be-audited

  -- 阶段2：whitelist 策略（只允许显式规则）--
  [access-control] switching active policy: permissive -> whitelist
    [whitelist] ALLOW 'alice' access 'write' on '/data/finance.csv' -> matched whitelist rule #0
    [whitelist] DENY 'alice' access 'read' on '/home/alice/notes.txt' -> (no matching whitelist rule)
    [whitelist] DENY 'bob' access 'write' on '/data/finance.csv' -> (no matching whitelist rule)

  -- 阶段3：strict-deny 策略（默认拒绝一切，除了读自己拥有的对象）--
  [access-control] switching active policy: whitelist -> strict-deny
    [strict-deny] DENY 'alice' access 'write' on '/data/finance.csv' -> (strict-deny: everything else is denied by default)
    [strict-deny] ALLOW 'alice' access 'read' on '/home/alice/notes.txt' -> self-owned object read-only exception
    [strict-deny] DENY 'bob' access 'write' on '/data/finance.csv' -> (strict-deny: everything else is denied by default)

  === done ===
  说明：check_access() 从头到尾没有改变一行代码，仅仅通过
        set_active_policy() 切换了三种完全不同的安全决策逻辑。
  ```
  （同样的 `req1`/`req2`/`req3` 三个请求，在三种策略下裁决结果完全不同——`req1` 在
  permissive/whitelist 下被允许，在 strict-deny 下被拒绝，直观展示"策略即插即用"。）
- 为什么是 DI：`check_access()`（Core）只认识 `struct policy` 契约（`decide` 函数指针），
  不知道具体是哪种策略；这是 02 篇 Ops Table / Strategy Pattern 在"安全决策"领域的具体应用，
  说明 LSM hook 提供的"检查点机制"和"检查点上跑的策略"是两个独立可替换的维度。

### 9.3 `audit_hook_pipeline.c` — 审计钩子管道

- 场景：`noise-filter -> formatter -> escalation-detector -> log-sink` 四级管道处理审计事件，
  验证"过滤丢弃"、"格式化加工"、"条件升级"、"落地持久化"逐级生效。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o audit_hook_pipeline audit_hook_pipeline.c`
- 实测输出：
  ```
  === audit_hook_pipeline demo ===

  -- 组装管道：filter -> format -> escalation -> sink --
  [pipeline] stage 'noise-filter' appended (position 0)
  [pipeline] stage 'formatter' appended (position 1)
  [pipeline] stage 'escalation-detector' appended (position 2)
  [pipeline] stage 'log-sink' appended (position 3)

  -- 事件1：alice 写 finance.csv 被拒绝（应触发升级 + 落地）--
  [pipeline] new event: subject='alice' action='write' object='/data/finance.csv' denied=1
    [formatter] rendered: "AUDIT: subj=alice act=write obj=/data/finance.csv result=DENIED"
    [escalation] flagged as high-priority (#1)
    [sink] event #1 persisted: "AUDIT: subj=alice act=write obj=/data/finance.csv result=DENIED [ESCALATED: denied write attempt]"
    [pipeline] event reached end of pipeline: "AUDIT: subj=alice act=write obj=/data/finance.csv result=DENIED [ESCALATED: denied write attempt]"

  -- 事件2：bob 读 /tmp 下文件（应被 filter 阶段丢弃，不到达 sink）--
  [pipeline] new event: subject='bob' action='read' object='/tmp/scratch.txt' denied=0
    [filter] dropping low-value event on /tmp read
    [pipeline] stage 'noise-filter' dropped/consumed the event, stopping pipeline

  -- 事件3：alice 读自己的笔记（正常事件，格式化后落地，不升级）--
  [pipeline] new event: subject='alice' action='read' object='/home/alice/notes.txt' denied=0
    [formatter] rendered: "AUDIT: subj=alice act=read obj=/home/alice/notes.txt result=ALLOWED"
    [sink] event #2 persisted: "AUDIT: subj=alice act=read obj=/home/alice/notes.txt result=ALLOWED"
    [pipeline] event reached end of pipeline: "AUDIT: subj=alice act=read obj=/home/alice/notes.txt result=ALLOWED"

  -- 事件4：mallory 写 /etc/passwd 被拒绝（应触发升级 + 落地）--
  [pipeline] new event: subject='mallory' action='write' object='/etc/passwd' denied=1
    [formatter] rendered: "AUDIT: subj=mallory act=write obj=/etc/passwd result=DENIED"
    [escalation] flagged as high-priority (#2)
    [sink] event #3 persisted: "AUDIT: subj=mallory act=write obj=/etc/passwd result=DENIED [ESCALATED: denied write attempt]"
    [pipeline] event reached end of pipeline: "AUDIT: subj=mallory act=write obj=/etc/passwd result=DENIED [ESCALATED: denied write attempt]"

  === done ===
  high_priority_count=2, events_written(sink)=3
  ```
  （4 个事件里只有 3 个到达 `sink`，事件2 被 `noise-filter` 提前消费，`events_written=3`
  精确验证了这一点；`high_priority_count=2` 精确对应两次"被拒绝的写操作"。）
- 为什么是 DI：`audit_emit_event()`（Core）按注册顺序遍历 `struct audit_stage` 链，
  完全不知道每一级具体做什么处理；这是 Notifier Chain（05 篇）的"流水线变体"——
  区别在于每一级可以修改/丰富事件内容（而不只是只读观察），且可以主动"消费掉"事件终止传播。

一次性编译并运行全部三个示例：`cd c/08-security-hooks-lsm && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Security Hooks (LSM) | Notifier Chain | Ops Table | Driver Registration |
|---|---|---|---|---|
| 调用是否可被绕过 | **否**，核心路径强制调用，是"必经之路" | 否（事件发生就广播），但事件本身可能不发生 | 否，对象存在就必然被使用 | 否，匹配成功后必然 probe |
| 返回值语义 | **裁决类**：0=允许，非0=拒绝，直接影响业务流程 | 多为通知类，返回值通常只影响"是否继续广播" | 每个操作有自己的业务语义返回值 | 0=接管成功，非0=放弃/失败 |
| 多实现之间的关系 | **一票否决**：全部同意才算通过 | 各自独立处理，互不影响结果 | 一对一，不存在"多个同时生效" | 竞争关系，最多一个胜出 |
| 是否分离"检查点"与"策略" | **是**，核心思想（见 9.2 示例） | 不强调这个区分 | 不适用 | 不适用 |
| 何时用 LSM 式 hook | 需要在"不可绕过的关键路径"上做"强制性、可叠加"的决策 | 需要"事件通知"但不需要"决策裁决" | 需要"一整套确定的行为" | 需要"精确匹配一个实现来接管" |

一句话：**LSM Hook 是"强制性的 Notifier Chain"**——它借用了 Notifier Chain 的"链式遍历多个独立实现"
的结构，但把语义从"通知"升级为"裁决"（一票否决），并且明确分离了"在哪里检查"（Core 决定，不可绕过）
和"具体检查什么"（LSM/Policy 决定，可插拔）这两个独立维度。这是本批次教程的最后一种手法，
把 01-08 篇串起来看：**Callback 是原子单元，Ops Table 是打包的 Callback，Context Injection 让
Callback 能访问私有状态，Driver Registration 是"匹配 + Callback + Ops Table"的完整对象化，
Notifier Chain 是"一对多广播"，Initcall 是"注册动作本身的零成本化"，Workqueue 是"Callback 在时间
维度上的异步延伸"，LSM Hook 是"Notifier Chain 在决策场景下的强制化变体"**——这八种手法共同构成了
Kernel DI 工具箱里最常用的核心部分。
