/*
 * PAIR 示例2：心跳保活 —— Node A（bind 端）
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zmq.h>

#define HEARTBEAT_INTERVAL_MS 1000
#define TIMEOUT_MS 3000

static long
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int
main(void)
{
	void *ctx = zmq_ctx_new();
	void *pair = zmq_socket(ctx, ZMQ_PAIR);
	zmq_bind(pair, "tcp://*:5571");
	printf("[NodeA] PAIR bound on tcp://*:5571\n");

	long last_sent = 0, last_seen = now_ms();
	int seq = 0, warned = 0;

	for(;;){
		zmq_pollitem_t items[] = { { pair, 0, ZMQ_POLLIN, 0 } };
		zmq_poll(items, 1, 200);

		if(items[0].revents & ZMQ_POLLIN){
			char buf[64];
			int len = zmq_recv(pair, buf, sizeof(buf) - 1, 0);
			buf[len] = '\0';
			printf("[NodeA] Received: %s\n", buf);
			last_seen = now_ms();
			warned = 0;
		}

		if(now_ms() - last_sent >= HEARTBEAT_INTERVAL_MS){
			char msg[32];
			snprintf(msg, sizeof(msg), "PING A %d", seq++);
			zmq_send(pair, msg, strlen(msg), 0);
			printf("[NodeA] Sent: %s\n", msg);
			last_sent = now_ms();
		}

		if(!warned && now_ms() - last_seen > TIMEOUT_MS){
			printf("[NodeA] *** WARNING: no message from peer for %.1fs, peer might be down ***\n",
				(now_ms() - last_seen) / 1000.0);
			warned = 1;
		}
	}

	zmq_close(pair);
	zmq_ctx_destroy(ctx);
	return 0;
}
