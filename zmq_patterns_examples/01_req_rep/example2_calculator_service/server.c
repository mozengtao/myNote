/*
 * REQ/REP 示例2：结构化计算服务端（多部分消息）
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zmq.h>

#define MAXFRAMES 4
#define FRAMELEN 64

static int
recv_multipart(void *sock, char frames[MAXFRAMES][FRAMELEN])
{
	int n, len;
	int64_t more;
	size_t more_size = sizeof(more);

	n = 0;
	for(;;){
		len = zmq_recv(sock, frames[n], FRAMELEN - 1, 0);
		if(len < 0)
			return -1;
		frames[n][len] = '\0';
		n++;

		zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_size);
		if(!more || n >= MAXFRAMES)
			break;
	}
	return n;
}

static void
handle_request(char frames[MAXFRAMES][FRAMELEN], int nframes, char *reply, size_t replylen)
{
	if(nframes != 3){
		snprintf(reply, replylen, "ERR|expected 3 frames, got %d", nframes);
		return;
	}

	const char *op = frames[0];
	double a = atof(frames[1]);
	double b = atof(frames[2]);

	if(strcmp(op, "add") == 0){
		snprintf(reply, replylen, "OK|%g", a + b);
	}else if(strcmp(op, "sub") == 0){
		snprintf(reply, replylen, "OK|%g", a - b);
	}else if(strcmp(op, "mul") == 0){
		snprintf(reply, replylen, "OK|%g", a * b);
	}else if(strcmp(op, "div") == 0){
		if(b == 0)
			snprintf(reply, replylen, "ERR|division by zero");
		else
			snprintf(reply, replylen, "OK|%g", a / b);
	}else{
		snprintf(reply, replylen, "ERR|unknown operator: %s", op);
	}
}

int
main(void)
{
	void *ctx, *rep;
	char frames[MAXFRAMES][FRAMELEN];
	char reply[128];
	int nframes;

	ctx = zmq_ctx_new();
	rep = zmq_socket(ctx, ZMQ_REP);
	zmq_bind(rep, "tcp://*:5556");
	printf("[Server] Calculator service listening on tcp://*:5556\n");

	for(;;){
		nframes = recv_multipart(rep, frames);
		if(nframes < 0)
			break;

		printf("[Server] Request:");
		for(int i = 0; i < nframes; i++)
			printf(" %s", frames[i]);
		printf("\n");

		handle_request(frames, nframes, reply, sizeof(reply));
		printf("[Server] Reply: %s\n", reply);

		zmq_send(rep, reply, strlen(reply), 0);
	}

	zmq_close(rep);
	zmq_ctx_destroy(ctx);
	return 0;
}
