/*
 * DEALER/ROUTER 示例3：基于 Identity 的主动推送服务端
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zmq.h>

#define MAX_CLIENTS 16
#define PUSH_INTERVAL_MS 2000

typedef struct {
	char name[32];
	char identity[256];
	int idlen;
} client_t;

static client_t clients[MAX_CLIENTS];
static int nclients = 0;

static void
register_client(const char *identity, int idlen, const char *name)
{
	int i;
	for(i = 0; i < nclients; i++){
		if(strcmp(clients[i].name, name) == 0){
			memcpy(clients[i].identity, identity, idlen);
			clients[i].idlen = idlen;
			return;
		}
	}
	if(nclients < MAX_CLIENTS){
		strncpy(clients[nclients].name, name, sizeof(clients[nclients].name) - 1);
		memcpy(clients[nclients].identity, identity, idlen);
		clients[nclients].idlen = idlen;
		nclients++;
	}
}

static long
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int
main(void)
{
	void *ctx, *router;
	int counter = 0;
	long last_push;

	ctx = zmq_ctx_new();
	router = zmq_socket(ctx, ZMQ_ROUTER);
	zmq_bind(router, "tcp://*:5570");
	printf("[Server] ROUTER listening on tcp://*:5570\n");

	last_push = now_ms();

	for(;;){
		zmq_pollitem_t items[] = { { router, 0, ZMQ_POLLIN, 0 } };
		zmq_poll(items, 1, 200); /* 200ms超时，保证能定期检查推送时机 */

		if(items[0].revents & ZMQ_POLLIN){
			char identity[256], payload[256];
			int idlen, paylen;

			idlen = zmq_recv(router, identity, sizeof(identity), 0);
			paylen = zmq_recv(router, payload, sizeof(payload) - 1, 0);
			payload[paylen] = '\0';

			if(strncmp(payload, "REGISTER ", 9) == 0){
				const char *name = payload + 9;
				register_client(identity, idlen, name);
				printf("[Server] Registered client: %s\n", name);
			}
		}

		if(nclients > 0 && now_ms() - last_push >= PUSH_INTERVAL_MS){
			client_t *target = &clients[counter % nclients];
			char notif[64];
			snprintf(notif, sizeof(notif), "System update #%d", counter);

			printf("[Server] Pushing notification to %s: \"%s\"\n", target->name, notif);

			zmq_send(router, target->identity, target->idlen, ZMQ_SNDMORE);
			zmq_send(router, notif, strlen(notif), 0);

			counter++;
			last_push = now_ms();
		}
	}

	zmq_close(router);
	zmq_ctx_destroy(ctx);
	return 0;
}
