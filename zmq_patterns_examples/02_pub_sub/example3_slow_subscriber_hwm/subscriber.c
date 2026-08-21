/*
 * PUB/SUB 示例3：处理很慢的订阅端（小 RCVHWM）
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <zmq.h>

#define RCVHWM 20
#define TOTAL_EXPECTED 1000

int
main(void)
{
	void *ctx, *sub;
	int hwm = RCVHWM;
	char buf[32];
	int len, seq, expected = 0, received = 0;

	ctx = zmq_ctx_new();
	sub = zmq_socket(ctx, ZMQ_SUB);
	zmq_setsockopt(sub, ZMQ_RCVHWM, &hwm, sizeof(hwm));
	zmq_connect(sub, "tcp://127.0.0.1:5561");
	zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);

	printf("[Subscriber] Subscribed, RCVHWM=%d, simulating slow processing (5ms/msg)\n", RCVHWM);

	/* 用最后一个序号(TOTAL_EXPECTED-1)作为"流结束"信号，而不是数received数量，
	 * 因为如果消息被丢弃，received永远达不到TOTAL_EXPECTED。 */
	for(;;){
		len = zmq_recv(sub, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		received++;

		seq = atoi(buf + strlen("SEQ "));
		printf("[Subscriber] recv %s\n", buf);

		if(seq != expected){
			printf("[Subscriber] *** Gap detected: expected %d, got %d (lost %d messages) ***\n",
				expected, seq, seq - expected);
		}
		expected = seq + 1;

		usleep(5000); /* 模拟慢处理 */

		if(seq == TOTAL_EXPECTED - 1)
			break;
	}

	printf("[Subscriber] Done. Sent=%d (by publisher), Received=%d, Lost=%d\n",
		TOTAL_EXPECTED, received, TOTAL_EXPECTED - received);

	zmq_close(sub);
	zmq_ctx_destroy(ctx);
	return 0;
}
