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
} EventKind;

typedef enum {
	ACT_DEDUCT_STOCK = 0,
	ACT_NOTIFY_LOGISTICS,
	ACT_ADD_POINTS,
	ACT_SEND_SMS,
	ACT_RELEASE_STOCK
} ActionKind;

typedef struct {
	EventKind kind;
	int order_id;
	const char *sku;
	int qty;
	int amount;
	const char *phone;
} Event;

typedef struct {
	ActionKind kind;
	const char *sku;
	int qty;
	int order_id;
	int amount;
	const char *phone;
	const char *msg;
} Action;

typedef struct {
	State state;
	int order_id;
	const char *sku;
	int qty;
	int amount;
	const char *phone;
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

static const char *sms_msg[] = { "paid", "shipped", "cancelled" };

/* ---------------------------------------------------------------------------
 * 3. Engine  -- table lookup + bind args from Event or Ctx
 * --------------------------------------------------------------------------- */

/*
 * fsm decide: kind selects workers; order fields come from event or ctx
 */
static Decision
fsm_decide(const Ctx *ctx, const Event *ev)
{
	Decision d;
	unsigned i;
	int k;
	int order_id, qty, amount;
	const char *sku, *phone;
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
		if (ev->kind == EV_PAY_SUCCESS) {
			order_id = ev->order_id;
			sku = ev->sku;
			qty = ev->qty;
			amount = ev->amount;
			phone = ev->phone;
		} else {
			order_id = ctx->order_id;
			sku = ctx->sku;
			qty = ctx->qty;
			amount = ctx->amount;
			phone = ctx->phone;
		}
		for (k = 0; k < d.n_actions; k++) {
			a = &d.actions[k];
			a->kind = table[i].action_kinds[k];
			switch (a->kind) {
			case ACT_DEDUCT_STOCK:
			case ACT_RELEASE_STOCK:
				a->sku = sku;
				a->qty = qty;
				break;
			case ACT_NOTIFY_LOGISTICS:
				a->order_id = order_id;
				a->sku = sku;
				break;
			case ACT_ADD_POINTS:
				a->order_id = order_id;
				a->amount = amount;
				break;
			case ACT_SEND_SMS:
				a->phone = phone;
				a->msg = sms_msg[ev->kind];
				break;
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
 * inventory_deduct: take qty units of sku out of stock
 */
static void
inventory_deduct(const char *sku, int qty)
{
	printf("[EXEC] inventory_deduct(\"%s\", %d)\n", sku, qty);
}

/*
 * inventory_release: put qty units of sku back
 */
static void
inventory_release(const char *sku, int qty)
{
	printf("[EXEC] inventory_release(\"%s\", %d)\n", sku, qty);
}

/*
 * logistics_create: open a shipment for this order
 */
static void
logistics_create(int order_id, const char *sku)
{
	printf("[EXEC] logistics_create(%d, \"%s\")\n", order_id, sku);
}

/*
 * points_add: credit loyalty points from the paid amount
 */
static void
points_add(int order_id, int amount)
{
	printf("[EXEC] points_add(%d, %d)\n", order_id, amount);
}

/*
 * sms_send: notify the customer
 */
static void
sms_send(const char *phone, const char *msg)
{
	printf("[EXEC] sms_send(\"%s\", \"%s\")\n", phone, msg);
}

/* ---------------------------------------------------------------------------
 * 5. Dispatch
 * --------------------------------------------------------------------------- */

/*
 * unpack each action and call the matching worker with args
 */
static void
dispatch(const Decision *d)
{
	int i;
	const Action *a;

	for (i = 0; i < d->n_actions; i++) {
		a = &d->actions[i];
		switch (a->kind) {
		case ACT_DEDUCT_STOCK:
			printf("[DISPATCH] DEDUCT_STOCK(sku=%s, qty=%d)\n",
			       a->sku, a->qty);
			inventory_deduct(a->sku, a->qty);
			break;
		case ACT_NOTIFY_LOGISTICS:
			printf("[DISPATCH] NOTIFY_LOGISTICS(order_id=%d, sku=%s)\n",
			       a->order_id, a->sku);
			logistics_create(a->order_id, a->sku);
			break;
		case ACT_ADD_POINTS:
			printf("[DISPATCH] ADD_POINTS(order_id=%d, amount=%d)\n",
			       a->order_id, a->amount);
			points_add(a->order_id, a->amount);
			break;
		case ACT_SEND_SMS:
			printf("[DISPATCH] SEND_SMS(phone=%s, msg=%s)\n",
			       a->phone, a->msg);
			sms_send(a->phone, a->msg);
			break;
		case ACT_RELEASE_STOCK:
			printf("[DISPATCH] RELEASE_STOCK(sku=%s, qty=%d)\n",
			       a->sku, a->qty);
			inventory_release(a->sku, a->qty);
			break;
		}
	}
}

/* ---------------------------------------------------------------------------
 * 6. Apply  -- commit ctx, then dispatch
 * --------------------------------------------------------------------------- */

/*
 * format PAY_SUCCESS payload; SHIP/CANCEL have no fields
 */
static void
format_event(const Event *ev, char *buf, int buflen)
{
	if (ev->kind == EV_PAY_SUCCESS)
		snprintf(buf, (size_t)buflen,
			 "PAY_SUCCESS(order_id=%d, sku=%s, qty=%d, amount=%d, phone=%s)",
			 ev->order_id, ev->sku, ev->qty, ev->amount, ev->phone);
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
	char evbuf[160];

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
	if (ev->kind == EV_PAY_SUCCESS) {
		ctx->order_id = ev->order_id;
		ctx->sku = ev->sku;
		ctx->qty = ev->qty;
		ctx->amount = ev->amount;
		ctx->phone = ev->phone;
	}
	dispatch(&d);
}

/*
 * seed an order in CREATED; later SHIP/CANCEL read these fields from ctx
 */
static void
new_order(Ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->state = ST_CREATED;
	ctx->order_id = 1001;
	ctx->sku = "SKU-7";
	ctx->qty = 2;
	ctx->amount = 99;
	ctx->phone = "13800001111";
}

/*
 * run one scripted order from CREATED
 */
static void
run_scenario(const char *title, const Event *events, unsigned n)
{
	Ctx ctx;
	unsigned i;

	new_order(&ctx);
	printf("=== scenario: %s ===\n", title);
	printf("[INIT] state=%s order_id=%d sku=%s qty=%d amount=%d phone=%s\n\n",
	       state_names[ctx.state], ctx.order_id, ctx.sku, ctx.qty,
	       ctx.amount, ctx.phone);
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
	Event happy[] = {
		{ EV_SHIP, 0, NULL, 0, 0, NULL },
		{ EV_PAY_SUCCESS, 1001, "SKU-7", 2, 99, "13800001111" },
		{ EV_SHIP, 0, NULL, 0, 0, NULL },
		{ EV_CANCEL, 0, NULL, 0, 0, NULL }
	};
	Event cancel_created[] = {
		{ EV_CANCEL, 0, NULL, 0, 0, NULL }
	};
	Event cancel_paid[] = {
		{ EV_PAY_SUCCESS, 1001, "SKU-7", 2, 99, "13800001111" },
		{ EV_CANCEL, 0, NULL, 0, 0, NULL }
	};

	run_scenario("happy-path", happy, sizeof(happy) / sizeof(happy[0]));
	run_scenario("cancel-created", cancel_created,
		     sizeof(cancel_created) / sizeof(cancel_created[0]));
	run_scenario("cancel-paid", cancel_paid,
		     sizeof(cancel_paid) / sizeof(cancel_paid[0]));
	return 0;
}
