/*
 * 05_protocol.c - Network protocol family: TCP / UDP / QUIC
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct proto;

struct proto_ops {
	int  (*connect)(struct proto *self, const char *peer);
	int  (*send)(struct proto *self, const char *data, int len);
	int  (*recv)(struct proto *self, char *buf, int len);
	void (*close)(struct proto *self);
};

struct proto {
	const char *name;
	int fd;
	const struct proto_ops *ops;
};

static int proto_connect(struct proto *self, const char *peer)
{
	return (self && self->ops && self->ops->connect)
		? self->ops->connect(self, peer) : -1;
}

static int proto_send(struct proto *self, const char *data, int len)
{
	return (self && self->ops && self->ops->send)
		? self->ops->send(self, data, len) : -1;
}

static void proto_close(struct proto *self)
{
	if (self && self->ops && self->ops->close)
		self->ops->close(self);
}

/* TCP */
struct tcp_proto {
	struct proto base;
	int seq;
	int ack;
};

static int tcp_connect(struct proto *self, const char *peer)
{
	struct tcp_proto *t = container_of(self, struct tcp_proto, base);

	self->fd = 100;
	t->seq = 1;
	t->ack = 0;
	printf("[TCP] 3-way handshake to %s fd=%d seq=%d\n",
	       peer, self->fd, t->seq);
	return 0;
}

static int tcp_send(struct proto *self, const char *data, int len)
{
	struct tcp_proto *t = container_of(self, struct tcp_proto, base);

	t->seq += len;
	printf("[TCP] send %d bytes seq=%d data=\"%.*s\"\n",
	       len, t->seq, len, data);
	return len;
}

static void tcp_close(struct proto *self)
{
	printf("[TCP] FIN/ACK close fd=%d\n", self->fd);
	self->fd = -1;
}

static const struct proto_ops tcp_ops = {
	.connect = tcp_connect,
	.send    = tcp_send,
	.close   = tcp_close,
};

/* UDP */
struct udp_proto {
	struct proto base;
	int datagrams;
};

static int udp_connect(struct proto *self, const char *peer)
{
	struct udp_proto *u = container_of(self, struct udp_proto, base);

	self->fd = 200;
	u->datagrams = 0;
	printf("[UDP] bind/connectless peer=%s fd=%d\n", peer, self->fd);
	return 0;
}

static int udp_send(struct proto *self, const char *data, int len)
{
	struct udp_proto *u = container_of(self, struct udp_proto, base);

	u->datagrams++;
	printf("[UDP] datagram#%d len=%d data=\"%.*s\"\n",
	       u->datagrams, len, len, data);
	return len;
}

static void udp_close(struct proto *self)
{
	struct udp_proto *u = container_of(self, struct udp_proto, base);

	printf("[UDP] close fd=%d sent=%d datagrams\n",
	       self->fd, u->datagrams);
	self->fd = -1;
}

static const struct proto_ops udp_ops = {
	.connect = udp_connect,
	.send    = udp_send,
	.close   = udp_close,
};

/* QUIC */
struct quic_proto {
	struct proto base;
	int stream_id;
	int encrypted;
};

static int quic_connect(struct proto *self, const char *peer)
{
	struct quic_proto *q = container_of(self, struct quic_proto, base);

	self->fd = 300;
	q->stream_id = 0;
	q->encrypted = 1;
	printf("[QUIC] TLS1.3+QUIC handshake to %s fd=%d encrypted=%d\n",
	       peer, self->fd, q->encrypted);
	return 0;
}

static int quic_send(struct proto *self, const char *data, int len)
{
	struct quic_proto *q = container_of(self, struct quic_proto, base);

	q->stream_id++;
	printf("[QUIC] stream=%d len=%d data=\"%.*s\"\n",
	       q->stream_id, len, len, data);
	return len;
}

static void quic_close(struct proto *self)
{
	printf("[QUIC] CONNECTION_CLOSE fd=%d\n", self->fd);
	self->fd = -1;
}

static const struct proto_ops quic_ops = {
	.connect = quic_connect,
	.send    = quic_send,
	.close   = quic_close,
};

static void exercise(struct proto *p, const char *peer, const char *msg)
{
	proto_connect(p, peer);
	proto_send(p, msg, (int)strlen(msg));
	proto_close(p);
	printf("\n");
}

int main(void)
{
	struct tcp_proto  tcp  = { .base = { .name = "tcp",  .ops = &tcp_ops  } };
	struct udp_proto  udp  = { .base = { .name = "udp",  .ops = &udp_ops  } };
	struct quic_proto quic = { .base = { .name = "quic", .ops = &quic_ops } };

	printf("=== 05 Network Protocol ===\n");
	exercise(&tcp.base,  "10.0.0.1:443", "GET /");
	exercise(&udp.base,  "8.8.8.8:53",   "DNS-query");
	exercise(&quic.base, "1.1.1.1:443",  "HTTP/3");
	printf("Done.\n");
	return 0;
}
