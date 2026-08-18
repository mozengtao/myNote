#include <cstdio>

enum class State {
	Start = 0,
	WaitServerHello,
	WaitFinished,
	Established,
	Closed
};

enum {
	StateCount = 5
};

enum class Event {
	Connect = 0,
	ServerHello,
	CertOk,
	Finished,
	Alert,
	Timeout
};

enum {
	EventCount = 6
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
 * return a printable name for a TLS teaching state
 */
static const char *
state_name(State state)
{
	if (state == State::WaitServerHello)
		return "WAIT_SERVER_HELLO";
	if (state == State::WaitFinished)
		return "WAIT_FINISHED";
	if (state == State::Established)
		return "ESTABLISHED";
	if (state == State::Closed)
		return "CLOSED";
	return "START";
}

/*
 * return a printable name for a TLS teaching event
 */
static const char *
event_name(Event event)
{
	if (event == Event::ServerHello)
		return "SERVER_HELLO";
	if (event == Event::CertOk)
		return "CERT_OK";
	if (event == Event::Finished)
		return "FINISHED";
	if (event == Event::Alert)
		return "ALERT";
	if (event == Event::Timeout)
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
	std::printf("action: send ClientHello\n");
}

/*
 * ServerHello accepted; still waiting for cert and Finished
 */
static void
action_got_server_hello(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: got ServerHello\n");
}

/*
 * certificate chain verified (crypto is outside this FSM)
 */
static void
action_cert_ok(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: cert ok\n");
}

/*
 * peer Finished accepted; handshake is done
 */
static void
action_handshake_done(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: handshake done\n");
}

/*
 * fatal alert; close the teaching session
 */
static void
action_alert(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: alert, close\n");
}

/*
 * handshake wait timed out
 */
static void
action_timeout(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: handshake timeout\n");
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
	table_set(State::Start, Event::Connect, action_send_client_hello,
	    State::WaitServerHello);
	table_set(State::WaitServerHello, Event::ServerHello,
	    action_got_server_hello, State::WaitFinished);
	table_set(State::WaitServerHello, Event::Alert, action_alert,
	    State::Closed);
	table_set(State::WaitServerHello, Event::Timeout, action_timeout,
	    State::Closed);
	table_set(State::WaitFinished, Event::CertOk, action_cert_ok,
	    State::WaitFinished);
	table_set(State::WaitFinished, Event::Finished, action_handshake_done,
	    State::Established);
	table_set(State::WaitFinished, Event::Alert, action_alert,
	    State::Closed);
	table_set(State::WaitFinished, Event::Timeout, action_timeout,
	    State::Closed);
	table_set(State::Established, Event::Alert, action_alert, State::Closed);
	table_ready = 1;
}

/*
 * put the teaching TLS FSM into START
 */
void
fsm_init(Fsm *fsm)
{
	ensure_table();
	fsm->state = State::Start;
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
	Fsm tls;

	std::printf("=== tls teaching FSM ===\n");

	std::printf("-- handshake --\n");
	fsm_init(&tls);
	std::printf("init: %s\n", state_name(tls.state));
	fsm_dispatch(&tls, Event::Connect);
	fsm_dispatch(&tls, Event::ServerHello);
	fsm_dispatch(&tls, Event::CertOk);
	fsm_dispatch(&tls, Event::Finished);

	std::printf("-- alert after established --\n");
	fsm_dispatch(&tls, Event::Alert);

	std::printf("-- timeout waiting for ServerHello --\n");
	fsm_init(&tls);
	fsm_dispatch(&tls, Event::Connect);
	fsm_dispatch(&tls, Event::Timeout);

	std::printf("-- alert while waiting for Finished --\n");
	fsm_init(&tls);
	fsm_dispatch(&tls, Event::Connect);
	fsm_dispatch(&tls, Event::ServerHello);
	fsm_dispatch(&tls, Event::Alert);

	std::printf("-- illegal input is a missing table cell --\n");
	fsm_init(&tls);
	fsm_dispatch(&tls, Event::Finished);

	return 0;
}
