/*
 * worker_thread_pool.c
 *
 * 场景：用 pthread 实现一个线程池，模拟 Kernel 的
 * struct work_struct { work_func_t func; struct list_head entry; };
 *
 * 关键点是 Context Injection：work_struct 本身不携带"业务数据"，
 * 业务数据靠 "把 work_struct 嵌入到自己的结构体里"，回调时用 container_of()
 * 反推回外层对象 —— 与 03 篇标题里说的 "container_of / work_struct" 是同一件事的
 * 两个侧面：container_of_demo.c 讲机制，本文件讲这个机制在异步任务队列里的应用。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -o worker_thread_pool worker_thread_pool.c
 * 运行：
 *   ./worker_thread_pool
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>

#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

/* ------------------------------------------------------------------ */
/* 1. 契约：mini_work，对应 Kernel struct work_struct                  */
/* ------------------------------------------------------------------ */

struct mini_work;
typedef void (*mini_work_fn)(struct mini_work *work);

struct mini_work {
	mini_work_fn func;     /* <-- Function Pointer Callback（01 篇）*/
	struct mini_work *next;
};

#define INIT_WORK(w, fn) do { (w)->func = (fn); (w)->next = NULL; } while (0)

/* ------------------------------------------------------------------ */
/* 2. Core：workqueue + worker thread                                  */
/* ------------------------------------------------------------------ */

struct mini_workqueue {
	pthread_mutex_t lock;
	pthread_cond_t  cond;
	struct mini_work *head, *tail;
	pthread_t worker;
	int stop;
	long processed;
};

static struct mini_workqueue g_wq;

static void wq_init(struct mini_workqueue *wq)
{
	memset(wq, 0, sizeof(*wq));
	pthread_mutex_init(&wq->lock, NULL);
	pthread_cond_init(&wq->cond, NULL);
}

/* schedule_work() 风格接口：把 work 塞进队列，注入的是"数据 + 回调"打包体 */
static void queue_work(struct mini_workqueue *wq, struct mini_work *w)
{
	pthread_mutex_lock(&wq->lock);
	w->next = NULL;
	if (wq->tail)
		wq->tail->next = w;
	else
		wq->head = w;
	wq->tail = w;
	pthread_cond_signal(&wq->cond);
	pthread_mutex_unlock(&wq->lock);
}

/* worker_thread()：对应 kernel/workqueue.c worker_thread()/process_one_work() */
static void *worker_thread(void *arg)
{
	struct mini_workqueue *wq = arg;

	for (;;) {
		struct mini_work *w;

		pthread_mutex_lock(&wq->lock);
		while (!wq->head && !wq->stop)
			pthread_cond_wait(&wq->cond, &wq->lock);

		if (!wq->head && wq->stop) {
			pthread_mutex_unlock(&wq->lock);
			break;
		}

		w = wq->head;
		wq->head = w->next;
		if (!wq->head)
			wq->tail = NULL;
		pthread_mutex_unlock(&wq->lock);

		/* <-- 核心一行：worker 不知道 w 具体嵌在哪个对象里，
		 * 只调用 func(w)，由 func 内部用 container_of 找回上下文 */
		w->func(w);
		wq->processed++;
	}
	printf("[worker] exiting, processed %ld work item(s) total\n", wq->processed);
	return NULL;
}

static void wq_start(struct mini_workqueue *wq)
{
	pthread_create(&wq->worker, NULL, worker_thread, wq);
}

static void wq_stop_and_join(struct mini_workqueue *wq)
{
	pthread_mutex_lock(&wq->lock);
	wq->stop = 1;
	pthread_cond_signal(&wq->cond);
	pthread_mutex_unlock(&wq->lock);
	pthread_join(wq->worker, NULL);
}

/* ------------------------------------------------------------------ */
/* 3. 业务对象 A：磁盘刷盘任务，把 mini_work 嵌入自己的结构体            */
/* ------------------------------------------------------------------ */

struct flush_task {
	char device_name[32];
	int  dirty_pages;
	struct mini_work work;   /* <-- Context Injection：嵌入通用 work 结构 */
};

static void flush_task_fn(struct mini_work *w)
{
	/* container_of：从 core 传进来的通用 work 指针，反推出业务对象 */
	struct flush_task *ft = container_of(w, struct flush_task, work);
	printf("  [flush] device='%s' flushing %d dirty pages (tid=%lu)\n",
	       ft->device_name, ft->dirty_pages, (unsigned long)pthread_self());
	usleep(10 * 1000);
	printf("  [flush] device='%s' done\n", ft->device_name);
}

/* ------------------------------------------------------------------ */
/* 4. 业务对象 B：网络重传任务，携带不同的业务数据                      */
/* ------------------------------------------------------------------ */

struct retransmit_task {
	int seq_no;
	int attempt;
	struct mini_work work;
};

static void retransmit_task_fn(struct mini_work *w)
{
	struct retransmit_task *rt = container_of(w, struct retransmit_task, work);
	printf("  [retransmit] seq=%d attempt=%d\n", rt->seq_no, rt->attempt);
}

int main(void)
{
	struct flush_task ft1 = { .device_name = "sda1", .dirty_pages = 128 };
	struct flush_task ft2 = { .device_name = "sdb1", .dirty_pages = 64 };
	struct retransmit_task rt1 = { .seq_no = 1001, .attempt = 1 };
	struct retransmit_task rt2 = { .seq_no = 1002, .attempt = 3 };

	printf("=== worker_thread_pool demo ===\n\n");

	wq_init(&g_wq);
	wq_start(&g_wq);

	INIT_WORK(&ft1.work, flush_task_fn);
	INIT_WORK(&ft2.work, flush_task_fn);
	INIT_WORK(&rt1.work, retransmit_task_fn);
	INIT_WORK(&rt2.work, retransmit_task_fn);

	printf("-- 主线程提交 4 个任务到 workqueue --\n");
	queue_work(&g_wq, &ft1.work);
	queue_work(&g_wq, &rt1.work);
	queue_work(&g_wq, &ft2.work);
	queue_work(&g_wq, &rt2.work);

	/* 给 worker 一点时间处理完队列 */
	usleep(100 * 1000);

	printf("\n-- 停止 workqueue --\n");
	wq_stop_and_join(&g_wq);

	printf("\n=== done, total processed = %ld ===\n", g_wq.processed);
	return 0;
}
