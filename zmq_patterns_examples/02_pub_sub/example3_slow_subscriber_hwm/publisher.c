/*
 * PUB/SUB 示例3：高速发布端（小 SNDHWM）
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#define TOTAL_MESSAGES 1000
#define SNDHWM 20

int
main(void)
{
	void *ctx, *pub;
	int hwm = SNDHWM;
	int i;

	ctx = zmq_ctx_new();
	pub = zmq_socket(ctx, ZMQ_PUB);
	zmq_setsockopt(pub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
	zmq_bind(pub, "tcp://*:5561");

	printf("[Publisher] Listening on tcp://*:5561, SNDHWM=%d\n", SNDHWM);
	sleep(1); /* 给 Subscriber 建立连接的时间 */

	for(i = 0; i < TOTAL_MESSAGES; i++){
		char msg[32];
		snprintf(msg, sizeof(msg), "SEQ %d", i);
		zmq_send(pub, msg, strlen(msg), 0);
	}

	printf("[Publisher] Done sending %d messages\n", TOTAL_MESSAGES);

	sleep(1); /* 留点时间让最后一批消息真正发出 */
	zmq_close(pub);
	zmq_ctx_destroy(ctx);
	return 0;
}
