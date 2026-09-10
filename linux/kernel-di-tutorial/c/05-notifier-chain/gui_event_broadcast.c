/*
 * gui_event_broadcast.c
 *
 * 场景：GUI 系统里"多个监听者（widget）同时监听同一个事件源"的经典场景
 * ——一个按钮被点击，多个不相关的 widget（状态栏、日志面板、动画层）
 * 都需要响应，且响应顺序、是否"消费掉事件不再往下传播"都要可控。
 *
 * 与 notifier chain 的对应关系：
 *   - 事件冒泡 + "consumed 后停止传播" 就是 NOTIFY_STOP_MASK 语义的 GUI 版本
 *   - 每个 widget 的 z-order（层级）决定响应优先级，等价于 notifier priority
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o gui_event_broadcast gui_event_broadcast.c
 * 运行：
 *   ./gui_event_broadcast
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LISTENERS 16

struct gui_event {
	const char *type;      /* "click" / "key" / "resize" */
	int x, y;
	int consumed;           /* 一旦被置 1，后续监听者不再被调用（对应 NOTIFY_STOP_MASK） */
};

struct widget;

/* 返回值：1 = 消费了这个事件（停止继续传播），0 = 不处理，继续往下传播 */
typedef int (*event_handler_fn)(struct widget *self, struct gui_event *ev);

struct widget {
	char name[32];
	int  z_order;    /* 数值越大越"上层"，越先收到事件，对应 notifier priority */
	event_handler_fn on_event;
	int  events_handled;
};

struct event_dispatcher {
	struct widget *listeners[MAX_LISTENERS];
	int count;
};

static struct event_dispatcher g_dispatcher = { .count = 0 };

/* 注册时按 z_order 从高到低插入排序 —— 上层 widget 先收到事件 */
static int dispatcher_add_widget(struct widget *w)
{
	int i, pos;

	if (g_dispatcher.count >= MAX_LISTENERS)
		return -1;

	pos = g_dispatcher.count;
	for (i = 0; i < g_dispatcher.count; i++) {
		if (w->z_order > g_dispatcher.listeners[i]->z_order) {
			pos = i;
			break;
		}
	}
	memmove(&g_dispatcher.listeners[pos + 1], &g_dispatcher.listeners[pos],
		(size_t)(g_dispatcher.count - pos) * sizeof(struct widget *));
	g_dispatcher.listeners[pos] = w;
	g_dispatcher.count++;

	printf("[dispatcher] widget '%s' registered (z_order=%d)\n", w->name, w->z_order);
	return 0;
}

static void dispatcher_remove_widget(struct widget *w)
{
	int i;
	for (i = 0; i < g_dispatcher.count; i++) {
		if (g_dispatcher.listeners[i] == w) {
			memmove(&g_dispatcher.listeners[i], &g_dispatcher.listeners[i + 1],
				(size_t)(g_dispatcher.count - i - 1) * sizeof(struct widget *));
			g_dispatcher.count--;
			printf("[dispatcher] widget '%s' removed\n", w->name);
			return;
		}
	}
}

/* dispatcher_dispatch()：核心广播逻辑，按 z_order 从高到低尝试，
 * 一旦某个 widget "consumed" 事件就停止 —— GUI 里的"事件冒泡截断" */
static void dispatcher_dispatch(struct gui_event *ev)
{
	int i;
	printf("[dispatcher] dispatching '%s' event at (%d,%d) to %d widget(s)\n",
	       ev->type, ev->x, ev->y, g_dispatcher.count);

	for (i = 0; i < g_dispatcher.count; i++) {
		struct widget *w = g_dispatcher.listeners[i];
		int handled = w->on_event(w, ev);   /* <-- 盲调 */
		if (handled) {
			w->events_handled++;
			ev->consumed = 1;
			printf("  [dispatcher] event consumed by '%s' (z=%d), stop propagation\n",
			       w->name, w->z_order);
			break;
		}
	}
	if (!ev->consumed)
		printf("  [dispatcher] no widget consumed the event, it falls through\n");
}

/* ------------------------------------------------------------------ */
/* widget A：弹出的确认对话框（最上层，z_order 最高，优先响应点击）      */
/* ------------------------------------------------------------------ */

static int dialog_on_event(struct widget *self, struct gui_event *ev)
{
	(void)self;
	if (strcmp(ev->type, "click") == 0 && ev->x < 200 && ev->y < 100) {
		printf("    [dialog] click inside dialog bounds, handling 'OK' button\n");
		return 1;   /* 消费掉，下层 widget 不应该再收到这次点击 */
	}
	printf("    [dialog] click outside dialog bounds, ignoring\n");
	return 0;
}

static struct widget dialog_widget = { .name = "confirm-dialog", .z_order = 100, .on_event = dialog_on_event };

/* ------------------------------------------------------------------ */
/* widget B：状态栏（中间层）                                          */
/* ------------------------------------------------------------------ */

static int statusbar_on_event(struct widget *self, struct gui_event *ev)
{
	(void)self;
	if (strcmp(ev->type, "click") == 0 && ev->y > 500) {
		printf("    [statusbar] click in status bar, toggling menu\n");
		return 1;
	}
	return 0;
}

static struct widget statusbar_widget = { .name = "status-bar", .z_order = 50, .on_event = statusbar_on_event };

/* ------------------------------------------------------------------ */
/* widget C：日志面板 —— 不消费任何事件，只是"旁观并记录"（最底层）       */
/* ------------------------------------------------------------------ */

static int logpanel_on_event(struct widget *self, struct gui_event *ev)
{
	(void)self;
	printf("    [logpanel] observed event '%s' at (%d,%d), not consuming\n",
	       ev->type, ev->x, ev->y);
	return 0;   /* 永远不消费，只是记录 —— 说明"最底层但先注册的 widget 也能看到事件" */
}

static struct widget logpanel_widget = { .name = "log-panel", .z_order = 0, .on_event = logpanel_on_event };

int main(void)
{
	struct gui_event ev1 = { .type = "click", .x = 50,  .y = 30,  .consumed = 0 };
	struct gui_event ev2 = { .type = "click", .x = 400, .y = 550, .consumed = 0 };
	struct gui_event ev3 = { .type = "click", .x = 400, .y = 300, .consumed = 0 };

	printf("=== gui_event_broadcast demo ===\n\n");

	/* 乱序注册，验证按 z_order 排序后响应顺序正确 */
	dispatcher_add_widget(&logpanel_widget);
	dispatcher_add_widget(&statusbar_widget);
	dispatcher_add_widget(&dialog_widget);

	printf("\n-- 事件1：点在对话框范围内，应该被 dialog 消费，statusbar/logpanel 不会收到 --\n");
	dispatcher_dispatch(&ev1);

	printf("\n-- 事件2：点在状态栏区域，dialog 忽略，statusbar 消费 --\n");
	dispatcher_dispatch(&ev2);

	printf("\n-- 事件3：点在空白区域，谁都不消费，最终 logpanel 只是观察到但不处理 --\n");
	dispatcher_dispatch(&ev3);

	printf("\n-- 关闭对话框（注销），重新触发事件1的坐标 --\n");
	dispatcher_remove_widget(&dialog_widget);
	ev1.consumed = 0;
	dispatcher_dispatch(&ev1);

	printf("\n=== done ===\n");
	printf("dialog handled=%d, statusbar handled=%d, logpanel handled=%d\n",
	       dialog_widget.events_handled, statusbar_widget.events_handled, logpanel_widget.events_handled);
	return 0;
}
