/*
 * audit_hook_pipeline.c
 *
 * 场景：审计（audit）钩子管道 —— 与前两个文件的"访问控制"钩子不同，
 * audit hook 的语义是"事后观察并记录"，不参与放行/拒绝决策（不能改变
 * 访问结果），但多个 audit hook 可以组成一条"管道"，逐级加工同一份
 * 审计事件（过滤 -> 格式化 -> 分发到多个 sink），对应真实 Kernel 里
 * security_ops 的多个 *_audit_rule_* 钩子以及 kauditd 的事件处理管线。
 *
 * 与 05 篇 notifier chain 的区别：notifier 是"广播，各自独立处理”；
 * 这里是"流水线，前一级的输出是后一级的输入”，且允许某一级"过滤掉"
 * 事件（不再传给后续阶段），语义更接近 Unix pipe 而不是"广播"。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o audit_hook_pipeline audit_hook_pipeline.c
 * 运行：
 *   ./audit_hook_pipeline
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STAGES 8
#define MAX_MSG 256

struct audit_event {
	char subject[32];
	char action[16];
	char object[64];
	int  denied;         /* 上游 access-control 的裁决结果，audit 只读不改 */
	char message[MAX_MSG];   /* 格式化阶段会填充/追加内容 */
	int  dropped;          /* 某个 filter 阶段可以把这个事件标记为"丢弃" */
};

struct audit_stage;

/* 返回 1 = 继续传递给下一阶段，0 = 到此为止（被这一阶段消费/丢弃） */
typedef int (*audit_stage_fn)(struct audit_stage *self, struct audit_event *ev);

struct audit_stage {
	const char *name;
	audit_stage_fn process;
	void *private_data;
};

/* ------------------------------------------------------------------ */
/* Core：管道，按注册顺序依次调用每一级                                  */
/* ------------------------------------------------------------------ */

struct audit_pipeline {
	struct audit_stage *stages[MAX_STAGES];
	int count;
};

static struct audit_pipeline g_pipeline = { .count = 0 };

static void pipeline_add_stage(struct audit_stage *s)
{
	g_pipeline.stages[g_pipeline.count++] = s;
	printf("[pipeline] stage '%s' appended (position %d)\n", s->name, g_pipeline.count - 1);
}

/* audit_emit_event()：Core 侧唯一入口，对应真实 Kernel 里 audit_log_xxx() 系列函数
 * 被各个 security hook 调用之后，事件流入 audit 子系统的场景 */
static void audit_emit_event(struct audit_event *ev)
{
	int i;
	printf("[pipeline] new event: subject='%s' action='%s' object='%s' denied=%d\n",
	       ev->subject, ev->action, ev->object, ev->denied);

	for (i = 0; i < g_pipeline.count; i++) {
		struct audit_stage *s = g_pipeline.stages[i];
		int cont = s->process(s, ev);   /* <-- 盲调 */
		if (!cont) {
			printf("  [pipeline] stage '%s' dropped/consumed the event, stopping pipeline\n",
			       s->name);
			return;
		}
	}
	printf("  [pipeline] event reached end of pipeline: \"%s\"\n", ev->message);
}

/* ------------------------------------------------------------------ */
/* 阶段 A：过滤器 —— 丢弃"噪音"事件（比如对 /tmp 下文件的只读访问）        */
/* ------------------------------------------------------------------ */

static int filter_noise_stage(struct audit_stage *self, struct audit_event *ev)
{
	(void)self;
	if (strncmp(ev->object, "/tmp/", 5) == 0 && strcmp(ev->action, "read") == 0 && !ev->denied) {
		ev->dropped = 1;
		printf("  [filter] dropping low-value event on /tmp read\n");
		return 0;   /* 消费掉，不再往下传递 */
	}
	return 1;
}

static struct audit_stage filter_stage = { .name = "noise-filter", .process = filter_noise_stage };

/* ------------------------------------------------------------------ */
/* 阶段 B：格式化 —— 把结构化事件渲染成一条可读的日志行                    */
/* ------------------------------------------------------------------ */

static int format_stage_fn(struct audit_stage *self, struct audit_event *ev)
{
	(void)self;
	snprintf(ev->message, sizeof(ev->message),
		 "AUDIT: subj=%s act=%s obj=%s result=%s",
		 ev->subject, ev->action, ev->object, ev->denied ? "DENIED" : "ALLOWED");
	printf("  [formatter] rendered: \"%s\"\n", ev->message);
	return 1;   /* 继续往下传 */
}

static struct audit_stage format_stage = { .name = "formatter", .process = format_stage_fn };

/* ------------------------------------------------------------------ */
/* 阶段 C：升级检测 —— 如果是"被拒绝的敏感操作”，追加告警标记后继续传递    */
/* ------------------------------------------------------------------ */

struct escalation_ctx { int high_priority_count; };
static struct escalation_ctx g_escalation_ctx;

static int escalation_stage_fn(struct audit_stage *self, struct audit_event *ev)
{
	struct escalation_ctx *ctx = self->private_data;
	if (ev->denied && strcmp(ev->action, "write") == 0) {
		ctx->high_priority_count++;
		strncat(ev->message, " [ESCALATED: denied write attempt]",
			sizeof(ev->message) - strlen(ev->message) - 1);
		printf("  [escalation] flagged as high-priority (#%d)\n", ctx->high_priority_count);
	}
	return 1;
}

static struct audit_stage escalation_stage = {
	.name = "escalation-detector", .process = escalation_stage_fn, .private_data = &g_escalation_ctx,
};

/* ------------------------------------------------------------------ */
/* 阶段 D：sink —— 最终落地（这里模拟"写入日志文件"和"发送到 SIEM"两个终点）*/
/* ------------------------------------------------------------------ */

struct sink_ctx { int events_written; };
static struct sink_ctx g_sink_ctx;

static int sink_stage_fn(struct audit_stage *self, struct audit_event *ev)
{
	struct sink_ctx *ctx = self->private_data;
	ctx->events_written++;
	printf("  [sink] event #%d persisted: \"%s\"\n", ctx->events_written, ev->message);
	return 1;   /* sink 通常是管道最后一级，但仍然返回 1，表示"没有主动丢弃" */
}

static struct audit_stage sink_stage = {
	.name = "log-sink", .process = sink_stage_fn, .private_data = &g_sink_ctx,
};

int main(void)
{
	struct audit_event ev1 = { .subject = "alice", .action = "write", .object = "/data/finance.csv", .denied = 1 };
	struct audit_event ev2 = { .subject = "bob",   .action = "read",  .object = "/tmp/scratch.txt",   .denied = 0 };
	struct audit_event ev3 = { .subject = "alice", .action = "read",  .object = "/home/alice/notes.txt", .denied = 0 };
	struct audit_event ev4 = { .subject = "mallory", .action = "write", .object = "/etc/passwd", .denied = 1 };

	printf("=== audit_hook_pipeline demo ===\n\n");

	printf("-- 组装管道：filter -> format -> escalation -> sink --\n");
	pipeline_add_stage(&filter_stage);
	pipeline_add_stage(&format_stage);
	pipeline_add_stage(&escalation_stage);
	pipeline_add_stage(&sink_stage);

	printf("\n-- 事件1：alice 写 finance.csv 被拒绝（应触发升级 + 落地）--\n");
	audit_emit_event(&ev1);

	printf("\n-- 事件2：bob 读 /tmp 下文件（应被 filter 阶段丢弃，不到达 sink）--\n");
	audit_emit_event(&ev2);

	printf("\n-- 事件3：alice 读自己的笔记（正常事件，格式化后落地，不升级）--\n");
	audit_emit_event(&ev3);

	printf("\n-- 事件4：mallory 写 /etc/passwd 被拒绝（应触发升级 + 落地）--\n");
	audit_emit_event(&ev4);

	printf("\n=== done ===\n");
	printf("high_priority_count=%d, events_written(sink)=%d\n",
	       g_escalation_ctx.high_priority_count, g_sink_ctx.events_written);
	return 0;
}
