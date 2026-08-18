#include <cstdio>

enum {
	RetryBudget = 3,
	StateCount = 6,
	EventCount = 10
};

enum class State {
	Idle = 0,
	Scanning,
	Authenticating,
	Associating,
	Connected,
	Disconnected
};

enum class Event {
	Start = 0,
	ScanDone,
	AuthOk,
	AuthFail,
	AssocOk,
	AssocFail,
	DataOk,
	Lost,
	Timeout,
	Retry
};

struct Context {
	int retries_left;
	int timer_armed;
};

struct Fsm {
	State state;
	Context ctx;
};

using ActionFn = void (*)(Fsm *fsm);

struct Cell {
	int used;
	ActionFn action;
	State next;
};

static Cell table[StateCount][EventCount];

/*
 * return a printable name for a wifi session state
 */
static const char *
state_name(State state)
{
	if (state == State::Scanning)
		return "SCANNING";
	if (state == State::Authenticating)
		return "AUTHENTICATING";
	if (state == State::Associating)
		return "ASSOCIATING";
	if (state == State::Connected)
		return "CONNECTED";
	if (state == State::Disconnected)
		return "DISCONNECTED";
	return "IDLE";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	if (event == Event::ScanDone)
		return "SCAN_DONE";
	if (event == Event::AuthOk)
		return "AUTH_OK";
	if (event == Event::AuthFail)
		return "AUTH_FAIL";
	if (event == Event::AssocOk)
		return "ASSOC_OK";
	if (event == Event::AssocFail)
		return "ASSOC_FAIL";
	if (event == Event::DataOk)
		return "DATA_OK";
	if (event == Event::Lost)
		return "LOST";
	if (event == Event::Timeout)
		return "TIMEOUT";
	if (event == Event::Retry)
		return "RETRY";
	return "START";
}

/*
 * print one transition and the retry budget
 */
static void
print_transition(State prev, Event event, const Fsm *fsm)
{
	std::printf("%s --%s--> %s (retries left=%d)\n",
	    state_name(prev), event_name(event), state_name(fsm->state),
	    fsm->ctx.retries_left);
}

/*
 * begin a session: arm retry budget and start scan
 */
static void
action_start_scan(Fsm *fsm)
{
	fsm->ctx.retries_left = RetryBudget;
	fsm->ctx.timer_armed = 1;
	std::printf("start scan (retries left=%d)\n", fsm->ctx.retries_left);
}

/*
 * scan found a candidate BSS; begin 802.11 authentication
 */
static void
action_begin_auth(Fsm *fsm)
{
	fsm->ctx.timer_armed = 1;
	std::printf("scan done, begin auth\n");
}

/*
 * authentication succeeded; begin association
 */
static void
action_begin_assoc(Fsm *fsm)
{
	fsm->ctx.timer_armed = 1;
	std::printf("auth ok, begin assoc\n");
}

/*
 * association succeeded; session is up (4-way handshake omitted)
 */
static void
action_connected(Fsm *fsm)
{
	fsm->ctx.retries_left = RetryBudget;
	fsm->ctx.timer_armed = 0;
	std::printf("assoc ok, connected\n");
}

/*
 * data path is alive
 */
static void
action_data_ok(Fsm *fsm)
{
	(void)fsm;
	std::printf("data ok\n");
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
	std::printf("session fail, retries left=%d\n", fsm->ctx.retries_left);
}

/*
 * still have budget: return to SCANNING
 */
static void
action_retry_scan(Fsm *fsm)
{
	fsm->ctx.timer_armed = 1;
	std::printf("retry scan, retries left=%d\n", fsm->ctx.retries_left);
}

/*
 * budget exhausted: stop recovering
 */
static void
action_give_up(Fsm *fsm)
{
	fsm->ctx.timer_armed = 0;
	std::printf("give up, back to idle\n");
}

/*
 * fill table[state][event] once
 */
static void
table_set(State state, Event event, ActionFn action, State next)
{
	table[static_cast<int>(state)][static_cast<int>(event)].used = 1;
	table[static_cast<int>(state)][static_cast<int>(event)].action = action;
	table[static_cast<int>(state)][static_cast<int>(event)].next = next;
}

/*
 * install the session table (recovery from DISCONNECTED is guarded)
 */
static void
table_init()
{
	static int ready;

	if (ready)
		return;
	ready = 1;

	table_set(State::Idle, Event::Start, action_start_scan,
	    State::Scanning);

	table_set(State::Scanning, Event::ScanDone, action_begin_auth,
	    State::Authenticating);
	table_set(State::Scanning, Event::Timeout, action_session_fail,
	    State::Disconnected);

	table_set(State::Authenticating, Event::AuthOk, action_begin_assoc,
	    State::Associating);
	table_set(State::Authenticating, Event::AuthFail, action_session_fail,
	    State::Disconnected);
	table_set(State::Authenticating, Event::Timeout, action_session_fail,
	    State::Disconnected);

	table_set(State::Associating, Event::AssocOk, action_connected,
	    State::Connected);
	table_set(State::Associating, Event::AssocFail, action_session_fail,
	    State::Disconnected);
	table_set(State::Associating, Event::Timeout, action_session_fail,
	    State::Disconnected);

	table_set(State::Connected, Event::DataOk, action_data_ok,
	    State::Connected);
	table_set(State::Connected, Event::Lost, action_session_fail,
	    State::Disconnected);
}

/*
 * put the wifi FSM into Idle with a clean context
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Idle;
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

	if (fsm->state == State::Disconnected &&
	    (event == Event::Retry || event == Event::Timeout)) {
		if (fsm->ctx.retries_left > 0) {
			action_retry_scan(fsm);
			fsm->state = State::Scanning;
		} else {
			action_give_up(fsm);
			fsm->state = State::Idle;
		}
		print_transition(prev, event, fsm);
		return;
	}

	cell = &table[static_cast<int>(fsm->state)][static_cast<int>(event)];
	if (!cell->used)
		return;

	if (cell->action)
		cell->action(fsm);
	fsm->state = cell->next;
	print_transition(prev, event, fsm);
}

int
main()
{
	Fsm wifi{};

	std::printf("=== wifi session demo ===\n");

	std::printf("-- 1. happy path association --\n");
	fsm_init(&wifi);
	std::printf("init: %s\n", state_name(wifi.state));
	fsm_dispatch(&wifi, Event::Start);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthOk);
	fsm_dispatch(&wifi, Event::AssocOk);
	fsm_dispatch(&wifi, Event::DataOk);

	std::printf("-- 2. auth fail, then retry, then success --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, Event::Start);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthFail);
	fsm_dispatch(&wifi, Event::Retry);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthOk);
	fsm_dispatch(&wifi, Event::AssocOk);

	std::printf("-- 3. assoc fail until budget exhausted --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, Event::Start);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthOk);
	fsm_dispatch(&wifi, Event::AssocFail);
	fsm_dispatch(&wifi, Event::Retry);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthOk);
	fsm_dispatch(&wifi, Event::AssocFail);
	fsm_dispatch(&wifi, Event::Retry);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthOk);
	fsm_dispatch(&wifi, Event::AssocFail);
	fsm_dispatch(&wifi, Event::Retry);

	std::printf("-- 4. connected then lost, timeout recovers to scan --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, Event::Start);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthOk);
	fsm_dispatch(&wifi, Event::AssocOk);
	fsm_dispatch(&wifi, Event::Lost);
	fsm_dispatch(&wifi, Event::Timeout);
	fsm_dispatch(&wifi, Event::ScanDone);
	fsm_dispatch(&wifi, Event::AuthOk);
	fsm_dispatch(&wifi, Event::AssocOk);

	std::printf("-- 5. scan timeout then give up after budget --\n");
	fsm_init(&wifi);
	fsm_dispatch(&wifi, Event::Start);
	fsm_dispatch(&wifi, Event::Timeout);
	fsm_dispatch(&wifi, Event::Retry);
	fsm_dispatch(&wifi, Event::Timeout);
	fsm_dispatch(&wifi, Event::Retry);
	fsm_dispatch(&wifi, Event::Timeout);
	fsm_dispatch(&wifi, Event::Retry);

	return 0;
}
