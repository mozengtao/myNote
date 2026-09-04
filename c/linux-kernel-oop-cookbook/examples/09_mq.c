/*
 * 09_mq.c - Message queue: Producer / Consumer / Dispatcher
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

#define MQ_CAP 16

struct message {
	int id;
	char payload[64];
};

struct mq_node;

struct mq_ops {
	void (*on_message)(struct mq_node *self, const struct message *msg);
	void (*destroy)(struct mq_node *self);
};

struct mq_node {
	const char *name;
	const struct mq_ops *ops;
};

struct message_queue {
	struct message buf[MQ_CAP];
	int head, tail, count;
	struct mq_node *consumers[8];
	int n_consumers;
};

static void mq_init(struct message_queue *q)
{
	memset(q, 0, sizeof(*q));
}

static void mq_subscribe(struct message_queue *q, struct mq_node *node)
{
	if (q->n_consumers < 8) {
		q->consumers[q->n_consumers++] = node;
		printf("[MQ] subscribe consumer \"%s\"\n", node->name);
	}
}

static int mq_push(struct message_queue *q, int id, const char *payload)
{
	struct message *m;

	if (q->count >= MQ_CAP)
		return -1;
	m = &q->buf[q->tail];
	m->id = id;
	strncpy(m->payload, payload, sizeof(m->payload) - 1);
	m->payload[sizeof(m->payload) - 1] = '\0';
	q->tail = (q->tail + 1) % MQ_CAP;
	q->count++;
	printf("[Producer] enqueue id=%d payload=\"%s\"\n", id, payload);
	return 0;
}

static void mq_dispatch(struct message_queue *q)
{
	int i;

	printf("[Dispatcher] draining %d messages\n", q->count);
	while (q->count > 0) {
		struct message *m = &q->buf[q->head];

		for (i = 0; i < q->n_consumers; i++) {
			struct mq_node *n = q->consumers[i];

			if (n->ops && n->ops->on_message)
				n->ops->on_message(n, m);
		}
		q->head = (q->head + 1) % MQ_CAP;
		q->count--;
	}
}

/* Logger consumer */
struct logger_consumer {
	struct mq_node base;
	int seen;
};

static void logger_on_message(struct mq_node *self, const struct message *msg)
{
	struct logger_consumer *lc =
		container_of(self, struct logger_consumer, base);

	lc->seen++;
	printf("  [LoggerConsumer] #%d saw id=%d \"%s\"\n",
	       lc->seen, msg->id, msg->payload);
}

static const struct mq_ops logger_ops = {
	.on_message = logger_on_message,
};

/* Metrics consumer */
struct metrics_consumer {
	struct mq_node base;
	int bytes;
};

static void metrics_on_message(struct mq_node *self, const struct message *msg)
{
	struct metrics_consumer *mc =
		container_of(self, struct metrics_consumer, base);

	mc->bytes += (int)strlen(msg->payload);
	printf("  [MetricsConsumer] total_bytes=%d (msg id=%d)\n",
	       mc->bytes, msg->id);
}

static const struct mq_ops metrics_ops = {
	.on_message = metrics_on_message,
};

/* Alert consumer */
struct alert_consumer {
	struct mq_node base;
	int threshold;
	int alerts;
};

static void alert_on_message(struct mq_node *self, const struct message *msg)
{
	struct alert_consumer *ac =
		container_of(self, struct alert_consumer, base);

	if (msg->id >= ac->threshold) {
		ac->alerts++;
		printf("  [AlertConsumer] ALERT id=%d >= threshold=%d\n",
		       msg->id, ac->threshold);
	} else {
		printf("  [AlertConsumer] id=%d ok\n", msg->id);
	}
}

static const struct mq_ops alert_ops = {
	.on_message = alert_on_message,
};

int main(void)
{
	struct message_queue q;
	struct logger_consumer  logger = {
		.base = { .name = "logger",  .ops = &logger_ops  },
	};
	struct metrics_consumer metrics = {
		.base = { .name = "metrics", .ops = &metrics_ops },
	};
	struct alert_consumer   alert = {
		.base = { .name = "alert",   .ops = &alert_ops   },
		.threshold = 2,
	};

	printf("=== 09 Message Queue Framework ===\n");
	mq_init(&q);
	mq_subscribe(&q, &logger.base);
	mq_subscribe(&q, &metrics.base);
	mq_subscribe(&q, &alert.base);

	mq_push(&q, 1, "heartbeat");
	mq_push(&q, 2, "disk-full");
	mq_push(&q, 3, "oom-killer");

	mq_dispatch(&q);

	printf("Summary: logger.seen=%d metrics.bytes=%d alerts=%d\n",
	       logger.seen, metrics.bytes, alert.alerts);
	printf("Done.\n");
	return 0;
}
