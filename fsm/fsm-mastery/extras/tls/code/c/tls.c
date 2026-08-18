#include <stdio.h>

typedef enum {
	STATE_START = 0,
	STATE_WAIT_SERVER_HELLO,
	STATE_WAIT_FINISHED,
	STATE_ESTABLISHED,
	STATE_CLOSED,
	STATE_COUNT
} State;

typedef enum {
	EVENT_CONNECT = 0,
	EVENT_SERVER_HELLO,
	EVENT_CERT_OK,
	EVENT_FINISHED,
	EVENT_ALERT,
	EVENT_TIMEOUT,
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
 * return a printable name for a TLS teaching state
 */
static const char *
state_name(State state)
{
	if (state == STATE_WAIT_SERVER_HELLO)
		return "WAIT_SERVER_HELLO";
	if (state == STATE_WAIT_FINISHED)
		return "WAIT_FINISHED";
	if (state == STATE_ESTABLISHED)
		return "ESTABLISHED";
	if (state == STATE_CLOSED)
		return "CLOSED";
	return "START";
}

/*
 * return a printable name for a TLS teaching event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_SERVER_HELLO)
		return "SERVER_HELLO";
	if (event == EVENT_CERT_OK)
		return "CERT_OK";
	if (event == EVENT_FINISHED)
		return "FINISHED";
	if (event == EVENT_ALERT)
		return "ALERT";
	if (event == EVENT_TIMEOUT)
		return "TIMEOUT";
	return "CONNECT";
}

/*
 * start handshake: send ClientHello
 */
static void
action_send_client_hello(Fsm *fsm)
{
	(void)fsm;
	printf("action: send ClientHello\n");
}

/*
 * ServerHello accepted; still waiting for cert and Finished
 */
static void
action_got_server_hello(Fsm *fsm)
{
	(void)fsm;
	printf("action: got ServerHello\n");
}

/*
 * certificate chain verified (crypto is outside this FSM)
 */
static void
action_cert_ok(Fsm *fsm)
{
	(void)fsm;
	printf("action: cert ok\n");
}

/*
 * peer Finished accepted; handshake is done
 */
static void
action_handshake_done(Fsm *fsm)
{
	(void)fsm;
	printf("action: handshake done\n");
}

/*
 * fatal alert; close the teaching session
 */
static void
action_alert(Fsm *fsm)
{
	(void)fsm;
	printf("action: alert, close\n");
}

/*
 * handshake wait timed out
 */
static void
action_timeout(Fsm *fsm)
{
	(void)fsm;
	printf("action: handshake timeout\n");
}

static const Cell table[STATE_COUNT][EVENT_COUNT] = {
	[STATE_START][EVENT_CONNECT] = {
		action_send_client_hello, STATE_WAIT_SERVER_HELLO, 1
	},
	[STATE_WAIT_SERVER_HELLO][EVENT_SERVER_HELLO] = {
		action_got_server_hello, STATE_WAIT_FINISHED, 1
	},
	[STATE_WAIT_SERVER_HELLO][EVENT_ALERT] = {
		action_alert, STATE_CLOSED, 1
	},
	[STATE_WAIT_SERVER_HELLO][EVENT_TIMEOUT] = {
		action_timeout, STATE_CLOSED, 1
	},
	[STATE_WAIT_FINISHED][EVENT_CERT_OK] = {
		action_cert_ok, STATE_WAIT_FINISHED, 1
	},
	[STATE_WAIT_FINISHED][EVENT_FINISHED] = {
		action_handshake_done, STATE_ESTABLISHED, 1
	},
	[STATE_WAIT_FINISHED][EVENT_ALERT] = {
		action_alert, STATE_CLOSED, 1
	},
	[STATE_WAIT_FINISHED][EVENT_TIMEOUT] = {
		action_timeout, STATE_CLOSED, 1
	},
	[STATE_ESTABLISHED][EVENT_ALERT] = {
		action_alert, STATE_CLOSED, 1
	},
};

/*
 * put the teaching TLS FSM into START
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_START;
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
	Fsm tls;

	printf("=== tls teaching FSM ===\n");

	printf("-- handshake --\n");
	fsm_init(&tls);
	printf("init: %s\n", state_name(tls.state));
	fsm_dispatch(&tls, EVENT_CONNECT);
	fsm_dispatch(&tls, EVENT_SERVER_HELLO);
	fsm_dispatch(&tls, EVENT_CERT_OK);
	fsm_dispatch(&tls, EVENT_FINISHED);

	printf("-- alert after established --\n");
	fsm_dispatch(&tls, EVENT_ALERT);

	printf("-- timeout waiting for ServerHello --\n");
	fsm_init(&tls);
	fsm_dispatch(&tls, EVENT_CONNECT);
	fsm_dispatch(&tls, EVENT_TIMEOUT);

	printf("-- alert while waiting for Finished --\n");
	fsm_init(&tls);
	fsm_dispatch(&tls, EVENT_CONNECT);
	fsm_dispatch(&tls, EVENT_SERVER_HELLO);
	fsm_dispatch(&tls, EVENT_ALERT);

	printf("-- illegal input is a missing table cell --\n");
	fsm_init(&tls);
	fsm_dispatch(&tls, EVENT_FINISHED);

	return 0;
}
