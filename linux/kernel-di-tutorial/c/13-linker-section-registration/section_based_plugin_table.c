/*
 * section_based_plugin_table.c
 *
 * 场景：前两个示例（boot_param_setup_sim.c 用字符串前缀匹配，
 * pci_fixup_table_sim.c 用数值 vendor/device 匹配）分别复刻了两个具体的
 * 真实内核子系统。本文件把两者背后共享的"骨架"抽出来，做成一个
 * 与具体业务类型无关的最小工具库：
 *
 *     DECLARE_SECTION_TABLE(section, type)   声明某个 section 的边界符号
 *     SECTION_REGISTER(section, uniq, type, ...)  把一个静态常量对象塞进该 section
 *     SECTION_TABLE_FOREACH(section, type, it)    遍历该 section 里的所有对象
 *
 * 这三个宏本身完全不知道"对象里装的是命令处理器还是 PCI 修补规则还是
 * initcall 函数指针"——这正是本篇 Why 小节说的"链接器 section 自注册"是一个
 * 可以脱离 initcall/__setup/PCI fixup 这些具体场景、独立存在的通用元模式。
 * 本文件用两个完全不相关的领域（CLI 命令表、错误码描述表）复用同一套宏，
 * 证明这套骨架的领域无关性。
 *
 * 对应真实内核的位置：这套"边界符号 + 遍历"骨架在 06/13 两篇提到的所有真实
 * 机制里反复出现——initcall 的 __initcall_start/__initcall_end
 * （include/linux/init.h、init/main.c）、__setup 的 __setup_start/__setup_end
 * （include/linux/init.h:254、init/main.c:390）、tracepoint 的
 * __start___tracepoints_ptrs/__stop___tracepoints_ptrs
 * （kernel/tracepoint.c:29-30，见 14-tracepoint.md）、PCI fixup 的
 * __start_pci_fixups_early 等（drivers/pci/quirks.c:2922）——每一处都是
 * "同一个骨架 + 不同的元素类型 + 不同的匹配/遍历策略"的组合。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o section_based_plugin_table section_based_plugin_table.c
 * 运行：./section_based_plugin_table
 */

#include <stdio.h>
#include <string.h>

/* ==================================================================== */
/* 通用工具库：与业务类型无关，只负责"把对象放进 section"和"给出边界"      */
/* ==================================================================== */

/* 声明某个 section 的起止边界符号（依赖 GNU ld 对合法 C 标识符 section
 * 名字自动生成 __start_<section>/__stop_<section>，无需自定义链接脚本） */
#define DECLARE_SECTION_TABLE(sec, type)		\
	extern type __start_##sec[];			\
	extern type __stop_##sec[]

/* 把一个 const 对象放进指定 section；uniq 要求在同一个 section 内唯一，
 * 强制 aligned(sizeof(long)) 避免 padding 缝隙（见另外两个示例踩过的坑） */
#define SECTION_REGISTER(sec, uniq, type, ...)				\
	static const type __entry_##sec##_##uniq __attribute__((used))\
		__attribute__((__section__(#sec)))			\
		__attribute__((aligned(sizeof(long))))			\
		= { __VA_ARGS__ }

/* 遍历宏：it 是循环变量名字（一个 `const type *` 指针），领域代码在循环体里
 * 自己决定"匹配"逻辑是什么——这正是本文件要证明的"机制与策略分离"在工具库
 * 这个更底层的抽象层次上的体现 */
#define SECTION_TABLE_FOREACH(sec, type, it)				\
	for (const type *it = __start_##sec; it < __stop_##sec; it++)

#define SECTION_TABLE_COUNT(sec, type)	\
	((long)(__stop_##sec - __start_##sec))

/* ==================================================================== */
/* 领域 A：CLI 命令表 —— 按名字字符串匹配（结构类似 __setup）              */
/* ==================================================================== */

struct cli_command {
	const char *name;
	const char *help;
	int (*handler)(const char *arg);
};

DECLARE_SECTION_TABLE(cli_cmd_tab, struct cli_command);

#define CLI_COMMAND(uniq, cmdname, helpstr, fn)			\
	SECTION_REGISTER(cli_cmd_tab, uniq, struct cli_command, cmdname, helpstr, fn)

static int cmd_help(const char *arg)
{
	(void)arg;
	printf("      [cmd:help] listing all registered commands:\n");
	SECTION_TABLE_FOREACH(cli_cmd_tab, struct cli_command, it)
		printf("        %-10s %s\n", it->name, it->help);
	return 0;
}
CLI_COMMAND(help, "help", "list all available commands", cmd_help);

static int cmd_version(const char *arg)
{
	(void)arg;
	printf("      [cmd:version] section_based_plugin_table demo v1.0\n");
	return 0;
}
CLI_COMMAND(version, "version", "print demo version", cmd_version);

static int cmd_echo(const char *arg)
{
	printf("      [cmd:echo] %s\n", arg ? arg : "(nothing to echo)");
	return 0;
}
CLI_COMMAND(echo, "echo", "echo back the argument", cmd_echo);

/* Core（领域相关的匹配逻辑，与工具库本身无关）：按名字线性查找并调用 */
static int cli_dispatch(const char *name, const char *arg)
{
	SECTION_TABLE_FOREACH(cli_cmd_tab, struct cli_command, it) {
		if (strcmp(it->name, name) == 0)
			return it->handler(arg);
	}
	printf("    unknown command '%s'\n", name);
	return -1;
}

/* ==================================================================== */
/* 领域 B：错误码描述表 —— 按数值精确匹配（结构类似 PCI fixup 的 vendor/device） */
/* ==================================================================== */

struct errno_desc {
	int code;
	const char *desc;
};

DECLARE_SECTION_TABLE(errno_tab, struct errno_desc);

#define ERRNO_DESC(uniq, codeval, descstr)	\
	SECTION_REGISTER(errno_tab, uniq, struct errno_desc, codeval, descstr)

ERRNO_DESC(enoent, -2,  "No such file or directory");
ERRNO_DESC(einval, -22, "Invalid argument");
ERRNO_DESC(enomem, -12, "Cannot allocate memory");
ERRNO_DESC(eagain, -11, "Resource temporarily unavailable");

/* Core：按数值查找，找不到给出通用兜底描述 */
static const char *errno_lookup(int code)
{
	SECTION_TABLE_FOREACH(errno_tab, struct errno_desc, it) {
		if (it->code == code)
			return it->desc;
	}
	return "(no description registered for this code)";
}

int main(void)
{
	int i;
	int codes_to_check[5] = { -2, -22, -12, -11, -99 };

	printf("=== section_based_plugin_table demo ===\n\n");

	printf("cli_cmd_tab 里注册的命令数: %ld\n", SECTION_TABLE_COUNT(cli_cmd_tab, struct cli_command));
	printf("errno_tab   里注册的错误码数: %ld\n\n", SECTION_TABLE_COUNT(errno_tab, struct errno_desc));

	printf("-- 领域 A：CLI 命令表（字符串匹配） --\n");
	printf("  执行 'help':\n");
	cli_dispatch("help", NULL);
	printf("  执行 'version':\n");
	cli_dispatch("version", NULL);
	printf("  执行 'echo hello linker sections':\n");
	cli_dispatch("echo", "hello linker sections");
	printf("  执行 'foobar'（未注册的命令）:\n");
	cli_dispatch("foobar", NULL);

	printf("\n-- 领域 B：错误码描述表（数值精确匹配） --\n");
	for (i = 0; i < 5; i++)
		printf("  errno_lookup(%3d) -> %s\n", codes_to_check[i], errno_lookup(codes_to_check[i]));

	printf("\n=== done ===\n");
	printf("说明：DECLARE_SECTION_TABLE/SECTION_REGISTER/SECTION_TABLE_FOREACH\n"
	       "      这三个宏被原样复用了两次，分别撑起了一个字符串匹配的命令表和\n"
	       "      一个数值匹配的错误码表——工具库本身只关心\"怎么把对象放进 section、\n"
	       "      怎么给出遍历边界\"，完全不关心对象内部字段是什么、怎么匹配；\n"
	       "      这就是链接器 section 自注册作为通用元模式（而不是 initcall 专属技巧）\n"
	       "      的直接证据。\n");
	return 0;
}
