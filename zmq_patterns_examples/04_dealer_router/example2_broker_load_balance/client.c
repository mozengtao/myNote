/*
 * DEALER/ROUTER 示例2：REQ 客户端（连接到 Broker 前端）
 * 用法：./client <name>
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

#define NUM_REQUESTS 3

int
main(int argc, char **argv)
{
	void *ctx, *req;
	char buf[256];
	int i, len;
	const char *name = (argc > 1) ? argv[1] : "?";

	ctx = zmq_ctx_new();
	req = zmq_socket(ctx, ZMQ_REQ);
	zmq_connect(req, "tcp://127.0.0.1:5568");

	for(i = 0; i < NUM_REQUESTS; i++){
		char msg[32];
		snprintf(msg, sizeof(msg), "req %d", i);
		printf("[Client %s] Sending: %s\n", name, msg);
		zmq_send(req, msg, strlen(msg), 0);

		len = zmq_recv(req, buf, sizeof(buf) - 1, 0);
		buf[len] = '\0';
		printf("[Client %s] Received: %s\n", name, buf);
	}

	zmq_close(req);
	zmq_ctx_destroy(ctx);
	return 0;
}
