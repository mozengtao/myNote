/*
 * toy_bytecode_verifier.c
 *
 * 场景：模拟 BPF 最核心的思想 —— 用户态提交一段"字节码程序”，
 * 内核（这里用普通用户态代码模拟）先跑一遍"验证器”（verifier）做静态安全检查，
 * 只有验证通过的程序才允许被"加载”（load）进虚拟机执行。
 *
 * 这与 01 篇的 function pointer callback 最大的区别是：
 *
 *   01 篇：调用者信任回调实现者，回调是原生机器码，验证器不存在。
 *   本篇：调用者不信任提交者，提交的是"数据”（字节码），必须先跑
 *         静态分析证明"安全”，才能被解释执行 —— 相当于运行前加了
 *         一层沙箱契约检查。
 *
 * 极简指令集（只有 6 条指令，足够体现"验证 + 执行”两阶段）：
 *
 *   LOAD  dst, imm      : R[dst] = imm
 *   ADD   dst, src       : R[dst] += R[src]
 *   SUB   dst, src       : R[dst] -= R[src]
 *   CMP   dst, src       : flag = (R[dst] > R[src]) ? 1 : 0
 *   JMP   target, cond   : cond==0 时无条件跳转到 target；cond==1 时仅当 flag==1 才跳转
 *   RET   dst            : 程序结束，返回 R[dst]
 *
 * 验证器规则（对应真实 eBPF verifier 的三条核心检查）：
 *   1. 指令数量不能超过 MAX_INSNS（真实 kernel 里对应 BPF_COMPLEXITY_LIMIT_INSNS）
 *   2. 寄存器编号必须在 [0, NUM_REGS) 内（越界访问检查）
 *   3. 跳转目标必须在 [0, len) 内，且不允许"跳向自己或更早的指令”
 *      （不允许后向跳转 —— 这是最简单的"禁止无限循环”实现方式，
 *       真实 eBPF 在允许有界循环之前也是完全禁止后向跳转的）
 *   4. 程序必须以 RET 结尾（保证一定会终止）
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -std=gnu11 -o toy_bytecode_verifier toy_bytecode_verifier.c
 * 运行：
 *   ./toy_bytecode_verifier
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_INSNS 64
#define NUM_REGS  8

/* ------------------------------------------------------------------ */
/* 1. 契约（Contract）：字节码指令格式，内核/验证器/执行器三方都遵守它  */
/* ------------------------------------------------------------------ */

enum opcode {
	OP_LOAD = 0,
	OP_ADD,
	OP_SUB,
	OP_CMP,
	OP_JMP,
	OP_RET,
	OP_INVALID_FOR_TEST, /* 故意加一个非法 opcode，用于演示"未知指令"拒绝 */
};

struct insn {
	uint8_t  op;
	uint8_t  dst;
	uint8_t  src;
	uint8_t  cond;   /* 仅 JMP 用：0=无条件跳转，1=flag==1 才跳 */
	int32_t  imm;    /* LOAD 的立即数，JMP 的跳转目标（存在 imm 里） */
};

struct bpf_prog {
	const char *name;
	struct insn insns[MAX_INSNS];
	int len;
};

/* ------------------------------------------------------------------ */
/* 2. 验证器（Core 的一部分）：只依赖 insn 的字段布局，不关心"语义"      */
/* ------------------------------------------------------------------ */

enum verify_result {
	VERIFY_OK = 0,
	VERIFY_ERR_TOO_LONG,
	VERIFY_ERR_BAD_OPCODE,
	VERIFY_ERR_BAD_REG,
	VERIFY_ERR_BAD_JUMP_TARGET,
	VERIFY_ERR_BACKWARD_JUMP,
	VERIFY_ERR_NO_RET,
};

static const char *verify_err_str(enum verify_result r)
{
	switch (r) {
	case VERIFY_OK:                  return "OK";
	case VERIFY_ERR_TOO_LONG:        return "program too long (exceeds complexity limit)";
	case VERIFY_ERR_BAD_OPCODE:      return "unknown opcode";
	case VERIFY_ERR_BAD_REG:         return "register index out of range";
	case VERIFY_ERR_BAD_JUMP_TARGET: return "jump target out of bounds";
	case VERIFY_ERR_BACKWARD_JUMP:   return "backward jump rejected (would allow unbounded loop)";
	case VERIFY_ERR_NO_RET:          return "program does not end with RET";
	}
	return "unknown error";
}

/*
 * bpf_verify() —— 对应真实 kernel 的 bpf_check()/do_check()：
 * 静态扫描整段字节码，不执行任何一条指令，只做"这段程序是否可能作恶"的证明。
 */
static enum verify_result bpf_verify(const struct bpf_prog *p)
{
	int i;

	if (p->len <= 0 || p->len > MAX_INSNS)
		return VERIFY_ERR_TOO_LONG;

	for (i = 0; i < p->len; i++) {
		const struct insn *ins = &p->insns[i];

		switch (ins->op) {
		case OP_LOAD:
			if (ins->dst >= NUM_REGS)
				return VERIFY_ERR_BAD_REG;
			break;
		case OP_ADD:
		case OP_SUB:
		case OP_CMP:
			if (ins->dst >= NUM_REGS || ins->src >= NUM_REGS)
				return VERIFY_ERR_BAD_REG;
			break;
		case OP_JMP:
			if (ins->imm < 0 || ins->imm >= p->len)
				return VERIFY_ERR_BAD_JUMP_TARGET;
			/* 核心安全规则：禁止后向/自身跳转，杜绝无限循环 */
			if (ins->imm <= i)
				return VERIFY_ERR_BACKWARD_JUMP;
			break;
		case OP_RET:
			if (ins->dst >= NUM_REGS)
				return VERIFY_ERR_BAD_REG;
			break;
		default:
			return VERIFY_ERR_BAD_OPCODE;
		}
	}

	if (p->insns[p->len - 1].op != OP_RET)
		return VERIFY_ERR_NO_RET;

	return VERIFY_OK;
}

/* ------------------------------------------------------------------ */
/* 3. 执行器（Core 的另一部分）：只在验证通过后才会被调用                */
/* ------------------------------------------------------------------ */

static int32_t bpf_run(const struct bpf_prog *p, int32_t *out)
{
	int32_t regs[NUM_REGS] = {0};
	int flag = 0;
	int pc = 0;
	int steps = 0;

	/*
	 * 因为验证器已经禁止了后向跳转，pc 严格单调不减，
	 * 所以这里的循环上限本质上是 p->len —— 不可能真正死循环，
	 * 这行 steps 计数只是"运行期再验证一次"的防御性编程（belt and suspenders）。
	 */
	while (pc < p->len) {
		const struct insn *ins = &p->insns[pc];

		if (++steps > MAX_INSNS * 2) {
			fprintf(stderr, "[bpf_run] runtime step limit exceeded, aborting (should never happen post-verify)\n");
			return -1;
		}

		switch (ins->op) {
		case OP_LOAD:
			regs[ins->dst] = ins->imm;
			pc++;
			break;
		case OP_ADD:
			regs[ins->dst] += regs[ins->src];
			pc++;
			break;
		case OP_SUB:
			regs[ins->dst] -= regs[ins->src];
			pc++;
			break;
		case OP_CMP:
			flag = (regs[ins->dst] > regs[ins->src]) ? 1 : 0;
			pc++;
			break;
		case OP_JMP:
			if (ins->cond == 0 || flag == 1)
				pc = ins->imm;
			else
				pc++;
			break;
		case OP_RET:
			*out = regs[ins->dst];
			return 0;
		default:
			return -1; /* 不可达：验证器已经排除了这种情况 */
		}
	}

	return -1; /* 不可达：验证器保证最后一条是 RET */
}

/*
 * bpf_load_and_run() —— 对应真实 kernel 的 sys_bpf(BPF_PROG_LOAD, ...)：
 * 先验证，验证不通过直接拒绝，绝不会把未验证的字节码交给执行器。
 */
static void bpf_load_and_run(const struct bpf_prog *p)
{
	enum verify_result vr = bpf_verify(p);

	printf("[verifier] checking program '%s' (%d insns)... ", p->name, p->len);
	if (vr != VERIFY_OK) {
		printf("REJECTED: %s\n", verify_err_str(vr));
		return;
	}
	printf("PASSED\n");

	int32_t result = 0;
	if (bpf_run(p, &result) == 0)
		printf("  [executor] program ran, return value = %d\n", result);
	else
		printf("  [executor] internal error (unreachable if verifier is correct)\n");
}

/* ------------------------------------------------------------------ */
/* 4. 若干测试程序：合法的 + 各种应该被拒绝的恶意/错误程序               */
/* ------------------------------------------------------------------ */

int main(void)
{
	printf("=== toy_bytecode_verifier demo ===\n\n");

	/* 程序 1：合法 —— 计算 (10 + 32) - 7 = 35 */
	struct bpf_prog good_math = {
		.name = "good_math (compute (10+32)-7)",
		.len = 6,
		.insns = {
			{ .op = OP_LOAD, .dst = 0, .imm = 10 },
			{ .op = OP_LOAD, .dst = 1, .imm = 32 },
			{ .op = OP_ADD,  .dst = 0, .src = 1 },
			{ .op = OP_LOAD, .dst = 1, .imm = 7 },
			{ .op = OP_SUB,  .dst = 0, .src = 1 },
			{ .op = OP_RET,  .dst = 0 },
		},
	};
	bpf_load_and_run(&good_math);

	/* 程序 2：合法 —— 用条件跳转实现 "if (5 > 3) return 100 else return 200"：
	 *   idx0: LOAD r0, 5
	 *   idx1: LOAD r1, 3
	 *   idx2: CMP  r0, r1        ; flag = (5 > 3) = 1
	 *   idx3: JMP  cond=1, ->6   ; flag==1，跳到 idx6 (then 分支)
	 *   idx4: LOAD r2, 200       ; else 分支
	 *   idx5: RET  r2
	 *   idx6: LOAD r2, 100       ; then 分支
	 *   idx7: RET  r2
	 */
	struct bpf_prog branch_final = {
		.name = "branch_final (if 5>3 return 100 else return 200)",
		.len = 8,
		.insns = {
			{ .op = OP_LOAD, .dst = 0, .imm = 5 },
			{ .op = OP_LOAD, .dst = 1, .imm = 3 },
			{ .op = OP_CMP,  .dst = 0, .src = 1 },
			{ .op = OP_JMP,  .cond = 1, .imm = 6 },
			{ .op = OP_LOAD, .dst = 2, .imm = 200 },
			{ .op = OP_RET,  .dst = 2 },
			{ .op = OP_LOAD, .dst = 2, .imm = 100 },
			{ .op = OP_RET,  .dst = 2 },
		},
	};
	bpf_load_and_run(&branch_final);

	/* 程序 3：恶意 —— 后向跳转试图构造无限循环 */
	struct bpf_prog evil_loop = {
		.name = "evil_loop (backward jump, infinite loop attempt)",
		.len = 3,
		.insns = {
			{ .op = OP_LOAD, .dst = 0, .imm = 1 },
			{ .op = OP_ADD,  .dst = 0, .src = 0 },
			{ .op = OP_JMP,  .cond = 0, .imm = 1 }, /* 跳回 index 1，永远循环 */
		},
	};
	bpf_load_and_run(&evil_loop);

	/* 程序 4：恶意 —— 寄存器越界访问（试图读一个不存在的寄存器） */
	struct bpf_prog evil_reg = {
		.name = "evil_reg (out-of-range register access)",
		.len = 2,
		.insns = {
			{ .op = OP_LOAD, .dst = 200, .imm = 42 }, /* 200 号寄存器不存在 */
			{ .op = OP_RET,  .dst = 200 },
		},
	};
	bpf_load_and_run(&evil_reg);

	/* 程序 5：恶意 —— 跳转目标越界（跳出程序范围） */
	struct bpf_prog evil_jump = {
		.name = "evil_jump (jump target out of program bounds)",
		.len = 2,
		.insns = {
			{ .op = OP_JMP, .cond = 0, .imm = 999 }, /* 跳到不存在的 index 999 */
			{ .op = OP_RET, .dst = 0 },
		},
	};
	bpf_load_and_run(&evil_jump);

	/* 程序 6：错误 —— 忘了写 RET，程序不保证终止 */
	struct bpf_prog no_ret = {
		.name = "no_ret (missing terminating RET)",
		.len = 2,
		.insns = {
			{ .op = OP_LOAD, .dst = 0, .imm = 1 },
			{ .op = OP_ADD,  .dst = 0, .src = 0 },
		},
	};
	bpf_load_and_run(&no_ret);

	/* 程序 7：错误 —— 未知 opcode（模拟内核版本不识别的新指令） */
	struct bpf_prog bad_op = {
		.name = "bad_op (unrecognized opcode)",
		.len = 1,
		.insns = {
			{ .op = OP_INVALID_FOR_TEST, .dst = 0 },
		},
	};
	bpf_load_and_run(&bad_op);

	/* 程序 8：超长程序，触发复杂度上限 */
	struct bpf_prog too_long;
	too_long.name = "too_long (exceeds MAX_INSNS complexity limit)";
	too_long.len = MAX_INSNS + 1; /* 故意超过数组容量对应的上限判断 */
	memset(too_long.insns, 0, sizeof(too_long.insns));
	bpf_load_and_run(&too_long);

	printf("\n=== done ===\n");
	return 0;
}
