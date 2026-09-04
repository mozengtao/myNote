/*
 * 04_plugin.c - Plugin framework: interface + loader + dynamic dispatch
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct plugin;

struct plugin_ops {
	int  (*init)(struct plugin *self);
	void (*execute)(struct plugin *self, const char *arg);
	void (*shutdown)(struct plugin *self);
};

struct plugin {
	const char *name;
	const char *version;
	const struct plugin_ops *ops;
};

/* Registry */
#define MAX_PLUGINS 8

struct plugin_registry {
	struct plugin *slots[MAX_PLUGINS];
	int count;
};

static void registry_register(struct plugin_registry *reg, struct plugin *p)
{
	if (reg->count >= MAX_PLUGINS)
		return;
	printf("[Loader] register plugin \"%s\" v%s\n", p->name, p->version);
	reg->slots[reg->count++] = p;
	if (p->ops && p->ops->init)
		p->ops->init(p);
}

static void registry_run_all(struct plugin_registry *reg, const char *arg)
{
	int i;

	for (i = 0; i < reg->count; i++) {
		struct plugin *p = reg->slots[i];

		printf("[Loader] execute %s(%s)\n", p->name, arg);
		if (p->ops && p->ops->execute)
			p->ops->execute(p, arg);
	}
}

static void registry_shutdown(struct plugin_registry *reg)
{
	int i;

	for (i = 0; i < reg->count; i++) {
		struct plugin *p = reg->slots[i];

		if (p->ops && p->ops->shutdown)
			p->ops->shutdown(p);
	}
	reg->count = 0;
}

/* ---- Logger plugin ---- */

struct logger_plugin {
	struct plugin base;
	int log_count;
};

static int logger_init(struct plugin *self)
{
	struct logger_plugin *lp = container_of(self, struct logger_plugin, base);

	lp->log_count = 0;
	printf("  [Logger] init ok\n");
	return 0;
}

static void logger_execute(struct plugin *self, const char *arg)
{
	struct logger_plugin *lp = container_of(self, struct logger_plugin, base);

	lp->log_count++;
	printf("  [Logger] LOG#%d: %s\n", lp->log_count, arg);
}

static void logger_shutdown(struct plugin *self)
{
	struct logger_plugin *lp = container_of(self, struct logger_plugin, base);

	printf("  [Logger] shutdown (logged %d msgs)\n", lp->log_count);
}

static const struct plugin_ops logger_ops = {
	.init     = logger_init,
	.execute  = logger_execute,
	.shutdown = logger_shutdown,
};

/* ---- Crypto plugin ---- */

struct crypto_plugin {
	struct plugin base;
	const char *algo;
};

static int crypto_init(struct plugin *self)
{
	struct crypto_plugin *cp = container_of(self, struct crypto_plugin, base);

	printf("  [Crypto] init algo=%s\n", cp->algo);
	return 0;
}

static void crypto_execute(struct plugin *self, const char *arg)
{
	struct crypto_plugin *cp = container_of(self, struct crypto_plugin, base);

	printf("  [Crypto] encrypt(\"%s\") with %s -> \"%s#hash\"\n",
	       arg, cp->algo, arg);
}

static void crypto_shutdown(struct plugin *self)
{
	(void)self;
	printf("  [Crypto] shutdown\n");
}

static const struct plugin_ops crypto_ops = {
	.init     = crypto_init,
	.execute  = crypto_execute,
	.shutdown = crypto_shutdown,
};

/* ---- Metrics plugin ---- */

struct metrics_plugin {
	struct plugin base;
	int samples;
};

static int metrics_init(struct plugin *self)
{
	struct metrics_plugin *mp =
		container_of(self, struct metrics_plugin, base);

	mp->samples = 0;
	printf("  [Metrics] init ok\n");
	return 0;
}

static void metrics_execute(struct plugin *self, const char *arg)
{
	struct metrics_plugin *mp =
		container_of(self, struct metrics_plugin, base);

	mp->samples++;
	printf("  [Metrics] sample#%d event=\"%s\"\n", mp->samples, arg);
}

static void metrics_shutdown(struct plugin *self)
{
	struct metrics_plugin *mp =
		container_of(self, struct metrics_plugin, base);

	printf("  [Metrics] shutdown (samples=%d)\n", mp->samples);
}

static const struct plugin_ops metrics_ops = {
	.init     = metrics_init,
	.execute  = metrics_execute,
	.shutdown = metrics_shutdown,
};

int main(void)
{
	struct plugin_registry reg = {0};
	struct logger_plugin  logger = {
		.base = { .name = "logger", .version = "1.0", .ops = &logger_ops },
	};
	struct crypto_plugin  crypto = {
		.base = { .name = "crypto", .version = "2.1", .ops = &crypto_ops },
		.algo = "AES-256",
	};
	struct metrics_plugin metrics = {
		.base = { .name = "metrics", .version = "0.9", .ops = &metrics_ops },
	};

	printf("=== 04 Plugin Framework ===\n");
	registry_register(&reg, &logger.base);
	registry_register(&reg, &crypto.base);
	registry_register(&reg, &metrics.base);

	registry_run_all(&reg, "hello-world");
	registry_run_all(&reg, "second-event");

	registry_shutdown(&reg);
	printf("Done.\n");
	return 0;
}
