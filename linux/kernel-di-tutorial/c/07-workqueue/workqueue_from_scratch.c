/*
 * workqueue_from_scratch.c
 *
 * 场景：用 pthread 从零实现一个"多 worker 线程"的 workqueue，尽量贴近
 * Kernel kernel/workqueue.c 的核心概念：
 *
 *   - create_workqueue(nr_workers)：对应 alloc_workqueue()
 *   - queue_work(wq, work)：把 work 塞进共享队列，唤醒某个空闲 worker
 *   - flush_workqueue(wq)：阻塞直到当前所有已入队的 work 执行完毕
 *   - destroy_workqueue(wq)：停止所有 worker 线程并回收资源
 *
 * 与 03-context-injection/worker_thread_pool.c 的区别：那里只有 1 个
 * worker 线程，重点是演示 container_of；这里有 N 个 worker 线程并发
 * 处理同一个队列，重点是演示 workqueue 的"并发调度 + flush 语义"。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -o workqueue_from_scratch workqueue_from_scratch.c
 * 运行：
 *   ./workqueue_from_scratch
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

#define MAX_WORKERS 8

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	work_func_t func;
	struct work_struct *next;
};

#define INIT_WORK(w, fn) do { (w)->func = (fn); (w)->next = NULL; } while (0)

struct workqueue_struct {
	pthread_mutex_t lock;
	pthread_cond_t  has_work;
	pthread_cond_t  all_idle;      /* flush_workqueue 用来等待队列清空 */
	struct work_struct *head, *tail;
	pthread_t workers[MAX_WORKERS];
	int nr_workers;
	int busy_workers;               /* 正在执行 work 的 worker 数 */
	int pending;                     /* 队列中还未被取走的 work 数 */
	int stop;
	long total_processed;
};

/* ------------------------------------------------------------------ */
/* Core：workqueue 生命周期管理                                        */
/* ------------------------------------------------------------------ */

static void *worker_thread_main(void *arg);

/* alloc_workqueue() 风格接口 */
static struct workqueue_struct *create_workqueue(const char *name, int nr_workers)
{
	struct workqueue_struct *wq = calloc(1, sizeof(*wq));
	int i;

	pthread_mutex_init(&wq->lock, NULL);
	pthread_cond_init(&wq->has_work, NULL);
	pthread_cond_init(&wq->all_idle, NULL);
	wq->nr_workers = nr_workers > MAX_WORKERS ? MAX_WORKERS : nr_workers;

	printf("[wq:%s] creating workqueue with %d worker thread(s)\n", name, wq->nr_workers);
	for (i = 0; i < wq->nr_workers; i++)
		pthread_create(&wq->workers[i], NULL, worker_thread_main, wq);

	return wq;
}

/* queue_work() 风格接口：注入一个 work，Core 决定何时/由哪个 worker 执行 */
static int queue_work(struct workqueue_struct *wq, struct work_struct *w)
{
	pthread_mutex_lock(&wq->lock);
	w->next = NULL;
	if (wq->tail)
		wq->tail->next = w;
	else
		wq->head = w;
	wq->tail = w;
	wq->pending++;
	pthread_cond_signal(&wq->has_work);   /* 唤醒一个空闲 worker */
	pthread_mutex_unlock(&wq->lock);
	return 1;
}

/* flush_workqueue()：阻塞直到队列清空且没有 worker 正在处理 work */
static void flush_workqueue(struct workqueue_struct *wq)
{
	pthread_mutex_lock(&wq->lock);
	while (wq->pending > 0 || wq->busy_workers > 0)
		pthread_cond_wait(&wq->all_idle, &wq->lock);
	pthread_mutex_unlock(&wq->lock);
}

/* worker_thread()：对应 kernel/workqueue.c worker_thread()，
 * 本质就是一个"不断从队列取 work 并调用 work->func(work)"的循环 */
static void *worker_thread_main(void *arg)
{
	struct workqueue_struct *wq = arg;

	for (;;) {
		struct work_struct *w;

		pthread_mutex_lock(&wq->lock);
		while (!wq->head && !wq->stop)
			pthread_cond_wait(&wq->has_work, &wq->lock);

		if (!wq->head && wq->stop) {
			pthread_mutex_unlock(&wq->lock);
			break;
		}

		w = wq->head;
		wq->head = w->next;
		if (!wq->head)
			wq->tail = NULL;
		wq->pending--;
		wq->busy_workers++;
		pthread_mutex_unlock(&wq->lock);

		w->func(w);   /* <-- 盲调，worker 不知道具体是谁提交的、干什么的 */

		pthread_mutex_lock(&wq->lock);
		wq->busy_workers--;
		wq->total_processed++;
		if (wq->pending == 0 && wq->busy_workers == 0)
			pthread_cond_broadcast(&wq->all_idle);   /* 通知 flush_workqueue */
		pthread_mutex_unlock(&wq->lock);
	}
	return NULL;
}

/* destroy_workqueue()：停止所有 worker，回收线程资源 */
static void destroy_workqueue(struct workqueue_struct *wq)
{
	int i;

	pthread_mutex_lock(&wq->lock);
	wq->stop = 1;
	pthread_cond_broadcast(&wq->has_work);
	pthread_mutex_unlock(&wq->lock);

	for (i = 0; i < wq->nr_workers; i++)
		pthread_join(wq->workers[i], NULL);

	printf("[wq] destroyed, total_processed=%ld\n", wq->total_processed);
	free(wq);
}

/* ------------------------------------------------------------------ */
/* 业务对象：图片缩略图生成任务                                        */
/* ------------------------------------------------------------------ */

struct thumbnail_job {
	char filename[64];
	int  width, height;
	struct work_struct work;
};

static void thumbnail_work_fn(struct work_struct *w)
{
	struct thumbnail_job *job = container_of(w, struct thumbnail_job, work);
	printf("  [worker tid=%lu] generating %dx%d thumbnail for '%s'\n",
	       (unsigned long)pthread_self(), job->width, job->height, job->filename);
	usleep(20 * 1000);   /* 模拟耗时的图像处理 */
	printf("  [worker tid=%lu] done with '%s'\n", (unsigned long)pthread_self(), job->filename);
}

int main(void)
{
	struct workqueue_struct *wq;
	struct thumbnail_job jobs[6];
	int i;

	printf("=== workqueue_from_scratch demo ===\n\n");

	wq = create_workqueue("thumbnail-wq", 3);   /* 3 个 worker 并发处理 */

	printf("\n-- 提交 6 个缩略图生成任务到 3 个 worker 的 workqueue --\n");
	for (i = 0; i < 6; i++) {
		snprintf(jobs[i].filename, sizeof(jobs[i].filename), "photo_%02d.jpg", i);
		jobs[i].width = 128;
		jobs[i].height = 128;
		INIT_WORK(&jobs[i].work, thumbnail_work_fn);
		queue_work(wq, &jobs[i].work);
	}

	printf("\n-- flush_workqueue()：阻塞直到全部 6 个任务处理完毕 --\n");
	flush_workqueue(wq);
	printf("[main] flush returned, all work confirmed done\n");

	printf("\n-- 销毁 workqueue --\n");
	destroy_workqueue(wq);

	printf("\n=== done ===\n");
	return 0;
}
