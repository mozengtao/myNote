#include <stdio.h>
#include <string.h>

#define MAX_ACTIONS 8

/* ---------------------------------------------------------------------------
 * 1. Types
 * --------------------------------------------------------------------------- */

typedef enum {
	ST_RED = 0,
	ST_GREEN,
	ST_YELLOW
} State;

typedef enum {
	EV_TICK = 0
} EventKind;

typedef enum {
	ACT_SET_LAMP = 0
} ActionKind;

typedef enum {
	COLOR_RED = 0,
	COLOR_GREEN,
	COLOR_YELLOW
} Color;

typedef struct {详细解释如下python代码
@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]
	EventKind kind;
	unsigned now_ms;
} Event;

typedef struct {
	ActionKind kind;
	Color color;
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
	{ ST_RED,    EV_TICK, ST_GREEN,  1, { ACT_SET_LAMP } },
	{ ST_GREEN,  EV_TICK, ST_YELLOW, 1, { ACT_SET_LAMP } },
	{ ST_YELLOW, EV_TICK, ST_RED,    1, { ACT_SET_LAMP } },
};

static const char *state_names[] = { "RED", "GREEN", "YELLOW" };
static const char *event_names[] = { "TICK" };
static const char *color_names[] = { "RED", "GREEN", "YELLOW" };

/*
 * map next state to the lamp color the decision chose
 */
static Color
color_of(State st)
{
	switch (st) {
	case ST_GREEN:
		return COLOR_GREEN;
	case ST_YELLOW:
		return COLOR_YELLOW;
	case ST_RED:
	default:
		return COLOR_RED;
	}
}

/* ---------------------------------------------------------------------------
 * 3. Engine  -- table lookup + bind args; zero side effects
 * --------------------------------------------------------------------------- */

/*
 * fsm decide: lookup by kind, then fill lamp color from next state
 */
static Decision
fsm_decide(const Ctx *ctx, const Event *ev)
{
	Decision d;
	unsigned i;

	memset(&d, 0, sizeof(d));
	d.accepted = 0;
	d.next_state = ctx->state;

	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (table[i].from == ctx->state && table[i].event == ev->kind) {
			d.accepted = 1;
			d.next_state = table[i].to;
			d.n_actions = table[i].n_actions;
			d.actions[0].kind = ACT_SET_LAMP;
			d.actions[0].color = color_of(d.next_state);
			return d;
		}
	}
	return d;
}

/* ---------------------------------------------------------------------------
 * 4. Workers  -- parameterized functions; no FSM state
 * --------------------------------------------------------------------------- */

/*
 * lamp_set: drive the physical lamp to a color
 */
static void
lamp_set(Color color)
{
	printf("[EXEC] lamp_set(%s)\n", color_names[color]);
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * unpack SET_LAMP and call lamp_set(color)
 */
static void
dispatch(const Decision *d)
{
	int i;
	const Action *a;

	for (i = 0; i < d->n_actions; i++) {
		a = &d->actions[i];
		printf("[DISPATCH] SET_LAMP(color=%s)\n", color_names[a->color]);
		lamp_set(a->color);
	}
}

/* ---------------------------------------------------------------------------
 * 6. Apply  -- commit ctx, then dispatch
 * --------------------------------------------------------------------------- */

/*
 * format TICK payload
 */
static void
format_event(const Event *ev, char *buf, int buflen)
{
	snprintf(buf, (size_t)buflen, "TICK(now_ms=%u)", ev->now_ms);
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
		{ EV_TICK, 1000 },
		{ EV_TICK, 2000 },
		{ EV_TICK, 3000 },
		{ EV_TICK, 4000 }
	};
	unsigned i;

	memset(&ctx, 0, sizeof(ctx));
	ctx.state = ST_RED;
	printf("[INIT] state=%s\n\n", state_names[ctx.state]);
	for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
		apply(&ctx, &seq[i]);
		printf("\n");
	}
	return 0;
}
