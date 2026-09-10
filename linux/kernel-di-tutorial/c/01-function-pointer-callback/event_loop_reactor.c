/*
 * event_loop_reactor.c
 *
 * 场景：经典 Reactor 模式 —— 一个事件循环（event loop / reactor core）
 * 通过 select() 监听多个 fd，当某个 fd 可读时，"盲调"该 fd 注册时提供的回调函数。
 *
 * 这精确对应 Kernel 中 "irq -> irq_desc->handle_irq -> action->handler(irq, dev_id)"
 * 的调度模型（细节见 14-irq-handler-registration，后续批次）：
 *
 *   reactor_run()
 *        │
 *        │ select() 发现 fd 就绪
 *        ▼
 *   查 fd -> handler 映射表
 *        │
 *        ▼
 *   handler->on_readable(handler, fd)   <-- 回调，reactor 不知道谁实现的
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o event_loop_reactor event_loop_reactor.c
 * 运行：
 *   ./event_loop_reactor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#define MAX_HANDLERS 32

struct fd_handler;

/* 契约：reactor 只认这个函数指针签名 */
typedef void (*on_readable_fn)(struct fd_handler *self, int fd);

struct fd_handler {
	int fd;
	char name[32];
	void *ctx;                 /* Context Injection：携带该 handler 私有状态 */
	on_readable_fn on_readable; /* <-- DI 注入点 */
};

struct reactor {
	struct fd_handler *handlers[MAX_HANDLERS];
	int nr_handlers;
	int running;
};

static struct reactor g_reactor = { .nr_handlers = 0, .running = 0 };

/* reactor_add_handler()：注册回调，等价于 request_irq() */
static int reactor_add_handler(struct fd_handler *h)
{
	if (g_reactor.nr_handlers >= MAX_HANDLERS)
		return -1;
	if (!h->on_readable) {
		fprintf(stderr, "[reactor] refuse handler '%s' with NULL callback\n", h->name);
		return -1;
	}
	g_reactor.handlers[g_reactor.nr_handlers++] = h;
	printf("[reactor] added handler '%s' on fd=%d\n", h->name, h->fd);
	return 0;
}

/* reactor_remove_handler()：注销回调，等价于 free_irq() */
static void reactor_remove_handler(struct fd_handler *h)
{
	int i;
	for (i = 0; i < g_reactor.nr_handlers; i++) {
		if (g_reactor.handlers[i] == h) {
			g_reactor.handlers[i] = g_reactor.handlers[g_reactor.nr_handlers - 1];
			g_reactor.nr_handlers--;
			printf("[reactor] removed handler '%s'\n", h->name);
			return;
		}
	}
}

/* reactor_run()：核心 dispatch loop —— select() 之后逐一盲调回调 */
static void reactor_run(int max_iterations)
{
	int iter = 0;

	g_reactor.running = 1;
	while (g_reactor.running && iter < max_iterations) {
		fd_set rfds;
		int maxfd = -1;
		int i, ret;
		struct timeval tv = { .tv_sec = 0, .tv_usec = 100 * 1000 };

		FD_ZERO(&rfds);
		for (i = 0; i < g_reactor.nr_handlers; i++) {
			int fd = g_reactor.handlers[i]->fd;
			FD_SET(fd, &rfds);
			if (fd > maxfd)
				maxfd = fd;
		}

		if (maxfd < 0)
			break;

		ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("select");
			break;
		}

		if (ret > 0) {
			/* 注意：遍历期间 handler 表可能被回调本身修改，这里用副本规避，
			 * 对应 Failure 小节里讨论的 "回调内注销自身" 竞争问题 */
			struct fd_handler *snapshot[MAX_HANDLERS];
			int n = g_reactor.nr_handlers;
			memcpy(snapshot, g_reactor.handlers, sizeof(struct fd_handler *) * n);

			for (i = 0; i < n; i++) {
				struct fd_handler *h = snapshot[i];
				if (FD_ISSET(h->fd, &rfds)) {
					/* <-- 这里就是"运行期盲调"：reactor 不知道 on_readable 里是什么 */
					h->on_readable(h, h->fd);
				}
			}
		}

		iter++;
	}
	printf("[reactor] event loop exiting after %d iterations\n", iter);
}

/* -------------------------------------------------------------- */
/* 具体实现 A：echo handler —— 读一行，打印出来                    */
/* -------------------------------------------------------------- */

struct echo_ctx {
	int lines_seen;
};

static void echo_on_readable(struct fd_handler *self, int fd)
{
	struct echo_ctx *c = self->ctx;
	char buf[256];
	ssize_t n = read(fd, buf, sizeof(buf) - 1);

	if (n <= 0) {
		printf("  [echo] fd=%d closed or error, removing self\n", fd);
		reactor_remove_handler(self);
		return;
	}
	buf[n] = '\0';
	c->lines_seen++;
	printf("  [echo] (#%d) got %zd bytes: %s", c->lines_seen, n, buf);
}

/* -------------------------------------------------------------- */
/* 具体实现 B：counter handler —— 只统计触发次数，不关心内容        */
/* -------------------------------------------------------------- */

struct counter_ctx {
	int fired;
};

static void counter_on_readable(struct fd_handler *self, int fd)
{
	struct counter_ctx *c = self->ctx;
	char buf[64];
	ssize_t n = read(fd, buf, sizeof(buf));
	c->fired++;
	printf("  [counter] fd=%d fired %d time(s), consumed %zd bytes\n",
	       fd, c->fired, n < 0 ? 0 : n);
}

int main(void)
{
	int pipe_a[2], pipe_b[2];
	struct echo_ctx ectx = { .lines_seen = 0 };
	struct counter_ctx cctx = { .fired = 0 };
	struct fd_handler h_echo, h_counter;

	if (pipe(pipe_a) < 0 || pipe(pipe_b) < 0) {
		perror("pipe");
		return 1;
	}

	memset(&h_echo, 0, sizeof(h_echo));
	strcpy(h_echo.name, "echo-handler");
	h_echo.fd = pipe_a[0];
	h_echo.ctx = &ectx;
	h_echo.on_readable = echo_on_readable;

	memset(&h_counter, 0, sizeof(h_counter));
	strcpy(h_counter.name, "counter-handler");
	h_counter.fd = pipe_b[0];
	h_counter.ctx = &cctx;
	h_counter.on_readable = counter_on_readable;

	printf("=== event_loop_reactor demo ===\n\n");
	reactor_add_handler(&h_echo);
	reactor_add_handler(&h_counter);

	/* 模拟外部事件源写入数据 —— 真实世界里这是网卡、磁盘、定时器等中断源 */
	write(pipe_a[1], "hello reactor\n", 14);
	write(pipe_b[1], "x", 1);
	write(pipe_b[1], "y", 1);

	printf("\n-- 运行事件循环 --\n");
	reactor_run(5);

	close(pipe_a[0]); close(pipe_a[1]);
	close(pipe_b[0]); close(pipe_b[1]);

	printf("\n=== done, echo saw %d line(s), counter fired %d time(s) ===\n",
	       ectx.lines_seen, cctx.fired);
	return 0;
}
