/*
 * DEALER/ROUTER 示例2：REP Worker（连接到 Broker 后端）
 * 用法：./worker <id>
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

int
main(int argc, char **argv)
{
	void *ctx, *rep;
	char buf[256];
	int len;
	const char *id = (argc > 1) ? argv[1] : "?";

	ctx = zmq_ctx_new();
	rep = zmq_socket(ctx, ZMQ_REP);
	zmq_connect(rep, "tcp://127.0.0.1:5569");
	printf("[Worker %s] Connected to broker backend\n", id);

	for(;;){
		len = zmq_recv(rep, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		printf("[Worker %s] Handling: %s\n", id, buf);

		char reply[256];
		snprintf(reply, sizeof(reply), "Handled by worker %s: %s", id, buf);
		zmq_send(rep, reply, strlen(reply), 0);
	}

	zmq_close(rep);
	zmq_ctx_destroy(ctx);
	return 0;
}
