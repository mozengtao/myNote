/*
 * pubsub_event_bus.c
 *
 * 场景：一个通用发布订阅事件总线（pub/sub event bus），按"事件主题
 * (topic)"分组订阅——与 notifier chain 的差异在于：notifier chain
 * 通常是"一条链响应一类事件"，pub/sub 则是"任意多个 topic，每个 topic
 * 独立维护一条订阅链"，是 notifier chain 思想的横向扩展。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o pubsub_event_bus pubsub_event_bus.c
 * 运行：
 *   ./pubsub_event_bus
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOPICS 8
#define MAX_SUBS_PER_TOPIC 8
#define MAX_TOPIC_NAME 32

struct subscriber {
	char name[32];
	void (*on_message)(struct subscriber *self, const char *topic, const char *payload);
	int  messages_received;
};

struct topic_entry {
	char name[MAX_TOPIC_NAME];
	struct subscriber *subs[MAX_SUBS_PER_TOPIC];
	int nr_subs;
};

struct event_bus {
	struct topic_entry topics[MAX_TOPICS];
	int nr_topics;
};

static struct event_bus g_bus = { .nr_topics = 0 };

static struct topic_entry *bus_find_or_create_topic(const char *name)
{
	int i;
	for (i = 0; i < g_bus.nr_topics; i++) {
		if (strcmp(g_bus.topics[i].name, name) == 0)
			return &g_bus.topics[i];
	}
	if (g_bus.nr_topics >= MAX_TOPICS)
		return NULL;

	struct topic_entry *t = &g_bus.topics[g_bus.nr_topics++];
	memset(t, 0, sizeof(*t));
	strncpy(t->name, name, sizeof(t->name) - 1);
	printf("[bus] created new topic '%s'\n", name);
	return t;
}

/* bus_subscribe()：注册接口，等价于 notifier_chain_register，但按 topic 分组 */
static int bus_subscribe(const char *topic, struct subscriber *sub)
{
	struct topic_entry *t = bus_find_or_create_topic(topic);
	if (!t || t->nr_subs >= MAX_SUBS_PER_TOPIC)
		return -1;

	t->subs[t->nr_subs++] = sub;
	printf("[bus] '%s' subscribed to topic '%s'\n", sub->name, topic);
	return 0;
}

static void bus_unsubscribe(const char *topic, struct subscriber *sub)
{
	int i;
	for (i = 0; i < g_bus.nr_topics; i++) {
		if (strcmp(g_bus.topics[i].name, topic) != 0)
			continue;
		struct topic_entry *t = &g_bus.topics[i];
		int j;
		for (j = 0; j < t->nr_subs; j++) {
			if (t->subs[j] == sub) {
				t->subs[j] = t->subs[--t->nr_subs];
				printf("[bus] '%s' unsubscribed from topic '%s'\n", sub->name, topic);
				return;
			}
		}
	}
}

/* bus_publish()：广播——对应 notifier_call_chain，但作用域限定在单个 topic */
static void bus_publish(const char *topic, const char *payload)
{
	int i;
	for (i = 0; i < g_bus.nr_topics; i++) {
		if (strcmp(g_bus.topics[i].name, topic) != 0)
			continue;
		struct topic_entry *t = &g_bus.topics[i];
		int j;
		printf("[bus] publishing to topic '%s' (%d subscriber(s)): \"%s\"\n",
		       topic, t->nr_subs, payload);
		for (j = 0; j < t->nr_subs; j++) {
			struct subscriber *s = t->subs[j];
			s->messages_received++;
			s->on_message(s, topic, payload);   /* <-- 盲调 */
		}
		return;
	}
	printf("[bus] publish to unknown topic '%s' (no subscribers, dropped)\n", topic);
}

/* ------------------------------------------------------------------ */
/* 订阅者实现 A：审计日志（订阅所有 topic 的一个简化做法：分别订阅两次）*/
/* ------------------------------------------------------------------ */

static void audit_on_message(struct subscriber *self, const char *topic, const char *payload)
{
	printf("  [%s] AUDIT LOG: topic='%s' payload=\"%s\" (total seen=%d)\n",
	       self->name, topic, payload, self->messages_received);
}

static struct subscriber audit_sub = { .name = "audit-logger", .on_message = audit_on_message };

/* ------------------------------------------------------------------ */
/* 订阅者实现 B：告警系统，只关心 "system.error" topic                 */
/* ------------------------------------------------------------------ */

static void alert_on_message(struct subscriber *self, const char *topic, const char *payload)
{
	printf("  [%s] !! PAGE ON-CALL !! topic='%s' payload=\"%s\"\n", self->name, topic, payload);
}

static struct subscriber alert_sub = { .name = "pager-duty", .on_message = alert_on_message };

/* ------------------------------------------------------------------ */
/* 订阅者实现 C：指标统计，关心 "order.created" topic                   */
/* ------------------------------------------------------------------ */

static void metrics_on_message(struct subscriber *self, const char *topic, const char *payload)
{
	(void)payload;
	printf("  [%s] incrementing counter for topic='%s' (count=%d)\n",
	       self->name, topic, self->messages_received);
}

static struct subscriber metrics_sub = { .name = "metrics-collector", .on_message = metrics_on_message };

int main(void)
{
	printf("=== pubsub_event_bus demo ===\n\n");

	bus_subscribe("system.error", &audit_sub);
	bus_subscribe("system.error", &alert_sub);
	bus_subscribe("order.created", &audit_sub);
	bus_subscribe("order.created", &metrics_sub);

	printf("\n-- 发布事件 --\n");
	bus_publish("order.created", "order#1001 placed by user42");
	printf("\n");
	bus_publish("system.error", "disk /dev/sda1 read error");
	printf("\n");
	bus_publish("unknown.topic", "nobody is listening");

	printf("\n-- 取消订阅 alert_sub，再发一次 error --\n");
	bus_unsubscribe("system.error", &alert_sub);
	bus_publish("system.error", "second disk error, no paging this time");

	printf("\n=== done ===\n");
	printf("audit_sub total messages   = %d\n", audit_sub.messages_received);
	printf("alert_sub total messages   = %d\n", alert_sub.messages_received);
	printf("metrics_sub total messages = %d\n", metrics_sub.messages_received);
	return 0;
}
