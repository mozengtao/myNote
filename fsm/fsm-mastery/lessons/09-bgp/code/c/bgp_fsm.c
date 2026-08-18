#include <stdio.h>

#define QUEUE_CAP 16
#define HOLD_BUDGET 5
#define KEEPALIVE_BUDGET 2
#define CONNECTRETRY_BUDGET 3

typedef enum {
	STATE_IDLE = 0,
	STATE_CONNECT,
	STATE_OPENSENT,
	STATE_OPENCONFIRM,
	STATE_ESTABLISHED,
	STATE_COUNT
} State;

typedef enum {
	EVENT_START = 0,
	EVENT_STOP,
	EVENT_TCP_OPEN,
	EVENT_TCP_FAIL,
	EVENT_OPEN_MSG,
	EVENT_KEEPALIVE_MSG,
	EVENT_HOLD_EXPIRED,
	EVENT_KEEPALIVE_EXPIRED,
	EVENT_CONNECTRETRY_EXPIRED,
	EVENT_COUNT
} Event;

typedef struct {
	Event items[QUEUE_CAP];
	int head;
	int count;
} EventQueue;

typedef struct Fsm Fsm;

struct Fsm {
	State state;
	EventQueue queue;
	int hold_ticks;
	int keepalive_ticks;
	int connectretry_ticks;
};

typedef void (*ActionFn)(Fsm *fsm);

typedef struct {
	ActionFn action;
	State next;
	int defined;
} Cell;

/*
 * return a printable name for a BGP teaching state
 */
static const char *
state_name(State state)
{
	if (state == STATE_CONNECT)
		return "CONNECT";
	if (state == STATE_OPENSENT)
		return "OPENSENT";
	if (state == STATE_OPENCONFIRM)
		return "OPENCONFIRM";
	if (state == STATE_ESTABLISHED)
		return "ESTABLISHED";
	return "IDLE";
}

/*
 * return a printable name for a BGP teaching event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_STOP)
		return "STOP";
	if (event == EVENT_TCP_OPEN)
		return "TCP_OPEN";
	if (event == EVENT_TCP_FAIL)
		return "TCP_FAIL";
	if (event == EVENT_OPEN_MSG)
		return "OPEN_MSG";
	if (event == EVENT_KEEPALIVE_MSG)
		return "KEEPALIVE_MSG";
	if (event == EVENT_HOLD_EXPIRED)
		return "HOLD_EXPIRED";
	if (event == EVENT_KEEPALIVE_EXPIRED)
		return "KEEPALIVE_EXPIRED";
	if (event == EVENT_CONNECTRETRY_EXPIRED)
		return "CONNECTRETRY_EXPIRED";
	return "START";
}

/*
 * enqueue one event; drop if the teaching queue is full
 */
static void
enqueue(EventQueue *q, Event event)
{
	int tail;

	if (q->count >= QUEUE_CAP) {
		printf("queue full, drop %s\n", event_name(event));
		return;
	}
	tail = (q->head + q->count) % QUEUE_CAP;
	q->items[tail] = event;
	q->count++;
}

/*
 * dequeue one event; return 1 if an event was written
 */
static int
dequeue(EventQueue *q, Event *event)
{
	if (q->count <= 0)
		return 0;
	*event = q->items[q->head];
	q->head = (q->head + 1) % QUEUE_CAP;
	q->count--;
	return 1;
}

/*
 * stop all three teaching timers
 */
static void
clear_timers(Fsm *fsm)
{
	fsm->hold_ticks = 0;
	fsm->keepalive_ticks = 0;
	fsm->connectretry_ticks = 0;
}

/*
 * arm ConnectRetry after entering CONNECT
 */
static void
arm_connectretry(Fsm *fsm)
{
	fsm->connectretry_ticks = CONNECTRETRY_BUDGET;
	printf("timer: arm ConnectRetry=%d\n", fsm->connectretry_ticks);
}

/*
 * arm Hold and Keepalive after the OPEN handshake is in progress
 */
static void
arm_session_timers(Fsm *fsm)
{
	fsm->hold_ticks = HOLD_BUDGET;
	fsm->keepalive_ticks = KEEPALIVE_BUDGET;
	fsm->connectretry_ticks = 0;
	printf("timer: arm Hold=%d Keepalive=%d\n", fsm->hold_ticks,
	    fsm->keepalive_ticks);
}

/*
 * application asks the session to start
 */
static void
action_start_connect(Fsm *fsm)
{
	printf("action: open TCP\n");
	arm_connectretry(fsm);
}

/*
 * TCP is up; send OPEN
 */
static void
action_send_open(Fsm *fsm)
{
	printf("action: send OPEN\n");
	fsm->connectretry_ticks = 0;
	fsm->hold_ticks = HOLD_BUDGET;
	printf("timer: arm Hold=%d\n", fsm->hold_ticks);
}

/*
 * peer OPEN accepted; send KEEPALIVE and wait for theirs
 */
static void
action_send_keepalive_openconfirm(Fsm *fsm)
{
	printf("action: send KEEPALIVE\n");
	arm_session_timers(fsm);
}

/*
 * session is established
 */
static void
action_established(Fsm *fsm)
{
	printf("action: session ESTABLISHED\n");
	arm_session_timers(fsm);
}

/*
 * peer keepalive resets Hold
 */
static void
action_reset_hold(Fsm *fsm)
{
	fsm->hold_ticks = HOLD_BUDGET;
	printf("action: recv KEEPALIVE, reset Hold=%d\n", fsm->hold_ticks);
}

/*
 * local Keepalive timer expired; send and re-arm
 */
static void
action_send_keepalive(Fsm *fsm)
{
	printf("action: send KEEPALIVE\n");
	fsm->keepalive_ticks = KEEPALIVE_BUDGET;
	printf("timer: re-arm Keepalive=%d\n", fsm->keepalive_ticks);
}

/*
 * drop the session and return to IDLE
 */
static void
action_drop(Fsm *fsm)
{
	printf("action: drop session\n");
	clear_timers(fsm);
}

/*
 * ConnectRetry expired; try TCP again
 */
static void
action_retry_connect(Fsm *fsm)
{
	printf("action: retry open TCP\n");
	arm_connectretry(fsm);
}

static const Cell table[STATE_COUNT][EVENT_COUNT] = {
	[STATE_IDLE][EVENT_START] = {
		action_start_connect, STATE_CONNECT, 1
	},
	[STATE_CONNECT][EVENT_TCP_OPEN] = {
		action_send_open, STATE_OPENSENT, 1
	},
	[STATE_CONNECT][EVENT_TCP_FAIL] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_CONNECT][EVENT_STOP] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_CONNECT][EVENT_CONNECTRETRY_EXPIRED] = {
		action_retry_connect, STATE_CONNECT, 1
	},
	[STATE_OPENSENT][EVENT_OPEN_MSG] = {
		action_send_keepalive_openconfirm, STATE_OPENCONFIRM, 1
	},
	[STATE_OPENSENT][EVENT_TCP_FAIL] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_OPENSENT][EVENT_STOP] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_OPENSENT][EVENT_HOLD_EXPIRED] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_OPENCONFIRM][EVENT_KEEPALIVE_MSG] = {
		action_established, STATE_ESTABLISHED, 1
	},
	[STATE_OPENCONFIRM][EVENT_HOLD_EXPIRED] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_OPENCONFIRM][EVENT_STOP] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_OPENCONFIRM][EVENT_TCP_FAIL] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_ESTABLISHED][EVENT_KEEPALIVE_MSG] = {
		action_reset_hold, STATE_ESTABLISHED, 1
	},
	[STATE_ESTABLISHED][EVENT_KEEPALIVE_EXPIRED] = {
		action_send_keepalive, STATE_ESTABLISHED, 1
	},
	[STATE_ESTABLISHED][EVENT_HOLD_EXPIRED] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_ESTABLISHED][EVENT_STOP] = {
		action_drop, STATE_IDLE, 1
	},
	[STATE_ESTABLISHED][EVENT_TCP_FAIL] = {
		action_drop, STATE_IDLE, 1
	},
};

/*
 * put the teaching BGP FSM into IDLE with an empty queue
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_IDLE;
	fsm->queue.head = 0;
	fsm->queue.count = 0;
	clear_timers(fsm);
}

/*
 * dispatch one event by table lookup
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	const Cell *cell;
	State prev;

	prev = fsm->state;
	cell = &table[fsm->state][event];
	if (!cell->defined) {
		printf("ignore: %s + %s\n", state_name(prev),
		    event_name(event));
		return;
	}
	if (cell->action)
		cell->action(fsm);
	fsm->state = cell->next;
	printf("%s --%s--> %s\n", state_name(prev), event_name(event),
	    state_name(fsm->state));
}

/*
 * post an event onto the queue; it is not dispatched yet
 */
void
fsm_post(Fsm *fsm, Event event)
{
	printf("post: %s\n", event_name(event));
	enqueue(&fsm->queue, event);
}

/*
 * drain the queue: while (dequeue) dispatch
 */
void
fsm_run(Fsm *fsm)
{
	Event event;

	while (dequeue(&fsm->queue, &event))
		fsm_dispatch(fsm, event);
}

/*
 * decrement armed timers; expiry becomes a queued event
 */
void
fsm_tick(Fsm *fsm)
{
	printf("tick: hold=%d keepalive=%d connectretry=%d\n",
	    fsm->hold_ticks, fsm->keepalive_ticks,
	    fsm->connectretry_ticks);

	if (fsm->connectretry_ticks > 0) {
		fsm->connectretry_ticks--;
		if (fsm->connectretry_ticks == 0) {
			printf("timer: ConnectRetry expired\n");
			enqueue(&fsm->queue, EVENT_CONNECTRETRY_EXPIRED);
		}
	}
	if (fsm->hold_ticks > 0) {
		fsm->hold_ticks--;
		if (fsm->hold_ticks == 0) {
			printf("timer: Hold expired\n");
			enqueue(&fsm->queue, EVENT_HOLD_EXPIRED);
		}
	}
	if (fsm->keepalive_ticks > 0) {
		fsm->keepalive_ticks--;
		if (fsm->keepalive_ticks == 0) {
			printf("timer: Keepalive expired\n");
			enqueue(&fsm->queue, EVENT_KEEPALIVE_EXPIRED);
		}
	}
}

int
main(void)
{
	Fsm bgp;
	int i;

	printf("=== bgp teaching FSM ===\n");

	printf("-- session up, keepalive fire, then hold expire --\n");
	fsm_init(&bgp);
	printf("init: %s\n", state_name(bgp.state));
	fsm_post(&bgp, EVENT_START);
	fsm_run(&bgp);
	fsm_post(&bgp, EVENT_TCP_OPEN);
	fsm_run(&bgp);
	fsm_post(&bgp, EVENT_OPEN_MSG);
	fsm_run(&bgp);
	fsm_post(&bgp, EVENT_KEEPALIVE_MSG);
	fsm_run(&bgp);
	for (i = 0; i < HOLD_BUDGET; i++) {
		fsm_tick(&bgp);
		fsm_run(&bgp);
	}

	printf("-- connectretry when TCP never comes up --\n");
	fsm_init(&bgp);
	printf("init: %s\n", state_name(bgp.state));
	fsm_post(&bgp, EVENT_START);
	fsm_run(&bgp);
	for (i = 0; i < CONNECTRETRY_BUDGET; i++) {
		fsm_tick(&bgp);
		fsm_run(&bgp);
	}

	return 0;
}
