/*
 * PAIR 示例3：双向聊天 —— Peer B（connect 端）
 *
 * 逻辑与 peer_a.c 完全相同，只有 bind/connect 和标签不同。
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

int
main(void)
{
	void *ctx = zmq_ctx_new();
	void *pair = zmq_socket(ctx, ZMQ_PAIR);
	zmq_connect(pair, "tcp://127.0.0.1:5572");

	printf("[PeerB] Connected. Type messages, type /quit to exit.\n> ");
	fflush(stdout);

	for(;;){
		zmq_pollitem_t items[] = {
			{ pair, 0, ZMQ_POLLIN, 0 },
			{ NULL, STDIN_FILENO, ZMQ_POLLIN, 0 },
		};
		zmq_poll(items, 2, -1);

		if(items[0].revents & ZMQ_POLLIN){
			char buf[256];
			int len = zmq_recv(pair, buf, sizeof(buf) - 1, 0);
			buf[len] = '\0';
			printf("\n[A]: %s\n> ", buf);
			fflush(stdout);
		}

		if(items[1].revents & ZMQ_POLLIN){
			char line[256];
			if(fgets(line, sizeof(line), stdin) == NULL)
				break;
			line[strcspn(line, "\n")] = '\0';

			zmq_send(pair, line, strlen(line), 0);

			if(strcmp(line, "/quit") == 0)
				break;

			printf("> ");
			fflush(stdout);
		}
	}

	zmq_close(pair);
	zmq_ctx_destroy(ctx);
	return 0;
}
