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
} Event;

typedef enum {
	ACT_SEND_SYN = 0,
	ACT_SEND_SYNACK,
	ACT_SEND_ACK
} Action;

typedef struct {
	int accepted;
	State next_state;
	int n_actions;
	Action actions[MAX_ACTIONS];
} Decision;

typedef struct {
	State from;
	Event event;
	State to;
	int n_actions;
	Action actions[MAX_ACTIONS];
} TransRule;

/* ---------------------------------------------------------------------------
 * 2. Table  (State, Event) -> (NextState, ActionList)
 * --------------------------------------------------------------------------- */

static const TransRule table[] = {
	{ ST_CLOSED,  EV_PASSIVE_OPEN, ST_LISTEN,      0, { 0 } },
	{ ST_LISTEN,  EV_SYN,          ST_SYN_RCVD,    1, { ACT_SEND_SYNACK } },
	{ ST_SYN_RCVD, EV_ACK,         ST_ESTABLISHED, 0, { 0 } },
	{ ST_CLOSED,  EV_ACTIVE_OPEN,  ST_SYN_SENT,    1, { ACT_SEND_SYN } },
	{ ST_SYN_SENT, EV_SYNACK,      ST_ESTABLISHED, 1, { ACT_SEND_ACK } },
};

static const char *state_names[] = {
	"CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED"
};
static const char *event_names[] = {
	"PASSIVE_OPEN", "ACTIVE_OPEN", "SYN", "SYNACK", "ACK"
};
static const char *action_names[] = {
	"SEND_SYN", "SEND_SYNACK", "SEND_ACK"
};

/* ---------------------------------------------------------------------------
 * 3. Engine  -- table lookup only, zero side effects
 * --------------------------------------------------------------------------- */

/*
 * fsm decide: lookup (state, event); never IO, never call workers
 */
static Decision
fsm_decide(State state, Event event)
{
	Decision d;
	unsigned i;

	memset(&d, 0, sizeof(d));
	d.accepted = 0;
	d.next_state = state;

	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (table[i].from == state && table[i].event == event) {
			d.accepted = 1;
			d.next_state = table[i].to;
			d.n_actions = table[i].n_actions;
			if (d.n_actions > 0)
				memcpy(d.actions, table[i].actions,
				       sizeof(Action) * (unsigned)d.n_actions);
			return d;
		}
	}
	return d;
}

/* ---------------------------------------------------------------------------
 * 4. Workers  -- execution; workers do not know current state
 * --------------------------------------------------------------------------- */

/*
 * nic worker: put the chosen TCP control packet on the wire
 */
static void
worker_nic(Action a)
{
	switch (a) {
	case ACT_SEND_SYN:
		printf("[EXEC] nic -> SEND SYN\n");
		break;
	case ACT_SEND_SYNACK:
		printf("[EXEC] nic -> SEND SYN+ACK\n");
		break;
	case ACT_SEND_ACK:
		printf("[EXEC] nic -> SEND ACK\n");
		break;
	}
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * route each action to the nic worker
 */
static void
dispatch(const Decision *d)
{
	int i;

	for (i = 0; i < d->n_actions; i++) {
		printf("[DISPATCH] %s\n", action_names[d->actions[i]]);
		worker_nic(d->actions[i]);
	}
}

/* ---------------------------------------------------------------------------
 * 6. Apply  -- commit next state, then dispatch
 * --------------------------------------------------------------------------- */

/*
 * format action names into "A, B"
 */
static void
format_actions(const Decision *d, char *buf, int buflen)
{
	int i, n;

	buf[0] = '\0';
	n = 0;
	for (i = 0; i < d->n_actions; i++) {
		if (i)
			n += snprintf(buf + n, (size_t)(buflen - n), ", ");
		n += snprintf(buf + n, (size_t)(buflen - n), "%s",
			      action_names[d->actions[i]]);
	}
}

/*
 * handle one event: decide, maybe reject, else commit + dispatch
 */
static State
apply(State state, Event event)
{
	Decision d;
	char acts[128];

	printf("[EVENT] %s | state=%s\n", event_names[event], state_names[state]);
	d = fsm_decide(state, event);
	if (!d.accepted) {
		printf("[DECISION] REJECT %s + %s  (state unchanged)\n",
		       state_names[state], event_names[event]);
		return state;
	}
	format_actions(&d, acts, (int)sizeof(acts));
	printf("[DECISION] %s + %s -> %s  actions=[%s]\n",
	       state_names[state], event_names[event],
	       state_names[d.next_state], acts);
	dispatch(&d);
	return d.next_state;
}

/*
 * run one scripted handshake from CLOSED
 */
static void
run_scenario(const char *title, const Event *events, unsigned n)
{
	State state;
	unsigned i;

	state = ST_CLOSED;
	printf("=== scenario: %s ===\n", title);
	printf("[INIT] state=%s\n\n", state_names[state]);
	for (i = 0; i < n; i++) {
		state = apply(state, events[i]);
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
		EV_PASSIVE_OPEN, EV_SYN, EV_ACK, EV_SYN
	};
	Event client[] = {
		EV_ACTIVE_OPEN, EV_SYNACK, EV_ACK
	};

	run_scenario("server", server, sizeof(server) / sizeof(server[0]));
	run_scenario("client", client, sizeof(client) / sizeof(client[0]));
	return 0;
}
