/*
 * PUSH/PULL 示例2：三级流水线 —— Worker
 * 用法：./worker <id>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <zmq.h>

int
main(int argc, char **argv)
{
	void *ctx, *receiver, *sender;
	char buf[16];
	int len, cost;
	const char *id = (argc > 1) ? argv[1] : "?";

	ctx = zmq_ctx_new();

	receiver = zmq_socket(ctx, ZMQ_PULL);
	zmq_connect(receiver, "tcp://127.0.0.1:5563");

	sender = zmq_socket(ctx, ZMQ_PUSH);
	zmq_connect(sender, "tcp://127.0.0.1:5564");

	int processed = 0;
	for(;;){
		len = zmq_recv(receiver, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		cost = atoi(buf);

		usleep(cost * 1000);
		processed++;

		zmq_send(sender, "done", 4, 0);
	}
	printf("[Worker %s] processed %d tasks\n", id, processed);

	zmq_close(receiver);
	zmq_close(sender);
	zmq_ctx_destroy(ctx);
	return 0;
}
