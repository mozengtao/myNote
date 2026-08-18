#include <stdio.h>

typedef enum {
	STATE_CLOSED = 0,
	STATE_LISTEN,
	STATE_SYN_SENT,
	STATE_ESTABLISHED,
	STATE_TIME_WAIT,
	STATE_COUNT
} State;

typedef enum {
	EVENT_APP_LISTEN = 0,
	EVENT_APP_CONNECT,
	EVENT_RCV_SYN,
	EVENT_RCV_SYNACK,
	EVENT_RCV_ACK,
	EVENT_APP_CLOSE,
	EVENT_RCV_FIN,
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
 * return a printable name for a TCP teaching state
 */
static const char *
state_name(State state)
{
	if (state == STATE_LISTEN)
		return "LISTEN";
	if (state == STATE_SYN_SENT)
		return "SYN_SENT";
	if (state == STATE_ESTABLISHED)
		return "ESTABLISHED";
	if (state == STATE_TIME_WAIT)
		return "TIME_WAIT";
	return "CLOSED";
}

/*
 * return a printable name for a TCP teaching event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_APP_CONNECT)
		return "APP_CONNECT";
	if (event == EVENT_RCV_SYN)
		return "RCV_SYN";
	if (event == EVENT_RCV_SYNACK)
		return "RCV_SYNACK";
	if (event == EVENT_RCV_ACK)
		return "RCV_ACK";
	if (event == EVENT_APP_CLOSE)
		return "APP_CLOSE";
	if (event == EVENT_RCV_FIN)
		return "RCV_FIN";
	if (event == EVENT_TIMEOUT)
		return "TIMEOUT";
	return "APP_LISTEN";
}

/*
 * application starts listening
 */
static void
action_listen(Fsm *fsm)
{
	(void)fsm;
	printf("action: listen\n");
}

/*
 * send a SYN (active open)
 */
static void
action_send_syn(Fsm *fsm)
{
	(void)fsm;
	printf("action: send SYN\n");
}

/*
 * send SYN-ACK; teaching path skips SYN_RCVD
 */
static void
action_send_synack(Fsm *fsm)
{
	(void)fsm;
	printf("action: send SYN-ACK (skip SYN_RCVD)\n");
}

/*
 * send an ACK
 */
static void
action_send_ack(Fsm *fsm)
{
	(void)fsm;
	printf("action: send ACK\n");
}

/*
 * send a FIN
 */
static void
action_send_fin(Fsm *fsm)
{
	(void)fsm;
	printf("action: send FIN\n");
}

/*
 * retransmit SYN after RTO (retry review)
 */
static void
action_retransmit_syn(Fsm *fsm)
{
	(void)fsm;
	printf("action: retransmit SYN\n");
}

/*
 * note that this ACK does not change the established session
 */
static void
action_recv_ack(Fsm *fsm)
{
	(void)fsm;
	printf("action: recv ACK (stay ESTABLISHED)\n");
}

static const Cell table[STATE_COUNT][EVENT_COUNT] = {
	[STATE_CLOSED][EVENT_APP_LISTEN] = {
		action_listen, STATE_LISTEN, 1
	},
	[STATE_CLOSED][EVENT_APP_CONNECT] = {
		action_send_syn, STATE_SYN_SENT, 1
	},
	[STATE_LISTEN][EVENT_RCV_SYN] = {
		action_send_synack, STATE_ESTABLISHED, 1
	},
	[STATE_LISTEN][EVENT_APP_CLOSE] = {
		0, STATE_CLOSED, 1
	},
	[STATE_SYN_SENT][EVENT_RCV_SYNACK] = {
		action_send_ack, STATE_ESTABLISHED, 1
	},
	[STATE_SYN_SENT][EVENT_TIMEOUT] = {
		action_retransmit_syn, STATE_SYN_SENT, 1
	},
	[STATE_SYN_SENT][EVENT_APP_CLOSE] = {
		0, STATE_CLOSED, 1
	},
	[STATE_ESTABLISHED][EVENT_APP_CLOSE] = {
		action_send_fin, STATE_TIME_WAIT, 1
	},
	[STATE_ESTABLISHED][EVENT_RCV_FIN] = {
		action_send_ack, STATE_TIME_WAIT, 1
	},
	[STATE_ESTABLISHED][EVENT_RCV_ACK] = {
		action_recv_ack, STATE_ESTABLISHED, 1
	},
	[STATE_TIME_WAIT][EVENT_TIMEOUT] = {
		0, STATE_CLOSED, 1
	},
};

/*
 * put the teaching TCP FSM into CLOSED
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_CLOSED;
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
	Fsm tcp;

	printf("=== tcp teaching FSM ===\n");

	printf("-- passive open then close --\n");
	fsm_init(&tcp);
	printf("init: %s\n", state_name(tcp.state));
	fsm_dispatch(&tcp, EVENT_APP_LISTEN);
	fsm_dispatch(&tcp, EVENT_RCV_SYN);
	fsm_dispatch(&tcp, EVENT_APP_CLOSE);
	fsm_dispatch(&tcp, EVENT_TIMEOUT);

	printf("-- active open, retransmit, then peer close --\n");
	fsm_init(&tcp);
	printf("init: %s\n", state_name(tcp.state));
	fsm_dispatch(&tcp, EVENT_APP_CONNECT);
	fsm_dispatch(&tcp, EVENT_TIMEOUT);
	fsm_dispatch(&tcp, EVENT_RCV_SYNACK);
	fsm_dispatch(&tcp, EVENT_RCV_ACK);
	fsm_dispatch(&tcp, EVENT_RCV_FIN);
	fsm_dispatch(&tcp, EVENT_TIMEOUT);

	printf("-- illegal input is a missing table cell --\n");
	fsm_init(&tcp);
	fsm_dispatch(&tcp, EVENT_RCV_FIN);

	return 0;
}
