/*
 * REQ/REP 示例3：Lazy Pirate 客户端（超时 + 重试）
 */
#include <stdio.h>
#include <string.h>
#include <zmq.h>

#define SERVER_ENDPOINT   "tcp://127.0.0.1:5557"
#define REQUEST_TIMEOUT   2500  /* ms，明显小于服务端最坏延迟(4s) */
#define REQUEST_RETRIES   3
#define NUM_TASKS         5

static void *
new_req_socket(void *ctx)
{
	void *s = zmq_socket(ctx, ZMQ_REQ);
	zmq_connect(s, SERVER_ENDPOINT);
	return s;
}

int
main(void)
{
	void *ctx = zmq_ctx_new();
	void *client = new_req_socket(ctx);

	for(int task = 0; task < NUM_TASKS; task++){
		char request[64];
		snprintf(request, sizeof(request), "Request %d", task);

		printf("[Client] Sending: %s\n", request);
		zmq_send(client, request, strlen(request), 0);

		int retries_left = REQUEST_RETRIES;
		int got_reply = 0;

		while(!got_reply){
			zmq_pollitem_t items[] = { { client, 0, ZMQ_POLLIN, 0 } };
			int rc = zmq_poll(items, 1, REQUEST_TIMEOUT);

			if(rc == -1)
				break; /* interrupted */

			if(items[0].revents & ZMQ_POLLIN){
				char buf[256];
				int len = zmq_recv(client, buf, sizeof(buf) - 1, 0);
				buf[len] = '\0';
				printf("[Client] Received reply: %s\n", buf);
				got_reply = 1;
				retries_left = REQUEST_RETRIES;
				continue;
			}

			retries_left--;
			if(retries_left == 0){
				printf("[Client] Server seems offline, abandoning task: %s\n", request);
				break;
			}

			printf("[Client] No response from server, retrying... (%d retries left)\n", retries_left);

			/* REQ socket 处于"半个问句"状态，必须整体丢弃重建 */
			zmq_close(client);
			client = new_req_socket(ctx);
			zmq_send(client, request, strlen(request), 0);
		}
	}

	zmq_close(client);
	zmq_ctx_destroy(ctx);
	return 0;
}
