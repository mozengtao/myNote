/*
 * PUSH/PULL 示例2：三级流水线 —— Producer (ventilator)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <zmq.h>

#define NUM_TASKS 100

int
main(void)
{
	void *ctx, *sender, *sink_ctrl;
	int i, total_cost = 0;

	srand((unsigned)time(NULL));

	ctx = zmq_ctx_new();

	sender = zmq_socket(ctx, ZMQ_PUSH);
	zmq_bind(sender, "tcp://*:5563");

	sink_ctrl = zmq_socket(ctx, ZMQ_PUSH);
	zmq_connect(sink_ctrl, "tcp://127.0.0.1:5565");

	printf("[Producer] Listening on tcp://*:5563 (tasks)\n");
	sleep(1); /* 给 Worker 和 Sink 连接的时间 */

	char count_msg[16];
	snprintf(count_msg, sizeof(count_msg), "%d", NUM_TASKS);
	zmq_send(sink_ctrl, count_msg, strlen(count_msg), 0);
	printf("[Producer] Told Sink to expect %d results\n", NUM_TASKS);

	for(i = 0; i < NUM_TASKS; i++){
		int cost = 1 + rand() % 20; /* ms */
		total_cost += cost;

		char task[16];
		snprintf(task, sizeof(task), "%d", cost);
		zmq_send(sender, task, strlen(task), 0);
	}
	printf("[Producer] Dispatched %d tasks, total workload=%dms\n", NUM_TASKS, total_cost);

	sleep(1);
	zmq_close(sender);
	zmq_close(sink_ctrl);
	zmq_ctx_destroy(ctx);
	return 0;
}
