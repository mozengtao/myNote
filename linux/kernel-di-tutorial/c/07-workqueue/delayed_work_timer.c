/*
 * delayed_work_timer.c
 *
 * 场景：模拟 Kernel 的 struct delayed_work —— "定时器到期后才把 work
 * 真正塞进 workqueue"，而不是立即执行。对应真实实现
 * kernel/workqueue.c delayed_work_timer_fn() -> __queue_work()。
 *
 * 同时演示 cancel_delayed_work()：在定时器还没触发前取消，work 永远
 * 不会被执行 —— 这是异步注入场景下必须处理的典型竞态。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -o delayed_work_timer delayed_work_timer.c
 * 运行：
 *   ./delayed_work_timer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	work_func_t func;
	struct work_struct *next;
};

struct workqueue_struct {
	pthread_mutex_t lock;
	pthread_cond_t  cond;
	struct work_struct *head, *tail;
	pthread_t worker;
	int stop;
};

/* ------------------------------------------------------------------ */
/* 极简单 worker workqueue（复用 03/07-01 的思路，这里精简到最小）        */
/* ------------------------------------------------------------------ */

static void queue_work(struct workqueue_struct *wq, struct work_struct *w)
{
	pthread_mutex_lock(&wq->lock);
	w->next = NULL;
	if (wq->tail) wq->tail->next = w; else wq->head = w;
	wq->tail = w;
	pthread_cond_signal(&wq->cond);
	pthread_mutex_unlock(&wq->lock);
}

static void *wq_worker(void *arg)
{
	struct workqueue_struct *wq = arg;
	for (;;) {
		struct work_struct *w;
		pthread_mutex_lock(&wq->lock);
		while (!wq->head && !wq->stop)
			pthread_cond_wait(&wq->cond, &wq->lock);
		if (!wq->head && wq->stop) { pthread_mutex_unlock(&wq->lock); break; }
		w = wq->head; wq->head = w->next;
		if (!wq->head) wq->tail = NULL;
		pthread_mutex_unlock(&wq->lock);
		w->func(w);
	}
	return NULL;
}

static struct workqueue_struct *wq_create(void)
{
	struct workqueue_struct *wq = calloc(1, sizeof(*wq));
	pthread_mutex_init(&wq->lock, NULL);
	pthread_cond_init(&wq->cond, NULL);
	pthread_create(&wq->worker, NULL, wq_worker, wq);
	return wq;
}

static void wq_destroy(struct workqueue_struct *wq)
{
	pthread_mutex_lock(&wq->lock);
	wq->stop = 1;
	pthread_cond_signal(&wq->cond);
	pthread_mutex_unlock(&wq->lock);
	pthread_join(wq->worker, NULL);
	free(wq);
}

/* ------------------------------------------------------------------ */
/* 1. delayed_work：内嵌 work_struct + 定时器线程                       */
/* ------------------------------------------------------------------ */

struct delayed_work {
	struct work_struct work;
	struct workqueue_struct *wq;   /* 到期后要投递到哪个 workqueue */
	pthread_t timer_thread;
	int delay_ms;
	volatile int cancelled;
	volatile int fired;
};

struct timer_arg {
	struct delayed_work *dwork;
};

/* delayed_work_timer_fn()：对应真实 Kernel 的同名函数，
 * 定时器到期后才把 work 塞进 workqueue，而不是立刻执行 */
static void *delayed_work_timer_thread(void *arg)
{
	struct timer_arg *ta = arg;
	struct delayed_work *dw = ta->dwork;
	free(ta);

	usleep((useconds_t)dw->delay_ms * 1000);

	if (dw->cancelled) {
		printf("  [timer] delayed_work was cancelled before firing, skipping queue_work()\n");
		return NULL;
	}

	dw->fired = 1;
	printf("  [timer] delay of %dms elapsed, now queue_work() into wq\n", dw->delay_ms);
	queue_work(dw->wq, &dw->work);
	return NULL;
}

/* schedule_delayed_work()：注册接口 —— 不是立即注入 work，而是注入一个"未来会注入 work"的承诺 */
static void schedule_delayed_work(struct workqueue_struct *wq, struct delayed_work *dw, int delay_ms)
{
	struct timer_arg *ta = malloc(sizeof(*ta));
	dw->wq = wq;
	dw->delay_ms = delay_ms;
	dw->cancelled = 0;
	dw->fired = 0;
	ta->dwork = dw;
	pthread_create(&dw->timer_thread, NULL, delayed_work_timer_thread, ta);
	printf("[schedule_delayed_work] armed timer for %dms\n", delay_ms);
}

/* cancel_delayed_work()：在定时器触发前尝试取消 */
static int cancel_delayed_work(struct delayed_work *dw)
{
	if (dw->fired) {
		printf("[cancel_delayed_work] too late, work already queued/executing\n");
		return 0;
	}
	dw->cancelled = 1;
	printf("[cancel_delayed_work] marked cancelled (race: timer thread may still be sleeping)\n");
	return 1;
}

/* ------------------------------------------------------------------ */
/* 业务对象：日志刷盘（成功触发）与缓存过期（会被取消）                   */
/* ------------------------------------------------------------------ */

struct flush_ctx {
	char target[32];
	struct delayed_work dwork;
};

static void flush_work_fn(struct work_struct *w)
{
	struct flush_ctx *ctx = container_of(w, struct flush_ctx, dwork.work);
	printf("    [flush_work_fn] flushing '%s' to disk now\n", ctx->target);
}

struct cache_expire_ctx {
	char cache_key[32];
	struct delayed_work dwork;
};

static void cache_expire_fn(struct work_struct *w)
{
	struct cache_expire_ctx *ctx = container_of(w, struct cache_expire_ctx, dwork.work);
	printf("    [cache_expire_fn] evicting cache key '%s' (should NOT print if cancelled!)\n",
	       ctx->cache_key);
}

int main(void)
{
	struct workqueue_struct *wq = wq_create();
	struct flush_ctx flush_ctx = { .target = "app.log" };
	struct cache_expire_ctx cache_ctx = { .cache_key = "session:42" };

	printf("=== delayed_work_timer demo ===\n\n");

	flush_ctx.dwork.work.func = flush_work_fn;
	cache_ctx.dwork.work.func = cache_expire_fn;

	printf("-- 场景1：正常触发 —— 100ms 后 flush 'app.log' --\n");
	schedule_delayed_work(wq, &flush_ctx.dwork, 100);

	printf("\n-- 场景2：安排 200ms 后过期缓存，但 50ms 后就取消它 --\n");
	schedule_delayed_work(wq, &cache_ctx.dwork, 200);
	usleep(50 * 1000);
	cancel_delayed_work(&cache_ctx.dwork);

	printf("\n-- 等待所有定时器/工作完成 --\n");
	usleep(300 * 1000);   /* 足够长，确保两个定时器都已到期（或被取消） */
	pthread_join(flush_ctx.dwork.timer_thread, NULL);
	pthread_join(cache_ctx.dwork.timer_thread, NULL);

	printf("\n-- 销毁 workqueue --\n");
	wq_destroy(wq);

	printf("\n=== done: flush_ctx fired=%d, cache_ctx fired=%d (expected 0, cancelled) ===\n",
	       flush_ctx.dwork.fired, cache_ctx.dwork.fired);
	return 0;
}
