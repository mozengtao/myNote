#include <stdio.h>

#define RETRY_BUDGET 3
#define STATE_COUNT 6
#define EVENT_COUNT 10

typedef enum {
	STATE_IDLE = 0,
	STATE_SCANNING,
	STATE_AUTHENTICATING,
	STATE_ASSOCIATING,
	STATE_CONNECTED,
	STATE_DISCONNECTED
} State;

typedef enum {
	EVENT_START = 0,
	EVENT_SCAN_DONE,
	EVENT_AUTH_OK,
	EVENT_AUTH_FAIL,
	EVENT_ASSOC_OK,
	EVENT_ASSOC_FAIL,
	EVENT_DATA_OK,
	EVENT_LOST,
	EVENT_TIMEOUT,
	EVENT_RETRY
} Event;

typedef struct {
	int retries_left;
	int timer_armed;
} Context;

typedef struct {
	State state;
	Context ctx;
} Fsm;

typedef void (*ActionFn)(Fsm *fsm);

typedef struct {
	int used;
	ActionFn action;
	State next;
} Cell;

static Cell table[STATE_COUNT][EVENT_COUNT];

/*
 * return a printable name for a wifi session state
 */
static const char *
state_name(State state)
{
	if (state == STATE_SCANNING)
		return "SCANNING";
	if (state == STATE_AUTHENTICATING)
		return "AUTHENTICATING";
	if (state == STATE_ASSOCIATING)
		return "ASSOCIATING";
	if (state == STATE_CONNECTED)
		return "CONNECTED";
	if (state == STATE_DISCONNECTED)
		return "DISCONNECTED";
	return "IDLE";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_SCAN_DONE)
		return "SCAN_DONE";
	if (event == EVENT_AUTH_OK)
		return "AUTH_OK";
	if (event == EVENT_AUTH_FAIL)
		return "AUTH_FAIL";
	if (event == EVENT_ASSOC_OK)
		return "ASSOC_OK";
	if (event == EVENT_ASSOC_FAIL)
		return "ASSOC_FAIL";
	if (event == EVENT_DATA_OK)
		return "DATA_OK";
	if (event == EVENT_LOST)
		return "LOST";
	if (event == EVENT_TIMEOUT)
		return "TIMEOUT";
	if (event == EVENT_RETRY)
		return "RETRY";
	return "START";
}

/*
 * print one transition and the retry budget
 */
static void
print_transition(State prev, Event event, const Fsm *fsm)
{
	printf("%s --%s--> %s (retries left=%d)\n",
	    state_name(prev), event_name(event), state_name(fsm->state),
	    fsm->ctx.retries_left);
}

/*
 * begin a session: arm retry budget and start scan
 */
static void
action_start_scan(Fsm *fsm)
{
	fsm->ctx.retries_left = RETRY_BUDGET;
	fsm->ctx.timer_armed = 1;
	printf("start scan (retries left=%d)\n", fsm->ctx.retries_left);
}

/*
 * scan found a candidate BSS; begin 802.11 authentication
 */
static void
action_begin_auth(Fsm *fsm)
{
	fsm->ctx.timer_armed = 1;
	printf("scan done, begin auth\n");
}

/*
 * authentication succeeded; begin association
 */
static void
action_begin_assoc(Fsm *fsm)
{
	fsm->ctx.timer_armed = 1;
	printf("auth ok, begin assoc\n");
}

/*
 * association succeeded; session is up (4-way handshake omitted)
 */
static void
action_connected(Fsm *fsm)
{
	fsm->ctx.retries_left = RETRY_BUDGET;
	fsm->ctx.timer_armed = 0;
	printf("assoc ok, connected\n");
}

/*
 * data path is alive
 */
static void
action_data_ok(Fsm *fsm)
{
	(void)fsm;
	printf("data ok\n");
}

/*
 * auth, assoc, scan wait, or link loss: go to the recovery state
 */
static void
action_session_fail(Fsm *fsm)
{
	if (fsm->ctx.retries_left > 0)
		fsm->ctx.retries_left--;
	fsm->ctx.timer_armed = 1;
	printf("session fail, retries left=%d\n", fsm->ctx.retries_left);
}

/*
 * still have budget: return to SCANNING
 */
static void
action_retry_scan(Fsm *fsm)
{
	fsm->ctx.timer_armed = 1;
	printf("retry scan, retries left=%d\n", fsm->ctx.retries_left);
}

/*
 * budget exhausted: stop recovering
 */
static void
action_give_up(Fsm *fsm)
{
	fsm->ctx.timer_armed = 0;
	printf("give up, back to idle\n");
}

/*
 * fill table[state][event] once
 */
static void
table_set(State state, Event event, ActionFn action, State next)
{
	table[state][event].used = 1;
	table[state][event].action = action;
	table[state][event].next = next;
}

/*
 * install the session table (recovery from DISCONNECTED is guarded)
 */
static void
table_init(void)
{
	static int ready;

	if (ready)
		return;
	ready = 1;

	table_set(STATE_IDLE, EVENT_START, action_start_scan, STATE_SCANNING);

	table_set(STATE_SCANNING, EVENT_SCAN_DONE, action_begin_auth,
	    STATE_AUTHENTICATING);
	table_set(STATE_SCANNING, EVENT_TIMEOUT, action_session_fail,
	    STATE_DISCONNECTED);

	table_set(STATE_AUTHENTICATING, EVENT_AUTH_OK, action_begin_assoc,
	    STATE_ASSOCIATING);
	table_set(STATE_AUTHENTICATING, EVENT_AUTH_FAIL, action_session_fail,
	    STATE_DISCONNECTED);
	table_set(STATE_AUTHENTICATING, EVENT_TIMEOUT, action_session_fail,
	    STATE_DISCONNECTED);

	table_set(STATE_ASSOCIATING, EVENT_ASSOC_OK, action_connected,
	    STATE_CONNECTED);
	table_set(STATE_ASSOCIATING, EVENT_ASSOC_FAIL, action_session_fail,
	    STATE_DISCONNECTED);
	table_set(STATE_ASSOCIATING, EVENT_TIMEOUT, action_session_fail,
	    STATE_DISCONNECTED);

	table_set(STATE_CONNECTED, EVENT_DATA_OK, action_data_ok,
	    STATE_CONNECTED);
	table_set(STATE_CONNECTED, EVENT_LOST, action_session_fail,
	    STATE_DISCONNECTED);
}

/*
 * put the wifi FSM into IDLE with a clean context
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_IDLE;
	fsm->ctx.retries_left = 0;
	fsm->ctx.timer_armed = 0;
	table_init();
}

/*
 * dispatch one event into the wifi session FSM
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;
	Cell *cell;

	prev = fsm->state;

	if (fsm->state == STATE_DISCONNECTED &&
	    (event == EVENT_RETRY || event == EVENT_TIMEOUT)) {
		if (fsm->ctx.retries_left > 0) {
			action_retry_scan(fsm);
			fsm->state = STATE_SCANNING;
		} else {
			action_give_up(fsm);
			fsm->state = STATE_IDLE;
		}
		print_transition(prev, event, fsm);
		return;
	}

	cell = &table[fsm->state][event];
	if (!cell->used)
		return;

	if (cell->action)
		cell->action(fsm);
	fsm->state = cell->next;
	print_transition(prev, event, fsm);
}

int
main(void)
{
	Fsm wifi;

	printf("=== wifi session demo ===\n");

	printf("-- 1. happy path association --\n");
	fsm_init(&wifi);
	printf("init: %s\n", state_name(wifi.state));
	fsm_dispatch(&wifi, EVENT_START);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_OK);
	fsm_dispatch(&wifi, EVENT_ASSOC_OK);
	fsm_dispatch(&wifi, EVENT_DATA_OK);

	printf("-- 2. auth fail, then retry, then success --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, EVENT_START);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_FAIL);
	fsm_dispatch(&wifi, EVENT_RETRY);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_OK);
	fsm_dispatch(&wifi, EVENT_ASSOC_OK);

	printf("-- 3. assoc fail until budget exhausted --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, EVENT_START);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_OK);
	fsm_dispatch(&wifi, EVENT_ASSOC_FAIL);
	fsm_dispatch(&wifi, EVENT_RETRY);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_OK);
	fsm_dispatch(&wifi, EVENT_ASSOC_FAIL);
	fsm_dispatch(&wifi, EVENT_RETRY);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_OK);
	fsm_dispatch(&wifi, EVENT_ASSOC_FAIL);
	fsm_dispatch(&wifi, EVENT_RETRY);

	printf("-- 4. connected then lost, timeout recovers to scan --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, EVENT_START);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_OK);
	fsm_dispatch(&wifi, EVENT_ASSOC_OK);
	fsm_dispatch(&wifi, EVENT_LOST);
	fsm_dispatch(&wifi, EVENT_TIMEOUT);
	fsm_dispatch(&wifi, EVENT_SCAN_DONE);
	fsm_dispatch(&wifi, EVENT_AUTH_OK);
	fsm_dispatch(&wifi, EVENT_ASSOC_OK);

	printf("-- 5. scan timeout then give up after budget --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, EVENT_START);
	fsm_dispatch(&wifi, EVENT_TIMEOUT);
	fsm_dispatch(&wifi, EVENT_RETRY);
	fsm_dispatch(&wifi, EVENT_TIMEOUT);
	fsm_dispatch(&wifi, EVENT_RETRY);
	fsm_dispatch(&wifi, EVENT_TIMEOUT);
	fsm_dispatch(&wifi, EVENT_RETRY);

	return 0;
}
