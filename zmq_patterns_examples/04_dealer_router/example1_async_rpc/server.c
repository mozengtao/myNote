/*
 * DEALER/ROUTER 示例1：ROUTER 服务端
 *
 * ROUTER 收消息时，第一帧总是"这条消息来自谁"的 identity；
 * 回复时必须把同样的 identity 作为第一帧发回去。
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *router;
	char identity[256], payload[256];
	int idlen, paylen;

	ctx = zmq_ctx_new();
	router = zmq_socket(ctx, ZMQ_ROUTER);
	zmq_bind(router, "tcp://*:5567");
	printf("[Server] ROUTER listening on tcp://*:5567\n");

	for(;;){
		idlen = zmq_recv(router, identity, sizeof(identity) - 1, 0);
		if(idlen < 0)
			break;

		paylen = zmq_recv(router, payload, sizeof(payload) - 1, 0);
		payload[paylen] = '\0';

		printf("[Server] Received from client: %s\n", payload);
		usleep((10 + (paylen * 7) % 50) * 1000); /* 模拟处理耗时，各请求耗时不同 */

		char reply[256];
		snprintf(reply, sizeof(reply), "Reply to %s", payload);

		zmq_send(router, identity, idlen, ZMQ_SNDMORE);
		zmq_send(router, reply, strlen(reply), 0);
	}

	zmq_close(router);
	zmq_ctx_destroy(ctx);
	return 0;
}
