/*
 * lsm_hook_chain.c
 *
 * 场景：模拟 Linux Security Module (LSM) hook 机制。本仓库版本
 * （security/security.c）用的是"单个 security_ops 指针"（只能选一个 LSM），
 * 但真实 Kernel 后来演进为"每个 hook 点一条链，多个 LSM 可以同时叠加"
 * （security_hook_list + hlist，SELinux + AppArmor + Yama 可以同时生效）。
 * 本文件同时演示两种形态，并重点实现"多 LSM 叠加、任意一个拒绝则整体拒绝"
 * 的链式语义（与 05 篇 Notifier Chain 的 STOP 语义高度相似，但语义更严格：
 * 这里是"一票否决"，不是"可选的短路"）。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o lsm_hook_chain lsm_hook_chain.c
 * 运行：
 *   ./lsm_hook_chain
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HOOKS_PER_POINT 8

/* ------------------------------------------------------------------ */
/* 1. 契约：每个 hook 点的函数签名，对应 security_operations 里的字段    */
/*    真实定义参考 include/linux/security.h:1380 struct security_operations */
/* ------------------------------------------------------------------ */

struct cred_sim { char user[32]; int uid; };
struct file_sim { char path[64]; int flags; };
struct task_sim { char name[32]; int pid; };

/* 返回 0 = 允许，负数 = 拒绝（errno 风格，与真实 Kernel 完全一致） */
typedef int (*hook_file_open_fn)(struct cred_sim *cred, struct file_sim *file);
typedef int (*hook_task_kill_fn)(struct cred_sim *cred, struct task_sim *target, int sig);
typedef int (*hook_ptrace_fn)(struct task_sim *tracer, struct task_sim *tracee);

struct lsm_hooks {
	const char *lsm_name;
	hook_file_open_fn  file_open;
	hook_task_kill_fn  task_kill;
	hook_ptrace_fn     ptrace_access_check;
};

/* ------------------------------------------------------------------ */
/* 2. Core：每个 hook 点维护一条链（多个 LSM 可以同时挂钩同一个点）        */
/* ------------------------------------------------------------------ */

struct security_hook_head {
	const struct lsm_hooks *lsms[MAX_HOOKS_PER_POINT];
	int count;
};

static struct security_hook_head g_registered_lsms;

static void security_add_lsm(const struct lsm_hooks *hooks)
{
	g_registered_lsms.lsms[g_registered_lsms.count++] = hooks;
	printf("[security] LSM '%s' stacked (now %d LSM(s) active)\n",
	       hooks->lsm_name, g_registered_lsms.count);
}

/* security_file_open()：Core 侧统一入口，对应真实的 security_file_permission()
 * 风格函数——VFS open() 路径里必经此处，业务代码完全不知道背后有几个 LSM */
static int security_file_open(struct cred_sim *cred, struct file_sim *file)
{
	int i;
	for (i = 0; i < g_registered_lsms.count; i++) {
		const struct lsm_hooks *lsm = g_registered_lsms.lsms[i];
		if (!lsm->file_open)
			continue;
		int ret = lsm->file_open(cred, file);   /* <-- 盲调 */
		if (ret != 0) {
			printf("  [security] LSM '%s' DENIED file_open('%s') for uid=%d (ret=%d)\n",
			       lsm->lsm_name, file->path, cred->uid, ret);
			return ret;   /* 一票否决：任意一个 LSM 拒绝，整体立即拒绝 */
		}
	}
	return 0;   /* 所有 LSM 都放行才算真正放行 */
}

static int security_task_kill(struct cred_sim *cred, struct task_sim *target, int sig)
{
	int i;
	for (i = 0; i < g_registered_lsms.count; i++) {
		const struct lsm_hooks *lsm = g_registered_lsms.lsms[i];
		if (!lsm->task_kill)
			continue;
		int ret = lsm->task_kill(cred, target, sig);
		if (ret != 0) {
			printf("  [security] LSM '%s' DENIED kill(pid=%d, sig=%d) by uid=%d\n",
			       lsm->lsm_name, target->pid, sig, cred->uid);
			return ret;
		}
	}
	return 0;
}

static int security_ptrace_access_check(struct task_sim *tracer, struct task_sim *tracee)
{
	int i;
	for (i = 0; i < g_registered_lsms.count; i++) {
		const struct lsm_hooks *lsm = g_registered_lsms.lsms[i];
		if (!lsm->ptrace_access_check)
			continue;
		int ret = lsm->ptrace_access_check(tracer, tracee);
		if (ret != 0) {
			printf("  [security] LSM '%s' DENIED ptrace('%s' -> '%s')\n",
			       lsm->lsm_name, tracer->name, tracee->name);
			return ret;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* 3. LSM 实现 A：selinux 风格 —— 按路径前缀强制访问控制                */
/* ------------------------------------------------------------------ */

static int selinux_style_file_open(struct cred_sim *cred, struct file_sim *file)
{
	if (strncmp(file->path, "/etc/shadow", 11) == 0 && cred->uid != 0) {
		return -13;   /* -EACCES */
	}
	return 0;
}

static const struct lsm_hooks selinux_style_hooks = {
	.lsm_name = "selinux-style",
	.file_open = selinux_style_file_open,
};

/* ------------------------------------------------------------------ */
/* 4. LSM 实现 B：yama 风格 —— 限制 ptrace（只允许父子进程之间）          */
/* ------------------------------------------------------------------ */

static int yama_style_ptrace_check(struct task_sim *tracer, struct task_sim *tracee)
{
	(void)tracer;
	if (strstr(tracee->name, "sensitive") != NULL)
		return -1;   /* -EPERM，模拟 Yama 的 ptrace_scope 限制 */
	return 0;
}

static const struct lsm_hooks yama_style_hooks = {
	.lsm_name = "yama-style",
	.ptrace_access_check = yama_style_ptrace_check,
};

/* ------------------------------------------------------------------ */
/* 5. LSM 实现 C：一个自定义的"审计强制"模块，禁止向 pid=1 发送信号        */
/* ------------------------------------------------------------------ */

static int custom_task_kill_guard(struct cred_sim *cred, struct task_sim *target, int sig)
{
	(void)cred; (void)sig;
	if (target->pid == 1)
		return -1;   /* -EPERM，模拟保护 init 进程 */
	return 0;
}

static const struct lsm_hooks custom_guard_hooks = {
	.lsm_name = "custom-init-guard",
	.task_kill = custom_task_kill_guard,
};

int main(void)
{
	struct cred_sim root_cred = { .user = "root", .uid = 0 };
	struct cred_sim user_cred = { .user = "alice", .uid = 1000 };
	struct file_sim shadow_file = { .path = "/etc/shadow", .flags = 0 };
	struct file_sim normal_file = { .path = "/home/alice/notes.txt", .flags = 0 };
	struct task_sim init_task = { .name = "init", .pid = 1 };
	struct task_sim normal_task = { .name = "worker", .pid = 4242 };
	struct task_sim sensitive_task = { .name = "sensitive-vault", .pid = 5000 };
	struct task_sim tracer_task = { .name = "debugger", .pid = 999 };

	printf("=== lsm_hook_chain demo ===\n\n");

	printf("-- 叠加 3 个 LSM（真实 Kernel 里 SELinux/AppArmor/Yama 可以同时生效）--\n");
	security_add_lsm(&selinux_style_hooks);
	security_add_lsm(&yama_style_hooks);
	security_add_lsm(&custom_guard_hooks);

	printf("\n-- 测试 file_open：alice 尝试打开 /etc/shadow（应被 selinux-style 拒绝）--\n");
	{
		int ret = security_file_open(&user_cred, &shadow_file);
		printf("  result: %s (ret=%d)\n", ret == 0 ? "ALLOWED" : "DENIED", ret);
	}

	printf("\n-- 测试 file_open：root 打开 /etc/shadow（应被允许）--\n");
	{
		int ret = security_file_open(&root_cred, &shadow_file);
		printf("  result: %s (ret=%d)\n", ret == 0 ? "ALLOWED" : "DENIED", ret);
	}

	printf("\n-- 测试 file_open：alice 打开自己的普通文件（应被允许，无 LSM 关心此路径）--\n");
	{
		int ret = security_file_open(&user_cred, &normal_file);
		printf("  result: %s (ret=%d)\n", ret == 0 ? "ALLOWED" : "DENIED", ret);
	}

	printf("\n-- 测试 task_kill：alice 尝试 kill pid=1（应被 custom-init-guard 拒绝）--\n");
	{
		int ret = security_task_kill(&user_cred, &init_task, 9);
		printf("  result: %s (ret=%d)\n", ret == 0 ? "ALLOWED" : "DENIED", ret);
	}

	printf("\n-- 测试 task_kill：alice 尝试 kill 普通进程（应被允许）--\n");
	{
		int ret = security_task_kill(&user_cred, &normal_task, 15);
		printf("  result: %s (ret=%d)\n", ret == 0 ? "ALLOWED" : "DENIED", ret);
	}

	printf("\n-- 测试 ptrace：debugger 尝试 attach 到 sensitive-vault（应被 yama-style 拒绝）--\n");
	{
		int ret = security_ptrace_access_check(&tracer_task, &sensitive_task);
		printf("  result: %s (ret=%d)\n", ret == 0 ? "ALLOWED" : "DENIED", ret);
	}

	printf("\n=== done ===\n");
	return 0;
}
