/*
 * PUB/SUB 示例1：主题过滤广播 —— 订阅端（只订阅 AI）
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *sub;
	char buf[256];
	int len;

	ctx = zmq_ctx_new();
	sub = zmq_socket(ctx, ZMQ_SUB);
	zmq_connect(sub, "tcp://127.0.0.1:5558");

	zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "AI", 2);
	printf("[Subscriber] Subscribed to topic: AI\n");

	for(;;){
		len = zmq_recv(sub, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		printf("[Subscriber] %s\n", buf);
	}

	zmq_close(sub);
	zmq_ctx_destroy(ctx);
	return 0;
}
