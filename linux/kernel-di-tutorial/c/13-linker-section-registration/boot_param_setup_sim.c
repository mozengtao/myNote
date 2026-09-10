/*
 * boot_param_setup_sim.c
 *
 * 场景：模拟 `__setup()`/`early_param()` 通过链接器 section 收集命令行参数
 * 处理器，并按参数名"匹配后才执行"——这与 06 篇 initcall 的"全部无条件执行"
 * 是同一个元模式（链接器 section 自注册）的两种截然不同的运行期语义。
 *
 * 真实定义：
 *   - struct obs_kernel_param  见 include/linux/init.h:225
 *   - __setup_param()/__setup()/early_param() 宏  见 include/linux/init.h:236-251
 *   - __setup_start[]/__setup_end[]（链接脚本符号）
 *     见 include/asm-generic/vmlinux.lds.h:614,616（INIT_SETUP 宏）
 *   - do_early_param()（早期一遍扫描，只执行 early=1 的条目）
 *     见 init/main.c:390
 *   - obsolete_checksetup()（完整命令行解析时的第二遍扫描，跳过已经在
 *     early 阶段执行过的条目）见 init/main.c:157
 *   - 真实参数样例：
 *     early_param("debug", debug_kernel) / early_param("quiet", quiet_kernel)
 *       见 init/main.c:206-207
 *     __setup("reset_devices", set_reset_devices)  见 init/main.c:150
 *     __setup("elevator=", elevator_setup)         见 block/elevator.c:196
 *
 * 与 initcall 的关键区别（06 篇已详细讲 initcall，这里不重复）：
 *   - initcall：section 里的每一项都会被 do_initcalls() 无条件调用一次。
 *   - __setup/early_param：section 里的每一项只有在命令行里出现了对应的
 *     参数名时才会被调用；大多数条目在大多数启动场景下"注册了但从未被调用"。
 *   - early_param 和 __setup 用同一个 section（`.init.setup`），但被扫描的
 *     "遍数"和"时机"不同：early_param 在极早期一遍扫描中执行；__setup
 *     在解析完整命令行时才执行，且会跳过已经作为 early_param 执行过的同名条目。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o boot_param_setup_sim boot_param_setup_sim.c
 * 运行：./boot_param_setup_sim
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 对应真实 include/linux/init.h:225 struct obs_kernel_param */
struct obs_kernel_param {
	const char *str;
	int (*setup_func)(char *arg);
	int early;
};

/* 对应真实 include/linux/init.h:236-251 的 __setup_param/__setup/early_param 宏，
 * 用户态没有自定义链接脚本，但 GNU ld 对合法 C 标识符形式的 section 名
 * 会自动生成 __start_<section>/__stop_<section> 边界符号。
 *
 * 注意：这里显式加上 __attribute__((aligned(sizeof(long))))，这一行不是多余的——
 * 真实 include/linux/init.h:239 的注释原文就是 "Force the alignment so the
 * compiler doesn't space elements of the obs_kernel_param 'array' too far
 * apart in .init.setup."。这里第一次尝试省略这个属性时，实测编译出的二进制里
 * 编译器给每个条目额外插入了 16 字节对齐的 padding（结构体本身只有 24 字节），
 * 导致按 sizeof(struct obs_kernel_param) 做指针步进遍历时跳过一半的真实条目——
 * 这正是本篇 Failure 小节要重点强调的"手工元数据布局"类 bug 的真实复现，
 * 不是凭空想象出来的坑。 */
#define __setup_param(str, fn, is_early)					\
	static struct obs_kernel_param __setup_##fn __attribute__((used))	\
		__attribute__((section("kdi_setup")))				\
		__attribute__((aligned(sizeof(long))))				\
		= { str, fn, is_early }

#define __setup(str, fn)       __setup_param(str, fn, 0)
#define early_param(str, fn)   __setup_param(str, fn, 1)

extern struct obs_kernel_param __start_kdi_setup[], __stop_kdi_setup[];

/* ------------------------------------------------------------------ */
/* 模拟几个真实存在于本仓库的 __setup/early_param 处理函数                */
/* ------------------------------------------------------------------ */

static int g_console_loglevel = 7;
static char g_chosen_elevator[32];
static int g_reset_devices;

/* 对应 init/main.c:194 debug_kernel() */
static int debug_kernel(char *arg)
{
	(void)arg;
	g_console_loglevel = 10;
	printf("    [early_param] debug_kernel: console_loglevel -> %d\n", g_console_loglevel);
	return 0;
}
early_param("debug", debug_kernel);

/* 对应 init/main.c:201 quiet_kernel() */
static int quiet_kernel(char *arg)
{
	(void)arg;
	g_console_loglevel = 4;
	printf("    [early_param] quiet_kernel: console_loglevel -> %d\n", g_console_loglevel);
	return 0;
}
early_param("quiet", quiet_kernel);

/* 对应 init/main.c:144 set_reset_devices() */
static int set_reset_devices(char *arg)
{
	(void)arg;
	g_reset_devices = 1;
	printf("    [__setup] set_reset_devices: reset_devices -> 1\n");
	return 1;
}
__setup("reset_devices", set_reset_devices);

/* 对应 block/elevator.c:186 elevator_setup()：带 "=" 取值的 __setup 参数 */
static int elevator_setup(char *arg)
{
	if (!arg) {
		fprintf(stderr, "    [__setup] elevator_setup: missing value, ignored\n");
		return 1;
	}
	strncpy(g_chosen_elevator, arg, sizeof(g_chosen_elevator) - 1);
	g_chosen_elevator[sizeof(g_chosen_elevator) - 1] = '\0';
	printf("    [__setup] elevator_setup: chosen_elevator -> '%s'\n", g_chosen_elevator);
	return 1;
}
__setup("elevator=", elevator_setup);

/* ------------------------------------------------------------------ */
/* Core：对应 init/main.c 的 do_early_param() 与 obsolete_checksetup()   */
/* ------------------------------------------------------------------ */

/* 判断 token（如 "elevator=deadline"）是否匹配某条 __setup 声明的名字
 * （如 "elevator=" 或 "debug"），返回值指向 token 里 '=' 之后的部分（可能为 NULL） */
static int param_match(const char *token, const char *decl_str, char **val_out)
{
	size_t n = strlen(decl_str);

	if (strncmp(token, decl_str, n) != 0)
		return 0;

	/* decl_str 自身以 '=' 收尾（如真实的 "elevator="），说明这是"带值参数"，
	 * 匹配到前缀之后剩下的部分直接就是值，不需要再找一次 '=' */
	if (n > 0 && decl_str[n - 1] == '=') {
		*val_out = (char *)(token + n);
		return 1;
	}

	if (token[n] == '\0') {
		*val_out = NULL;		/* 无值参数，如 "debug" */
		return 1;
	}
	if (token[n] == '=') {
		*val_out = (char *)(token + n + 1);	/* 带值参数，如 "foo=bar" */
		return 1;
	}
	return 0;	/* 前缀相同但不是完整匹配，如 "debugfoo" 不应匹配 "debug" */
}

/* 对应 init/main.c:390 do_early_param()：只执行 early==1 的条目 */
static void do_early_param(char *tokens[], int ntok)
{
	struct obs_kernel_param *p;
	int i;

	printf("-- parse_early_param()：仅扫描 early_param 声明的条目 --\n");
	for (i = 0; i < ntok; i++) {
		int matched = 0;
		for (p = __start_kdi_setup; p < __stop_kdi_setup; p++) {
			char *val;
			if (!p->early)
				continue;
			if (param_match(tokens[i], p->str, &val)) {
				matched = 1;
				p->setup_func(val);
			}
		}
		(void)matched;
	}
}

/* 对应 init/main.c:157 obsolete_checksetup()：扫描全部条目，但跳过
 * early==1 的（因为已经在上一遍处理过了），未匹配任何声明的参数被忽略 */
static void obsolete_checksetup(char *tokens[], int ntok)
{
	struct obs_kernel_param *p;
	int i;

	printf("\n-- 完整命令行解析（obsolete_checksetup 风格）：扫描非 early 条目 --\n");
	for (i = 0; i < ntok; i++) {
		int matched = 0;
		for (p = __start_kdi_setup; p < __stop_kdi_setup; p++) {
			char *val;
			if (!param_match(tokens[i], p->str, &val))
				continue;
			matched = 1;
			if (p->early) {
				printf("  '%s' already handled by early_param, skip\n", tokens[i]);
			} else {
				p->setup_func(val);
			}
		}
		if (!matched)
			printf("  Unknown boot option '%s' -- ignoring\n", tokens[i]);
	}
}

int main(void)
{
	/* 模拟一条完整的启动命令行，混合 early_param/__setup/未注册参数 */
	char cmdline[] = "reset_devices debug elevator=deadline panic=1 quiet unknown_param=5";
	char *tokens[16];
	int ntok = 0;
	char *save = NULL;
	char *tok;

	printf("=== boot_param_setup_sim demo ===\n\n");
	printf("模拟的启动命令行: \"%s\"\n\n", cmdline);
	printf("已通过链接器 section 'kdi_setup' 自注册的处理器一共 %ld 个：\n",
	       (long)(__stop_kdi_setup - __start_kdi_setup));
	{
		struct obs_kernel_param *p;
		for (p = __start_kdi_setup; p < __stop_kdi_setup; p++)
			printf("  str='%-16s' early=%d\n", p->str, p->early);
	}

	/* 简单按空格切分 token，等价 parse_args() 的分词步骤 */
	tok = strtok_r(cmdline, " ", &save);
	while (tok && ntok < 16) {
		tokens[ntok++] = tok;
		tok = strtok_r(NULL, " ", &save);
	}

	printf("\n");
	do_early_param(tokens, ntok);
	obsolete_checksetup(tokens, ntok);

	printf("\n=== done ===\n");
	printf("最终状态：console_loglevel=%d reset_devices=%d chosen_elevator='%s'\n",
	       g_console_loglevel, g_reset_devices, g_chosen_elevator);
	printf("说明：本文件注册了 4 个处理器，只有命令行里出现了对应名字的那几个被调用；\n"
	       "      'panic=1'/'unknown_param=5' 从未被声明过 __setup，被忽略——这与 initcall\n"
	       "      的\"section 里有什么就无条件全部执行\"完全不同：这里是\"section 里有什么\n"
	       "      只是候选，命令行决定谁被执行\"。\n");
	return 0;
}
