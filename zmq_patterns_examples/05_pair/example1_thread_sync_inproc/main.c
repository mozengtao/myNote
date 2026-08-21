/*
 * PAIR 示例1：inproc 线程间控制信号
 *
 * 主线程 bind "inproc://control"，工作线程 connect 同一地址。
 * inproc 要求两端共享同一个 Context，且必须先 bind 后 connect。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>

#define NUM_STEPS 10

static void *
worker_thread(void *arg)
{
	void *ctx = arg;
	void *pair = zmq_socket(ctx, ZMQ_PAIR);
	zmq_connect(pair, "inproc://control");

	char buf[64];
	int len = zmq_recv(pair, buf, sizeof(buf) - 1, 0);
	buf[len] = '\0';
	printf("[Worker] Received %s, beginning work...\n", buf);

	int stopped = 0;
	int progress;
	for(progress = 0; progress < NUM_STEPS; progress++){
		usleep(200 * 1000); /* 模拟一步工作 */

		char msg[32];
		snprintf(msg, sizeof(msg), "PROGRESS %d", progress);
		printf("[Worker] Progress %d/%d\n", progress, NUM_STEPS);
		zmq_send(pair, msg, strlen(msg), 0);

		/* 非阻塞检查是否收到 STOP 信号 */
		zmq_pollitem_t items[] = { { pair, 0, ZMQ_POLLIN, 0 } };
		if(zmq_poll(items, 1, 0) > 0 && (items[0].revents & ZMQ_POLLIN)){
			char cmd[32];
			int clen = zmq_recv(pair, cmd, sizeof(cmd) - 1, 0);
			cmd[clen] = '\0';
			if(strcmp(cmd, "STOP") == 0){
				printf("[Worker] Received STOP signal, exiting early\n");
				stopped = 1;
				break;
			}
		}
	}

	char done[64];
	if(stopped)
		snprintf(done, sizeof(done), "DONE (stopped early at progress %d)", progress);
	else
		snprintf(done, sizeof(done), "DONE (completed all %d steps)", NUM_STEPS);
	zmq_send(pair, done, strlen(done), 0);

	zmq_close(pair);
	return NULL;
}

int
main(void)
{
	void *ctx = zmq_ctx_new();
	void *pair = zmq_socket(ctx, ZMQ_PAIR);
	zmq_bind(pair, "inproc://control"); /* inproc 必须先 bind */

	pthread_t tid;
	pthread_create(&tid, NULL, worker_thread, ctx);

	zmq_send(pair, "START", 5, 0);
	printf("[Main] Sent START\n");

	char buf[64];
	int stop_sent = 0;
	for(;;){
		int len = zmq_recv(pair, buf, sizeof(buf) - 1, 0);
		buf[len] = '\0';

		if(strncmp(buf, "PROGRESS", 8) == 0){
			int p = atoi(buf + 9);
			printf("[Main] Progress: %d/%d\n", p, NUM_STEPS);

			if(p == 3 && !stop_sent){
				printf("[Main] Got progress 3, sending STOP early\n");
				zmq_send(pair, "STOP", 4, 0);
				stop_sent = 1;
			}
		}else if(strncmp(buf, "DONE", 4) == 0){
			printf("[Main] Worker done: %s\n", buf);
			break;
		}
	}

	pthread_join(tid, NULL);

	zmq_close(pair);
	zmq_ctx_destroy(ctx);
	return 0;
}
