/*
 * PUSH/PULL 示例3：日志收集端（多对一）
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

#define NUM_PRODUCERS 3
#define LOGS_PER_PRODUCER 10
#define TOTAL_LOGS (NUM_PRODUCERS * LOGS_PER_PRODUCER)
#define MAX_SOURCES 8

int
main(void)
{
	void *ctx, *pull;
	char buf[256];
	char sources[MAX_SOURCES][64];
	int counts[MAX_SOURCES] = { 0 };
	int nsources = 0;
	int len, i, received;

	ctx = zmq_ctx_new();
	pull = zmq_socket(ctx, ZMQ_PULL);
	zmq_bind(pull, "tcp://*:5566");
	printf("[Collector] Listening on tcp://*:5566, waiting for %d logs total\n", TOTAL_LOGS);

	for(received = 0; received < TOTAL_LOGS; received++){
		len = zmq_recv(pull, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		printf("[Collector] %s\n", buf);

		char source[64];
		sscanf(buf, "%63[^:]:", source);

		int found = -1;
		for(i = 0; i < nsources; i++){
			if(strcmp(sources[i], source) == 0){
				found = i;
				break;
			}
		}
		if(found < 0 && nsources < MAX_SOURCES){
			found = nsources++;
			strncpy(sources[found], source, sizeof(sources[found]) - 1);
		}
		if(found >= 0)
			counts[found]++;
	}

	printf("[Collector] All %d logs received.\n", received);
	printf("[Collector] Summary: ");
	for(i = 0; i < nsources; i++)
		printf("%s=%d%s", sources[i], counts[i], (i < nsources - 1) ? ", " : "\n");

	zmq_close(pull);
	zmq_ctx_destroy(ctx);
	return 0;
}
