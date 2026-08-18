#include <stdio.h>

#define MAX_REDIRECT 3
#define STATE_COUNT 5
#define EVENT_COUNT 9

typedef enum {
	STATE_IDLE = 0,
	STATE_SENDING,
	STATE_RECEIVING,
	STATE_DONE,
	STATE_ERROR
} State;

typedef enum {
	EVENT_START = 0,
	EVENT_SENT,
	EVENT_HEADER_OK,
	EVENT_BODY_DONE,
	EVENT_REDIRECT,
	EVENT_FAIL,
	EVENT_CANCEL,
	EVENT_TIMEOUT,
	EVENT_CHUNK
} Event;

typedef struct {
	int cancel_requested;
	int redirect_count;
	int connection_open;
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
 * return a printable name for a request state
 */
static const char *
state_name(State state)
{
	if (state == STATE_SENDING)
		return "SENDING";
	if (state == STATE_RECEIVING)
		return "RECEIVING";
	if (state == STATE_DONE)
		return "DONE";
	if (state == STATE_ERROR)
		return "ERROR";
	return "IDLE";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_SENT)
		return "SENT";
	if (event == EVENT_HEADER_OK)
		return "HEADER_OK";
	if (event == EVENT_BODY_DONE)
		return "BODY_DONE";
	if (event == EVENT_REDIRECT)
		return "REDIRECT";
	if (event == EVENT_FAIL)
		return "FAIL";
	if (event == EVENT_CANCEL)
		return "CANCEL";
	if (event == EVENT_TIMEOUT)
		return "TIMEOUT";
	if (event == EVENT_CHUNK)
		return "CHUNK";
	return "START";
}

/*
 * print one transition and the request context
 */
static void
print_transition(State prev, Event event, const Fsm *fsm)
{
	printf("%s --%s--> %s (cancel=%d redirects=%d conn=%d)\n",
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
		printf("reuse connection\n");
	else
		printf("new connection\n");
	printf("send GET\n");
}

/*
 * the request bytes left the client
 */
static void
action_sent(Fsm *fsm)
{
	fsm->ctx.connection_open = 1;
	printf("request sent\n");
}

/*
 * response status line is a success (teaching: always 200)
 */
static void
action_header_ok(Fsm *fsm)
{
	(void)fsm;
	printf("got 200\n");
}

/*
 * response body finished
 */
static void
action_body_done(Fsm *fsm)
{
	fsm->ctx.timer_armed = 0;
	printf("body done\n");
}

/*
 * follow a 302: count one hop and send GET again
 */
static void
action_follow_redirect(Fsm *fsm)
{
	fsm->ctx.redirect_count++;
	printf("got 302, follow redirect (%d/%d)\n",
	    fsm->ctx.redirect_count, MAX_REDIRECT);
	printf("send GET\n");
}

/*
 * redirect budget exhausted
 */
static void
action_too_many_redirects(Fsm *fsm)
{
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	printf("too many redirects (max %d)\n", MAX_REDIRECT);
}

/*
 * transport or protocol failure
 */
static void
action_fail(Fsm *fsm)
{
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	printf("fail\n");
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
	printf("cancel\n");
}

/*
 * request timer fired
 */
static void
action_timeout(Fsm *fsm)
{
	fsm->ctx.connection_open = 0;
	fsm->ctx.timer_armed = 0;
	printf("timeout\n");
}

/*
 * one body fragment arrived (streaming)
 */
static void
action_chunk(Fsm *fsm)
{
	(void)fsm;
	printf("chunk\n");
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
 * install the request lifecycle table
 */
static void
table_init(void)
{
	static int ready;

	if (ready)
		return;
	ready = 1;

	table_set(STATE_IDLE, EVENT_START, action_send_get, STATE_SENDING);
	table_set(STATE_DONE, EVENT_START, action_send_get, STATE_SENDING);
	table_set(STATE_ERROR, EVENT_START, action_send_get, STATE_SENDING);

	table_set(STATE_SENDING, EVENT_SENT, action_sent, STATE_RECEIVING);
	table_set(STATE_SENDING, EVENT_FAIL, action_fail, STATE_ERROR);
	table_set(STATE_SENDING, EVENT_CANCEL, action_cancel, STATE_ERROR);
	table_set(STATE_SENDING, EVENT_TIMEOUT, action_timeout, STATE_ERROR);

	table_set(STATE_RECEIVING, EVENT_HEADER_OK, action_header_ok,
	    STATE_RECEIVING);
	table_set(STATE_RECEIVING, EVENT_CHUNK, action_chunk, STATE_RECEIVING);
	table_set(STATE_RECEIVING, EVENT_BODY_DONE, action_body_done,
	    STATE_DONE);
	table_set(STATE_RECEIVING, EVENT_FAIL, action_fail, STATE_ERROR);
	table_set(STATE_RECEIVING, EVENT_CANCEL, action_cancel, STATE_ERROR);
	table_set(STATE_RECEIVING, EVENT_TIMEOUT, action_timeout, STATE_ERROR);
}

/*
 * put the request FSM into IDLE with a clean context
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_IDLE;
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
	    (event == EVENT_SENT || event == EVENT_HEADER_OK ||
	     event == EVENT_BODY_DONE || event == EVENT_REDIRECT ||
	     event == EVENT_CHUNK)) {
		printf("ignore %s (cancel_requested)\n", event_name(event));
		return;
	}

	if (event == EVENT_REDIRECT && fsm->state == STATE_RECEIVING) {
		if (fsm->ctx.redirect_count >= MAX_REDIRECT) {
			action_too_many_redirects(fsm);
			fsm->state = STATE_ERROR;
			print_transition(prev, event, fsm);
			return;
		}
		action_follow_redirect(fsm);
		fsm->state = STATE_SENDING;
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

	if (event == EVENT_CANCEL && prev != STATE_IDLE) {
		prev = STATE_ERROR;
		fsm->state = STATE_IDLE;
		printf("%s --> %s (after cancel)\n",
		    state_name(prev), state_name(fsm->state));
	}
}

int
main(void)
{
	Fsm req;

	printf("=== http client demo ===\n");

	printf("-- 1. happy path --\n");
	fsm_init(&req);
	printf("init: %s\n", state_name(req.state));
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_HEADER_OK);
	fsm_dispatch(&req, EVENT_BODY_DONE);

	printf("-- 2. connection reuse (second GET on same client) --\n");
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_HEADER_OK);
	fsm_dispatch(&req, EVENT_BODY_DONE);

	printf("-- 3. one redirect then 200 --\n");
	fsm_init(&req);
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_REDIRECT);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_HEADER_OK);
	fsm_dispatch(&req, EVENT_BODY_DONE);

	printf("-- 4. too many redirects --\n");
	fsm_init(&req);
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_REDIRECT);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_REDIRECT);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_REDIRECT);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_REDIRECT);

	printf("-- 5. cancel while sending --\n");
	fsm_init(&req);
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_CANCEL);
	fsm_dispatch(&req, EVENT_SENT);

	printf("-- 6. cancel while receiving --\n");
	fsm_init(&req);
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_HEADER_OK);
	fsm_dispatch(&req, EVENT_CANCEL);
	fsm_dispatch(&req, EVENT_BODY_DONE);

	printf("-- 7. streaming chunks then done --\n");
	fsm_init(&req);
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_HEADER_OK);
	fsm_dispatch(&req, EVENT_CHUNK);
	fsm_dispatch(&req, EVENT_CHUNK);
	fsm_dispatch(&req, EVENT_BODY_DONE);

	printf("-- 8. timeout while sending --\n");
	fsm_init(&req);
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_TIMEOUT);

	printf("-- 9. fail while receiving --\n");
	fsm_init(&req);
	fsm_dispatch(&req, EVENT_START);
	fsm_dispatch(&req, EVENT_SENT);
	fsm_dispatch(&req, EVENT_FAIL);

	return 0;
}
