/*
 * DEALER/ROUTER 示例1：DEALER 客户端
 * 用法：./client <name>
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

#define NUM_REQUESTS 5

int
main(int argc, char **argv)
{
	void *ctx, *dealer;
	int i, len;
	char buf[256];
	const char *name = (argc > 1) ? argv[1] : "?";

	ctx = zmq_ctx_new();
	dealer = zmq_socket(ctx, ZMQ_DEALER);
	zmq_connect(dealer, "tcp://127.0.0.1:5567");

	printf("[Client %s] Sending %d requests without waiting for replies...\n", name, NUM_REQUESTS);
	for(i = 0; i < NUM_REQUESTS; i++){
		char req[32];
		snprintf(req, sizeof(req), "req %d", i);
		zmq_send(dealer, req, strlen(req), 0);
		printf("[Client %s] Sent: %s\n", name, req);
	}

	for(i = 0; i < NUM_REQUESTS; i++){
		len = zmq_recv(dealer, buf, sizeof(buf) - 1, 0);
		buf[len] = '\0';
		printf("[Client %s] Received reply: %s\n", name, buf);
	}

	zmq_close(dealer);
	zmq_ctx_destroy(ctx);
	return 0;
}
