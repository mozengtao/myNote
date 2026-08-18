#include <stdio.h>

typedef enum {
	STATE_SEND_INITIAL = 0,
	STATE_HANDSHAKE,
	STATE_CONNECTED,
	STATE_DRAINING,
	STATE_CLOSED,
	STATE_COUNT
} State;

typedef enum {
	EVENT_INITIAL_ACK = 0,
	EVENT_HANDSHAKE_DONE,
	EVENT_APP_CLOSE,
	EVENT_IDLE_TIMEOUT,
	EVENT_PATH_FAIL,
	EVENT_COUNT
} Event;

typedef struct {
	State state;
} Fsm;

typedef void (*ActionFn)(Fsm *fsm);

typedef struct {
	ActionFn action;
	State next;
	int defined;
} Cell;

/*
 * return a printable name for a QUIC teaching state
 */
static const char *
state_name(State state)
{
	if (state == STATE_HANDSHAKE)
		return "HANDSHAKE";
	if (state == STATE_CONNECTED)
		return "CONNECTED";
	if (state == STATE_DRAINING)
		return "DRAINING";
	if (state == STATE_CLOSED)
		return "CLOSED";
	return "SEND_INITIAL";
}

/*
 * return a printable name for a QUIC teaching event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_HANDSHAKE_DONE)
		return "HANDSHAKE_DONE";
	if (event == EVENT_APP_CLOSE)
		return "APP_CLOSE";
	if (event == EVENT_IDLE_TIMEOUT)
		return "IDLE_TIMEOUT";
	if (event == EVENT_PATH_FAIL)
		return "PATH_FAIL";
	return "INITIAL_ACK";
}

/*
 * Initial was acknowledged; send Handshake packets
 */
static void
action_send_handshake(Fsm *fsm)
{
	(void)fsm;
	printf("action: send Handshake\n");
}

/*
 * handshake confirmed; streams may be created (not implemented)
 */
static void
action_connected(Fsm *fsm)
{
	(void)fsm;
	printf("action: connected\n");
}

/*
 * application closes; send CONNECTION_CLOSE and drain
 */
static void
action_connection_close(Fsm *fsm)
{
	(void)fsm;
	printf("action: send CONNECTION_CLOSE\n");
}

/*
 * abandon before the session is up
 */
static void
action_abandon(Fsm *fsm)
{
	(void)fsm;
	printf("action: abandon handshake\n");
}

/*
 * idle timeout tore the connection down
 */
static void
action_idle_close(Fsm *fsm)
{
	(void)fsm;
	printf("action: idle timeout\n");
}

/*
 * path failed during handshake; no session to drain
 */
static void
action_path_fail_hard(Fsm *fsm)
{
	(void)fsm;
	printf("action: path fail, close\n");
}

/*
 * path failed after connect; drain leftover packets
 */
static void
action_path_fail_drain(Fsm *fsm)
{
	(void)fsm;
	printf("action: path fail, drain\n");
}

/*
 * draining period ended
 */
static void
action_drained(Fsm *fsm)
{
	(void)fsm;
	printf("action: drained, close\n");
}

static const Cell table[STATE_COUNT][EVENT_COUNT] = {
	[STATE_SEND_INITIAL][EVENT_INITIAL_ACK] = {
		action_send_handshake, STATE_HANDSHAKE, 1
	},
	[STATE_SEND_INITIAL][EVENT_APP_CLOSE] = {
		action_abandon, STATE_CLOSED, 1
	},
	[STATE_SEND_INITIAL][EVENT_IDLE_TIMEOUT] = {
		action_idle_close, STATE_CLOSED, 1
	},
	[STATE_SEND_INITIAL][EVENT_PATH_FAIL] = {
		action_path_fail_hard, STATE_CLOSED, 1
	},
	[STATE_HANDSHAKE][EVENT_HANDSHAKE_DONE] = {
		action_connected, STATE_CONNECTED, 1
	},
	[STATE_HANDSHAKE][EVENT_APP_CLOSE] = {
		action_connection_close, STATE_DRAINING, 1
	},
	[STATE_HANDSHAKE][EVENT_IDLE_TIMEOUT] = {
		action_idle_close, STATE_CLOSED, 1
	},
	[STATE_HANDSHAKE][EVENT_PATH_FAIL] = {
		action_path_fail_hard, STATE_CLOSED, 1
	},
	[STATE_CONNECTED][EVENT_APP_CLOSE] = {
		action_connection_close, STATE_DRAINING, 1
	},
	[STATE_CONNECTED][EVENT_IDLE_TIMEOUT] = {
		action_idle_close, STATE_CLOSED, 1
	},
	[STATE_CONNECTED][EVENT_PATH_FAIL] = {
		action_path_fail_drain, STATE_DRAINING, 1
	},
	[STATE_DRAINING][EVENT_IDLE_TIMEOUT] = {
		action_drained, STATE_CLOSED, 1
	},
	[STATE_DRAINING][EVENT_PATH_FAIL] = {
		action_drained, STATE_CLOSED, 1
	},
};

/*
 * put the teaching QUIC FSM into SEND_INITIAL
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_SEND_INITIAL;
}

/*
 * dispatch one event by table lookup: table[state][event]
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

int
main(void)
{
	Fsm quic;

	printf("=== quic teaching FSM ===\n");

	printf("-- handshake --\n");
	fsm_init(&quic);
	printf("init: %s\n", state_name(quic.state));
	fsm_dispatch(&quic, EVENT_INITIAL_ACK);
	fsm_dispatch(&quic, EVENT_HANDSHAKE_DONE);

	printf("-- app close then drain --\n");
	fsm_dispatch(&quic, EVENT_APP_CLOSE);
	fsm_dispatch(&quic, EVENT_IDLE_TIMEOUT);

	printf("-- idle timeout from CONNECTED --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, EVENT_INITIAL_ACK);
	fsm_dispatch(&quic, EVENT_HANDSHAKE_DONE);
	fsm_dispatch(&quic, EVENT_IDLE_TIMEOUT);

	printf("-- path fail after connect drains --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, EVENT_INITIAL_ACK);
	fsm_dispatch(&quic, EVENT_HANDSHAKE_DONE);
	fsm_dispatch(&quic, EVENT_PATH_FAIL);
	fsm_dispatch(&quic, EVENT_IDLE_TIMEOUT);

	printf("-- path fail during handshake --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, EVENT_INITIAL_ACK);
	fsm_dispatch(&quic, EVENT_PATH_FAIL);

	printf("-- illegal input is a missing table cell --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, EVENT_HANDSHAKE_DONE);

	return 0;
}
