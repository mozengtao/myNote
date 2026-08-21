/*
 * DEALER/ROUTER 示例3：注册并接收主动推送的客户端
 * 用法：./client <name>
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

int
main(int argc, char **argv)
{
	void *ctx, *dealer;
	char buf[256];
	int len;
	const char *name = (argc > 1) ? argv[1] : "?";

	ctx = zmq_ctx_new();
	dealer = zmq_socket(ctx, ZMQ_DEALER);
	zmq_connect(dealer, "tcp://127.0.0.1:5570");

	char reg[64];
	snprintf(reg, sizeof(reg), "REGISTER %s", name);
	zmq_send(dealer, reg, strlen(reg), 0);
	printf("[Client %s] Registered with server\n", name);

	for(;;){
		len = zmq_recv(dealer, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		printf("[Client %s] Notification: %s\n", name, buf);
	}

	zmq_close(dealer);
	zmq_ctx_destroy(ctx);
	return 0;
}
