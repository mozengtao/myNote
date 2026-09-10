/*
 * command_dispatcher.c
 *
 * 场景：一个命令行/RPC 风格的"命令分发器”（command dispatcher）。
 * 核心（dispatcher_execute）只知道"命令名字符串 -> 函数指针"这张表，
 * 具体每个命令怎么实现，dispatcher 完全不关心。
 *
 * 这与 Kernel 里 syscall table（sys_call_table[nr] = sys_xxx）、
 * ioctl cmd 分发（file->f_op->unlocked_ioctl 内部按 cmd 分发）本质相同：
 *
 *   dispatcher_execute("copy", argv)
 *        │
 *        │ 查表：name -> cmd_entry
 *        ▼
 *   cmd_entry->handler(argc, argv, cmd_entry->ctx)   <-- 回调
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o command_dispatcher command_dispatcher.c
 * 运行：
 *   ./command_dispatcher
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CMDS   32
#define MAX_ARGS   8
#define MAX_NAME   32

/* ---------------------------------------------------------- */
/* 契约：dispatcher 只认这个签名                                */
/* ---------------------------------------------------------- */

/* 返回值：0 成功，非 0 失败（类似 Kernel 里的 errno 风格返回值） */
typedef int (*cmd_handler_fn)(int argc, char **argv, void *ctx);

struct cmd_entry {
	char name[MAX_NAME];
	char help[64];
	cmd_handler_fn handler;   /* <-- DI 注入点 */
	void *ctx;                /* Context Injection：命令私有状态 */
};

struct dispatcher {
	struct cmd_entry table[MAX_CMDS];
	int count;
};

static struct dispatcher g_disp = { .count = 0 };

/* register_command()：注册接口，等价于 sys_call_table 填表 / register_chrdev() */
static int register_command(const char *name, const char *help,
			     cmd_handler_fn fn, void *ctx)
{
	int i;

	/* 防止重复注册同名命令，类似 Kernel 里防止两个驱动争抢同一个 major number */
	for (i = 0; i < g_disp.count; i++) {
		if (strcmp(g_disp.table[i].name, name) == 0) {
			fprintf(stderr, "[dispatcher] command '%s' already registered\n", name);
			return -1;
		}
	}

	if (g_disp.count >= MAX_CMDS)
		return -1;
	if (!fn) {
		fprintf(stderr, "[dispatcher] refuse NULL handler for '%s'\n", name);
		return -1;
	}

	struct cmd_entry *e = &g_disp.table[g_disp.count++];
	strncpy(e->name, name, MAX_NAME - 1);
	strncpy(e->help, help, sizeof(e->help) - 1);
	e->handler = fn;
	e->ctx = ctx;

	printf("[dispatcher] registered command '%s' -> %p\n", name, (void *)fn);
	return 0;
}

/* dispatcher_execute()：核心分发逻辑，唯一"知道"的是这张表 */
static int dispatcher_execute(const char *line)
{
	char buf[256];
	char *argv[MAX_ARGS];
	int argc = 0;
	char *tok;
	int i;

	strncpy(buf, line, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	tok = strtok(buf, " \t");
	while (tok && argc < MAX_ARGS) {
		argv[argc++] = tok;
		tok = strtok(NULL, " \t");
	}
	if (argc == 0)
		return 0;

	for (i = 0; i < g_disp.count; i++) {
		if (strcmp(g_disp.table[i].name, argv[0]) == 0) {
			struct cmd_entry *e = &g_disp.table[i];
			printf("[dispatcher] dispatch '%s' -> handler=%p\n", argv[0], (void *)e->handler);
			/* <-- 运行期盲调：dispatcher 不知道 handler 内部逻辑 */
			return e->handler(argc, argv, e->ctx);
		}
	}

	fprintf(stderr, "[dispatcher] unknown command '%s'\n", argv[0]);
	return -1;
}

static void dispatcher_list_commands(void)
{
	int i;
	printf("[dispatcher] %d command(s) registered:\n", g_disp.count);
	for (i = 0; i < g_disp.count; i++)
		printf("    %-10s : %s\n", g_disp.table[i].name, g_disp.table[i].help);
}

/* ---------------------------------------------------------- */
/* 具体命令实现 A：echo                                         */
/* ---------------------------------------------------------- */

static int cmd_echo(int argc, char **argv, void *ctx)
{
	int i;
	(void)ctx;
	printf("  [echo] ");
	for (i = 1; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");
	return 0;
}

/* ---------------------------------------------------------- */
/* 具体命令实现 B：sum（携带 context：累计历史总和）              */
/* ---------------------------------------------------------- */

struct sum_ctx {
	long total;
	int calls;
};

static int cmd_sum(int argc, char **argv, void *ctx)
{
	struct sum_ctx *c = ctx;
	long local = 0;
	int i;

	for (i = 1; i < argc; i++)
		local += atol(argv[i]);

	c->total += local;
	c->calls++;
	printf("  [sum] this call = %ld, running total across %d call(s) = %ld\n",
	       local, c->calls, c->total);
	return 0;
}

/* ---------------------------------------------------------- */
/* 具体命令实现 C：fail（用来演示错误返回值的传播）                */
/* ---------------------------------------------------------- */

static int cmd_fail(int argc, char **argv, void *ctx)
{
	(void)argc; (void)argv; (void)ctx;
	fprintf(stderr, "  [fail] intentionally returning error\n");
	return -1;
}

int main(void)
{
	struct sum_ctx sctx = { .total = 0, .calls = 0 };
	const char *script[] = {
		"echo hello from dispatcher",
		"sum 1 2 3",
		"sum 10 20",
		"fail",
		"nosuchcmd foo bar",
	};
	int i, ret;

	printf("=== command_dispatcher demo ===\n\n");

	register_command("echo", "print arguments back", cmd_echo, NULL);
	register_command("sum",  "sum integer arguments, keep running total", cmd_sum, &sctx);
	register_command("fail", "always fail, for error-path demo", cmd_fail, NULL);

	/* 重复注册应该被拒绝 */
	register_command("echo", "duplicate", cmd_echo, NULL);

	printf("\n");
	dispatcher_list_commands();

	printf("\n-- 执行脚本 --\n");
	for (i = 0; i < (int)(sizeof(script) / sizeof(script[0])); i++) {
		printf("> %s\n", script[i]);
		ret = dispatcher_execute(script[i]);
		printf("  (exit code = %d)\n", ret);
	}

	printf("\n=== done, sum called %d time(s), final total = %ld ===\n",
	       sctx.calls, sctx.total);
	return 0;
}
