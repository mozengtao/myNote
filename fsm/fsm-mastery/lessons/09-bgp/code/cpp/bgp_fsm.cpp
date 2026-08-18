#include <cstdio>

enum {
	QueueCap = 16,
	HoldBudget = 5,
	KeepaliveBudget = 2,
	ConnectRetryBudget = 3
};

enum class State {
	Idle = 0,
	Connect,
	OpenSent,
	OpenConfirm,
	Established
};

enum {
	StateCount = 5
};

enum class Event {
	Start = 0,
	Stop,
	TcpOpen,
	TcpFail,
	OpenMsg,
	KeepaliveMsg,
	HoldExpired,
	KeepaliveExpired,
	ConnectRetryExpired
};

enum {
	EventCount = 9
};

struct EventQueue {
	Event items[QueueCap];
	int head;
	int count;
};

struct Fsm {
	State state;
	EventQueue queue;
	int hold_ticks;
	int keepalive_ticks;
	int connectretry_ticks;
};

using ActionFn = void (*)(Fsm *fsm);

struct Cell {
	ActionFn action;
	State next;
	int defined;
};

static Cell table[StateCount][EventCount];
static int table_ready;

/*
 * return a printable name for a BGP teaching state
 */
static const char *
state_name(State state)
{
	if (state == State::Connect)
		return "CONNECT";
	if (state == State::OpenSent)
		return "OPENSENT";
	if (state == State::OpenConfirm)
		return "OPENCONFIRM";
	if (state == State::Established)
		return "ESTABLISHED";
	return "IDLE";
}

/*
 * return a printable name for a BGP teaching event
 */
static const char *
event_name(Event event)
{
	if (event == Event::Stop)
		return "STOP";
	if (event == Event::TcpOpen)
		return "TCP_OPEN";
	if (event == Event::TcpFail)
		return "TCP_FAIL";
	if (event == Event::OpenMsg)
		return "OPEN_MSG";
	if (event == Event::KeepaliveMsg)
		return "KEEPALIVE_MSG";
	if (event == Event::HoldExpired)
		return "HOLD_EXPIRED";
	if (event == Event::KeepaliveExpired)
		return "KEEPALIVE_EXPIRED";
	if (event == Event::ConnectRetryExpired)
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

	if (q->count >= QueueCap) {
		std::printf("queue full, drop %s\n", event_name(event));
		return;
	}
	tail = (q->head + q->count) % QueueCap;
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
	q->head = (q->head + 1) % QueueCap;
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
	fsm->connectretry_ticks = ConnectRetryBudget;
	std::printf("timer: arm ConnectRetry=%d\n", fsm->connectretry_ticks);
}

/*
 * arm Hold and Keepalive after the OPEN handshake is in progress
 */
static void
arm_session_timers(Fsm *fsm)
{
	fsm->hold_ticks = HoldBudget;
	fsm->keepalive_ticks = KeepaliveBudget;
	fsm->connectretry_ticks = 0;
	std::printf("timer: arm Hold=%d Keepalive=%d\n", fsm->hold_ticks,
	    fsm->keepalive_ticks);
}

/*
 * application asks the session to start
 */
static void
action_start_connect(Fsm *fsm)
{
	std::printf("action: open TCP\n");
	arm_connectretry(fsm);
}

/*
 * TCP is up; send OPEN
 */
static void
action_send_open(Fsm *fsm)
{
	std::printf("action: send OPEN\n");
	fsm->connectretry_ticks = 0;
	fsm->hold_ticks = HoldBudget;
	std::printf("timer: arm Hold=%d\n", fsm->hold_ticks);
}

/*
 * peer OPEN accepted; send KEEPALIVE and wait for theirs
 */
static void
action_send_keepalive_openconfirm(Fsm *fsm)
{
	std::printf("action: send KEEPALIVE\n");
	arm_session_timers(fsm);
}

/*
 * session is established
 */
static void
action_established(Fsm *fsm)
{
	std::printf("action: session ESTABLISHED\n");
	arm_session_timers(fsm);
}

/*
 * peer keepalive resets Hold
 */
static void
action_reset_hold(Fsm *fsm)
{
	fsm->hold_ticks = HoldBudget;
	std::printf("action: recv KEEPALIVE, reset Hold=%d\n",
	    fsm->hold_ticks);
}

/*
 * local Keepalive timer expired; send and re-arm
 */
static void
action_send_keepalive(Fsm *fsm)
{
	std::printf("action: send KEEPALIVE\n");
	fsm->keepalive_ticks = KeepaliveBudget;
	std::printf("timer: re-arm Keepalive=%d\n", fsm->keepalive_ticks);
}

/*
 * drop the session and return to IDLE
 */
static void
action_drop(Fsm *fsm)
{
	std::printf("action: drop session\n");
	clear_timers(fsm);
}

/*
 * ConnectRetry expired; try TCP again
 */
static void
action_retry_connect(Fsm *fsm)
{
	std::printf("action: retry open TCP\n");
	arm_connectretry(fsm);
}

/*
 * install one table cell
 */
static void
table_set(State state, Event event, ActionFn action, State next)
{
	table[static_cast<int>(state)][static_cast<int>(event)] = {
		action, next, 1
	};
}

/*
 * fill the sparse transition table once
 */
static void
ensure_table(void)
{
	if (table_ready)
		return;
	table_set(State::Idle, Event::Start, action_start_connect,
	    State::Connect);
	table_set(State::Connect, Event::TcpOpen, action_send_open,
	    State::OpenSent);
	table_set(State::Connect, Event::TcpFail, action_drop, State::Idle);
	table_set(State::Connect, Event::Stop, action_drop, State::Idle);
	table_set(State::Connect, Event::ConnectRetryExpired,
	    action_retry_connect, State::Connect);
	table_set(State::OpenSent, Event::OpenMsg,
	    action_send_keepalive_openconfirm, State::OpenConfirm);
	table_set(State::OpenSent, Event::TcpFail, action_drop, State::Idle);
	table_set(State::OpenSent, Event::Stop, action_drop, State::Idle);
	table_set(State::OpenSent, Event::HoldExpired, action_drop,
	    State::Idle);
	table_set(State::OpenConfirm, Event::KeepaliveMsg, action_established,
	    State::Established);
	table_set(State::OpenConfirm, Event::HoldExpired, action_drop,
	    State::Idle);
	table_set(State::OpenConfirm, Event::Stop, action_drop, State::Idle);
	table_set(State::OpenConfirm, Event::TcpFail, action_drop,
	    State::Idle);
	table_set(State::Established, Event::KeepaliveMsg, action_reset_hold,
	    State::Established);
	table_set(State::Established, Event::KeepaliveExpired,
	    action_send_keepalive, State::Established);
	table_set(State::Established, Event::HoldExpired, action_drop,
	    State::Idle);
	table_set(State::Established, Event::Stop, action_drop, State::Idle);
	table_set(State::Established, Event::TcpFail, action_drop,
	    State::Idle);
	table_ready = 1;
}

/*
 * put the teaching BGP FSM into IDLE with an empty queue
 */
void
fsm_init(Fsm *fsm)
{
	ensure_table();
	fsm->state = State::Idle;
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
	cell = &table[static_cast<int>(fsm->state)][static_cast<int>(event)];
	if (!cell->defined) {
		std::printf("ignore: %s + %s\n", state_name(prev),
		    event_name(event));
		return;
	}
	if (cell->action)
		cell->action(fsm);
	fsm->state = cell->next;
	std::printf("%s --%s--> %s\n", state_name(prev), event_name(event),
	    state_name(fsm->state));
}

/*
 * post an event onto the queue; it is not dispatched yet
 */
void
fsm_post(Fsm *fsm, Event event)
{
	std::printf("post: %s\n", event_name(event));
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
	std::printf("tick: hold=%d keepalive=%d connectretry=%d\n",
	    fsm->hold_ticks, fsm->keepalive_ticks,
	    fsm->connectretry_ticks);

	if (fsm->connectretry_ticks > 0) {
		fsm->connectretry_ticks--;
		if (fsm->connectretry_ticks == 0) {
			std::printf("timer: ConnectRetry expired\n");
			enqueue(&fsm->queue, Event::ConnectRetryExpired);
		}
	}
	if (fsm->hold_ticks > 0) {
		fsm->hold_ticks--;
		if (fsm->hold_ticks == 0) {
			std::printf("timer: Hold expired\n");
			enqueue(&fsm->queue, Event::HoldExpired);
		}
	}
	if (fsm->keepalive_ticks > 0) {
		fsm->keepalive_ticks--;
		if (fsm->keepalive_ticks == 0) {
			std::printf("timer: Keepalive expired\n");
			enqueue(&fsm->queue, Event::KeepaliveExpired);
		}
	}
}

int
main(void)
{
	Fsm bgp;

	std::printf("=== bgp teaching FSM ===\n");

	std::printf("-- session up, keepalive fire, then hold expire --\n");
	fsm_init(&bgp);
	std::printf("init: %s\n", state_name(bgp.state));
	fsm_post(&bgp, Event::Start);
	fsm_run(&bgp);
	fsm_post(&bgp, Event::TcpOpen);
	fsm_run(&bgp);
	fsm_post(&bgp, Event::OpenMsg);
	fsm_run(&bgp);
	fsm_post(&bgp, Event::KeepaliveMsg);
	fsm_run(&bgp);
	for (int i = 0; i < HoldBudget; i++) {
		fsm_tick(&bgp);
		fsm_run(&bgp);
	}

	std::printf("-- connectretry when TCP never comes up --\n");
	fsm_init(&bgp);
	std::printf("init: %s\n", state_name(bgp.state));
	fsm_post(&bgp, Event::Start);
	fsm_run(&bgp);
	for (int i = 0; i < ConnectRetryBudget; i++) {
		fsm_tick(&bgp);
		fsm_run(&bgp);
	}

	return 0;
}
