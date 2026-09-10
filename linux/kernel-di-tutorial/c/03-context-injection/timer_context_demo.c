/*
 * timer_context_demo.c
 *
 * 场景：模拟 Kernel 的 struct timer_list，演示"回调函数如何携带上下文"
 * 这一具体问题在 Kernel API 演进中的两代方案：
 *
 *   旧式（本仓库版本，kernel/timer.c 时代）：
 *       void (*function)(unsigned long data);   // 只能传一个 unsigned long
 *       setup_timer(&t, fn, (unsigned long)ptr); // 把指针硬转成整数传递
 *
 *   新式（4.15+ 之后的 timer_setup()/container_of 风格，这里一并演示）：
 *       void (*function)(struct timer_list *t);
 *       struct my_dev { struct timer_list timer; ... };
 *       -> 回调里用 container_of(t, struct my_dev, timer) 取回上下文
 *
 * 两种方式都是 "Context Injection"：都是在注册回调的同时，把"该回调
 * 需要用到的私有数据"一并注入进去，只是载体不同（整数 vs 内嵌结构体）。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o timer_context_demo timer_context_demo.c
 * 运行：
 *   ./timer_context_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

/* ------------------------------------------------------------------ */
/* 1a. 旧式契约：function(unsigned long data)                          */
/* ------------------------------------------------------------------ */

struct legacy_timer_list {
	long expires;                          /* 模拟 jiffies 过期时间 */
	void (*function)(unsigned long data);  /* <-- 回调 */
	unsigned long data;                    /* <-- Context Injection：唯一的上下文载体 */
	struct legacy_timer_list *next;
};

/* ------------------------------------------------------------------ */
/* 1b. 新式契约：function(struct timer_list *)，配合 container_of      */
/* ------------------------------------------------------------------ */

struct timer_list {
	long expires;
	void (*function)(struct timer_list *t);  /* <-- 回调，签名不再需要 data 参数 */
	struct timer_list *next;
};

#define timer_setup(t, fn) do { (t)->function = (fn); (t)->next = NULL; } while (0)

/* ------------------------------------------------------------------ */
/* 2. 模拟"时钟中断"驱动的定时器核心：run_timers()                     */
/* ------------------------------------------------------------------ */

static struct legacy_timer_list *g_legacy_list;
static struct timer_list *g_modern_list;
static long g_now;   /* 模拟 jiffies */

static void legacy_add_timer(struct legacy_timer_list *t)
{
	t->next = g_legacy_list;
	g_legacy_list = t;
}

static void modern_add_timer(struct timer_list *t)
{
	t->next = g_modern_list;
	g_modern_list = t;
}

/* 对应 kernel/timer.c __run_timers()：找出所有到期定时器，逐一调用 */
static void legacy_run_timers(void)
{
	struct legacy_timer_list **pp = &g_legacy_list;
	while (*pp) {
		struct legacy_timer_list *t = *pp;
		if (t->expires <= g_now) {
			*pp = t->next;   /* 先从链表摘除，再调用（避免回调里重新添加自己造成的重入问题） */
			t->function(t->data);   /* <-- 盲调，携带的唯一上下文是 data */
		} else {
			pp = &t->next;
		}
	}
}

static void modern_run_timers(void)
{
	struct timer_list **pp = &g_modern_list;
	while (*pp) {
		struct timer_list *t = *pp;
		if (t->expires <= g_now) {
			*pp = t->next;
			t->function(t);   /* <-- 盲调，把整个 timer_list 指针传回去 */
		} else {
			pp = &t->next;
		}
	}
}

/* ------------------------------------------------------------------ */
/* 3. 旧式用法：把指针塞进 unsigned long                                */
/* ------------------------------------------------------------------ */

struct watchdog_device {
	char name[32];
	int  timeout_count;
};

static void legacy_watchdog_fn(unsigned long data)
{
	/* 必须手动把整数转回指针 —— 类型不安全，编译器无法帮你检查 */
	struct watchdog_device *wd = (struct watchdog_device *)data;
	wd->timeout_count++;
	printf("  [legacy] watchdog '%s' fired, timeout_count=%d (data=0x%lx)\n",
	       wd->name, wd->timeout_count, data);
}

/* ------------------------------------------------------------------ */
/* 4. 新式用法：timer_list 内嵌在业务结构体里，用 container_of 找回      */
/* ------------------------------------------------------------------ */

struct modern_watchdog {
	char name[32];
	int  timeout_count;
	struct timer_list timer;   /* <-- 内嵌，天然把"定时器身份"和"业务对象"绑在一起 */
};

static void modern_watchdog_fn(struct timer_list *t)
{
	/* 类型安全：container_of 在编译期就检查 member 类型是否匹配 */
	struct modern_watchdog *wd = container_of(t, struct modern_watchdog, timer);
	wd->timeout_count++;
	printf("  [modern] watchdog '%s' fired, timeout_count=%d (timer@%p wd@%p)\n",
	       wd->name, wd->timeout_count, (void *)t, (void *)wd);
}

/* ------------------------------------------------------------------ */
/* 5. delayed_work 的简化模拟：定时器到期后，把工作丢进"队列"而不是立即跑 */
/* ------------------------------------------------------------------ */

struct delayed_flush {
	char target[32];
	int  fired;
	struct timer_list timer;
};

static void delayed_flush_fire(struct timer_list *t)
{
	struct delayed_flush *df = container_of(t, struct delayed_flush, timer);
	df->fired = 1;
	printf("  [delayed_work] timer expired -> would schedule_work() to flush '%s' now\n",
	       df->target);
}

int main(void)
{
	struct watchdog_device wd_legacy = { .name = "eth0-wd", .timeout_count = 0 };
	struct modern_watchdog wd_modern = { .name = "eth1-wd", .timeout_count = 0 };
	struct delayed_flush df = { .target = "log-buffer", .fired = 0 };
	struct legacy_timer_list lt;
	int i;

	printf("=== timer_context_demo ===\n\n");

	/* -- 旧式：data 携带上下文 -- */
	lt.expires = 5;
	lt.function = legacy_watchdog_fn;
	lt.data = (unsigned long)&wd_legacy;   /* <-- 把指针"塞"进一个整数字段 */
	legacy_add_timer(&lt);

	/* -- 新式：内嵌 timer_list，用 container_of 找回 -- */
	wd_modern.timer.expires = 3;
	timer_setup(&wd_modern.timer, modern_watchdog_fn);
	modern_add_timer(&wd_modern.timer);

	df.timer.expires = 7;
	timer_setup(&df.timer, delayed_flush_fire);
	modern_add_timer(&df.timer);

	printf("-- 模拟时钟推进，每次 tick 检查所有定时器 --\n");
	for (i = 1; i <= 8; i++) {
		g_now = i;
		printf("tick %ld:\n", g_now);
		legacy_run_timers();
		modern_run_timers();
	}

	printf("\n=== done ===\n");
	printf("legacy watchdog fired %d time(s)\n", wd_legacy.timeout_count);
	printf("modern watchdog fired %d time(s)\n", wd_modern.timeout_count);
	printf("delayed_flush fired = %d\n", df.fired);
	return 0;
}
