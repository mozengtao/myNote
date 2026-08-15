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
} EventKind;

typedef enum {
	ACT_MOTOR_RUN = 0,
	ACT_LATCH_SET
} ActionKind;

typedef enum {
	DIR_OPEN = 0,
	DIR_CLOSE
} Dir;

typedef struct {
	EventKind kind;
	int sensor_id;
	unsigned elapsed_ms;
} Event;

typedef struct {
	ActionKind kind;
	Dir direction;
	int duty;
	int open;
} Action;

typedef struct {
	State state;
} Ctx;

typedef struct {
	int accepted;
	State next_state;
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
	{ ST_CLOSED,  EV_PERSON,  ST_OPENING, 1, { ACT_MOTOR_RUN } },
	{ ST_OPENING, EV_TIMEOUT, ST_OPEN,    1, { ACT_LATCH_SET } },
	{ ST_OPEN,    EV_PERSON,  ST_OPEN,    0, { 0 } },
	{ ST_OPEN,    EV_TIMEOUT, ST_CLOSING, 1, { ACT_MOTOR_RUN } },
	{ ST_CLOSING, EV_PERSON,  ST_OPENING, 1, { ACT_MOTOR_RUN } },
	{ ST_CLOSING, EV_TIMEOUT, ST_CLOSED,  1, { ACT_LATCH_SET } },
};

static const char *state_names[] = {
	"CLOSED", "OPENING", "OPEN", "CLOSING"
};
static const char *event_names[] = { "PERSON", "TIMEOUT" };
static const char *dir_names[] = { "OPEN", "CLOSE" };

/* ---------------------------------------------------------------------------
 * 3. Engine  -- table lookup + bind args; zero side effects
 * --------------------------------------------------------------------------- */

/*
 * fsm decide: dir/duty and latch position come from the next state
 */
static Decision
fsm_decide(const Ctx *ctx, const Event *ev)
{
	Decision d;
	unsigned i;
	int k;
	Action *a;

	memset(&d, 0, sizeof(d));
	d.accepted = 0;
	d.next_state = ctx->state;

	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (table[i].from != ctx->state || table[i].event != ev->kind)
			continue;
		d.accepted = 1;
		d.next_state = table[i].to;
		d.n_actions = table[i].n_actions;
		for (k = 0; k < d.n_actions; k++) {
			a = &d.actions[k];
			a->kind = table[i].action_kinds[k];
			if (a->kind == ACT_MOTOR_RUN) {
				a->direction = (d.next_state == ST_OPENING)
				    ? DIR_OPEN : DIR_CLOSE;
				a->duty = 80;
			} else {
				a->open = (d.next_state == ST_OPEN) ? 1 : 0;
			}
		}
		return d;
	}
	return d;
}

/* ---------------------------------------------------------------------------
 * 4. Workers  -- parameterized functions; no FSM state
 * --------------------------------------------------------------------------- */

/*
 * motor_run: drive the door in a direction at a PWM duty
 */
static void
motor_run(Dir direction, int duty)
{
	printf("[EXEC] motor_run(%s, %d)\n", dir_names[direction], duty);
}

/*
 * latch_set: 1 = hold open, 0 = hold closed
 */
static void
latch_set(int open)
{
	printf("[EXEC] latch_set(%d)\n", open);
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * unpack MOTOR_RUN / LATCH_SET and call workers with args
 */
static void
dispatch(const Decision *d)
{
	int i;
	const Action *a;

	for (i = 0; i < d->n_actions; i++) {
		a = &d->actions[i];
		if (a->kind == ACT_MOTOR_RUN) {
			printf("[DISPATCH] MOTOR_RUN(dir=%s, duty=%d)\n",
			       dir_names[a->direction], a->duty);
			motor_run(a->direction, a->duty);
		} else {
			printf("[DISPATCH] LATCH_SET(open=%d)\n", a->open);
			latch_set(a->open);
		}
	}
}

/* ---------------------------------------------------------------------------
 * 6. Apply  -- commit ctx, then dispatch
 * --------------------------------------------------------------------------- */

/*
 * format PERSON / TIMEOUT payload
 */
static void
format_event(const Event *ev, char *buf, int buflen)
{
	if (ev->kind == EV_PERSON)
		snprintf(buf, (size_t)buflen, "PERSON(sensor_id=%d)",
			 ev->sensor_id);
	else
		snprintf(buf, (size_t)buflen, "TIMEOUT(elapsed_ms=%u)",
			 ev->elapsed_ms);
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
	dispatch(&d);
}

/* ---------------------------------------------------------------------------
 * 7. Main
 * --------------------------------------------------------------------------- */

int
main(void)
{
	Ctx ctx;
	Event seq[] = {
		{ EV_TIMEOUT, 0, 500 },
		{ EV_PERSON,  1, 0 },
		{ EV_TIMEOUT, 0, 2000 },
		{ EV_PERSON,  1, 0 },
		{ EV_TIMEOUT, 0, 5000 },
		{ EV_PERSON,  2, 0 },
		{ EV_TIMEOUT, 0, 2000 },
		{ EV_TIMEOUT, 0, 5000 },
		{ EV_TIMEOUT, 0, 2000 }
	};
	unsigned i;

	memset(&ctx, 0, sizeof(ctx));
	ctx.state = ST_CLOSED;
	printf("[INIT] state=%s\n\n", state_names[ctx.state]);
	for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
		apply(&ctx, &seq[i]);
		printf("\n");
	}
	return 0;
}
