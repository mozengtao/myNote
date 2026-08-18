#include <cstdio>

enum {
	MaxRedirect = 3,
	StateCount = 5,
	EventCount = 9
};

enum class State {
	Idle = 0,
	Sending,
	Receiving,
	Done,
	Error
};

enum class Event {
	Start = 0,
	Sent,
	HeaderOk,
	BodyDone,
	Redirect,
	Fail,
	Cancel,
	Timeout,
	Chunk
};

struct Context {
	int cancel_requested;
	int redirect_count;
	int connection_open;
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
 * return a printable name for a request state
 */
static const char *
state_name(State state)
{
	if (state == State::Sending)
		return "SENDING";
	if (state == State::Receiving)
		return "RECEIVING";
	if (state == State::Done)
		return "DONE";
	if (state == State::Error)
		return "ERROR";
	return "IDLE";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	if (event == Event::Sent)
		return "SENT";
	if (event == Event::HeaderOk)
		return "HEADER_OK";
	if (event == Event::BodyDone)
		return "BODY_DONE";
	if (event == Event::Redirect)
		return "REDIRECT";
	if (event == Event::Fail)
		return "FAIL";
	if (event == Event::Cancel)
		return "CANCEL";
	if (event == Event::Timeout)
		return "TIMEOUT";
	if (event == Event::Chunk)
		return "CHUNK";
	return "START";
}

/*
 * print one transition and the request context
 */
static void
print_transition(State prev, Event event, const Fsm *fsm)
{
	std::printf("%s --%s--> %s (cancel=%d redirects=%d conn=%d)\n",
	    state_name(prev), event_name(event), state_name(fsm->state),
	    fsm->ctx.cancel_requested, fsm->ctx.redirect_count,
	    fsm->ctx.connection_open);
}

/*
 * begin a GET: new socket or reused connection
 */
static void
action_send_get(Fsm *fsm)
{
	fsm->ctx.cancel_requested = 0;
	fsm->ctx.redirect_count = 0;
	fsm->ctx.timer_armed = 1;
	if (fsm->ctx.connection_open)
		std::printf("reuse connection\n");
	else
		std::printf("new connection\n");
	std::printf("send GET\n");
}

/*
 * the request bytes left the client
 */
static void
action_sent(Fsm *fsm)
{
	fsm->ctx.connection_open = 1;
	std::printf("request sent\n");
}

/*
 * response status line is a success (teaching: always 200)
 */
static void
action_header_ok(Fsm *fsm)
{
	(void)fsm;
	std::printf("got 200\n");
}

/*
 * response body finished
 */
static void
action_body_done(Fsm *fsm)
{
	fsm->ctx.timer_armed = 0;
	std::printf("body done\n");
}

/*
 * follow a 302: count one hop and send GET again
 */
static void
action_follow_redirect(Fsm *fsm)
{
	fsm->ctx.redirect_count++;
	std::printf("got 302, follow redirect (%d/%d)\n",
	    fsm->ctx.redirect_count, MaxRedirect);
	std::printf("send GET\n");
}

/*
 * redirect budget exhausted
 */
static void
action_too_many_redirects(Fsm *fsm)
{
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	std::printf("too many redirects (max %d)\n", MaxRedirect);
}

/*
 * transport or protocol failure
 */
static void
action_fail(Fsm *fsm)
{
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	std::printf("fail\n");
}

/*
 * user abort: set the cancel flag, drop the request
 */
static void
action_cancel(Fsm *fsm)
{
	fsm->ctx.cancel_requested = 1;
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	std::printf("cancel\n");
}

/*
 * request timer fired
 */
static void
action_timeout(Fsm *fsm)
{
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	std::printf("timeout\n");
}

/*
 * one body fragment arrived (streaming)
 */
static void
action_chunk(Fsm *fsm)
{
	(void)fsm;
	std::printf("chunk\n");
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
 * install the request lifecycle table
 */
static void
table_init()
{
	static int ready;

	if (ready)
		return;
	ready = 1;

	table_set(State::Idle, Event::Start, action_send_get, State::Sending);
	table_set(State::Done, Event::Start, action_send_get, State::Sending);
	table_set(State::Error, Event::Start, action_send_get, State::Sending);

	table_set(State::Sending, Event::Sent, action_sent, State::Receiving);
	table_set(State::Sending, Event::Fail, action_fail, State::Error);
	table_set(State::Sending, Event::Cancel, action_cancel, State::Error);
	table_set(State::Sending, Event::Timeout, action_timeout, State::Error);

	table_set(State::Receiving, Event::HeaderOk, action_header_ok,
	    State::Receiving);
	table_set(State::Receiving, Event::Chunk, action_chunk, State::Receiving);
	table_set(State::Receiving, Event::BodyDone, action_body_done,
	    State::Done);
	table_set(State::Receiving, Event::Fail, action_fail, State::Error);
	table_set(State::Receiving, Event::Cancel, action_cancel, State::Error);
	table_set(State::Receiving, Event::Timeout, action_timeout,
	    State::Error);
}

/*
 * put the request FSM into Idle with a clean context
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Idle;
	fsm->ctx.cancel_requested = 0;
	fsm->ctx.redirect_count = 0;
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	table_init();
}

/*
 * dispatch one event into the HTTP request FSM
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;
	Cell *cell;

	prev = fsm->state;

	if (fsm->ctx.cancel_requested &&
	    (event == Event::Sent || event == Event::HeaderOk ||
	     event == Event::BodyDone || event == Event::Redirect ||
	     event == Event::Chunk)) {
		std::printf("ignore %s (cancel_requested)\n",
		    event_name(event));
		return;
	}

	if (event == Event::Redirect && fsm->state == State::Receiving) {
		if (fsm->ctx.redirect_count >= MaxRedirect) {
			action_too_many_redirects(fsm);
			fsm->state = State::Error;
			print_transition(prev, event, fsm);
			return;
		}
		action_follow_redirect(fsm);
		fsm->state = State::Sending;
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

	if (event == Event::Cancel && prev != State::Idle) {
		prev = State::Error;
		fsm->state = State::Idle;
		std::printf("%s --> %s (after cancel)\n",
		    state_name(prev), state_name(fsm->state));
	}
}

int
main()
{
	Fsm req{};

	std::printf("=== http client demo ===\n");

	std::printf("-- 1. happy path --\n");
	fsm_init(&req);
	std::printf("init: %s\n", state_name(req.state));
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::HeaderOk);
	fsm_dispatch(&req, Event::BodyDone);

	std::printf("-- 2. connection reuse (second GET on same client) --\n");
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::HeaderOk);
	fsm_dispatch(&req, Event::BodyDone);

	std::printf("-- 3. one redirect then 200 --\n");
	fsm_init(&req);
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::Redirect);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::HeaderOk);
	fsm_dispatch(&req, Event::BodyDone);

	std::printf("-- 4. too many redirects --\n");
	fsm_init(&req);
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::Redirect);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::Redirect);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::Redirect);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::Redirect);

	std::printf("-- 5. cancel while sending --\n");
	fsm_init(&req);
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Cancel);
	fsm_dispatch(&req, Event::Sent);

	std::printf("-- 6. cancel while receiving --\n");
	fsm_init(&req);
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::HeaderOk);
	fsm_dispatch(&req, Event::Cancel);
	fsm_dispatch(&req, Event::BodyDone);

	std::printf("-- 7. streaming chunks then done --\n");
	fsm_init(&req);
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::HeaderOk);
	fsm_dispatch(&req, Event::Chunk);
	fsm_dispatch(&req, Event::Chunk);
	fsm_dispatch(&req, Event::BodyDone);

	std::printf("-- 8. timeout while sending --\n");
	fsm_init(&req);
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Timeout);

	std::printf("-- 9. fail while receiving --\n");
	fsm_init(&req);
	fsm_dispatch(&req, Event::Start);
	fsm_dispatch(&req, Event::Sent);
	fsm_dispatch(&req, Event::Fail);

	return 0;
}
