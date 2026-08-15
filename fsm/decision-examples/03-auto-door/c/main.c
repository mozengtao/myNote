#include <stdio.h>
#include <string.h>

#define MAX_ACTIONS 8

/* ---------------------------------------------------------------------------
 * 1. Types
 * --------------------------------------------------------------------------- */

typedef enum {
	ST_CLOSED = 0,
	ST_OPENING,
	ST_OPEN,
	ST_CLOSING
} State;

typedef enum {
	EV_PERSON = 0,
	EV_TIMEOUT
} Event;

typedef enum {
	ACT_MOTOR_OPEN = 0,
	ACT_MOTOR_CLOSE,
	ACT_LATCH_OPEN,
	ACT_LATCH_CLOSED
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
	{ ST_CLOSED,  EV_PERSON,  ST_OPENING, 1, { ACT_MOTOR_OPEN   } },
	{ ST_OPENING, EV_TIMEOUT, ST_OPEN,    1, { ACT_LATCH_OPEN   } },
	{ ST_OPEN,    EV_PERSON,  ST_OPEN,    0, { 0 } },
	{ ST_OPEN,    EV_TIMEOUT, ST_CLOSING, 1, { ACT_MOTOR_CLOSE  } },
	{ ST_CLOSING, EV_PERSON,  ST_OPENING, 1, { ACT_MOTOR_OPEN   } },
	{ ST_CLOSING, EV_TIMEOUT, ST_CLOSED,  1, { ACT_LATCH_CLOSED } },
};

static const char *state_names[] = {
	"CLOSED", "OPENING", "OPEN", "CLOSING"
};
static const char *event_names[] = { "PERSON", "TIMEOUT" };
static const char *action_names[] = {
	"MOTOR_OPEN", "MOTOR_CLOSE", "LATCH_OPEN", "LATCH_CLOSED"
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
 * motor worker: drive the door open or closed
 */
static void
worker_motor(Action a)
{
	switch (a) {
	case ACT_MOTOR_OPEN:
		printf("[EXEC] motor -> OPEN\n");
		break;
	case ACT_MOTOR_CLOSE:
		printf("[EXEC] motor -> CLOSE\n");
		break;
	default:
		break;
	}
}

/*
 * latch worker: hold the door fully open or fully closed
 */
static void
worker_latch(Action a)
{
	switch (a) {
	case ACT_LATCH_OPEN:
		printf("[EXEC] latch -> OPEN\n");
		break;
	case ACT_LATCH_CLOSED:
		printf("[EXEC] latch -> CLOSED\n");
		break;
	default:
		break;
	}
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * route each action to motor or latch
 */
static void
dispatch(const Decision *d)
{
	int i;
	Action a;

	for (i = 0; i < d->n_actions; i++) {
		a = d->actions[i];
		printf("[DISPATCH] %s\n", action_names[a]);
		if (a == ACT_MOTOR_OPEN || a == ACT_MOTOR_CLOSE)
			worker_motor(a);
		else
			worker_latch(a);
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

/* ---------------------------------------------------------------------------
 * 7. Main
 * --------------------------------------------------------------------------- */

int
main(void)
{
	State state;
	Event seq[] = {
		EV_TIMEOUT,
		EV_PERSON,
		EV_TIMEOUT,
		EV_PERSON,
		EV_TIMEOUT,
		EV_PERSON,
		EV_TIMEOUT,
		EV_TIMEOUT,
		EV_TIMEOUT
	};
	unsigned i;

	state = ST_CLOSED;
	printf("[INIT] state=%s\n\n", state_names[state]);
	for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
		state = apply(state, seq[i]);
		printf("\n");
	}
	return 0;
}
