/*
 * PUSH/PULL 示例3：日志生产者（模拟一个微服务节点）
 * 用法：./log_producer <service_name>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <zmq.h>

#define LOGS_PER_PRODUCER 10

int
main(int argc, char **argv)
{
	void *ctx, *push;
	int i;
	const char *name = (argc > 1) ? argv[1] : "unknown";

	ctx = zmq_ctx_new();
	push = zmq_socket(ctx, ZMQ_PUSH);
	zmq_connect(push, "tcp://127.0.0.1:5566");

	for(i = 0; i < LOGS_PER_PRODUCER; i++){
		char msg[128];
		snprintf(msg, sizeof(msg), "%s: log line %d", name, i);
		zmq_send(push, msg, strlen(msg), 0);
		usleep((rand() % 200) * 1000);
	}

	printf("[%s] Sent %d logs\n", name, LOGS_PER_PRODUCER);

	zmq_close(push);
	zmq_ctx_destroy(ctx);
	return 0;
}
