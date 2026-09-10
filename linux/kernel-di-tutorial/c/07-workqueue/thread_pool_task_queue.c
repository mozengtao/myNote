/*
 * thread_pool_task_queue.c
 *
 * 场景：一个更通用的"任务队列线程池"，任务可以携带任意参数并返回结果
 * （类似简化版 std::future / Kernel 里不常见但用户态常见的模式），
 * 用来对比 Kernel workqueue（fire-and-forget，不关心返回值）与
 * 用户态常见"需要拿到结果"的任务队列之间的设计差异。
 *
 * DI 体现：Core（threadpool_worker）只认识 struct task 契约
 * （run + arg + result），不知道每个任务具体在计算什么。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -o thread_pool_task_queue thread_pool_task_queue.c
 * 运行：
 *   ./thread_pool_task_queue
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_QUEUE 64

struct task;
typedef void (*task_run_fn)(struct task *self);

struct task {
	task_run_fn run;      /* <-- DI 注入点：这个任务具体怎么算 */
	void *arg;              /* 输入参数 */
	long  result;           /* 输出结果 */
	int   done;              /* 完成标志，配合条件变量实现"等待结果" */
	pthread_mutex_t done_lock;
	pthread_cond_t  done_cond;
	struct task *next;
};

static void task_init(struct task *t, task_run_fn fn, void *arg)
{
	t->run = fn;
	t->arg = arg;
	t->result = 0;
	t->done = 0;
	t->next = NULL;
	pthread_mutex_init(&t->done_lock, NULL);
	pthread_cond_init(&t->done_cond, NULL);
}

/* task_wait()：类似 future.get()，阻塞直到任务被某个 worker 执行完 */
static long task_wait(struct task *t)
{
	pthread_mutex_lock(&t->done_lock);
	while (!t->done)
		pthread_cond_wait(&t->done_cond, &t->done_lock);
	pthread_mutex_unlock(&t->done_lock);
	return t->result;
}

static void task_mark_done(struct task *t)
{
	pthread_mutex_lock(&t->done_lock);
	t->done = 1;
	pthread_cond_signal(&t->done_cond);
	pthread_mutex_unlock(&t->done_lock);
}

/* ------------------------------------------------------------------ */
/* Core：线程池                                                        */
/* ------------------------------------------------------------------ */

#define MAX_THREADS 4

struct threadpool {
	pthread_mutex_t lock;
	pthread_cond_t  has_task;
	struct task *head, *tail;
	pthread_t threads[MAX_THREADS];
	int nr_threads;
	int stop;
	long total_run;
};

static void *threadpool_worker(void *arg)
{
	struct threadpool *pool = arg;

	for (;;) {
		struct task *t;

		pthread_mutex_lock(&pool->lock);
		while (!pool->head && !pool->stop)
			pthread_cond_wait(&pool->has_task, &pool->lock);
		if (!pool->head && pool->stop) {
			pthread_mutex_unlock(&pool->lock);
			break;
		}
		t = pool->head;
		pool->head = t->next;
		if (!pool->head)
			pool->tail = NULL;
		pool->total_run++;
		pthread_mutex_unlock(&pool->lock);

		t->run(t);            /* <-- 盲调：pool 不知道 task 具体算什么 */
		task_mark_done(t);     /* 通知等待结果的线程 */
	}
	return NULL;
}

static struct threadpool *threadpool_create(int nr_threads)
{
	struct threadpool *pool = calloc(1, sizeof(*pool));
	int i;

	pthread_mutex_init(&pool->lock, NULL);
	pthread_cond_init(&pool->has_task, NULL);
	pool->nr_threads = nr_threads > MAX_THREADS ? MAX_THREADS : nr_threads;

	for (i = 0; i < pool->nr_threads; i++)
		pthread_create(&pool->threads[i], NULL, threadpool_worker, pool);
	return pool;
}

static void threadpool_submit(struct threadpool *pool, struct task *t)
{
	pthread_mutex_lock(&pool->lock);
	t->next = NULL;
	if (pool->tail) pool->tail->next = t; else pool->head = t;
	pool->tail = t;
	pthread_cond_signal(&pool->has_task);
	pthread_mutex_unlock(&pool->lock);
}

static void threadpool_destroy(struct threadpool *pool)
{
	int i;
	pthread_mutex_lock(&pool->lock);
	pool->stop = 1;
	pthread_cond_broadcast(&pool->has_task);
	pthread_mutex_unlock(&pool->lock);
	for (i = 0; i < pool->nr_threads; i++)
		pthread_join(pool->threads[i], NULL);
	printf("[pool] destroyed, total_run=%ld\n", pool->total_run);
	free(pool);
}

/* ------------------------------------------------------------------ */
/* 任务实现 A：计算斐波那契数（CPU 密集型）                             */
/* ------------------------------------------------------------------ */

static long fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

static void fib_task_fn(struct task *self)
{
	int n = *(int *)self->arg;
	self->result = fib(n);
	printf("  [tid=%lu] fib(%d) = %ld\n", (unsigned long)pthread_self(), n, self->result);
}

/* ------------------------------------------------------------------ */
/* 任务实现 B：字符串反转（简单变换类任务）                              */
/* ------------------------------------------------------------------ */

struct reverse_arg { char buf[64]; };

static void reverse_task_fn(struct task *self)
{
	struct reverse_arg *ra = self->arg;
	int len = (int)strlen(ra->buf);
	int i;
	for (i = 0; i < len / 2; i++) {
		char tmp = ra->buf[i];
		ra->buf[i] = ra->buf[len - 1 - i];
		ra->buf[len - 1 - i] = tmp;
	}
	self->result = len;
	printf("  [tid=%lu] reversed string -> \"%s\" (len=%ld)\n",
	       (unsigned long)pthread_self(), ra->buf, self->result);
}

/* ------------------------------------------------------------------ */
/* 任务实现 C：模拟"校验和计算"，用来演示批量提交 + 汇总结果               */
/* ------------------------------------------------------------------ */

static void checksum_task_fn(struct task *self)
{
	const char *data = self->arg;
	long sum = 0;
	size_t i;
	for (i = 0; data[i]; i++)
		sum += (unsigned char)data[i];
	self->result = sum;
}

int main(void)
{
	struct threadpool *pool;
	struct task fib_tasks[3];
	int fib_args[3] = { 20, 25, 28 };
	struct task rev_task;
	struct reverse_arg rev_arg = { .buf = "DependencyInjection" };
	struct task checksum_tasks[4];
	const char *words[4] = { "kernel", "workqueue", "container_of", "notifier" };
	long total_checksum = 0;
	int i;

	printf("=== thread_pool_task_queue demo ===\n\n");

	pool = threadpool_create(4);

	printf("-- 提交 3 个 CPU 密集型 fib 任务（会分散到不同线程并行计算）--\n");
	for (i = 0; i < 3; i++) {
		task_init(&fib_tasks[i], fib_task_fn, &fib_args[i]);
		threadpool_submit(pool, &fib_tasks[i]);
	}
	for (i = 0; i < 3; i++)
		printf("[main] fib_tasks[%d] result = %ld\n", i, task_wait(&fib_tasks[i]));

	printf("\n-- 提交字符串反转任务 --\n");
	task_init(&rev_task, reverse_task_fn, &rev_arg);
	threadpool_submit(pool, &rev_task);
	printf("[main] reverse task result (length) = %ld\n", task_wait(&rev_task));

	printf("\n-- 批量提交 4 个校验和任务，汇总结果 --\n");
	for (i = 0; i < 4; i++) {
		task_init(&checksum_tasks[i], checksum_task_fn, (void *)words[i]);
		threadpool_submit(pool, &checksum_tasks[i]);
	}
	for (i = 0; i < 4; i++) {
		long r = task_wait(&checksum_tasks[i]);
		printf("[main] checksum('%s') = %ld\n", words[i], r);
		total_checksum += r;
	}
	printf("[main] total_checksum across 4 words = %ld\n", total_checksum);

	printf("\n-- 销毁线程池 --\n");
	threadpool_destroy(pool);

	printf("\n=== done ===\n");
	return 0;
}
