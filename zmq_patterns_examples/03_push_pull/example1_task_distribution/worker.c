/*
 * PUSH/PULL 示例1：Worker（可启动多个实例做负载均衡）
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
	void *ctx, *pull;
	char buf[64];
	int len, task_id, cost;
	const char *id = (argc > 1) ? argv[1] : "?";

	ctx = zmq_ctx_new();
	pull = zmq_socket(ctx, ZMQ_PULL);
	zmq_connect(pull, "tcp://127.0.0.1:5562");

	for(;;){
		len = zmq_recv(pull, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';

		sscanf(buf, "%d %d", &task_id, &cost);
		printf("[Worker %s] processing task %d (cost=%d units)\n", id, task_id, cost);
		usleep(cost * 100 * 1000);
	}

	zmq_close(pull);
	zmq_ctx_destroy(ctx);
	return 0;
}
