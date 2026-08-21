/*
 * PUB/SUB 示例2：多发布者 —— 订阅端（同时连接A和B，只订阅 AI）
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *sub;
	char topic[64], source[64], headline[128];
	int len;

	ctx = zmq_ctx_new();
	sub = zmq_socket(ctx, ZMQ_SUB);
	zmq_connect(sub, "tcp://127.0.0.1:5559");
	zmq_connect(sub, "tcp://127.0.0.1:5560");

	zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "AI", 2);
	printf("[Subscriber] Subscribed to topic: AI (from both Source A and Source B)\n");

	for(;;){
		len = zmq_recv(sub, topic, sizeof(topic) - 1, 0);
		if(len < 0)
			break;
		topic[len] = '\0';

		len = zmq_recv(sub, source, sizeof(source) - 1, 0);
		source[len] = '\0';

		len = zmq_recv(sub, headline, sizeof(headline) - 1, 0);
		headline[len] = '\0';

		printf("[Subscriber] [%s] %s\n", source, headline);
	}

	zmq_close(sub);
	zmq_ctx_destroy(ctx);
	return 0;
}
