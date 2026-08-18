#include <cstdio>

enum class State {
	Closed = 0,
	Listen,
	SynSent,
	Established,
	TimeWait
};

enum {
	StateCount = 5
};

enum class Event {
	AppListen = 0,
	AppConnect,
	RcvSyn,
	RcvSynack,
	RcvAck,
	AppClose,
	RcvFin,
	Timeout
};

enum {
	EventCount = 8
};

struct Fsm {
	State state;
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
 * return a printable name for a TCP teaching state
 */
static const char *
state_name(State state)
{
	if (state == State::Listen)
		return "LISTEN";
	if (state == State::SynSent)
		return "SYN_SENT";
	if (state == State::Established)
		return "ESTABLISHED";
	if (state == State::TimeWait)
		return "TIME_WAIT";
	return "CLOSED";
}

/*
 * return a printable name for a TCP teaching event
 */
static const char *
event_name(Event event)
{
	if (event == Event::AppConnect)
		return "APP_CONNECT";
	if (event == Event::RcvSyn)
		return "RCV_SYN";
	if (event == Event::RcvSynack)
		return "RCV_SYNACK";
	if (event == Event::RcvAck)
		return "RCV_ACK";
	if (event == Event::AppClose)
		return "APP_CLOSE";
	if (event == Event::RcvFin)
		return "RCV_FIN";
	if (event == Event::Timeout)
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
	std::printf("action: listen\n");
}

/*
 * send a SYN (active open)
 */
static void
action_send_syn(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send SYN\n");
}

/*
 * send SYN-ACK; teaching path skips SYN_RCVD
 */
static void
action_send_synack(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send SYN-ACK (skip SYN_RCVD)\n");
}

/*
 * send an ACK
 */
static void
action_send_ack(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send ACK\n");
}

/*
 * send a FIN
 */
static void
action_send_fin(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send FIN\n");
}

/*
 * retransmit SYN after RTO (retry review)
 */
static void
action_retransmit_syn(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: retransmit SYN\n");
}

/*
 * note that this ACK does not change the established session
 */
static void
action_recv_ack(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: recv ACK (stay ESTABLISHED)\n");
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
	table_set(State::Closed, Event::AppListen, action_listen,
	    State::Listen);
	table_set(State::Closed, Event::AppConnect, action_send_syn,
	    State::SynSent);
	table_set(State::Listen, Event::RcvSyn, action_send_synack,
	    State::Established);
	table_set(State::Listen, Event::AppClose, nullptr, State::Closed);
	table_set(State::SynSent, Event::RcvSynack, action_send_ack,
	    State::Established);
	table_set(State::SynSent, Event::Timeout, action_retransmit_syn,
	    State::SynSent);
	table_set(State::SynSent, Event::AppClose, nullptr, State::Closed);
	table_set(State::Established, Event::AppClose, action_send_fin,
	    State::TimeWait);
	table_set(State::Established, Event::RcvFin, action_send_ack,
	    State::TimeWait);
	table_set(State::Established, Event::RcvAck, action_recv_ack,
	    State::Established);
	table_set(State::TimeWait, Event::Timeout, nullptr, State::Closed);
	table_ready = 1;
}

/*
 * put the teaching TCP FSM into CLOSED
 */
void
fsm_init(Fsm *fsm)
{
	ensure_table();
	fsm->state = State::Closed;
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

int
main(void)
{
	Fsm tcp;

	std::printf("=== tcp teaching FSM ===\n");

	std::printf("-- passive open then close --\n");
	fsm_init(&tcp);
	std::printf("init: %s\n", state_name(tcp.state));
	fsm_dispatch(&tcp, Event::AppListen);
	fsm_dispatch(&tcp, Event::RcvSyn);
	fsm_dispatch(&tcp, Event::AppClose);
	fsm_dispatch(&tcp, Event::Timeout);

	std::printf("-- active open, retransmit, then peer close --\n");
	fsm_init(&tcp);
	std::printf("init: %s\n", state_name(tcp.state));
	fsm_dispatch(&tcp, Event::AppConnect);
	fsm_dispatch(&tcp, Event::Timeout);
	fsm_dispatch(&tcp, Event::RcvSynack);
	fsm_dispatch(&tcp, Event::RcvAck);
	fsm_dispatch(&tcp, Event::RcvFin);
	fsm_dispatch(&tcp, Event::Timeout);

	std::printf("-- illegal input is a missing table cell --\n");
	fsm_init(&tcp);
	fsm_dispatch(&tcp, Event::RcvFin);

	return 0;
}
