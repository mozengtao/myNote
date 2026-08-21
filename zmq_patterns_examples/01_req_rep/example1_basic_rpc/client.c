/*
 * REQ/REP 示例1：基础 Ping-Pong 客户端
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *req;
	char buf[256];
	int i, len;

	ctx = zmq_ctx_new();
	req = zmq_socket(ctx, ZMQ_REQ);
	zmq_connect(req, "tcp://127.0.0.1:5555");

	for(i = 0; i < 5; i++){
		char msg[64];
		snprintf(msg, sizeof(msg), "Hello %d", i);

		printf("[Client] Sending: %s\n", msg);
		zmq_send(req, msg, strlen(msg), 0);

		len = zmq_recv(req, buf, sizeof(buf) - 1, 0);
		buf[len] = '\0';
		printf("[Client] Received reply: %s\n", buf);
	}

	zmq_close(req);
	zmq_ctx_destroy(ctx);
	return 0;
}
