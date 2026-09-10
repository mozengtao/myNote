/*
 * congestion_control_policy.c
 *
 * 场景：模拟 Linux TCP 拥塞控制算法的可插拔策略机制。
 * 真实定义：
 *   - struct tcp_congestion_ops         见 include/net/tcp.h:689
 *   - tcp_register_congestion_control() 见 net/ipv4/tcp_cong.c:38
 *   - tcp_init_congestion_control()     见 net/ipv4/tcp_cong.c:78
 *   - tcp_set_congestion_control()      见 net/ipv4/tcp_cong.c:236（对应
 *     /proc/sys/net/ipv4/tcp_congestion_control sysctl 写入路径，
 *     实现见 net/ipv4/sysctl_net_ipv4.c:122 proc_tcp_congestion_control()）
 *   - tcp_reno_ssthresh()/tcp_reno_cong_avoid()/struct tcp_reno
 *     见 net/ipv4/tcp_cong.c:389,355(附近),404
 *   - CUBIC 窗口增长公式（W(t) = C*(t-K)^3 + Wmax）的真实实现
 *     见 net/ipv4/tcp_cubic.c:205 bictcp_update()
 *
 * 本文件用浮点数简化复刻两种真实存在于本仓库的拥塞控制算法的核心增长曲线：
 *   - reno：慢启动指数增长 + 拥塞避免阶段每 RTT 加性增长 1（AIMD）。
 *   - cubic：丢包后基于三次函数 W(t)=C*(t-K)^3+Wmax 增长——增长曲线先"平台"
 *     （逼近上次丢包时的窗口 Wmax，增长很慢），后"加速"（离开平台后凸增长），
 *     这正是 CUBIC 相比 reno 线性增长最大的形态差异。
 *   注意：真实 tcp_cubic.c 用整数定点运算（BICTCP_HZ 位移、tcp_time_stamp
 *   以 jiffies 为单位）实现，这里用浮点数、以"回合数"代替真实时间，是为了
 *   在用户态清晰复现"形状"而不是逐位复刻定点数值——这是本例明确声明的简化。
 *
 * 核心思想：tcp_init_congestion_control()/tcp_cong_avoid()（Core）只认识
 * struct cong_ops 契约（ssthresh/cong_avoid 两个函数指针），完全不知道
 * cwnd 具体怎么长——这就是 Policy Injection 在网络协议栈里的应用。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o congestion_control_policy congestion_control_policy.c -lm
 * 运行：./congestion_control_policy
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#define LOSS_TRIGGER_CWND	32.0	/* 慢启动阶段 cwnd 达到该值时模拟一次丢包 */
#define TOTAL_ROUNDS		18

/* 一条 TCP 连接的可变状态，类似简化版 struct tcp_sock */
struct tcp_conn {
	double cwnd;		/* 拥塞窗口 */
	double ssthresh;	/* 慢启动门限 */
	int    in_slow_start;
	long   round;		/* 当前 RTT 轮次 */
	/* cubic 私有状态（对应真实 struct bictcp） */
	double wmax;		/* 上次丢包时的窗口，即 last_max_cwnd */
	long   epoch_start;	/* 本次丢包后的起始轮次 */
	double bic_k;
	int    loss_triggered;	/* 本次演示只模拟一次丢包，避免重复触发 */
};

/* ------------------------------------------------------------------ */
/* 契约：core（tcp_cong_avoid()）只认识这份 ops                        */
/* 对应真实的 struct tcp_congestion_ops（include/net/tcp.h:689）        */
/* ------------------------------------------------------------------ */
struct cong_ops {
	const char *name;
	void   (*init)(struct tcp_conn *sk);
	double (*ssthresh)(struct tcp_conn *sk);	/* 丢包时新的门限 */
	void   (*cong_avoid)(struct tcp_conn *sk);	/* 每个 round 调整一次 cwnd */
};

/* ==================================================================== */
/* 策略 A：reno —— 对应 tcp_reno_cong_avoid()/tcp_reno_ssthresh()         */
/* ==================================================================== */

static void reno_init(struct tcp_conn *sk)
{
	sk->cwnd = 2.0;
	sk->ssthresh = 1e9;	/* 初始门限视为无穷大，等价真实实现里的默认值 */
	sk->in_slow_start = 1;
	sk->loss_triggered = 0;
}

/* 对应 net/ipv4/tcp_cong.c:389 tcp_reno_ssthresh()：门限 = 当前窗口的一半 */
static double reno_ssthresh(struct tcp_conn *sk)
{
	return sk->cwnd / 2.0 > 2.0 ? sk->cwnd / 2.0 : 2.0;
}

/* 对应 tcp_reno_cong_avoid()：慢启动指数增长，拥塞避免线性（加性）增长 */
static void reno_cong_avoid(struct tcp_conn *sk)
{
	if (sk->in_slow_start && sk->cwnd < sk->ssthresh) {
		sk->cwnd *= 2.0;	/* 每 RTT 翻倍 */
	} else {
		sk->in_slow_start = 0;
		sk->cwnd += 1.0;	/* 每 RTT 加 1（AIMD 的 "AI" 部分） */
	}
}

static struct cong_ops reno_ops = {
	.name       = "reno",
	.init       = reno_init,
	.ssthresh   = reno_ssthresh,
	.cong_avoid = reno_cong_avoid,
};

/* ==================================================================== */
/* 策略 B：cubic —— 对应 net/ipv4/tcp_cubic.c bictcp_update() 的简化浮点版 */
/* ==================================================================== */

#define CUBIC_C		0.4	/* 对应真实 bic_scale 换算后的比例常数 */
#define CUBIC_BETA	0.7	/* 丢包后的窗口保留比例（乘性减小系数） */

static void cubic_init(struct tcp_conn *sk)
{
	sk->cwnd = 2.0;
	sk->ssthresh = 1e9;
	sk->in_slow_start = 1;
	sk->wmax = 0.0;
	sk->epoch_start = 0;
	sk->bic_k = 0.0;
	sk->loss_triggered = 0;
}

/* 对应真实 cubic 丢包处理：Wmax=当前cwnd，新 cwnd = Wmax * beta */
static double cubic_ssthresh(struct tcp_conn *sk)
{
	sk->wmax = sk->cwnd;
	sk->epoch_start = sk->round;
	/* 对应 bictcp_update() 里的 bic_K 计算：
	 * K = cbrt((Wmax - beta*Wmax) / C) = cbrt(Wmax*(1-beta)/C) */
	sk->bic_k = cbrt(sk->wmax * (1.0 - CUBIC_BETA) / CUBIC_C);
	return sk->wmax * CUBIC_BETA;
}

/* 对应 bictcp_update()：W(t) = C*(t-K)^3 + Wmax，t 为丢包后经过的轮次 */
static void cubic_cong_avoid(struct tcp_conn *sk)
{
	double t, target;

	if (sk->in_slow_start && sk->cwnd < sk->ssthresh) {
		sk->cwnd *= 2.0;
		return;
	}
	sk->in_slow_start = 0;

	t = (double)(sk->round - sk->epoch_start);
	target = CUBIC_C * pow(t - sk->bic_k, 3.0) + sk->wmax;
	if (target < sk->cwnd + 1.0 / sk->cwnd)
		target = sk->cwnd + 1.0 / sk->cwnd;	/* 保证至少有 TCP-friendly 的最小增长 */
	sk->cwnd = target;
}

static struct cong_ops cubic_ops = {
	.name       = "cubic",
	.init       = cubic_init,
	.ssthresh   = cubic_ssthresh,
	.cong_avoid = cubic_cong_avoid,
};

/* ==================================================================== */
/* Core：对应 tcp_init_congestion_control()/tcp_cong_avoid_ai() 的骨架     */
/* ==================================================================== */

/* 对应真实的 icsk->icsk_ca_ops 指针（inet_connection_sock 内嵌字段） */
static void run_policy(struct cong_ops *policy, double *history_out)
{
	struct tcp_conn sk;
	long r;

	policy->init(&sk);	/* 对应 tcp_init_congestion_control() */
	sk.round = 0;

	for (r = 0; r < TOTAL_ROUNDS; r++) {
		sk.round = r;

		/* 慢启动阶段达到触发阈值 -> 模拟一次丢包（core 检测到，不知道具体策略） */
		if (!sk.loss_triggered && sk.cwnd >= LOSS_TRIGGER_CWND) {
			sk.loss_triggered = 1;
			double new_ssthresh = policy->ssthresh(&sk);	/* <-- 盲调 */
			printf("  [%-6s] round %2ld: LOSS EVENT, cwnd %.2f -> ssthresh %.2f\n",
			       policy->name, r, sk.cwnd, new_ssthresh);
			sk.cwnd = new_ssthresh;
			sk.ssthresh = new_ssthresh;
		}

		policy->cong_avoid(&sk);	/* <-- 盲调，core 不知道 cwnd 具体怎么变 */
		history_out[r] = sk.cwnd;
	}
}

int main(void)
{
	double reno_hist[TOTAL_ROUNDS], cubic_hist[TOTAL_ROUNDS];
	long r;

	printf("=== congestion_control_policy demo ===\n\n");

	printf("-- 运行 reno 策略 %d 个 RTT 轮次 --\n", TOTAL_ROUNDS);
	run_policy(&reno_ops, reno_hist);

	printf("\n-- 运行 cubic 策略 %d 个 RTT 轮次 --\n", TOTAL_ROUNDS);
	run_policy(&cubic_ops, cubic_hist);

	printf("\n-- 窗口增长曲线对比（同样触发一次丢包，之后的增长形状完全不同）--\n");
	printf("  round |   reno cwnd | cubic cwnd\n");
	printf("  ------+-------------+-----------\n");
	for (r = 0; r < TOTAL_ROUNDS; r++)
		printf("  %5ld | %11.2f | %10.2f\n", r, reno_hist[r], cubic_hist[r]);

	printf("\n=== done ===\n");
	printf("说明：两种策略共享同一套 run_policy()/cong_avoid 调用骨架（Core），\n"
	       "      Core 从未 #include 任何具体算法的头文件；reno 丢包后是严格的\n"
	       "      直线爬升（每轮 +1），cubic 丢包后先在 Wmax 附近爬升缓慢（\"平台\"），\n"
	       "      随后凸起加速——这正是 CUBIC 相对 reno 的设计目标：既能快速逼近\n"
	       "      上次的可用带宽，又能在探索新带宽时加速增长。\n");
	return 0;
}
