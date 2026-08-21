/*
 * REQ/REP 示例2：结构化计算服务客户端（多部分消息）
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

static void
send_request(void *sock, const char *op, const char *a, const char *b)
{
	zmq_send(sock, op, strlen(op), ZMQ_SNDMORE);
	zmq_send(sock, a, strlen(a), ZMQ_SNDMORE);
	zmq_send(sock, b, strlen(b), 0);
}

static void
call(void *sock, const char *op, const char *a, const char *b)
{
	char buf[128];
	int len;

	send_request(sock, op, a, b);
	len = zmq_recv(sock, buf, sizeof(buf) - 1, 0);
	buf[len] = '\0';
	printf("[Client] %s %s %s -> %s\n", op, a, b, buf);
}

int
main(void)
{
	void *ctx, *req;

	ctx = zmq_ctx_new();
	req = zmq_socket(ctx, ZMQ_REQ);
	zmq_connect(req, "tcp://127.0.0.1:5556");

	call(req, "add", "3", "5");
	call(req, "sub", "10", "4");
	call(req, "mul", "6", "7");
	call(req, "div", "10", "0");
	call(req, "mod", "7", "2");

	zmq_close(req);
	zmq_ctx_destroy(ctx);
	return 0;
}
