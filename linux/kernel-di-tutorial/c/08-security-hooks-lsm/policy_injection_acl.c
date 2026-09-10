/*
 * policy_injection_acl.c
 *
 * 场景："策略注入"（Policy Injection）—— LSM hook 提供的是"在哪里检查"
 * 这个机制点，而"具体的检查逻辑（policy）"本身也应该是可插拔的。
 * 本文件演示同一个 access_control 检查点，运行期可以切换三种不同的策略：
 *
 *   - permissive_policy   ：只记录不拒绝（对应 SELinux permissive mode）
 *   - whitelist_policy    ：白名单机制，只允许显式列出的操作
 *   - strict_deny_policy  ：默认拒绝一切，是最严格的策略
 *
 * 这与 02 篇 Ops Table / Strategy Pattern 是同一种手法，但应用在"安全决策"
 * 这个特定领域——所以单独强调："LSM hook 点" 和 "hook 点上跑的具体策略"
 * 是两个可以独立演化的维度。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o policy_injection_acl policy_injection_acl.c
 * 运行：
 *   ./policy_injection_acl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RULES 16

struct access_request {
	char subject[32];   /* 谁 */
	char object[32];     /* 访问什么资源 */
	char action[16];     /* 读/写/执行 */
};

struct policy;

/* 契约：一个策略必须能对一次访问请求做出裁决 */
typedef int (*policy_decide_fn)(struct policy *self, const struct access_request *req, char *reason, size_t reason_sz);

struct policy {
	const char *name;
	policy_decide_fn decide;
	void *private_data;   /* Context Injection：策略自己的规则表等 */
};

/* ------------------------------------------------------------------ */
/* Core：访问控制检查点 —— 对应 LSM hook 里"调用当前生效策略"这一行代码   */
/* ------------------------------------------------------------------ */

static struct policy *g_active_policy;   /* 当前生效的策略，运行期可切换 */

static void set_active_policy(struct policy *p)
{
	printf("[access-control] switching active policy: %s -> %s\n",
	       g_active_policy ? g_active_policy->name : "(none)", p->name);
	g_active_policy = p;
}

/* check_access()：Core 侧唯一的检查入口，完全不知道当前策略的具体逻辑 */
static int check_access(const struct access_request *req)
{
	char reason[128] = {0};
	int ret;

	if (!g_active_policy) {
		fprintf(stderr, "[access-control] no policy loaded, default-deny\n");
		return -1;
	}

	ret = g_active_policy->decide(g_active_policy, req, reason, sizeof(reason));   /* <-- 盲调 */
	printf("  [%s] %s '%s' access '%s' on '%s' -> %s%s%s\n",
	       g_active_policy->name, ret == 0 ? "ALLOW" : "DENY",
	       req->subject, req->action, req->object,
	       ret == 0 ? "" : "(", reason, ret == 0 ? "" : ")");
	return ret;
}

/* ------------------------------------------------------------------ */
/* 策略 A：permissive —— 一切都放行，只是记录（对应 SELinux permissive）   */
/* ------------------------------------------------------------------ */

static int permissive_decide(struct policy *self, const struct access_request *req, char *reason, size_t sz)
{
	(void)self; (void)req;
	snprintf(reason, sz, "permissive mode, would-be-audited");
	return 0;   /* 永远放行 */
}

static struct policy permissive_policy = { .name = "permissive", .decide = permissive_decide };

/* ------------------------------------------------------------------ */
/* 策略 B：whitelist —— 只允许规则表里显式列出的 (subject, object, action) */
/* ------------------------------------------------------------------ */

struct whitelist_rule { char subject[32]; char object[32]; char action[16]; };

struct whitelist_data {
	struct whitelist_rule rules[MAX_RULES];
	int count;
};

static struct whitelist_data g_whitelist_data;

static void whitelist_add_rule(const char *subject, const char *object, const char *action)
{
	struct whitelist_rule *r = &g_whitelist_data.rules[g_whitelist_data.count++];
	strncpy(r->subject, subject, sizeof(r->subject) - 1);
	strncpy(r->object, object, sizeof(r->object) - 1);
	strncpy(r->action, action, sizeof(r->action) - 1);
}

static int whitelist_decide(struct policy *self, const struct access_request *req, char *reason, size_t sz)
{
	struct whitelist_data *wd = self->private_data;
	int i;
	for (i = 0; i < wd->count; i++) {
		struct whitelist_rule *r = &wd->rules[i];
		if (strcmp(r->subject, req->subject) == 0 &&
		    strcmp(r->object, req->object) == 0 &&
		    strcmp(r->action, req->action) == 0) {
			snprintf(reason, sz, "matched whitelist rule #%d", i);
			return 0;
		}
	}
	snprintf(reason, sz, "no matching whitelist rule");
	return -1;
}

static struct policy whitelist_policy = {
	.name = "whitelist", .decide = whitelist_decide, .private_data = &g_whitelist_data,
};

/* ------------------------------------------------------------------ */
/* 策略 C：strict-deny —— 除了对自己拥有的对象读取，其它一律拒绝           */
/* ------------------------------------------------------------------ */

static int strict_deny_decide(struct policy *self, const struct access_request *req, char *reason, size_t sz)
{
	(void)self;
	if (strcmp(req->action, "read") == 0 && strstr(req->object, req->subject) != NULL) {
		snprintf(reason, sz, "self-owned object read-only exception");
		return 0;
	}
	snprintf(reason, sz, "strict-deny: everything else is denied by default");
	return -1;
}

static struct policy strict_deny_policy = { .name = "strict-deny", .decide = strict_deny_decide };

int main(void)
{
	struct access_request req1 = { .subject = "alice", .object = "/data/finance.csv", .action = "write" };
	struct access_request req2 = { .subject = "alice", .object = "/home/alice/notes.txt", .action = "read" };
	struct access_request req3 = { .subject = "bob",   .object = "/data/finance.csv", .action = "write" };

	printf("=== policy_injection_acl demo ===\n\n");

	printf("-- 阶段1：permissive 策略（一切放行）--\n");
	set_active_policy(&permissive_policy);
	check_access(&req1);
	check_access(&req2);
	check_access(&req3);

	printf("\n-- 阶段2：whitelist 策略（只允许显式规则）--\n");
	whitelist_add_rule("alice", "/data/finance.csv", "write");
	set_active_policy(&whitelist_policy);
	check_access(&req1);   /* 命中规则 -> ALLOW */
	check_access(&req2);   /* 没有对应规则 -> DENY */
	check_access(&req3);   /* bob 没有规则 -> DENY */

	printf("\n-- 阶段3：strict-deny 策略（默认拒绝一切，除了读自己拥有的对象）--\n");
	set_active_policy(&strict_deny_policy);
	check_access(&req1);   /* alice 写别人的文件 -> DENY */
	check_access(&req2);   /* alice 读自己的文件 -> ALLOW（对象路径含 subject 名字） */
	check_access(&req3);   /* bob 写别人文件 -> DENY */

	printf("\n=== done ===\n");
	printf("说明：check_access() 从头到尾没有改变一行代码，仅仅通过\n"
	       "      set_active_policy() 切换了三种完全不同的安全决策逻辑。\n");
	return 0;
}
