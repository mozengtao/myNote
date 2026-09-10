/*
 * hook_point_bpf_attach.c
 *
 * 场景：模拟把一段"已经验证通过"的字节码程序挂接（attach）到内核里的
 * 一个真实 hook 点上 —— 这里模拟的是网络设备的"包接收路径”
 * （对应真实内核的 XDP / cls_bpf / socket filter attach 点）。
 *
 * 与前两个示例的区别：
 *   toy_bytecode_verifier.c    只演示"验证 + 单次执行"。
 *   pluggable_prog_type_ops.c  只演示"按类型分发验证/执行规则"。
 *   本示例                     演示程序被 attach 之后，成为 hook 点的
 *                              "常驻策略"：每一个流经 hook 点的真实事件
 *                              （模拟的收包）都会重新触发一次已验证程序的
 *                              执行，运行期动态决定放行还是丢弃 —— 这正是
 *                              "受限的、可验证的运行时代码注入"的完整闭环。
 *
 * 模拟的 hook 点：net_recv_hook_point，每次"收到一个包”就调用挂在它上面
 * 的所有已验证程序，任何一个程序判定 DROP，这个包就被丢弃（模拟真实
 * XDP/tc-bpf 多程序链式短路的简化版本）。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -std=gnu11 -o hook_point_bpf_attach hook_point_bpf_attach.c
 * 运行：
 *   ./hook_point_bpf_attach
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_INSNS     16
#define MAX_ATTACHED  8

/* ------------------------------------------------------------------ */
/* 1. 契约：字节码指令 + 收包上下文                                     */
/* ------------------------------------------------------------------ */

enum op {
	OP_LOAD_LEN = 0,   /* R0 = pkt->len */
	OP_LOAD_PROTO,     /* R0 = pkt->proto */
	OP_LOAD_IMM,       /* R0 = imm */
	OP_CMP_EQ,         /* flag = (R0 == imm) */
	OP_CMP_GT,         /* flag = (R0 > imm) */
	OP_JMP,            /* cond==0 无条件跳；cond==1 flag==1 才跳 */
	OP_PASS,           /* 结束：放行（XDP_PASS 语义） */
	OP_DROP,           /* 结束：丢弃（XDP_DROP 语义） */
};

struct insn {
	uint8_t  op;
	uint8_t  cond;
	int32_t  imm;
	int32_t  target;
};

struct bpf_prog {
	char name[32];
	struct insn insns[MAX_INSNS];
	int len;
	int verified; /* 1 表示已通过验证；未验证的程序禁止 attach */
};

struct packet {
	int32_t len;
	int32_t proto; /* 6=TCP, 17=UDP, 1=ICMP，模拟简化的协议号 */
};

enum verdict { PASS = 0, DROP = 1 };

/* ------------------------------------------------------------------ */
/* 2. 验证器（复用 toy_bytecode_verifier.c 的思路：禁止后向跳转/越界）    */
/* ------------------------------------------------------------------ */

static int bpf_verify(struct bpf_prog *p)
{
	int i;

	if (p->len <= 0 || p->len > MAX_INSNS)
		return -1;

	for (i = 0; i < p->len; i++) {
		struct insn *ins = &p->insns[i];

		if (ins->op == OP_JMP) {
			if (ins->target < 0 || ins->target >= p->len || ins->target <= i)
				return -1; /* 越界或后向跳转，一律拒绝 */
		}
	}
	if (p->insns[p->len - 1].op != OP_PASS && p->insns[p->len - 1].op != OP_DROP)
		return -1;

	p->verified = 1;
	return 0;
}

static enum verdict bpf_run(const struct bpf_prog *p, const struct packet *pkt)
{
	int32_t r0 = 0;
	int flag = 0;
	int pc = 0;

	while (pc < p->len) {
		const struct insn *ins = &p->insns[pc];

		switch (ins->op) {
		case OP_LOAD_LEN:   r0 = pkt->len;   pc++; break;
		case OP_LOAD_PROTO: r0 = pkt->proto; pc++; break;
		case OP_LOAD_IMM:   r0 = ins->imm;   pc++; break;
		case OP_CMP_EQ:     flag = (r0 == ins->imm); pc++; break;
		case OP_CMP_GT:     flag = (r0 > ins->imm);  pc++; break;
		case OP_JMP:
			pc = (ins->cond == 0 || flag) ? ins->target : pc + 1;
			break;
		case OP_PASS: return PASS;
		case OP_DROP: return DROP;
		default:      return DROP; /* 防御性：未知指令一律丢弃 */
		}
	}
	return DROP;
}

/* ------------------------------------------------------------------ */
/* 3. Hook 点：内核里真实存在的"挂载位"，本身不认识任何具体字节码语义     */
/* ------------------------------------------------------------------ */

struct hook_point {
	const char *name;
	const struct bpf_prog *attached[MAX_ATTACHED];
	int count;
};

static struct hook_point net_recv_hook_point = { .name = "net_recv_hook_point" };

/*
 * bpf_attach() —— 对应真实内核的 bpf_prog_attach()/dev_change_xdp_fd()：
 * 只允许"已经验证过"的程序挂接，这是整条 DI 链路里最关键的一道闸门。
 */
static int bpf_attach(struct hook_point *hp, const struct bpf_prog *p)
{
	if (!p->verified) {
		fprintf(stderr, "[attach] refuse to attach '%s': not verified\n", p->name);
		return -1;
	}
	if (hp->count >= MAX_ATTACHED) {
		fprintf(stderr, "[attach] hook point '%s' is full\n", hp->name);
		return -1;
	}
	hp->attached[hp->count++] = p;
	printf("[attach] program '%s' attached to hook point '%s' (slot %d)\n",
	       p->name, hp->name, hp->count - 1);
	return 0;
}

static void bpf_detach(struct hook_point *hp, const struct bpf_prog *p)
{
	int i;

	for (i = 0; i < hp->count; i++) {
		if (hp->attached[i] == p) {
			hp->attached[i] = hp->attached[hp->count - 1];
			hp->attached[hp->count - 1] = NULL;
			hp->count--;
			printf("[detach] program '%s' detached from hook point '%s'\n",
			       p->name, hp->name);
			return;
		}
	}
}

/*
 * net_recv_hook_run() —— 模拟真实收包路径在关键节点调用挂载的 BPF 程序。
 * Core（这个函数）完全不知道挂载了什么程序、程序内部怎么判断，
 * 只认"所有挂载程序里只要有一个 DROP，这个包就被丢弃"这条统一契约。
 */
static enum verdict net_recv_hook_run(struct hook_point *hp, const struct packet *pkt)
{
	int i;

	for (i = 0; i < hp->count; i++) {
		enum verdict v = bpf_run(hp->attached[i], pkt);  /* <-- 盲调 */
		if (v == DROP)
			return DROP; /* 短路：任意一个程序判定丢弃即丢弃 */
	}
	return PASS;
}

/* ------------------------------------------------------------------ */
/* 4. 模拟收包主循环                                                    */
/* ------------------------------------------------------------------ */

static void simulate_packet(struct hook_point *hp, struct packet pkt, const char *desc)
{
	enum verdict v = net_recv_hook_run(hp, &pkt);
	printf("  [recv] pkt(len=%d, proto=%d) [%s] -> %s\n",
	       pkt.len, pkt.proto, desc, v == PASS ? "PASS" : "DROP");
}

int main(void)
{
	printf("=== hook_point_bpf_attach demo ===\n\n");

	/* 程序 A：丢弃所有 proto == 17 (UDP) 的包，其余放行（含 then/else 两个分支） */
	struct bpf_prog drop_udp = {
		.name = "drop_udp",
		.len = 5,
		.insns = {
			{ .op = OP_LOAD_PROTO },
			{ .op = OP_CMP_EQ, .imm = 17 },
			{ .op = OP_JMP,    .cond = 1, .target = 4 }, /* proto==17 -> DROP 分支 */
			{ .op = OP_PASS },
			{ .op = OP_DROP },
		},
	};

	/* 程序 B：丢弃所有长度超过 1400 字节的包（模拟丢弃巨帧） */
	struct bpf_prog drop_jumbo = {
		.name = "drop_jumbo",
		.len = 5,
		.insns = {
			{ .op = OP_LOAD_LEN },
			{ .op = OP_CMP_GT, .imm = 1400 },
			{ .op = OP_JMP,    .cond = 1, .target = 4 },
			{ .op = OP_PASS },
			{ .op = OP_DROP },
		},
	};

	/* 程序 C：一个"未验证"的恶意程序，试图直接 attach（应被拒绝） */
	struct bpf_prog unverified = {
		.name = "unverified_evil",
		.len = 1,
		.insns = { { .op = OP_DROP } },
		.verified = 0, /* 显式标记：从未经过 bpf_verify() */
	};

	printf("-- 验证并挂接 drop_udp --\n");
	if (bpf_verify(&drop_udp) == 0)
		bpf_attach(&net_recv_hook_point, &drop_udp);

	printf("\n-- 验证并挂接 drop_jumbo --\n");
	if (bpf_verify(&drop_jumbo) == 0)
		bpf_attach(&net_recv_hook_point, &drop_jumbo);

	printf("\n-- 尝试直接挂接未验证的程序（应被拒绝） --\n");
	bpf_attach(&net_recv_hook_point, &unverified);

	printf("\n-- 模拟收包，%d 个已验证程序同时生效 --\n", net_recv_hook_point.count);
	simulate_packet(&net_recv_hook_point, (struct packet){ .len = 512,  .proto = 6  }, "TCP, normal size");
	simulate_packet(&net_recv_hook_point, (struct packet){ .len = 512,  .proto = 17 }, "UDP, should be dropped by drop_udp");
	simulate_packet(&net_recv_hook_point, (struct packet){ .len = 9000, .proto = 6  }, "TCP, jumbo, should be dropped by drop_jumbo");
	simulate_packet(&net_recv_hook_point, (struct packet){ .len = 64,   .proto = 1  }, "ICMP, small, should pass");

	printf("\n-- detach drop_udp，之后 UDP 包应该重新放行 --\n");
	bpf_detach(&net_recv_hook_point, &drop_udp);
	simulate_packet(&net_recv_hook_point, (struct packet){ .len = 512, .proto = 17 }, "UDP, drop_udp already detached");

	printf("\n=== done ===\n");
	return 0;
}
