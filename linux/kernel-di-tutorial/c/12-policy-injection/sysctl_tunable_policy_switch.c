/*
 * sysctl_tunable_policy_switch.c
 *
 * 场景：模拟通过 /proc/sys 风格的运行期接口，在多个已注册策略之间热切换
 * "当前生效策略"，验证切换前后行为差异——这是 Policy Injection 最典型的
 * 用户可见形态。
 *
 * 真实对照：
 *   - /proc/sys/net/ipv4/tcp_congestion_control 的写入路径
 *     proc_tcp_congestion_control() 见 net/ipv4/sysctl_net_ipv4.c:122，
 *     最终调用 tcp_set_default_congestion_control()（net/ipv4/tcp_cong.c:111）
 *     按名字在 tcp_cong_list 上查找，找不到返回 -ENOENT。
 *   - /sys/block/<dev>/queue/scheduler 的写入路径
 *     elv_iosched_store() 见 block/elevator.c:1038，调用 elevator_change()
 *     （block/elevator.c:1014）按名字查找 elv_list，找不到打印
 *     "elevator: switch to %s failed"。
 *
 * 两者的共同结构是："写入一个策略名字符串" -> "在注册表里按名字查找" ->
 * "找到则原子替换当前生效指针；找不到则返回错误，保留旧策略生效"。
 * 本文件用一个通用化的"负载均衡策略"场景复刻这个通用结构：
 * round-robin / least-connections / client-hash（会话保持）三种策略，
 * 通过一个模拟的 sysctl 写入函数运行期切换。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o sysctl_tunable_policy_switch sysctl_tunable_policy_switch.c
 * 运行：./sysctl_tunable_policy_switch
 */

#include <stdio.h>
#include <string.h>

#define NR_SERVERS	4
#define MAX_POLICIES	8
#define POLICY_NAME_MAX	32

struct server {
	int id;
	int active_conns;
};

static struct server g_pool[NR_SERVERS];

/* ------------------------------------------------------------------ */
/* 契约：core（dispatch_request）只认识这份 ops                        */
/* ------------------------------------------------------------------ */
struct lb_policy {
	char name[POLICY_NAME_MAX];
	int  (*pick_server)(int client_id);	/* 返回选中的 server index */
};

static struct lb_policy g_registry[MAX_POLICIES];
static int g_registry_count;
static struct lb_policy *g_active_policy;	/* 对应真实 icsk_ca_ops / q->elevator */

/* ==================================================================== */
/* 策略 A：round-robin —— 轮询，不关心连接数、不关心 client 身份           */
/* ==================================================================== */
static int rr_cursor;

static int rr_pick_server(int client_id)
{
	(void)client_id;
	int idx = rr_cursor % NR_SERVERS;
	rr_cursor++;
	return idx;
}

/* ==================================================================== */
/* 策略 B：least-connections —— 总是挑当前活跃连接数最少的 server          */
/* ==================================================================== */
static int lc_pick_server(int client_id)
{
	int i, best = 0;

	(void)client_id;
	for (i = 1; i < NR_SERVERS; i++) {
		if (g_pool[i].active_conns < g_pool[best].active_conns)
			best = i;
	}
	return best;
}

/* ==================================================================== */
/* 策略 C：client-hash —— 同一个 client 永远落在同一台 server（会话保持）  */
/* ==================================================================== */
static int hash_pick_server(int client_id)
{
	/* 简单乘法哈希，足够在演示里体现"确定性映射"这一性质 */
	unsigned int h = (unsigned int)client_id * 2654435761u;
	return (int)(h % NR_SERVERS);
}

/* ------------------------------------------------------------------ */
/* Core：注册表 + 切换 + 查找，对应 tcp_register_congestion_control()/    */
/* tcp_set_default_congestion_control() 或 elv_register()/elevator_change() */
/* ------------------------------------------------------------------ */

static void register_policy(const char *name, int (*fn)(int))
{
	strncpy(g_registry[g_registry_count].name, name, POLICY_NAME_MAX - 1);
	g_registry[g_registry_count].name[POLICY_NAME_MAX - 1] = '\0';
	g_registry[g_registry_count].pick_server = fn;
	g_registry_count++;
	printf("[register] load-balance policy '%s' registered\n", name);
}

static struct lb_policy *find_policy(const char *name)
{
	int i;

	for (i = 0; i < g_registry_count; i++) {
		if (strcmp(g_registry[i].name, name) == 0)
			return &g_registry[i];
	}
	return NULL;
}

/*
 * 模拟对 /proc/sys/net/lb/policy 的一次 write(2)。
 * 对应真实 proc_tcp_congestion_control() 的行为：先在注册表里查找，
 * 找不到就返回错误、不改变当前生效策略；找到了才原子替换。
 */
static int proc_write_policy(const char *buf)
{
	struct lb_policy *p = find_policy(buf);

	if (!p) {
		fprintf(stderr, "[proc_write] write '%s' to policy file -> -ENOENT (unknown policy, kept '%s')\n",
			buf, g_active_policy->name);
		return -2;	/* 模拟 -ENOENT */
	}
	printf("[proc_write] write '%s' to policy file -> switched active policy: '%s' -> '%s'\n",
	       buf, g_active_policy->name, p->name);
	g_active_policy = p;
	return 0;
}

/* 模拟对同一个文件的 read(2)，返回当前生效策略名字（类似 elv_iosched_show()） */
static const char *proc_read_policy(void)
{
	return g_active_policy->name;
}

/* Core 的分发路径：完全不知道当前策略具体怎么选服务器，只管盲调 */
static void dispatch_request(int client_id)
{
	int idx = g_active_policy->pick_server(client_id);	/* <-- 盲调 */

	g_pool[idx].active_conns++;
	printf("  request(client=%2d) -> server[%d] (active_conns now=%d)\n",
	       client_id, idx, g_pool[idx].active_conns);
}

static void print_pool_state(const char *label)
{
	int i;

	printf("  -- pool state (%s): ", label);
	for (i = 0; i < NR_SERVERS; i++)
		printf("s%d=%d ", g_pool[i].id, g_pool[i].active_conns);
	printf("\n");
}

int main(void)
{
	int i, clients_batch1[6] = { 1, 2, 3, 4, 5, 6 };
	int clients_batch3[6] = { 101, 202, 101, 202, 101, 202 };

	printf("=== sysctl_tunable_policy_switch demo ===\n\n");

	for (i = 0; i < NR_SERVERS; i++) {
		g_pool[i].id = i;
		g_pool[i].active_conns = 0;
	}
	/* 故意让初始负载不均匀，凸显 least-connections 与 round-robin 的差异 */
	g_pool[0].active_conns = 5;
	g_pool[1].active_conns = 1;
	g_pool[2].active_conns = 3;
	g_pool[3].active_conns = 0;

	register_policy("round-robin", rr_pick_server);
	register_policy("least-conn", lc_pick_server);
	register_policy("client-hash", hash_pick_server);

	g_active_policy = find_policy("round-robin");	/* 默认策略，对应 CONFIG_DEFAULT_IOSCHED */
	printf("\n当前生效策略（读 proc 文件）：%s\n", proc_read_policy());

	printf("\n-- 阶段1：round-robin 策略下派发 6 个请求（忽略初始不均匀负载） --\n");
	print_pool_state("before");
	for (i = 0; i < 6; i++)
		dispatch_request(clients_batch1[i]);
	print_pool_state("after");

	printf("\n-- 阶段2：写 sysctl 切换到 least-conn，再派发 6 个请求 --\n");
	proc_write_policy("least-conn");
	print_pool_state("before");
	for (i = 0; i < 6; i++)
		dispatch_request(clients_batch1[i]);
	print_pool_state("after");

	printf("\n-- 阶段3：尝试写一个不存在的策略名（应被拒绝，当前策略保持不变） --\n");
	proc_write_policy("weighted-fair-queue");
	printf("当前生效策略（读 proc 文件）：%s\n", proc_read_policy());

	printf("\n-- 阶段4：切换到 client-hash，验证同一 client 永远落到同一 server --\n");
	proc_write_policy("client-hash");
	for (i = 0; i < 6; i++)
		dispatch_request(clients_batch3[i]);

	printf("\n=== done ===\n");
	printf("说明：dispatch_request()/proc_write_policy()（Core）从头到尾没有一行\n"
	       "      if (strcmp(name,\"round-robin\")==0) 这样的分支，全部通过\n"
	       "      g_active_policy->pick_server() 盲调；round-robin 均匀轮询，\n"
	       "      least-conn 持续填补负载最低的 server，client-hash 让同一个\n"
	       "      client_id（101/202）在阶段4里始终落在同一个 server——三种\n"
	       "      完全不同的行为，Core 代码却一行没变。\n");
	return 0;
}
