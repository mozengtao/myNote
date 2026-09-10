/*
 * self_registering_tests.c
 *
 * 场景：一个类似 kselftest/KUnit 的"自注册测试框架"——每个测试用例
 * 用 TEST_CASE(name) { ... } 定义后，*不需要*手动把它加到任何列表里，
 * 框架在 main() 里通过遍历一个自定义 linker section 就能找到全部测试
 * 并逐一运行、汇总 TAP（Test Anything Protocol）风格的结果。
 *
 * 这与 06-initcall 的核心技巧完全一致：把"注册"这个动作从"运行期显式
 * 调用 register_xxx()"下放给"链接器在链接阶段自动收集所有落在同一个
 * section 里的符号"——对测试框架作者而言，新增一个测试用例的成本
 * 降到了"写一个 TEST_CASE 宏调用"，不需要碰任何"测试列表"文件。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o self_registering_tests self_registering_tests.c
 * 运行：
 *   ./self_registering_tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 1. 测试框架内部状态（每个测试用例执行时使用）                        */
/* ------------------------------------------------------------------ */

struct test_ctx {
	int assertions_run;
	int assertions_failed;
	char first_failure[128];
};

typedef void (*test_fn_t)(struct test_ctx *ctx);

struct test_entry {
	const char *name;
	test_fn_t fn;
};

/* ------------------------------------------------------------------ */
/* 2. 断言宏：失败时记录但不 abort，让测试框架能收集"全部"失败点          */
/* ------------------------------------------------------------------ */

/* 注意：actual/expected 只能各求值一次 —— 如果直接在宏体里重复展开
 * (actual)/(expected)，当传入的是像 stack_pop(&s) 这种带副作用的表达式时，
 * 会被求值两次，产生极难排查的 bug（经典的宏参数多次求值陷阱）。
 * 这里用局部变量各求值一次，然后只使用局部变量。 */
#define EXPECT_EQ(ctx, actual, expected) do {                                   \
	long __actual = (long)(actual);                                              \
	long __expected = (long)(expected);                                          \
	(ctx)->assertions_run++;                                                     \
	if (__actual != __expected) {                                                \
		(ctx)->assertions_failed++;                                              \
		if ((ctx)->first_failure[0] == '\0')                                     \
			snprintf((ctx)->first_failure, sizeof((ctx)->first_failure),        \
				 "%s:%d: expected %ld got %ld",                                  \
				 __FILE__, __LINE__, __expected, __actual);                      \
	}                                                                             \
} while (0)

#define EXPECT_TRUE(ctx, cond) EXPECT_EQ(ctx, (cond) ? 1 : 0, 1)

/* ------------------------------------------------------------------ */
/* 3. 自注册机制：TEST_CASE 宏 —— 与 DEFINE_INITCALL 是同一种技巧        */
/* ------------------------------------------------------------------ */

#define TEST_CASE(tname)                                                        \
	static void test_##tname(struct test_ctx *ctx);                             \
	static struct test_entry __test_entry_##tname                               \
		__attribute__((used, section("kdi_tests"))) = {                         \
			.name = #tname, .fn = test_##tname,                                  \
		};                                                                        \
	static void test_##tname(struct test_ctx *ctx)

/* 链接器自动生成的 section 边界 */
extern struct test_entry __start_kdi_tests[];
extern struct test_entry __stop_kdi_tests[];

/* ------------------------------------------------------------------ */
/* 4. 被测代码：一个极简的"栈"实现（业务代码，与测试框架无关）           */
/* ------------------------------------------------------------------ */

struct int_stack {
	int data[16];
	int top;
};

static void stack_init(struct int_stack *s) { s->top = -1; }
static int  stack_push(struct int_stack *s, int v)
{
	if (s->top >= 15)
		return -1;
	s->data[++s->top] = v;
	return 0;
}
static int stack_pop(struct int_stack *s)
{
	if (s->top < 0)
		return -1;   /* 防御性检查，避免负下标访问，也消除编译器的越界告警 */
	return s->data[s->top--];
}
static int stack_empty(struct int_stack *s) { return s->top < 0; }

/* ------------------------------------------------------------------ */
/* 5. 测试用例们：每个都是"自包含"的，framework 在编译期完全不知道       */
/*    它们的存在，全靠链接期 section 收集                                */
/* ------------------------------------------------------------------ */

TEST_CASE(stack_push_pop_basic)
{
	struct int_stack s;
	stack_init(&s);
	stack_push(&s, 42);
	EXPECT_EQ(ctx, stack_pop(&s), 42);
	EXPECT_TRUE(ctx, stack_empty(&s));
}

TEST_CASE(stack_lifo_order)
{
	struct int_stack s;
	stack_init(&s);
	stack_push(&s, 1);
	stack_push(&s, 2);
	stack_push(&s, 3);
	EXPECT_EQ(ctx, stack_pop(&s), 3);
	EXPECT_EQ(ctx, stack_pop(&s), 2);
	EXPECT_EQ(ctx, stack_pop(&s), 1);
}

TEST_CASE(stack_empty_after_init)
{
	struct int_stack s;
	stack_init(&s);
	EXPECT_TRUE(ctx, stack_empty(&s));
}

/* 一个故意失败的测试用例，用来验证框架能正确报告失败而不是崩溃退出 */
TEST_CASE(stack_intentional_failure_demo)
{
	struct int_stack s;
	stack_init(&s);
	stack_push(&s, 100);
	/* 故意断言错误的期望值，演示失败报告路径 */
	EXPECT_EQ(ctx, stack_pop(&s), 999);
}

TEST_CASE(stack_overflow_guard)
{
	struct int_stack s;
	int i, ret = 0;
	stack_init(&s);
	for (i = 0; i < 20; i++)
		ret = stack_push(&s, i);
	EXPECT_EQ(ctx, ret, -1);   /* 第 17 次 push 应该失败 */
}

/* ------------------------------------------------------------------ */
/* 6. 测试运行器：main()，遍历 section，逐一运行，输出 TAP 风格结果      */
/* ------------------------------------------------------------------ */

int main(void)
{
	struct test_entry *e;
	int total = 0, passed = 0, failed = 0;

	printf("=== self_registering_tests demo ===\n\n");
	printf("TAP version 13\n");
	printf("1..%ld\n", (long)(__stop_kdi_tests - __start_kdi_tests));

	for (e = __start_kdi_tests; e < __stop_kdi_tests; e++) {
		struct test_ctx ctx;
		memset(&ctx, 0, sizeof(ctx));
		total++;

		e->fn(&ctx);   /* <-- 盲调：runner 完全不知道每个测试内部做什么 */

		if (ctx.assertions_failed == 0) {
			printf("ok %d - %s (%d assertion(s))\n", total, e->name, ctx.assertions_run);
			passed++;
		} else {
			printf("not ok %d - %s (%d/%d assertion(s) failed)\n",
			       total, e->name, ctx.assertions_failed, ctx.assertions_run);
			printf("  # %s\n", ctx.first_failure);
			failed++;
		}
	}

	printf("\n=== summary: %d total, %d passed, %d failed ===\n", total, passed, failed);
	return failed > 0 ? 1 : 0;
}
