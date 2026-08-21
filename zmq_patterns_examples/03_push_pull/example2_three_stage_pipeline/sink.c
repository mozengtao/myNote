/*
 * PUSH/PULL 示例2：三级流水线 —— Sink
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *ctrl, *results;
	char buf[16];
	int len, expected, received;
	struct timespec start, end;

	ctx = zmq_ctx_new();

	ctrl = zmq_socket(ctx, ZMQ_PULL);
	zmq_bind(ctrl, "tcp://*:5565");

	results = zmq_socket(ctx, ZMQ_PULL);
	zmq_bind(results, "tcp://*:5564");

	printf("[Sink] Waiting for control message...\n");
	len = zmq_recv(ctrl, buf, sizeof(buf) - 1, 0);
	buf[len] = '\0';
	expected = atoi(buf);
	printf("[Sink] Expecting %d results\n", expected);

	clock_gettime(CLOCK_MONOTONIC, &start);

	for(received = 0; received < expected; received++){
		len = zmq_recv(results, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;

		if((received + 1) % (expected / 5 == 0 ? 1 : expected / 5) == 0)
			printf("[Sink] Progress: %d/%d\n", received + 1, expected);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

	printf("[Sink] All %d results received. Total time: %.2fs\n", received, elapsed);

	zmq_close(ctrl);
	zmq_close(results);
	zmq_ctx_destroy(ctx);
	return 0;
}
