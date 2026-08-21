/*
 * PUB/SUB 示例2：多发布者 —— Source A
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

int
main(void)
{
	void *ctx, *pub;
	const char *topics[] = { "NEWS", "SPORT", "AI" };
	int i;

	ctx = zmq_ctx_new();
	pub = zmq_socket(ctx, ZMQ_PUB);
	zmq_bind(pub, "tcp://*:5559");
	printf("[SourceA] Broadcasting on tcp://*:5559\n");

	sleep(1);

	for(i = 0; ; i++){
		const char *topic = topics[i % 3];
		char headline[64];
		snprintf(headline, sizeof(headline), "AI headline #%d", i);
		if(strcmp(topic, "AI") != 0)
			snprintf(headline, sizeof(headline), "%s headline #%d", topic, i);

		zmq_send(pub, topic, strlen(topic), ZMQ_SNDMORE);
		zmq_send(pub, "SourceA", strlen("SourceA"), ZMQ_SNDMORE);
		zmq_send(pub, headline, strlen(headline), 0);

		printf("[SourceA] Sent: %s | SourceA | %s\n", topic, headline);
		usleep(400 * 1000);
	}

	zmq_close(pub);
	zmq_ctx_destroy(ctx);
	return 0;
}
