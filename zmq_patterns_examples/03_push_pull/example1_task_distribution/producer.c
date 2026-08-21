/*
 * PUSH/PULL 示例1：任务分发端
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <zmq.h>

#define NUM_TASKS 20

int
main(void)
{
	void *ctx, *push;
	int i;

	srand((unsigned)time(NULL));

	ctx = zmq_ctx_new();
	push = zmq_socket(ctx, ZMQ_PUSH);
	zmq_bind(push, "tcp://*:5562");
	printf("[Producer] Listening on tcp://*:5562\n");

	sleep(1); /* 给 Worker 一点时间连接 */

	printf("[Producer] Dispatching %d tasks...\n", NUM_TASKS);
	for(i = 0; i < NUM_TASKS; i++){
		int cost = 1 + rand() % 5;
		char task[32];
		snprintf(task, sizeof(task), "%d %d", i, cost);
		zmq_send(push, task, strlen(task), 0);
		usleep(50 * 1000);
	}
	printf("[Producer] Done dispatching.\n");

	sleep(1);
	zmq_close(push);
	zmq_ctx_destroy(ctx);
	return 0;
}
