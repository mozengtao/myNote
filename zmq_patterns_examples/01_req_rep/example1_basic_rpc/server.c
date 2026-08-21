/*
 * REQ/REP 示例1：基础 Ping-Pong 服务端
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *rep;
	char buf[256];
	int rc, len;

	ctx = zmq_ctx_new();
	rep = zmq_socket(ctx, ZMQ_REP);

	rc = zmq_bind(rep, "tcp://*:5555");
	if(rc != 0){
		perror("zmq_bind");
		return 1;
	}
	printf("[Server] Listening on tcp://*:5555\n");

	for(;;){
		len = zmq_recv(rep, buf, sizeof(buf) - 1, 0);
		if(len < 0)
			break;
		buf[len] = '\0';
		printf("[Server] Received: %s\n", buf);

		char reply[256];
		snprintf(reply, sizeof(reply), "World %s", buf + strlen("Hello "));
		zmq_send(rep, reply, strlen(reply), 0);
	}

	zmq_close(rep);
	zmq_ctx_destroy(ctx);
	return 0;
}
