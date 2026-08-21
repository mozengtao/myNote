/*
 * PUB/SUB 示例1：主题过滤广播 —— 发布端
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *pub;
	const char *topics[] = { "NEWS", "SPORT", "AI" };
	int i;

	ctx = zmq_ctx_new();
	pub = zmq_socket(ctx, ZMQ_PUB);
	zmq_bind(pub, "tcp://*:5558");
	printf("[Publisher] Broadcasting on tcp://*:5558\n");

	/* 给 Subscriber 一点时间建立连接，减少最开始几条消息被丢弃 */
	sleep(1);

	for(i = 0; ; i++){
		char msg[64];
		const char *topic = topics[i % 3];
		snprintf(msg, sizeof(msg), "%s: message %d", topic, i);

		printf("[Publisher] Sending: %s\n", msg);
		zmq_send(pub, msg, strlen(msg), 0);

		usleep(300 * 1000);
	}

	zmq_close(pub);
	zmq_ctx_destroy(ctx);
	return 0;
}
