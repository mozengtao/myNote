#include <stdio.h>
#include <string.h>

#define MAX_ACTIONS 8

/* ---------------------------------------------------------------------------
 * 1. Types
 * --------------------------------------------------------------------------- */

typedef enum {
	ST_CREATED = 0,
	ST_PAID,
	ST_SHIPPED,
	ST_CANCELLED
} State;

typedef enum {
	EV_PAY_SUCCESS = 0,
	EV_SHIP,
	EV_CANCEL
} Event;

typedef enum {
	ACT_DEDUCT_STOCK = 0,
	ACT_NOTIFY_LOGISTICS,
	ACT_ADD_POINTS,
	ACT_SEND_SMS,
	ACT_RELEASE_STOCK
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
	{
		ST_CREATED, EV_PAY_SUCCESS, ST_PAID, 4,
		{
			ACT_DEDUCT_STOCK,
			ACT_NOTIFY_LOGISTICS,
			ACT_ADD_POINTS,
			ACT_SEND_SMS
		}
	},
	{ ST_PAID,    EV_SHIP,   ST_SHIPPED,   1, { ACT_SEND_SMS } },
	{ ST_CREATED, EV_CANCEL, ST_CANCELLED, 1, { ACT_SEND_SMS } },
	{
		ST_PAID, EV_CANCEL, ST_CANCELLED, 2,
		{ ACT_RELEASE_STOCK, ACT_SEND_SMS }
	},
};

static const char *state_names[] = {
	"CREATED", "PAID", "SHIPPED", "CANCELLED"
};
static const char *event_names[] = {
	"PAY_SUCCESS", "SHIP", "CANCEL"
};
static const char *action_names[] = {
	"DEDUCT_STOCK", "NOTIFY_LOGISTICS", "ADD_POINTS",
	"SEND_SMS", "RELEASE_STOCK"
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
 * inventory worker: deduct or release stock
 */
static void
worker_inventory(Action a)
{
	switch (a) {
	case ACT_DEDUCT_STOCK:
		printf("[EXEC] inventory -> DEDUCT\n");
		break;
	case ACT_RELEASE_STOCK:
		printf("[EXEC] inventory -> RELEASE\n");
		break;
	default:
		break;
	}
}

/*
 * logistics worker: create a shipment
 */
static void
worker_logistics(Action a)
{
	if (a == ACT_NOTIFY_LOGISTICS)
		printf("[EXEC] logistics -> CREATE_SHIPMENT\n");
}

/*
 * points worker: credit loyalty points
 */
static void
worker_points(Action a)
{
	if (a == ACT_ADD_POINTS)
		printf("[EXEC] points -> ADD\n");
}

/*
 * sms worker: send a notification
 */
static void
worker_sms(Action a)
{
	if (a == ACT_SEND_SMS)
		printf("[EXEC] sms -> SEND\n");
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * route each action to the matching worker
 */
static void
dispatch(const Decision *d)
{
	int i;
	Action a;

	for (i = 0; i < d->n_actions; i++) {
		a = d->actions[i];
		printf("[DISPATCH] %s\n", action_names[a]);
		switch (a) {
		case ACT_DEDUCT_STOCK:
		case ACT_RELEASE_STOCK:
			worker_inventory(a);
			break;
		case ACT_NOTIFY_LOGISTICS:
			worker_logistics(a);
			break;
		case ACT_ADD_POINTS:
			worker_points(a);
			break;
		case ACT_SEND_SMS:
			worker_sms(a);
			break;
		}
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
	char acts[256];

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
 * run one scripted order from CREATED
 */
static void
run_scenario(const char *title, const Event *events, unsigned n)
{
	State state;
	unsigned i;

	state = ST_CREATED;
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
	Event happy[] = {
		EV_SHIP, EV_PAY_SUCCESS, EV_SHIP, EV_CANCEL
	};
	Event cancel_created[] = { EV_CANCEL };
	Event cancel_paid[] = { EV_PAY_SUCCESS, EV_CANCEL };

	run_scenario("happy-path", happy,
		     sizeof(happy) / sizeof(happy[0]));
	run_scenario("cancel-created", cancel_created,
		     sizeof(cancel_created) / sizeof(cancel_created[0]));
	run_scenario("cancel-paid", cancel_paid,
		     sizeof(cancel_paid) / sizeof(cancel_paid[0]));
	return 0;
}
