#include <stdio.h>
#include <string.h>

#define MAX_ACTIONS 8

/* ---------------------------------------------------------------------------
 * 1. Types
 * --------------------------------------------------------------------------- */

typedef enum {
	ST_CLOSED = 0,
	ST_LISTEN,
	ST_SYN_SENT,
	ST_SYN_RCVD,
	ST_ESTABLISHED
} State;

typedef enum {
	EV_PASSIVE_OPEN = 0,
	EV_ACTIVE_OPEN,
	EV_SYN,
	EV_SYNACK,
	EV_ACK
} EventKind;

typedef enum {
	ACT_NIC_SEND = 0
} ActionKind;

typedef struct {
	EventKind kind;
	unsigned seq;
	unsigned ack;
} Event;

typedef struct {
	ActionKind kind;
	const char *flags;
	unsigned seq;
	unsigned ack;
} Action;

typedef struct {
	State state;
	unsigned local_seq;
	unsigned remote_seq;
} Ctx;

typedef struct {
	int accepted;
	State next_state;
	unsigned local_seq;
	unsigned remote_seq;
	int n_actions;
	Action actions[MAX_ACTIONS];
} Decision;

typedef struct {
	State from;
	EventKind event;
	State to;
	int n_actions;
	ActionKind action_kinds[MAX_ACTIONS];
} TransRule;

/* ---------------------------------------------------------------------------
 * 2. Table  (State, EventKind) -> (NextState, ActionKind list)
 * --------------------------------------------------------------------------- */

static const TransRule table[] = {
	{ ST_CLOSED,   EV_PASSIVE_OPEN, ST_LISTEN,      0, { 0 } },
	{ ST_LISTEN,   EV_SYN,          ST_SYN_RCVD,    1, { ACT_NIC_SEND } },
	{ ST_SYN_RCVD, EV_ACK,          ST_ESTABLISHED, 0, { 0 } },
	{ ST_CLOSED,   EV_ACTIVE_OPEN,  ST_SYN_SENT,    1, { ACT_NIC_SEND } },
	{ ST_SYN_SENT, EV_SYNACK,       ST_ESTABLISHED, 1, { ACT_NIC_SEND } },
};

static const char *state_names[] = {
	"CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED"
};
static const char *event_names[] = {
	"PASSIVE_OPEN", "ACTIVE_OPEN", "SYN", "SYNACK", "ACK"
};

/* ---------------------------------------------------------------------------
 * 3. Engine  -- table lookup + bind seq/ack from Ctx and Event
 * --------------------------------------------------------------------------- */

/*
 * fsm decide: kind selects the transition; seq/ack are copied from TCB + packet
 */
static Decision
fsm_decide(const Ctx *ctx, const Event *ev)
{
	Decision d;
	unsigned i;
	Action *a;

	memset(&d, 0, sizeof(d));
	d.accepted = 0;
	d.next_state = ctx->state;
	d.local_seq = ctx->local_seq;
	d.remote_seq = ctx->remote_seq;

	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (table[i].from != ctx->state || table[i].event != ev->kind)
			continue;
		d.accepted = 1;
		d.next_state = table[i].to;
		d.n_actions = table[i].n_actions;
		if (d.n_actions == 0)
			return d;
		a = &d.actions[0];
		a->kind = ACT_NIC_SEND;
		if (ev->kind == EV_SYN) {
			d.remote_seq = ev->seq;
			a->flags = "SYN+ACK";
			a->seq = ctx->local_seq;
			a->ack = ev->seq + 1;
		} else if (ev->kind == EV_ACTIVE_OPEN) {
			a->flags = "SYN";
			a->seq = ctx->local_seq;
			a->ack = 0;
		} else if (ev->kind == EV_SYNACK) {
			d.remote_seq = ev->seq;
			d.local_seq = ctx->local_seq + 1;
			a->flags = "ACK";
			a->seq = d.local_seq;
			a->ack = ev->seq + 1;
		}
		return d;
	}
	return d;
}

/* ---------------------------------------------------------------------------
 * 4. Workers  -- parameterized functions; no FSM state
 * --------------------------------------------------------------------------- */

/*
 * nic_send: put a TCP control packet on the wire
 */
static void
nic_send(const char *flags, unsigned seq, unsigned ack)
{
	printf("[EXEC] nic_send(%s, %u, %u)\n", flags, seq, ack);
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * unpack NIC_SEND and call nic_send(flags, seq, ack)
 */
static void
dispatch(const Decision *d)
{
	int i;
	const Action *a;

	for (i = 0; i < d->n_actions; i++) {
		a = &d->actions[i];
		printf("[DISPATCH] NIC_SEND(flags=%s, seq=%u, ack=%u)\n",
		       a->flags, a->seq, a->ack);
		nic_send(a->flags, a->seq, a->ack);
	}
}

/* ---------------------------------------------------------------------------
 * 6. Apply  -- commit ctx (including seq), then dispatch
 * --------------------------------------------------------------------------- */

/*
 * format control-packet payload; open events have no fields
 */
static void
format_event(const Event *ev, char *buf, int buflen)
{
	if (ev->kind == EV_SYN || ev->kind == EV_SYNACK || ev->kind == EV_ACK)
		snprintf(buf, (size_t)buflen, "%s(seq=%u, ack=%u)",
			 event_names[ev->kind], ev->seq, ev->ack);
	else
		snprintf(buf, (size_t)buflen, "%s", event_names[ev->kind]);
}

/*
 * handle one event: decide, maybe reject, else commit + dispatch
 */
static void
apply(Ctx *ctx, const Event *ev)
{
	Decision d;
	char evbuf[64];

	format_event(ev, evbuf, (int)sizeof(evbuf));
	printf("[EVENT] %s | state=%s\n", evbuf, state_names[ctx->state]);
	d = fsm_decide(ctx, ev);
	if (!d.accepted) {
		printf("[DECISION] REJECT %s + %s  (state unchanged)\n",
		       state_names[ctx->state], event_names[ev->kind]);
		return;
	}
	printf("[DECISION] %s + %s -> %s\n",
	       state_names[ctx->state], event_names[ev->kind],
	       state_names[d.next_state]);
	ctx->state = d.next_state;
	ctx->local_seq = d.local_seq;
	ctx->remote_seq = d.remote_seq;
	dispatch(&d);
}

/*
 * run one scripted handshake from CLOSED
 */
static void
run_scenario(const char *title, unsigned local_seq,
	     const Event *events, unsigned n)
{
	Ctx ctx;
	unsigned i;

	memset(&ctx, 0, sizeof(ctx));
	ctx.state = ST_CLOSED;
	ctx.local_seq = local_seq;
	printf("=== scenario: %s ===\n", title);
	printf("[INIT] state=%s local_seq=%u remote_seq=%u\n\n",
	       state_names[ctx.state], ctx.local_seq, ctx.remote_seq);
	for (i = 0; i < n; i++) {
		apply(&ctx, &events[i]);
		printf("\n");
	}
}

/* ---------------------------------------------------------------------------
 * 7. Main
 * --------------------------------------------------------------------------- */

int
main(void)
{
	Event server[] = {
		{ EV_PASSIVE_OPEN, 0, 0 },
		{ EV_SYN, 2000, 0 },
		{ EV_ACK, 2001, 1001 },
		{ EV_SYN, 2002, 1001 }
	};
	Event client[] = {
		{ EV_ACTIVE_OPEN, 0, 0 },
		{ EV_SYNACK, 8000, 5001 },
		{ EV_ACK, 8001, 5001 }
	};

	run_scenario("server", 1000, server, sizeof(server) / sizeof(server[0]));
	run_scenario("client", 5000, client, sizeof(client) / sizeof(client[0]));
	return 0;
}
