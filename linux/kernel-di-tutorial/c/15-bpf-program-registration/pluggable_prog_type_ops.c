/*
 * pluggable_prog_type_ops.c
 *
 * 场景：真实 BPF 子系统里，不同的"程序类型”（prog_type，如
 * BPF_PROG_TYPE_SOCKET_FILTER / BPF_PROG_TYPE_KPROBE / BPF_PROG_TYPE_XDP）
 * 有各自不同的验证规则（能不能访问某些 helper 函数、上下文结构长什么样、
 * 允许的最大栈深度等）和不同的执行环境。内核用一个按 prog_type 索引的
 * ops 表（真实内核里是 `struct bpf_prog_ops` / `struct bpf_verifier_ops`
 * 按类型分发）把"验证规则"和"执行方式"都做成可插拔的。
 *
 * 这是 02-ops-table.md 手法在 BPF 场景下的具体应用：
 * 每种 prog_type 都是一份"verify + run"的实现，核心分发逻辑
 * (`bpf_prog_load`) 完全不知道具体类型的验证细节，只按 type 查表。
 *
 * 本示例模拟两种 prog_type：
 *   PROG_TYPE_SOCKET_FILTER : 网络包过滤类型 —— 只允许访问"包长度"这一个
 *                              上下文字段，不允许任何跳转（保持极简单）。
 *   PROG_TYPE_SYSCALL_FILTER: 系统调用过滤类型（模拟 seccomp-BPF）—— 允许
 *                              访问"syscall 号"和"参数"，允许条件跳转，
 *                              但禁止访问 SOCKET_FILTER 专属的字段。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -std=gnu11 -o pluggable_prog_type_ops pluggable_prog_type_ops.c
 * 运行：
 *   ./pluggable_prog_type_ops
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_INSNS 32

/* ------------------------------------------------------------------ */
/* 1. 契约：通用的字节码指令 + 每种类型自己的"上下文"                    */
/* ------------------------------------------------------------------ */

enum field {
	FIELD_PKT_LEN     = 0,  /* 只有 SOCKET_FILTER 类型允许访问 */
	FIELD_SYSCALL_NR  = 1,  /* 只有 SYSCALL_FILTER 类型允许访问 */
	FIELD_SYSCALL_ARG = 2,  /* 只有 SYSCALL_FILTER 类型允许访问 */
	FIELD_MAX,
};

enum op {
	OP_LOAD_FIELD = 0,  /* R0 = ctx[field] */
	OP_LOAD_IMM,        /* R0 = imm */
	OP_JMP_IF_GT,       /* if (R0 > imm) jmp target；仅 SYSCALL_FILTER 允许 */
	OP_DENY_IF_LT,      /* if (R0 < imm) 立即结束并 DENY；仅 SOCKET_FILTER 允许 */
	OP_ALLOW,           /* 结束：放行 */
	OP_DENY,            /* 结束：拒绝 */
};

struct insn {
	uint8_t  op;
	uint8_t  field;
	int32_t  imm;
	int32_t  target;
};

struct bpf_prog {
	const char *name;
	int prog_type;
	struct insn insns[MAX_INSNS];
	int len;
};

/* 运行期上下文：网络包场景只填 pkt_len，syscall 场景只填 nr/arg0 */
struct run_ctx {
	int32_t pkt_len;
	int32_t syscall_nr;
	int32_t syscall_arg0;
};

enum prog_verdict { VERDICT_ALLOW, VERDICT_DENY, VERDICT_ERROR };

/* ------------------------------------------------------------------ */
/* 2. 每种 prog_type 的 ops：verify + run，签名统一，内部规则各不相同    */
/* ------------------------------------------------------------------ */

struct bpf_prog_ops {
	const char *type_name;
	int  (*verify)(const struct bpf_prog *p, char *errbuf, size_t errbuf_len);
	enum prog_verdict (*run)(const struct bpf_prog *p, const struct run_ctx *ctx);
};

/* ---- SOCKET_FILTER 类型实现 ---- */

static int socket_filter_verify(const struct bpf_prog *p, char *errbuf, size_t n)
{
	int i;

	if (p->len == 0 || p->len > MAX_INSNS) {
		snprintf(errbuf, n, "invalid program length");
		return -1;
	}
	for (i = 0; i < p->len; i++) {
		const struct insn *ins = &p->insns[i];

		/* socket filter 类型的规则：只允许访问 FIELD_PKT_LEN，不允许真正的跳转 */
		if (ins->op == OP_LOAD_FIELD && ins->field != FIELD_PKT_LEN) {
			snprintf(errbuf, n,
				 "socket_filter: field %d not allowed (only PKT_LEN)",
				 ins->field);
			return -1;
		}
		if (ins->op == OP_JMP_IF_GT) {
			snprintf(errbuf, n,
				 "socket_filter: jumps are not allowed for this prog_type");
			return -1;
		}
	}
	if (p->insns[p->len - 1].op != OP_ALLOW && p->insns[p->len - 1].op != OP_DENY) {
		snprintf(errbuf, n, "program does not terminate with ALLOW/DENY");
		return -1;
	}
	return 0;
}

static enum prog_verdict socket_filter_run(const struct bpf_prog *p, const struct run_ctx *ctx)
{
	int32_t r0 = 0;
	int i;

	for (i = 0; i < p->len; i++) {
		const struct insn *ins = &p->insns[i];

		switch (ins->op) {
		case OP_LOAD_FIELD:
			r0 = ctx->pkt_len; /* 已由验证器保证 field == FIELD_PKT_LEN */
			break;
		case OP_LOAD_IMM:
			r0 = ins->imm;
			break;
		case OP_DENY_IF_LT:
			if (r0 < ins->imm)
				return VERDICT_DENY;
			break;
		case OP_ALLOW:
			return VERDICT_ALLOW;
		case OP_DENY:
			return VERDICT_DENY;
		default:
			return VERDICT_ERROR;
		}
	}
	return VERDICT_ERROR;
}

static const struct bpf_prog_ops socket_filter_ops = {
	.type_name = "SOCKET_FILTER",
	.verify    = socket_filter_verify,
	.run       = socket_filter_run,
};

/* ---- SYSCALL_FILTER 类型实现（模拟 seccomp-BPF） ---- */

static int syscall_filter_verify(const struct bpf_prog *p, char *errbuf, size_t n)
{
	int i;

	if (p->len == 0 || p->len > MAX_INSNS) {
		snprintf(errbuf, n, "invalid program length");
		return -1;
	}
	for (i = 0; i < p->len; i++) {
		const struct insn *ins = &p->insns[i];

		/* syscall filter 类型的规则：不允许访问 PKT_LEN（那是网络类型专属字段） */
		if (ins->op == OP_LOAD_FIELD && ins->field == FIELD_PKT_LEN) {
			snprintf(errbuf, n,
				 "syscall_filter: FIELD_PKT_LEN not available in this context");
			return -1;
		}
		/* DENY_IF_LT 是 socket_filter 专属的"无跳转早退"指令，这里不可用 */
		if (ins->op == OP_DENY_IF_LT) {
			snprintf(errbuf, n,
				 "syscall_filter: OP_DENY_IF_LT is socket_filter-only");
			return -1;
		}
		/* 允许跳转，但目标必须在范围内且只能前跳（复用 15 篇第一个示例的思路） */
		if (ins->op == OP_JMP_IF_GT) {
			if (ins->target <= i || ins->target >= p->len) {
				snprintf(errbuf, n,
					 "syscall_filter: bad jump target %d", ins->target);
				return -1;
			}
		}
	}
	if (p->insns[p->len - 1].op != OP_ALLOW && p->insns[p->len - 1].op != OP_DENY) {
		snprintf(errbuf, n, "program does not terminate with ALLOW/DENY");
		return -1;
	}
	return 0;
}

static enum prog_verdict syscall_filter_run(const struct bpf_prog *p, const struct run_ctx *ctx)
{
	int32_t r0 = 0;
	int pc = 0;

	while (pc < p->len) {
		const struct insn *ins = &p->insns[pc];

		switch (ins->op) {
		case OP_LOAD_FIELD:
			r0 = (ins->field == FIELD_SYSCALL_NR) ? ctx->syscall_nr : ctx->syscall_arg0;
			pc++;
			break;
		case OP_LOAD_IMM:
			r0 = ins->imm;
			pc++;
			break;
		case OP_JMP_IF_GT:
			pc = (r0 > ins->imm) ? ins->target : pc + 1;
			break;
		case OP_ALLOW:
			return VERDICT_ALLOW;
		case OP_DENY:
			return VERDICT_DENY;
		default:
			return VERDICT_ERROR;
		}
	}
	return VERDICT_ERROR;
}

static const struct bpf_prog_ops syscall_filter_ops = {
	.type_name = "SYSCALL_FILTER",
	.verify    = syscall_filter_verify,
	.run       = syscall_filter_run,
};

/* ------------------------------------------------------------------ */
/* 3. Core：按 prog_type 查表分发，从不知道具体类型的验证/执行细节       */
/* ------------------------------------------------------------------ */

enum prog_type { PROG_TYPE_SOCKET_FILTER = 0, PROG_TYPE_SYSCALL_FILTER = 1, PROG_TYPE_MAX };

/* 这张表就是"依赖注入点"：新增一种 prog_type，只需要新增一行，
 * bpf_prog_load()/bpf_prog_run() 一行都不用改 —— 与 02-ops-table.md 完全同构 */
static const struct bpf_prog_ops *g_prog_ops_table[PROG_TYPE_MAX] = {
	[PROG_TYPE_SOCKET_FILTER]  = &socket_filter_ops,
	[PROG_TYPE_SYSCALL_FILTER] = &syscall_filter_ops,
};

static int bpf_prog_load(struct bpf_prog *p)
{
	const struct bpf_prog_ops *ops;
	char errbuf[128];

	if (p->prog_type < 0 || p->prog_type >= PROG_TYPE_MAX) {
		printf("[core] load '%s': unknown prog_type %d\n", p->name, p->prog_type);
		return -1;
	}
	ops = g_prog_ops_table[p->prog_type];   /* <-- 盲调分发点 */

	printf("[core] load '%s' (type=%s): ", p->name, ops->type_name);
	if (ops->verify(p, errbuf, sizeof(errbuf)) != 0) {
		printf("REJECTED (%s)\n", errbuf);
		return -1;
	}
	printf("ACCEPTED\n");
	return 0;
}

static void bpf_prog_run_and_report(struct bpf_prog *p, const struct run_ctx *ctx)
{
	const struct bpf_prog_ops *ops = g_prog_ops_table[p->prog_type];
	enum prog_verdict v = ops->run(p, ctx);   /* <-- 盲调分发点 */
	const char *vs = (v == VERDICT_ALLOW) ? "ALLOW" : (v == VERDICT_DENY) ? "DENY" : "ERROR";

	printf("  [core] run '%s' -> verdict = %s\n", p->name, vs);
}

/* ------------------------------------------------------------------ */
/* 4. 测试程序                                                          */
/* ------------------------------------------------------------------ */

int main(void)
{
	printf("=== pluggable_prog_type_ops demo ===\n\n");

	/* 网络过滤程序：pkt_len < 64 就丢弃（模拟丢弃过小的包），否则放行 */
	struct bpf_prog net_filter = {
		.name = "drop_small_packets",
		.prog_type = PROG_TYPE_SOCKET_FILTER,
		.len = 3,
		.insns = {
			{ .op = OP_LOAD_FIELD,  .field = FIELD_PKT_LEN },
			{ .op = OP_DENY_IF_LT,  .imm = 64 },
			{ .op = OP_ALLOW },
		},
	};
	printf("-- 加载网络过滤程序 --\n");
	if (bpf_prog_load(&net_filter) == 0) {
		struct run_ctx ctx_big   = { .pkt_len = 1500 };
		struct run_ctx ctx_small = { .pkt_len = 20 };
		bpf_prog_run_and_report(&net_filter, &ctx_big);
		bpf_prog_run_and_report(&net_filter, &ctx_small);
	}

	printf("\n-- 尝试把网络过滤程序错误标记为 SYSCALL_FILTER 类型加载 --\n");
	struct bpf_prog net_filter_wrong_type = net_filter;
	net_filter_wrong_type.name = "drop_small_packets_mislabeled";
	net_filter_wrong_type.prog_type = PROG_TYPE_SYSCALL_FILTER;
	bpf_prog_load(&net_filter_wrong_type); /* 应该被 syscall_filter_verify 拒绝：不允许访问 PKT_LEN */

	printf("\n-- 加载 syscall 过滤程序：只允许 syscall_nr <= 300（模拟白名单） --\n");
	struct bpf_prog syscall_filter = {
		.name = "syscall_nr_whitelist",
		.prog_type = PROG_TYPE_SYSCALL_FILTER,
		.len = 4,
		.insns = {
			{ .op = OP_LOAD_FIELD,  .field = FIELD_SYSCALL_NR },
			{ .op = OP_JMP_IF_GT,   .imm = 300, .target = 3 },
			{ .op = OP_ALLOW },
			{ .op = OP_DENY },
		},
	};
	if (bpf_prog_load(&syscall_filter) == 0) {
		struct run_ctx ctx_ok  = { .syscall_nr = 42 };
		struct run_ctx ctx_bad = { .syscall_nr = 9999 };
		bpf_prog_run_and_report(&syscall_filter, &ctx_ok);
		bpf_prog_run_and_report(&syscall_filter, &ctx_bad);
	}

	printf("\n-- 尝试加载一个 syscall 过滤程序，但跳转目标非法 --\n");
	struct bpf_prog syscall_bad_jump = {
		.name = "syscall_bad_jump",
		.prog_type = PROG_TYPE_SYSCALL_FILTER,
		.len = 2,
		.insns = {
			{ .op = OP_LOAD_FIELD, .field = FIELD_SYSCALL_NR },
			{ .op = OP_JMP_IF_GT,  .imm = 1, .target = 99 }, /* 越界目标 */
		},
	};
	bpf_prog_load(&syscall_bad_jump);

	printf("\n-- 尝试加载一个未知 prog_type 的程序 --\n");
	struct bpf_prog unknown_type = {
		.name = "unknown_type_prog",
		.prog_type = 999,
		.len = 1,
		.insns = { { .op = OP_ALLOW } },
	};
	bpf_prog_load(&unknown_type);

	printf("\n=== done ===\n");
	return 0;
}
