#include <cstdio>

enum class State {
	SendInitial = 0,
	Handshake,
	Connected,
	Draining,
	Closed
};

enum {
	StateCount = 5
};

enum class Event {
	InitialAck = 0,
	HandshakeDone,
	AppClose,
	IdleTimeout,
	PathFail
};

enum {
	EventCount = 5
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
 * return a printable name for a QUIC teaching state
 */
static const char *
state_name(State state)
{
	if (state == State::Handshake)
		return "HANDSHAKE";
	if (state == State::Connected)
		return "CONNECTED";
	if (state == State::Draining)
		return "DRAINING";
	if (state == State::Closed)
		return "CLOSED";
	return "SEND_INITIAL";
}

/*
 * return a printable name for a QUIC teaching event
 */
static const char *
event_name(Event event)
{
	if (event == Event::HandshakeDone)
		return "HANDSHAKE_DONE";
	if (event == Event::AppClose)
		return "APP_CLOSE";
	if (event == Event::IdleTimeout)
		return "IDLE_TIMEOUT";
	if (event == Event::PathFail)
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
	std::printf("action: send Handshake\n");
}

/*
 * handshake confirmed; streams may be created (not implemented)
 */
static void
action_connected(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: connected\n");
}

/*
 * application closes; send CONNECTION_CLOSE and drain
 */
static void
action_connection_close(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send CONNECTION_CLOSE\n");
}

/*
 * abandon before the session is up
 */
static void
action_abandon(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: abandon handshake\n");
}

/*
 * idle timeout tore the connection down
 */
static void
action_idle_close(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: idle timeout\n");
}

/*
 * path failed during handshake; no session to drain
 */
static void
action_path_fail_hard(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: path fail, close\n");
}

/*
 * path failed after connect; drain leftover packets
 */
static void
action_path_fail_drain(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: path fail, drain\n");
}

/*
 * draining period ended
 */
static void
action_drained(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: drained, close\n");
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
	table_set(State::SendInitial, Event::InitialAck, action_send_handshake,
	    State::Handshake);
	table_set(State::SendInitial, Event::AppClose, action_abandon,
	    State::Closed);
	table_set(State::SendInitial, Event::IdleTimeout, action_idle_close,
	    State::Closed);
	table_set(State::SendInitial, Event::PathFail, action_path_fail_hard,
	    State::Closed);
	table_set(State::Handshake, Event::HandshakeDone, action_connected,
	    State::Connected);
	table_set(State::Handshake, Event::AppClose, action_connection_close,
	    State::Draining);
	table_set(State::Handshake, Event::IdleTimeout, action_idle_close,
	    State::Closed);
	table_set(State::Handshake, Event::PathFail, action_path_fail_hard,
	    State::Closed);
	table_set(State::Connected, Event::AppClose, action_connection_close,
	    State::Draining);
	table_set(State::Connected, Event::IdleTimeout, action_idle_close,
	    State::Closed);
	table_set(State::Connected, Event::PathFail, action_path_fail_drain,
	    State::Draining);
	table_set(State::Draining, Event::IdleTimeout, action_drained,
	    State::Closed);
	table_set(State::Draining, Event::PathFail, action_drained,
	    State::Closed);
	table_ready = 1;
}

/*
 * put the teaching QUIC FSM into SEND_INITIAL
 */
void
fsm_init(Fsm *fsm)
{
	ensure_table();
	fsm->state = State::SendInitial;
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
	Fsm quic;

	std::printf("=== quic teaching FSM ===\n");

	std::printf("-- handshake --\n");
	fsm_init(&quic);
	std::printf("init: %s\n", state_name(quic.state));
	fsm_dispatch(&quic, Event::InitialAck);
	fsm_dispatch(&quic, Event::HandshakeDone);

	std::printf("-- app close then drain --\n");
	fsm_dispatch(&quic, Event::AppClose);
	fsm_dispatch(&quic, Event::IdleTimeout);

	std::printf("-- idle timeout from CONNECTED --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, Event::InitialAck);
	fsm_dispatch(&quic, Event::HandshakeDone);
	fsm_dispatch(&quic, Event::IdleTimeout);

	std::printf("-- path fail after connect drains --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, Event::InitialAck);
	fsm_dispatch(&quic, Event::HandshakeDone);
	fsm_dispatch(&quic, Event::PathFail);
	fsm_dispatch(&quic, Event::IdleTimeout);

	std::printf("-- path fail during handshake --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, Event::InitialAck);
	fsm_dispatch(&quic, Event::PathFail);

	std::printf("-- illegal input is a missing table cell --\n");
	fsm_init(&quic);
	fsm_dispatch(&quic, Event::HandshakeDone);

	return 0;
}
