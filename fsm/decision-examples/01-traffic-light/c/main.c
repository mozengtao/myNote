#include <stdio.h>
#include <string.h>

#define MAX_ACTIONS 8

/* ---------------------------------------------------------------------------
 * 1. Types
 * --------------------------------------------------------------------------- */

typedef enum {
	ST_RED = 0,
	ST_GREEN,
	ST_YELLOW,
	NSTATE
} State;

typedef enum {
	EV_TICK = 0,
	NEVENT
} Event;

typedef enum {
	ACT_LIGHT_RED = 0,
	ACT_LIGHT_GREEN,
	ACT_LIGHT_YELLOW
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
	{ ST_RED,    EV_TICK, ST_GREEN,  1, { ACT_LIGHT_GREEN  } },
	{ ST_GREEN,  EV_TICK, ST_YELLOW, 1, { ACT_LIGHT_YELLOW } },
	{ ST_YELLOW, EV_TICK, ST_RED,    1, { ACT_LIGHT_RED    } },
};

static const char *state_names[] = { "RED", "GREEN", "YELLOW" };
static const char *event_names[] = { "TICK" };
static const char *action_names[] = { "LIGHT_RED", "LIGHT_GREEN", "LIGHT_YELLOW" };

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
			memcpy(d.actions, table[i].actions,
			       sizeof(Action) * (unsigned)table[i].n_actions);
			return d;
		}
	}
	return d;
}

/* ---------------------------------------------------------------------------
 * 4. Workers  -- execution; workers do not know current state
 * --------------------------------------------------------------------------- */

/*
 * lamp worker: change the physical light
 */
static void
worker_lamp(Action a)
{
	switch (a) {
	case ACT_LIGHT_RED:
		printf("[EXEC] lamp -> RED\n");
		break;
	case ACT_LIGHT_GREEN:
		printf("[EXEC] lamp -> GREEN\n");
		break;
	case ACT_LIGHT_YELLOW:
		printf("[EXEC] lamp -> YELLOW\n");
		break;
	}
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * route each action to a worker
 */
static void
dispatch(const Decision *d)
{
	int i;

	for (i = 0; i < d->n_actions; i++) {
		printf("[DISPATCH] %s\n", action_names[d->actions[i]]);
		worker_lamp(d->actions[i]);
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
	Event seq[] = { EV_TICK, EV_TICK, EV_TICK, EV_TICK };
	unsigned i;

	state = ST_RED;
	printf("[INIT] state=%s\n\n", state_names[state]);
	for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
		state = apply(state, seq[i]);
		printf("\n");
	}
	return 0;
}
