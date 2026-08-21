/*
 * DEALER/ROUTER 示例2：扩展请求-响应 Broker
 *
 * 前端 ROUTER 面向 REQ 客户端，后端 DEALER 面向 REP Worker，
 * zmq_proxy 自动转发消息，DEALER 天然做 Round-Robin 负载均衡。
 */
#include <stdio.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *frontend, *backend;

	ctx = zmq_ctx_new();

	frontend = zmq_socket(ctx, ZMQ_ROUTER);
	zmq_bind(frontend, "tcp://*:5568");

	backend = zmq_socket(ctx, ZMQ_DEALER);
	zmq_bind(backend, "tcp://*:5569");

	printf("[Broker] Frontend (clients) on tcp://*:5568\n");
	printf("[Broker] Backend  (workers) on tcp://*:5569\n");
	printf("[Broker] Proxying...\n");

	zmq_proxy(frontend, backend, NULL); /* 阻塞运行，直到进程退出 */

	zmq_close(frontend);
	zmq_close(backend);
	zmq_ctx_destroy(ctx);
	return 0;
}
