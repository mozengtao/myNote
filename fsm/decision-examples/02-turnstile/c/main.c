#include <stdio.h>
#include <string.h>

#define MAX_ACTIONS 8

/* ---------------------------------------------------------------------------
 * 1. Types
 * --------------------------------------------------------------------------- */

typedef enum {
	ST_LOCKED = 0,
	ST_UNLOCKED
} State;

typedef enum {
	EV_COIN = 0,
	EV_PUSH
} EventKind;

typedef enum {
	ACT_CREDIT_BOX = 0,
	ACT_SET_MECHANISM
} ActionKind;

typedef struct {
	EventKind kind;
	int cents;
} Event;

typedef struct {
	ActionKind kind;
	int cents;
	int locked;
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
	{
		ST_LOCKED, EV_COIN, ST_UNLOCKED, 2,
		{ ACT_CREDIT_BOX, ACT_SET_MECHANISM }
	},
	{ ST_UNLOCKED, EV_PUSH, ST_LOCKED, 1, { ACT_SET_MECHANISM } },
};

static const char *state_names[] = { "LOCKED", "UNLOCKED" };
static const char *event_names[] = { "COIN", "PUSH" };

/* ---------------------------------------------------------------------------
 * 3. Engine  -- table lookup + bind args; zero side effects
 * --------------------------------------------------------------------------- */

/*
 * fsm decide: lookup by kind; cents from event, locked from next state
 */
static Decision
fsm_decide(const Ctx *ctx, const Event *ev)
{
	Decision d;
	unsigned i;
	int k;

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
			d.actions[k].kind = table[i].action_kinds[k];
			if (d.actions[k].kind == ACT_CREDIT_BOX)
				d.actions[k].cents = ev->cents;
			else
				d.actions[k].locked =
				    (d.next_state == ST_UNLOCKED) ? 0 : 1;
		}
		return d;
	}
	return d;
}

/* ---------------------------------------------------------------------------
 * 4. Workers  -- parameterized functions; no FSM state
 * --------------------------------------------------------------------------- */

/*
 * coin_box_credit: drop coins into the cash box
 */
static void
coin_box_credit(int cents)
{
	printf("[EXEC] coin_box_credit(%d)\n", cents);
}

/*
 * mechanism_set: 0 = unlocked, 1 = locked
 */
static void
mechanism_set(int locked)
{
	printf("[EXEC] mechanism_set(%d)\n", locked);
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * unpack CREDIT_BOX / SET_MECHANISM and call workers with args
 */
static void
dispatch(const Decision *d)
{
	int i;
	const Action *a;

	for (i = 0; i < d->n_actions; i++) {
		a = &d->actions[i];
		if (a->kind == ACT_CREDIT_BOX) {
			printf("[DISPATCH] CREDIT_BOX(cents=%d)\n", a->cents);
			coin_box_credit(a->cents);
		} else {
			printf("[DISPATCH] SET_MECHANISM(locked=%d)\n", a->locked);
			mechanism_set(a->locked);
		}
	}
}

/* ---------------------------------------------------------------------------
 * 6. Apply  -- commit ctx, then dispatch
 * --------------------------------------------------------------------------- */

/*
 * format COIN payload; PUSH has no fields
 */
static void
format_event(const Event *ev, char *buf, int buflen)
{
	if (ev->kind == EV_COIN)
		snprintf(buf, (size_t)buflen, "COIN(cents=%d)", ev->cents);
	else
		snprintf(buf, (size_t)buflen, "PUSH");
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
		{ EV_COIN, 100 },
		{ EV_PUSH, 0 },
		{ EV_PUSH, 0 },
		{ EV_COIN, 100 },
		{ EV_COIN, 50 },
		{ EV_PUSH, 0 }
	};
	unsigned i;

	memset(&ctx, 0, sizeof(ctx));
	ctx.state = ST_LOCKED;
	printf("[INIT] state=%s\n\n", state_names[ctx.state]);
	for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
		apply(&ctx, &seq[i]);
		printf("\n");
	}
	return 0;
}
