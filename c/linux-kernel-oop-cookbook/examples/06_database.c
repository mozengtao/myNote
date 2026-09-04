/*
 * 06_database.c - Database driver interface: MySQL / SQLite / PostgreSQL
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct db_driver;

struct db_ops {
	int  (*open)(struct db_driver *self, const char *dsn);
	int  (*query)(struct db_driver *self, const char *sql);
	int  (*close)(struct db_driver *self);
};

struct db_driver {
	const char *name;
	int connected;
	const struct db_ops *ops;
};

static int db_open(struct db_driver *self, const char *dsn)
{
	return (self && self->ops && self->ops->open)
		? self->ops->open(self, dsn) : -1;
}

static int db_query(struct db_driver *self, const char *sql)
{
	return (self && self->ops && self->ops->query)
		? self->ops->query(self, sql) : -1;
}

static int db_close(struct db_driver *self)
{
	return (self && self->ops && self->ops->close)
		? self->ops->close(self) : -1;
}

/* MySQL */
struct mysql_driver {
	struct db_driver base;
	const char *host;
	int port;
};

static int mysql_open(struct db_driver *self, const char *dsn)
{
	struct mysql_driver *m = container_of(self, struct mysql_driver, base);

	m->host = "127.0.0.1";
	m->port = 3306;
	self->connected = 1;
	printf("[MySQL] open dsn=%s -> %s:%d\n", dsn, m->host, m->port);
	return 0;
}

static int mysql_query(struct db_driver *self, const char *sql)
{
	struct mysql_driver *m = container_of(self, struct mysql_driver, base);

	printf("[MySQL] query on %s:%d => \"%s\" -> 3 rows\n",
	       m->host, m->port, sql);
	return 3;
}

static int mysql_close(struct db_driver *self)
{
	printf("[MySQL] close\n");
	self->connected = 0;
	return 0;
}

static const struct db_ops mysql_ops = {
	.open  = mysql_open,
	.query = mysql_query,
	.close = mysql_close,
};

/* SQLite */
struct sqlite_driver {
	struct db_driver base;
	const char *path;
};

static int sqlite_open(struct db_driver *self, const char *dsn)
{
	struct sqlite_driver *s = container_of(self, struct sqlite_driver, base);

	s->path = dsn;
	self->connected = 1;
	printf("[SQLite] open file=%s\n", s->path);
	return 0;
}

static int sqlite_query(struct db_driver *self, const char *sql)
{
	struct sqlite_driver *s = container_of(self, struct sqlite_driver, base);

	printf("[SQLite] query on %s => \"%s\" -> 1 row\n", s->path, sql);
	return 1;
}

static int sqlite_close(struct db_driver *self)
{
	printf("[SQLite] close\n");
	self->connected = 0;
	return 0;
}

static const struct db_ops sqlite_ops = {
	.open  = sqlite_open,
	.query = sqlite_query,
	.close = sqlite_close,
};

/* PostgreSQL */
struct pgsql_driver {
	struct db_driver base;
	const char *db;
	int backend_pid;
};

static int pgsql_open(struct db_driver *self, const char *dsn)
{
	struct pgsql_driver *p = container_of(self, struct pgsql_driver, base);

	p->db = "appdb";
	p->backend_pid = 4242;
	self->connected = 1;
	printf("[PgSQL] open dsn=%s db=%s backend_pid=%d\n",
	       dsn, p->db, p->backend_pid);
	return 0;
}

static int pgsql_query(struct db_driver *self, const char *sql)
{
	struct pgsql_driver *p = container_of(self, struct pgsql_driver, base);

	printf("[PgSQL] query pid=%d => \"%s\" -> 5 rows\n",
	       p->backend_pid, sql);
	return 5;
}

static int pgsql_close(struct db_driver *self)
{
	printf("[PgSQL] close\n");
	self->connected = 0;
	return 0;
}

static const struct db_ops pgsql_ops = {
	.open  = pgsql_open,
	.query = pgsql_query,
	.close = pgsql_close,
};

static void use_db(struct db_driver *drv, const char *dsn, const char *sql)
{
	int rows;

	db_open(drv, dsn);
	rows = db_query(drv, sql);
	printf("  -> framework got %d rows from %s\n", rows, drv->name);
	db_close(drv);
	printf("\n");
}

int main(void)
{
	struct mysql_driver  mysql = {
		.base = { .name = "mysql",  .ops = &mysql_ops  },
	};
	struct sqlite_driver sqlite = {
		.base = { .name = "sqlite", .ops = &sqlite_ops },
	};
	struct pgsql_driver  pgsql = {
		.base = { .name = "pgsql",  .ops = &pgsql_ops  },
	};

	printf("=== 06 Database Driver ===\n");
	use_db(&mysql.base,  "mysql://local",   "SELECT * FROM users");
	use_db(&sqlite.base, "/tmp/app.db",     "SELECT count(*) FROM t");
	use_db(&pgsql.base,  "pgsql://local",   "SELECT id FROM orders");
	printf("Done.\n");
	return 0;
}
