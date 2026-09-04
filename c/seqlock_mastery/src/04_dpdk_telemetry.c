/*
 * 04_dpdk_telemetry.c
 *
 * 示例 4：DPDK 风格 64 字节缓存行对齐网络遥测统计系统（Fast-Path vs Slow-Path CLI）
 *
 * DPDK 核心设计哲学深度复刻：
 * 1. 缓存行对齐（Cache Line Alignment, 64 字节）：
 *    - 避免伪共享（False Sharing）：将 seqcount 与统计数据结构体按照 64 字节强制对齐，
 *      杜绝数据面与管理面、或多个不同端口之间的缓存行交叉失效。
 * 2. 读写不对称性（Asymmetric Performance）：
 *    - 快路径（Fast-Path / Dataplane）：单核独占处理 100GbE 报文（67.2ns 极限制约），
 *      绝对不能容忍任何互斥锁、系统调用或导致 CPU 流水线清空的重型指令。
 *      Seqlock 仅需两条极轻量的序列号递增与内存屏障，开销几乎为零。
 *    - 慢路径（Slow-Path / Telemetry CLI）：每隔 100ms ~ 1s 通过 CLI 或 REST API 轮询一次，
 *      慢路径即使遇到写者并发而发生几次微秒级重试，也对系统整体吞吐量毫无负面影响。
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "../include/seqlock.h"

#define RTE_CACHE_LINE_SIZE 64
#define __rte_cache_aligned __attribute__((aligned(RTE_CACHE_LINE_SIZE)))

/*
 * DPDK 风格网络端口核心统计结构体（强制 64 字节对齐）
 */
typedef struct {
	uint64_t rx_packets;
	uint64_t tx_packets;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint64_t rx_drops;
	uint64_t tx_drops;
	uint64_t rx_errors;
	uint64_t tx_errors;
} __rte_cache_aligned dpdk_port_stats_t;

/*
 * 端口遥测通道：seqlock 与 stats 分别位于独立的 Cacheline
 */
typedef struct {
	seqcount_t seq __rte_cache_aligned;
	dpdk_port_stats_t stats __rte_cache_aligned;
} __rte_cache_aligned dpdk_port_telemetry_t;

static volatile int g_running = 1;

/*
 * 获取纳秒级单调时钟
 */
static inline uint64_t
get_monotonic_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/*
 * 快路径（Dataplane）：批量报文处理后的统计更新
 */
static inline void
dpdk_stats_update(dpdk_port_telemetry_t *port,
		  uint64_t rx_pkts, uint64_t rx_bytes,
		  uint64_t tx_pkts, uint64_t tx_bytes,
		  uint64_t drops)
{
	/* 开启写入标记：seq++ (奇数) + Release Fence */
	write_seqbegin(&port->seq);

	port->stats.rx_packets += rx_pkts;
	port->stats.rx_bytes += rx_bytes;
	port->stats.tx_packets += tx_pkts;
	port->stats.tx_bytes += tx_bytes;
	port->stats.rx_drops += drops;

	/* 结束写入标记：seq++ (偶数) + Release Store */
	write_seqend(&port->seq);
}

/*
 * 慢路径（Management CLI / Telemetry）：获取一致性端口统计快照
 */
static inline uint32_t
dpdk_stats_get(const dpdk_port_telemetry_t *port,
	       dpdk_port_stats_t *out_stats,
	       uint32_t *retry_counter)
{
	uint32_t seq;
	uint32_t retries = 0;

	for (;;) {
		seq = read_seqbegin(&port->seq);

		/* 局部内存拷贝 */
		*out_stats = port->stats;

		if (!read_seqretry(&port->seq, seq))
			break;

		retries++;
		seqlock_cpu_relax();
	}

	if (retry_counter)
		*retry_counter += retries;

	return seq;
}

/*
 * 数据面工作线程（模拟 DPDK 轮询驱动模式 PMD lcore）
 */
static void *
dataplane_lcore_func(void *arg)
{
	dpdk_port_telemetry_t *port = (dpdk_port_telemetry_t *)arg;
	uint64_t burst_count = 0;

	printf("[Dataplane lcore] 启动，进入 100GbE 极速数据包收发循环...\n");

	while (g_running) {
		burst_count++;

		/*
		 * 模拟收发突发报文（Burst = 32）
		 * 以太网帧最小 64 字节，平均 800 字节
		 */
		dpdk_stats_update(port,
				  32, 32 * 800,
				  32, 32 * 800,
				  (burst_count % 1000 == 0) ? 1 : 0);

		/* 模拟微小处理时延 */
		if ((burst_count % 50000) == 0)
			usleep(50);
	}

	printf("[Dataplane lcore] 退出，累计处理收发 Burst: %lu 次\n", burst_count);
	return NULL;
}

/*
 * 管理面遥测采集线程（模拟 dpdk-telemetry CLI 客户端）
 */
static void *
telemetry_cli_func(void *arg)
{
	dpdk_port_telemetry_t *port = (dpdk_port_telemetry_t *)arg;
	dpdk_port_stats_t snap;
	dpdk_port_stats_t prev = {0};
	uint32_t total_retries = 0;
	uint64_t sample_idx = 0;
	uint64_t last_time = get_monotonic_ns();

	printf("[Telemetry CLI] 启动，每隔 500ms 抓取一次端口状态...\n");

	while (g_running) {
		usleep(500000); /* 500 毫秒采样周期 */
		if (!g_running)
			break;

		sample_idx++;
		uint32_t round_retries = 0;
		dpdk_stats_get(port, &snap, &round_retries);
		total_retries += round_retries;

		uint64_t now = get_monotonic_ns();
		double delta_sec = (double)(now - last_time) / 1000000000.0;
		last_time = now;

		double rx_pps = (double)(snap.rx_packets - prev.rx_packets) / delta_sec;
		double rx_mbps = ((double)(snap.rx_bytes - prev.rx_bytes) * 8.0) / (delta_sec * 1000000.0);

		/* 校验最小以太网报文大小恒等式：bytes >= packets * 64 */
		if (snap.rx_bytes < snap.rx_packets * 64) {
			fprintf(stderr, "[严重告警] 观测到非法报文比例 (可能数据撕裂)! pkts=%lu, bytes=%lu\n",
				snap.rx_packets, snap.rx_bytes);
		}

		printf("+----------------------------------------------------------------------+\n");
		printf("| [Port 0 Telemetry] 采样点 #%-5lu (重试冲突: %u 次)                   |\n",
			sample_idx, round_retries);
		printf("+----------------------------------------------------------------------+\n");
		printf("| RX 报文数: %15lu | RX 吞吐速率: %10.2f pps             |\n", snap.rx_packets, rx_pps);
		printf("| RX 字节数: %15lu | RX 带宽速率: %10.2f Mbps            |\n", snap.rx_bytes, rx_mbps);
		printf("| TX 报文数: %15lu | TX 字节数:   %15lu       |\n", snap.tx_packets, snap.tx_bytes);
		printf("| RX 丢包数: %15lu | RX 错误数:   %15lu       |\n", snap.rx_drops, snap.rx_errors);
		printf("+----------------------------------------------------------------------+\n\n");

		prev = snap;
	}

	printf("[Telemetry CLI] 退出，全生命周期总重试冲突: %u 次\n", total_retries);
	return NULL;
}

/*
 * 主函数
 */
int
main(void)
{
	pthread_t dataplane_tid;
	pthread_t telemetry_tid;
	dpdk_port_telemetry_t port_channel;

	printf("======================================================================\n");
	printf("  示例 4：DPDK 风格 64 字节缓存行对齐网络遥测系统（Fast-Path 解耦） \n");
	printf("======================================================================\n");

	/* 验证缓存行对齐 */
	printf("[内存布局验证] Cacheline 大小: %d 字节\n", RTE_CACHE_LINE_SIZE);
	printf("[内存布局验证] seq 地址:   %p (对齐偏移: %lu)\n",
		(void *)&port_channel.seq, (uintptr_t)&port_channel.seq % RTE_CACHE_LINE_SIZE);
	printf("[内存布局验证] stats 地址: %p (对齐偏移: %lu)\n",
		(void *)&port_channel.stats, (uintptr_t)&port_channel.stats % RTE_CACHE_LINE_SIZE);

	if (((uintptr_t)&port_channel.seq % RTE_CACHE_LINE_SIZE) != 0 ||
	    ((uintptr_t)&port_channel.stats % RTE_CACHE_LINE_SIZE) != 0) {
		fprintf(stderr, "[警告] 缓存行未完全对齐!\n");
	} else {
		printf("[内存布局验证] 完美对齐！seqcount 与 stats 绝对不会发生伪共享冲突。\n");
	}

	/* 初始化 */
	seqcount_init(&port_channel.seq);
	memset(&port_channel.stats, 0, sizeof(port_channel.stats));

	pthread_create(&dataplane_tid, NULL, dataplane_lcore_func, &port_channel);
	pthread_create(&telemetry_tid, NULL, telemetry_cli_func, &port_channel);

	/* 运行 3 秒钟 */
	sleep(3);
	g_running = 0;

	pthread_join(dataplane_tid, NULL);
	pthread_join(telemetry_tid, NULL);

	printf("======================================================================\n");
	printf("  示例 4 运行完成：快路径极致性能，慢路径稳定一致，无锁设计大获全胜！\n");
	printf("======================================================================\n");

	return EXIT_SUCCESS;
}
