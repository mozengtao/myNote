/*
 * REQ/REP 示例3：模拟"偶尔很慢"的服务端
 *
 * 约 30% 的请求会故意延迟 4 秒才回复（模拟假死/GC停顿），
 * 用来触发客户端的超时重试逻辑。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *rep;
	char buf[256];
	int len;

	srand((unsigned)time(NULL));

	ctx = zmq_ctx_new();
	rep = zmq_socket(ctx, ZMQ_REP);
	zmq_bind(rep, "tcp://*:5557");
	printf("[Server] Listening on tcp://*:5557 (30%% chance of 4s slow response)\n");

	for(;;){
		len = zmq_recv(rep, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		printf("[Server] Received: %s\n", buf);

		if(rand() % 10 < 3){
			printf("[Server] Simulating slow processing (4s)...\n");
			sleep(4);
		}else{
			usleep((rand() % 300) * 1000);
		}

		char reply[256];
		snprintf(reply, sizeof(reply), "Reply to %s", buf);
		zmq_send(rep, reply, strlen(reply), 0);
		printf("[Server] Sent: %s\n", reply);
	}

	zmq_close(rep);
	zmq_ctx_destroy(ctx);
	return 0;
}
